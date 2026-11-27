/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Pop3/FetchJob.h"

#include <algorithm>

#include <QCryptographicHash>
#include <QDateTime>
#include <QSet>
#include <QSettings>
#include <QSslSocket>
#include <QTimer>

#include "Common/PortNumbers.h"
#include "Common/SettingsNames.h"
#include "LocalMail/Store.h"

namespace Pop3 {

namespace {

const QString inbox = QStringLiteral("INBOX");

/** Whether a negative answer means the server is busy rather than that the credentials are wrong (RFC 2449, 3206) */
bool isTemporary(const QByteArray &status)
{
    const QByteArray upper = status.toUpper();
    return upper.contains("[IN-USE]") || upper.contains("[LOGIN-DELAY]") || upper.contains("[SYS/TEMP]");
}

}

quint16 Settings::effectivePort() const
{
    if (port)
        return port;
    return security == Security::Tls ? Common::PORT_POP3S : Common::PORT_POP3;
}

QString Settings::securityName(Security security)
{
    switch (security) {
    case Security::Tls:
        return QStringLiteral("tls");
    case Security::StartTls:
        return QStringLiteral("starttls");
    case Security::None:
        return QStringLiteral("none");
    }
    return QStringLiteral("tls");
}

Security Settings::securityFromName(const QString &name)
{
    if (name == QLatin1String("starttls"))
        return Security::StartTls;
    if (name == QLatin1String("none"))
        return Security::None;
    return Security::Tls;
}

QString Settings::retentionName(Retention retention)
{
    switch (retention) {
    case Retention::Keep:
        return QStringLiteral("keep");
    case Retention::AfterDownload:
        return QStringLiteral("afterDownload");
    case Retention::AfterDays:
        return QStringLiteral("afterDays");
    }
    return QStringLiteral("keep");
}

Retention Settings::retentionFromName(const QString &name)
{
    if (name == QLatin1String("afterDownload"))
        return Retention::AfterDownload;
    if (name == QLatin1String("afterDays"))
        return Retention::AfterDays;
    return Retention::Keep;
}

Settings Settings::fromSettings(const QSettings &account)
{
    using Common::SettingsNames;
    Settings settings;
    settings.host = account.value(SettingsNames::pop3HostKey).toString();
    settings.port = quint16(account.value(SettingsNames::pop3PortKey, 0).toUInt());
    settings.security = securityFromName(account.value(SettingsNames::pop3SecurityKey).toString());
    settings.user = account.value(SettingsNames::pop3UserKey).toString();
    settings.retention = retentionFromName(account.value(SettingsNames::pop3RetentionKey).toString());
    settings.retentionDays = qMax(1, account.value(SettingsNames::pop3RetentionDaysKey, 14).toInt());
    settings.interval = qMax(0, account.value(SettingsNames::pop3IntervalKey, 10).toInt());
    return settings;
}

void Settings::save(QSettings &account) const
{
    using Common::SettingsNames;
    account.setValue(SettingsNames::pop3HostKey, host);
    account.setValue(SettingsNames::pop3PortKey, port);
    account.setValue(SettingsNames::pop3SecurityKey, securityName(security));
    account.setValue(SettingsNames::pop3UserKey, user);
    account.setValue(SettingsNames::pop3RetentionKey, retentionName(retention));
    account.setValue(SettingsNames::pop3RetentionDaysKey, retentionDays);
    account.setValue(SettingsNames::pop3IntervalKey, interval);
}

FetchJob::FetchJob(const Settings &settings, std::shared_ptr<LocalMail::Store> store, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_store(std::move(store))
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &FetchJob::onTimeout);
}

FetchJob::~FetchJob()
{
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        delete m_socket;
    }
}

void FetchJob::setTrustCheck(TrustCheck check)
{
    m_trustCheck = std::move(check);
}

void FetchJob::setTimeout(int seconds)
{
    m_timeoutSeconds = qMax(1, seconds);
}

bool FetchJob::isRunning() const
{
    return m_step != Step::Idle && m_step != Step::Done;
}

int FetchJob::downloaded() const
{
    return m_downloaded;
}

int FetchJob::removedFromServer() const
{
    return m_removed;
}

void FetchJob::start()
{
    if (isRunning())
        return;
    if (!m_store) {
        m_step = Step::Greeting;
        fail(Failure::Storage, tr("The mail folder on this computer could not be opened."));
        return;
    }
    if (m_settings.host.isEmpty()) {
        m_step = Step::Greeting;
        fail(Failure::Network, tr("No POP3 server is set."));
        return;
    }

    m_upgraded = false;
    m_capabilities.clear();
    m_sslErrors.clear();
    m_certificateRejected = false;
    m_serverMessages.clear();
    m_toDownload.clear();
    m_toDelete.clear();
    m_deletedKeys.clear();
    m_downloaded = 0;
    m_removed = 0;
    m_buffer.clear();

    m_socket = new QSslSocket(this);
    m_socket->setPeerVerifyName(m_settings.host);
    connect(m_socket, &QSslSocket::readyRead, this, &FetchJob::onReadyRead);
    connect(m_socket, &QSslSocket::encrypted, this, &FetchJob::onEncrypted);
    connect(m_socket, &QSslSocket::sslErrors, this, &FetchJob::onSslErrors);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &FetchJob::onSocketError);

    m_step = Step::Greeting;
    m_multiLine = false;
    m_statusRead = false;
    m_lines.clear();
    m_timer->start(m_timeoutSeconds * 1000);
    if (m_settings.security == Security::Tls)
        m_socket->connectToHostEncrypted(m_settings.host, m_settings.effectivePort());
    else
        m_socket->connectToHost(m_settings.host, m_settings.effectivePort());
}

void FetchJob::abort()
{
    if (isRunning())
        fail(Failure::Network, tr("Checking for mail was cancelled."));
}

void FetchJob::onEncrypted()
{
    if (m_step == Step::StartTls) {
        // RFC 2595: what the server said before TLS cannot be trusted, so ask again
        m_upgraded = true;
        requestCapabilities();
    }
}

void FetchJob::onSslErrors(const QList<QSslError> &errors)
{
    if (!m_socket)
        return;
    if (m_trustCheck && m_trustCheck(m_settings.host, m_socket->peerCertificate())) {
        m_socket->ignoreSslErrors();
        return;
    }
    // Not ignoring the errors ends the handshake; onSocketError() reports it
    m_sslErrors = errors;
    m_certificateRejected = true;
}

void FetchJob::onSocketError()
{
    if (!isRunning() || !m_socket)
        return;
    if (m_certificateRejected) {
        emit certificateRejected(m_socket->peerCertificateChain(), m_sslErrors);
        QStringList reasons;
        for (const QSslError &error : std::as_const(m_sslErrors))
            reasons << error.errorString();
        fail(Failure::Certificate, tr("The certificate of %1 is not trusted: %2")
                                       .arg(m_settings.host, reasons.join(QLatin1String("; "))));
        return;
    }
    if (m_step == Step::Quit) {
        // Some servers close as soon as they have answered QUIT; the mail is stored either way
        finish();
        return;
    }
    fail(Failure::Network, m_socket->errorString());
}

void FetchJob::onTimeout()
{
    if (isRunning())
        fail(Failure::Network, tr("%1 did not answer in time.").arg(m_settings.host));
}

void FetchJob::send(const QByteArray &command, Step step, bool multiLine)
{
    m_step = step;
    m_multiLine = multiLine;
    m_statusRead = false;
    m_statusOk = false;
    m_status.clear();
    m_lines.clear();
    m_timer->start(m_timeoutSeconds * 1000);
    m_socket->write(command + "\r\n");
}

void FetchJob::onReadyRead()
{
    if (!m_socket)
        return;
    m_buffer += m_socket->readAll();
    m_timer->start(m_timeoutSeconds * 1000);

    qsizetype start = 0;
    while (isRunning()) {
        const qsizetype end = m_buffer.indexOf('\n', start);
        if (end < 0)
            break;
        QByteArray line = m_buffer.mid(start, end - start);
        start = end + 1;
        if (line.endsWith('\r'))
            line.chop(1);

        if (!m_statusRead) {
            m_statusRead = true;
            m_status = line;
            m_statusOk = line.startsWith("+OK");
            if (!m_multiLine || !m_statusOk) {
                m_buffer.remove(0, start);
                start = 0;
                handleResponse(m_statusOk, m_status, {});
            }
            continue;
        }
        if (line == ".") {
            m_buffer.remove(0, start);
            start = 0;
            const QList<QByteArray> lines = std::move(m_lines);
            m_lines.clear();
            handleResponse(true, m_status, lines);
            continue;
        }
        // Dot-unstuffing, RFC 1939 section 3
        if (line.startsWith('.'))
            line.remove(0, 1);
        m_lines.append(line);
    }
    if (start > 0)
        m_buffer.remove(0, start);
}

QString FetchJob::serverText(const QByteArray &status)
{
    QByteArray text = status;
    if (text.startsWith("-ERR"))
        text.remove(0, 4);
    else if (text.startsWith("+OK"))
        text.remove(0, 3);
    return QString::fromUtf8(text.trimmed());
}

void FetchJob::requestCapabilities()
{
    send("CAPA", Step::Capabilities, true);
}

void FetchJob::handleResponse(bool ok, const QByteArray &status, const QList<QByteArray> &lines)
{
    switch (m_step) {
    case Step::Idle:
    case Step::Done:
        return;

    case Step::Greeting:
        if (!ok) {
            fail(Failure::Protocol, tr("%1 refused the connection: %2").arg(m_settings.host, serverText(status)));
            return;
        }
        requestCapabilities();
        return;

    case Step::Capabilities:
        m_capabilities.clear();
        if (ok) {
            for (const QByteArray &line : lines)
                m_capabilities << QString::fromUtf8(line).trimmed().toUpper();
        }
        if (m_settings.security == Security::StartTls && !m_upgraded) {
            // A server without CAPA may still know STLS, so it is only skipped when CAPA says it is missing
            if (!m_capabilities.isEmpty() && !m_capabilities.contains(QLatin1String("STLS"))) {
                fail(Failure::Security, tr("%1 does not offer STLS, so the password was not sent. Choose TLS or ask "
                                           "the provider which protection the server supports.")
                                            .arg(m_settings.host));
                return;
            }
            send("STLS", Step::StartTls, false);
            return;
        }
        signIn();
        return;

    case Step::StartTls:
        if (!ok) {
            fail(Failure::Security,
                 tr("%1 refused STLS, so the password was not sent: %2").arg(m_settings.host, serverText(status)));
            return;
        }
        // Anything the server sent after its STLS answer was outside TLS; drop it
        m_buffer.clear();
        m_timer->start(m_timeoutSeconds * 1000);
        m_socket->startClientEncryption();
        return;

    case Step::AuthPlain:
        if (status.startsWith("+ ") || status == "+") {
            // The server ignored the initial response and asks for it again
            m_statusRead = false;
            m_socket->write(QByteArray(QByteArray(1, '\0') + m_settings.user.toUtf8() + QByteArray(1, '\0')
                                       + m_settings.password.toUtf8())
                                .toBase64()
                            + "\r\n");
            return;
        }
        [[fallthrough]];
    case Step::Pass:
        if (!ok) {
            if (isTemporary(status))
                fail(Failure::Network, tr("%1 cannot be used right now: %2").arg(m_settings.host, serverText(status)));
            else
                fail(Failure::Authentication, serverText(status));
            return;
        }
        listMessages();
        return;

    case Step::User:
        if (!ok) {
            fail(Failure::Authentication, serverText(status));
            return;
        }
        send("PASS " + m_settings.password.toUtf8(), Step::Pass, false);
        return;

    case Step::Uidl:
        if (!ok) {
            send("LIST", Step::List, true);
            return;
        }
        for (const QByteArray &line : lines) {
            const QList<QByteArray> parts = line.simplified().split(' ');
            bool numberOk = false;
            const int number = parts.value(0).toInt(&numberOk);
            if (!numberOk || parts.size() < 2)
                continue;
            m_serverMessages.append({number, QStringLiteral("uidl:") + QString::fromLatin1(parts.at(1))});
        }
        m_toDownload.clear();
        for (int i = 0; i < m_serverMessages.size(); ++i) {
            if (!m_store->knowsOrigin(m_serverMessages.at(i).second))
                m_toDownload.append(i);
        }
        m_cursor = 0;
        nextDownload();
        return;

    case Step::List:
        if (!ok) {
            fail(Failure::Protocol, tr("%1 cannot list its messages: %2").arg(m_settings.host, serverText(status)));
            return;
        }
        for (const QByteArray &line : lines) {
            bool numberOk = false;
            const int number = line.simplified().split(' ').value(0).toInt(&numberOk);
            if (numberOk)
                m_serverMessages.append({number, QString()});
        }
        m_cursor = 0;
        nextHeaderDigest();
        return;

    case Step::Top: {
        if (!ok) {
            fail(Failure::Protocol, tr("%1 supports neither UIDL nor TOP, so messages cannot be told apart.")
                                        .arg(m_settings.host));
            return;
        }
        QByteArray headers;
        for (const QByteArray &line : lines)
            headers += line + "\r\n";
        QString key = QStringLiteral("hdr:")
            + QString::fromLatin1(QCryptographicHash::hash(headers, QCryptographicHash::Sha256).toHex());
        // Two messages with identical headers still need their own keys
        QString unique = key;
        for (int n = 2;; ++n) {
            bool taken = false;
            for (int i = 0; i < m_cursor && !taken; ++i)
                taken = m_serverMessages.at(i).second == unique;
            if (!taken)
                break;
            unique = key + QLatin1Char('#') + QString::number(n);
        }
        m_serverMessages[m_cursor].second = unique;
        ++m_cursor;
        nextHeaderDigest();
        return;
    }

    case Step::Retrieve: {
        if (!ok) {
            fail(Failure::Protocol, tr("%1 did not send a message: %2").arg(m_settings.host, serverText(status)));
            return;
        }
        QByteArray raw;
        qsizetype size = 0;
        for (const QByteArray &line : lines)
            size += line.size() + 2;
        raw.reserve(size);
        for (const QByteArray &line : lines) {
            raw += line;
            raw += "\r\n";
        }
        const auto &message = m_serverMessages.at(m_toDownload.at(m_cursor));
        QString error;
        if (!m_store->append(inbox, raw, {}, {}, message.second, &error)) {
            fail(Failure::Storage, tr("A downloaded message could not be stored: %1").arg(error));
            return;
        }
        ++m_downloaded;
        ++m_cursor;
        nextDownload();
        return;
    }

    case Step::Delete:
        // A message the server refuses to delete stays there and is tried again next time
        if (ok)
            m_deletedKeys.append(m_serverMessages.at(m_toDelete.at(m_cursor)).second);
        ++m_cursor;
        nextDeletion();
        return;

    case Step::Quit: {
        QStringList kept;
        for (const auto &message : std::as_const(m_serverMessages))
            kept.append(message.second);
        if (ok) {
            // The deletions are final only now
            m_removed = m_deletedKeys.size();
            const QSet<QString> deleted(m_deletedKeys.cbegin(), m_deletedKeys.cend());
            kept.erase(std::remove_if(kept.begin(), kept.end(),
                                      [&deleted](const QString &key) { return deleted.contains(key); }),
                       kept.end());
        }
        m_store->keepOnlyOrigins(kept);
        finish();
        return;
    }
    }
}

void FetchJob::signIn()
{
    bool plain = false;
    for (const QString &capability : std::as_const(m_capabilities)) {
        if (capability.startsWith(QLatin1String("SASL "))
            && capability.split(QLatin1Char(' '), Qt::SkipEmptyParts).contains(QLatin1String("PLAIN")))
            plain = true;
    }
    if (plain) {
        const QByteArray credentials = QByteArray(1, '\0') + m_settings.user.toUtf8() + QByteArray(1, '\0')
            + m_settings.password.toUtf8();
        send("AUTH PLAIN " + credentials.toBase64(), Step::AuthPlain, false);
    } else {
        send("USER " + m_settings.user.toUtf8(), Step::User, false);
    }
}

void FetchJob::listMessages()
{
    m_serverMessages.clear();
    send("UIDL", Step::Uidl, true);
}

void FetchJob::nextHeaderDigest()
{
    if (m_cursor < m_serverMessages.size()) {
        send("TOP " + QByteArray::number(m_serverMessages.at(m_cursor).first) + " 0", Step::Top, true);
        return;
    }
    m_toDownload.clear();
    for (int i = 0; i < m_serverMessages.size(); ++i) {
        if (!m_store->knowsOrigin(m_serverMessages.at(i).second))
            m_toDownload.append(i);
    }
    m_cursor = 0;
    nextDownload();
}

void FetchJob::nextDownload()
{
    if (m_cursor < m_toDownload.size()) {
        send("RETR " + QByteArray::number(m_serverMessages.at(m_toDownload.at(m_cursor)).first), Step::Retrieve, true);
        return;
    }

    m_toDelete.clear();
    if (m_settings.retention != Retention::Keep) {
        const QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-m_settings.retentionDays);
        for (int i = 0; i < m_serverMessages.size(); ++i) {
            const QString &key = m_serverMessages.at(i).second;
            // Only mail that is safely on disk here may go from the server
            if (!m_store->knowsOrigin(key))
                continue;
            if (m_settings.retention == Retention::AfterDays) {
                const QDateTime stored = m_store->originStored(key);
                if (!stored.isValid() || stored > cutoff)
                    continue;
            }
            m_toDelete.append(i);
        }
    }
    m_cursor = 0;
    nextDeletion();
}

void FetchJob::nextDeletion()
{
    if (m_cursor < m_toDelete.size()) {
        send("DELE " + QByteArray::number(m_serverMessages.at(m_toDelete.at(m_cursor)).first), Step::Delete, false);
        return;
    }
    quit();
}

void FetchJob::quit()
{
    send("QUIT", Step::Quit, false);
}

void FetchJob::fail(Failure failure, const QString &message)
{
    if (m_step == Step::Done)
        return;
    m_step = Step::Done;
    m_timer->stop();
    if (m_socket) {
        // Closing without QUIT makes the server undo any DELE of this session
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    emit failed(failure, message);
}

void FetchJob::finish()
{
    if (m_step == Step::Done)
        return;
    m_step = Step::Done;
    m_timer->stop();
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->disconnectFromHost();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    emit succeeded(m_downloaded);
}

}
