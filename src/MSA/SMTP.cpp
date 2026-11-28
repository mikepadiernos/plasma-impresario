/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>
   Copyright (C) 2013 Pali Rohár <pali.rohar@gmail.com>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "SMTP.h"
#include <QMetaMethod>
#include <QSslKey>
#include "OAuth/Session.h"
#include "UiUtils/Formatting.h"

namespace MSA
{

SMTP::SMTP(QObject *parent, const QString &host, quint16 port, bool encryptedConnect, bool startTls, bool auth,
           const QString &user):
    AbstractMSA(parent), host(host), port(port),
    encryptedConnect(encryptedConnect), startTls(startTls), auth(auth),
    user(user), failed(false), isWaitingForPassword(false), sendingMode(MODE_SMTP_INVALID)
{
    createClient();
}

void SMTP::createClient()
{
    qwwSmtp = new QwwSmtpClient(this);
    connect(qwwSmtp, &QwwSmtpClient::sslErrors, this, &SMTP::handleSslErrors);
    connect(qwwSmtp, &QwwSmtpClient::connected, this, &AbstractMSA::sending);
    connect(qwwSmtp, &QwwSmtpClient::done, this, &SMTP::handleDone);
    connect(qwwSmtp, &QwwSmtpClient::socketError, this, &SMTP::handleError);
    connect(qwwSmtp, &QwwSmtpClient::logReceived, this, [this](const QByteArray& data) {
        emit logged(Common::LogKind::LOG_IO_READ, QStringLiteral("SMTP"), QString::fromUtf8(data));
    });
    connect(qwwSmtp, &QwwSmtpClient::logSent, this, [this](const QByteArray& data) {
        emit logged(Common::LogKind::LOG_IO_WRITTEN, QStringLiteral("SMTP"), QString::fromUtf8(data));
    });
    connect(qwwSmtp, &QwwSmtpClient::saslAuthenticationFailed, this,
            [this](const QByteArray &mechanism, const QByteArray &challenge) {
        saslFailed = true;
        saslMechanism = mechanism;
        saslChallenge = challenge;
    });
}

void SMTP::setTokenSource(OAuth::TokenSource *source)
{
    if (tokenSource)
        tokenSource->disconnect(this);
    tokenSource = source;
    if (tokenSource) {
        connect(tokenSource.data(), &OAuth::TokenSource::accessTokenAvailable, this, &SMTP::oauthTokenAvailable);
        connect(tokenSource.data(), &OAuth::TokenSource::failed, this, &SMTP::oauthTokenFailed);
    }
}

void SMTP::setPinnedPublicKey(const QByteArray &pemPublicKey)
{
    pinnedPubKey = pemPublicKey;
}

void SMTP::requestOAuthToken()
{
    isWaitingForToken = true;
    tokenSource->requestAccessToken();
}

void SMTP::oauthTokenAvailable(const QString &accessToken)
{
    if (!isWaitingForToken || failed)
        return;
    isWaitingForToken = false;
    oauthToken = accessToken;
    sendContinueGotPassword();
}

void SMTP::oauthTokenFailed(const QString &message)
{
    if (!isWaitingForToken || failed)
        return;
    isWaitingForToken = false;
    failed = true;
    emit error(message);
}

bool SMTP::handleSaslFailure()
{
    saslFailed = false;
    if (!tokenSource || saslMechanism.isEmpty()) {
        failed = true;
        emit error(OAuth::errorMessage(OAuth::Error::MechanismUnavailable));
        return true;
    }
    if (!oauthRetried) {
        // The token might have been revoked or have expired early; start over once with a fresh one
        oauthRetried = true;
        qwwSmtp->disconnect(this);
        qwwSmtp->deleteLater();
        createClient();
        tokenSource->rejectAccessToken(oauthToken);
        oauthToken.clear();
        requestOAuthToken();
        return true;
    }
    QString detail = qwwSmtp->errorString();
    const QString challenge = OAuth::Sasl::describeChallenge(saslChallenge);
    if (!challenge.isEmpty())
        detail = tr("%1 (%2)").arg(detail, challenge);
    failed = true;
    emit error(OAuth::errorMessage(OAuth::Error::Rejected, detail));
    return true;
}

void SMTP::cancel()
{
    isWaitingForToken = false;
    if (isWaitingForCertificate) {
        isWaitingForCertificate = false;
        qwwSmtp->resumeAfterSslErrors(false);
    }
    qwwSmtp->disconnectFromHost();
    if (!failed) {
        failed = true;
        emit error(tr("Sending of the message was cancelled"));
    }
}

void SMTP::handleDone(bool ok)
{
    if (failed) {
        // This is a duplicate notification. The QwwSmtpClient is known to send contradicting results, see e.g. bug 321272.
        return;
    }
    if (!ok && saslFailed && handleSaslFailure())
        return;
    if (ok) {
        emit sent();
    } else {
        failed = true;
        // A 5xx reply to the sender, a recipient or the data refuses this message; 530 asks to sign in first
        const int refusal = qwwSmtp->mailRefusalCode();
        setPermanentFailure(refusal >= 500 && refusal < 600 && refusal != 530);
        if (qwwSmtp->errorString().isEmpty())
            emit error(tr("Sending of the message failed."));
        else
            emit error(tr("Sending of the message failed with the following error: %1").arg(qwwSmtp->errorString()));
    }
}

void SMTP::handleError(QAbstractSocket::SocketError err, const QString &msg)
{
    Q_UNUSED(err);
    // A TLS failure is reported through handleSslErrors() first
    if (failed)
        return;
    failed = true;
    emit error(msg);
}

void SMTP::handleSslErrors(const QList<QSslError>& errors)
{
    if (failed)
        return;
    sslErrors = errors;
    const QList<QSslCertificate> chain = qwwSmtp->peerCertificateChain();
    offeredPubKey = chain.isEmpty() ? QByteArray() : chain.first().publicKey().toPem();
    if (!pinnedPubKey.isEmpty() && offeredPubKey == pinnedPubKey) {
        // The user has already accepted this server's key
        qwwSmtp->ignoreSslErrors();
        return;
    }
    if (!isSignalConnected(QMetaMethod::fromSignal(&AbstractMSA::certificateCheckRequested))) {
        // Nobody can answer, so do not leave the paused handshake waiting forever
        failWithSslErrors();
        qwwSmtp->resumeAfterSslErrors(false);
        return;
    }
    // The handshake stays paused until the user decides
    isWaitingForCertificate = true;
    emit certificateCheckRequested(chain, errors);
}

void SMTP::setCertificateAccepted(bool accepted)
{
    // The socket pauses only once the sslErrors() slots return, so never resume from within them
    QMetaObject::invokeMethod(this, [this, accepted]() { finishCertificateCheck(accepted); }, Qt::QueuedConnection);
}

void SMTP::finishCertificateCheck(bool accepted)
{
    if (!isWaitingForCertificate)
        return;
    isWaitingForCertificate = false;
    if (accepted && !offeredPubKey.isEmpty()) {
        // An OAuth retry reconnects; do not ask about the same server again
        pinnedPubKey = offeredPubKey;
    }
    if (!accepted)
        failWithSslErrors();
    // A rejected handshake fails with a socket error, which handleError() ignores after the report above
    qwwSmtp->resumeAfterSslErrors(accepted);
}

void SMTP::failWithSslErrors()
{
    failed = true;
    emit error(tr("<p>Cannot send message due to an SSL/TLS error</p>\n%1")
                   .arg(UiUtils::Formatting::sslErrorsToHtml(sslErrors)));
}

void SMTP::setPassword(const QString &password)
{
    pass = password;
    if (isWaitingForPassword)
        sendContinueGotPassword();
}

void SMTP::sendMail(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &data)
{
    this->from = from;
    this->to = to;
    this->data = data;
    this->sendingMode = MODE_SMTP_DATA;
    this->dataEscaped = false;
    this->isWaitingForPassword = true;
    emit progressMax(data.size());
    emit progress(0);
    emit connecting();
    if (auth && tokenSource) {
        isWaitingForPassword = false;
        requestOAuthToken();
        return;
    }
    if (!auth || !pass.isEmpty()) {
        sendContinueGotPassword();
        return;
    }
    emit passwordRequested(user, host);
}

void SMTP::sendContinueGotPassword()
{
    isWaitingForPassword = false;
    if (encryptedConnect)
        qwwSmtp->connectToHostEncrypted(host, port);
    else
        qwwSmtp->connectToHost(host, port);
    if (startTls)
        qwwSmtp->startTls();
    if (auth && tokenSource) {
        QList<QwwSmtpClient::SaslCandidate> candidates;
        const auto mechanisms = tokenSource->mechanisms();
        for (const OAuth::Mechanism mechanism : mechanisms) {
            candidates.append({OAuth::mechanismName(mechanism),
                               OAuth::Sasl::initialResponse(mechanism, user, oauthToken, tokenSource->host(),
                                                            tokenSource->port()),
                               OAuth::Sasl::cancelResponse(mechanism)});
        }
        qwwSmtp->authenticateSasl(candidates);
    } else if (auth) {
        qwwSmtp->authenticate(user, pass, QwwSmtpClient::AuthAny);
    }
    emit sending(); // FIXME: later
    switch (sendingMode) {
    case MODE_SMTP_DATA:
        {
            //RFC5321 specifies to prepend a period to lines starting with a period in section 4.5.2
            // A retried submission must not escape the data twice
            if (!dataEscaped) {
                if (data.startsWith('.'))
                    data.prepend('.');
                data.replace("\n.", "\n..");
                dataEscaped = true;
            }
            qwwSmtp->sendMail(from, to, data);
        }
        break;
    case MODE_SMTP_BURL:
        qwwSmtp->sendMailBurl(from, to, data);
        break;
    default:
        failed = true;
        emit error(tr("Unknown SMTP mode"));
        break;
    }
    qwwSmtp->disconnectFromHost();
}

bool SMTP::supportsBurl() const
{
    return true;
}

void SMTP::sendBurl(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &imapUrl)
{
    this->from = from;
    this->to = to;
    this->data = imapUrl;
    this->sendingMode = MODE_SMTP_BURL;
    this->isWaitingForPassword = true;
    emit progressMax(1);
    emit progress(0);
    emit connecting();
    if (auth && tokenSource) {
        isWaitingForPassword = false;
        requestOAuthToken();
        return;
    }
    if (!auth || !pass.isEmpty()) {
        sendContinueGotPassword();
        return;
    }
    emit passwordRequested(user, host);
}

SMTPFactory::SMTPFactory(const QString &host, quint16 port, bool encryptedConnect, bool startTls,
                         bool auth, const QString &user):
    m_host(host), m_port(port), m_encryptedConnect(encryptedConnect), m_startTls(startTls),
    m_auth(auth), m_user(user)
{
}

SMTPFactory::~SMTPFactory()
{
}

AbstractMSA *SMTPFactory::create(QObject *parent) const
{
    auto smtp = new SMTP(parent, m_host, m_port, m_encryptedConnect, m_startTls, m_auth, m_user);
    if (m_oauth && m_auth) {
        auto session = new OAuth::Session(m_oauthConfig, m_passwordPlugin, smtp);
        session->setServer(m_host, m_port);
        smtp->setTokenSource(session);
    }
    smtp->setPinnedPublicKey(m_pinnedPubKey);
    return smtp;
}

void SMTPFactory::setPinnedPublicKey(const QByteArray &pemPublicKey)
{
    m_pinnedPubKey = pemPublicKey;
}

void SMTPFactory::setOAuth(const OAuth::AccountConfig &config, const OAuth::TokenStore::PluginGetter &passwordPlugin)
{
    m_oauth = true;
    m_oauthConfig = config;
    m_passwordPlugin = passwordPlugin;
}

}
