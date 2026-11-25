/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef OAUTH_SESSION_H
#define OAUTH_SESSION_H

#include <QDateTime>
#include <QPointer>
#include <QTimer>
#include <functional>

#include "OAuth/TokenSource.h"
#include "OAuth/TokenStore.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTcpServer;
class QTcpSocket;

namespace OAuth {

/** @short OAuth 2.0 client of one account

authorize() runs the authorization code flow with PKCE (RFC 7636) in the system browser and receives the redirect
on a loopback port (RFC 8252). The resulting refresh token is stored per account in the TokenStore. Access tokens are
cached in memory and renewed with the refresh token shortly before they expire. Concurrent token requests share one
refresh.
*/
class Session : public TokenSource
{
    Q_OBJECT
public:
    using BrowserLauncher = std::function<bool(const QUrl &)>;

    Session(const AccountConfig &config, TokenStore::PluginGetter passwordPlugin, QObject *parent = nullptr);
    ~Session() override;

    const AccountConfig &config() const;

    QList<Mechanism> mechanisms() const override;
    void requestAccessToken() override;
    void rejectAccessToken(const QString &accessToken) override;

    /** @short Use this network access manager instead of a private one */
    void setNetworkAccessManager(QNetworkAccessManager *manager);
    /** @short Replace the system browser, e.g. in tests; returns false when the URL cannot be opened */
    void setBrowserLauncher(BrowserLauncher launcher);
    /** @short How long authorize() waits for the browser to return */
    void setAuthorizationTimeout(int milliseconds);

    bool isAuthorizing() const;

    /** @short The loopback redirect URI of the running authorization, or an empty string */
    QString redirectUri() const;

public slots:
    /** @short Open the browser for consent and store the resulting grant; answered by authorized() or authorizationFailed() */
    void authorize();
    /** @short Stop a running authorization */
    void cancel();
    /** @short Forget the cached access token and delete the stored grant */
    void signOut();
    /** @short Find out whether a grant for the current configuration is stored; answered by signInStateKnown() */
    void checkSignedIn();

signals:
    void authorized();
    void authorizationFailed(const QString &message);
    void signedOut();
    void signOutFailed(const QString &message);
    void signInStateKnown(bool signedIn);

private:
    QNetworkAccessManager *networkAccessManager();
    QNetworkReply *postToTokenEndpoint(const QByteArray &body);
    TokenResponse readTokenReply(QNetworkReply *reply) const;
    void cacheAccessToken(const TokenResponse &response);
    void finishRefresh(Error error, const QString &detail);
    void handleConnection(QTcpServer *server);
    void handleRedirectRequest(QTcpSocket *socket, const QByteArray &request);
    void respond(QTcpSocket *socket, int status, const QByteArray &reason, const QString &message);
    void exchangeCode(const QString &code);
    void stopListening();
    void failAuthorization(Error error, const QString &detail = QString());
    static void dropReply(QPointer<QNetworkReply> &reply);

    AccountConfig m_config;
    TokenStore m_store;
    QPointer<QNetworkAccessManager> m_manager;
    BrowserLauncher m_launcher;
    QString m_accessToken;
    QDateTime m_accessTokenExpiry;
    bool m_refreshing = false;
    QPointer<QNetworkReply> m_refreshReply;

    bool m_authorizing = false;
    QList<QTcpServer *> m_servers;
    QTimer m_timeout;
    QString m_redirectUri;
    QByteArray m_state;
    QByteArray m_codeVerifier;
    QPointer<QNetworkReply> m_exchangeReply;
};

}

#endif
