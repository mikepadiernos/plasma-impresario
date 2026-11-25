/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrlQuery>
#include <QtTest>
#include <memory>
#include "test_OAuth.h"
#include "Common/SettingsNames.h"
#include "MSA/SMTP.h"
#include "OAuth/OAuth.h"
#include "OAuth/Session.h"
#include "OAuth/TokenSource.h"
#include "Plugins/PasswordPlugin.h"

namespace {

const QString user = QStringLiteral("user@example.com");

class FakePasswordPlugin;

/** @short Completes asynchronously, like the real credential backends */
class FakePasswordJob : public Plugins::PasswordJob
{
public:
    enum class Operation { Read, Store, Delete };
    FakePasswordJob(FakePasswordPlugin *plugin, Operation operation, const QString &key, const QString &value)
        : PasswordJob(nullptr)
        , m_plugin(plugin)
        , m_operation(operation)
        , m_key(key)
        , m_value(value)
    {
    }

protected:
    void doStart() override;
    void doStop() override { emit error(Stopped, QString()); }

private:
    FakePasswordPlugin *m_plugin;
    Operation m_operation;
    QString m_key;
    QString m_value;
};

class FakePasswordPlugin : public Plugins::PasswordPlugin
{
public:
    explicit FakePasswordPlugin(bool encrypted = true)
        : PasswordPlugin(nullptr)
        , m_encrypted(encrypted)
    {
    }

    Features features() const override { return m_encrypted ? FeatureEncryptedStorage : Features(); }

    Plugins::PasswordJob *requestPassword(const QString &accountId, const QString &accountType) override
    {
        return job(FakePasswordJob::Operation::Read, accountId, accountType, QString());
    }
    Plugins::PasswordJob *storePassword(const QString &accountId, const QString &accountType,
                                        const QString &password) override
    {
        return job(FakePasswordJob::Operation::Store, accountId, accountType, password);
    }
    Plugins::PasswordJob *deletePassword(const QString &accountId, const QString &accountType) override
    {
        return job(FakePasswordJob::Operation::Delete, accountId, accountType, QString());
    }

    static QString key(const QString &accountId) { return accountId + QLatin1String("/oauth"); }

    QMap<QString, QString> entries;
    QStringList credentialTypes;
    bool failReads = false;
    bool failWrites = false;

private:
    Plugins::PasswordJob *job(FakePasswordJob::Operation operation, const QString &accountId, const QString &type,
                              const QString &value)
    {
        credentialTypes << type;
        return new FakePasswordJob(this, operation, accountId + QLatin1Char('/') + type, value);
    }

    bool m_encrypted;
};

void FakePasswordJob::doStart()
{
    switch (m_operation) {
    case Operation::Read:
        if (m_plugin->failReads)
            emit error(UnknownError, QStringLiteral("The wallet is locked"));
        else if (!m_plugin->entries.contains(m_key))
            emit error(NoSuchPassword, QString());
        else
            emit passwordAvailable(m_plugin->entries.value(m_key));
        break;
    case Operation::Store:
        if (m_plugin->failWrites) {
            emit error(UnknownError, QStringLiteral("The wallet is read-only"));
        } else {
            m_plugin->entries.insert(m_key, m_value);
            emit passwordStored();
        }
        break;
    case Operation::Delete:
        if (m_plugin->failWrites) {
            emit error(UnknownError, QStringLiteral("The wallet is read-only"));
        } else if (m_plugin->entries.remove(m_key) == 0) {
            emit error(NoSuchPassword, QString());
        } else {
            emit passwordDeleted();
        }
        break;
    }
}

/** @short The provider's token endpoint, answering each POST with the next queued reply */
class FakeTokenEndpoint : public QObject
{
public:
    struct Reply {
        int status;
        QByteArray body;
    };

    FakeTokenEndpoint()
    {
        connect(&m_server, &QTcpServer::newConnection, this, &FakeTokenEndpoint::accept);
        QVERIFY(m_server.listen(QHostAddress::LocalHost, 0));
    }

    QUrl url() const { return QUrl(QStringLiteral("http://127.0.0.1:%1/token").arg(m_server.serverPort())); }

    void reply(int status, const QByteArray &body) { replies.append({status, body}); }

    QList<Reply> replies;
    QList<QUrlQuery> requests;
    QList<QByteArray> paths;

private:
    void accept()
    {
        while (QTcpSocket *socket = m_server.nextPendingConnection()) {
            socket->setParent(this);
            auto buffer = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer] {
                *buffer += socket->readAll();
                const int headerEnd = buffer->indexOf("\r\n\r\n");
                if (headerEnd < 0)
                    return;
                qsizetype length = 0;
                const auto lines = buffer->left(headerEnd).split('\n');
                for (const QByteArray &line : lines) {
                    if (line.toLower().startsWith("content-length:"))
                        length = line.mid(15).trimmed().toLongLong();
                }
                if (buffer->size() < headerEnd + 4 + length)
                    return;
                requests << QUrlQuery(QString::fromLatin1(buffer->mid(headerEnd + 4, length)));
                paths << lines.value(0).split(' ').value(1);
                const Reply reply = replies.isEmpty() ? Reply{500, QByteArray()} : replies.takeFirst();
                socket->write("HTTP/1.1 " + QByteArray::number(reply.status) + " Fake\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: " + QByteArray::number(reply.body.size()) + "\r\n"
                              "Connection: close\r\n\r\n" + reply.body);
                socket->disconnectFromHost();
                buffer->clear();
            });
        }
    }

    QTcpServer m_server;
};

/** @short The system browser: the provider redirects it to the loopback URI of the application */
class FakeBrowser : public QObject
{
public:
    enum class Action {
        ReturnCode,
        DenyConsent,
        ForeignState,
        Nothing,
    };

    OAuth::Session::BrowserLauncher launcher()
    {
        return [this](const QUrl &url) {
            opened = url;
            QTimer::singleShot(0, this, [this] { act(); });
            return true;
        };
    }

    QUrlQuery openedQuery() const { return QUrlQuery(opened); }

    Action action = Action::ReturnCode;
    QUrl opened;
    QList<QByteArray> pages;

private:
    void act()
    {
        const QString state = openedQuery().queryItemValue(QStringLiteral("state"), QUrl::FullyDecoded);
        switch (action) {
        case Action::ReturnCode:
            visit(QStringLiteral("code=auth-code-1&state=%1&scope=granted").arg(state));
            break;
        case Action::DenyConsent:
            visit(QStringLiteral("error=access_denied&error_description=The%20user%20denied%20access&state=%1")
                      .arg(state));
            break;
        case Action::ForeignState:
            visit(QStringLiteral("code=stolen&state=not-%1").arg(state));
            break;
        case Action::Nothing:
            break;
        }
    }

    void visit(const QString &query)
    {
        const QUrl redirect(openedQuery().queryItemValue(QStringLiteral("redirect_uri"), QUrl::FullyDecoded));
        auto *socket = new QTcpSocket(this);
        auto page = std::make_shared<QByteArray>();
        connect(socket, &QTcpSocket::connected, socket, [socket, query] {
            socket->write("GET /?" + query.toLatin1() + " HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
        });
        connect(socket, &QTcpSocket::readyRead, socket, [socket, page] { *page += socket->readAll(); });
        connect(socket, &QTcpSocket::disconnected, this, [this, page] { pages << *page; });
        socket->connectToHost(QHostAddress(redirect.host()), redirect.port());
    }
};

OAuth::AccountConfig testConfig(const QString &accountId, const QUrl &tokenEndpoint,
                                const QString &providerId = QStringLiteral("google"))
{
    OAuth::AccountConfig config;
    config.accountId = accountId;
    config.provider = OAuth::provider(providerId);
    config.provider.authorizationEndpoint = QUrl(QStringLiteral("https://accounts.example.test/authorize"));
    config.provider.tokenEndpoint = tokenEndpoint;
    if (config.provider.revocationEndpoint.isValid()) {
        // Never reach the real provider; an invalid endpoint makes revocation fail instead
        QUrl revocation = tokenEndpoint;
        revocation.setPath(QStringLiteral("/revoke"));
        config.provider.revocationEndpoint = revocation;
    }
    config.provider.redirectHost = QStringLiteral("127.0.0.1");
    config.clientId = QStringLiteral("test-client");
    config.loginHint = user;
    return config;
}

QString storedGrant(const QString &refreshToken, const QString &clientId = QStringLiteral("test-client"))
{
    OAuth::Grant grant;
    grant.provider = QStringLiteral("google");
    grant.clientId = clientId;
    grant.refreshToken = refreshToken;
    return grant.toJson();
}

QByteArray tokenReply(const QString &accessToken, const QVariant &expiresIn, const QString &refreshToken = QString())
{
    QJsonObject json{{QStringLiteral("access_token"), accessToken},
                     {QStringLiteral("token_type"), QStringLiteral("Bearer")},
                     {QStringLiteral("expires_in"), QJsonValue::fromVariant(expiresIn)}};
    if (!refreshToken.isEmpty())
        json.insert(QStringLiteral("refresh_token"), refreshToken);
    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

/** @short A session wired to the fakes */
struct Fixture {
    explicit Fixture(bool encrypted = true)
        : plugin(encrypted)
    {
        network.setProxy(QNetworkProxy::NoProxy);
    }

    std::unique_ptr<OAuth::Session> session(const QString &accountId,
                                            const QString &providerId = QStringLiteral("google"),
                                            const QString &clientSecret = QString())
    {
        FakePasswordPlugin *store = &plugin;
        OAuth::AccountConfig config = testConfig(accountId, endpoint.url(), providerId);
        config.clientSecret = clientSecret;
        auto session = std::make_unique<OAuth::Session>(config,
                                                        [store]() -> Plugins::PasswordPlugin * { return store; });
        session->setNetworkAccessManager(&network);
        session->setBrowserLauncher(browser.launcher());
        return session;
    }

    FakePasswordPlugin plugin;
    FakeTokenEndpoint endpoint;
    FakeBrowser browser;
    QNetworkAccessManager network;
};

bool authorize(Fixture &fixture, OAuth::Session &session)
{
    fixture.endpoint.reply(200, tokenReply(QStringLiteral("access-1"), 3600, QStringLiteral("refresh-1")));
    QSignalSpy authorized(&session, &OAuth::Session::authorized);
    QSignalSpy failed(&session, &OAuth::Session::authorizationFailed);
    session.authorize();
    return authorized.wait(5000) && failed.isEmpty();
}

/** @short A token source handing out queued tokens */
class FakeTokenSource : public OAuth::TokenSource
{
public:
    QList<OAuth::Mechanism> mechanisms() const override { return supported; }
    void requestAccessToken() override
    {
        const QString token = tokens.value(requests, QStringLiteral("token-last"));
        ++requests;
        QTimer::singleShot(0, this, [this, token] {
            if (failure.isEmpty())
                emit accessTokenAvailable(token);
            else
                emit failed(failure);
        });
    }
    void rejectAccessToken(const QString &accessToken) override { rejected << accessToken; }

    QList<OAuth::Mechanism> supported{OAuth::Mechanism::XOAuth2, OAuth::Mechanism::OAuthBearer};
    QStringList tokens{QStringLiteral("token-1"), QStringLiteral("token-2")};
    QString failure;
    int requests = 0;
    QStringList rejected;
};

/** @short A submission server which refuses the first OAuth attempts with an RFC 7628 error challenge */
class FakeSmtpServer : public QObject
{
public:
    FakeSmtpServer()
    {
        connect(&m_server, &QTcpServer::newConnection, this, &FakeSmtpServer::accept);
        QVERIFY(m_server.listen(QHostAddress::LocalHost, 0));
    }

    quint16 port() const { return m_server.serverPort(); }

    QByteArray authCapability = "AUTH PLAIN LOGIN XOAUTH2 OAUTHBEARER";
    int rejectedAttempts = 1;
    int connections = 0;
    QList<QByteArray> authCommands;
    QList<QByteArray> cancelResponses;
    QList<QByteArray> messages;

    static QByteArray errorChallenge()
    {
        return QByteArray(R"({"status":"401","schemes":"bearer","scope":"https://mail.google.com/"})").toBase64();
    }

private:
    void accept()
    {
        while (QTcpSocket *socket = m_server.nextPendingConnection()) {
            ++connections;
            socket->setParent(this);
            auto inData = std::make_shared<bool>(false);
            auto awaitingCancel = std::make_shared<bool>(false);
            auto message = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, this, [=, this] {
                while (socket->canReadLine()) {
                    const QByteArray line = socket->readLine();
                    if (*inData) {
                        if (line == ".\r\n") {
                            *inData = false;
                            messages << *message;
                            socket->write("250 2.0.0 Queued\r\n");
                        } else {
                            *message += line;
                        }
                        continue;
                    }
                    if (*awaitingCancel) {
                        *awaitingCancel = false;
                        cancelResponses << line.trimmed();
                        socket->write("535 5.7.8 Username and Password not accepted\r\n");
                        continue;
                    }
                    const QByteArray command = line.left(4).toUpper();
                    if (command == "EHLO") {
                        socket->write("250-fake.example.test\r\n");
                        if (!authCapability.isEmpty())
                            socket->write("250-" + authCapability + "\r\n");
                        socket->write("250 8BITMIME\r\n");
                    } else if (command == "AUTH") {
                        authCommands << line.trimmed();
                        if (authCommands.size() <= rejectedAttempts) {
                            *awaitingCancel = true;
                            socket->write("334 " + errorChallenge() + "\r\n");
                        } else {
                            socket->write("235 2.7.0 Accepted\r\n");
                        }
                    } else if (command == "MAIL" || command == "RCPT") {
                        socket->write("250 2.1.0 OK\r\n");
                    } else if (command == "DATA") {
                        *inData = true;
                        socket->write("354 Go ahead\r\n");
                    } else if (command == "QUIT") {
                        socket->write("221 2.0.0 Bye\r\n");
                        socket->disconnectFromHost();
                    } else {
                        socket->write("502 5.5.1 Unrecognized command\r\n");
                    }
                }
            });
            socket->write("220 fake.example.test ESMTP\r\n");
        }
    }

    QTcpServer m_server;
};

QByteArray authCommand(const QByteArray &mechanism, const QByteArray &initialResponse)
{
    return "AUTH " + mechanism + ' ' + initialResponse;
}

}

Q_DECLARE_METATYPE(OAuth::Error)

void OAuthTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void OAuthTest::saslInitialResponses()
{
    // Google's XOAUTH2 example and the OAUTHBEARER example of RFC 7628 section 4.1
    QCOMPARE(OAuth::Sasl::xoauth2(QStringLiteral("someuser@example.com"),
                                  QStringLiteral("ya29.vF9dft4qmTc2Nvb3RlckBhdHRhdmlzdGEuY29tCg")),
             QByteArrayLiteral("dXNlcj1zb21ldXNlckBleGFtcGxlLmNvbQFhdXRoPUJlYXJlciB5YTI5LnZGOWRmdDRxbVRjMk52YjNSbGNrQmhkSFJoZG1semRHRXVZMjl0Q2cBAQ=="));
    QCOMPARE(OAuth::Sasl::oauthBearer(QStringLiteral("user@example.com"),
                                      QStringLiteral("vF9dft4qmTc2Nvb3RlckBhdHRhdmlzdGEuY29tCg=="),
                                      QStringLiteral("server.example.com"), 143),
             QByteArrayLiteral("bixhPXVzZXJAZXhhbXBsZS5jb20sAWhvc3Q9c2VydmVyLmV4YW1wbGUuY29tAXBvcnQ9MTQzAWF1dGg9QmVhcmVyIHZGOWRmdDRxbVRjMk52YjNSbGNrQmhkSFJoZG1semRHRXVZMjl0Q2c9PQEB"));
    QCOMPARE(QByteArray::fromBase64(OAuth::Sasl::xoauth2(user, QStringLiteral("t"))),
             QByteArray("user=user@example.com\x01" "auth=Bearer t\x01\x01"));
    // RFC 7628 escapes "," and "=" in the authorization identity
    QCOMPARE(QByteArray::fromBase64(OAuth::Sasl::oauthBearer(QStringLiteral("a,b=c"), QStringLiteral("t"),
                                                             QStringLiteral("h"), 993)),
             QByteArray("n,a=a=2Cb=3Dc,\x01" "host=h\x01" "port=993\x01" "auth=Bearer t\x01\x01"));
    QCOMPARE(OAuth::Sasl::initialResponse(OAuth::Mechanism::XOAuth2, user, QStringLiteral("t"), QString(), 0),
             OAuth::Sasl::xoauth2(user, QStringLiteral("t")));
    QCOMPARE(OAuth::Sasl::initialResponse(OAuth::Mechanism::OAuthBearer, user, QStringLiteral("t"),
                                          QStringLiteral("h"), 993),
             OAuth::Sasl::oauthBearer(user, QStringLiteral("t"), QStringLiteral("h"), 993));
    QVERIFY(OAuth::Sasl::cancelResponse(OAuth::Mechanism::XOAuth2).isEmpty());
    QCOMPARE(OAuth::Sasl::cancelResponse(OAuth::Mechanism::OAuthBearer), QByteArray("\x01").toBase64());
}

void OAuthTest::saslChallengeAndMechanismChoice()
{
    QCOMPARE(OAuth::Sasl::describeChallenge(FakeSmtpServer::errorChallenge()),
             QStringLiteral("status=401, scope=https://mail.google.com/"));
    QVERIFY(OAuth::Sasl::describeChallenge("bm90IGpzb24=").isEmpty());

    const QList<OAuth::Mechanism> google = OAuth::provider(QStringLiteral("google")).mechanisms;
    QCOMPARE(OAuth::Sasl::choose(google, {QStringLiteral("PLAIN"), QStringLiteral("OAUTHBEARER"),
                                          QStringLiteral("XOAUTH2")}),
             OAuth::Mechanism::XOAuth2);
    QCOMPARE(OAuth::Sasl::choose(google, {QStringLiteral("oauthbearer")}), OAuth::Mechanism::OAuthBearer);
    QCOMPARE(OAuth::Sasl::choose(OAuth::provider(QStringLiteral("microsoft")).mechanisms,
                                 {QStringLiteral("OAUTHBEARER")}),
             OAuth::Mechanism::None);
    QCOMPARE(OAuth::Sasl::choose(google, {QStringLiteral("PLAIN"), QStringLiteral("LOGIN")}),
             OAuth::Mechanism::None);
    QCOMPARE(OAuth::mechanismName(OAuth::Mechanism::OAuthBearer), QByteArray("OAUTHBEARER"));
    QCOMPARE(OAuth::mechanismFromName(QStringLiteral("xoauth2")), OAuth::Mechanism::XOAuth2);
    QCOMPARE(OAuth::mechanismFromName(QStringLiteral("PLAIN")), OAuth::Mechanism::None);
}

void OAuthTest::pkce()
{
    // RFC 7636 appendix B
    QCOMPARE(OAuth::Pkce::challenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"),
             QByteArray("E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM"));
    const QByteArray verifier = OAuth::Pkce::generateVerifier();
    QCOMPARE(verifier.size(), 43);
    static const QRegularExpression unreserved(QStringLiteral("^[A-Za-z0-9._~-]+$"));
    QVERIFY(unreserved.match(QString::fromLatin1(verifier)).hasMatch());
    QVERIFY(OAuth::Pkce::generateVerifier() != verifier);
    QVERIFY(OAuth::generateState() != OAuth::generateState());
}

void OAuthTest::providerTable()
{
    const OAuth::Provider google = OAuth::provider(QStringLiteral("google"));
    QVERIFY(google.isValid());
    QCOMPARE(google.tokenEndpoint, QUrl(QStringLiteral("https://oauth2.googleapis.com/token")));
    QCOMPARE(google.scopes, QStringList{QStringLiteral("https://mail.google.com/")});
    QCOMPARE(google.mechanisms, (QList<OAuth::Mechanism>{OAuth::Mechanism::XOAuth2, OAuth::Mechanism::OAuthBearer}));
    QCOMPARE(google.redirectHost, QStringLiteral("127.0.0.1"));

    const OAuth::Provider microsoft = OAuth::provider(QStringLiteral("microsoft"));
    QVERIFY(microsoft.isValid());
    QCOMPARE(microsoft.tokenEndpoint,
             QUrl(QStringLiteral("https://login.microsoftonline.com/common/oauth2/v2.0/token")));
    QVERIFY(microsoft.scopes.contains(QStringLiteral("https://outlook.office.com/IMAP.AccessAsUser.All")));
    QVERIFY(microsoft.scopes.contains(QStringLiteral("https://outlook.office.com/SMTP.Send")));
    QVERIFY(microsoft.scopes.contains(QStringLiteral("offline_access")));
    QCOMPARE(microsoft.mechanisms, QList<OAuth::Mechanism>{OAuth::Mechanism::XOAuth2});
    QCOMPARE(microsoft.redirectHost, QStringLiteral("localhost"));

    // Google revokes refresh tokens, RFC 7009; Microsoft only lets the user remove the consent on a web page
    QCOMPARE(google.revocationEndpoint, QUrl(QStringLiteral("https://oauth2.googleapis.com/revoke")));
    QVERIFY(microsoft.revocationEndpoint.isEmpty());

    for (const OAuth::Provider &provider : OAuth::providers()) {
        QCOMPARE(provider.authorizationEndpoint.scheme(), QStringLiteral("https"));
        QCOMPARE(provider.tokenEndpoint.scheme(), QStringLiteral("https"));
        QCOMPARE(provider.accessManagementPage.scheme(), QStringLiteral("https"));
        QVERIFY(!provider.displayName.isEmpty());
        QVERIFY(!provider.mechanisms.isEmpty());
    }
    QVERIFY(!OAuth::provider(QStringLiteral("unknown")).isValid());
}

void OAuthTest::authorizationRequest()
{
    OAuth::AccountConfig config = testConfig(QStringLiteral("account-1"), QUrl(QStringLiteral("https://t.example.test")));
    const QString redirect = QStringLiteral("http://127.0.0.1:4711");
    const QUrlQuery query(OAuth::authorizationUrl(config, redirect, "state-1", "challenge-1"));
    QCOMPARE(query.queryItemValue(QStringLiteral("response_type")), QStringLiteral("code"));
    QCOMPARE(query.queryItemValue(QStringLiteral("client_id")), QStringLiteral("test-client"));
    QCOMPARE(query.queryItemValue(QStringLiteral("redirect_uri"), QUrl::FullyDecoded), redirect);
    QCOMPARE(query.queryItemValue(QStringLiteral("scope"), QUrl::FullyDecoded), QStringLiteral("https://mail.google.com/"));
    QCOMPARE(query.queryItemValue(QStringLiteral("state")), QStringLiteral("state-1"));
    QCOMPARE(query.queryItemValue(QStringLiteral("code_challenge")), QStringLiteral("challenge-1"));
    QCOMPARE(query.queryItemValue(QStringLiteral("code_challenge_method")), QStringLiteral("S256"));
    QCOMPARE(query.queryItemValue(QStringLiteral("login_hint"), QUrl::FullyDecoded), user);
    QCOMPARE(query.queryItemValue(QStringLiteral("access_type")), QStringLiteral("offline"));
    QVERIFY(!query.hasQueryItem(QStringLiteral("client_secret")));

    config.provider = OAuth::provider(QStringLiteral("microsoft"));
    const QUrlQuery microsoft(OAuth::authorizationUrl(config, redirect, "s", "c"));
    QCOMPARE(microsoft.queryItemValue(QStringLiteral("scope"), QUrl::FullyDecoded),
             QStringLiteral("https://outlook.office.com/IMAP.AccessAsUser.All https://outlook.office.com/SMTP.Send "
                            "offline_access"));
    QVERIFY(!microsoft.hasQueryItem(QStringLiteral("access_type")));

    QUrlQuery exchange(QString::fromLatin1(OAuth::authorizationCodeRequestBody(config, QStringLiteral("a code"),
                                                                               redirect, "verifier-1")));
    QCOMPARE(exchange.queryItemValue(QStringLiteral("grant_type")), QStringLiteral("authorization_code"));
    QCOMPARE(exchange.queryItemValue(QStringLiteral("code"), QUrl::FullyDecoded), QStringLiteral("a code"));
    QCOMPARE(exchange.queryItemValue(QStringLiteral("redirect_uri"), QUrl::FullyDecoded), redirect);
    QCOMPARE(exchange.queryItemValue(QStringLiteral("code_verifier")), QStringLiteral("verifier-1"));
    QCOMPARE(exchange.queryItemValue(QStringLiteral("client_id")), QStringLiteral("test-client"));
    QVERIFY(!exchange.hasQueryItem(QStringLiteral("client_secret")));

    config.clientSecret = QStringLiteral("not-so-secret");
    QUrlQuery refresh(QString::fromLatin1(OAuth::refreshRequestBody(config, QStringLiteral("refresh/1"))));
    QCOMPARE(refresh.queryItemValue(QStringLiteral("grant_type")), QStringLiteral("refresh_token"));
    QCOMPARE(refresh.queryItemValue(QStringLiteral("refresh_token"), QUrl::FullyDecoded), QStringLiteral("refresh/1"));
    QCOMPARE(refresh.queryItemValue(QStringLiteral("client_secret")), QStringLiteral("not-so-secret"));
}

void OAuthTest::tokenResponses_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<OAuth::Error>("error");
    QTest::addColumn<QString>("accessToken");
    QTest::addColumn<qint64>("expiresIn");

    QTest::newRow("success") << 200 << tokenReply(QStringLiteral("a"), 3599, QStringLiteral("r"))
                             << OAuth::Error::NoError << QStringLiteral("a") << qint64(3599);
    QTest::newRow("lifetime as a string") << 200 << tokenReply(QStringLiteral("a"), QStringLiteral("60"))
                                          << OAuth::Error::NoError << QStringLiteral("a") << qint64(60);
    QTest::newRow("no access token") << 200 << QByteArray("{\"token_type\":\"Bearer\"}") << OAuth::Error::Provider
                                     << QString() << qint64(0);
    QTest::newRow("google expired") << 400
                                    << QByteArray(R"({"error":"invalid_grant","error_description":"Token has been expired or revoked."})")
                                    << OAuth::Error::Expired << QString() << qint64(0);
    QTest::newRow("microsoft expired") << 400
                                       << QByteArray(R"({"error":"invalid_grant","error_description":"AADSTS700082: The refresh token has expired due to inactivity."})")
                                       << OAuth::Error::Expired << QString() << qint64(0);
    QTest::newRow("revoked") << 400 << QByteArray(R"({"error":"invalid_grant","error_description":"Bad Request"})")
                             << OAuth::Error::Revoked << QString() << qint64(0);
    QTest::newRow("consent denied") << 400 << QByteArray(R"({"error":"access_denied"})")
                                    << OAuth::Error::ConsentDenied << QString() << qint64(0);
    QTest::newRow("invalid client") << 401 << QByteArray(R"({"error":"invalid_client"})")
                                    << OAuth::Error::InvalidClient << QString() << qint64(0);
    QTest::newRow("server error") << 503 << QByteArray("Service Unavailable") << OAuth::Error::Provider
                                  << QString() << qint64(0);
}

void OAuthTest::tokenResponses()
{
    QFETCH(int, status);
    QFETCH(QByteArray, body);
    QFETCH(OAuth::Error, error);
    QFETCH(QString, accessToken);
    QFETCH(qint64, expiresIn);

    const OAuth::TokenResponse response = OAuth::parseTokenResponse(body, status);
    QCOMPARE(response.error, error);
    QCOMPARE(response.accessToken, accessToken);
    QCOMPARE(response.expiresIn, expiresIn);
    if (error != OAuth::Error::NoError) {
        QVERIFY(!OAuth::errorMessage(error).isEmpty());
        QVERIFY(OAuth::errorMessage(error, response.detail).startsWith(OAuth::errorMessage(error)));
    }
}

void OAuthTest::accountSettings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("account.conf")), QSettings::IniFormat);
    QVERIFY(!OAuth::isEnabled(settings, OAuth::Service::Imap));
    QVERIFY(!OAuth::isEnabled(settings, OAuth::Service::Smtp));

    settings.setValue(Common::SettingsNames::imapAuthMethodKey, Common::SettingsNames::authMethodOAuth);
    settings.setValue(Common::SettingsNames::imapUserKey, user);
    settings.setValue(Common::SettingsNames::oauthProviderKey, QStringLiteral("microsoft"));
    QVERIFY(OAuth::isEnabled(settings, OAuth::Service::Imap));
    QVERIFY(!OAuth::isEnabled(settings, OAuth::Service::Smtp));

    OAuth::AccountConfig config = OAuth::AccountConfig::fromSettings(settings, QStringLiteral("account-1"));
    QCOMPARE(config.accountId, QStringLiteral("account-1"));
    QCOMPARE(config.provider.id, QStringLiteral("microsoft"));
    QCOMPARE(config.loginHint, user);
    // Without a client ID, the build's default applies; this build may have none
    QCOMPARE(config.clientId, config.provider.defaultClientId);
    QCOMPARE(config.isValid(), !config.provider.defaultClientId.isEmpty());

    settings.setValue(Common::SettingsNames::oauthClientIdKey, QStringLiteral(" account-client "));
    config = OAuth::AccountConfig::fromSettings(settings, QStringLiteral("account-1"));
    QCOMPARE(config.clientId, QStringLiteral("account-client"));
    QVERIFY(config.clientSecret.isEmpty());
    QVERIFY(config.isValid());

    settings.setValue(Common::SettingsNames::smtpAuthMethodKey, Common::SettingsNames::authMethodOAuth);
    QVERIFY(OAuth::isEnabled(settings, OAuth::Service::Smtp));
}

void OAuthTest::revocationRequest()
{
    OAuth::AccountConfig config = testConfig(QStringLiteral("account-1"), QUrl());
    QUrlQuery body(QString::fromLatin1(OAuth::revocationRequestBody(config, QStringLiteral("refresh-1"))));
    QCOMPARE(body.queryItemValue(QStringLiteral("token")), QStringLiteral("refresh-1"));
    QCOMPARE(body.queryItemValue(QStringLiteral("token_type_hint")), QStringLiteral("refresh_token"));
    QCOMPARE(body.queryItemValue(QStringLiteral("client_id")), QStringLiteral("test-client"));
    QVERIFY(!body.hasQueryItem(QStringLiteral("client_secret")));

    config.clientSecret = QStringLiteral("s3cret");
    body = QUrlQuery(QString::fromLatin1(OAuth::revocationRequestBody(config, QStringLiteral("refresh-1"))));
    QCOMPARE(body.queryItemValue(QStringLiteral("client_secret")), QStringLiteral("s3cret"));
}

void OAuthTest::grantRoundTrip()
{
    const OAuth::Grant grant = OAuth::Grant::fromJson(storedGrant(QStringLiteral("refresh-1")));
    QVERIFY(grant.isValid());
    QCOMPARE(grant.refreshToken, QStringLiteral("refresh-1"));
    const OAuth::AccountConfig config = testConfig(QStringLiteral("account-1"), QUrl());
    QVERIFY(grant.matches(config));
    OAuth::AccountConfig otherClient = config;
    otherClient.clientId = QStringLiteral("other-client");
    QVERIFY(!grant.matches(otherClient));
    OAuth::AccountConfig otherProvider = config;
    otherProvider.provider = OAuth::provider(QStringLiteral("microsoft"));
    QVERIFY(!grant.matches(otherProvider));
    QVERIFY(!OAuth::Grant::fromJson(QStringLiteral("a plain password")).isValid());

    // The client secret is only written when there is one
    QVERIFY(!grant.toJson().contains(QLatin1String("clientSecret")));
    OAuth::Grant withSecret = grant;
    withSecret.clientSecret = QStringLiteral("s3cret");
    QCOMPARE(OAuth::Grant::fromJson(withSecret.toJson()).clientSecret, QStringLiteral("s3cret"));
}

void OAuthTest::authorizationSucceeds()
{
    Fixture fixture;
    auto session = fixture.session(QStringLiteral("account-1"));
    QSignalSpy failed(session.get(), &OAuth::Session::authorizationFailed);
    QVERIFY(authorize(fixture, *session));
    QVERIFY(failed.isEmpty());
    QVERIFY(!session->isAuthorizing());
    QCOMPARE(session->lastError(), OAuth::Error::NoError);

    // The browser was sent to the provider with PKCE and a loopback redirect, but without any secret
    const QUrlQuery opened = fixture.browser.openedQuery();
    QCOMPARE(fixture.browser.opened.host(), QStringLiteral("accounts.example.test"));
    QCOMPARE(opened.queryItemValue(QStringLiteral("client_id")), QStringLiteral("test-client"));
    QCOMPARE(opened.queryItemValue(QStringLiteral("code_challenge_method")), QStringLiteral("S256"));
    QVERIFY(!opened.hasQueryItem(QStringLiteral("client_secret")));
    const QString redirect = opened.queryItemValue(QStringLiteral("redirect_uri"), QUrl::FullyDecoded);
    QVERIFY(redirect.startsWith(QLatin1String("http://127.0.0.1:")));

    // The code was exchanged with the verifier matching the challenge
    QCOMPARE(fixture.endpoint.requests.size(), 1);
    const QUrlQuery exchange = fixture.endpoint.requests.first();
    QCOMPARE(exchange.queryItemValue(QStringLiteral("grant_type")), QStringLiteral("authorization_code"));
    QCOMPARE(exchange.queryItemValue(QStringLiteral("code")), QStringLiteral("auth-code-1"));
    QCOMPARE(exchange.queryItemValue(QStringLiteral("redirect_uri"), QUrl::FullyDecoded), redirect);
    QCOMPARE(QString::fromLatin1(OAuth::Pkce::challenge(exchange.queryItemValue(QStringLiteral("code_verifier")).toLatin1())),
             opened.queryItemValue(QStringLiteral("code_challenge")));
    QVERIFY(!exchange.hasQueryItem(QStringLiteral("client_secret")));

    // Only the refresh token is stored, under the account's own OAuth key
    QCOMPARE(fixture.plugin.entries.keys(), QStringList{FakePasswordPlugin::key(QStringLiteral("account-1"))});
    const QString stored = fixture.plugin.entries.first();
    QCOMPARE(OAuth::Grant::fromJson(stored).refreshToken, QStringLiteral("refresh-1"));
    QVERIFY(!stored.contains(QLatin1String("access-1")));
    for (const QString &type : std::as_const(fixture.plugin.credentialTypes))
        QCOMPARE(type, OAuth::credentialType());

    QTRY_COMPARE(fixture.browser.pages.size(), 1);
    QVERIFY(fixture.browser.pages.first().startsWith("HTTP/1.1 200 "));

    // The access token from the exchange is used without another request
    QSignalSpy tokens(session.get(), &OAuth::TokenSource::accessTokenAvailable);
    session->requestAccessToken();
    QVERIFY(tokens.isEmpty());
    QVERIFY(tokens.wait(1000));
    QCOMPARE(tokens.first().first().toString(), QStringLiteral("access-1"));
    QCOMPARE(fixture.endpoint.requests.size(), 1);
}

void OAuthTest::consentDenied()
{
    Fixture fixture;
    fixture.browser.action = FakeBrowser::Action::DenyConsent;
    auto session = fixture.session(QStringLiteral("account-1"));
    QSignalSpy failed(session.get(), &OAuth::Session::authorizationFailed);
    QSignalSpy authorized(session.get(), &OAuth::Session::authorized);
    session->authorize();
    QVERIFY(failed.wait(5000));
    QCOMPARE(session->lastError(), OAuth::Error::ConsentDenied);
    QVERIFY(failed.first().first().toString().startsWith(OAuth::errorMessage(OAuth::Error::ConsentDenied)));
    QVERIFY(authorized.isEmpty());
    QVERIFY(fixture.endpoint.requests.isEmpty());
    QVERIFY(fixture.plugin.entries.isEmpty());
    QVERIFY(!session->isAuthorizing());
}

void OAuthTest::authorizationIsCancelled()
{
    Fixture fixture;
    fixture.browser.action = FakeBrowser::Action::ForeignState;
    auto session = fixture.session(QStringLiteral("account-1"));
    QSignalSpy failed(session.get(), &OAuth::Session::authorizationFailed);
    session->authorize();
    QVERIFY(session->isAuthorizing());

    // A redirect with a foreign state is refused, and the sign-in keeps waiting
    QTRY_COMPARE(fixture.browser.pages.size(), 1);
    QVERIFY(fixture.browser.pages.first().startsWith("HTTP/1.1 400 "));
    QVERIFY(session->isAuthorizing());
    QVERIFY(failed.isEmpty());

    session->cancel();
    QCOMPARE(failed.size(), 1);
    QCOMPARE(session->lastError(), OAuth::Error::Cancelled);
    QCOMPARE(failed.first().first().toString(), OAuth::errorMessage(OAuth::Error::Cancelled));
    QVERIFY(!session->isAuthorizing());
    QVERIFY(session->redirectUri().isEmpty());
    QVERIFY(fixture.endpoint.requests.isEmpty());
    QVERIFY(fixture.plugin.entries.isEmpty());

    // Microsoft's registered redirect URI uses the host name
    FakeTokenEndpoint endpoint;
    OAuth::AccountConfig config = testConfig(QStringLiteral("account-2"), endpoint.url(), QStringLiteral("microsoft"));
    config.provider.redirectHost = OAuth::provider(QStringLiteral("microsoft")).redirectHost;
    FakePasswordPlugin *store = &fixture.plugin;
    OAuth::Session microsoft(config, [store]() -> Plugins::PasswordPlugin * { return store; });
    microsoft.setBrowserLauncher([](const QUrl &) { return true; });
    microsoft.authorize();
    QVERIFY(microsoft.redirectUri().startsWith(QLatin1String("http://localhost:")));
    microsoft.cancel();
    QCOMPARE(microsoft.lastError(), OAuth::Error::Cancelled);
}

void OAuthTest::authorizationTimesOut()
{
    Fixture fixture;
    fixture.browser.action = FakeBrowser::Action::Nothing;
    auto session = fixture.session(QStringLiteral("account-1"));
    session->setAuthorizationTimeout(100);
    QSignalSpy failed(session.get(), &OAuth::Session::authorizationFailed);
    session->authorize();
    QVERIFY(failed.wait(5000));
    QCOMPARE(session->lastError(), OAuth::Error::TimedOut);
    QCOMPARE(failed.first().first().toString(), OAuth::errorMessage(OAuth::Error::TimedOut));
    QVERIFY(!session->isAuthorizing());
    QVERIFY(fixture.plugin.entries.isEmpty());
}

void OAuthTest::browserUnavailable()
{
    Fixture fixture;
    auto session = fixture.session(QStringLiteral("account-1"));
    session->setBrowserLauncher([](const QUrl &) { return false; });
    QSignalSpy failed(session.get(), &OAuth::Session::authorizationFailed);
    session->authorize();
    QCOMPARE(failed.size(), 1);
    QCOMPARE(session->lastError(), OAuth::Error::BrowserUnavailable);
    QVERIFY(!session->isAuthorizing());

    // Without a client ID the browser is not even opened
    OAuth::AccountConfig config = testConfig(QStringLiteral("account-1"), fixture.endpoint.url());
    config.clientId.clear();
    FakePasswordPlugin *store = &fixture.plugin;
    OAuth::Session unconfigured(config, [store]() -> Plugins::PasswordPlugin * { return store; });
    bool opened = false;
    unconfigured.setBrowserLauncher([&opened](const QUrl &) { return opened = true; });
    QSignalSpy notConfigured(&unconfigured, &OAuth::Session::authorizationFailed);
    unconfigured.authorize();
    QCOMPARE(notConfigured.size(), 1);
    QCOMPARE(unconfigured.lastError(), OAuth::Error::NotConfigured);
    QVERIFY(!opened);
}

void OAuthTest::refreshRenewsAndRotatesTokens()
{
    Fixture fixture;
    const QString key = FakePasswordPlugin::key(QStringLiteral("account-1"));
    fixture.plugin.entries.insert(key, storedGrant(QStringLiteral("refresh-old")));
    fixture.endpoint.reply(200, tokenReply(QStringLiteral("access-2"), 3600, QStringLiteral("refresh-new")));
    fixture.endpoint.reply(200, tokenReply(QStringLiteral("access-3"), QStringLiteral("3599")));
    fixture.endpoint.reply(200, tokenReply(QStringLiteral("access-short"), 30));
    fixture.endpoint.reply(200, tokenReply(QStringLiteral("access-4"), 3600));
    auto session = fixture.session(QStringLiteral("account-1"));
    QSignalSpy tokens(session.get(), &OAuth::TokenSource::accessTokenAvailable);
    QSignalSpy failed(session.get(), &OAuth::TokenSource::failed);

    // Concurrent requests share one renewal
    session->requestAccessToken();
    session->requestAccessToken();
    QVERIFY(tokens.wait(5000));
    QTest::qWait(50);
    QCOMPARE(tokens.size(), 1);
    QCOMPARE(tokens.takeFirst().first().toString(), QStringLiteral("access-2"));
    QCOMPARE(fixture.endpoint.requests.size(), 1);
    QCOMPARE(fixture.endpoint.requests.first().queryItemValue(QStringLiteral("grant_type")),
             QStringLiteral("refresh_token"));
    QCOMPARE(fixture.endpoint.requests.first().queryItemValue(QStringLiteral("refresh_token")),
             QStringLiteral("refresh-old"));
    // The rotated refresh token replaces the stored one
    QTRY_COMPARE(OAuth::Grant::fromJson(fixture.plugin.entries.value(key)).refreshToken, QStringLiteral("refresh-new"));

    // A valid token is reused
    session->requestAccessToken();
    QVERIFY(tokens.wait(1000));
    QCOMPARE(tokens.takeFirst().first().toString(), QStringLiteral("access-2"));
    QCOMPARE(fixture.endpoint.requests.size(), 1);

    // A token refused by the server is renewed with the rotated refresh token
    session->rejectAccessToken(QStringLiteral("access-2"));
    session->requestAccessToken();
    QVERIFY(tokens.wait(5000));
    QCOMPARE(tokens.takeFirst().first().toString(), QStringLiteral("access-3"));
    QCOMPARE(fixture.endpoint.requests.size(), 2);
    QCOMPARE(fixture.endpoint.requests.last().queryItemValue(QStringLiteral("refresh_token")),
             QStringLiteral("refresh-new"));
    QCOMPARE(OAuth::Grant::fromJson(fixture.plugin.entries.value(key)).refreshToken, QStringLiteral("refresh-new"));

    // Rejecting a token which is no longer cached keeps the current one
    session->rejectAccessToken(QStringLiteral("access-2"));
    session->requestAccessToken();
    QVERIFY(tokens.wait(1000));
    QCOMPARE(tokens.takeFirst().first().toString(), QStringLiteral("access-3"));
    QCOMPARE(fixture.endpoint.requests.size(), 2);

    // A token about to expire is renewed before use
    session->rejectAccessToken(QStringLiteral("access-3"));
    session->requestAccessToken();
    QVERIFY(tokens.wait(5000));
    QCOMPARE(tokens.takeFirst().first().toString(), QStringLiteral("access-short"));
    session->requestAccessToken();
    QVERIFY(tokens.wait(5000));
    QCOMPARE(tokens.takeFirst().first().toString(), QStringLiteral("access-4"));
    QCOMPARE(fixture.endpoint.requests.size(), 4);
    QVERIFY(failed.isEmpty());
}

void OAuthTest::refreshFailures_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<QString>("storedClient");
    QTest::addColumn<bool>("reachable");
    QTest::addColumn<OAuth::Error>("error");
    QTest::addColumn<int>("requests");

    const QString client = QStringLiteral("test-client");
    QTest::newRow("expired") << 400
                             << QByteArray(R"({"error":"invalid_grant","error_description":"Token has been expired or revoked."})")
                             << client << true << OAuth::Error::Expired << 1;
    QTest::newRow("revoked") << 400 << QByteArray(R"({"error":"invalid_grant"})") << client << true
                             << OAuth::Error::Revoked << 1;
    QTest::newRow("invalid client") << 401 << QByteArray(R"({"error":"invalid_client"})") << client << true
                                    << OAuth::Error::InvalidClient << 1;
    QTest::newRow("server error") << 500 << QByteArray() << client << true << OAuth::Error::Provider << 1;
    QTest::newRow("unreachable") << 0 << QByteArray() << client << false << OAuth::Error::Network << 0;
    QTest::newRow("not signed in") << 0 << QByteArray() << QString() << true << OAuth::Error::NotSignedIn << 0;
    QTest::newRow("other client") << 0 << QByteArray() << QStringLiteral("other-client") << true
                                  << OAuth::Error::NotSignedIn << 0;
}

void OAuthTest::refreshFailures()
{
    QFETCH(int, status);
    QFETCH(QByteArray, body);
    QFETCH(QString, storedClient);
    QFETCH(bool, reachable);
    QFETCH(OAuth::Error, error);
    QFETCH(int, requests);

    Fixture fixture;
    if (!storedClient.isEmpty()) {
        fixture.plugin.entries.insert(FakePasswordPlugin::key(QStringLiteral("account-1")),
                                      storedGrant(QStringLiteral("refresh-1"), storedClient));
    }
    if (status)
        fixture.endpoint.reply(status, body);
    OAuth::AccountConfig config = testConfig(QStringLiteral("account-1"), fixture.endpoint.url());
    if (!reachable) {
        QTcpServer closed;
        QVERIFY(closed.listen(QHostAddress::LocalHost, 0));
        config.provider.tokenEndpoint = QUrl(QStringLiteral("http://127.0.0.1:%1/token").arg(closed.serverPort()));
    }
    FakePasswordPlugin *store = &fixture.plugin;
    OAuth::Session session(config, [store]() -> Plugins::PasswordPlugin * { return store; });
    session.setNetworkAccessManager(&fixture.network);
    QSignalSpy tokens(&session, &OAuth::TokenSource::accessTokenAvailable);
    QSignalSpy failed(&session, &OAuth::TokenSource::failed);
    session.requestAccessToken();
    QVERIFY(failed.wait(10000));
    QVERIFY(tokens.isEmpty());
    QCOMPARE(session.lastError(), error);
    QVERIFY(failed.first().first().toString().startsWith(OAuth::errorMessage(error)));
    QCOMPARE(fixture.endpoint.requests.size(), requests);
}

void OAuthTest::credentialStorageUnavailable()
{
    // Unencrypted storage is never used for grants
    Fixture clearText(false);
    auto session = clearText.session(QStringLiteral("account-1"));
    QSignalSpy authorizationFailed(session.get(), &OAuth::Session::authorizationFailed);
    session->authorize();
    QCOMPARE(authorizationFailed.size(), 1);
    QCOMPARE(authorizationFailed.first().first().toString(), OAuth::errorMessage(OAuth::Error::StorageUnavailable));
    QVERIFY(clearText.browser.opened.isEmpty());

    QSignalSpy failed(session.get(), &OAuth::TokenSource::failed);
    session->requestAccessToken();
    QVERIFY(failed.wait(1000));
    QCOMPARE(session->lastError(), OAuth::Error::StorageUnavailable);
    QSignalSpy state(session.get(), &OAuth::Session::signInStateKnown);
    session->checkSignedIn();
    QVERIFY(state.wait(1000));
    QCOMPARE(state.first().first().toBool(), false);
    QCOMPARE(session->lastError(), OAuth::Error::StorageUnavailable);
    QVERIFY(clearText.plugin.credentialTypes.isEmpty());

    // No credential plugin at all
    OAuth::Session noPlugin(testConfig(QStringLiteral("account-1"), clearText.endpoint.url()),
                            []() -> Plugins::PasswordPlugin * { return nullptr; });
    QSignalSpy signOutFailed(&noPlugin, &OAuth::Session::signOutFailed);
    noPlugin.signOut();
    QVERIFY(signOutFailed.wait(1000));
    QCOMPARE(signOutFailed.first().first().toString(), OAuth::errorMessage(OAuth::Error::StorageUnavailable));

    // A locked or read-only wallet
    Fixture locked;
    locked.plugin.failWrites = true;
    auto lockedSession = locked.session(QStringLiteral("account-1"));
    QSignalSpy storeFailed(lockedSession.get(), &OAuth::Session::authorizationFailed);
    locked.endpoint.reply(200, tokenReply(QStringLiteral("access-1"), 3600, QStringLiteral("refresh-1")));
    lockedSession->authorize();
    QVERIFY(storeFailed.wait(5000));
    QCOMPARE(lockedSession->lastError(), OAuth::Error::StorageUnavailable);
    QVERIFY(locked.plugin.entries.isEmpty());

    locked.plugin.failReads = true;
    locked.plugin.entries.insert(FakePasswordPlugin::key(QStringLiteral("account-1")), storedGrant(QStringLiteral("r")));
    QSignalSpy readFailed(lockedSession.get(), &OAuth::TokenSource::failed);
    lockedSession->requestAccessToken();
    QVERIFY(readFailed.wait(1000));
    QCOMPARE(lockedSession->lastError(), OAuth::Error::StorageUnavailable);
    QCOMPARE(readFailed.first().first().toString(), OAuth::errorMessage(OAuth::Error::StorageUnavailable));
}

void OAuthTest::accountsAreIsolated()
{
    Fixture fixture;
    auto work = fixture.session(QStringLiteral("account-1"));
    auto home = fixture.session(QStringLiteral("account-2"));
    QVERIFY(authorize(fixture, *work));
    QCOMPARE(fixture.plugin.entries.keys(), QStringList{FakePasswordPlugin::key(QStringLiteral("account-1"))});

    QSignalSpy homeState(home.get(), &OAuth::Session::signInStateKnown);
    home->checkSignedIn();
    QVERIFY(homeState.wait(1000));
    QCOMPARE(homeState.first().first().toBool(), false);
    QSignalSpy workState(work.get(), &OAuth::Session::signInStateKnown);
    work->checkSignedIn();
    QVERIFY(workState.wait(1000));
    QCOMPARE(workState.first().first().toBool(), true);

    QSignalSpy homeFailed(home.get(), &OAuth::TokenSource::failed);
    home->requestAccessToken();
    QVERIFY(homeFailed.wait(1000));
    QCOMPARE(home->lastError(), OAuth::Error::NotSignedIn);
    QCOMPARE(fixture.endpoint.requests.size(), 1);

    // Signing one account out leaves the other signed in
    QSignalSpy homeSignedOut(home.get(), &OAuth::Session::signedOut);
    home->signOut();
    QVERIFY(homeSignedOut.wait(1000));
    QCOMPARE(fixture.plugin.entries.keys(), QStringList{FakePasswordPlugin::key(QStringLiteral("account-1"))});
    QSignalSpy workTokens(work.get(), &OAuth::TokenSource::accessTokenAvailable);
    work->requestAccessToken();
    QVERIFY(workTokens.wait(1000));
    QCOMPARE(workTokens.first().first().toString(), QStringLiteral("access-1"));
}

void OAuthTest::signOutRemovesTheCredentials()
{
    Fixture fixture;
    auto session = fixture.session(QStringLiteral("account-1"));
    QVERIFY(authorize(fixture, *session));
    QCOMPARE(fixture.plugin.entries.size(), 1);

    QSignalSpy signedOut(session.get(), &OAuth::Session::signedOut);
    fixture.endpoint.reply(200, QByteArray());
    session->signOut();
    QVERIFY(signedOut.wait(5000));
    QVERIFY(fixture.plugin.entries.isEmpty());
    // The provider revoked the refresh token first
    QCOMPARE(signedOut.first().first().value<OAuth::Session::Revocation>(), OAuth::Session::Revocation::Revoked);
    QCOMPARE(fixture.endpoint.requests.size(), 2);
    QCOMPARE(fixture.endpoint.paths.last(), QByteArray("/revoke"));
    QCOMPARE(fixture.endpoint.requests.last().queryItemValue(QStringLiteral("token")), QStringLiteral("refresh-1"));

    // Neither the cached access token nor the grant are used afterwards
    QSignalSpy tokens(session.get(), &OAuth::TokenSource::accessTokenAvailable);
    QSignalSpy failed(session.get(), &OAuth::TokenSource::failed);
    session->requestAccessToken();
    QVERIFY(failed.wait(1000));
    QVERIFY(tokens.isEmpty());
    QCOMPARE(session->lastError(), OAuth::Error::NotSignedIn);
    QVERIFY(failed.first().first().toString().startsWith(OAuth::errorMessage(OAuth::Error::NotSignedIn)));
    QCOMPARE(fixture.endpoint.requests.size(), 2);

    // Signing out without a stored grant succeeds without asking the provider
    session->signOut();
    QVERIFY(signedOut.wait(1000));
    QCOMPARE(signedOut.size(), 2);
    QCOMPARE(signedOut.last().first().value<OAuth::Session::Revocation>(), OAuth::Session::Revocation::NotNeeded);
    QCOMPARE(fixture.endpoint.requests.size(), 2);
}

void OAuthTest::signOutRevocation_data()
{
    QTest::addColumn<QString>("provider");
    QTest::addColumn<int>("status");
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<OAuth::Session::Revocation>("expected");

    QTest::newRow("revoked") << QStringLiteral("google") << 200 << QByteArray() << OAuth::Session::Revocation::Revoked;
    // RFC 7009 section 2.2: a token which is already invalid needs no revocation
    QTest::newRow("already invalid") << QStringLiteral("google") << 400
                                     << QByteArray(R"({"error":"invalid_token"})")
                                     << OAuth::Session::Revocation::Revoked;
    QTest::newRow("provider error") << QStringLiteral("google") << 503 << QByteArray()
                                    << OAuth::Session::Revocation::Failed;
    QTest::newRow("rejected request") << QStringLiteral("google") << 400
                                      << QByteArray(R"({"error":"invalid_request"})")
                                      << OAuth::Session::Revocation::Failed;
    QTest::newRow("no endpoint") << QStringLiteral("microsoft") << 0 << QByteArray()
                                 << OAuth::Session::Revocation::Unsupported;
}

void OAuthTest::signOutRevocation()
{
    QFETCH(QString, provider);
    QFETCH(int, status);
    QFETCH(QByteArray, body);
    QFETCH(OAuth::Session::Revocation, expected);

    Fixture fixture;
    auto session = fixture.session(QStringLiteral("account-1"), provider);
    QVERIFY(authorize(fixture, *session));
    if (status)
        fixture.endpoint.reply(status, body);
    QSignalSpy signedOut(session.get(), &OAuth::Session::signedOut);
    QSignalSpy signOutFailed(session.get(), &OAuth::Session::signOutFailed);
    session->signOut();
    QVERIFY(signedOut.wait(5000));
    QCOMPARE(signedOut.first().first().value<OAuth::Session::Revocation>(), expected);
    QVERIFY(signOutFailed.isEmpty());
    // The local grant is gone whatever the provider answered
    QVERIFY(fixture.plugin.entries.isEmpty());
    QCOMPARE(fixture.endpoint.requests.size(), status ? 2 : 1);
}

void OAuthTest::clientSecretIsStoredWithTheGrant()
{
    Fixture fixture;
    const QString key = FakePasswordPlugin::key(QStringLiteral("account-1"));
    auto entered = fixture.session(QStringLiteral("account-1"), QStringLiteral("google"), QStringLiteral("s3cret"));
    QVERIFY(authorize(fixture, *entered));
    QCOMPARE(fixture.endpoint.requests.last().queryItemValue(QStringLiteral("client_secret")), QStringLiteral("s3cret"));
    QCOMPARE(OAuth::Grant::fromJson(fixture.plugin.entries.value(key)).clientSecret, QStringLiteral("s3cret"));
    QCOMPARE(entered->storedClientSecret(), QStringLiteral("s3cret"));

    // Without a configured secret, the renewal uses the stored one and a rotated grant keeps it
    auto session = fixture.session(QStringLiteral("account-1"));
    QSignalSpy state(session.get(), &OAuth::Session::signInStateKnown);
    session->checkSignedIn();
    QVERIFY(state.wait(1000));
    QCOMPARE(session->storedClientSecret(), QStringLiteral("s3cret"));
    fixture.endpoint.reply(200, tokenReply(QStringLiteral("access-2"), 3600, QStringLiteral("refresh-2")));
    QSignalSpy tokens(session.get(), &OAuth::TokenSource::accessTokenAvailable);
    session->requestAccessToken();
    QVERIFY(tokens.wait(5000));
    QCOMPARE(fixture.endpoint.requests.last().queryItemValue(QStringLiteral("client_secret")), QStringLiteral("s3cret"));
    QTRY_COMPARE(OAuth::Grant::fromJson(fixture.plugin.entries.value(key)).refreshToken, QStringLiteral("refresh-2"));
    QCOMPARE(OAuth::Grant::fromJson(fixture.plugin.entries.value(key)).clientSecret, QStringLiteral("s3cret"));

    // Signing in again without entering the secret keeps the stored one
    QVERIFY(authorize(fixture, *session));
    QCOMPARE(fixture.endpoint.requests.last().queryItemValue(QStringLiteral("grant_type")),
             QStringLiteral("authorization_code"));
    QCOMPARE(fixture.endpoint.requests.last().queryItemValue(QStringLiteral("client_secret")), QStringLiteral("s3cret"));
    QCOMPARE(OAuth::Grant::fromJson(fixture.plugin.entries.value(key)).clientSecret, QStringLiteral("s3cret"));

    // Revocation authenticates the client with the stored secret too
    fixture.endpoint.reply(200, QByteArray());
    QSignalSpy signedOut(session.get(), &OAuth::Session::signedOut);
    session->signOut();
    QVERIFY(signedOut.wait(5000));
    QCOMPARE(fixture.endpoint.paths.last(), QByteArray("/revoke"));
    QCOMPARE(fixture.endpoint.requests.last().queryItemValue(QStringLiteral("client_secret")), QStringLiteral("s3cret"));
    QVERIFY(session->storedClientSecret().isEmpty());
}

void OAuthTest::clientSecretMigratesFromTheSettings()
{
    Fixture fixture;
    const QString key = FakePasswordPlugin::key(QStringLiteral("account-1"));
    bool moved = false;

    // Without a grant, the secret stays in the settings
    auto legacy = fixture.session(QStringLiteral("account-1"), QStringLiteral("google"), QStringLiteral("legacy"));
    legacy->migrateClientSecret([&moved]() { moved = true; });
    QTest::qWait(100);
    QVERIFY(!moved);

    // A grant from before the migration gets the secret
    fixture.plugin.entries.insert(key, storedGrant(QStringLiteral("refresh-old")));
    legacy->migrateClientSecret([&moved]() { moved = true; });
    QTRY_VERIFY(moved);
    QCOMPARE(OAuth::Grant::fromJson(fixture.plugin.entries.value(key)).clientSecret, QStringLiteral("legacy"));
    QCOMPARE(OAuth::Grant::fromJson(fixture.plugin.entries.value(key)).refreshToken, QStringLiteral("refresh-old"));
    QCOMPARE(legacy->storedClientSecret(), QStringLiteral("legacy"));

    // Migrating again only confirms it
    moved = false;
    legacy->migrateClientSecret([&moved]() { moved = true; });
    QTRY_VERIFY(moved);

    // A different stored secret is not overwritten
    moved = false;
    auto other = fixture.session(QStringLiteral("account-1"), QStringLiteral("google"), QStringLiteral("other"));
    other->migrateClientSecret([&moved]() { moved = true; });
    QTest::qWait(100);
    QVERIFY(!moved);
    QCOMPARE(OAuth::Grant::fromJson(fixture.plugin.entries.value(key)).clientSecret, QStringLiteral("legacy"));
}

void OAuthTest::smtpRetriesOnceWithAFreshToken()
{
    FakeSmtpServer server;
    FakeTokenSource source;
    source.setServer(QStringLiteral("127.0.0.1"), server.port());
    MSA::SMTP smtp(nullptr, QStringLiteral("127.0.0.1"), server.port(), false, false, true, user);
    smtp.setTokenSource(&source);
    QSignalSpy sent(&smtp, &MSA::AbstractMSA::sent);
    QSignalSpy errors(&smtp, &MSA::AbstractMSA::error);
    QSignalSpy passwordRequested(&smtp, &MSA::AbstractMSA::passwordRequested);

    smtp.sendMail(user.toUtf8(), {QByteArrayLiteral("rcpt@example.com")},
                  QByteArrayLiteral("Subject: test\r\n\r\n.hidden\r\nbody\r\n"));
    QVERIFY2(sent.wait(10000), qPrintable(errors.isEmpty() ? QString() : errors.first().first().toString()));
    QVERIFY(errors.isEmpty());
    QVERIFY(passwordRequested.isEmpty());

    QCOMPARE(server.authCommands,
             (QList<QByteArray>{authCommand("XOAUTH2", OAuth::Sasl::xoauth2(user, QStringLiteral("token-1"))),
                                authCommand("XOAUTH2", OAuth::Sasl::xoauth2(user, QStringLiteral("token-2")))}));
    QCOMPARE(server.cancelResponses, QList<QByteArray>{QByteArray()});
    QCOMPARE(source.rejected, QStringList{QStringLiteral("token-1")});
    QCOMPARE(source.requests, 2);
    QCOMPARE(server.connections, 2);
    // The retried message is escaped only once
    QCOMPARE(server.messages.size(), 1);
    QVERIFY(server.messages.first().contains("\r\n..hidden\r\n"));
    QVERIFY(!server.messages.first().contains("..."));
}

void OAuthTest::smtpGivesUpAfterTheRetry()
{
    FakeSmtpServer server;
    server.authCapability = "AUTH OAUTHBEARER";
    server.rejectedAttempts = 2;
    FakeTokenSource source;
    source.setServer(QStringLiteral("smtp.example.test"), 587);
    MSA::SMTP smtp(nullptr, QStringLiteral("127.0.0.1"), server.port(), false, false, true, user);
    smtp.setTokenSource(&source);
    QSignalSpy sent(&smtp, &MSA::AbstractMSA::sent);
    QSignalSpy errors(&smtp, &MSA::AbstractMSA::error);

    smtp.sendMail(user.toUtf8(), {QByteArrayLiteral("rcpt@example.com")}, QByteArrayLiteral("Subject: test\r\n\r\nbody\r\n"));
    QVERIFY(errors.wait(10000));
    QTest::qWait(50);
    QCOMPARE(errors.size(), 1);
    QVERIFY(sent.isEmpty());
    const QString message = errors.first().first().toString();
    QVERIFY(message.startsWith(OAuth::errorMessage(OAuth::Error::Rejected)));
    QVERIFY(message.contains(QLatin1String("status=401")));
    // OAUTHBEARER carries the server name and port of the token source
    QCOMPARE(server.authCommands.size(), 2);
    QCOMPARE(server.authCommands.first(),
             authCommand("OAUTHBEARER", OAuth::Sasl::oauthBearer(user, QStringLiteral("token-1"),
                                                                 QStringLiteral("smtp.example.test"), 587)));
    QCOMPARE(server.cancelResponses, (QList<QByteArray>{"AQ==", "AQ=="}));
    QCOMPARE(source.rejected, QStringList{QStringLiteral("token-1")});
    QVERIFY(server.messages.isEmpty());
}

void OAuthTest::smtpWithoutOAuthMechanism()
{
    FakeSmtpServer server;
    server.authCapability = "AUTH PLAIN LOGIN";
    FakeTokenSource source;
    MSA::SMTP smtp(nullptr, QStringLiteral("127.0.0.1"), server.port(), false, false, true, user);
    smtp.setTokenSource(&source);
    QSignalSpy errors(&smtp, &MSA::AbstractMSA::error);
    QSignalSpy passwordRequested(&smtp, &MSA::AbstractMSA::passwordRequested);

    smtp.sendMail(user.toUtf8(), {QByteArrayLiteral("rcpt@example.com")}, QByteArrayLiteral("body\r\n"));
    QVERIFY(errors.wait(10000));
    QCOMPARE(errors.first().first().toString(), OAuth::errorMessage(OAuth::Error::MechanismUnavailable));
    QVERIFY(server.authCommands.isEmpty());
    QVERIFY(server.messages.isEmpty());
    QVERIFY(passwordRequested.isEmpty());
    QVERIFY(source.rejected.isEmpty());
}

void OAuthTest::smtpTokenFailure()
{
    FakeSmtpServer server;
    FakeTokenSource source;
    source.failure = OAuth::errorMessage(OAuth::Error::Revoked);
    MSA::SMTP smtp(nullptr, QStringLiteral("127.0.0.1"), server.port(), false, false, true, user);
    smtp.setTokenSource(&source);
    QSignalSpy errors(&smtp, &MSA::AbstractMSA::error);
    QSignalSpy passwordRequested(&smtp, &MSA::AbstractMSA::passwordRequested);

    smtp.sendMail(user.toUtf8(), {QByteArrayLiteral("rcpt@example.com")}, QByteArrayLiteral("body\r\n"));
    QVERIFY(errors.wait(5000));
    QCOMPARE(errors.first().first().toString(), source.failure);
    QCOMPARE(server.connections, 0);
    QVERIFY(passwordRequested.isEmpty());
}

QTEST_GUILESS_MAIN(OAuthTest)
