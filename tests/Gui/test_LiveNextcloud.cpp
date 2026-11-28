/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <memory>
#include <optional>

#include <KLocalizedString>
#include <KSharedConfig>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "../Nextcloud/FakePasswordPlugin.h"
#include "../Utils/LibMailboxSync.h"
#include "Gui/AttachmentPanel.h"
#include "Gui/NextcloudHub.h"
#include "Gui/NextcloudTransfers.h"
#include "Nextcloud/Account.h"
#include "Nextcloud/Client.h"
#include "Nextcloud/Shares.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/TaskFactory.h"
#include "Imap/Network/MsgPartNetAccessManager.h"
#include "LocalMail/LocalSocket.h"
#include "LocalMail/Store.h"
#include "UiUtils/ActivityLog.h"

using Gui::NextcloudHub;
using Gui::NextcloudTransfers;
using Nextcloud::Client;
using UiUtils::ActivityLog;

/** @short The Nextcloud workflows of the mail against a real, disposable server

Opt-in: tools/dev-container/live-nextcloud.sh starts Nextcloud with the users
"work" and "other", their files and shares, and passes the server, an app
password and a revocable one, and a link shared by "other" through
PLASMA_TEST_NEXTCLOUD_* variables. Without them, the test is skipped.
*/
class LiveNextcloudTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain(QByteArrayLiteral("plasma-impresario"));
        m_server = QUrl(qEnvironmentVariable("PLASMA_TEST_NEXTCLOUD_URL"));
        if (m_server.isEmpty())
            QSKIP("Run tools/dev-container/live-nextcloud.sh, which starts the "
                  "server this test needs");
        m_login = qEnvironmentVariable("PLASMA_TEST_NEXTCLOUD_LOGIN");
        m_appPassword = qEnvironmentVariable("PLASMA_TEST_NEXTCLOUD_APP_PASSWORD");
        m_revokablePassword = qEnvironmentVariable("PLASMA_TEST_NEXTCLOUD_REVOKABLE_PASSWORD");
        m_foreignLink = QUrl(qEnvironmentVariable("PLASMA_TEST_NEXTCLOUD_FOREIGN_LINK"));
        QVERIFY(!m_appPassword.isEmpty());
        QVERIFY(!m_revokablePassword.isEmpty());
        QVERIFY2(m_foreignLink.isValid(), qPrintable(m_foreignLink.toString()));

        m_directory = std::make_unique<QTemporaryDir>();
        QVERIFY(m_directory->isValid());
        m_plugin = std::make_unique<FakePasswordPlugin>();
        m_profile
            = std::make_unique<QSettings>(m_directory->filePath(QStringLiteral("profile.ini")), QSettings::IniFormat);
        auto config = KSharedConfig::openConfig(m_directory->filePath(QStringLiteral("rc")), KConfig::SimpleConfig);
        FakePasswordPlugin *plugin = m_plugin.get();
        m_hub = std::make_unique<NextcloudHub>(
            config, m_profile.get(), [plugin]() -> Plugins::PasswordPlugin * { return plugin; });
        m_hub->setConfirm([](QWidget *, const QString &, const QString &) { return false; });
        m_transfers = std::make_unique<NextcloudTransfers>(m_hub.get());
        m_transfers->setActivityLog(&m_log);
    }

    void signsInWithAnAppPassword()
    {
        // As the connect dialog does: who the app password belongs to, then the
        // account
        Nextcloud::Account account;
        account.id = QStringLiteral("live-work");
        account.server = m_server;
        account.loginName = m_login;
        const std::optional<Client::User> user = fetchUser(account, m_appPassword);
        QVERIFY(user);
        QCOMPARE(user->id, QStringLiteral("work"));
        QCOMPARE(user->displayName, QStringLiteral("Work Synthetic"));
        // The script limits the storage to 3 MB
        QVERIFY2(
            user->quotaFree > 0 && user->quotaFree <= 3 * 1024 * 1024, qPrintable(QString::number(user->quotaFree)));
        account.userId = user->id;
        account.displayName = user->displayName;
        QVERIFY(addAccount(account, m_appPassword));
        m_account = account.id;
        QCOMPARE(m_hub->account(m_account).title().isEmpty(), false);
    }

    void listsFoldersWithTheirPermissions()
    {
        QVERIFY(!m_account.isEmpty());
        const std::optional<Nextcloud::Listing> root = list(QStringLiteral("/"));
        QVERIFY(root);
        const Nextcloud::Entry mail = child(*root, QStringLiteral("Mail"));
        QVERIFY(mail.folder);
        QVERIFY(mail.permissions.canCreateFile());
        QVERIFY(!mail.permissions.isShared());

        // Shared with the user for reading only: shown, but nothing can be saved
        // there
        const Nextcloud::Entry team = child(*root, QStringLiteral("Team"));
        QVERIFY(team.folder);
        QVERIFY2(team.permissions.isShared(), qPrintable(team.permissions.letters));
        QVERIFY2(!team.permissions.canCreateFile(), qPrintable(team.permissions.letters));

        // The user's own public link shows on the file
        const Nextcloud::Entry shared = child(*root, QStringLiteral("Shared.pdf"));
        QVERIFY(!shared.folder);
        QVERIFY(shared.hasLinkShare());
        QVERIFY(!shared.etag.isEmpty());
        QVERIFY(shared.fileId > 0);
    }

    void savesAttachmentsWithoutReplacingFiles()
    {
        QVERIFY(!m_account.isEmpty());
        // If-None-Match keeps the first file; the second gets a numbered name
        QCOMPARE(store(QStringLiteral("/Mail"), QStringLiteral("invoice.pdf"), "first invoice"),
            QStringLiteral("/Mail/invoice.pdf"));
        QCOMPARE(store(QStringLiteral("/Mail"), QStringLiteral("invoice.pdf"), "second invoice"),
            QStringLiteral("/Mail/invoice (2).pdf"));
        QCOMPARE(fetch(QStringLiteral("/Mail/invoice.pdf")), QByteArray("first invoice"));
        QCOMPARE(fetch(QStringLiteral("/Mail/invoice (2).pdf")), QByteArray("second invoice"));
        // A name with a folder in it stays one file in the chosen folder
        const QString saved = store(QStringLiteral("/Mail"), QStringLiteral("../Escaped.txt"), "escaped");
        QVERIFY2(saved.startsWith(QLatin1String("/Mail/")), qPrintable(saved));
        QCOMPARE(saved.count(QLatin1Char('/')), 2);

        // Each transfer shows in the activity log as done
        QVERIFY(m_log.count() >= 5);
        for (int row = 0; row < m_log.count(); ++row) {
            QCOMPARE(m_log.entry(row).kind, ActivityLog::Kind::Job);
            QCOMPARE(m_log.entry(row).state, ActivityLog::State::Done);
        }
    }

    void fetchesWhatOthersSharedWithTheUser()
    {
        QVERIFY(!m_account.isEmpty());
        QCOMPARE(fetch(QStringLiteral("/Team/notes.txt")), QByteArray("synthetic team notes"));
    }

    void savesAttachmentsOfLocalAccountMessages()
    {
        // POP3 and JMAP mail is kept in a local store which the mail model reads
        // through an in-process IMAP session. The attachment of such a message is
        // read the way the attachment panel reads it before saving it to Nextcloud.
        QVERIFY(!m_account.isEmpty());
        QString error;
        const std::shared_ptr<LocalMail::Store> mailStore
            = LocalMail::Store::open(m_directory->filePath(QStringLiteral("local-mail")), &error);
        QVERIFY2(mailStore, qPrintable(error));
        mailStore->append(QStringLiteral("INBOX"),
            "From: Alice <alice@example.org>\r\nTo: work@example.org\r\nSubject: Local report\r\n"
            "Date: Mon, 02 Nov 2026 08:00:00 +0000\r\nMessage-ID: <local-report@example.org>\r\n"
            "MIME-Version: 1.0\r\nContent-Type: multipart/mixed; boundary=\"b\"\r\n\r\n"
            "--b\r\nContent-Type: text/plain\r\n\r\nThe report is attached.\r\n"
            "--b\r\nContent-Type: text/plain; name=\"local-report.txt\"\r\n"
            "Content-Disposition: attachment; filename=\"local-report.txt\"\r\n\r\n"
            "synthetic local attachment\r\n--b--\r\n");

        auto *model = new Imap::Mailbox::Model(this, std::make_shared<Imap::Mailbox::MemoryCache>(),
            Imap::Mailbox::SocketFactoryPtr(new LocalMail::LocalSocketFactory(mailStore)),
            Imap::Mailbox::TaskFactoryPtr(new Imap::Mailbox::TaskFactory()));
        LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_ONLINE);
        model->rowCount(QModelIndex());
        const auto findInbox = [model]() {
            for (int row = 0; row < model->rowCount(QModelIndex()); ++row) {
                const QModelIndex index = model->index(row, 0, QModelIndex());
                if (index.data(Imap::Mailbox::RoleMailboxName).toString() == QLatin1String("INBOX"))
                    return index;
            }
            return QModelIndex();
        };
        QTRY_VERIFY(findInbox().isValid());
        const QModelIndex inbox = findInbox();
        const QModelIndex list = model->index(0, 0, inbox);
        model->rowCount(list);
        QTRY_COMPARE(model->rowCount(list), 1);
        const QModelIndex message = model->index(0, 0, list);
        QPersistentModelIndex attachment;
        QTRY_VERIFY((attachment = findPart(model, message, QStringLiteral("local-report.txt"))).isValid());

        Imap::Network::MsgPartNetAccessManager netAccess;
        netAccess.setModelMessage(message);
        Gui::AttachmentPanel panel;
        panel.setNetworkAccessManager(&netAccess);
        QString fileName;
        QByteArray data;
        bool read = false;
        panel.readPart(attachment, this, [&](const QString &name, const QByteArray &bytes) {
            fileName = name;
            data = bytes;
            read = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(read, 30000);
        QCOMPARE(fileName, QStringLiteral("local-report.txt"));
        QCOMPARE(data, QByteArray("synthetic local attachment"));

        QCOMPARE(store(QStringLiteral("/Mail"), fileName, data), QStringLiteral("/Mail/local-report.txt"));
        QCOMPARE(fetch(QStringLiteral("/Mail/local-report.txt")), QByteArray("synthetic local attachment"));
        delete model;
    }

    void refusesSavingIntoReadOnlyShares()
    {
        QVERIFY(!m_account.isEmpty());
        QString error;
        QVERIFY(store(QStringLiteral("/Team"), QStringLiteral("a.txt"), "a", &error).isEmpty());
        QVERIFY2(error.contains(QLatin1String("may not create")), qPrintable(error));
        const ActivityLog::Entry entry = m_log.entry(0);
        QCOMPARE(entry.state, ActivityLog::State::Failed);
        QCOMPARE(entry.error, error);
        QVERIFY(m_log.canRetry(entry.key));
        const std::optional<Nextcloud::Listing> team = list(QStringLiteral("/Team"));
        QVERIFY(team);
        QCOMPARE(team->children.size(), 1);
    }

    void reportsAFullStorage()
    {
        QVERIFY(!m_account.isEmpty());
        QString error;
        QVERIFY(store(QStringLiteral("/Mail"), QStringLiteral("large.bin"), QByteArray(4 * 1024 * 1024, 'x'), &error)
                .isEmpty());
        QVERIFY2(error.contains(QLatin1String("full")), qPrintable(error));
        const std::optional<Nextcloud::Listing> mail = list(QStringLiteral("/Mail"));
        QVERIFY(mail);
        QVERIFY(child(*mail, QStringLiteral("large.bin")).path.isEmpty());
    }

    void reportsMissingFiles()
    {
        QVERIFY(!m_account.isEmpty());
        QString error;
        QVERIFY(fetch(QStringLiteral("/Missing.pdf"), &error).isNull());
        QVERIFY2(error.contains(QLatin1String("no longer exists")), qPrintable(error));
        QCOMPARE(m_log.entry(0).state, ActivityLog::State::Failed);
    }

    void findsExistingLinkSharesOnly()
    {
        QVERIFY(!m_account.isEmpty());
        const std::optional<Nextcloud::Listing> root = list(QStringLiteral("/"));
        QVERIFY(root);
        const QList<Nextcloud::Entry> files{child(*root, QStringLiteral("Shared.pdf")),
            child(*root, QStringLiteral("Public.txt")), child(*root, QStringLiteral("Mail"))};
        QList<NextcloudTransfers::Link> links;
        bool done = false;
        m_transfers->linkShares(m_account, files, this, [&](const QList<NextcloudTransfers::Link> &found) {
            links = found;
            done = true;
        });
        QTRY_VERIFY(done);
        QCOMPARE(links.size(), 3);
        QVERIFY2(links[0].url.isValid() && links[0].error.isEmpty(), qPrintable(links[0].error));
        QVERIFY(links[1].url.isValid());
        // No link is created for a file which has none
        QVERIFY(links[2].url.isEmpty());
        QVERIFY(links[2].error.isEmpty());

        // A message with these links and one shared by someone else
        const QString text = QStringLiteral("The report: %1\nThe notes: %2\nTheirs: %3\n")
                                 .arg(links[0].url.toString(), links[1].url.toString(), m_foreignLink.toString());
        const QList<QUrl> found = Nextcloud::findShareLinks(text);
        QCOMPARE(found, (QList<QUrl>{links[0].url, links[1].url, m_foreignLink}));
        QList<NextcloudHub::LinkShare> shares;
        done = false;
        m_hub->lookUpShares(found, this, [&](const QList<NextcloudHub::LinkShare> &result) {
            shares = result;
            done = true;
        });
        QTRY_VERIFY(done);
        QCOMPARE(shares.size(), 3);
        const QString protectedLink = shares[0].details.join(QLatin1Char(' '));
        QCOMPARE(shares[0].accountId, m_account);
        QVERIFY2(protectedLink.contains(QLatin1String("password")), qPrintable(protectedLink));
        QVERIFY2(protectedLink.contains(QLatin1String("2099")), qPrintable(protectedLink));
        const QString publicLink = shares[1].details.join(QLatin1Char(' '));
        QCOMPARE(shares[1].accountId, m_account);
        QVERIFY2(publicLink.contains(QLatin1String("does not expire")), qPrintable(publicLink));
        // Someone else's link on the same server is not described as the user's
        QVERIFY(shares[2].accountId.isEmpty());
        QVERIFY(shares[2].knownServer);
        QVERIFY(shares[2].problems.isEmpty());
        QVERIFY(shares[2].details.join(QLatin1Char(' ')).contains(QLatin1String("unknown")));
    }

    void unreachableServersFailWithoutWaitingLong()
    {
        // As when offline: the account is kept, its transfers fail and links cannot
        // be checked
        Nextcloud::Account account;
        account.id = QStringLiteral("live-offline");
        account.server = QUrl(QStringLiteral("http://localhost:9"));
        account.loginName = m_login;
        account.userId = QStringLiteral("work");
        QVERIFY(addAccount(account, m_appPassword));
        QString error;
        QVERIFY(fetch(QStringLiteral("/Shared.pdf"), &error, account.id).isNull());
        QVERIFY2(error.contains(QLatin1String("could not be reached")), qPrintable(error));
        QList<NextcloudHub::LinkShare> shares;
        bool done = false;
        m_hub->lookUpShares({QUrl(QStringLiteral("http://localhost:9/s/AbCdEfGh1234"))}, this,
            [&](const QList<NextcloudHub::LinkShare> &result) {
                shares = result;
                done = true;
            });
        QTRY_VERIFY(done);
        QCOMPARE(shares.size(), 1);
        QVERIFY(shares[0].knownServer);
        QVERIFY(shares[0].accountId.isEmpty());
        QVERIFY(!shares[0].problems.isEmpty());
    }

    void refusesARevokedAppPassword()
    {
        Nextcloud::Account account;
        account.server = m_server;
        account.loginName = m_login;
        account.userId = QStringLiteral("work");
        QVERIFY(fetchUser(account, m_revokablePassword));
        Client client;
        client.setAccount(account);
        client.setAppPassword(m_revokablePassword);
        bool revoked = false;
        QString error;
        client.revokeAppPassword([&]() { revoked = true; },
            [&](Client::Failure, const QString &message) {
                error = message.isEmpty() ? QStringLiteral("?") : message;
            });
        QTRY_VERIFY(revoked || !error.isEmpty());
        QVERIFY2(revoked, qPrintable(error));
        std::optional<Client::Failure> failure;
        fetchUser(account, m_revokablePassword, &failure);
        QCOMPARE(failure, std::optional<Client::Failure>(Client::Failure::Authentication));

        // In the window, the account is asked to sign in again
        account.id = QStringLiteral("live-revoked");
        QVERIFY(addAccount(account, m_revokablePassword));
        QVERIFY(fetch(QStringLiteral("/Shared.pdf"), &error, account.id).isNull());
        QCOMPARE(error, m_hub->refusedMessage(account.id));
        // The other app password still works
        QCOMPARE(fetch(QStringLiteral("/Shared.pdf")), QByteArray("synthetic shared report"));
    }

private:
    std::optional<Client::User> fetchUser(
        const Nextcloud::Account &account, const QString &password, std::optional<Client::Failure> *failure = nullptr)
    {
        Client client;
        client.setAccount(account);
        client.setAppPassword(password);
        std::optional<Client::User> user;
        bool done = false;
        client.fetchUser(
            [&](const Client::User &found) {
                user = found;
                done = true;
            },
            [&](Client::Failure kind, const QString &message) {
                if (failure)
                    *failure = kind;
                else
                    qWarning() << "Could not read the user:" << message;
                done = true;
            });
        if (!QTest::qWaitFor([&]() { return done; }, 30000))
            qWarning() << "No answer about the user";
        return user;
    }

    bool addAccount(const Nextcloud::Account &account, const QString &password)
    {
        bool done = false;
        QString error;
        m_hub->addAccount(account, password, this, [&](const QString &text) {
            error = text;
            done = true;
        });
        if (!QTest::qWaitFor([&]() { return done; }, 10000) || !error.isEmpty()) {
            qWarning() << "Could not add the account:" << error;
            return false;
        }
        return true;
    }

    std::optional<Nextcloud::Listing> list(const QString &path)
    {
        std::optional<Nextcloud::Listing> listing;
        bool done = false;
        m_hub->withClient(
            m_account, this,
            [&](Client *client) {
                client->list(
                    path,
                    [&](const Nextcloud::Listing &found) {
                        listing = found;
                        done = true;
                    },
                    [&](Client::Failure, const QString &message) {
                        qWarning() << "Could not list" << path << message;
                        done = true;
                    });
            },
            [&](const QString &message) {
                qWarning() << "No client:" << message;
                done = true;
            });
        QTest::qWaitFor([&]() { return done; }, 30000);
        return listing;
    }

    static Nextcloud::Entry child(const Nextcloud::Listing &listing, const QString &name)
    {
        for (const Nextcloud::Entry &entry : listing.children) {
            if (entry.name == name)
                return entry;
        }
        return {};
    }

    QString store(const QString &folder, const QString &name, const QByteArray &data, QString *error = nullptr)
    {
        QString path;
        QString failure;
        bool done = false;
        m_transfers->store(
            m_account, folder, name, data, this,
            [&](const QString &stored) {
                path = stored;
                done = true;
            },
            [&](const QString &message) {
                failure = message;
                done = true;
            });
        QTest::qWaitFor([&]() { return done; }, 60000);
        if (error)
            *error = failure;
        else if (!failure.isEmpty())
            qWarning() << "Could not store" << name << failure;
        return path;
    }

    /** Walks the message's parts, asking the model for each, until one has @p fileName */
    static QModelIndex findPart(QAbstractItemModel *model, const QModelIndex &parent, const QString &fileName)
    {
        const int rows = model->rowCount(parent);
        for (int row = 0; row < rows; ++row) {
            const QModelIndex part = model->index(row, 0, parent);
            if (part.data(Imap::Mailbox::RolePartFileName).toString() == fileName)
                return part;
            const QModelIndex found = findPart(model, part, fileName);
            if (found.isValid())
                return found;
        }
        return {};
    }

    QByteArray fetch(const QString &path, QString *error = nullptr, const QString &accountId = QString())
    {
        Nextcloud::Entry file;
        file.path = path;
        file.name = path.section(QLatin1Char('/'), -1);
        QByteArray data;
        QString failure;
        bool done = false;
        m_transfers->fetch(
            accountId.isEmpty() ? m_account : accountId, file, this,
            [&](const QByteArray &bytes) {
                data = bytes.isNull() ? QByteArray("") : bytes;
                done = true;
            },
            [&](const QString &message) {
                failure = message;
                done = true;
            });
        QTest::qWaitFor([&]() { return done; }, 60000);
        if (error)
            *error = failure;
        else if (!failure.isEmpty())
            qWarning() << "Could not fetch" << path << failure;
        return data;
    }

    QUrl m_server;
    QString m_login;
    QString m_appPassword;
    QString m_revokablePassword;
    QUrl m_foreignLink;
    QString m_account;
    std::unique_ptr<QTemporaryDir> m_directory;
    std::unique_ptr<FakePasswordPlugin> m_plugin;
    std::unique_ptr<QSettings> m_profile;
    std::unique_ptr<NextcloudHub> m_hub;
    std::unique_ptr<NextcloudTransfers> m_transfers;
    ActivityLog m_log;
};

QTEST_MAIN(LiveNextcloudTest)
#include "test_LiveNextcloud.moc"
