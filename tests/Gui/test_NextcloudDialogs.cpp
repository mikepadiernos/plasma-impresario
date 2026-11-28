/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <memory>

#include <KLocalizedString>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>

#include "../Nextcloud/FakeNextcloudServer.h"
#include "../Nextcloud/FakePasswordPlugin.h"
#include "Gui/NextcloudBrowserDialog.h"
#include "Gui/NextcloudConnectDialog.h"
#include "Gui/NextcloudHub.h"

using Gui::NextcloudBrowserDialog;
using Gui::NextcloudConnectDialog;
using Gui::NextcloudHub;

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
        const int polls = m_server->loginPolls;
        QTest::qWait(50);
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

private:
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
