/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Session.h"

#include <QDebug>
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>
#include <memory>

namespace OAuth {

namespace {
/** @short Renew access tokens this many seconds before they expire */
constexpr qint64 expiryMargin = 60;
constexpr int defaultAuthorizationTimeout = 5 * 60 * 1000;
constexpr int maximumRequestSize = 16 * 1024;

Session::BrowserLauncher &defaultLauncher()
{
    static Session::BrowserLauncher launcher = [](const QUrl &url) { return QDesktopServices::openUrl(url); };
    return launcher;
}
}

Session::Session(const AccountConfig &config, TokenStore::PluginGetter passwordPlugin, QObject *parent)
    : TokenSource(parent)
    , m_config(config)
    , m_store(std::move(passwordPlugin))
    , m_launcher(defaultLauncher())
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(defaultAuthorizationTimeout);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        stopListening();
        dropReply(m_exchangeReply);
        failAuthorization(Error::TimedOut);
    });
}

Session::~Session()
{
    dropReply(m_refreshReply);
    dropReply(m_exchangeReply);
    dropReply(m_revocationReply);
}

const AccountConfig &Session::config() const
{
    return m_config;
}

QList<Mechanism> Session::mechanisms() const
{
    return m_config.provider.mechanisms;
}

void Session::setNetworkAccessManager(QNetworkAccessManager *manager)
{
    m_manager = manager;
}

void Session::setBrowserLauncher(BrowserLauncher launcher)
{
    m_launcher = std::move(launcher);
}

void Session::setDefaultBrowserLauncher(BrowserLauncher launcher)
{
    defaultLauncher() = std::move(launcher);
}

void Session::setAuthorizationTimeout(int milliseconds)
{
    m_timeout.setInterval(milliseconds);
}

bool Session::isAuthorizing() const
{
    return m_authorizing;
}

QString Session::redirectUri() const
{
    return m_authorizing ? m_redirectUri : QString();
}

QString Session::storedClientSecret() const
{
    return m_storedClientSecret;
}

AccountConfig Session::configWithSecret(const Grant &grant) const
{
    AccountConfig config = m_config;
    if (config.clientSecret.isEmpty())
        config.clientSecret = grant.clientSecret;
    return config;
}

QNetworkAccessManager *Session::networkAccessManager()
{
    if (!m_manager)
        m_manager = new QNetworkAccessManager(this);
    return m_manager;
}

QNetworkReply *Session::postToTokenEndpoint(const QByteArray &body)
{
    QNetworkRequest request(m_config.provider.tokenEndpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/x-www-form-urlencoded"));
    request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
    return networkAccessManager()->post(request, body);
}

TokenResponse Session::readTokenReply(QNetworkReply *reply) const
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 0) {
        TokenResponse response;
        response.error = Error::Network;
        response.detail = reply->errorString();
        return response;
    }
    return parseTokenResponse(reply->readAll(), status);
}

void Session::dropReply(QPointer<QNetworkReply> &reply)
{
    if (!reply)
        return;
    reply->disconnect();
    reply->abort();
    reply->deleteLater();
    reply.clear();
}

void Session::cacheAccessToken(const TokenResponse &response)
{
    m_accessToken = response.accessToken;
    // Without a lifetime, use the token once and renew it for the next connection
    m_accessTokenExpiry = QDateTime::currentDateTimeUtc().addSecs(response.expiresIn);
}

void Session::requestAccessToken()
{
    if (!m_accessToken.isEmpty() && QDateTime::currentDateTimeUtc().secsTo(m_accessTokenExpiry) > expiryMargin) {
        const QString token = m_accessToken;
        QTimer::singleShot(0, this, [this, token] { emit accessTokenAvailable(token); });
        return;
    }
    if (m_refreshing)
        return;
    m_refreshing = true;
    if (!m_config.isValid()) {
        QTimer::singleShot(0, this, [this] { finishRefresh(Error::NotConfigured, QString()); });
        return;
    }
    // Always read the stored grant, as another session of this account may have rotated the refresh token
    m_store.load(m_config.accountId, this, [this](Error error, const Grant &grant) {
        if (error == Error::NoError && !grant.matches(m_config))
            error = Error::NotSignedIn;
        if (error != Error::NoError) {
            finishRefresh(error, QString());
            return;
        }
        m_storedClientSecret = grant.clientSecret;
        m_refreshReply = postToTokenEndpoint(refreshRequestBody(configWithSecret(grant), grant.refreshToken));
        connect(m_refreshReply, &QNetworkReply::finished, this, [this, grant] {
            QNetworkReply *reply = m_refreshReply;
            m_refreshReply.clear();
            reply->deleteLater();
            const TokenResponse response = readTokenReply(reply);
            if (response.error != Error::NoError) {
                finishRefresh(response.error, response.detail);
                return;
            }
            cacheAccessToken(response);
            if (!response.refreshToken.isEmpty() && response.refreshToken != grant.refreshToken) {
                Grant rotated = grant;
                rotated.refreshToken = response.refreshToken;
                rotated.clientSecret = configWithSecret(grant).clientSecret;
                m_store.save(m_config.accountId, rotated, this, [](Error error) {
                    if (error != Error::NoError)
                        qWarning() << "OAuth: cannot store the renewed refresh token";
                });
            }
            finishRefresh(Error::NoError, QString());
        });
    });
}

void Session::finishRefresh(Error error, const QString &detail)
{
    m_refreshing = false;
    setLastError(error);
    if (error == Error::NoError)
        emit accessTokenAvailable(m_accessToken);
    else
        emit failed(errorMessage(error, detail));
}

void Session::rejectAccessToken(const QString &accessToken)
{
    if (accessToken == m_accessToken) {
        m_accessToken.clear();
        m_accessTokenExpiry = QDateTime();
    }
}

void Session::authorize()
{
    if (m_authorizing)
        return;
    if (!m_config.isValid()) {
        failAuthorization(Error::NotConfigured);
        return;
    }
    if (m_store.availability() != Error::NoError) {
        failAuthorization(Error::StorageUnavailable);
        return;
    }

    auto *server = new QTcpServer(this);
    if (!server->listen(QHostAddress::LocalHost, 0)) {
        const QString detail = server->errorString();
        delete server;
        failAuthorization(Error::Network, detail);
        return;
    }
    m_servers << server;
    const quint16 port = server->serverPort();
    if (m_config.provider.redirectHost != QLatin1String("127.0.0.1")) {
        // A host name such as "localhost" may resolve to the IPv6 loopback first
        auto *server6 = new QTcpServer(this);
        if (server6->listen(QHostAddress::LocalHostIPv6, port))
            m_servers << server6;
        else
            delete server6;
    }
    for (QTcpServer *s : std::as_const(m_servers))
        connect(s, &QTcpServer::newConnection, this, [this, s] { handleConnection(s); });

    m_authorizing = true;
    m_redirectUri = QStringLiteral("http://%1:%2").arg(m_config.provider.redirectHost).arg(port);
    m_state = generateState();
    m_codeVerifier = Pkce::generateVerifier();
    m_timeout.start();
    if (!m_launcher || !m_launcher(authorizationUrl(m_config, m_redirectUri, m_state, Pkce::challenge(m_codeVerifier)))) {
        stopListening();
        failAuthorization(Error::BrowserUnavailable);
    }
}

void Session::handleConnection(QTcpServer *server)
{
    while (QTcpSocket *socket = server->nextPendingConnection()) {
        // Keep the socket while the reply is written, even after the servers are gone
        socket->setParent(this);
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        auto buffer = std::make_shared<QByteArray>();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer] {
            *buffer += socket->readAll();
            if (buffer->size() > maximumRequestSize) {
                socket->abort();
                return;
            }
            if (!buffer->contains("\r\n\r\n"))
                return;
            disconnect(socket, &QTcpSocket::readyRead, this, nullptr);
            handleRedirectRequest(socket, *buffer);
        });
    }
}

void Session::handleRedirectRequest(QTcpSocket *socket, const QByteArray &request)
{
    const QList<QByteArray> requestLine = request.left(request.indexOf("\r\n")).split(' ');
    if (requestLine.size() < 3 || requestLine[0] != "GET") {
        respond(socket, 405, "Method Not Allowed", QString());
        return;
    }
    const QUrl target(QStringLiteral("http://localhost") + QString::fromLatin1(requestLine[1]));
    if (!m_authorizing || (target.path() != QLatin1String("/") && !target.path().isEmpty())) {
        respond(socket, 404, "Not Found", QString());
        return;
    }
    const QUrlQuery query(target);
    if (query.queryItemValue(QStringLiteral("state"), QUrl::FullyDecoded) != QString::fromLatin1(m_state)) {
        // Not the browser we started; keep waiting for the real redirect
        respond(socket, 400, "Bad Request", Strings::tr("This sign-in request does not belong to Plasma Impresario."));
        return;
    }
    const QString error = query.queryItemValue(QStringLiteral("error"), QUrl::FullyDecoded);
    if (!error.isEmpty()) {
        const QString description = query.queryItemValue(QStringLiteral("error_description"), QUrl::FullyDecoded);
        respond(socket, 200, "OK", Strings::tr("Sign-in was not completed. You can close this window and return to "
                                               "Plasma Impresario."));
        stopListening();
        failAuthorization(classifyProviderError(error, description),
                          description.isEmpty() ? error : QStringLiteral("%1: %2").arg(error, description));
        return;
    }
    const QString code = query.queryItemValue(QStringLiteral("code"), QUrl::FullyDecoded);
    if (code.isEmpty()) {
        respond(socket, 400, "Bad Request", Strings::tr("The sign-in response contains no authorization code."));
        return;
    }
    respond(socket, 200, "OK", Strings::tr("Sign-in is complete. You can close this window and return to Plasma "
                                           "Impresario."));
    stopListening();
    exchangeCode(code);
}

void Session::respond(QTcpSocket *socket, int status, const QByteArray &reason, const QString &message)
{
    QByteArray body;
    if (!message.isEmpty()) {
        body = "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>Plasma Impresario</title></head><body><p>"
            + message.toHtmlEscaped().toUtf8() + "</p></body></html>";
    }
    socket->write("HTTP/1.1 " + QByteArray::number(status) + ' ' + reason + "\r\n"
                  "Content-Type: text/html; charset=utf-8\r\n"
                  "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                  "Cache-Control: no-store\r\n"
                  "Connection: close\r\n\r\n" + body);
    socket->disconnectFromHost();
}

void Session::exchangeCode(const QString &code)
{
    m_timeout.start();
    if (!m_config.clientSecret.isEmpty()) {
        exchangeCodeWithSecret(code, m_config.clientSecret);
        return;
    }
    // Signing in again without entering the secret keeps the one stored with the previous grant for this client
    m_store.load(m_config.accountId, this, [this, code](Error error, const Grant &grant) {
        if (!m_authorizing)
            return;
        exchangeCodeWithSecret(code, error == Error::NoError && grant.matches(m_config) ? grant.clientSecret
                                                                                         : QString());
    });
}

void Session::exchangeCodeWithSecret(const QString &code, const QString &clientSecret)
{
    AccountConfig config = m_config;
    config.clientSecret = clientSecret;
    m_exchangeReply = postToTokenEndpoint(authorizationCodeRequestBody(config, code, m_redirectUri, m_codeVerifier));
    connect(m_exchangeReply, &QNetworkReply::finished, this, [this, clientSecret] {
        QNetworkReply *reply = m_exchangeReply;
        m_exchangeReply.clear();
        reply->deleteLater();
        const TokenResponse response = readTokenReply(reply);
        if (response.error != Error::NoError) {
            failAuthorization(response.error, response.detail);
            return;
        }
        if (response.refreshToken.isEmpty()) {
            failAuthorization(Error::Provider, Strings::tr("The sign-in service did not issue a refresh token."));
            return;
        }
        Grant grant;
        grant.provider = m_config.provider.id;
        grant.clientId = m_config.clientId;
        grant.refreshToken = response.refreshToken;
        grant.clientSecret = clientSecret;
        m_store.save(m_config.accountId, grant, this, [this, response, clientSecret](Error error) {
            if (!m_authorizing)
                return;
            if (error != Error::NoError) {
                failAuthorization(error);
                return;
            }
            m_storedClientSecret = clientSecret;
            m_timeout.stop();
            cacheAccessToken(response);
            m_authorizing = false;
            setLastError(Error::NoError);
            emit authorized();
        });
    });
}

void Session::stopListening()
{
    for (QTcpServer *server : std::as_const(m_servers)) {
        server->close();
        server->deleteLater();
    }
    m_servers.clear();
}

void Session::failAuthorization(Error error, const QString &detail)
{
    m_timeout.stop();
    m_authorizing = false;
    m_state.clear();
    m_codeVerifier.clear();
    setLastError(error);
    emit authorizationFailed(errorMessage(error, detail));
}

void Session::cancel()
{
    if (!m_authorizing)
        return;
    stopListening();
    dropReply(m_exchangeReply);
    failAuthorization(Error::Cancelled);
}

void Session::signOut()
{
    m_accessToken.clear();
    m_accessTokenExpiry = QDateTime();
    if (m_revocationReply)
        return;
    m_store.load(m_config.accountId, this, [this](Error error, const Grant &grant) {
        if (error != Error::NoError || !grant.isValid()) {
            removeGrant(Revocation::NotNeeded);
            return;
        }
        revoke(grant);
    });
}

void Session::revoke(const Grant &grant)
{
    // The grant may belong to a provider or client which is no longer configured; revoke it where it was issued
    const bool current = grant.provider == m_config.provider.id;
    const Provider issuer = current ? m_config.provider : provider(grant.provider);
    if (!issuer.revocationEndpoint.isValid()) {
        removeGrant(Revocation::Unsupported);
        return;
    }
    AccountConfig config = configWithSecret(grant);
    config.provider = issuer;
    if (grant.clientId != config.clientId) {
        config.clientId = grant.clientId;
        config.clientSecret = grant.clientSecret;
    }
    QNetworkRequest request(issuer.revocationEndpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/x-www-form-urlencoded"));
    m_revocationReply = networkAccessManager()->post(request, revocationRequestBody(config, grant.refreshToken));
    connect(m_revocationReply, &QNetworkReply::finished, this, [this] {
        QNetworkReply *reply = m_revocationReply;
        m_revocationReply.clear();
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // RFC 7009 section 2.2: an invalid token counts as revoked
        const bool revoked = (status >= 200 && status < 300)
            || (status == 400 && parseTokenResponse(reply->readAll(), status).detail.startsWith(
                    QLatin1String("invalid_token")));
        if (!revoked)
            qWarning() << "OAuth: the provider did not revoke the grant, HTTP status" << status << reply->errorString();
        removeGrant(revoked ? Revocation::Revoked : Revocation::Failed);
    });
}

void Session::removeGrant(Revocation revocation)
{
    // Delete the local grant even when revoking failed, so that signing out always takes effect here
    m_store.remove(m_config.accountId, this, [this, revocation](Error error) {
        setLastError(error);
        if (error == Error::NoError) {
            m_storedClientSecret.clear();
            emit signedOut(revocation);
        } else {
            emit signOutFailed(errorMessage(error));
        }
    });
}

void Session::migrateClientSecret(std::function<void()> moved)
{
    if (m_config.clientSecret.isEmpty() || !m_config.isValid())
        return;
    const QString secret = m_config.clientSecret;
    m_store.load(m_config.accountId, this, [this, secret, moved](Error error, const Grant &grant) {
        if (error != Error::NoError || !grant.matches(m_config))
            return;
        if (grant.clientSecret == secret) {
            m_storedClientSecret = secret;
            moved();
            return;
        }
        if (!grant.clientSecret.isEmpty())
            return;
        Grant updated = grant;
        updated.clientSecret = secret;
        m_store.save(m_config.accountId, updated, this, [this, secret, moved](Error error) {
            if (error != Error::NoError)
                return;
            m_storedClientSecret = secret;
            moved();
        });
    });
}

void Session::checkSignedIn()
{
    m_store.load(m_config.accountId, this, [this](Error error, const Grant &grant) {
        if (error == Error::StorageUnavailable)
            setLastError(error);
        const bool signedIn = error == Error::NoError && grant.matches(m_config);
        m_storedClientSecret = signedIn ? grant.clientSecret : QString();
        emit signInStateKnown(signedIn);
    });
}

}
