/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/AccountStatus.h"
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Gui/DensityMenu.h"
#include "Gui/InlineNotificationBar.h"
#include "Gui/MailBoxTreeView.h"
#include "Gui/MessageListWidget.h"
#include "Gui/MsgListView.h"
#include "Gui/ShortcutHandler/ShortcutHandler.h"
#include "Gui/Window.h"
#include "Imap/Model/GroupingMsgListModel.h"
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/PrettyMsgListModel.h"
#include "Plugins/PasswordPlugin.h"
#include "Plugins/PluginManager.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "configure.cmake.h"
#ifdef TROJITA_HAVE_WEBENGINE
#include "Gui/WebEngineMessageProfile.h"
#endif
#include "static_plugins.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QActionGroup>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>

namespace {

void configureOffline(QSettings &settings, const QString &host)
{
    settings.setValue(Common::SettingsNames::imapMethodKey, Common::SettingsNames::methodSSL);
    settings.setValue(Common::SettingsNames::imapHostKey, host);
    settings.setValue(Common::SettingsNames::imapStartMode, Common::SettingsNames::netOffline);
}

void seedTwoAccounts(const QTemporaryDir &directory, QSettings &settings)
{
    settings.setValue(Common::SettingsNames::guiShowSystray, false);
    settings.setValue(QStringLiteral("accounts/ids"), QStringList{QStringLiteral("account-1"), QStringLiteral("account-2")});
    settings.setValue(QStringLiteral("accounts/active"), QStringLiteral("account-1"));
    settings.setValue(QStringLiteral("accounts/next"), 3);
    settings.setValue(QStringLiteral("accounts/legacy"), QStringLiteral("account-1"));
    settings.setValue(QStringLiteral("accounts/names/account-1"), QStringLiteral("Work"));
    settings.setValue(QStringLiteral("accounts/names/account-2"), QStringLiteral("Home"));
    QSettings first(directory.filePath(QStringLiteral("main-account-1.conf")), QSettings::IniFormat);
    configureOffline(first, QStringLiteral("work.example.test"));
    first.sync();
    QSettings second(directory.filePath(QStringLiteral("main-account-2.conf")), QSettings::IniFormat);
    configureOffline(second, QStringLiteral("home.example.test"));
    second.sync();
}

QComboBox *accountSwitcher(Gui::MainWindow &window)
{
    for (auto *combo : window.findChildren<QComboBox *>()) {
        if (combo->accessibleName() == i18n("Account"))
            return combo;
    }
    return nullptr;
}

}

class MainWindowAccountsTest : public QObject
{
    Q_OBJECT
private slots:
    void migratesSingleProfileIntoFirstAccount()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("legacy.ini")), QSettings::IniFormat);
        configureOffline(settings, QStringLiteral("legacy.example.test"));
        settings.setValue(Common::SettingsNames::guiShowSystray, false);
        Gui::MainWindow window(&settings);
        QCOMPARE(window.imapAccess()->server(), QStringLiteral("legacy.example.test"));
        QCOMPARE(window.accountSettings()->value(Common::SettingsNames::imapHostKey).toString(),
                 QStringLiteral("legacy.example.test"));
        QCOMPARE(settings.value(QStringLiteral("accounts/ids")).toStringList().size(), 1);
        QVERIFY(accountSwitcher(window));
        QCOMPARE(accountSwitcher(window)->count(), 1);
    }

    void switchingKeepsAccountsIndependent()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);

        Gui::MainWindow window(&settings);
        auto *switcher = accountSwitcher(window);
        QVERIFY(switcher);
        QCOMPARE(switcher->count(), 2);
        QCOMPARE(switcher->currentText(), QStringLiteral("Work"));
        auto *workAccess = window.imapAccess();
        auto *workModel = window.imapModel();
        QCOMPARE(workAccess->server(), QStringLiteral("work.example.test"));

        switcher->setCurrentIndex(1);
        emit switcher->activated(1);
        QVERIFY(window.imapAccess() != workAccess);
        QCOMPARE(window.imapAccess()->server(), QStringLiteral("home.example.test"));
        QCOMPARE(window.imapAccess()->credentialAccountId(), QStringLiteral("account-2"));
        QCOMPARE(window.accountSettings()->value(Common::SettingsNames::imapHostKey).toString(),
                 QStringLiteral("home.example.test"));
        QCOMPARE(settings.value(QStringLiteral("accounts/active")).toString(), QStringLiteral("account-2"));
        QVERIFY(window.windowTitle().contains(QStringLiteral("Home")));

        switcher->setCurrentIndex(0);
        emit switcher->activated(0);
        QCOMPARE(window.imapAccess(), workAccess);
        QCOMPARE(window.imapModel(), workModel);
        QCOMPARE(window.accountSettings()->value(Common::SettingsNames::imapHostKey).toString(),
                 QStringLiteral("work.example.test"));
        QVERIFY(window.windowTitle().contains(QStringLiteral("Work")));
    }

    void backgroundAccountStatusIsShown()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);

        Gui::MainWindow window(&settings);
        auto *switcher = accountSwitcher(window);
        QVERIFY(switcher);
        auto *workModel = window.imapModel();
        switcher->setCurrentIndex(1);
        emit switcher->activated(1);
        QVERIFY(window.imapModel() != workModel);
        QVERIFY(switcher->itemData(0, Qt::ToolTipRole).toString().isEmpty());

        emit workModel->networkError(QStringLiteral("Host lookup failed"));
        QVERIFY(switcher->itemData(0, Qt::ToolTipRole).toString().contains(QStringLiteral("Host lookup failed")));
        QCOMPARE(switcher->itemText(0), QStringLiteral("Work (%1)").arg(Common::AccountStatusTracker::label(Common::AccountStatusKind::NetworkError)));
        QVERIFY(switcher->itemData(1, Qt::ToolTipRole).toString().isEmpty());

        emit workModel->connectionStateChanged(1u, Imap::CONN_STATE_AUTHENTICATED);
        QVERIFY(switcher->itemData(0, Qt::ToolTipRole).toString().isEmpty());
        QCOMPARE(switcher->itemText(0), QStringLiteral("Work"));
        QCOMPARE(switcher->currentText(), QStringLiteral("Home"));
    }

    void removingAccountDeletesItsSettingsAndPasswords()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        settings.setValue(Common::SettingsNames::passwordPlugin, QStringLiteral("cleartextpassword"));

        Gui::MainWindow window(&settings);
        QTRY_VERIFY(window.pluginManager()->password() != nullptr);
        auto *password = window.pluginManager()->password();
        auto *store = password->storePassword(QStringLiteral("account-2"), QStringLiteral("imap"), QStringLiteral("home-secret"));
        QSignalSpy stored(store, &Plugins::PasswordJob::passwordStored);
        store->setAutoDelete(true);
        store->start();
        QVERIFY(stored.wait(1000));
        QVERIFY(settings.contains(QStringLiteral("accountPasswords/account-2/imap")));

        QVERIFY(QMetaObject::invokeMethod(&window, "removeAccountNow", Q_ARG(QString, QStringLiteral("account-2"))));
        QTRY_VERIFY(!settings.contains(QStringLiteral("accountPasswords/account-2/imap")));
        QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("main-account-2.conf"))));
        QCOMPARE(settings.value(QStringLiteral("accounts/ids")).toStringList(), QStringList{QStringLiteral("account-1")});
        auto *switcher = accountSwitcher(window);
        QVERIFY(switcher);
        QCOMPARE(switcher->count(), 1);
        QCOMPARE(window.imapAccess()->server(), QStringLiteral("work.example.test"));

        QVERIFY(QMetaObject::invokeMethod(&window, "removeAccountNow", Q_ARG(QString, QStringLiteral("account-1"))));
        QCOMPARE(switcher->count(), 1);
    }

    void accountsSwitchFromTheKeyboard()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);

        Gui::MainWindow window(&settings);
        auto *next = window.findChild<QAction *>(QStringLiteral("action_next_account"));
        auto *previous = window.findChild<QAction *>(QStringLiteral("action_previous_account"));
        auto *focusReader = window.findChild<QAction *>(QStringLiteral("action_focus_message_view"));
        QVERIFY(next);
        QVERIFY(previous);
        QVERIFY(focusReader);
        QVERIFY(!next->shortcut().isEmpty());
        QVERIFY(!previous->shortcut().isEmpty());
        auto activeAccount = [&settings]() { return settings.value(QStringLiteral("accounts/active")).toString(); };
        QCOMPARE(activeAccount(), QStringLiteral("account-1"));
        next->trigger();
        QCOMPARE(activeAccount(), QStringLiteral("account-2"));
        next->trigger();
        QCOMPARE(activeAccount(), QStringLiteral("account-1"));
        previous->trigger();
        QCOMPARE(activeAccount(), QStringLiteral("account-2"));
        previous->trigger();
        QCOMPARE(activeAccount(), QStringLiteral("account-1"));

        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        auto *mailboxes = window.findChild<Gui::MailBoxTreeView *>();
        QVERIFY(mailboxes);
        window.findChild<QAction *>(QStringLiteral("action_focus_mailbox_tree"))->trigger();
        QCOMPARE(window.focusWidget(), mailboxes);
        focusReader->trigger();
        QVERIFY(window.focusWidget());
        QVERIFY(window.focusWidget() != mailboxes);
    }

    void savedSearchesAndFolderExpansionFollowTheAccount()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        settings.setValue(Common::SettingsNames::guiExpandedMailboxes, QStringList{QStringLiteral("Shared")});
        auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
        KConfigGroup homeSearch = KConfigGroup(config, QStringLiteral("SavedSearches"))
                                      .group(QStringLiteral("account-2")).group(QStringLiteral("invoices"));
        homeSearch.writeEntry("Name", QStringLiteral("Home invoices"));
        homeSearch.writeEntry("Query", QStringLiteral("invoice"));
        QVERIFY(config->sync());

        Gui::MainWindow window(&settings);
        auto *mailboxes = window.findChild<Gui::MailBoxTreeView *>();
        auto *saved = window.findChild<QMenu *>(QStringLiteral("savedSearches"));
        QVERIFY(mailboxes);
        QVERIFY(saved);
        auto savedNames = [saved]() {
            emit saved->aboutToShow();
            QStringList names;
            for (auto *action : saved->actions())
                names << action->text();
            return names;
        };
        QCOMPARE(window.imapAccess()->credentialAccountId() == QStringLiteral("account-2"), false);
        QVERIFY(savedNames().isEmpty());
        QCOMPARE(mailboxes->desiredExpansion(), QStringList{QStringLiteral("Shared")});

        window.findChild<QAction *>(QStringLiteral("action_next_account"))->trigger();
        QCOMPARE(window.imapAccess()->credentialAccountId(), QStringLiteral("account-2"));
        QCOMPARE(savedNames(), QStringList{QStringLiteral("Home invoices")});
        QVERIFY(mailboxes->desiredExpansion().isEmpty());
        emit mailboxes->mailboxExpansionChanged({QStringLiteral("Projects")});
        QCOMPARE(window.accountSettings()->value(Common::SettingsNames::guiExpandedMailboxes).toStringList(),
                 QStringList{QStringLiteral("Projects")});
        QCOMPARE(settings.value(Common::SettingsNames::guiExpandedMailboxes).toStringList(), QStringList{QStringLiteral("Shared")});

        window.findChild<QAction *>(QStringLiteral("action_previous_account"))->trigger();
        QVERIFY(savedNames().isEmpty());
        QCOMPARE(mailboxes->desiredExpansion(), QStringList{QStringLiteral("Shared")});
        window.findChild<QAction *>(QStringLiteral("action_next_account"))->trigger();
        QCOMPARE(mailboxes->desiredExpansion(), QStringList{QStringLiteral("Projects")});
    }

    void focusCyclesThroughShellRegions()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);

        Gui::MainWindow window(&settings);
        auto *next = window.findChild<QAction *>(QStringLiteral("action_focus_next_region"));
        auto *previous = window.findChild<QAction *>(QStringLiteral("action_focus_previous_region"));
        QVERIFY(next);
        QVERIFY(previous);
        QCOMPARE(next->shortcut(), QKeySequence(Qt::Key_F6));
        QCOMPARE(previous->shortcut(), QKeySequence(Qt::SHIFT | Qt::Key_F6));

        window.resize(1000, 700);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        auto *mailboxes = window.findChild<Gui::MailBoxTreeView *>();
        auto *messages = window.findChild<Gui::MsgListView *>();
        auto *switcher = accountSwitcher(window);
        QVERIFY(mailboxes);
        QVERIFY(messages);
        QVERIFY(switcher);

        window.findChild<QAction *>(QStringLiteral("action_focus_mailbox_tree"))->trigger();
        QCOMPARE(window.focusWidget(), mailboxes);
        next->trigger();
        QCOMPARE(window.focusWidget(), messages);
        auto *search = window.findChild<Gui::MessageListWidget *>()->findChild<QLineEdit *>();
        QVERIFY(search);
        search->setEnabled(true);
        search->setFocus();
        previous->trigger();
        QCOMPARE(window.focusWidget(), mailboxes);
        next->trigger();
        next->trigger();
        QWidget *reader = window.focusWidget();
        QVERIFY(reader);
        QVERIFY(reader != mailboxes && reader != messages && reader != switcher);
        // The attachment list is hidden while no message is shown, so the cycle wraps to the header.
        next->trigger();
        QCOMPARE(window.focusWidget(), switcher->isVisible() ? static_cast<QWidget *>(switcher) : mailboxes);
        previous->trigger();
        QVERIFY(window.focusWidget() != mailboxes && window.focusWidget() != messages);
        QVERIFY(window.focusWidget() != switcher);
    }

    void goToFolderActionsWithoutFolders()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        Gui::MainWindow window(&settings);

        auto *goTo = window.findChild<QAction *>(QStringLiteral("action_go_to_folder"));
        QVERIFY(goTo);
        QCOMPARE(goTo->shortcut(), QKeySequence(QStringLiteral("Ctrl+J")));
        int ctrlJ = 0;
        for (QAction *action : window.findChildren<QAction *>()) {
            if (action->shortcuts().contains(QKeySequence(QStringLiteral("Ctrl+J"))))
                ++ctrlJ;
        }
        QCOMPARE(ctrlJ, 1);

        auto *menu = window.findChild<QMenu *>(QStringLiteral("goToFolderMenu"));
        QVERIFY(menu);
        QStringList names;
        for (QAction *action : menu->actions()) {
            names << action->objectName();
            // An offline account without cached folders offers no targets
            QVERIFY2(!action->isEnabled(), qPrintable(action->objectName()));
        }
        QCOMPARE(names, (QStringList{QStringLiteral("action_go_to_inbox"), QStringLiteral("action_go_to_drafts"),
                                     QStringLiteral("action_go_to_sent"), QStringLiteral("action_go_to_archive"),
                                     QStringLiteral("action_go_to_junk"), QStringLiteral("action_go_to_trash")}));

        // Switching the account rewires the actions to the new model and keeps them consistent
        window.findChild<QAction *>(QStringLiteral("action_next_account"))->trigger();
        QCOMPARE(window.imapAccess()->credentialAccountId(), QStringLiteral("account-2"));
        for (QAction *action : menu->actions())
            QVERIFY2(!action->isEnabled(), qPrintable(action->objectName()));

        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        goTo->trigger();
        auto *dialog = window.findChild<QDialog *>(QStringLiteral("goToFolderDialog"));
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->isVisible());
        QVERIFY(dialog->isModal());
        dialog->reject();
        QTRY_VERIFY(!window.findChild<QDialog *>(QStringLiteral("goToFolderDialog")));
    }

    void destructiveActionsOffline()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        Gui::MainWindow window(&settings);
        int prompts = 0;
        window.setConfirmationPrompt([&prompts](UiUtils::DestructiveActions::Confirmation, const QString &, const QString &) {
            ++prompts;
            return UiUtils::DestructiveActions::ConfirmationAnswer{};
        });

        auto *markAsDeleted = window.findChild<QAction *>(QStringLiteral("action_mark_as_deleted"));
        auto *deletePermanently = window.findChild<QAction *>(QStringLiteral("action_delete_permanently"));
        auto *movesToTrash = window.findChild<QAction *>(QStringLiteral("deleteMovesToTrash"));
        QVERIFY(markAsDeleted);
        QVERIFY(deletePermanently);
        QVERIFY(movesToTrash);
        QCOMPARE(markAsDeleted->shortcut(), QKeySequence(Qt::Key_Delete));
        QCOMPARE(deletePermanently->shortcut(), QKeySequence(Qt::SHIFT | Qt::Key_Delete));
        int shiftDelete = 0;
        for (QAction *action : window.findChildren<QAction *>()) {
            if (action->shortcuts().contains(QKeySequence(Qt::SHIFT | Qt::Key_Delete)))
                ++shiftDelete;
        }
        QCOMPARE(shiftDelete, 1);
        QVERIFY(!deletePermanently->isEnabled());
        // Without a known Trash folder Delete keeps toggling the \Deleted mark
        QVERIFY(markAsDeleted->isCheckable());
        QCOMPARE(markAsDeleted->text(), Gui::MainWindow::tr("Mark as &Deleted"));

        // The setting is per account and defaults to on
        QVERIFY(movesToTrash->isCheckable());
        QVERIFY(movesToTrash->isChecked());
        movesToTrash->setChecked(false);
        QCOMPARE(window.accountSettings()->value(Common::SettingsNames::imapDeleteMovesToTrash, true).toBool(), false);
        QVERIFY(!settings.contains(Common::SettingsNames::imapDeleteMovesToTrash));
        const QString firstAccount = window.imapAccess()->credentialAccountId();
        auto *nextAccount = window.findChild<QAction *>(QStringLiteral("action_next_account"));
        nextAccount->trigger();
        QCOMPARE(window.imapAccess()->credentialAccountId(), QStringLiteral("account-2"));
        QVERIFY(movesToTrash->isChecked());
        nextAccount->trigger();
        QCOMPARE(window.imapAccess()->credentialAccountId(), firstAccount);
        QVERIFY(!movesToTrash->isChecked());

        // Folder operations need the server
        for (const QString &name : {QStringLiteral("renameMailbox"), QStringLiteral("emptyTrash"), QStringLiteral("emptyJunk")}) {
            auto *action = window.findChild<QAction *>(name);
            QVERIFY2(action, qPrintable(name));
            QVERIFY2(!action->isEnabled(), qPrintable(name));
        }

        // Nothing to expunge without an open folder, so nothing is asked
        QVERIFY(QMetaObject::invokeMethod(&window, "slotExpunge"));
        QCOMPARE(prompts, 0);

        auto *bar = window.findChild<Gui::InlineNotificationBar *>(QStringLiteral("messageNotificationBar"));
        QVERIFY(bar);
        QVERIFY(bar->isHidden());
        QVERIFY(QMetaObject::invokeMethod(&window, "slotMoveToArchiveFailed", Q_ARG(QString, QStringLiteral("NO [TRYCREATE] gone"))));
        QVERIFY(!bar->isHidden());
        QCOMPARE(bar->kind(), Gui::InlineNotificationBar::Kind::Error);
        QVERIFY(bar->text().contains(QStringLiteral("NO [TRYCREATE] gone")));
        // A notification about one account must not linger after switching to another
        nextAccount->trigger();
        QVERIFY(bar->isHidden());
    }

    void quickActionsOffline()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        Gui::MainWindow window(&settings);

        const QList<std::pair<QString, QKeySequence>> expected = {
            {QStringLiteral("action_move_to_folder"), QKeySequence(QStringLiteral("V"))},
            {QStringLiteral("action_copy_to_folder"), QKeySequence(QStringLiteral("Shift+V"))},
            {QStringLiteral("action_mark_as_unread"), QKeySequence(QStringLiteral("Shift+M"))},
        };
        for (const auto &[name, shortcut] : expected) {
            auto *action = window.findChild<QAction *>(name);
            QVERIFY2(action, qPrintable(name));
            QCOMPARE(action->shortcut(), shortcut);
            // Offline and without a selection there is nothing to act on
            QVERIFY2(!action->isEnabled(), qPrintable(name));
        }

        // Every shortcut belongs to one action only
        QHash<QString, QString> owners;
        for (QAction *action : window.findChildren<QAction *>()) {
            if (!action->objectName().startsWith(QLatin1String("action_")))
                continue;
            for (const QKeySequence &shortcut : action->shortcuts()) {
                const QString key = shortcut.toString(QKeySequence::PortableText);
                if (key.isEmpty())
                    continue;
                const QString owner = owners.value(key);
                QVERIFY2(owner.isEmpty() || owner == action->objectName(),
                         qPrintable(key + QLatin1String(": ") + owner + QLatin1String(", ") + action->objectName()));
                owners.insert(key, action->objectName());
            }
        }

        for (const QString &name : {QStringLiteral("moveToMenu"), QStringLiteral("copyToMenu")}) {
            auto *menu = window.findChild<QMenu *>(name);
            QVERIFY2(menu, qPrintable(name));
            QVERIFY2(!menu->menuAction()->isEnabled(), qPrintable(name));
            // Without remembered folders only the picker is offered
            emit menu->aboutToShow();
            QCOMPARE(menu->actions().size(), 1);
        }
        auto *moveButton = window.findChild<QToolButton *>(QStringLiteral("moveToFolderButton"));
        QVERIFY(moveButton);
        QCOMPARE(moveButton->defaultAction(), window.findChild<QAction *>(QStringLiteral("action_move_to_folder")));
        QVERIFY(!moveButton->isEnabled());

        // Recent destinations are remembered per account, most recent first
        const QString key = Common::SettingsNames::imapRecentDestinationFolders;
        QVERIFY(QMetaObject::invokeMethod(&window, "rememberRecentFolder", Q_ARG(QString, QStringLiteral("Work/Projects"))));
        QVERIFY(QMetaObject::invokeMethod(&window, "rememberRecentFolder", Q_ARG(QString, QStringLiteral("R&D"))));
        const QStringList work{QStringLiteral("R&D"), QStringLiteral("Work/Projects")};
        QCOMPARE(window.accountSettings()->value(key).toStringList(), work);
        QVERIFY(!settings.contains(key));

        // Nothing is listed yet, so the folders cannot be judged and are offered; '&' is not a mnemonic
        auto *moveMenu = window.findChild<QMenu *>(QStringLiteral("moveToMenu"));
        emit moveMenu->aboutToShow();
        QStringList offered;
        for (QAction *action : moveMenu->actions()) {
            if (!action->isSeparator() && action->objectName().isEmpty())
                offered << action->data().toString();
        }
        QCOMPARE(offered, work);
        QCOMPARE(moveMenu->actions().constFirst()->text(), QStringLiteral("R&&D"));
        QCOMPARE(moveMenu->actions().constLast()->objectName(), QStringLiteral("moveToOtherFolder"));

        const QString firstAccount = window.imapAccess()->credentialAccountId();
        auto *nextAccount = window.findChild<QAction *>(QStringLiteral("action_next_account"));
        nextAccount->trigger();
        QCOMPARE(window.imapAccess()->credentialAccountId(), QStringLiteral("account-2"));
        QVERIFY(window.accountSettings()->value(key).toStringList().isEmpty());
        emit moveMenu->aboutToShow();
        QCOMPARE(moveMenu->actions().size(), 1);
        QVERIFY(QMetaObject::invokeMethod(&window, "rememberRecentFolder", Q_ARG(QString, QStringLiteral("Family"))));
        QCOMPARE(window.accountSettings()->value(key).toStringList(), QStringList{QStringLiteral("Family")});

        nextAccount->trigger();
        QCOMPARE(window.imapAccess()->credentialAccountId(), firstAccount);
        QCOMPARE(window.accountSettings()->value(key).toStringList(), work);
        QVERIFY(!settings.contains(key));

        // Offline, the picker is not offered
        window.findChild<QAction *>(QStringLiteral("action_move_to_folder"))->trigger();
        QVERIFY(QMetaObject::invokeMethod(&window, "slotMoveToFolder"));
        QVERIFY(!window.findChild<QDialog *>(QStringLiteral("moveToFolderDialog")));
    }

    void messageListFiltersOffline()
    {
        using Imap::Mailbox::PrettyMsgListModel;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        // The former global "hide read messages" preference seeds accounts without their own filters
        settings.setValue(Common::SettingsNames::guiMsgListHideRead, true);
        Gui::MainWindow window(&settings);

        auto action = [&window](const QString &name) {
            return window.findChild<QAction *>(name);
        };
        auto model = [&window]() {
            return window.findChild<PrettyMsgListModel *>(QStringLiteral("prettyMsgListModel"));
        };
        const QStringList filterNames{QStringLiteral("messageListFilterUnread"), QStringLiteral("messageListFilterFlagged"),
                                      QStringLiteral("messageListFilterAttachments")};
        for (const QString &name : filterNames + QStringList{QStringLiteral("messageListClearFilters")}) {
            QVERIFY2(action(name), qPrintable(name));
            auto *button = window.findChild<QToolButton *>(name + QLatin1String("Button"));
            QVERIFY2(button, qPrintable(name));
            QCOMPARE(button->defaultAction(), action(name));
            QVERIFY2(!button->accessibleName().isEmpty(), qPrintable(name));
            QVERIFY2(!button->toolTip().isEmpty(), qPrintable(name));
        }
        for (const QString &name : filterNames)
            QVERIFY2(action(name)->isCheckable(), qPrintable(name));

        auto *filterMenu = window.findChild<QMenu *>(QStringLiteral("messageListFilterMenu"));
        QVERIFY(filterMenu);
        for (const QString &name : filterNames + QStringList{QStringLiteral("messageListClearFilters")})
            QVERIFY2(filterMenu->actions().contains(action(name)), qPrintable(name));

        auto *sortButton = window.findChild<QToolButton *>(QStringLiteral("messageListSortButton"));
        QVERIFY(sortButton);
        QCOMPARE(sortButton->menu(), window.findChild<QMenu *>(QStringLiteral("sortMenu")));
        QVERIFY(!sortButton->toolTip().isEmpty());
        QVERIFY(!sortButton->accessibleName().isEmpty());

        // Selection shortcuts only apply to the message list, and no shortcut is used twice
        auto *tree = window.findChild<Gui::MessageListWidget *>()->tree;
        auto *selectAll = action(QStringLiteral("action_select_all_messages"));
        QVERIFY(selectAll);
        QCOMPARE(selectAll->shortcut(), QKeySequence(QKeySequence::SelectAll));
        for (const QString &name : {QStringLiteral("action_select_all_messages"), QStringLiteral("action_select_thread"),
                                    QStringLiteral("action_invert_selection"), QStringLiteral("action_deselect_messages")}) {
            QVERIFY2(action(name), qPrintable(name));
            QCOMPARE(action(name)->shortcutContext(), Qt::WidgetWithChildrenShortcut);
            QVERIFY2(tree->actions().contains(action(name)), qPrintable(name));
        }
        QHash<QString, QString> owners;
        for (QAction *candidate : window.findChildren<QAction *>()) {
            for (const QKeySequence &shortcut : candidate->shortcuts()) {
                const QString key = shortcut.toString(QKeySequence::PortableText);
                if (key.isEmpty() || candidate->objectName().isEmpty())
                    continue;
                const QString owner = owners.value(key);
                QVERIFY2(owner.isEmpty() || owner == candidate->objectName(),
                         qPrintable(key + QLatin1String(": ") + owner + QLatin1String(", ") + candidate->objectName()));
                owners.insert(key, candidate->objectName());
            }
        }

        // The legacy preference applies, and "Hide Read Messages" is the Unread filter
        auto *hideRead = action(QStringLiteral("hideReadMessages"));
        auto *clearButton = window.findChild<QToolButton *>(QStringLiteral("messageListClearFiltersButton"));
        QVERIFY(hideRead);
        QCOMPARE(model()->filters(), PrettyMsgListModel::Filters(PrettyMsgListModel::FilterUnread));
        QVERIFY(hideRead->isChecked());
        QVERIFY(action(QStringLiteral("messageListFilterUnread"))->isChecked());
        QVERIFY(!clearButton->isHidden());
        QVERIFY(action(QStringLiteral("messageListClearFilters"))->isEnabled());

        action(QStringLiteral("messageListFilterFlagged"))->trigger();
        QCOMPARE(model()->filters(), PrettyMsgListModel::FilterUnread | PrettyMsgListModel::FilterFlagged);
        hideRead->trigger();
        QCOMPARE(model()->filters(), PrettyMsgListModel::Filters(PrettyMsgListModel::FilterFlagged));
        QVERIFY(!action(QStringLiteral("messageListFilterUnread"))->isChecked());
        action(QStringLiteral("messageListFilterUnread"))->trigger();
        QVERIFY(hideRead->isChecked());
        action(QStringLiteral("messageListFilterAttachments"))->trigger();
        const auto all = PrettyMsgListModel::FilterUnread | PrettyMsgListModel::FilterFlagged | PrettyMsgListModel::FilterAttachments;
        QCOMPARE(model()->filters(), all);
        QCOMPARE(window.accountSettings()->value(Common::SettingsNames::guiMsgListFilterUnread).toBool(), true);
        QCOMPARE(window.accountSettings()->value(Common::SettingsNames::guiMsgListFilterFlagged).toBool(), true);
        QCOMPARE(window.accountSettings()->value(Common::SettingsNames::guiMsgListFilterAttachments).toBool(), true);
        for (const QString &key : {Common::SettingsNames::guiMsgListFilterUnread, Common::SettingsNames::guiMsgListFilterFlagged,
                                   Common::SettingsNames::guiMsgListFilterAttachments})
            QVERIFY2(!settings.contains(key), qPrintable(key));
        QCOMPARE(settings.value(Common::SettingsNames::guiMsgListHideRead).toBool(), true);

        // The other account keeps its own filters
        const QString firstAccount = window.imapAccess()->credentialAccountId();
        auto *nextAccount = action(QStringLiteral("action_next_account"));
        nextAccount->trigger();
        QCOMPARE(window.imapAccess()->credentialAccountId(), QStringLiteral("account-2"));
        QCOMPARE(model()->filters(), PrettyMsgListModel::Filters(PrettyMsgListModel::FilterUnread));
        QVERIFY(!action(QStringLiteral("messageListFilterFlagged"))->isChecked());
        QVERIFY(!action(QStringLiteral("messageListFilterAttachments"))->isChecked());
        action(QStringLiteral("messageListClearFilters"))->trigger();
        QCOMPARE(model()->filters(), PrettyMsgListModel::Filters(PrettyMsgListModel::NoFilter));
        for (const QString &name : filterNames)
            QVERIFY2(!action(name)->isChecked(), qPrintable(name));
        QVERIFY(!hideRead->isChecked());
        QVERIFY(clearButton->isHidden());
        QVERIFY(!action(QStringLiteral("messageListClearFilters"))->isEnabled());
        QCOMPARE(window.accountSettings()->value(Common::SettingsNames::guiMsgListFilterUnread, true).toBool(), false);

        nextAccount->trigger();
        QCOMPARE(window.imapAccess()->credentialAccountId(), firstAccount);
        QCOMPARE(model()->filters(), all);
        QVERIFY(!clearButton->isHidden());
        for (const QString &name : filterNames)
            QVERIFY2(action(name)->isChecked(), qPrintable(name));
        action(QStringLiteral("messageListClearFilters"))->trigger();
        QCOMPARE(model()->filters(), PrettyMsgListModel::Filters(PrettyMsgListModel::NoFilter));
        QVERIFY(!hideRead->isChecked());
        nextAccount->trigger();
        QCOMPARE(model()->filters(), PrettyMsgListModel::Filters(PrettyMsgListModel::NoFilter));
        nextAccount->trigger();
        QCOMPARE(model()->filters(), PrettyMsgListModel::Filters(PrettyMsgListModel::NoFilter));

        // Selection commands on a small threaded list: [1 [2, 3]], 4
        QStandardItemModel messages;
        auto message = [](uint uid) {
            auto *item = new QStandardItem(QString::number(uid));
            item->setData(uid, Imap::Mailbox::RoleMessageUid);
            return item;
        };
        QStandardItem *root = message(1);
        root->appendRow(message(2));
        root->appendRow(message(3));
        messages.appendRow(root);
        messages.appendRow(message(4));
        tree->setModel(&messages);
        tree->expandAll();
        auto *status = window.findChild<QLabel *>(QStringLiteral("messageListStatus"));
        QVERIFY(status);
        auto refreshStatus = [&window]() {
            return QMetaObject::invokeMethod(&window, "updateMessageListStatus");
        };

        tree->setCurrentIndex(messages.index(1, 0, messages.index(0, 0)));
        QVERIFY(QMetaObject::invokeMethod(&window, "slotSelectThread"));
        QVERIFY(refreshStatus());
        QCOMPARE(status->text(), QStringLiteral("3 selected"));
        QVERIFY(!status->isHidden());

        action(QStringLiteral("action_invert_selection"))->trigger();
        QCOMPARE(tree->selectionModel()->selectedRows().size(), 1);
        QCOMPARE(tree->selectionModel()->selectedRows().constFirst(), messages.index(1, 0));
        QVERIFY(refreshStatus());
        QVERIFY(status->text().isEmpty());
        QVERIFY(status->isHidden());

        selectAll->trigger();
        QCOMPARE(tree->selectionModel()->selectedRows().size(), 4);
        QVERIFY(refreshStatus());
        QCOMPARE(status->text(), QStringLiteral("4 selected"));

        action(QStringLiteral("action_deselect_messages"))->trigger();
        QVERIFY(!tree->selectionModel()->hasSelection());
        QVERIFY(refreshStatus());
        QVERIFY(status->isHidden());

        // A collapsed thread is selected as a whole without listing its hidden rows twice
        tree->collapseAll();
        tree->setCurrentIndex(messages.index(0, 0));
        QVERIFY(QMetaObject::invokeMethod(&window, "slotSelectThread"));
        QVERIFY(refreshStatus());
        QCOMPARE(status->text(), QStringLiteral("3 selected"));
        action(QStringLiteral("action_invert_selection"))->trigger();
        QCOMPARE(tree->selectionModel()->selectedRows(), QModelIndexList{messages.index(1, 0)});
        action(QStringLiteral("action_invert_selection"))->trigger();
        QCOMPARE(tree->selectionModel()->selectedRows(), QModelIndexList{messages.index(0, 0)});
        QVERIFY(refreshStatus());
        QCOMPARE(status->text(), QStringLiteral("3 selected"));
        tree->setModel(nullptr);
    }

    void shortcutOverviewAndSelectionOffline()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        Gui::MainWindow window(&settings);

        auto *overview = window.findChild<QAction *>(QStringLiteral("action_shortcut_overview"));
        QVERIFY(overview);
        QCOMPARE(overview->shortcut(), QKeySequence(QStringLiteral("?")));
        QVERIFY(overview->isEnabled());
        bool inMenu = false;
        for (QMenu *menu : window.findChildren<QMenu *>())
            inMenu = inMenu || menu->actions().contains(overview);
        QVERIFY(inMenu);

        // Every shortcut, including the new one, belongs to one action only
        QHash<QString, QString> owners;
        for (QAction *action : window.findChildren<QAction *>()) {
            if (!action->objectName().startsWith(QLatin1String("action_")))
                continue;
            for (const QKeySequence &shortcut : action->shortcuts()) {
                const QString key = shortcut.toString(QKeySequence::PortableText);
                if (key.isEmpty())
                    continue;
                const QString owner = owners.value(key);
                QVERIFY2(owner.isEmpty() || owner == action->objectName(),
                         qPrintable(key + QLatin1String(": ") + owner + QLatin1String(", ") + action->objectName()));
                owners.insert(key, action->objectName());
            }
        }

        overview->trigger();
        auto *dialog = window.findChild<QDialog *>(QStringLiteral("shortcutOverviewDialog"));
        QVERIFY(dialog);
        QVERIFY(dialog->isVisible());
        QVERIFY(!dialog->isModal());
        auto *list = dialog->findChild<QTreeWidget *>(QStringLiteral("shortcutOverviewList"));
        auto *search = dialog->findChild<QLineEdit *>(QStringLiteral("shortcutOverviewSearch"));
        QVERIFY(list);
        QVERIFY(search);
        QVERIFY(dialog->findChild<QPushButton *>(QStringLiteral("shortcutOverviewConfigure")));

        QStringList groups;
        QHash<QString, QString> areas;
        QHash<QString, QString> shortcuts;
        for (int i = 0; i < list->topLevelItemCount(); ++i) {
            QTreeWidgetItem *group = list->topLevelItem(i);
            groups << group->text(0);
            for (int j = 0; j < group->childCount(); ++j) {
                const QString id = group->child(j)->data(0, Qt::UserRole).toString();
                areas.insert(id, group->text(0));
                shortcuts.insert(id, group->child(j)->text(1));
            }
        }
        // Menus come first, in the order of the menu bar
        QCOMPARE(groups.value(0), QStringLiteral("Application"));
        QVERIFY(groups.contains(QStringLiteral("Help")));
        QVERIFY(groups.contains(QStringLiteral("Message List")));
        // Every defined action is listed once
        QCOMPARE(areas.size(), Gui::ShortcutHandler::instance()->actionDescriptions().size());
        QCOMPARE(areas.value(QStringLiteral("action_archive")), QStringLiteral("Message"));
        QCOMPARE(shortcuts.value(QStringLiteral("action_archive")), QKeySequence(QStringLiteral("A")).toString(QKeySequence::NativeText));
        QCOMPARE(areas.value(QStringLiteral("action_shortcut_overview")), QStringLiteral("Help"));
        QCOMPARE(areas.value(QStringLiteral("action_mark_as_junk")), QStringLiteral("Message List"));
        QCOMPARE(areas.value(QStringLiteral("action_messagewindow_close")), QStringLiteral("Other Windows"));
#ifdef TROJITA_HAVE_WEBENGINE
        QCOMPARE(areas.value(QStringLiteral("action_zoom_in")), QStringLiteral("Message Reader"));
#endif

        const int allGroups = list->topLevelItemCount();
        search->setText(QStringLiteral("archive"));
        QVERIFY(list->topLevelItemCount() >= 1);
        QVERIFY(list->topLevelItemCount() < allGroups);
        search->setText(QStringLiteral("no such action at all"));
        QCOMPARE(list->topLevelItemCount(), 0);
        search->clear();
        QCOMPARE(list->topLevelItemCount(), allGroups);

        // Triggering it again raises the same dialog
        overview->trigger();
        QCOMPARE(window.findChildren<QDialog *>(QStringLiteral("shortcutOverviewDialog")).size(), 1);
        dialog->close();
        QTRY_VERIFY(!window.findChild<QDialog *>(QStringLiteral("shortcutOverviewDialog")));

        // Removing the selected messages makes the next remaining one current and selected
        auto *tree = window.findChild<Gui::MessageListWidget *>()->tree;
        QStandardItemModel messages;
        for (uint uid = 1; uid <= 5; ++uid) {
            auto *item = new QStandardItem(QString::number(uid));
            item->setData(uid, Imap::Mailbox::RoleMessageUid);
            messages.appendRow(item);
        }
        tree->setModel(&messages);
        tree->setCurrentIndex(messages.index(1, 0));
        tree->selectionModel()->select(QItemSelection(messages.index(1, 0), messages.index(2, 0)),
                                       QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        QCOMPARE(tree->selectionModel()->selectedRows().size(), 2);
        messages.removeRows(1, 2);
        // The selection follows once the rows are gone
        QTRY_COMPARE(tree->selectionModel()->selectedRows(), QModelIndexList{messages.index(1, 0)});
        QCOMPARE(tree->currentIndex().data(Imap::Mailbox::RoleMessageUid).toUInt(), 4u);
        tree->setModel(nullptr);
    }

    void powerUserProcessingOffline()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        Gui::MainWindow window(&settings);
        auto action = [&window](const QString &name) {
            return window.findChild<QAction *>(name);
        };

        // Triage keys next to the unchanged N and P
#ifdef TROJITA_HAVE_WEBENGINE
        const QList<std::pair<QString, QString>> triage{
            {QStringLiteral("action_mark_read_go_to_next_unread"), QStringLiteral("Shift+N")},
            {QStringLiteral("action_go_to_next_message"), QStringLiteral("F")},
            {QStringLiteral("action_go_to_previous_message"), QStringLiteral("B")},
            {QStringLiteral("action_go_to_next_unread"), QStringLiteral("N")},
            {QStringLiteral("action_go_to_previous_unread"), QStringLiteral("P")},
        };
        for (const auto &[name, key] : triage) {
            QVERIFY2(action(name), qPrintable(name));
            QCOMPARE(action(name)->shortcut(), QKeySequence(key));
        }
#endif
        QHash<QString, QString> owners;
        for (QAction *candidate : window.findChildren<QAction *>()) {
            if (!candidate->objectName().startsWith(QLatin1String("action_")))
                continue;
            for (const QKeySequence &shortcut : candidate->shortcuts()) {
                const QString key = shortcut.toString(QKeySequence::PortableText);
                if (key.isEmpty())
                    continue;
                const QString owner = owners.value(key);
                QVERIFY2(owner.isEmpty() || owner == candidate->objectName(),
                         qPrintable(key + QLatin1String(": ") + owner + QLatin1String(", ") + candidate->objectName()));
                owners.insert(key, candidate->objectName());
            }
        }

        auto *tree = window.findChild<Gui::MessageListWidget *>()->tree;
        auto *relatedMenu = window.findChild<QMenu *>(QStringLiteral("showRelatedMenu"));
        auto *similarMenu = window.findChild<QMenu *>(QStringLiteral("selectSimilarMenu"));
        QVERIFY(relatedMenu);
        QVERIFY(similarMenu);
        const QStringList relatedNames{QStringLiteral("action_show_related_sender"), QStringLiteral("action_show_related_recipient"),
                                       QStringLiteral("action_show_related_subject"), QStringLiteral("action_show_related_list")};
        const QStringList similarNames{QStringLiteral("action_select_same_sender"), QStringLiteral("action_select_same_subject"),
                                       QStringLiteral("action_select_unread"), QStringLiteral("action_select_flagged")};
        for (const QString &name : relatedNames)
            QVERIFY2(relatedMenu->actions().contains(action(name)), qPrintable(name));
        for (const QString &name : similarNames)
            QVERIFY2(similarMenu->actions().contains(action(name)), qPrintable(name));
        for (const QString &name : relatedNames + similarNames) {
            QCOMPARE(action(name)->shortcutContext(), Qt::WidgetWithChildrenShortcut);
            QVERIFY2(tree->actions().contains(action(name)), qPrintable(name));
        }

        // A small list: [1 [2]], 3, 4 (not fetched yet)
        QStandardItemModel messages;
        auto message = [](uint uid, const QString &mailbox, const QString &subject, bool read, bool flagged, bool fetched = true) {
            auto *item = new QStandardItem(subject);
            item->setData(uid, Imap::Mailbox::RoleMessageUid);
            item->setData(fetched, Imap::Mailbox::RoleIsFetched);
            item->setData(QVariantList{QStringList{QStringLiteral("Someone"), QString(), mailbox, QStringLiteral("example.org")}},
                          Imap::Mailbox::RoleMessageFrom);
            item->setData(QVariantList{QStringList{QStringLiteral("Team"), QString(), QStringLiteral("team"), QStringLiteral("example.org")}},
                          Imap::Mailbox::RoleMessageTo);
            item->setData(subject, Imap::Mailbox::RoleMessageSubject);
            item->setData(read, Imap::Mailbox::RoleMessageIsMarkedRead);
            item->setData(flagged, Imap::Mailbox::RoleMessageIsMarkedFlagged);
            return item;
        };
        QStandardItem *root = message(1, QStringLiteral("ann"), QStringLiteral("[dev] Release plan"), true, false);
        root->appendRow(message(2, QStringLiteral("bob"), QStringLiteral("Re: [dev] Release plan"), false, true));
        messages.appendRow(root);
        messages.appendRow(message(3, QStringLiteral("Ann"), QStringLiteral("Lunch"), false, false));
        messages.appendRow(message(4, QStringLiteral("ann"), QStringLiteral("Release plan"), false, true, false));
        tree->setModel(&messages);
        tree->collapseAll();
        auto uids = [tree]() {
            QList<uint> result;
            for (const QModelIndex &index : tree->selectionModel()->selectedRows())
                result << index.data(Imap::Mailbox::RoleMessageUid).toUInt();
            std::sort(result.begin(), result.end());
            return result;
        };

        tree->setCurrentIndex(messages.index(0, 0));
        QVERIFY(QMetaObject::invokeMethod(&window, "updateRelatedActions"));
        for (const QString &name : relatedNames.mid(0, 3) + similarNames)
            QVERIFY2(action(name)->isEnabled(), qPrintable(name));
        // Not a list message
        QVERIFY(!action(QStringLiteral("action_show_related_list"))->isVisible());

        // Only loaded messages count; the collapsed thread is opened so that its other replies are not acted on
        action(QStringLiteral("action_select_same_sender"))->trigger();
        QCOMPARE(uids(), (QList<uint>{1, 3}));
        QCOMPARE(tree->currentIndex(), messages.index(0, 0));
        QVERIFY(tree->isExpanded(messages.index(0, 0)));
        // The status counts what the actions would act on
        QVERIFY(QMetaObject::invokeMethod(&window, "updateMessageListStatus"));
        QCOMPARE(window.findChild<QLabel *>(QStringLiteral("messageListStatus"))->text(), QStringLiteral("2 selected"));
        action(QStringLiteral("action_select_same_subject"))->trigger();
        QCOMPARE(uids(), (QList<uint>{1, 2}));
        action(QStringLiteral("action_select_unread"))->trigger();
        QCOMPARE(uids(), (QList<uint>{2, 3, 4}));
        action(QStringLiteral("action_select_flagged"))->trigger();
        QCOMPARE(uids(), (QList<uint>{2, 4}));

        // An unknown sender disables the actions which need it
        tree->setCurrentIndex(messages.index(2, 0));
        QVERIFY(QMetaObject::invokeMethod(&window, "updateRelatedActions"));
        QVERIFY(!action(QStringLiteral("action_show_related_sender"))->isEnabled());
        QVERIFY(!action(QStringLiteral("action_select_same_sender"))->isEnabled());
        QVERIFY(action(QStringLiteral("action_select_unread"))->isEnabled());

        // Show Related goes through the quick search, where the condition stays visible
        auto *list = window.findChild<Gui::MessageListWidget *>();
        auto *search = list->findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        QVERIFY(search);
        tree->setCurrentIndex(messages.index(0, 0, messages.index(0, 0)));
        QVERIFY(QMetaObject::invokeMethod(&window, "updateRelatedActions"));
        QSignalSpy searches(list, &Gui::MessageListWidget::requestingSearch);
        action(QStringLiteral("action_show_related_subject"))->trigger();
        QCOMPARE(search->text(), QStringLiteral("subject:Release plan"));
        QCOMPARE(list->searchConditions(), (QStringList{QStringLiteral("SUBJECT"), QStringLiteral("Release plan")}));
        QVERIFY(!searches.isEmpty());
        action(QStringLiteral("action_show_related_sender"))->trigger();
        QCOMPARE(search->text(), QStringLiteral("from:bob@example.org"));
        QCOMPARE(list->searchConditions(), (QStringList{QStringLiteral("FROM"), QStringLiteral("bob@example.org")}));
        QCOMPARE(tree->currentIndex().data(Imap::Mailbox::RoleMessageUid).toUInt(), 2u);
        // Escape in the field clears the filter
        search->setFocus();
        QTest::keyClick(search, Qt::Key_Escape);
        QVERIFY(search->text().isEmpty());
        QVERIFY(list->searchConditions().isEmpty());
        tree->setModel(nullptr);

        // The dense layout is compact, without the panels triage does not need, and gives everything back
        auto *density = window.findChild<Gui::DensityMenu *>();
        auto *dense = action(QStringLiteral("action_dense_processing_layout"));
        QVERIFY(density);
        QVERIFY(dense);
        QVERIFY(dense->isCheckable());
        QVERIFY(!dense->isChecked());
        density->setComfortable(true);
        QList<QAction *> panels;
        QAction *attachments = nullptr;
        for (const QString &id : {QStringLiteral("Attachments"), QStringLiteral("MessageDetails"), QStringLiteral("Contact"),
                                  QStringLiteral("RelatedMessages")}) {
            QAction *panel = window.findChild<QAction *>(QLatin1String("embeddedPanel") + id);
            if (!panel)
                continue;
            if (!panel->isChecked())
                panel->trigger();
            QVERIFY(panel->isChecked());
            if (id == QLatin1String("Attachments"))
                attachments = panel;
            else
                panels << panel;
        }
        QVERIFY(!panels.isEmpty());

        dense->trigger();
        QVERIFY(dense->isChecked());
        QVERIFY(!density->isComfortable());
        for (QAction *panel : std::as_const(panels))
            QVERIFY2(!panel->isChecked(), qPrintable(panel->objectName()));
        if (attachments)
            QVERIFY(attachments->isChecked());
        QVERIFY(KConfigGroup(KSharedConfig::openStateConfig(), QStringLiteral("DenseProcessing")).readEntry("Enabled", false));

        dense->trigger();
        QVERIFY(!dense->isChecked());
        QVERIFY(density->isComfortable());
        for (QAction *panel : std::as_const(panels))
            QVERIFY2(panel->isChecked(), qPrintable(panel->objectName()));
        QVERIFY(!KConfigGroup(KSharedConfig::openStateConfig(), QStringLiteral("DenseProcessing")).readEntry("Enabled", true));
    }

    void groupingAndRelevanceOffline()
    {
        using Imap::Mailbox::GroupingMsgListModel;
        using Common::SettingsNames;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        Gui::MainWindow window(&settings);
        auto action = [&window](const QString &name) {
            return window.findChild<QAction *>(name);
        };
        auto prettyModel = [&window]() {
            return window.findChild<Imap::Mailbox::PrettyMsgListModel *>(QStringLiteral("prettyMsgListModel"));
        };
        auto groupingModel = [&window]() {
            return window.findChild<GroupingMsgListModel *>(QStringLiteral("groupingMsgListModel"));
        };
        auto *list = window.findChild<Gui::MessageListWidget *>();
        auto *tree = list->tree;

        // View > Group By and the Sort button offer the same exclusive choices
        auto *groupMenu = window.findChild<QMenu *>(QStringLiteral("messageListGroupByMenu"));
        auto *sortMenu = window.findChild<QMenu *>(QStringLiteral("sortMenu"));
        QVERIFY(groupMenu);
        QVERIFY(sortMenu);
        QVERIFY(sortMenu->actions().contains(groupMenu->menuAction()));
        int menusWithGroups = 0;
        for (QMenu *menu : window.findChildren<QMenu *>())
            menusWithGroups += menu->actions().contains(groupMenu->menuAction()) ? 1 : 0;
        QCOMPARE(menusWithGroups, 2);
        const QStringList groupNames{QStringLiteral("messageListGroupByNone"), QStringLiteral("messageListGroupByConversation"),
                                     QStringLiteral("messageListGroupByDate"), QStringLiteral("messageListGroupBySender"),
                                     QStringLiteral("messageListGroupBySubject"), QStringLiteral("messageListGroupByTag"),
                                     QStringLiteral("messageListGroupByReadState"),
                                     QStringLiteral("messageListGroupByMailingList")};
        QActionGroup *groupBy = nullptr;
        for (const QString &name : groupNames) {
            QVERIFY2(action(name), qPrintable(name));
            QVERIFY2(action(name)->isCheckable(), qPrintable(name));
            QVERIFY2(groupMenu->actions().contains(action(name)), qPrintable(name));
            if (!groupBy)
                groupBy = action(name)->actionGroup();
            QVERIFY2(groupBy && action(name)->actionGroup() == groupBy, qPrintable(name));
        }
        QVERIFY(groupBy->isExclusive());
        auto *expandAll = action(QStringLiteral("messageListExpandAllGroups"));
        auto *collapseAll = action(QStringLiteral("messageListCollapseAllGroups"));
        QVERIFY(expandAll);
        QVERIFY(collapseAll);
        QVERIFY(groupMenu->actions().contains(expandAll));
        QVERIFY(groupMenu->actions().contains(collapseAll));
        auto *relevance = action(QStringLiteral("messageListSortByRelevance"));
        QVERIFY(relevance);
        QVERIFY(sortMenu->actions().contains(relevance));
        QVERIFY(relevance->isCheckable());

        QHash<QString, QString> owners;
        for (QAction *candidate : window.findChildren<QAction *>()) {
            for (const QKeySequence &shortcut : candidate->shortcuts()) {
                const QString key = shortcut.toString(QKeySequence::PortableText);
                if (key.isEmpty() || candidate->objectName().isEmpty())
                    continue;
                const QString owner = owners.value(key);
                QVERIFY2(owner.isEmpty() || owner == candidate->objectName(),
                         qPrintable(key + QLatin1String(": ") + owner + QLatin1String(", ") + candidate->objectName()));
                owners.insert(key, candidate->objectName());
            }
        }

        // By default the list looks as before
        QVERIFY(action(QStringLiteral("messageListGroupByNone"))->isChecked());
        QCOMPARE(tree->model(), prettyModel());
        QVERIFY(!expandAll->isEnabled());
        QVERIFY(!collapseAll->isEnabled());
        QVERIFY(!window.accountSettings()->contains(SettingsNames::guiMsgListGroupBy));

        // Grouping is stored per account; the threading preference of all accounts stays
        action(QStringLiteral("messageListGroupByDate"))->trigger();
        QVERIFY(groupingModel());
        QCOMPARE(tree->model(), groupingModel());
        QCOMPARE(groupingModel()->sourceModel(), prettyModel());
        QCOMPARE(groupingModel()->grouping(), GroupingMsgListModel::Grouping::Date);
        QVERIFY(action(QStringLiteral("messageListGroupByDate"))->isChecked());
        QVERIFY(!action(QStringLiteral("messageListGroupByNone"))->isChecked());
        QVERIFY(expandAll->isEnabled());
        QVERIFY(collapseAll->isEnabled());
        QVERIFY(tree->rootIsDecorated());
        QCOMPARE(window.accountSettings()->value(SettingsNames::guiMsgListGroupBy).toString(),
                 GroupingMsgListModel::groupingName(GroupingMsgListModel::Grouping::Date));
        QVERIFY(!settings.contains(SettingsNames::guiMsgListShowThreading));

        auto *nextAccount = action(QStringLiteral("action_next_account"));
        nextAccount->trigger();
        QCOMPARE(window.imapAccess()->credentialAccountId(), QStringLiteral("account-2"));
        QCOMPARE(tree->model(), prettyModel());
        QVERIFY(action(QStringLiteral("messageListGroupByNone"))->isChecked());
        QVERIFY(!window.accountSettings()->contains(SettingsNames::guiMsgListGroupBy));
        QVERIFY(!expandAll->isEnabled());
        QVERIFY(!tree->rootIsDecorated());

        nextAccount->trigger();
        QCOMPARE(settings.value(QStringLiteral("accounts/active")).toString(), QStringLiteral("account-1"));
        QCOMPARE(tree->model(), groupingModel());
        QCOMPARE(groupingModel()->grouping(), GroupingMsgListModel::Grouping::Date);
        QVERIFY(action(QStringLiteral("messageListGroupByDate"))->isChecked());

        action(QStringLiteral("messageListGroupByNone"))->trigger();
        QCOMPARE(tree->model(), prettyModel());
        QVERIFY(!groupingModel()->sourceModel());
        QVERIFY(action(QStringLiteral("messageListGroupByNone"))->isChecked());
        QVERIFY(!window.accountSettings()->contains(SettingsNames::guiMsgListGroupBy));
        QVERIFY(!tree->rootIsDecorated());

        // Relevance needs a search
        QVERIFY(!relevance->isEnabled());
        QVERIFY(!relevance->toolTip().isEmpty());
        list->showFieldFilter(QStringLiteral("from:ann@example.org"));
        QVERIFY(relevance->isEnabled());
        list->showFieldFilter(QString());
        QVERIFY(!relevance->isEnabled());

        // The headers of groups are not messages
        QStandardItemModel messages;
        for (uint uid = 1; uid <= 4; ++uid) {
            auto *item = new QStandardItem(QStringLiteral("Message %1").arg(uid));
            item->setData(uid, Imap::Mailbox::RoleMessageUid);
            item->setData(uid % 2 == 0, Imap::Mailbox::RoleMessageIsMarkedRead);
            messages.appendRow(item);
        }
        GroupingMsgListModel groups;
        groups.setSourceModel(&messages);
        groups.setGrouping(GroupingMsgListModel::Grouping::ReadState);
        QCOMPARE(groups.rowCount(), 2);
        tree->setModel(&groups);
        tree->expandAll();
        const QModelIndex unread = groups.index(0, 0);
        const QModelIndex read = groups.index(1, 0);
        QVERIFY(groups.isGroupHeader(unread));
        QVERIFY(groups.isGroupHeader(read));
        QVERIFY(!(groups.flags(unread) & Qt::ItemIsSelectable));

        action(QStringLiteral("action_select_all_messages"))->trigger();
        const QModelIndexList rows = tree->selectionModel()->selectedRows(0);
        QCOMPARE(rows.size(), 4);
        for (const QModelIndex &row : rows)
            QVERIFY(row.data(Imap::Mailbox::RoleMessageUid).isValid());
        QModelIndexList translated;
        QVERIFY(QMetaObject::invokeMethod(&window, "translatedSelection", Q_RETURN_ARG(QModelIndexList, translated)));
        QCOMPARE(translated.size(), 4);
        for (const QModelIndex &index : std::as_const(translated))
            QVERIFY(index.isValid() && index.model() == &messages);

        // Up and Down step over the headers of expanded groups; Left and Right collapse and expand a group
        const QModelIndex lastUnread = groups.index(groups.rowCount(unread) - 1, 0, unread);
        tree->setCurrentIndex(lastUnread);
        QTest::keyPress(tree, Qt::Key_Down);
        QCOMPARE(tree->currentIndex().siblingAtColumn(0), groups.index(0, 0, read));
        QTest::keyPress(tree, Qt::Key_Up);
        QCOMPARE(tree->currentIndex().siblingAtColumn(0), lastUnread);
        tree->setCurrentIndex(read);
        QTest::keyPress(tree, Qt::Key_Left);
        QVERIFY(!tree->isExpanded(read));
        QTest::keyPress(tree, Qt::Key_Right);
        QVERIFY(tree->isExpanded(read));
        tree->setModel(nullptr);
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir home;
    if (!home.isValid())
        return 1;
    qputenv("HOME", home.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", home.filePath(QStringLiteral("config")).toUtf8());
    qputenv("XDG_DATA_HOME", home.filePath(QStringLiteral("data")).toUtf8());
    qputenv("XDG_CACHE_HOME", home.filePath(QStringLiteral("cache")).toUtf8());
    qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent/plasma-impresario-test");
    qunsetenv("TROJITA_PROFILE");
    QStandardPaths::setTestModeEnabled(true);
#ifdef TROJITA_HAVE_WEBENGINE
    Gui::WebEngineMessageProfile::registerScheme();
#endif
    Common::registerMetaTypes();
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(license);
    KLocalizedString::setApplicationDomain("plasma-impresario");
    if (!Imap::Mailbox::SystemNetworkWatcher::init())
        return 1;
    MainWindowAccountsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_MainWindowAccounts.moc"
