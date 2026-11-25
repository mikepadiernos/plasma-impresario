/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "OAuth.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSettings>

#include "Common/SettingsNames.h"

#ifndef IMPRESARIO_OAUTH_GOOGLE_CLIENT_ID
#define IMPRESARIO_OAUTH_GOOGLE_CLIENT_ID ""
#endif
#ifndef IMPRESARIO_OAUTH_MICROSOFT_CLIENT_ID
#define IMPRESARIO_OAUTH_MICROSOFT_CLIENT_ID ""
#endif

namespace OAuth {

namespace {

QByteArray base64Url(const QByteArray &data)
{
    return data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

QByteArray randomBytes(int size)
{
    QByteArray bytes(size, Qt::Uninitialized);
    for (int i = 0; i < size; ++i)
        bytes[i] = static_cast<char>(QRandomGenerator::system()->bounded(256));
    return bytes;
}

void appendClient(QList<QPair<QString, QString>> &items, const AccountConfig &config)
{
    items.append({QStringLiteral("client_id"), config.clientId});
    if (!config.clientSecret.isEmpty())
        items.append({QStringLiteral("client_secret"), config.clientSecret});
}

}

QByteArray mechanismName(Mechanism mechanism)
{
    switch (mechanism) {
    case Mechanism::XOAuth2:
        return QByteArrayLiteral("XOAUTH2");
    case Mechanism::OAuthBearer:
        return QByteArrayLiteral("OAUTHBEARER");
    case Mechanism::None:
        break;
    }
    return QByteArray();
}

Mechanism mechanismFromName(const QString &name)
{
    if (name.compare(QLatin1String("XOAUTH2"), Qt::CaseInsensitive) == 0)
        return Mechanism::XOAuth2;
    if (name.compare(QLatin1String("OAUTHBEARER"), Qt::CaseInsensitive) == 0)
        return Mechanism::OAuthBearer;
    return Mechanism::None;
}

const QList<Provider> &providers()
{
    static const QList<Provider> list = [] {
        Provider google;
        google.id = QStringLiteral("google");
        google.displayName = QStringLiteral("Google");
        google.authorizationEndpoint = QUrl(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth"));
        google.tokenEndpoint = QUrl(QStringLiteral("https://oauth2.googleapis.com/token"));
        google.scopes = {QStringLiteral("https://mail.google.com/")};
        // Google issues a refresh token only for offline access, and again only after a new consent
        google.extraAuthorizationParameters = {{QStringLiteral("access_type"), QStringLiteral("offline")},
                                               {QStringLiteral("prompt"), QStringLiteral("consent")}};
        google.redirectHost = QStringLiteral("127.0.0.1");
        google.mechanisms = {Mechanism::XOAuth2, Mechanism::OAuthBearer};
        google.defaultClientId = QString::fromUtf8(IMPRESARIO_OAUTH_GOOGLE_CLIENT_ID);

        Provider microsoft;
        microsoft.id = QStringLiteral("microsoft");
        microsoft.displayName = QStringLiteral("Microsoft");
        microsoft.authorizationEndpoint = QUrl(QStringLiteral("https://login.microsoftonline.com/common/oauth2/v2.0/authorize"));
        microsoft.tokenEndpoint = QUrl(QStringLiteral("https://login.microsoftonline.com/common/oauth2/v2.0/token"));
        microsoft.scopes = {QStringLiteral("https://outlook.office.com/IMAP.AccessAsUser.All"),
                            QStringLiteral("https://outlook.office.com/SMTP.Send"),
                            QStringLiteral("offline_access")};
        // Microsoft accepts any port with a "http://localhost" redirect URI
        microsoft.redirectHost = QStringLiteral("localhost");
        microsoft.mechanisms = {Mechanism::XOAuth2};
        microsoft.defaultClientId = QString::fromUtf8(IMPRESARIO_OAUTH_MICROSOFT_CLIENT_ID);

        return QList<Provider>{google, microsoft};
    }();
    return list;
}

Provider provider(const QString &id)
{
    for (const Provider &p : providers()) {
        if (p.id == id)
            return p;
    }
    return Provider();
}

QString errorMessage(Error error, const QString &detail)
{
    QString message;
    switch (error) {
    case Error::NoError:
        return QString();
    case Error::NotConfigured:
        message = Strings::tr("OAuth sign-in is not configured for this account. Open the account settings, choose a "
                              "provider and enter the OAuth client ID.");
        break;
    case Error::NotSignedIn:
        message = Strings::tr("This account is not signed in. Open the account settings and choose Sign In.");
        break;
    case Error::Expired:
        message = Strings::tr("The sign-in for this account has expired. Open the account settings and choose Sign In "
                              "again.");
        break;
    case Error::Revoked:
        message = Strings::tr("The sign-in for this account was revoked or is no longer valid. Open the account "
                              "settings and choose Sign In again.");
        break;
    case Error::ConsentDenied:
        message = Strings::tr("Access to the mailbox was not granted in the browser. Choose Sign In again and allow "
                              "access.");
        break;
    case Error::InvalidClient:
        message = Strings::tr("The provider did not accept the OAuth client. Check the client ID, and the client "
                              "secret if your provider issued one, in the account settings.");
        break;
    case Error::Cancelled:
        message = Strings::tr("Sign-in was cancelled.");
        break;
    case Error::TimedOut:
        message = Strings::tr("Sign-in timed out before the browser returned to Plasma Impresario. Choose Sign In to "
                              "try again.");
        break;
    case Error::BrowserUnavailable:
        message = Strings::tr("The web browser for signing in could not be opened. Check that a default web browser "
                              "is configured, then choose Sign In again.");
        break;
    case Error::StorageUnavailable:
        message = Strings::tr("Secure credential storage is not available, so the sign-in cannot be stored or read. "
                              "Enable an encrypted password backend, such as KWallet or the Secret Service, then sign "
                              "in again.");
        break;
    case Error::Network:
        message = Strings::tr("The sign-in service could not be reached. Check the network connection and try again.");
        break;
    case Error::Provider:
        message = Strings::tr("The sign-in service reported an error. Try again later, or choose Sign In again in the "
                              "account settings.");
        break;
    case Error::MechanismUnavailable:
        message = Strings::tr("The server does not offer OAuth authentication. Check the server settings, or choose "
                              "password authentication in the account settings.");
        break;
    case Error::Rejected:
        message = Strings::tr("The server rejected the OAuth sign-in. Open the account settings and choose Sign In "
                              "again.");
        break;
    }
    if (!detail.isEmpty())
        message = Strings::tr("%1\n\nDetails: %2").arg(message, detail);
    return message;
}

Error classifyProviderError(const QString &code, const QString &description)
{
    if (code == QLatin1String("access_denied"))
        return Error::ConsentDenied;
    if (code == QLatin1String("invalid_client") || code == QLatin1String("unauthorized_client"))
        return Error::InvalidClient;
    if (code == QLatin1String("invalid_grant")) {
        // Microsoft reports expired refresh tokens as AADSTS70008 or AADSTS700082; Google says "expired"
        if (description.contains(QLatin1String("expired"), Qt::CaseInsensitive)
            || description.contains(QLatin1String("AADSTS70008")))
            return Error::Expired;
        return Error::Revoked;
    }
    return Error::Provider;
}

bool isEnabled(const QSettings &settings, Service service)
{
    const QString &key = service == Service::Imap ? Common::SettingsNames::imapAuthMethodKey
                                                  : Common::SettingsNames::smtpAuthMethodKey;
    return settings.value(key, Common::SettingsNames::authMethodPassword).toString()
        == Common::SettingsNames::authMethodOAuth;
}

AccountConfig AccountConfig::fromSettings(const QSettings &settings, const QString &accountId)
{
    AccountConfig config;
    config.accountId = accountId;
    config.provider = OAuth::provider(settings.value(Common::SettingsNames::oauthProviderKey).toString());
    config.clientId = settings.value(Common::SettingsNames::oauthClientIdKey).toString().trimmed();
    if (config.clientId.isEmpty())
        config.clientId = config.provider.defaultClientId;
    config.clientSecret = settings.value(Common::SettingsNames::oauthClientSecretKey).toString().trimmed();
    config.loginHint = settings.value(Common::SettingsNames::imapUserKey).toString();
    return config;
}

namespace Pkce {

QByteArray generateVerifier()
{
    // 32 random octets give a 43 character verifier, RFC 7636 section 4.1
    return base64Url(randomBytes(32));
}

QByteArray challenge(const QByteArray &verifier)
{
    return base64Url(QCryptographicHash::hash(verifier, QCryptographicHash::Sha256));
}

}

QByteArray generateState()
{
    return base64Url(randomBytes(16));
}

QByteArray formEncode(const QList<QPair<QString, QString>> &items)
{
    QByteArray result;
    for (const auto &item : items) {
        if (!result.isEmpty())
            result += '&';
        result += QUrl::toPercentEncoding(item.first) + '=' + QUrl::toPercentEncoding(item.second);
    }
    return result;
}

QUrl authorizationUrl(const AccountConfig &config, const QString &redirectUri, const QByteArray &state,
                      const QByteArray &codeChallenge)
{
    QList<QPair<QString, QString>> items = {
        {QStringLiteral("response_type"), QStringLiteral("code")},
        {QStringLiteral("client_id"), config.clientId},
        {QStringLiteral("redirect_uri"), redirectUri},
        {QStringLiteral("scope"), config.provider.scopes.join(QLatin1Char(' '))},
        {QStringLiteral("state"), QString::fromLatin1(state)},
        {QStringLiteral("code_challenge"), QString::fromLatin1(codeChallenge)},
        {QStringLiteral("code_challenge_method"), QStringLiteral("S256")},
    };
    if (!config.loginHint.isEmpty())
        items.append({QStringLiteral("login_hint"), config.loginHint});
    items += config.provider.extraAuthorizationParameters;
    QUrl url = config.provider.authorizationEndpoint;
    url.setQuery(QString::fromLatin1(formEncode(items)), QUrl::StrictMode);
    return url;
}

QByteArray authorizationCodeRequestBody(const AccountConfig &config, const QString &code, const QString &redirectUri,
                                        const QByteArray &codeVerifier)
{
    QList<QPair<QString, QString>> items = {
        {QStringLiteral("grant_type"), QStringLiteral("authorization_code")},
        {QStringLiteral("code"), code},
        {QStringLiteral("redirect_uri"), redirectUri},
        {QStringLiteral("code_verifier"), QString::fromLatin1(codeVerifier)},
    };
    appendClient(items, config);
    return formEncode(items);
}

QByteArray refreshRequestBody(const AccountConfig &config, const QString &refreshToken)
{
    QList<QPair<QString, QString>> items = {
        {QStringLiteral("grant_type"), QStringLiteral("refresh_token")},
        {QStringLiteral("refresh_token"), refreshToken},
    };
    appendClient(items, config);
    return formEncode(items);
}

TokenResponse parseTokenResponse(const QByteArray &body, int httpStatus)
{
    TokenResponse response;
    const QJsonObject json = QJsonDocument::fromJson(body).object();
    const QString errorCode = json.value(QLatin1String("error")).toString();
    if (!errorCode.isEmpty()) {
        const QString description = json.value(QLatin1String("error_description")).toString();
        response.error = classifyProviderError(errorCode, description);
        response.detail = description.isEmpty() ? errorCode : QStringLiteral("%1: %2").arg(errorCode, description);
        return response;
    }
    if (httpStatus < 200 || httpStatus > 299) {
        response.error = Error::Provider;
        response.detail = Strings::tr("HTTP status %1").arg(httpStatus);
        return response;
    }
    response.accessToken = json.value(QLatin1String("access_token")).toString();
    if (response.accessToken.isEmpty()) {
        response.error = Error::Provider;
        response.detail = Strings::tr("The reply contains no access token.");
        return response;
    }
    response.refreshToken = json.value(QLatin1String("refresh_token")).toString();
    const QJsonValue expiresIn = json.value(QLatin1String("expires_in"));
    response.expiresIn = expiresIn.isString() ? expiresIn.toString().toLongLong() : expiresIn.toInteger();
    return response;
}

QString credentialType()
{
    return QStringLiteral("oauth");
}

bool Grant::matches(const AccountConfig &config) const
{
    return isValid() && provider == config.provider.id && clientId == config.clientId;
}

QString Grant::toJson() const
{
    QJsonObject json;
    json.insert(QLatin1String("provider"), provider);
    json.insert(QLatin1String("clientId"), clientId);
    json.insert(QLatin1String("refreshToken"), refreshToken);
    return QString::fromUtf8(QJsonDocument(json).toJson(QJsonDocument::Compact));
}

Grant Grant::fromJson(const QString &json)
{
    const QJsonObject object = QJsonDocument::fromJson(json.toUtf8()).object();
    Grant grant;
    grant.provider = object.value(QLatin1String("provider")).toString();
    grant.clientId = object.value(QLatin1String("clientId")).toString();
    grant.refreshToken = object.value(QLatin1String("refreshToken")).toString();
    return grant;
}

namespace Sasl {

QByteArray xoauth2(const QString &user, const QString &accessToken)
{
    QByteArray message = "user=" + user.toUtf8() + '\x01';
    message += "auth=Bearer " + accessToken.toUtf8() + "\x01\x01";
    return message.toBase64();
}

QByteArray oauthBearer(const QString &user, const QString &accessToken, const QString &host, quint16 port)
{
    // The authorization identity is a saslname, RFC 5801 section 4
    QByteArray saslName = user.toUtf8();
    saslName.replace('=', "=3D");
    saslName.replace(',', "=2C");
    QByteArray message = "n,a=" + saslName + ',' + '\x01';
    if (!host.isEmpty())
        message += "host=" + host.toUtf8() + '\x01';
    if (port)
        message += "port=" + QByteArray::number(port) + '\x01';
    message += "auth=Bearer " + accessToken.toUtf8() + "\x01\x01";
    return message.toBase64();
}

QByteArray initialResponse(Mechanism mechanism, const QString &user, const QString &accessToken, const QString &host,
                           quint16 port)
{
    switch (mechanism) {
    case Mechanism::XOAuth2:
        return xoauth2(user, accessToken);
    case Mechanism::OAuthBearer:
        return oauthBearer(user, accessToken, host, port);
    case Mechanism::None:
        break;
    }
    return QByteArray();
}

QByteArray cancelResponse(Mechanism mechanism)
{
    // XOAUTH2 expects an empty response; OAUTHBEARER a single %x01, RFC 7628 section 3.2.3
    return mechanism == Mechanism::OAuthBearer ? QByteArray(1, '\x01').toBase64() : QByteArray();
}

Mechanism choose(const QList<Mechanism> &preferred, const QStringList &advertised)
{
    for (const Mechanism mechanism : preferred) {
        if (advertised.contains(QString::fromLatin1(mechanismName(mechanism)), Qt::CaseInsensitive))
            return mechanism;
    }
    return Mechanism::None;
}

QString describeChallenge(const QByteArray &base64Challenge)
{
    const QByteArray decoded = QByteArray::fromBase64(base64Challenge.trimmed());
    const QJsonObject json = QJsonDocument::fromJson(decoded).object();
    if (json.isEmpty())
        return QString();
    QStringList parts;
    for (const char *key : {"status", "error", "scope"}) {
        const QJsonValue value = json.value(QLatin1String(key));
        const QString text = value.isString() ? value.toString()
                                              : value.isDouble() ? QString::number(value.toInteger()) : QString();
        if (!text.isEmpty())
            parts << QStringLiteral("%1=%2").arg(QLatin1String(key), text);
    }
    return parts.join(QLatin1String(", "));
}

}

}
