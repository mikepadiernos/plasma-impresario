/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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
#ifndef SMTP_H
#define SMTP_H

#include <QPointer>
#include "AbstractMSA.h"
#include "OAuth/OAuth.h"
#include "OAuth/TokenStore.h"
#include "qwwsmtpclient/qwwsmtpclient.h"

namespace OAuth {
class TokenSource;
}

namespace MSA
{

class SMTP : public AbstractMSA
{
    Q_OBJECT
public:
    SMTP(QObject *parent, const QString &host, quint16 port, bool encryptedConnect, bool startTls, bool auth,
         const QString &user);
    void sendMail(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &data) override;

    bool supportsBurl() const override;
    void sendBurl(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &imapUrl) override;

    /** @short Authenticate with OAuth access tokens from this source instead of a password

    Only used when authentication is enabled. The source is not owned unless it is a child of this object.
    */
    void setTokenSource(OAuth::TokenSource *source);

    /** @short Accept an untrusted certificate with this PEM public key without asking */
    void setPinnedPublicKey(const QByteArray &pemPublicKey);
public slots:
    void cancel() override;
    void setPassword(const QString &password) override;
    void setCertificateAccepted(bool accepted) override;
    void handleDone(bool ok);
    void handleError(QAbstractSocket::SocketError err, const QString &msg);
    void handleSslErrors(const QList<QSslError>& errors);
private:
    QwwSmtpClient *qwwSmtp;
    QString host;
    quint16 port;
    bool encryptedConnect;
    bool startTls;
    bool auth;
    QString user;
    QString pass;
    bool failed;
    QByteArray from;
    QList<QByteArray> to;
    QByteArray data;
    bool isWaitingForPassword;
    enum { MODE_SMTP_INVALID, MODE_SMTP_DATA, MODE_SMTP_BURL } sendingMode;
    QByteArray pinnedPubKey;
    QList<QSslError> sslErrors;
    QByteArray offeredPubKey;
    bool isWaitingForCertificate = false;
    bool dataEscaped = false;

    QPointer<OAuth::TokenSource> tokenSource;
    QString oauthToken;
    bool isWaitingForToken = false;
    bool oauthRetried = false;
    bool saslFailed = false;
    QByteArray saslMechanism;
    QByteArray saslChallenge;

    void createClient();
    void sendContinueGotPassword();
    void requestOAuthToken();
    void oauthTokenAvailable(const QString &accessToken);
    void oauthTokenFailed(const QString &message);
    bool handleSaslFailure();
    void finishCertificateCheck(bool accepted);
    void failWithSslErrors();

    SMTP(const SMTP &); // don't implement
    SMTP &operator=(const SMTP &); // don't implement
};

class SMTPFactory: public MSAFactory
{
public:
    SMTPFactory(const QString &host, quint16 port, bool encryptedConnect, bool startTls, bool auth,
         const QString &user);
    ~SMTPFactory() override;
    AbstractMSA *create(QObject *parent) const override;
    /** @short Authenticate the created submissions with this account's OAuth sign-in instead of a password */
    void setOAuth(const OAuth::AccountConfig &config, const OAuth::TokenStore::PluginGetter &passwordPlugin);
    /** @short Accept an untrusted certificate with this PEM public key, as previously confirmed by the user */
    void setPinnedPublicKey(const QByteArray &pemPublicKey);
private:
    QString m_host;
    quint16 m_port;
    bool m_encryptedConnect;
    bool m_startTls;
    bool m_auth;
    QString m_user;
    bool m_oauth = false;
    OAuth::AccountConfig m_oauthConfig;
    OAuth::TokenStore::PluginGetter m_passwordPlugin;
    QByteArray m_pinnedPubKey;
};

}

#endif // SMTP_H
