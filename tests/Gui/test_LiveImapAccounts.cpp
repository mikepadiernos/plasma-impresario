/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Composer/AccountIdentities.h"
#include "Composer/SenderIdentitiesModel.h"
#include "Gui/AccountSidebar.h"
#include "Gui/ComposeWidget.h"
#include "Gui/MailBoxTreeView.h"
#include "Gui/MessageDetailsPanel.h"
#include "Gui/MessageListWidget.h"
#include "Gui/MsgListView.h"
#include "Gui/Window.h"
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "Imap/Model/Utils.h"
#include "configure.cmake.h"
#ifdef TROJITA_HAVE_WEBENGINE
#include "Gui/WebEngineMessageProfile.h"
#endif
#include "static_plugins.h"

#include "Plugins/PluginManager.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QComboBox>
#include <QDialog>
#include <QInputDialog>
#include <QLabel>
#include <QPointer>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>

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

/** @short Whether any settings file in @arg directory contains @arg text */
bool settingsContain(const QTemporaryDir &directory, const QString &text)
{
    const auto files = QDir(directory.path()).entryInfoList(QDir::Files);
    for (const auto &file : files) {
        QFile settingsFile(file.filePath());
        if (settingsFile.open(QIODevice::ReadOnly) && settingsFile.readAll().contains(text.toUtf8()))
            return true;
    }
    return false;
}

QComboBox *accountSwitcher(Gui::MainWindow &window)
{
    for (auto *combo : window.findChildren<QComboBox *>()) {
        if (combo->accessibleName() == i18n("Account"))
            return combo;
    }
    return nullptr;
}

void openMailbox(Gui::MailBoxTreeView *tree, const QModelIndex &mailbox)
{
    tree->scrollTo(mailbox);
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualRect(mailbox).center());
}

/** @short Click the message with the highest UID in the message list and return its UID */
uint clickNewestMessage(Gui::MsgListView *messages)
{
    QModelIndex newest;
    for (int row = 0; row < messages->model()->rowCount(); ++row) {
        const QModelIndex index = messages->model()->index(row, 0);
        if (!newest.isValid() || index.data(Imap::Mailbox::RoleMessageUid).toUInt() > newest.data(Imap::Mailbox::RoleMessageUid).toUInt())
            newest = index;
    }
    if (!newest.isValid())
        return 0;
    messages->scrollTo(newest);
    QTest::mouseClick(messages->viewport(), Qt::LeftButton, Qt::NoModifier, messages->visualRect(newest).center());
    return newest.data(Imap::Mailbox::RoleMessageUid).toUInt();
}

KConfigGroup savedSelection(const QString &accountId)
{
    return KConfigGroup(KSharedConfig::openStateConfig(), QStringLiteral("MailboxSelection")).group(accountId);
}

QString currentMailbox(Gui::MainWindow &window)
{
    auto *list = qobject_cast<Imap::Mailbox::MsgListModel *>(window.imapAccess()->msgListModel());
    return list ? list->currentMailbox().data(Imap::Mailbox::RoleMailboxName).toString() : QString();
}

}

class LiveImapAccountsTest : public QObject
{
    Q_OBJECT
private slots:
    /**
     * Expects a loopback IMAPS server on PLASMA_TEST_IMAPS_PORT with two users sharing the synthetic password:
     * work@example.test with exactly two unread messages in INBOX and no other folders, and home@example.test
     * with exactly one message in INBOX and a Projects folder. See tools/dev-container/live-imap-accounts.sh.
     * When PLASMA_TEST_HOME_MAILDIR names home's Maildir, a message is delivered there to check for new mail.
     */
    void accountsConnectAndSwitchIndependently()
    {
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        settings.setValue(Common::SettingsNames::guiShowSystray, false);
        settings.setValue(Common::SettingsNames::passwordPlugin, QStringLiteral("cleartextpassword"));
        settings.setValue(QStringLiteral("accounts/ids"),
                          QStringList{QStringLiteral("account-1"), QStringLiteral("account-2"), QStringLiteral("account-3")});
        settings.setValue(QStringLiteral("accounts/active"), QStringLiteral("account-1"));
        settings.setValue(QStringLiteral("accounts/next"), 4);
        settings.setValue(QStringLiteral("accounts/names/account-1"), QStringLiteral("Work"));
        settings.setValue(QStringLiteral("accounts/names/account-2"), QStringLiteral("Home"));
        settings.setValue(QStringLiteral("accounts/names/account-3"), QStringLiteral("Broken"));
        settings.setValue(QStringLiteral("accountPasswords/account-1/imap"), syntheticPassword);
        settings.setValue(QStringLiteral("accountPasswords/account-2/imap"), syntheticPassword);
        settings.setValue(QStringLiteral("accountPasswords/account-3/imap"), QStringLiteral("incorrect-synthetic-password"));
        settings.sync();
        configureAccount(directory.filePath(QStringLiteral("main-account-1.conf")), port, QStringLiteral("work@example.test"));
        configureAccount(directory.filePath(QStringLiteral("main-account-2.conf")), port, QStringLiteral("home@example.test"));
        configureAccount(directory.filePath(QStringLiteral("main-account-3.conf")), port, QStringLiteral("home@example.test"));

        int passwordPrompts = 0;
        int certificatePrompts = 0;
        QTimer dialogs;
        connect(&dialogs, &QTimer::timeout, this, [&] {
            for (QWidget *widget : QApplication::topLevelWidgets()) {
                if (!widget->isVisible())
                    continue;
                if (auto *message = qobject_cast<QMessageBox *>(widget);
                    message && message->standardButtons() == (QMessageBox::Yes | QMessageBox::No)) {
                    ++certificatePrompts;
                    message->button(QMessageBox::Yes)->click();
                } else if (auto *dialog = qobject_cast<QDialog *>(widget);
                           dialog && dialog->findChild<QLineEdit *>(QStringLiteral("passwordLineEdit"))) {
                    ++passwordPrompts;
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
        auto *messages = window.findChild<Gui::MsgListView *>(QStringLiteral("messageListView"));
        QVERIFY(switcher);
        QVERIFY(tree);
        QVERIFY(searchWidget);
        QVERIFY(messages);
        QCOMPARE(switcher->count(), 3);
        auto *workModel = window.imapModel();
        QSignalSpy workErrors(workModel, &Imap::Mailbox::Model::imapError);
        auto *savedMenu = searchWidget->findChild<QMenu *>(QStringLiteral("savedSearches"));
        QVERIFY(savedMenu);
        auto savedNames = [savedMenu]() {
            emit savedMenu->aboutToShow();
            QStringList names;
            for (auto *action : savedMenu->actions())
                names << action->text();
            return names;
        };

        // The active account connects and shows its own folders and counts.
        QModelIndex inbox;
        QTRY_VERIFY_WITH_TIMEOUT((inbox = findMailbox(tree->model(), QStringLiteral("INBOX"))).isValid(), 20000);
        QVERIFY(!findMailbox(tree->model(), QStringLiteral("Projects")).isValid());
        openMailbox(tree, inbox);
        QTRY_COMPARE_WITH_TIMEOUT(inbox.data(Imap::Mailbox::RoleTotalMessageCount).toInt(), 2, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(inbox.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 2, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 2, 10000);

        // The account sidebar shows each account's unread INBOX count, the background account's too
        auto *sidebar = window.findChild<Gui::AccountSidebar *>();
        QVERIFY(sidebar);
        QTRY_COMPARE_WITH_TIMEOUT(sidebar->unreadCount(QStringLiteral("account-1")), 2, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(sidebar->unreadCount(QStringLiteral("account-2")), 1, 20000);
        QCOMPARE(sidebar->headers().at(1)->text(), QStringLiteral("Home (1)"));
        // The task manager entry shows the unread messages of both inboxes
        QTRY_COMPARE_WITH_TIMEOUT(window.unreadCountOnTaskManager(), 3, 10000);

        // Each account saves its own mailbox and message selection.
        QTRY_VERIFY_WITH_TIMEOUT(messages->model()->index(1, 0).data(Imap::Mailbox::RoleMessageUid).toUInt() > 0, 10000);
        const uint workUid = clickNewestMessage(messages);
        QVERIFY(workUid > 1);
        QCOMPARE(savedSelection(QStringLiteral("account-1")).readEntry("Mailbox", QString()), QStringLiteral("INBOX"));
        QCOMPARE(savedSelection(QStringLiteral("account-1")).readEntry("MessageUid", 0u), workUid);
        const uint workUidValidity = savedSelection(QStringLiteral("account-1")).readEntry("MessageUidValidity", 0u);
        QVERIFY(workUidValidity);

        // A background account with a rejected stored password is flagged without prompting.
        QTRY_VERIFY2_WITH_TIMEOUT(switcher->itemText(2).startsWith(QLatin1String("Broken (")),
                                  qPrintable(QStringLiteral("%1 | %2 | %3 prompts: %4")
                                                 .arg(switcher->itemText(1), switcher->itemText(2),
                                                      switcher->itemData(2, Qt::ToolTipRole).toString())
                                                 .arg(certificatePrompts)),
                                  20000);
        QVERIFY(!switcher->itemData(2, Qt::ToolTipRole).toString().isEmpty());
        QCOMPARE(switcher->itemText(1), QStringLiteral("Home"));

        auto *query = searchWidget->findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        QVERIFY(query);
        query->setText(QStringLiteral("Synthetic"));
        QTimer::singleShot(0, []() {
            if (auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget())) {
                dialog->setTextValue(QStringLiteral("Work search"));
                dialog->accept();
            }
        });
        searchWidget->findChild<QAction *>(QStringLiteral("saveSearch"))->trigger();
        QCOMPARE(savedNames(), QStringList{QStringLiteral("Work search")});
        query->clear();

        // Switching uses the already connected background account and its own state.
        switcher->setCurrentIndex(1);
        emit switcher->activated(1);
        QVERIFY(window.imapModel() != workModel);
        QCOMPARE(window.imapAccess()->credentialAccountId(), QStringLiteral("account-2"));
        QSignalSpy homeErrors(window.imapModel(), &Imap::Mailbox::Model::imapError);
        QModelIndex homeInbox;
        QTRY_VERIFY_WITH_TIMEOUT((homeInbox = findMailbox(tree->model(), QStringLiteral("INBOX"))).isValid(), 20000);
        QTRY_VERIFY_WITH_TIMEOUT(findMailbox(tree->model(), QStringLiteral("Projects")).isValid(), 10000);
        openMailbox(tree, homeInbox);
        QTRY_COMPARE_WITH_TIMEOUT(homeInbox.data(Imap::Mailbox::RoleTotalMessageCount).toInt(), 1, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 1, 10000);
        QVERIFY(savedNames().isEmpty());

        // The second account's selection is saved separately and leaves the first account's untouched.
        QTRY_VERIFY_WITH_TIMEOUT(messages->model()->index(0, 0).data(Imap::Mailbox::RoleMessageUid).toUInt() > 0, 10000);
        const uint homeUid = clickNewestMessage(messages);
        QVERIFY(homeUid);
        QVERIFY(homeUid != workUid);
        QCOMPARE(savedSelection(QStringLiteral("account-2")).readEntry("MessageUid", 0u), homeUid);
        openMailbox(tree, findMailbox(tree->model(), QStringLiteral("Projects")));
        QTRY_COMPARE_WITH_TIMEOUT(currentMailbox(window), QStringLiteral("Projects"), 10000);
        QCOMPARE(savedSelection(QStringLiteral("account-2")).readEntry("Mailbox", QString()), QStringLiteral("Projects"));
        QCOMPARE(savedSelection(QStringLiteral("account-2")).readEntry("MessageMailbox", QString()), QStringLiteral("INBOX"));
        QCOMPARE(savedSelection(QStringLiteral("account-2")).readEntry("MessageUid", 0u), homeUid);
        QCOMPARE(savedSelection(QStringLiteral("account-1")).readEntry("Mailbox", QString()), QStringLiteral("INBOX"));
        QCOMPARE(savedSelection(QStringLiteral("account-1")).readEntry("MessageUid", 0u), workUid);
        QCOMPARE(savedSelection(QStringLiteral("account-1")).readEntry("MessageUidValidity", 0u), workUidValidity);

        // Switching back keeps the first account's connection, counts and saved searches.
        switcher->setCurrentIndex(0);
        emit switcher->activated(0);
        QCOMPARE(window.imapModel(), workModel);
        QTRY_VERIFY_WITH_TIMEOUT((inbox = findMailbox(tree->model(), QStringLiteral("INBOX"))).isValid(), 10000);
        QVERIFY(!findMailbox(tree->model(), QStringLiteral("Projects")).isValid());
        QTRY_COMPARE_WITH_TIMEOUT(inbox.data(Imap::Mailbox::RoleTotalMessageCount).toInt(), 2, 10000);
        QCOMPARE(savedNames(), QStringList{QStringLiteral("Work search")});
        QVERIFY(switcher->itemText(2).startsWith(QLatin1String("Broken (")));

        // Switching restores each account's own mailbox and message.
        QTRY_COMPARE_WITH_TIMEOUT(currentMailbox(window), QStringLiteral("INBOX"), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(tree->currentIndex().data(Imap::Mailbox::RoleMailboxName).toString(), QStringLiteral("INBOX"), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(messages->currentIndex().data(Imap::Mailbox::RoleMessageUid).toUInt(), workUid, 10000);
        switcher->setCurrentIndex(1);
        emit switcher->activated(1);
        QTRY_COMPARE_WITH_TIMEOUT(currentMailbox(window), QStringLiteral("Projects"), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(tree->currentIndex().data(Imap::Mailbox::RoleMailboxName).toString(), QStringLiteral("Projects"), 10000);
        QCOMPARE(messages->model()->rowCount(), 0);
        switcher->setCurrentIndex(0);
        emit switcher->activated(0);
        QCOMPARE(window.imapModel(), workModel);
        QTRY_COMPARE_WITH_TIMEOUT(messages->currentIndex().data(Imap::Mailbox::RoleMessageUid).toUInt(), workUid, 10000);
        QCOMPARE(savedSelection(QStringLiteral("account-2")).readEntry("Mailbox", QString()), QStringLiteral("Projects"));
        QCOMPARE(savedSelection(QStringLiteral("account-1")).readEntry("MessageUid", 0u), workUid);

        // The server narrows the quick search to a date range; the synthetic messages are dated 24 Nov 2026
        using UiUtils::SearchScope::Dates;
        searchWidget->setDateRange({Dates::Custom, QDate(2026, 11, 24), QDate(2026, 11, 24)});
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 2, 10000);
        searchWidget->setDateRange({Dates::Custom, QDate(2026, 11, 25), QDate()});
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 0, 10000);
        QVERIFY(query->isEnabled());
        query->setText(QStringLiteral("work-second"));
        searchWidget->setDateRange({Dates::Custom, QDate(), QDate(2026, 11, 24)});
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 1, 10000);
        query->clear();
        searchWidget->setDateRange({});
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 2, 10000);

        QCOMPARE(passwordPrompts, 0);
        qInfo("Certificate prompts accepted: %d", certificatePrompts);
        QVERIFY2(workErrors.isEmpty(), qPrintable(workErrors.isEmpty() ? QString() : workErrors.first().first().toString()));
        QVERIFY2(homeErrors.isEmpty(), qPrintable(homeErrors.isEmpty() ? QString() : homeErrors.first().first().toString()));

        // Check for New Mail asks the background account again, which has a folder other than INBOX open
        const QString homeMaildir = qEnvironmentVariable("PLASMA_TEST_HOME_MAILDIR");
        if (!homeMaildir.isEmpty()) {
            QTRY_VERIFY_WITH_TIMEOUT(sidebar->unreadCount(QStringLiteral("account-2")) >= 0, 10000);
            const int homeUnread = sidebar->unreadCount(QStringLiteral("account-2"));
            QFile late(homeMaildir + QStringLiteral("/new/home-late.synthetic"));
            QVERIFY(late.open(QIODevice::WriteOnly));
            late.write("From: Synthetic Sender <sender@example.test>\r\nTo: home@example.test\r\n"
                       "Subject: Synthetic home-late\r\nDate: Tue, 24 Nov 2026 13:00:00 +0000\r\n"
                       "Message-ID: <home-late@example.test>\r\n\r\nSynthetic body.\r\n");
            late.close();
            late.setPermissions(late.permissions() | QFileDevice::ReadOther | QFileDevice::ReadGroup);
            auto *checkMail = window.findChild<QAction *>(QStringLiteral("action_check_mail"));
            QVERIFY(checkMail);
            checkMail->trigger();
            QTRY_COMPARE_WITH_TIMEOUT(sidebar->unreadCount(QStringLiteral("account-2")), homeUnread + 1, 10000);
        }

        // Removing the connected background account deletes its settings and stored password; the active one stays.
        QVERIFY(QMetaObject::invokeMethod(&window, "removeAccountNow", Q_ARG(QString, QStringLiteral("account-2"))));
        QTRY_VERIFY(!settings.contains(QStringLiteral("accountPasswords/account-2/imap")));
        QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("main-account-2.conf"))));
        QCOMPARE(switcher->count(), 2);
        QCOMPARE(window.imapModel(), workModel);
        QVERIFY(settings.contains(QStringLiteral("accountPasswords/account-1/imap")));

        // Removing the active account switches to the remaining one, which keeps its own stored password. The server
        // rejects that password, so the now active account may ask for a new one once; when depends on timing.
        QVERIFY(QMetaObject::invokeMethod(&window, "removeAccountNow", Q_ARG(QString, QStringLiteral("account-1"))));
        QTRY_VERIFY(!settings.contains(QStringLiteral("accountPasswords/account-1/imap")));
        QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("main-account-1.conf"))));
        QCOMPARE(settings.value(QStringLiteral("accounts/ids")).toStringList(), QStringList{QStringLiteral("account-3")});
        QCOMPARE(window.imapAccess()->credentialAccountId(), QStringLiteral("account-3"));
        QCOMPARE(switcher->count(), 1);
        QCOMPARE(settings.value(QStringLiteral("accountPasswords/account-3/imap")).toString(),
                 QStringLiteral("incorrect-synthetic-password"));
        QVERIFY2(passwordPrompts <= 1, qPrintable(QString::number(passwordPrompts)));
    }

    /**
     * A new profile stores passwords through QtKeychain. The container has no Secret Service, so the stored password
     * is missing: the app must prompt, sign in with the typed password and never write it to the settings files.
     * Expects the same server as accountsConnectAndSwitchIndependently.
     */
    void newProfilesKeepPasswordsOutOfSettings()
    {
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        settings.setValue(Common::SettingsNames::guiShowSystray, false);
        settings.setValue(QStringLiteral("accounts/ids"), QStringList{QStringLiteral("account-1")});
        settings.setValue(QStringLiteral("accounts/active"), QStringLiteral("account-1"));
        settings.setValue(QStringLiteral("accounts/next"), 2);
        settings.setValue(QStringLiteral("accounts/names/account-1"), QStringLiteral("Work"));
        settings.sync();
        configureAccount(directory.filePath(QStringLiteral("main-account-1.conf")), port, QStringLiteral("work@example.test"));

        int passwordPrompts = 0;
        QTimer dialogs;
        connect(&dialogs, &QTimer::timeout, this, [&] {
            for (QWidget *widget : QApplication::topLevelWidgets()) {
                if (!widget->isVisible())
                    continue;
                if (auto *message = qobject_cast<QMessageBox *>(widget);
                    message && message->standardButtons() == (QMessageBox::Yes | QMessageBox::No)) {
                    message->button(QMessageBox::Yes)->click();
                } else if (auto *dialog = qobject_cast<QDialog *>(widget)) {
                    if (auto *input = dialog->findChild<QLineEdit *>(QStringLiteral("passwordLineEdit"))) {
                        ++passwordPrompts;
                        input->setText(syntheticPassword);
                        dialog->accept();
                    }
                }
            }
        });
        dialogs.start(20);

        Gui::MainWindow window(&settings);
        QCOMPARE(window.pluginManager()->passwordPlugin(), QStringLiteral("qtkeychainpassword"));
        window.resize(1000, 700);
        window.show();
        auto *tree = window.findChild<Gui::MailBoxTreeView *>();
        QVERIFY(tree);
        QModelIndex inbox;
        QTRY_VERIFY_WITH_TIMEOUT((inbox = findMailbox(tree->model(), QStringLiteral("INBOX"))).isValid(), 20000);
        openMailbox(tree, inbox);
        QTRY_COMPARE_WITH_TIMEOUT(inbox.data(Imap::Mailbox::RoleTotalMessageCount).toInt(), 2, 10000);
        QVERIFY(passwordPrompts >= 1);
        qInfo("QtKeychain plugin loaded: %s; password prompts: %d", window.pluginManager()->password() ? "yes" : "no",
              passwordPrompts);
        settings.sync();
        window.accountSettings()->sync();
        QVERIFY(!settings.contains(QStringLiteral("accountPasswords")));
        QVERIFY(!settingsContain(directory, syntheticPassword));
    }

    /**
     * A reply from the Unified Inbox is sent from the account which received the message, unless the composer switches
     * to another one; either way the original message is marked as answered on its own account.
     * Expects the same server as accountsConnectAndSwitchIndependently.
     */
    void repliesSendFromTheMessagesAccount()
    {
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        settings.setValue(Common::SettingsNames::guiShowSystray, false);
        settings.setValue(Common::SettingsNames::passwordPlugin, QStringLiteral("cleartextpassword"));
        settings.setValue(QStringLiteral("accounts/ids"), QStringList{QStringLiteral("account-1"), QStringLiteral("account-2")});
        settings.setValue(QStringLiteral("accounts/active"), QStringLiteral("account-1"));
        settings.setValue(QStringLiteral("accounts/next"), 3);
        settings.setValue(QStringLiteral("accounts/names/account-1"), QStringLiteral("Work"));
        settings.setValue(QStringLiteral("accounts/names/account-2"), QStringLiteral("Home"));
        settings.setValue(QStringLiteral("accountPasswords/account-1/imap"), syntheticPassword);
        settings.setValue(QStringLiteral("accountPasswords/account-2/imap"), syntheticPassword);
        settings.sync();
        const QList<std::pair<QString, QString>> accounts{{QStringLiteral("account-1"), QStringLiteral("work")},
                                                          {QStringLiteral("account-2"), QStringLiteral("home")}};
        for (const auto &[accountId, user] : accounts) {
            const QString fileName = directory.filePath(QStringLiteral("main-%1.conf").arg(accountId));
            configureAccount(fileName, port, user + QStringLiteral("@example.test"));
            // Each account sends through a script which keeps what it sent
            const QString script = directory.filePath(user + QStringLiteral(".sh"));
            QFile file(script);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("cat > '" + directory.filePath(user + QStringLiteral(".eml")).toUtf8() + "'\n");
            file.close();
            QSettings account(fileName, QSettings::IniFormat);
            account.setValue(Common::SettingsNames::sendmailKey, QString(QStringLiteral("/bin/sh ") + script));
            account.setValue(Common::SettingsNames::composerSaveToImapKey, false);
            account.sync();
        }

        QStringList errors;
        QTimer dialogs;
        connect(&dialogs, &QTimer::timeout, this, [&] {
            for (QWidget *widget : QApplication::topLevelWidgets()) {
                if (!widget->isVisible())
                    continue;
                if (auto *message = qobject_cast<QMessageBox *>(widget);
                    message && message->standardButtons() == (QMessageBox::Yes | QMessageBox::No)) {
                    message->button(QMessageBox::Yes)->click();
                } else if (message) {
                    errors << message->text();
                    message->reject();
                } else if (auto *dialog = qobject_cast<QDialog *>(widget);
                           dialog && dialog->findChild<QLineEdit *>(QStringLiteral("passwordLineEdit"))) {
                    errors << QStringLiteral("password prompt");
                    dialog->reject();
                }
            }
        });
        dialogs.start(20);

        Gui::MainWindow window(&settings);
        window.resize(1400, 900);
        window.show();
        for (const auto &[accountId, user] : accounts) {
            QVERIFY(window.withAccount(accountId, [&, user = user]() {
                Composer::AccountIdentities::setSource(*window.accountSettings(), Composer::AccountIdentities::Source::Own);
                window.senderIdentitiesModel()->appendIdentity(Composer::ItemSenderIdentity(
                    QStringLiteral("Pat"), user + QStringLiteral("@example.test"), QString(), QString()));
            }));
        }
        auto *messages = window.findChild<Gui::MsgListView *>(QStringLiteral("messageListView"));
        auto *details = window.findChild<Gui::MessageDetailsPanel *>();
        QVERIFY(messages && details);
        if (auto *panel = window.findChild<QAction *>(QStringLiteral("embeddedPanelMessageDetails")); panel && !panel->isChecked())
            panel->trigger();
        auto *unified = window.findChild<QAction *>(QStringLiteral("action_unified_inbox"));
        QVERIFY(unified);
        unified->trigger();
        QVERIFY(unified->isChecked());

        // Open a message of the background account in the Unified Inbox
        QModelIndex home;
        QTRY_VERIFY_WITH_TIMEOUT(messages->model()->rowCount() >= 3, 20000);
        const auto findHome = [&]() {
            const auto matches = messages->model()->match(messages->model()->index(0, 0), Imap::Mailbox::RoleMessageAccountId,
                                                          QStringLiteral("account-2"), 1, Qt::MatchExactly | Qt::MatchRecursive);
            home = matches.isEmpty() ? QModelIndex() : matches.first();
            return home.isValid() && home.data(Imap::Mailbox::RoleMessageUid).toUInt() > 0;
        };
        QTRY_VERIFY_WITH_TIMEOUT(findHome(), 20000);
        messages->scrollTo(home);
        QTest::mouseClick(messages->viewport(), Qt::LeftButton, Qt::NoModifier,
                          messages->visualRect(home).intersected(messages->viewport()->rect()).center());
        QTRY_COMPARE_WITH_TIMEOUT(details->fieldText(QStringLiteral("Account")), QStringLiteral("Home"), 10000);
        const QPersistentModelIndex original = Imap::deproxifiedIndex(details->message());
        QVERIFY(original.isValid());
        QVERIFY(!original.data(Imap::Mailbox::RoleMessageIsMarkedReplied).toBool());
        QLabel *envelopeAccount = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((envelopeAccount = window.findChild<QLabel *>(QStringLiteral("envelopeAccount"))) &&
                                     envelopeAccount->text().contains(QLatin1String("Home")),
                                 10000);

        auto *replyAction = window.findChild<QAction *>(QStringLiteral("action_reply_private"));
        QVERIFY(replyAction);
        QPointer<Gui::ComposeWidget> composer;
        const auto findComposer = [&]() {
            for (QWidget *widget : QApplication::topLevelWidgets()) {
                if (auto *candidate = qobject_cast<Gui::ComposeWidget *>(widget); candidate && candidate->isVisible())
                    composer = candidate;
            }
            return !composer.isNull();
        };
        auto sentFile = [&directory](const QString &user) {
            QFile file(directory.filePath(user + QStringLiteral(".eml")));
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        };

        // The reply comes from the message's account and its identity
        QTRY_VERIFY_WITH_TIMEOUT(replyAction->isEnabled(), 10000);
        replyAction->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(findComposer(), 15000);
        QCOMPARE(composer->accountId(), QStringLiteral("account-2"));
        auto *sender = composer->findChild<QComboBox *>(QStringLiteral("sender"));
        QVERIFY2(sender->currentText().contains(QLatin1String("home@example.test")), qPrintable(sender->currentText()));
        QVERIFY(composer->findChild<QLabel *>(QStringLiteral("composerAccountWarning"))->isHidden());
        QVERIFY(QMetaObject::invokeMethod(composer, "send"));
        QTRY_VERIFY2_WITH_TIMEOUT(!composer, qPrintable(errors.join(QLatin1Char('\n'))), 20000);
        QVERIFY2(errors.isEmpty(), qPrintable(errors.join(QLatin1Char('\n'))));
        QByteArray sent = sentFile(QStringLiteral("home"));
        QVERIFY2(sent.contains("From: Pat <home@example.test>"), sent.constData());
        QVERIFY2(sent.contains("In-Reply-To: <home-first@example.test>") || sent.contains("In-Reply-To: <home-late@example.test>"),
                 sent.constData());
        QVERIFY(sentFile(QStringLiteral("work")).isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(original.data(Imap::Mailbox::RoleMessageIsMarkedReplied).toBool(), 10000);

        // Switched to another account, the reply is sent from that one and still answers the original message
        replyAction->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(findComposer(), 15000);
        QCOMPARE(composer->accountId(), QStringLiteral("account-2"));
        auto *account = composer->findChild<QComboBox *>(QStringLiteral("composerAccount"));
        QVERIFY(account && !account->isHidden());
        const int work = account->findData(QStringLiteral("account-1"));
        QVERIFY(work >= 0);
        account->setCurrentIndex(work);
        emit account->activated(work);
        QCOMPARE(composer->accountId(), QStringLiteral("account-1"));
        sender = composer->findChild<QComboBox *>(QStringLiteral("sender"));
        QVERIFY2(sender->currentText().contains(QLatin1String("work@example.test")), qPrintable(sender->currentText()));
        QVERIFY(QMetaObject::invokeMethod(composer, "send"));
        QTRY_VERIFY2_WITH_TIMEOUT(!composer, qPrintable(errors.join(QLatin1Char('\n'))), 20000);
        QVERIFY2(errors.isEmpty(), qPrintable(errors.join(QLatin1Char('\n'))));
        sent = sentFile(QStringLiteral("work"));
        QVERIFY2(sent.contains("From: Pat <work@example.test>"), sent.constData());
        QVERIFY2(sent.contains("In-Reply-To: <home-"), sent.constData());
        QVERIFY(original.isValid());
        QVERIFY(original.data(Imap::Mailbox::RoleMessageIsMarkedReplied).toBool());
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
    LiveImapAccountsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_LiveImapAccounts.moc"
