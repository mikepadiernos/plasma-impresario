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

    /** @short What happened to the grant at the provider when signing out */
    enum class Revocation {
        /** No grant was stored */
        NotNeeded,
        /** The provider revoked the grant, or already considered it invalid */
        Revoked,
        /** The provider offers no revocation endpoint; see Provider::accessManagementPage */
        Unsupported,
        /** The revocation request failed; the grant may still be valid at the provider */
        Failed,
    };
    Q_ENUM(Revocation)

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
    /** @short Replace the browser of every session created afterwards, e.g. in tests of the main window */
    static void setDefaultBrowserLauncher(BrowserLauncher launcher);
    /** @short How long authorize() waits for the browser to return */
    void setAuthorizationTimeout(int milliseconds);

    bool isAuthorizing() const;

    /** @short The loopback redirect URI of the running authorization, or an empty string */
    QString redirectUri() const;

    /** @short The client secret stored with the grant, once checkSignedIn() or authorize() found or stored one */
    QString storedClientSecret() const;

    /**
     * @short Move a client secret from the settings file into the stored grant
     *
     * Calls @arg moved once the grant holds the configured secret, so that the caller can remove it from the
     * settings. Nothing happens without a configured secret or without a grant for the current configuration.
     */
    void migrateClientSecret(std::function<void()> moved);

public slots:
    /** @short Open the browser for consent and store the resulting grant; answered by authorized() or authorizationFailed() */
    void authorize();
    /** @short Stop a running authorization */
    void cancel();
    /** @short Revoke the stored grant at the provider where possible, then delete it; answered by signedOut() */
    void signOut();
    /** @short Find out whether a grant for the current configuration is stored; answered by signInStateKnown() */
    void checkSignedIn();

signals:
    void authorized();
    void authorizationFailed(const QString &message);
    void signedOut(OAuth::Session::Revocation revocation);
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
    void exchangeCodeWithSecret(const QString &code, const QString &clientSecret);
    void revoke(const Grant &grant);
    void removeGrant(Revocation revocation);
    /** @short The configuration with the client secret of the stored grant, unless one is configured */
    AccountConfig configWithSecret(const Grant &grant) const;
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
    QPointer<QNetworkReply> m_revocationReply;
    QString m_storedClientSecret;

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
