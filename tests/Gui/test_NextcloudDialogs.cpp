/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <memory>
#include <optional>

#include <KLocalizedString>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>

#include "../Nextcloud/FakeNextcloudServer.h"
#include "../Nextcloud/FakePasswordPlugin.h"
#include "Gui/NextcloudBrowserDialog.h"
#include "Gui/NextcloudConnectDialog.h"
#include "Gui/NextcloudHub.h"
#include "Gui/NextcloudShareReviewDialog.h"
#include "Gui/NextcloudTransfers.h"
#include "Nextcloud/OnlineAccounts.h"
#include "UiUtils/ActivityLog.h"

using Gui::NextcloudBrowserDialog;
using Gui::NextcloudConnectDialog;
using Gui::NextcloudHub;
using Gui::NextcloudShareReviewDialog;
using Gui::NextcloudTransfers;

/** @short KDE Online Accounts as the hub sees it */
class FakeOnlineAccounts : public Nextcloud::OnlineAccounts
{
public:
    bool available = true;
    QList<Nextcloud::Account> listed;
    QString loginName;
    QString password;
    std::optional<Problem> problem;
    int asked = 0;

    bool isAvailable() const override
    {
        return available;
    }
    QList<Nextcloud::Account> accounts() const override
    {
        return available ? listed : QList<Nextcloud::Account>();
    }
    void credentials(quint32, QObject *context, Credentials done, Failed failed) override
    {
        ++asked;
        QPointer<QObject> guard(context);
        QTimer::singleShot(0, this, [this, guard, done, failed] {
            if (!guard)
                return;
            if (problem)
                failed(*problem, QString());
            else
                done(loginName, password);
        });
    }
    void change()
    {
        emit accountsChanged();
    }
};

class NextcloudDialogsTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain(QByteArrayLiteral("plasma-impresario"));
    }

    void init()
    {
        m_directory = std::make_unique<QTemporaryDir>();
        QVERIFY(m_directory->isValid());
        m_server = std::make_unique<FakeNextcloudServer>();
        m_plugin = std::make_unique<FakePasswordPlugin>();
        m_profile
            = std::make_unique<QSettings>(m_directory->filePath(QStringLiteral("profile.ini")), QSettings::IniFormat);
        m_hub = makeHub(m_plugin.get());
    }

    void cleanup()
    {
        m_hub.reset();
        m_profile.reset();
        m_plugin.reset();
        m_server.reset();
        m_directory.reset();
    }

    void connectsWithAppPassword()
    {
        NextcloudConnectDialog dialog(m_hub.get());
        QString connected;
        connect(&dialog, &NextcloudConnectDialog::connected, this, [&](const QString &id) { connected = id; });
        auto *button = dialog.findChild<QPushButton *>(QStringLiteral("nextcloudConnect"));
        QVERIFY(button);
        QVERIFY(!button->isEnabled());
        dialog.setServer(m_server->address());
        dialog.setUseAppPassword(true);
        dialog.setLoginName(m_server->loginName);
        QVERIFY(!button->isEnabled());
        dialog.setAppPassword(m_server->appPassword);
        QVERIFY(button->isEnabled());
        button->click();
        QVERIFY(dialog.isBusy());
        QTRY_VERIFY(!connected.isEmpty());
        QVERIFY(!dialog.isBusy());

        QCOMPARE(m_hub->accounts().size(), 1);
        const Nextcloud::Account account = m_hub->account(connected);
        QCOMPARE(account.userId, m_server->userId);
        QCOMPARE(account.loginName, m_server->loginName);
        QCOMPARE(account.displayName, m_server->displayName);
        QCOMPARE(m_plugin->entries.value(connected + QStringLiteral("/nextcloud")), m_server->appPassword);

        // The accounts survive a restart; the app password is not in the
        // configuration
        auto again = makeHub(m_plugin.get());
        QCOMPARE(again->accounts(), m_hub->accounts());
        QFile rc(m_directory->filePath(QStringLiteral("rc")));
        QVERIFY(rc.open(QIODevice::ReadOnly));
        QVERIFY(!rc.readAll().contains(m_server->appPassword.toUtf8()));
    }

    void refusedAppPasswordIsNotKept()
    {
        NextcloudConnectDialog dialog(m_hub.get());
        dialog.setServer(m_server->address());
        dialog.setUseAppPassword(true);
        dialog.setLoginName(m_server->loginName);
        dialog.setAppPassword(QStringLiteral("wrong"));
        dialog.connectAccount();
        QTRY_VERIFY(!dialog.isBusy());
        QVERIFY(dialog.statusText().contains(QLatin1String("refused")));
        QVERIFY(m_hub->accounts().isEmpty());
        QVERIFY(m_plugin->entries.isEmpty());
    }

    void connectsInTheBrowserAndReplacesTheSameUser()
    {
        QList<QUrl> opened;
        QString connected;
        {
            NextcloudConnectDialog dialog(m_hub.get());
            dialog.setUrlOpener([&](const QUrl &url) { opened << url; });
            dialog.setPollInterval(10);
            connect(&dialog, &NextcloudConnectDialog::connected, this, [&](const QString &id) { connected = id; });
            dialog.setServer(m_server->address() + QStringLiteral("/index.php/apps/files/"));
            dialog.connectAccount();
            QTRY_COMPARE(opened.size(), 1);
            QVERIFY(opened.constFirst().path().contains(QLatin1String("/login/v2/flow/")));
            auto *reopen = dialog.findChild<QPushButton *>(QStringLiteral("nextcloudReopenSignIn"));
            QVERIFY(reopen && !reopen->isHidden());
            reopen->click();
            QCOMPARE(opened.size(), 2);
            QTRY_VERIFY(m_server->loginPolls >= 2);
            QVERIFY(connected.isEmpty());
            m_server->loginGranted = true;
            QTRY_VERIFY(!connected.isEmpty());
        }
        QCOMPARE(m_hub->accounts().size(), 1);
        QCOMPARE(m_hub->account(connected).server, QUrl(m_server->address()));

        // Signing in as the same user again replaces the account and its app
        // password
        m_server->appPassword = QStringLiteral("app-password-2");
        NextcloudConnectDialog again(m_hub.get());
        again.setUrlOpener([](const QUrl &) { });
        again.setPollInterval(10);
        again.setServer(m_server->address());
        again.connectAccount();
        QTRY_COMPARE(again.accountId(), connected);
        QCOMPARE(m_hub->accounts().size(), 1);
        QCOMPARE(m_plugin->entries.value(connected + QStringLiteral("/nextcloud")), QStringLiteral("app-password-2"));
    }

    void browserSignInExpires()
    {
        NextcloudConnectDialog dialog(m_hub.get());
        dialog.setUrlOpener([](const QUrl &) { });
        dialog.setPollInterval(10);
        dialog.setPollLimit(100);
        dialog.setServer(m_server->address());
        dialog.connectAccount();
        QTRY_VERIFY(m_server->loginPolls > 0);
        QTRY_VERIFY(!dialog.isBusy());
        QVERIFY(dialog.statusText().contains(QLatin1String("not finished in time")));
        const int polls = m_server->loginPolls;
        QTest::qWait(50);
        QCOMPARE(m_server->loginPolls, polls);
        QVERIFY(m_hub->accounts().isEmpty());
    }

    void cancellingStopsTheSignIn()
    {
        NextcloudConnectDialog dialog(m_hub.get());
        dialog.setUrlOpener([](const QUrl &) { });
        dialog.setPollInterval(10);
        dialog.setServer(m_server->address());
        dialog.connectAccount();
        QTRY_VERIFY(m_server->loginPolls > 0);
        dialog.reject();
        QVERIFY(!dialog.isBusy());
        // A poll sent just before may still arrive at the server, which runs in this thread
        QTest::qWait(50);
        const int polls = m_server->loginPolls;
        QTest::qWait(100);
        QCOMPARE(m_server->loginPolls, polls);
    }

    void needsEncryptedStorageAndTls()
    {
        FakePasswordPlugin plain(false);
        auto hub = makeHub(&plain);
        NextcloudConnectDialog dialog(hub.get());
        dialog.setServer(m_server->address());
        auto *button = dialog.findChild<QPushButton *>(QStringLiteral("nextcloudConnect"));
        QVERIFY(!button->isEnabled());
        QVERIFY(dialog.statusText().contains(QLatin1String("encrypted password storage")));
        dialog.connectAccount();
        QVERIFY(!dialog.isBusy());
        QVERIFY(m_server->requests.isEmpty());

        NextcloudConnectDialog plainHttp(m_hub.get());
        plainHttp.setServer(QStringLiteral("http://cloud.example.com"));
        plainHttp.connectAccount();
        QVERIFY(!plainHttp.isBusy());
        QVERIFY(plainHttp.statusText().contains(QLatin1String("https://")));
    }

    void browsesFoldersWithTheirAccess()
    {
        m_server->addFolder(QStringLiteral("/Documents"));
        m_server->addFile(QStringLiteral("/Documents/report.pdf"), "pdf");
        m_server->addFolder(QStringLiteral("/Shared"), QStringLiteral("SRGDNV"));
        m_server->addFile(QStringLiteral("/notes.txt"), "notes", QStringLiteral("RG"));
        m_server->addLinkShare(QStringLiteral("/Documents/report.pdf"));
        const QString id = addAccount();

        NextcloudBrowserDialog dialog(m_hub.get(), NextcloudBrowserDialog::Mode::Browse);
        QList<QUrl> opened;
        dialog.setUrlOpener([&](const QUrl &url) { opened << url; });
        QCOMPARE(dialog.accountId(), id);
        QTRY_COMPARE(dialog.entries().size(), 3);
        auto *tree = dialog.findChild<QTreeWidget *>(QStringLiteral("nextcloudFiles"));
        QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("Documents"));
        QCOMPARE(tree->topLevelItem(1)->text(0), QStringLiteral("Shared"));
        QVERIFY(tree->topLevelItem(1)->text(3).contains(QLatin1String("Shared with you")));
        QVERIFY(tree->topLevelItem(1)->text(3).contains(QLatin1String("Read only")));
        QVERIFY(tree->topLevelItem(2)->text(3).contains(QLatin1String("Read only")));
        auto *up = dialog.findChild<QToolButton *>(QStringLiteral("nextcloudUp"));
        QVERIFY(!up->isEnabled());

        emit tree->itemActivated(tree->topLevelItem(0), 0);
        QTRY_COMPARE(dialog.entries().size(), 1);
        QCOMPARE(dialog.path(), QStringLiteral("/Documents"));
        QVERIFY(tree->topLevelItem(0)->text(3).contains(QLatin1String("Public link")));
        dialog.findChild<QToolButton *>(QStringLiteral("nextcloudOpenInBrowser"))->click();
        QCOMPARE(opened.size(), 1);
        QVERIFY(opened.constFirst().toString().contains(QLatin1String("Documents")));

        QVERIFY(up->isEnabled());
        up->click();
        QTRY_COMPARE(dialog.entries().size(), 3);
        QCOMPARE(dialog.path(), QStringLiteral("/"));
    }

    void navigatingAgainDropsTheEarlierListing()
    {
        m_server->addFolder(QStringLiteral("/A"));
        m_server->addFolder(QStringLiteral("/B"));
        m_server->addFile(QStringLiteral("/B/b.txt"), "b");
        addAccount();
        NextcloudBrowserDialog dialog(m_hub.get(), NextcloudBrowserDialog::Mode::Browse);
        dialog.setPath(QStringLiteral("/A"));
        dialog.setPath(QStringLiteral("/B"));
        QTRY_COMPARE(dialog.entries().size(), 1);
        QTest::qWait(50);
        QCOMPARE(dialog.path(), QStringLiteral("/B"));
        QCOMPARE(dialog.entries().constFirst().name, QStringLiteral("b.txt"));
    }

    void choosesFiles()
    {
        m_server->addFolder(QStringLiteral("/Folder"));
        m_server->addFile(QStringLiteral("/a.txt"), "a");
        m_server->addFile(QStringLiteral("/b.txt"), "b");
        const QString id = addAccount();
        NextcloudBrowserDialog dialog(m_hub.get(), NextcloudBrowserDialog::Mode::ChooseFiles);
        QString chosenAccount;
        QList<Nextcloud::Entry> chosen;
        connect(&dialog, &NextcloudBrowserDialog::filesChosen, this,
            [&](const QString &account, const QList<Nextcloud::Entry> &files) {
                chosenAccount = account;
                chosen = files;
            });
        QTRY_COMPARE(dialog.entries().size(), 3);
        auto *choose = dialog.findChild<QPushButton *>(QStringLiteral("nextcloudChoose"));
        auto *tree = dialog.findChild<QTreeWidget *>(QStringLiteral("nextcloudFiles"));
        QVERIFY(!choose->isEnabled());
        tree->topLevelItem(0)->setSelected(true);
        QVERIFY(!choose->isEnabled());
        tree->topLevelItem(1)->setSelected(true);
        tree->topLevelItem(2)->setSelected(true);
        QVERIFY(choose->isEnabled());
        choose->click();
        QCOMPARE(chosenAccount, id);
        QCOMPARE(chosen.size(), 2);
        QCOMPARE(chosen.at(0).path, QStringLiteral("/a.txt"));
        QCOMPARE(chosen.at(1).path, QStringLiteral("/b.txt"));
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
    }

    void choosesOnlyWritableFolders()
    {
        m_server->addFolder(QStringLiteral("/ReadOnly"), QStringLiteral("SRG"));
        m_server->addFolder(QStringLiteral("/Writable"));
        addAccount();
        NextcloudBrowserDialog dialog(m_hub.get(), NextcloudBrowserDialog::Mode::ChooseFolder);
        QString folder;
        connect(&dialog, &NextcloudBrowserDialog::folderChosen, this,
            [&](const QString &, const QString &path) { folder = path; });
        auto *choose = dialog.findChild<QPushButton *>(QStringLiteral("nextcloudChoose"));
        dialog.setPath(QStringLiteral("/ReadOnly"));
        QTRY_VERIFY(dialog.statusText().contains(QLatin1String("empty")));
        QVERIFY(!choose->isEnabled());
        QVERIFY(!choose->toolTip().isEmpty());
        dialog.setPath(QStringLiteral("/Writable"));
        QTRY_VERIFY(choose->isEnabled());
        choose->click();
        QCOMPARE(folder, QStringLiteral("/Writable"));
    }

    void reportsFailuresWithRetry()
    {
        addAccount();
        m_server->appPassword = QStringLiteral("changed");
        NextcloudBrowserDialog dialog(m_hub.get(), NextcloudBrowserDialog::Mode::Browse);
        auto *retry = dialog.findChild<QPushButton *>(QStringLiteral("nextcloudRetry"));
        QTRY_VERIFY(!retry->isHidden());
        QVERIFY(dialog.statusText().contains(QLatin1String("refused")));

        dialog.setPath(QStringLiteral("/missing"));
        QTRY_VERIFY(!retry->isHidden());
        // The app password is read again after a refusal; still the wrong one
        QVERIFY(dialog.statusText().contains(QLatin1String("refused")));
    }

    void offersToConnectWithoutAccounts()
    {
        NextcloudBrowserDialog dialog(m_hub.get(), NextcloudBrowserDialog::Mode::Browse);
        auto *pages = dialog.findChild<QStackedWidget *>();
        QCOMPARE(pages->currentWidget()->objectName(), QStringLiteral("nextcloudEmpty"));
        int requested = 0;
        connect(&dialog, &NextcloudBrowserDialog::connectRequested, this, [&] { ++requested; });
        dialog.findChild<QPushButton *>(QStringLiteral("nextcloudConnectAccount"))->click();
        QCOMPARE(requested, 1);

        m_server->addFile(QStringLiteral("/a.txt"), "a");
        addAccount();
        QVERIFY(pages->currentWidget()->objectName() != QLatin1String("nextcloudEmpty"));
        QTRY_COMPARE(dialog.entries().size(), 1);
    }

    void disconnectingRevokesTheAppPassword()
    {
        const QString id = addAccount();
        QString warning = QStringLiteral("not called");
        QObject context;
        m_hub->removeAccount(id, &context, [&](const QString &text) { warning = text; });
        QTRY_VERIFY(warning != QLatin1String("not called"));
        QVERIFY(warning.isEmpty());
        QVERIFY(m_server->appPasswordRevoked);
        QVERIFY(m_hub->accounts().isEmpty());
        QVERIFY(m_plugin->entries.isEmpty());
    }

    void disconnectingWarnsWhenTheServerIsGone()
    {
        const QString id = addAccount();
        m_server.reset();
        QString warning = QStringLiteral("not called");
        QObject context;
        m_hub->removeAccount(id, &context, [&](const QString &text) { warning = text; });
        QTRY_VERIFY(warning != QLatin1String("not called"));
        QVERIFY(warning.contains(QLatin1String("revoke"), Qt::CaseInsensitive));
        QVERIFY(m_hub->accounts().isEmpty());
        QVERIFY(m_plugin->entries.isEmpty());
        m_server = std::make_unique<FakeNextcloudServer>();
    }

    void transfersFetchFiles()
    {
        m_server->addFile(QStringLiteral("/report.pdf"), "pdf content");
        const QString id = addAccount();
        NextcloudTransfers transfers(m_hub.get());
        Nextcloud::Entry file;
        file.path = QStringLiteral("/report.pdf");
        file.name = QStringLiteral("report.pdf");
        QByteArray data;
        QString error = QStringLiteral("not called");
        transfers.fetch(
            id, file, this,
            [&](const QByteArray &bytes) {
                data = bytes;
                error.clear();
            },
            [&](const QString &message) { error = message; });
        QCOMPARE(transfers.runningCount(), 1);
        QTRY_COMPARE(data, QByteArray("pdf content"));
        QVERIFY(error.isEmpty());
        QTRY_COMPARE(transfers.runningCount(), 0);
    }

    void transfersStoreWithoutReplacing()
    {
        m_server->addFolder(QStringLiteral("/Mail"));
        m_server->addFile(QStringLiteral("/Mail/a.txt"), "original");
        const QString id = addAccount();
        NextcloudTransfers transfers(m_hub.get());
        QStringList stored;
        QString error;
        for (int i = 0; i < 2; ++i) {
            transfers.store(
                id, QStringLiteral("/Mail/"), QStringLiteral("a.txt"), "copy", this,
                [&](const QString &path) { stored << path; }, [&](const QString &message) { error = message; });
            QTRY_COMPARE(stored.size(), i + 1);
        }
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(stored, (QStringList{QStringLiteral("/Mail/a (2).txt"), QStringLiteral("/Mail/a (3).txt")}));
        QCOMPARE(m_server->nodes.value(QStringLiteral("/Mail/a.txt")).content, QByteArray("original"));
        QCOMPARE(m_server->nodes.value(QStringLiteral("/Mail/a (3).txt")).content, QByteArray("copy"));

        // A name with a slash stays one file in the chosen folder
        stored.clear();
        transfers.store(
            id, QStringLiteral("/Mail"), QStringLiteral("../b/c.txt"), "c", this,
            [&](const QString &path) { stored << path; }, [&](const QString &message) { error = message; });
        QTRY_COMPARE(stored.size(), 1);
        QVERIFY(stored.first().startsWith(QLatin1String("/Mail/")));
        QCOMPARE(stored.first().count(QLatin1Char('/')), 2);
        QTRY_COMPARE(transfers.runningCount(), 0);
    }

    void transfersReportFoldersWithoutPermission()
    {
        m_server->addFolder(QStringLiteral("/ReadOnly"), QStringLiteral("SRG"));
        const QString id = addAccount();
        NextcloudTransfers transfers(m_hub.get());
        QString error;
        bool stored = false;
        transfers.store(
            id, QStringLiteral("/ReadOnly"), QStringLiteral("a.txt"), "a", this,
            [&](const QString &) { stored = true; }, [&](const QString &message) { error = message; });
        QTRY_VERIFY(!error.isEmpty());
        QVERIFY(!stored);
        QVERIFY2(error.contains(QLatin1String("may not create")), qPrintable(error));
        QVERIFY(!m_server->nodes.contains(QStringLiteral("/ReadOnly/a.txt")));
    }

    void transfersReportRefusedAppPasswords()
    {
        m_server->addFile(QStringLiteral("/a.txt"), "a");
        const QString id = addAccount();
        m_server->appPassword = QStringLiteral("changed");
        NextcloudTransfers transfers(m_hub.get());
        Nextcloud::Entry file;
        file.path = QStringLiteral("/a.txt");
        QString error;
        transfers.fetch(id, file, this, [](const QByteArray &) { }, [&](const QString &message) { error = message; });
        QTRY_VERIFY(!error.isEmpty());
        QVERIFY2(error.contains(QLatin1String("refused")), qPrintable(error));
    }

    void transfersShowInTheActivityLog()
    {
        using UiUtils::ActivityLog;
        m_server->addFile(QStringLiteral("/report.pdf"), "pdf content");
        m_server->addFolder(QStringLiteral("/Locked"), QStringLiteral("SRG"));
        const QString id = addAccount();
        ActivityLog log;
        NextcloudTransfers transfers(m_hub.get());
        transfers.setActivityLog(&log);

        // A download shows as running, then as done
        Nextcloud::Entry file;
        file.path = QStringLiteral("/report.pdf");
        file.name = QStringLiteral("report.pdf");
        bool fetched = false;
        transfers.fetch(id, file, this, [&](const QByteArray &) { fetched = true; }, [](const QString &) { });
        QCOMPARE(log.count(), 1);
        QCOMPARE(log.entry(0).state, ActivityLog::State::Running);
        QCOMPARE(log.entry(0).kind, ActivityLog::Kind::Job);
        QVERIFY(log.entry(0).title.contains(QLatin1String("report.pdf")));
        QCOMPARE(log.entry(0).detail, m_hub->account(id).title());
        QTRY_VERIFY(fetched);
        QCOMPARE(log.entry(0).state, ActivityLog::State::Done);
        QCOMPARE(log.entry(0).progress, -1);

        // A refused upload fails with the reason and can be retried with the same data
        QStringList stored;
        int failures = 0;
        transfers.store(
            id, QStringLiteral("/Locked"), QStringLiteral("a.txt"), "retried", this,
            [&](const QString &path) { stored << path; }, [&](const QString &) { ++failures; });
        QCOMPARE(log.count(), 2);
        const QString key = log.entry(0).key;
        QVERIFY(log.entry(0).title.contains(QLatin1String("a.txt")));
        QTRY_COMPARE(failures, 1);
        QCOMPARE(log.entry(0).state, ActivityLog::State::Failed);
        QVERIFY2(log.entry(0).error.contains(QLatin1String("may not create")), qPrintable(log.entry(0).error));
        QVERIFY(log.canRetry(key));
        QCOMPARE(log.entry(0).action, ActivityLog::Action::Retry);

        m_server->nodes[QStringLiteral("/Locked")].permissions = QStringLiteral("RGDNVCK");
        QVERIFY(log.retry(key));
        QCOMPARE(log.count(), 2);
        QCOMPARE(log.entry(log.find(key)).state, ActivityLog::State::Running);
        QTRY_COMPARE(stored, QStringList{QStringLiteral("/Locked/a.txt")});
        QCOMPARE(failures, 1);
        QCOMPARE(log.entry(log.find(key)).state, ActivityLog::State::Done);
        QCOMPARE(log.entry(log.find(key)).detail, QStringLiteral("/Locked/a.txt"));
        QVERIFY(!log.canRetry(key));
        QCOMPARE(m_server->nodes.value(QStringLiteral("/Locked/a.txt")).content, QByteArray("retried"));
        QTRY_COMPARE(transfers.runningCount(), 0);
    }

    void transfersStopWithTheirWindow()
    {
        using UiUtils::ActivityLog;
        m_server->addFolder(QStringLiteral("/Mail"));
        const QString id = addAccount();
        ActivityLog log;
        NextcloudTransfers transfers(m_hub.get());
        transfers.setActivityLog(&log);
        auto context = std::make_unique<QObject>();
        bool called = false;
        transfers.store(
            id, QStringLiteral("/Mail"), QStringLiteral("a.txt"), "a", context.get(),
            [&](const QString &) { called = true; }, [&](const QString &) { called = true; });
        context.reset();
        QTRY_COMPARE(transfers.runningCount(), 0);
        QVERIFY(!called);
        QCOMPARE(log.count(), 1);
        QVERIFY(log.entry(0).state != ActivityLog::State::Running);
        QVERIFY(!log.canRetry(log.entry(0).key));
    }

    void transfersFindExistingLinkShares()
    {
        m_server->addFile(QStringLiteral("/shared.pdf"), "s");
        m_server->addFile(QStringLiteral("/private.pdf"), "p");
        m_server->addLinkShare(QStringLiteral("/shared.pdf"));
        const QString id = addAccount();
        NextcloudTransfers transfers(m_hub.get());
        Nextcloud::Entry shared;
        shared.path = QStringLiteral("/shared.pdf");
        Nextcloud::Entry unshared;
        unshared.path = QStringLiteral("/private.pdf");
        Nextcloud::Entry missing;
        missing.path = QStringLiteral("/gone.pdf");
        QList<NextcloudTransfers::Link> links;
        bool done = false;
        transfers.linkShares(id, {shared, unshared, missing}, this, [&](const QList<NextcloudTransfers::Link> &found) {
            links = found;
            done = true;
        });
        QTRY_VERIFY(done);
        QCOMPARE(links.size(), 3);
        QCOMPARE(links[0].file.path, shared.path);
        QVERIFY(links[0].url.isValid());
        QVERIFY(links[0].url.path().contains(QLatin1String("/s/")));
        QVERIFY(links[1].url.isEmpty());
        QVERIFY(links[1].error.isEmpty());
        QVERIFY(links[2].url.isEmpty());
        QVERIFY(!links[2].error.isEmpty());
        // Only read: no share was created
        QCOMPARE(m_server->shares.size(), 1);
        QTRY_COMPARE(transfers.runningCount(), 0);
    }

    void transfersStopReportingWithoutTheirContext()
    {
        m_server->addFile(QStringLiteral("/a.txt"), "a");
        const QString id = addAccount();
        NextcloudTransfers transfers(m_hub.get());
        Nextcloud::Entry file;
        file.path = QStringLiteral("/a.txt");
        int calls = 0;
        auto context = std::make_unique<QObject>();
        transfers.fetch(
            id, file, context.get(), [&](const QByteArray &) { ++calls; }, [&](const QString &) { ++calls; });
        context.reset();
        QTRY_COMPARE(transfers.runningCount(), 0);
        QTest::qWait(50);
        QCOMPARE(calls, 0);
    }

    void onlineAccountsSignInWithTheDesktopsCredentials()
    {
        m_server->addFile(QStringLiteral("/a.txt"), "a");
        FakeOnlineAccounts online;
        online.listed = {onlineAccount(7)};
        online.loginName = m_server->loginName;
        online.password = m_server->appPassword;
        QSignalSpy changed(m_hub.get(), &NextcloudHub::accountsChanged);
        m_hub->setOnlineAccounts(&online);
        QVERIFY(m_hub->hasOnlineAccounts());
        QCOMPARE(changed.size(), 1);
        QCOMPARE(m_hub->accounts().size(), 1);
        const Nextcloud::Account account = m_hub->accounts().constFirst();
        QCOMPARE(account.source, Nextcloud::Account::Source::OnlineAccounts);
        QCOMPARE(account.onlineAccountId, 7u);
        QCOMPARE(account.userId, m_server->userId);

        NextcloudTransfers transfers(m_hub.get());
        Nextcloud::Entry file;
        file.path = QStringLiteral("/a.txt");
        QByteArray fetched;
        QString error;
        transfers.fetch(
            account.id, file, this, [&](const QByteArray &data) { fetched = data; },
            [&](const QString &message) { error = message; });
        QTRY_COMPARE(fetched, QByteArray("a"));
        QVERIFY(error.isEmpty());
        QCOMPARE(online.asked, 1);
        // The credentials stay with the desktop
        QVERIFY(m_plugin->entries.isEmpty());
        QFile rc(m_directory->filePath(QStringLiteral("rc")));
        QVERIFY(!rc.open(QIODevice::ReadOnly) || !rc.readAll().contains(m_server->appPassword.toUtf8()));

        // Removed in System Settings rather than here
        bool done = false;
        QString warning;
        m_hub->removeAccount(account.id, this, [&](const QString &text) {
            warning = text;
            done = true;
        });
        QTRY_VERIFY(done);
        QVERIFY2(warning.contains(QLatin1String("System Settings")), qPrintable(warning));
        QCOMPARE(m_hub->accounts().size(), 1);
        QVERIFY(!m_server->appPasswordRevoked);

        online.listed.clear();
        online.change();
        QCOMPARE(changed.size(), 2);
        QVERIFY(m_hub->accounts().isEmpty());
        error.clear();
        transfers.fetch(
            account.id, file, this, [](const QByteArray &) { }, [&](const QString &message) { error = message; });
        QTRY_VERIFY(!error.isEmpty());
        QVERIFY2(error.contains(QLatin1String("no longer connected")), qPrintable(error));
        m_hub->setOnlineAccounts(nullptr);
    }

    void onlineAccountProblemsAreReported()
    {
        m_server->addFile(QStringLiteral("/a.txt"), "a");
        FakeOnlineAccounts online;
        online.listed = {onlineAccount(7)};
        online.loginName = m_server->loginName;
        online.password = m_server->appPassword;
        m_hub->setOnlineAccounts(&online);
        const QString id = Nextcloud::onlineAccountKey(7);
        NextcloudTransfers transfers(m_hub.get());
        Nextcloud::Entry file;
        file.path = QStringLiteral("/a.txt");
        auto fetchError = [&]() {
            QString error;
            bool fetched = false;
            transfers.fetch(
                id, file, this, [&](const QByteArray &) { fetched = true; },
                [&](const QString &message) { error = message; });
            [&] { QTRY_VERIFY(fetched || !error.isEmpty()); }();
            return error;
        };
        using Problem = Nextcloud::OnlineAccounts::Problem;
        const QList<QPair<Problem, QLatin1String>> problems{{Problem::Removed, QLatin1String("removed")},
            {Problem::Expired, QLatin1String("expired")}, {Problem::Unavailable, QLatin1String("did not answer")}};
        for (const auto &[problem, text] : problems) {
            online.problem = problem;
            const QString error = fetchError();
            QVERIFY2(error.contains(text), qPrintable(error));
        }

        // The server refuses an authorization revoked there: sign in again in System Settings
        online.problem.reset();
        online.password = QStringLiteral("revoked");
        QString error = fetchError();
        QVERIFY2(error.contains(QLatin1String("Online Accounts")), qPrintable(error));
        // Asked for again after a refusal, so a new sign-in is used at once
        online.password = m_server->appPassword;
        QVERIFY2(fetchError().isEmpty(), qPrintable(error));

        // Without the service the accounts disappear, and those connected here stay
        const QString manual = addAccount();
        online.available = false;
        online.change();
        QVERIFY(!m_hub->hasOnlineAccounts());
        QCOMPARE(m_hub->accounts().size(), 1);
        QCOMPARE(m_hub->accounts().constFirst().id, manual);
        m_hub->setOnlineAccounts(nullptr);
    }

    void onlineAccountsDoNotRepeatConnectedOnes()
    {
        const QString manual = addAccount();
        FakeOnlineAccounts online;
        online.listed = {onlineAccount(7)};
        m_hub->setOnlineAccounts(&online);
        QCOMPARE(m_hub->accounts().size(), 1);
        QCOMPARE(m_hub->accounts().constFirst().id, manual);
        m_hub->setOnlineAccounts(nullptr);
    }

    void shareReviewShowsWhoCanOpenTheLink()
    {
        m_server->addFile(QStringLiteral("/shared.pdf"), "s");
        const QString token = m_server->addLinkShare(QStringLiteral("/shared.pdf"), 1, true);
        const QString id = addAccount();
        const auto shares = m_server->shares;
        const QUrl link(m_server->address() + QStringLiteral("/s/") + token);
        NextcloudShareReviewDialog dialog(m_hub.get(), link);
        QVERIFY(dialog.isBusy());
        QVERIFY(!dialog.canInsert());
        QTRY_VERIFY(!dialog.isBusy());
        QCOMPARE(dialog.accountId(), id);
        QVERIFY2(dialog.detailsText().contains(QLatin1String("password")), qPrintable(dialog.detailsText()));
        QVERIFY(dialog.canInsert());
        QSignalSpy inserted(&dialog, &NextcloudShareReviewDialog::insertRequested);
        dialog.findChild<QPushButton *>(QStringLiteral("nextcloudShareInsert"))->click();
        QCOMPARE(inserted.size(), 1);
        QCOMPARE(inserted.constFirst().constFirst().toUrl(), link);
        // Only read
        QCOMPARE(m_server->shares, shares);
        for (const FakeNextcloudServer::Request &request : std::as_const(m_server->requests))
            QVERIFY2(request.verb == "GET" || request.verb == "PROPFIND", request.verb.constData());
    }

    void hubLooksUpTheLinksOfAMessageAtOnce()
    {
        m_server->addFile(QStringLiteral("/shared.pdf"), "s");
        const QString token
            = m_server->addLinkShare(QStringLiteral("/shared.pdf"), 1, false, QStringLiteral("2099-01-01"));
        const QString id = addAccount();
        const QUrl mine(m_server->address() + QStringLiteral("/s/") + token);
        const QUrl notMine(m_server->address() + QStringLiteral("/s/NotMine12345"));
        const QUrl elsewhere(QStringLiteral("https://elsewhere.example.test/s/AbCdEfGh1234"));
        const int before = m_server->requests.size();
        QList<NextcloudHub::LinkShare> found;
        bool called = false;
        m_hub->lookUpShares({mine, notMine, elsewhere}, this, [&](const QList<NextcloudHub::LinkShare> &shares) {
            found = shares;
            called = true;
        });
        QTRY_VERIFY(called);
        QCOMPARE(found.size(), 3);
        QCOMPARE(found[0].link, mine);
        QCOMPARE(found[0].accountId, id);
        QVERIFY(found[0].knownServer);
        QVERIFY2(found[0].details.join(QLatin1Char(' ')).contains(QLatin1String("2099")),
            qPrintable(found[0].details.join(QLatin1Char(' '))));
        QVERIFY(found[1].accountId.isEmpty());
        QVERIFY(found[1].knownServer);
        QVERIFY(found[1].details.join(QLatin1Char(' ')).contains(QLatin1String("unknown")));
        QVERIFY(found[2].accountId.isEmpty());
        QVERIFY(!found[2].knownServer);
        QVERIFY(found[2].problems.isEmpty());
        // The account's shares are read once, and only read
        int shareRequests = 0;
        for (int i = before; i < m_server->requests.size(); ++i) {
            const FakeNextcloudServer::Request &request = m_server->requests.at(i);
            QVERIFY2(request.verb == "GET" || request.verb == "PROPFIND", request.verb.constData());
            if (request.path.contains(QLatin1String("files_sharing")))
                ++shareRequests;
        }
        QCOMPARE(shareRequests, 1);

        // Without links or accounts on their servers, the answer comes right away
        called = false;
        m_hub->lookUpShares(
            {elsewhere}, this, [&](const QList<NextcloudHub::LinkShare> &shares) { called = shares.size() == 1; });
        QVERIFY(called);

        // Nothing is reported once the context is gone
        called = false;
        {
            QObject context;
            m_hub->lookUpShares({mine}, &context, [&](const QList<NextcloudHub::LinkShare> &) { called = true; });
        }
        QTest::qWait(200);
        QVERIFY(!called);
    }

    void shareReviewWarnsAboutUnknownLinks()
    {
        addAccount();
        // Not one of the user's shares
        NextcloudShareReviewDialog unknown(m_hub.get(), QUrl(m_server->address() + QStringLiteral("/s/NotMine12345")));
        QTRY_VERIFY(!unknown.isBusy());
        QVERIFY(unknown.accountId().isEmpty());
        QVERIFY(unknown.detailsText().contains(QLatin1String("unknown")));
        QVERIFY(unknown.canInsert());

        // The account's sign-in is refused: the link is still shown as unknown, with why
        m_server->appPassword = QStringLiteral("changed");
        NextcloudShareReviewDialog refused(m_hub.get(), QUrl(m_server->address() + QStringLiteral("/s/NotMine12345")));
        QTRY_VERIFY(!refused.isBusy());
        auto *status = refused.findChild<QLabel *>(QStringLiteral("nextcloudShareStatus"));
        QVERIFY(status);
        QVERIFY2(status->text().contains(QLatin1String("refused")), qPrintable(status->text()));

        for (const QString &text :
            {QStringLiteral("https://cloud.example.test/apps/files/"), QStringLiteral("mailto:someone@example.test"),
                QStringLiteral("https://user:secret@cloud.example.test/s/AbCdEfGh1234")}) {
            NextcloudShareReviewDialog notShare(m_hub.get(), QUrl(text));
            QVERIFY(!notShare.isBusy());
            QVERIFY2(!notShare.canInsert(), qPrintable(text));
        }
    }

private:
    Nextcloud::Account onlineAccount(quint32 id) const
    {
        // As the Nextcloud provider of KDE Online Accounts stores it, named after the login
        return Nextcloud::onlineAccount(id, m_server->loginName,
            {{QStringLiteral("server"), m_server->address()},
                {QStringLiteral("dav/storagePath"),
                    QString(QStringLiteral("/remote.php/dav/files/") + m_server->userId)}});
    }

    std::unique_ptr<NextcloudHub> makeHub(FakePasswordPlugin *plugin)
    {
        auto config = KSharedConfig::openConfig(m_directory->filePath(QStringLiteral("rc")), KConfig::SimpleConfig);
        auto hub = std::make_unique<NextcloudHub>(
            config, m_profile.get(), [plugin]() -> Plugins::PasswordPlugin * { return plugin; });
        hub->setConfirm([](QWidget *, const QString &, const QString &) { return false; });
        return hub;
    }

    QString addAccount()
    {
        Nextcloud::Account account;
        account.id = QStringLiteral("cloud-%1").arg(++m_accounts);
        account.server = QUrl(m_server->address());
        account.loginName = m_server->loginName;
        account.userId = m_server->userId;
        account.displayName = m_server->displayName;
        bool done = false;
        QString error;
        m_hub->addAccount(account, m_server->appPassword, this, [&](const QString &text) {
            error = text;
            done = true;
        });
        [&] { QTRY_VERIFY(done); }();
        [&] { QVERIFY2(error.isEmpty(), qPrintable(error)); }();
        return account.id;
    }

    std::unique_ptr<QTemporaryDir> m_directory;
    std::unique_ptr<FakeNextcloudServer> m_server;
    std::unique_ptr<FakePasswordPlugin> m_plugin;
    std::unique_ptr<QSettings> m_profile;
    std::unique_ptr<NextcloudHub> m_hub;
    int m_accounts = 0;
};

QTEST_MAIN(NextcloudDialogsTest)
#include "test_NextcloudDialogs.moc"
