/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/ClamdScanner.h"

#include <QFileInfo>
#include <QLocalSocket>
#include <QtEndian>

namespace UiUtils {

namespace {

const QByteArray instreamCommand("zINSTREAM", 10);
const QByteArray streamPrefix("stream: ");
const QByteArray foundSuffix(" FOUND");
const QByteArray errorSuffix("ERROR");
/** At most this many bytes wait in the socket's buffer, so that a large file is not read into memory at once */
const int bufferedChunks = 4;

}

ClamdScanner::ClamdScanner(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(60 * 1000);
    connect(&m_timer, &QTimer::timeout, this, [this]() { finish(MailSafety::ScanResult::Failed); });
}

ClamdScanner::~ClamdScanner()
{
    m_running = false;
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
    }
}

QStringList ClamdScanner::defaultSockets()
{
    return {
        QStringLiteral("/run/clamav/clamd.ctl"),
        QStringLiteral("/var/run/clamav/clamd.ctl"),
        QStringLiteral("/run/clamd.scan/clamd.sock"),
        QStringLiteral("/var/run/clamd.scan/clamd.sock"),
    };
}

QString ClamdScanner::findSocket(const QStringList &candidates)
{
    for (const QString &candidate : candidates) {
        if (!candidate.isEmpty() && QFileInfo::exists(candidate))
            return candidate;
    }
    return QString();
}

QByteArray ClamdScanner::chunk(const QByteArray &data)
{
    QByteArray framed(4, '\0');
    qToBigEndian<quint32>(quint32(data.size()), framed.data());
    return framed + data;
}

MailSafety::ScanResult ClamdScanner::parseReply(const QByteArray &reply, QString *threat)
{
    QByteArray text = reply;
    const int end = text.indexOf('\0');
    if (end >= 0)
        text.truncate(end);
    text = text.trimmed();
    if (text.endsWith(foundSuffix)) {
        if (threat) {
            QByteArray name = text.chopped(foundSuffix.size());
            if (name.startsWith(streamPrefix))
                name.remove(0, streamPrefix.size());
            *threat = QString::fromUtf8(name.trimmed());
        }
        return MailSafety::ScanResult::Infected;
    }
    if (text == streamPrefix + QByteArrayLiteral("OK"))
        return MailSafety::ScanResult::Clean;
    // An ERROR reply, such as a size limit, and anything unexpected are failed scans, never clean ones
    return MailSafety::ScanResult::Failed;
}

void ClamdScanner::setTimeout(int milliseconds)
{
    m_timer.setInterval(milliseconds);
}

void ClamdScanner::setChunkSize(int bytes)
{
    m_chunkSize = qMax(1, bytes);
}

void ClamdScanner::scanFile(const QString &path, const QString &socketPath)
{
    Q_ASSERT(!m_running);
    m_running = true;
    m_connected = false;
    m_sentEnd = false;
    m_reply.clear();

    if (socketPath.isEmpty()) {
        finish(MailSafety::ScanResult::Unavailable);
        return;
    }
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadOnly)) {
        finish(MailSafety::ScanResult::Failed);
        return;
    }

    m_socket = new QLocalSocket(this);
    connect(m_socket, &QLocalSocket::connected, this, &ClamdScanner::onConnected);
    connect(m_socket, &QLocalSocket::bytesWritten, this, &ClamdScanner::sendMore);
    connect(m_socket, &QLocalSocket::readyRead, this, &ClamdScanner::onReadyRead);
    connect(m_socket, &QLocalSocket::disconnected, this, &ClamdScanner::onDisconnected);
    connect(m_socket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError error) {
        if (error == QLocalSocket::PeerClosedError && m_connected)
            onDisconnected();
        else
            finish(m_connected ? MailSafety::ScanResult::Failed : MailSafety::ScanResult::Unavailable);
    });
    m_timer.start();
    m_socket->connectToServer(socketPath);
}

void ClamdScanner::onConnected()
{
    m_connected = true;
    m_socket->write(instreamCommand);
    sendMore();
}

void ClamdScanner::sendMore()
{
    if (!m_running || !m_connected || m_sentEnd || !m_socket)
        return;
    while (m_socket->bytesToWrite() < qint64(bufferedChunks) * m_chunkSize && !m_file.atEnd()) {
        const QByteArray data = m_file.read(m_chunkSize);
        if (data.isEmpty()) {
            if (m_file.error() != QFileDevice::NoError) {
                finish(MailSafety::ScanResult::Failed);
                return;
            }
            break;
        }
        m_socket->write(chunk(data));
    }
    if (m_file.atEnd()) {
        // A chunk of length zero ends the stream
        m_socket->write(QByteArray(4, '\0'));
        m_sentEnd = true;
        m_file.close();
    }
}

void ClamdScanner::onReadyRead()
{
    if (!m_running || !m_socket)
        return;
    m_reply += m_socket->readAll();
    if (m_reply.contains('\0')) {
        QString threat;
        const MailSafety::ScanResult result = parseReply(m_reply, &threat);
        finish(result, threat);
    }
}

void ClamdScanner::onDisconnected()
{
    if (!m_running)
        return;
    if (m_socket)
        m_reply += m_socket->readAll();
    QString threat;
    const MailSafety::ScanResult result = m_reply.trimmed().isEmpty() ? MailSafety::ScanResult::Failed
                                                                      : parseReply(m_reply, &threat);
    finish(result, threat);
}

void ClamdScanner::finish(MailSafety::ScanResult result, const QString &threat)
{
    if (!m_running)
        return;
    m_running = false;
    m_timer.stop();
    m_file.close();
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    // Queued, so that a receiver may open a dialog or delete the scanner without the socket's call stack below it
    QMetaObject::invokeMethod(this, [this, result, threat]() { emit finished(result, threat); }, Qt::QueuedConnection);
}

}
