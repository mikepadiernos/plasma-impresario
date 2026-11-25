/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef OAUTH_OAUTH_H
#define OAUTH_OAUTH_H

#include <QByteArray>
#include <QCoreApplication>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QUrl>

class QSettings;

/** @short OAuth 2.0 sign-in for IMAP and SMTP accounts

The account authorizes the application once in the system browser (authorization code flow with PKCE and a
loopback redirect, RFC 8252). The refresh token is kept in the account's credential storage; access tokens live in
memory only and are presented to the servers through the XOAUTH2 or OAUTHBEARER (RFC 7628) SASL mechanisms.
*/
namespace OAuth {

/** @short Translation context of the OAuth messages */
class Strings
{
    Q_DECLARE_TR_FUNCTIONS(OAuth)
};

/** @short SASL mechanisms which carry a bearer token */
enum class Mechanism {
    None,
    XOAuth2,
    OAuthBearer,
};

QByteArray mechanismName(Mechanism mechanism);
Mechanism mechanismFromName(const QString &name);

/** @short Endpoints and capabilities of an OAuth provider */
struct Provider {
    QString id;
    QString displayName;
    QUrl authorizationEndpoint;
    QUrl tokenEndpoint;
    QStringList scopes;
    /** @short Additional parameters of the authorization request, e.g. to obtain a refresh token */
    QList<QPair<QString, QString>> extraAuthorizationParameters;
    /** @short Host name used in the loopback redirect URI, as registered with the provider */
    QString redirectHost;
    /** @short Supported SASL mechanisms, most preferred first */
    QList<Mechanism> mechanisms;
    /** @short Client ID configured at build time; may be empty */
    QString defaultClientId;
    /** @short Token revocation endpoint, RFC 7009; empty when the provider offers none */
    QUrl revocationEndpoint;
    /** @short Page where the user can remove the application's access to the account */
    QUrl accessManagementPage;

    bool isValid() const { return !id.isEmpty(); }
};

/** @short All known providers */
const QList<Provider> &providers();

/** @short Look up a provider by its ID; returns an invalid provider for an unknown ID */
Provider provider(const QString &id);

/** @short Failure reasons, each with its own recovery message */
enum class Error {
    NoError,
    NotConfigured,
    NotSignedIn,
    Expired,
    Revoked,
    ConsentDenied,
    InvalidClient,
    Cancelled,
    TimedOut,
    BrowserUnavailable,
    StorageUnavailable,
    Network,
    Provider,
    MechanismUnavailable,
    Rejected,
};

/** @short A localized message which tells the user how to recover from the error */
QString errorMessage(Error error, const QString &detail = QString());

/** @short Map an RFC 6749 error code and its description to an error */
Error classifyProviderError(const QString &code, const QString &description);

enum class Service {
    Imap,
    Smtp,
};

/** @short Whether the account settings select OAuth for the service */
bool isEnabled(const QSettings &settings, Service service);

/** @short The OAuth configuration of one account */
struct AccountConfig {
    /** @short Key of the account's credentials in the password plugin */
    QString accountId;
    Provider provider;
    QString clientId;
    /** @short Optional; only for providers which issue a secret to installed applications. Never built in.

    Normally empty here: the secret is stored with the grant. A value in the settings file predates that and is moved
    into the grant by Session::migrateClientSecret().
    */
    QString clientSecret;
    QString loginHint;

    bool isValid() const { return !accountId.isEmpty() && provider.isValid() && !clientId.isEmpty(); }

    static AccountConfig fromSettings(const QSettings &settings, const QString &accountId);
};

/** @short Proof Key for Code Exchange, RFC 7636 */
namespace Pkce {
QByteArray generateVerifier();
QByteArray challenge(const QByteArray &verifier);
}

QByteArray generateState();

/** @short Encode the pairs as application/x-www-form-urlencoded */
QByteArray formEncode(const QList<QPair<QString, QString>> &items);

QUrl authorizationUrl(const AccountConfig &config, const QString &redirectUri, const QByteArray &state,
                      const QByteArray &codeChallenge);
QByteArray authorizationCodeRequestBody(const AccountConfig &config, const QString &code, const QString &redirectUri,
                                        const QByteArray &codeVerifier);
QByteArray refreshRequestBody(const AccountConfig &config, const QString &refreshToken);
/** @short Request body which revokes a refresh token, RFC 7009 section 2.1 */
QByteArray revocationRequestBody(const AccountConfig &config, const QString &refreshToken);

struct TokenResponse {
    Error error = Error::NoError;
    QString detail;
    QString accessToken;
    QString refreshToken;
    qint64 expiresIn = 0;
};

/** @short Parse the token endpoint's reply, RFC 6749 sections 5.1 and 5.2 */
TokenResponse parseTokenResponse(const QByteArray &body, int httpStatus);

/** @short Credential type of the stored grant in the password plugin */
QString credentialType();

/** @short The long-lived part of an authorization, as stored per account

The optional client secret is kept with the refresh token, so that it is never written to the settings file.
*/
struct Grant {
    QString provider;
    QString clientId;
    QString refreshToken;
    QString clientSecret;

    bool isValid() const { return !provider.isEmpty() && !refreshToken.isEmpty(); }
    /** @short Whether the grant was issued for the account's current provider and client */
    bool matches(const AccountConfig &config) const;
    QString toJson() const;
    static Grant fromJson(const QString &json);
};

/** @short Builders for the SASL exchange */
namespace Sasl {
/** @short Base64 initial response of XOAUTH2 */
QByteArray xoauth2(const QString &user, const QString &accessToken);
/** @short Base64 initial response of OAUTHBEARER, RFC 7628 section 3.1 */
QByteArray oauthBearer(const QString &user, const QString &accessToken, const QString &host, quint16 port);
QByteArray initialResponse(Mechanism mechanism, const QString &user, const QString &accessToken, const QString &host,
                           quint16 port);
/** @short Response which ends the exchange after the server reported an error in a challenge */
QByteArray cancelResponse(Mechanism mechanism);
/** @short Pick the most preferred mechanism among the advertised SASL mechanism names */
Mechanism choose(const QList<Mechanism> &preferred, const QStringList &advertised);
/** @short Short, non-sensitive description of a base64 JSON error challenge */
QString describeChallenge(const QByteArray &base64Challenge);
}

}

#endif
