/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <optional>

#include <QSignalSpy>
#include <QTest>

#include "FakeNextcloudServer.h"
#include "Nextcloud/Client.h"
#include "Nextcloud/CredentialStore.h"
#include "Plugins/PasswordPlugin.h"

using namespace Nextcloud;

namespace {

class FakePasswordPlugin;

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
    void doStop() override
    {
        emit error(Stopped, QString());
    }

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

    Features features() const override
    {
        return m_encrypted ? FeatureEncryptedStorage : Features();
    }
    Plugins::PasswordJob *requestPassword(const QString &accountId, const QString &accountType) override
    {
        return new FakePasswordJob(
            this, FakePasswordJob::Operation::Read, accountId + QLatin1Char('/') + accountType, {});
    }
    Plugins::PasswordJob *storePassword(
        const QString &accountId, const QString &accountType, const QString &password) override
    {
        return new FakePasswordJob(
            this, FakePasswordJob::Operation::Store, accountId + QLatin1Char('/') + accountType, password);
    }
    Plugins::PasswordJob *deletePassword(const QString &accountId, const QString &accountType) override
    {
        return new FakePasswordJob(
            this, FakePasswordJob::Operation::Delete, accountId + QLatin1Char('/') + accountType, {});
    }

    QMap<QString, QString> entries;
    bool failWrites = false;

private:
    bool m_encrypted;
};

void FakePasswordJob::doStart()
{
    switch (m_operation) {
    case Operation::Read:
        if (!m_plugin->entries.contains(m_key))
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
        if (m_plugin->entries.remove(m_key) == 0)
            emit error(NoSuchPassword, QString());
        else
            emit passwordDeleted();
        break;
    }
}

/** @short What one request reported */
template <typename T>
struct Outcome {
    std::optional<T> value;
    std::optional<Client::Failure> failure;
    QString message;
    int reports = 0;

    bool finished() const
    {
        return reports > 0;
    }
    auto done()
    {
        return [this](const T &result) {
            value = result;
            ++reports;
        };
    }
    Client::Failed failed()
    {
        return [this](Client::Failure kind, const QString &text) {
            failure = kind;
            message = text;
            ++reports;
        };
    }
};

} // namespace

class NextcloudClientTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<FakeNextcloudServer> server;
    std::unique_ptr<Client> client;

    void signIn()
    {
        Account account;
        account.id = QStringLiteral("nc1");
        account.server = QUrl(server->address());
        account.loginName = server->loginName;
        account.userId = server->userId;
        client->setAccount(account);
        client->setAppPassword(server->appPassword);
    }

private slots:
    void init()
    {
        server = std::make_unique<FakeNextcloudServer>();
        client = std::make_unique<Client>();
        client->setTimeout(10);
    }

    void cleanup()
    {
        client.reset();
        server.reset();
    }

    void loginFlow()
    {
        Account account;
        account.server = QUrl(server->address());
        client->setAccount(account);

        Outcome<Client::LoginFlow> started;
        client->startLogin(started.done(), started.failed());
        QTRY_VERIFY(started.finished());
        QVERIFY2(started.value, qPrintable(started.message));
        QVERIFY(started.value->login.path().startsWith(QLatin1String("/index.php/login/v2/flow/")));
        QVERIFY(!started.value->token.isEmpty());
        QCOMPARE(server->requests.constLast().headers.value("authorization"), QByteArray());

        // Pending while the user has not granted access in the browser
        std::optional<bool> finished;
        Client::Credentials credentials;
        Outcome<int> poll;
        auto pollDone = [&](bool ready, const Client::Credentials &result) {
            finished = ready;
            credentials = result;
        };
        client->pollLogin(*started.value, pollDone, poll.failed());
        QTRY_VERIFY(finished.has_value());
        QCOMPARE(*finished, false);
        QVERIFY(!poll.failure);

        server->loginGranted = true;
        finished.reset();
        client->pollLogin(*started.value, pollDone, poll.failed());
        QTRY_VERIFY(finished.has_value());
        QCOMPARE(*finished, true);
        QCOMPARE(credentials.loginName, server->loginName);
        QCOMPARE(credentials.appPassword, server->appPassword);
        QCOMPARE(credentials.server, QUrl(server->address()));
        QCOMPARE(server->loginPolls, 2);
        QCOMPARE(
            QUrlQuery(QString::fromLatin1(server->requests.constLast().body)).queryItemValue(QStringLiteral("token")),
            started.value->token);
    }

    void loginAtSomethingElse()
    {
        Account account;
        account.server = QUrl(server->address() + QStringLiteral("/not-nextcloud"));
        client->setAccount(account);
        server->overrides.insert(QStringLiteral("/not-nextcloud/index.php/login/v2"), {404, QByteArray("Not found")});
        Outcome<Client::LoginFlow> started;
        client->startLogin(started.done(), started.failed());
        QTRY_VERIFY(started.finished());
        QCOMPARE(started.failure, std::optional(Client::Failure::Protocol));
        QVERIFY(started.message.contains(QLatin1String("does not look like Nextcloud")));
    }

    void insecureServerSendsNothing()
    {
        Account account;
        account.server = QUrl(QStringLiteral("http://cloud.example.invalid"));
        account.loginName = QStringLiteral("alice");
        account.userId = QStringLiteral("alice");
        client->setAccount(account);
        client->setAppPassword(QStringLiteral("secret"));
        Outcome<Listing> listed;
        const Client::RequestId id = client->list(QStringLiteral("/"), listed.done(), listed.failed());
        QVERIFY(id != 0);
        QVERIFY(!listed.finished());
        QTRY_VERIFY(listed.finished());
        QCOMPARE(listed.failure, std::optional(Client::Failure::Security));
        QCOMPARE(client->runningCount(), 0);
    }

    void fetchUser()
    {
        signIn();
        Outcome<Client::User> user;
        client->fetchUser(user.done(), user.failed());
        QTRY_VERIFY(user.finished());
        QVERIFY2(user.value, qPrintable(user.message));
        QCOMPARE(user.value->id, server->userId);
        QCOMPARE(user.value->displayName, server->displayName);
        QCOMPARE(user.value->email, server->loginName);
        QCOMPARE(user.value->quotaFree, qint64(1000000));
        QCOMPARE(server->requests.constLast().headers.value("ocs-apirequest"), QByteArray("true"));
    }

    void wrongAppPassword()
    {
        signIn();
        client->setAppPassword(QStringLiteral("revoked"));
        Outcome<Client::User> user;
        client->fetchUser(user.done(), user.failed());
        QTRY_VERIFY(user.finished());
        QCOMPARE(user.failure, std::optional(Client::Failure::Authentication));
        QCOMPARE(user.reports, 1);

        Outcome<Listing> listed;
        client->list(QStringLiteral("/"), listed.done(), listed.failed());
        QTRY_VERIFY(listed.finished());
        QCOMPARE(listed.failure, std::optional(Client::Failure::Authentication));
        QVERIFY2(listed.message.contains(QLatin1String("No public access")), qPrintable(listed.message));
    }

    void list()
    {
        server->addFolder(QStringLiteral("/Photos"));
        server->addFolder(QStringLiteral("/Read only"), QStringLiteral("SG"));
        server->addFile(QStringLiteral("/Notes & plans.txt"), "hello");
        server->addFile(QStringLiteral("/Photos/cat.jpg"), "meow");
        server->addLinkShare(QStringLiteral("/Notes & plans.txt"));
        signIn();

        Outcome<Listing> listed;
        client->list(QStringLiteral("/"), listed.done(), listed.failed());
        QTRY_VERIFY(listed.finished());
        QVERIFY2(listed.value, qPrintable(listed.message));
        QCOMPARE(listed.value->folder.path, QStringLiteral("/"));
        QVERIFY(listed.value->folder.folder);
        QCOMPARE(listed.value->children.size(), 3);
        QCOMPARE(listed.value->children.at(0).name, QStringLiteral("Photos"));
        QCOMPARE(listed.value->children.at(1).name, QStringLiteral("Read only"));
        QVERIFY(!listed.value->children.at(1).permissions.canCreateFile());
        QVERIFY(listed.value->children.at(1).permissions.isShared());
        const Entry &notes = listed.value->children.at(2);
        QCOMPARE(notes.path, QStringLiteral("/Notes & plans.txt"));
        QCOMPARE(notes.size, qint64(5));
        QVERIFY(notes.hasLinkShare());
        QCOMPARE(server->requests.constLast().verb, QByteArray("PROPFIND"));
        QCOMPARE(server->requests.constLast().headers.value("depth"), QByteArray("1"));

        Outcome<Listing> sub;
        client->list(QStringLiteral("/Photos/"), sub.done(), sub.failed());
        QTRY_VERIFY(sub.finished());
        QVERIFY2(sub.value, qPrintable(sub.message));
        QCOMPARE(sub.value->folder.path, QStringLiteral("/Photos"));
        QCOMPARE(sub.value->children.size(), 1);
        QCOMPARE(sub.value->children.constFirst().path, QStringLiteral("/Photos/cat.jpg"));

        Outcome<Listing> gone;
        client->list(QStringLiteral("/Gone"), gone.done(), gone.failed());
        QTRY_VERIFY(gone.finished());
        QCOMPARE(gone.failure, std::optional(Client::Failure::NotFound));
        QVERIFY2(gone.message.contains(QLatin1String("could not be located")), qPrintable(gone.message));
    }

    void serverInSubfolder()
    {
        server->base = QStringLiteral("/nextcloud");
        server->addFile(QStringLiteral("/a.txt"), "a");
        signIn();
        Outcome<Listing> listed;
        client->list(QStringLiteral("/"), listed.done(), listed.failed());
        QTRY_VERIFY(listed.finished());
        QVERIFY2(listed.value, qPrintable(listed.message));
        QCOMPARE(listed.value->children.size(), 1);
        QVERIFY(server->requests.constLast().path.startsWith(QLatin1String("/nextcloud/remote.php/dav/files/alice")));
    }

    void download()
    {
        server->addFile(QStringLiteral("/Q&A #1/50% done?.txt"), QByteArray(5000, 'x'));
        server->addFolder(QStringLiteral("/Q&A #1"));
        signIn();
        Outcome<QByteArray> fetched;
        qint64 lastDone = 0;
        client->download(QStringLiteral("/Q&A #1/50% done?.txt"), fetched.done(), fetched.failed(),
            [&](qint64 done, qint64) { lastDone = done; });
        QTRY_VERIFY(fetched.finished());
        QVERIFY2(fetched.value, qPrintable(fetched.message));
        QCOMPARE(fetched.value->size(), qsizetype(5000));
        QCOMPARE(lastDone, qint64(5000));

        Outcome<QByteArray> missing;
        client->download(QStringLiteral("/nothing.txt"), missing.done(), missing.failed());
        QTRY_VERIFY(missing.finished());
        QCOMPARE(missing.failure, std::optional(Client::Failure::NotFound));
    }

    void upload()
    {
        server->addFolder(QStringLiteral("/Inbox"));
        server->addFile(QStringLiteral("/Inbox/report.pdf"), "old");
        server->addFolder(QStringLiteral("/Shared"), QStringLiteral("SGV"));
        signIn();

        Outcome<bool> created;
        client->upload(QStringLiteral("/Inbox/new.pdf"), "new", false, [&] { created.done()(true); }, created.failed());
        QTRY_VERIFY(created.finished());
        QVERIFY2(created.value, qPrintable(created.message));
        QCOMPARE(server->nodes.value(QStringLiteral("/Inbox/new.pdf")).content, QByteArray("new"));
        QCOMPARE(server->requests.constLast().headers.value("if-none-match"), QByteArray("*"));

        Outcome<bool> exists;
        client->upload(
            QStringLiteral("/Inbox/report.pdf"), "newer", false, [&] { exists.done()(true); }, exists.failed());
        QTRY_VERIFY(exists.finished());
        QCOMPARE(exists.failure, std::optional(Client::Failure::Exists));
        QCOMPARE(server->nodes.value(QStringLiteral("/Inbox/report.pdf")).content, QByteArray("old"));

        Outcome<bool> replaced;
        client->upload(
            QStringLiteral("/Inbox/report.pdf"), "newer", true, [&] { replaced.done()(true); }, replaced.failed());
        QTRY_VERIFY(replaced.finished());
        QVERIFY2(replaced.value, qPrintable(replaced.message));
        QCOMPARE(server->nodes.value(QStringLiteral("/Inbox/report.pdf")).content, QByteArray("newer"));
        QVERIFY(!server->requests.constLast().headers.contains("if-none-match"));

        Outcome<bool> denied;
        client->upload(QStringLiteral("/Shared/x.txt"), "x", false, [&] { denied.done()(true); }, denied.failed());
        QTRY_VERIFY(denied.finished());
        QCOMPARE(denied.failure, std::optional(Client::Failure::Permission));

        Outcome<bool> noFolder;
        client->upload(QStringLiteral("/Missing/x.txt"), "x", false, [&] { noFolder.done()(true); }, noFolder.failed());
        QTRY_VERIFY(noFolder.finished());
        QCOMPARE(noFolder.failure, std::optional(Client::Failure::NotFound));

        server->quotaFull = true;
        Outcome<bool> full;
        client->upload(
            QStringLiteral("/Inbox/big.bin"), QByteArray(100, 'b'), false, [&] { full.done()(true); }, full.failed());
        QTRY_VERIFY(full.finished());
        QCOMPARE(full.failure, std::optional(Client::Failure::Quota));
        QVERIFY2(full.message.contains(QLatin1String("Insufficient storage")), qPrintable(full.message));
    }

    void shares()
    {
        server->addFile(QStringLiteral("/a.txt"), "a");
        server->addFolder(QStringLiteral("/Drop box"));
        const QString first
            = server->addLinkShare(QStringLiteral("/a.txt"), Share::Read, true, QStringLiteral("2026-12-31"));
        server->addLinkShare(QStringLiteral("/Drop box"), Share::Create);
        signIn();

        Outcome<QList<Share>> all;
        client->shares(QString(), all.done(), all.failed());
        QTRY_VERIFY(all.finished());
        QVERIFY2(all.value, qPrintable(all.message));
        QCOMPARE(all.value->size(), 2);

        Outcome<QList<Share>> one;
        client->shares(QStringLiteral("/a.txt"), one.done(), one.failed());
        QTRY_VERIFY(one.finished());
        QVERIFY2(one.value, qPrintable(one.message));
        QCOMPARE(one.value->size(), 1);
        const Share &share = one.value->constFirst();
        QCOMPARE(share.token, first);
        QVERIFY(share.hasPassword);
        QCOMPARE(share.expiration, QDate(2026, 12, 31));
        QCOMPARE(server->requests.constLast().verb, QByteArray("GET"));
        QCOMPARE(server->requests.constLast().headers.value("ocs-apirequest"), QByteArray("true"));

        Outcome<QList<Share>> folder;
        client->shares(QStringLiteral("/Drop box"), folder.done(), folder.failed());
        QTRY_VERIFY(folder.finished());
        QVERIFY2(folder.value, qPrintable(folder.message));
        QCOMPARE(folder.value->size(), 1);
        QVERIFY(folder.value->constFirst().folder);

        Outcome<QList<Share>> missing;
        client->shares(QStringLiteral("/nope"), missing.done(), missing.failed());
        QTRY_VERIFY(missing.finished());
        QCOMPARE(missing.failure, std::optional(Client::Failure::NotFound));

        // Only GET requests went to the share API: nothing is created, changed or
        // revoked
        for (const FakeNextcloudServer::Request &request : std::as_const(server->requests)) {
            if (request.path.contains(QLatin1String("/shares")))
                QCOMPARE(request.verb, QByteArray("GET"));
        }
    }

    void redirects()
    {
        server->addFile(QStringLiteral("/a.txt"), "content");
        signIn();
        const QString davRoot = QStringLiteral("/remote.php/dav/files/alice");
        server->overrides.insert(davRoot + QStringLiteral("/moved.txt"),
            {302, (server->address() + davRoot + QStringLiteral("/a.txt")).toUtf8()});
        Outcome<QByteArray> followed;
        client->download(QStringLiteral("/moved.txt"), followed.done(), followed.failed());
        QTRY_VERIFY(followed.finished());
        QVERIFY2(followed.value, qPrintable(followed.message));
        QCOMPARE(*followed.value, QByteArray("content"));
        QVERIFY(server->requests.constLast().headers.contains("authorization"));

        // To another origin, the app password stays behind
        const QString otherOrigin = QStringLiteral("http://localhost:%1").arg(QUrl(server->address()).port());
        server->overrides.insert(davRoot + QStringLiteral("/elsewhere.txt"),
            {302, (otherOrigin + davRoot + QStringLiteral("/a.txt")).toUtf8()});
        Outcome<QByteArray> crossed;
        client->download(QStringLiteral("/elsewhere.txt"), crossed.done(), crossed.failed());
        QTRY_VERIFY(crossed.finished());
        QCOMPARE(crossed.failure, std::optional(Client::Failure::Authentication));
        QVERIFY(!server->requests.constLast().headers.contains("authorization"));

        server->overrides.insert(
            davRoot + QStringLiteral("/plain.txt"), {301, QByteArray("http://cloud.example.invalid/a.txt")});
        Outcome<QByteArray> insecure;
        client->download(QStringLiteral("/plain.txt"), insecure.done(), insecure.failed());
        QTRY_VERIFY(insecure.finished());
        QCOMPARE(insecure.failure, std::optional(Client::Failure::Security));

        server->overrides.insert(davRoot + QStringLiteral("/loop.txt"), {302, QByteArray("loop.txt")});
        Outcome<QByteArray> loop;
        client->download(QStringLiteral("/loop.txt"), loop.done(), loop.failed());
        QTRY_VERIFY(loop.finished());
        QCOMPARE(loop.failure, std::optional(Client::Failure::Protocol));
        QCOMPARE(server->requests.size(), qsizetype(5 + 6));

        // A POST is not resent to where a 302 points
        server->overrides.insert(QStringLiteral("/index.php/login/v2"), {302, QByteArray("/elsewhere")});
        Outcome<Client::LoginFlow> login;
        client->startLogin(login.done(), login.failed());
        QTRY_VERIFY(login.finished());
        QCOMPARE(login.failure, std::optional(Client::Failure::Protocol));
    }

    void abort()
    {
        server->addFile(QStringLiteral("/a.txt"), "a");
        signIn();
        Outcome<QByteArray> first;
        Outcome<QByteArray> second;
        const Client::RequestId id = client->download(QStringLiteral("/a.txt"), first.done(), first.failed());
        client->download(QStringLiteral("/a.txt"), second.done(), second.failed());
        QCOMPARE(client->runningCount(), 2);
        client->abort(id);
        QCOMPARE(client->runningCount(), 1);
        QTRY_VERIFY(second.finished());
        QTest::qWait(50);
        QCOMPARE(first.reports, 0);

        Outcome<QByteArray> third;
        client->download(QStringLiteral("/a.txt"), third.done(), third.failed());
        client->abortAll();
        QCOMPARE(client->runningCount(), 0);
        QTest::qWait(50);
        QCOMPARE(third.reports, 0);

        // Destroying the client reports nothing either
        Outcome<QByteArray> fourth;
        client->download(QStringLiteral("/a.txt"), fourth.done(), fourth.failed());
        client.reset();
        QTest::qWait(50);
        QCOMPARE(fourth.reports, 0);
    }

    void credentialStore()
    {
        FakePasswordPlugin plugin;
        CredentialStore store([&plugin]() -> Plugins::PasswordPlugin * { return &plugin; });
        QCOMPARE(store.availability(), CredentialStore::Error::NoError);

        std::optional<CredentialStore::Error> result;
        QString password;
        auto loaded = [&](CredentialStore::Error error, const QString &value) {
            result = error;
            password = value;
        };
        store.load(QStringLiteral("nc1"), this, loaded);
        QTRY_VERIFY(result);
        QCOMPARE(*result, CredentialStore::Error::NotFound);

        result.reset();
        store.save(QStringLiteral("nc1"), QStringLiteral("app-password-1"), this,
            [&](CredentialStore::Error e) { result = e; });
        QTRY_VERIFY(result);
        QCOMPARE(*result, CredentialStore::Error::NoError);
        QCOMPARE(plugin.entries.value(QStringLiteral("nc1/nextcloud")), QStringLiteral("app-password-1"));

        result.reset();
        store.load(QStringLiteral("nc1"), this, loaded);
        QTRY_VERIFY(result);
        QCOMPARE(*result, CredentialStore::Error::NoError);
        QCOMPARE(password, QStringLiteral("app-password-1"));

        result.reset();
        store.remove(QStringLiteral("nc1"), this, [&](CredentialStore::Error e) { result = e; });
        QTRY_VERIFY(result);
        QCOMPARE(*result, CredentialStore::Error::NoError);
        QVERIFY(plugin.entries.isEmpty());

        // Deleting what is not stored succeeds
        result.reset();
        store.remove(QStringLiteral("nc1"), this, [&](CredentialStore::Error e) { result = e; });
        QTRY_VERIFY(result);
        QCOMPARE(*result, CredentialStore::Error::NoError);

        plugin.failWrites = true;
        result.reset();
        store.save(QStringLiteral("nc1"), QStringLiteral("x"), this, [&](CredentialStore::Error e) { result = e; });
        QTRY_VERIFY(result);
        QCOMPARE(*result, CredentialStore::Error::StorageUnavailable);
    }

    void credentialStoreNeedsEncryption()
    {
        FakePasswordPlugin plain(false);
        CredentialStore store([&plain]() -> Plugins::PasswordPlugin * { return &plain; });
        QCOMPARE(store.availability(), CredentialStore::Error::StorageUnavailable);
        std::optional<CredentialStore::Error> result;
        store.save(
            QStringLiteral("nc1"), QStringLiteral("secret"), this, [&](CredentialStore::Error e) { result = e; });
        QTRY_VERIFY(result);
        QCOMPARE(*result, CredentialStore::Error::StorageUnavailable);
        QVERIFY(plain.entries.isEmpty());

        result.reset();
        store.remove(QStringLiteral("nc1"), this, [&](CredentialStore::Error e) { result = e; });
        QTRY_VERIFY(result);
        QCOMPARE(*result, CredentialStore::Error::NoError);

        CredentialStore none([]() -> Plugins::PasswordPlugin * { return nullptr; });
        QCOMPARE(none.availability(), CredentialStore::Error::StorageUnavailable);
        result.reset();
        none.load(QStringLiteral("nc1"), this, [&](CredentialStore::Error e, const QString &) { result = e; });
        QTRY_VERIFY(result);
        QCOMPARE(*result, CredentialStore::Error::StorageUnavailable);
    }
};

QTEST_GUILESS_MAIN(NextcloudClientTest)

#include "test_NextcloudClient.moc"
