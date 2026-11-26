/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Gui/MailBoxTreeView.h"
#include "Gui/MessageListWidget.h"
#include "Gui/MsgListView.h"
#include "Gui/SearchFoldersDialog.h"
#include "Gui/Window.h"
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "UiUtils/SearchScope.h"
#include "configure.cmake.h"
#ifdef TROJITA_HAVE_WEBENGINE
#include "Gui/WebEngineMessageProfile.h"
#endif
#include "static_plugins.h"

#include "Plugins/PluginManager.h"

#include <KLocalizedString>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>

namespace {

// Synthetic credentials for a disposable loopback server; never point this test at a real account.
const QString syntheticPassword = QStringLiteral("synthetic-test-only");

void configureAccount(const QString &fileName, int port, const QString &user)
{
    QSettings account(fileName, QSettings::IniFormat);
    account.setValue(Common::SettingsNames::imapMethodKey, Common::SettingsNames::methodSSL);
    account.setValue(Common::SettingsNames::imapHostKey, QStringLiteral("127.0.0.1"));
    account.setValue(Common::SettingsNames::imapPortKey, port);
    account.setValue(Common::SettingsNames::imapUserKey, user);
    account.setValue(Common::SettingsNames::imapStartMode, Common::SettingsNames::netOnline);
    account.setValue(Common::SettingsNames::imapNeedsNetwork, false);
    account.setValue(Common::SettingsNames::imapUseSystemProxy, false);
    account.setValue(Common::SettingsNames::cacheOfflineKey, Common::SettingsNames::cacheOfflineNone);
    account.setValue(Common::SettingsNames::msaMethodKey, Common::SettingsNames::methodSENDMAIL);
    account.sync();
}

QModelIndex findMailbox(const QAbstractItemModel *model, const QString &name)
{
    if (!model || model->rowCount() == 0)
        return {};
    const QModelIndexList matches = model->match(model->index(0, 0), Imap::Mailbox::RoleMailboxName, name, 1, Qt::MatchExactly);
    return matches.isEmpty() ? QModelIndex() : QModelIndex(matches.first());
}

QComboBox *accountSwitcher(Gui::MainWindow &window)
{
    for (auto *combo : window.findChildren<QComboBox *>()) {
        if (combo->accessibleName() == i18n("Account"))
            return combo;
    }
    return nullptr;
}

/** @short "Subject | row label" of each shown message whose envelope is loaded, sorted */
QStringList shownRows(const QAbstractItemModel *model)
{
    QStringList rows;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        const QString subject = index.data(Imap::Mailbox::RoleMessageSubject).toString();
        if (subject.isEmpty())
            return {QStringLiteral("(not loaded)")};
        rows << subject + QStringLiteral(" | ") + index.data(Imap::Mailbox::RoleMessageRowLabel).toString();
    }
    rows.sort();
    return rows;
}

QString describe(const QStringList &rows, Gui::MessageListWidget *widget)
{
    auto *status = widget->findChild<QLabel *>(QStringLiteral("messageListStatus"));
    return rows.join(QStringLiteral("; ")) + QStringLiteral(" | status: ") + (status ? status->text() : QString());
}

}

class LiveImapSearchTest : public QObject
{
    Q_OBJECT
private slots:
    /**
     * Expects a loopback IMAPS server on PLASMA_TEST_IMAPS_PORT with users sharing the synthetic password, each
     * message with the subject "Synthetic NAME": alpha@example.test with invoice-alpha-inbox and lunch in INBOX,
     * invoice-alpha-archive in Archive and invoice-alpha-old in Archive/2024; beta@example.test with invoice-beta in
     * INBOX and invoice-beta-receipt in Receipts. PLASMA_TEST_ALPHA_MAILDIR names alpha's Maildir. See
     * tools/dev-container/live-imap-search.sh.
     */
    void searchesAcrossFoldersAndAccounts()
    {
        using UiUtils::SearchScope::Places;
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
        const QString alphaMaildir = qEnvironmentVariable("PLASMA_TEST_ALPHA_MAILDIR");
        QVERIFY(!alphaMaildir.isEmpty());
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        settings.setValue(Common::SettingsNames::guiShowSystray, false);
        settings.setValue(Common::SettingsNames::passwordPlugin, QStringLiteral("cleartextpassword"));
        settings.setValue(QStringLiteral("accounts/ids"),
                          QStringList{QStringLiteral("account-1"), QStringLiteral("account-2"), QStringLiteral("account-3")});
        settings.setValue(QStringLiteral("accounts/active"), QStringLiteral("account-1"));
        settings.setValue(QStringLiteral("accounts/next"), 4);
        settings.setValue(QStringLiteral("accounts/names/account-1"), QStringLiteral("Alpha"));
        settings.setValue(QStringLiteral("accounts/names/account-2"), QStringLiteral("Beta"));
        settings.setValue(QStringLiteral("accounts/names/account-3"), QStringLiteral("Broken"));
        settings.setValue(QStringLiteral("accountPasswords/account-1/imap"), syntheticPassword);
        settings.setValue(QStringLiteral("accountPasswords/account-2/imap"), syntheticPassword);
        settings.setValue(QStringLiteral("accountPasswords/account-3/imap"), QStringLiteral("incorrect-synthetic-password"));
        settings.sync();
        configureAccount(directory.filePath(QStringLiteral("main-account-1.conf")), port, QStringLiteral("alpha@example.test"));
        configureAccount(directory.filePath(QStringLiteral("main-account-2.conf")), port, QStringLiteral("beta@example.test"));
        configureAccount(directory.filePath(QStringLiteral("main-account-3.conf")), port, QStringLiteral("beta@example.test"));

        QTimer dialogs;
        connect(&dialogs, &QTimer::timeout, this, [&] {
            for (QWidget *widget : QApplication::topLevelWidgets()) {
                if (!widget->isVisible())
                    continue;
                if (auto *message = qobject_cast<QMessageBox *>(widget);
                    message && message->standardButtons() == (QMessageBox::Yes | QMessageBox::No)) {
                    message->button(QMessageBox::Yes)->click();
                } else if (auto *dialog = qobject_cast<QDialog *>(widget);
                           dialog && dialog->findChild<QLineEdit *>(QStringLiteral("passwordLineEdit"))) {
                    dialog->reject();
                }
            }
        });
        dialogs.start(20);

        Gui::MainWindow window(&settings);
        window.resize(1000, 700);
        window.show();
        auto *switcher = accountSwitcher(window);
        auto *tree = window.findChild<Gui::MailBoxTreeView *>();
        auto *searchWidget = window.findChild<Gui::MessageListWidget *>();
        auto *messages = window.findChild<Gui::MsgListView *>();
        QVERIFY(switcher && tree && searchWidget && messages);
        auto *query = searchWidget->findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        auto *scope = searchWidget->findChild<QLabel *>(QStringLiteral("messageListScope"));
        auto *status = searchWidget->findChild<QLabel *>(QStringLiteral("messageListStatus"));
        QVERIFY(query && scope && status);

        QModelIndex inbox;
        QTRY_VERIFY_WITH_TIMEOUT((inbox = findMailbox(tree->model(), QStringLiteral("INBOX"))).isValid(), 20000);
        tree->scrollTo(inbox);
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualRect(inbox).center());
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 2, 10000);
        // The account with a rejected password is known to have failed before searching everywhere
        QTRY_VERIFY_WITH_TIMEOUT(switcher->itemText(2).startsWith(QLatin1String("Broken (")), 20000);

        // The current folder is searched by default, as before
        QCOMPARE(searchWidget->searchPlaces(), Places::CurrentFolder);
        query->setText(QStringLiteral("invoice"));
        emit query->returnPressed();
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 1, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->index(0, 0).data(Imap::Mailbox::RoleMessageSubject).toString(),
                                  QStringLiteral("Synthetic invoice-alpha-inbox"), 10000);

        // Every folder of the shown account, with the folder of each result
        QStringList rows;
        searchWidget->setSearchPlaces(Places::Account);
        QTRY_COMPARE_WITH_TIMEOUT(scope->text(), QStringLiteral("Search Results"), 5000);
        QTRY_VERIFY2_WITH_TIMEOUT((rows = shownRows(messages->model())).size() == 3 && status->text().contains(QLatin1String("3 messages found")),
                                  qPrintable(describe(rows, searchWidget)), 30000);
        QCOMPARE(rows, (QStringList{QStringLiteral("Synthetic invoice-alpha-archive | Alpha \u00B7 Archive"),
                                    QStringLiteral("Synthetic invoice-alpha-inbox | Alpha \u00B7 Inbox"),
                                    QStringLiteral("Synthetic invoice-alpha-old | Alpha \u00B7 Archive/2024")}));
        QVERIFY(!status->text().contains(QLatin1String("could not be searched")));

        // All accounts; the one which cannot log in is reported
        searchWidget->setSearchPlaces(Places::AllAccounts);
        QTRY_VERIFY2_WITH_TIMEOUT((rows = shownRows(messages->model())).size() == 5 && status->text().contains(QLatin1String("5 messages found")),
                                  qPrintable(describe(rows, searchWidget)), 30000);
        QCOMPARE(rows, (QStringList{QStringLiteral("Synthetic invoice-alpha-archive | Alpha \u00B7 Archive"),
                                    QStringLiteral("Synthetic invoice-alpha-inbox | Alpha \u00B7 Inbox"),
                                    QStringLiteral("Synthetic invoice-alpha-old | Alpha \u00B7 Archive/2024"),
                                    QStringLiteral("Synthetic invoice-beta | Beta \u00B7 Inbox"),
                                    QStringLiteral("Synthetic invoice-beta-receipt | Beta \u00B7 Receipts")}));
        QVERIFY2(status->text().contains(QLatin1String("1 place could not be searched")), qPrintable(status->text()));
        QVERIFY2(scope->toolTip().contains(QLatin1String("Broken")), qPrintable(scope->toolTip()));

        // Folders of both accounts, chosen in the dialog
        searchWidget->findChild<QAction *>(QStringLiteral("searchPlaces-folders"))->trigger();
        Gui::SearchFoldersDialog *chooser = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((chooser = window.findChild<Gui::SearchFoldersDialog *>()) && chooser->isVisible(), 5000);
        QTreeWidget *folders = chooser->tree();
        QCOMPARE(folders->topLevelItemCount(), 3);
        const auto check = [folders](int account, const QString &path) {
            QTreeWidgetItem *parent = folders->topLevelItem(account);
            for (int child = 0; child < parent->childCount(); ++child) {
                if (parent->child(child)->text(0) == path) {
                    parent->child(child)->setCheckState(0, Qt::Checked);
                    return true;
                }
            }
            return false;
        };
        QVERIFY(check(0, QStringLiteral("Archive / 2024")));
        QVERIFY(check(1, QStringLiteral("Receipts")));
        chooser->accept();
        QCOMPARE(searchWidget->searchPlaces(), Places::SelectedFolders);
        QCOMPARE(searchWidget->selectedFolders(),
                 (QStringList{QStringLiteral("account-1/Archive/2024"), QStringLiteral("account-2/Receipts")}));
        QTRY_VERIFY2_WITH_TIMEOUT((rows = shownRows(messages->model())).size() == 2 && status->text().contains(QLatin1String("2 messages found")),
                                  qPrintable(describe(rows, searchWidget)), 30000);
        QCOMPARE(rows, (QStringList{QStringLiteral("Synthetic invoice-alpha-old | Alpha \u00B7 Archive/2024"),
                                    QStringLiteral("Synthetic invoice-beta-receipt | Beta \u00B7 Receipts")}));

        // A missing folder is reported, and the others are still searched
        searchWidget->setSelectedFolders({QStringLiteral("account-1/Archive"), QStringLiteral("account-1/Gone")});
        QTRY_VERIFY2_WITH_TIMEOUT((rows = shownRows(messages->model())).size() == 1 && status->text().contains(QLatin1String("1 message found")),
                                  qPrintable(describe(rows, searchWidget)), 30000);
        QVERIFY2(status->text().contains(QLatin1String("1 place could not be searched")), qPrintable(status->text()));

        // Leaving the results shows the folder from before, which is selected on the server again
        searchWidget->clearSearch();
        QTRY_VERIFY_WITH_TIMEOUT(scope->text() != QStringLiteral("Search Results"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 2, 10000);
        QFile late(alphaMaildir + QStringLiteral("/new/alpha-late.synthetic"));
        QVERIFY(late.open(QIODevice::WriteOnly));
        late.write("From: Synthetic Sender <sender@example.test>\r\nTo: alpha@example.test\r\n"
                   "Subject: Synthetic alpha-late\r\nDate: Tue, 24 Nov 2026 13:00:00 +0000\r\n"
                   "Message-ID: <alpha-late@example.test>\r\n\r\nSynthetic body.\r\n");
        late.close();
        late.setPermissions(late.permissions() | QFileDevice::ReadOther | QFileDevice::ReadGroup);
        if (auto *checkMail = window.findChild<QAction *>(QStringLiteral("action_check_mail")))
            checkMail->trigger();
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 3, 20000);

        // In the Unified Inbox, the shown list is the INBOX of each account
        searchWidget->setSearchPlaces(Places::CurrentFolder);
        auto *unified = window.findChild<QAction *>(QStringLiteral("action_unified_inbox"));
        QVERIFY(unified);
        unified->trigger();
        QVERIFY(unified->isChecked());
        QVERIFY(query->isEnabled() || messages->model()->rowCount() == 0);
        QTRY_VERIFY_WITH_TIMEOUT(query->isEnabled(), 20000);
        QCOMPARE(searchWidget->findChild<QAction *>(QStringLiteral("searchPlaces-current"))->text(), QStringLiteral("Unified Inbox"));
        query->setText(QStringLiteral("invoice"));
        emit query->returnPressed();
        QTRY_VERIFY2_WITH_TIMEOUT((rows = shownRows(messages->model())).size() == 2 && status->text().contains(QLatin1String("2 messages found")),
                                  qPrintable(describe(rows, searchWidget)), 30000);
        QCOMPARE(rows, (QStringList{QStringLiteral("Synthetic invoice-alpha-inbox | Alpha \u00B7 Inbox"),
                                    QStringLiteral("Synthetic invoice-beta | Beta \u00B7 Inbox")}));
        query->clear();
        emit query->returnPressed();
        QTRY_VERIFY2_WITH_TIMEOUT(shownRows(messages->model()).size() >= 4 && !status->text().contains(QLatin1String("found")),
                                  qPrintable(describe(shownRows(messages->model()), searchWidget)), 20000);
        QCOMPARE(scope->text(), QStringLiteral("Unified Inbox"));

        // Leaving the Unified Inbox clears its search and shows the folder again
        query->setText(QStringLiteral("invoice"));
        emit query->returnPressed();
        QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QLatin1String("found")), 30000);
        unified->trigger();
        QVERIFY(!unified->isChecked());
        QVERIFY(query->text().isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 3, 10000);
        QCOMPARE(searchWidget->findChild<QAction *>(QStringLiteral("searchPlaces-current"))->text(), QStringLiteral("Current Folder"));
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir home;
    if (!home.isValid())
        return 1;
    const QString homePath = home.path();
    qputenv("HOME", homePath.toUtf8());
    qputenv("XDG_CONFIG_HOME", (homePath + QStringLiteral("/config")).toUtf8());
    qputenv("XDG_DATA_HOME", (homePath + QStringLiteral("/data")).toUtf8());
    qputenv("XDG_CACHE_HOME", (homePath + QStringLiteral("/cache")).toUtf8());
    qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent/plasma-impresario-live-test");
    qunsetenv("TROJITA_PROFILE");
    QStandardPaths::setTestModeEnabled(true);
#ifdef TROJITA_HAVE_WEBENGINE
    Gui::WebEngineMessageProfile::registerScheme();
#endif
    Common::registerMetaTypes();
    QApplication app(argc, argv);
    for (auto location : {QStandardPaths::GenericConfigLocation, QStandardPaths::AppDataLocation, QStandardPaths::CacheLocation}) {
        if (!QStandardPaths::writableLocation(location).startsWith(homePath + QLatin1Char('/')))
            return 1;
    }
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(license);
    KLocalizedString::setApplicationDomain("plasma-impresario");
    if (!Imap::Mailbox::SystemNetworkWatcher::init())
        return 1;
    LiveImapSearchTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_LiveImapSearch.moc"
