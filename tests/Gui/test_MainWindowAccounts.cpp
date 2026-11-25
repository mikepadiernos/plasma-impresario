/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/AccountStatus.h"
#include "Common/MetaTypes.h"
#include "Common/Paths.h"
#include "Common/SettingsNames.h"
#include "Gui/AccountSidebar.h"
#include "Gui/CompleteMessageWidget.h"
#include "Gui/ContactPanel.h"
#include "Gui/MessageDetailsPanel.h"
#include "Gui/MessageView.h"
#include "Gui/ReaderActionBar.h"
#include "Composer/AccountIdentities.h"
#include "Composer/SenderIdentitiesModel.h"
#include "Gui/ComposeWidget.h"
#include "Gui/DensityMenu.h"
#include "Gui/InlineNotificationBar.h"
#include "Gui/MailBoxTreeView.h"
#include "Gui/MessageListWidget.h"
#include "Gui/MessageRowsMenu.h"
#include "Gui/MsgListView.h"
#include "Gui/ShortcutHandler/ShortcutHandler.h"
#include "Gui/Window.h"
#include "Imap/Model/GroupingMsgListModel.h"
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/CombinedCache.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxMetadata.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/PrettyMsgListModel.h"
#include "OAuth/Session.h"
#include "Plugins/PasswordPlugin.h"
#include "Plugins/PluginManager.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "UiUtils/ViewStateStore.h"
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
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QPointer>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QShortcut>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrlQuery>

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

/** @short Let the offline first account know the folders INBOX and Archive with five messages each */
bool seedFirstAccountCache()
{
    // The first account is the migrated one, whose cache has no account directory of its own
    const QString cacheDir = Common::writablePath(Common::LOCATION_CACHE) + QLatin1Char('/');
    if (!QDir().mkpath(cacheDir))
        return false;
    Imap::Mailbox::CombinedCache cache(QStringLiteral("trojita-imap-cache"), cacheDir);
    if (!cache.open())
        return false;
    const QString separator = QStringLiteral(".");
    cache.setChildMailboxes(QString(), {Imap::Mailbox::MailboxMetadata(QStringLiteral("Archive"), separator, {}),
                                        Imap::Mailbox::MailboxMetadata(QStringLiteral("INBOX"), separator, {})});
    const QList<std::pair<QString, uint>> folders{{QStringLiteral("INBOX"), 7}, {QStringLiteral("Archive"), 8}};
    for (const auto &[mailbox, validity] : folders) {
        Imap::Mailbox::SyncState sync;
        sync.setExists(5);
        sync.setUidValidity(validity);
        sync.setUidNext(6);
        sync.setUnSeenCount(5);
        sync.setRecent(0);
        cache.setMailboxSyncState(mailbox, sync);
        cache.setUidMapping(mailbox, Imap::Uids{1, 2, 3, 4, 5});
        for (uint uid = 1; uid <= 5; ++uid)
            cache.setMsgFlags(mailbox, uid, QStringList());
    }
    return true;
}

/** @short Make the account send through a script that saves the message to @arg output, and not file it in Sent */
void sendThroughScript(const QTemporaryDir &directory, const QString &accountFile, const QString &output)
{
    const QString script = directory.filePath(output + QStringLiteral(".sh"));
    QFile file(script);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("cat > '" + directory.filePath(output).toUtf8() + "'\n");
    file.close();
    QSettings account(directory.filePath(accountFile), QSettings::IniFormat);
    account.setValue(Common::SettingsNames::msaMethodKey, Common::SettingsNames::methodSENDMAIL);
    account.setValue(Common::SettingsNames::sendmailKey, QString(QStringLiteral("/bin/sh ") + script));
    account.setValue(Common::SettingsNames::composerSaveToImapKey, false);
    account.sync();
}

Gui::ComposeWidget *openComposer(Gui::MainWindow &window)
{
    auto *composer = Gui::ComposeWidget::createFromUrl(
        &window, QUrl(QStringLiteral("mailto:bob@example.test?subject=Synthetic&body=Synthetic%20body")));
    if (composer) {
        if (auto *sender = composer->findChild<QComboBox *>(QStringLiteral("sender")))
            sender->setEditText(QStringLiteral("pat@example.test"));
    }
    return composer;
}

QMenu *openAccountMenu(Gui::MainWindow &window, const QString &accountId)
{
    for (QMenu *old : window.findChildren<QMenu *>(QStringLiteral("accountMenu")))
        old->close();
    QMetaObject::invokeMethod(&window, "showAccountMenu", Q_ARG(QString, accountId), Q_ARG(QPoint, QPoint(10, 10)));
    for (QMenu *menu : window.findChildren<QMenu *>(QStringLiteral("accountMenu"))) {
        if (menu->isVisible())
            return menu;
    }
    return nullptr;
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

        auto *alerts = window.findChild<QToolButton *>(QStringLiteral("backgroundAccountAlerts"));
        QVERIFY(alerts);
        QVERIFY(alerts->isHidden());

        emit workModel->networkError(QStringLiteral("Host lookup failed"));
        QVERIFY(switcher->itemData(0, Qt::ToolTipRole).toString().contains(QStringLiteral("Host lookup failed")));
        QCOMPARE(switcher->itemText(0), QStringLiteral("Work (%1)").arg(Common::AccountStatusTracker::label(Common::AccountStatusKind::NetworkError)));
        QVERIFY(switcher->itemData(1, Qt::ToolTipRole).toString().isEmpty());
        // The footer names the background account and its error
        QVERIFY(!alerts->isHidden());
        QVERIFY(alerts->text().contains(QStringLiteral("Work")));
        QVERIFY(alerts->toolTip().contains(QStringLiteral("Host lookup failed")));

        emit workModel->connectionStateChanged(1u, Imap::CONN_STATE_AUTHENTICATED);
        QVERIFY(switcher->itemData(0, Qt::ToolTipRole).toString().isEmpty());
        QCOMPARE(switcher->itemText(0), QStringLiteral("Work"));
        QCOMPARE(switcher->currentText(), QStringLiteral("Home"));
        QVERIFY(alerts->isHidden());

        // Clicking the footer switches to the flagged account, which then reports its error itself
        emit workModel->authAttemptFailed(QStringLiteral("Rejected password"));
        QVERIFY(!alerts->isHidden());
        alerts->click();
        QCOMPARE(switcher->currentText(), QStringLiteral("Work (%1)").arg(Common::AccountStatusTracker::label(Common::AccountStatusKind::AuthFailed)));
        QCOMPARE(window.imapModel(), workModel);
        QVERIFY(alerts->isHidden());
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
        // The migrated account caches directly in the cache root, and the other one in its own directory.
        const QString cacheRoot = Common::writablePath(Common::LOCATION_CACHE);
        const QStringList cachedFiles{QStringLiteral("imap.cache.sqlite"), QStringLiteral("account-2/imap.cache.sqlite"),
                                      QStringLiteral("account-2/SU5CT1g=/3_1.cache")};
        for (const QString &name : cachedFiles) {
            QVERIFY(QDir().mkpath(QFileInfo(cacheRoot + name).absolutePath()));
            QFile file(cacheRoot + name);
            QVERIFY(file.open(QIODevice::WriteOnly));
        }

        QVERIFY(QMetaObject::invokeMethod(&window, "removeAccountNow", Q_ARG(QString, QStringLiteral("account-2"))));
        QVERIFY(!QFileInfo::exists(cacheRoot + QStringLiteral("account-2")));
        QVERIFY(QFile::exists(cacheRoot + QStringLiteral("imap.cache.sqlite")));
        QTRY_VERIFY(!settings.contains(QStringLiteral("accountPasswords/account-2/imap")));
        QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("main-account-2.conf"))));
        QCOMPARE(settings.value(QStringLiteral("accounts/ids")).toStringList(), QStringList{QStringLiteral("account-1")});
        auto *switcher = accountSwitcher(window);
        QVERIFY(switcher);
        QCOMPARE(switcher->count(), 1);
        QCOMPARE(window.imapAccess()->server(), QStringLiteral("work.example.test"));

        QVERIFY(QMetaObject::invokeMethod(&window, "removeAccountNow", Q_ARG(QString, QStringLiteral("account-1"))));
        QCOMPARE(switcher->count(), 1);
        // The last account cannot be removed, so its cache stays.
        QVERIFY(QFile::exists(cacheRoot + QStringLiteral("imap.cache.sqlite")));
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

#ifdef TROJITA_HAVE_WEBENGINE
    void focusFollowsScreenOrder()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);

        Gui::MainWindow window(&settings);
        window.resize(1400, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        const auto action = [&window](const QString &name) {
            auto *result = window.findChild<QAction *>(name);
            Q_ASSERT(result);
            return result;
        };
        auto *mailboxes = window.findChild<Gui::MailBoxTreeView *>();
        auto *messages = window.findChild<Gui::MsgListView *>();
        auto *reader = window.findChild<Gui::CompleteMessageWidget *>();
        auto *switcher = accountSwitcher(window);
        QVERIFY(mailboxes && messages && reader && switcher);
        QVERIFY(switcher->isVisible());

        const auto in = [&window](QWidget *container) {
            QWidget *focus = window.focusWidget();
            return focus && (focus == container || container->isAncestorOf(focus));
        };
        // The panes visited by F6 from the message list until it comes back
        const auto cycle = [&]() {
            QList<QWidget *> order;
            action(QStringLiteral("action_focus_msg_list"))->trigger();
            for (int i = 0; i < 12; ++i) {
                action(QStringLiteral("action_focus_next_region"))->trigger();
                if (in(messages))
                    break;
                for (QWidget *pane : {static_cast<QWidget *>(mailboxes), static_cast<QWidget *>(reader),
                                      static_cast<QWidget *>(switcher)}) {
                    if (in(pane))
                        order << pane;
                }
            }
            return order;
        };
        QVERIFY(QMetaObject::invokeMethod(&window, "slotLayoutWide"));
        QCOMPARE(cycle(), (QList<QWidget *>{reader, switcher, mailboxes}));

        // Folders on the right come after the reader, before the cycle starts again at the top
        action(QStringLiteral("mailboxSidebarRight"))->trigger();
        QTRY_VERIFY(mailboxes->mapToGlobal(QPoint()).x() > reader->mapToGlobal(QPoint()).x());
        QCOMPARE(cycle(), (QList<QWidget *>{reader, mailboxes, switcher}));
        action(QStringLiteral("action_focus_msg_list"))->trigger();
        action(QStringLiteral("action_focus_previous_region"))->trigger();
        QVERIFY(in(switcher));
        action(QStringLiteral("action_focus_previous_region"))->trigger();
        QCOMPARE(window.focusWidget(), mailboxes);

        // Right to left: the panes are visited from the right edge, then the header
        window.setLayoutDirection(Qt::RightToLeft);
        QTRY_VERIFY(mailboxes->mapToGlobal(QPoint()).x() < reader->mapToGlobal(QPoint()).x());
        QList<QWidget *> fromRight{messages, reader, mailboxes};
        std::sort(fromRight.begin(), fromRight.end(), [](QWidget *a, QWidget *b) {
            return a->mapToGlobal(QPoint(a->width(), 0)).x() > b->mapToGlobal(QPoint(b->width(), 0)).x();
        });
        const qsizetype list = fromRight.indexOf(messages);
        const QList<QWidget *> expected = fromRight.mid(list + 1) + QList<QWidget *>{switcher} + fromRight.mid(0, list);
        QCOMPARE(cycle(), expected);
        window.setLayoutDirection(Qt::LeftToRight);
        action(QStringLiteral("mailboxSidebarLeft"))->trigger();
    }

    void escapeReturnsToMessageList()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);

        Gui::MainWindow window(&settings);
        window.resize(1400, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        auto *messages = window.findChild<Gui::MsgListView *>();
        auto *reader = window.findChild<Gui::CompleteMessageWidget *>();
        auto *contact = window.findChild<Gui::ContactPanel *>();
        auto *bar = window.findChild<Gui::ReaderActionBar *>(QStringLiteral("readerActions"));
        auto *search = window.findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        QVERIFY(messages && reader && contact && bar && search);
        // Without a message the panels show a placeholder; each panel is still a scope of its own
        QVERIFY(!contact->findChildren<QShortcut *>(QStringLiteral("escapeToMessageList")).isEmpty());

        reader->messageView->setFocus();
        QVERIFY(reader->messageView->hasFocus() || reader->messageView->isAncestorOf(window.focusWidget()));
        QTest::keyClick(window.focusWidget(), Qt::Key_Escape);
        QCOMPARE(window.focusWidget(), messages);

        // The bar is hidden without a message; its buttons are still in the scope
        QVERIFY(!bar->findChildren<QShortcut *>(QStringLiteral("escapeToMessageList")).isEmpty());

        // The quick search keeps its own Escape, which also ends in the list
        search->setEnabled(true);
        search->setFocus();
        QTest::keyClick(search, Qt::Key_Escape);
        QCOMPARE(window.focusWidget(), messages);
    }
#endif

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

    void checkMailAndNextUnreadFolder()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        {
            // All accounts open their disk cache under the same database connection name
            QSettings second(directory.filePath(QStringLiteral("main-account-2.conf")), QSettings::IniFormat);
            second.setValue(Common::SettingsNames::cacheOfflineKey, Common::SettingsNames::cacheOfflineNone);
        }
        QVERIFY(seedFirstAccountCache());
        Gui::MainWindow window(&settings);

        auto *checkMail = window.findChild<QAction *>(QStringLiteral("action_check_mail"));
        auto *nextUnread = window.findChild<QAction *>(QStringLiteral("action_go_to_next_unread_folder"));
        QVERIFY(checkMail);
        QVERIFY(nextUnread);
        QCOMPARE(checkMail->shortcut(), QKeySequence(Qt::Key_F5));
        QCOMPARE(nextUnread->shortcut(), QKeySequence(QStringLiteral("Ctrl+Alt+N")));
        for (const QKeySequence &key : {checkMail->shortcut(), nextUnread->shortcut()}) {
            int uses = 0;
            for (QAction *action : window.findChildren<QAction *>()) {
                if (action->shortcuts().contains(key))
                    ++uses;
            }
            QCOMPARE(uses, 1);
        }
        bool inMailboxMenu = false;
        for (QAction *menu : window.menuBar()->actions()) {
            if (menu->menu() && menu->menu()->actions().contains(checkMail) && menu->menu()->actions().contains(nextUnread))
                inMailboxMenu = true;
        }
        QVERIFY(inMailboxMenu);
        // Checking needs an account that is online, which these are not; it must not disable itself either
        checkMail->trigger();
        QVERIFY(checkMail->isEnabled());

        // The sidebar header of the first account shows its cached unread INBOX count
        auto *sidebar = window.findChild<Gui::AccountSidebar *>();
        QVERIFY(sidebar);
        QTRY_COMPARE(sidebar->unreadCount(QStringLiteral("account-1")), 5);
        QCOMPARE(sidebar->headers().at(0)->text(), QStringLiteral("Work (5)"));
        QCOMPARE(sidebar->headers().at(0)->accessibleName(), QStringLiteral("Work, 5 unread"));
        QCOMPARE(sidebar->unreadCount(QStringLiteral("account-2")), -1);

        // Next Unread Folder walks the cached folders that have unread messages, and wraps around
        auto *mailboxes = window.findChild<Gui::MailBoxTreeView *>();
        QVERIFY(mailboxes);
        auto openMailbox = [&window]() {
            auto *msgList = qobject_cast<Imap::Mailbox::MsgListModel *>(window.imapAccess()->msgListModel());
            return msgList ? msgList->currentMailbox().data(Imap::Mailbox::RoleMailboxName).toString() : QString();
        };
        mailboxes->restoreMailbox(QStringLiteral("INBOX"));
        QTRY_COMPARE(openMailbox(), QStringLiteral("INBOX"));
        QTRY_VERIFY(window.imapModel()->mailboxIndexByName(QStringLiteral("Archive"))
                        .data(Imap::Mailbox::RoleUnreadMessageCount).isValid());
        nextUnread->trigger();
        QTRY_COMPARE(openMailbox(), QStringLiteral("Archive"));
        nextUnread->trigger();
        QTRY_COMPARE(openMailbox(), QStringLiteral("INBOX"));
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
        auto *rows = window.findChild<Gui::MessageRowsMenu *>();
        auto *rowsView = window.findChild<Gui::MsgListView *>();
        QVERIFY(rows);
        QVERIFY(rowsView);
        rows->setMultiLine(true);
        QVERIFY(rowsView->multiLineRows());
        QVERIFY(rowsView->isHeaderHidden());
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
        QVERIFY(!rows->isMultiLine());
        QVERIFY(!rowsView->multiLineRows());
        QVERIFY(!rowsView->isHeaderHidden());
        for (QAction *panel : std::as_const(panels))
            QVERIFY2(!panel->isChecked(), qPrintable(panel->objectName()));
        if (attachments)
            QVERIFY(attachments->isChecked());
        QVERIFY(KConfigGroup(KSharedConfig::openStateConfig(), QStringLiteral("DenseProcessing")).readEntry("Enabled", false));

        dense->trigger();
        QVERIFY(!dense->isChecked());
        QVERIFY(density->isComfortable());
        QVERIFY(rows->isMultiLine());
        QVERIFY(rowsView->multiLineRows());
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

    void folderViewStateOffline()
    {
        using Imap::Mailbox::GroupingMsgListModel;
        using UiUtils::ViewStateStore;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        {
            // All accounts open their disk cache under the same database connection name, so the second account
            // would take the connection of the first one away
            QSettings second(directory.filePath(QStringLiteral("main-account-2.conf")), QSettings::IniFormat);
            second.setValue(Common::SettingsNames::cacheOfflineKey, Common::SettingsNames::cacheOfflineNone);
        }
        QVERIFY(seedFirstAccountCache());
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
        auto openMailbox = [&window]() {
            auto *msgList = qobject_cast<Imap::Mailbox::MsgListModel *>(window.imapAccess()->msgListModel());
            return msgList ? msgList->currentMailbox().data(Imap::Mailbox::RoleMailboxName).toString() : QString();
        };
        auto *list = window.findChild<Gui::MessageListWidget *>();
        auto *tree = list->tree;
        auto *mailboxes = window.findChild<Gui::MailBoxTreeView *>();
        QVERIFY(mailboxes);
        auto *scope = window.findChild<QLabel *>(QStringLiteral("messageListScope"));
        QVERIFY(scope);
        auto *forAll = action(QStringLiteral("messageListViewForAllFolders"));
        auto *resetFolder = action(QStringLiteral("messageListResetFolderView"));
        QVERIFY(forAll);
        QVERIFY(resetFolder);
        auto *sortMenu = window.findChild<QMenu *>(QStringLiteral("sortMenu"));
        QVERIFY(sortMenu);
        QVERIFY(sortMenu->actions().contains(forAll));
        QVERIFY(sortMenu->actions().contains(resetFolder));
        QAction *descending = nullptr;
        for (QAction *candidate : sortMenu->actions()) {
            if (candidate->text() == QLatin1String("&Descending"))
                descending = candidate;
        }
        QVERIFY(descending);
        auto messageIndex = [tree](uint uid) {
            const QAbstractItemModel *model = tree->model();
            const auto matches = model->match(model->index(0, 0), Imap::Mailbox::RoleMessageUid, uid, 1,
                                              Qt::MatchExactly | Qt::MatchRecursive);
            return matches.isEmpty() ? QModelIndex() : matches.constFirst();
        };
        auto goTo = [&](const QString &mailbox) {
            mailboxes->restoreMailbox(mailbox);
            QTRY_COMPARE(openMailbox(), mailbox);
            QTRY_COMPARE(prettyModel()->rowCount(), 5);
        };
        auto storedViews = [&window]() {
            ViewStateStore store;
            store.load(*window.accountSettings());
            return store;
        };
        auto answerConfirmation = [&window](QMessageBox::ButtonRole role) {
            QTimer::singleShot(0, &window, [&window, role]() {
                auto *box = window.findChild<QMessageBox *>(QStringLiteral("messageListViewForAllFoldersConfirmation"));
                QVERIFY(box);
                for (QAbstractButton *button : box->buttons()) {
                    if (box->buttonRole(button) == role) {
                        button->click();
                        return;
                    }
                }
                box->reject();
            });
        };

        // Without an open folder there is no scope and nothing to apply to the folders
        QVERIFY(scope->isHidden());
        QVERIFY(!forAll->isEnabled());
        QVERIFY(!resetFolder->isEnabled());

        goTo(QStringLiteral("INBOX"));
        QCOMPARE(scope->text(), QStringLiteral("Work › INBOX"));
        QVERIFY(!scope->isHidden());
        QVERIFY(scope->accessibleName().contains(QStringLiteral("INBOX")));
        QVERIFY(scope->accessibleName().contains(QStringLiteral("Work")));
        QVERIFY(forAll->isEnabled());
        QVERIFY(!resetFolder->isEnabled());
        QCOMPARE(tree->model(), prettyModel());

        // The arrangement chosen in a folder belongs to that folder only
        action(QStringLiteral("messageListGroupByDate"))->trigger();
        QCOMPARE(tree->model(), groupingModel());
        QCOMPARE(groupingModel()->grouping(), GroupingMsgListModel::Grouping::Date);
        descending->trigger();
        QCOMPARE(tree->header()->sortIndicatorOrder(), Qt::DescendingOrder);
        QVERIFY(resetFolder->isEnabled());
        QVERIFY(window.accountSettings()->contains(ViewStateStore::folderViewsKey));
        QVERIFY(!window.accountSettings()->contains(Common::SettingsNames::guiMsgListGroupBy));
        QVERIFY(!settings.contains(Common::SettingsNames::guiMsgListShowThreading));
        {
            const auto inbox = storedViews().folderView(QStringLiteral("INBOX"));
            QVERIFY(inbox);
            QCOMPARE(inbox->groupBy, GroupingMsgListModel::groupingName(GroupingMsgListModel::Grouping::Date));
            QCOMPARE(inbox->sortOrder, Qt::DescendingOrder);
            QCOMPARE(storedViews().scope(QStringLiteral("Archive")), ViewStateStore::Scope::Global);
        }

        // The current message of the folder comes back with it
        QTRY_VERIFY(messageIndex(3).isValid());
        tree->selectionModel()->setCurrentIndex(messageIndex(3),
                                                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        QCOMPARE(tree->currentIndex().data(Imap::Mailbox::RoleMessageUid).toUInt(), 3u);

        // Another folder without its own arrangement looks like all folders by default
        goTo(QStringLiteral("Archive"));
        QCOMPARE(scope->text(), QStringLiteral("Work › Archive"));
        QCOMPARE(tree->model(), prettyModel());
        QVERIFY(action(QStringLiteral("messageListGroupByNone"))->isChecked());
        QCOMPARE(tree->header()->sortIndicatorOrder(), Qt::AscendingOrder);
        QVERIFY(!descending->isChecked());
        QVERIFY(!resetFolder->isEnabled());

        goTo(QStringLiteral("INBOX"));
        QCOMPARE(tree->model(), groupingModel());
        QCOMPARE(groupingModel()->grouping(), GroupingMsgListModel::Grouping::Date);
        QVERIFY(descending->isChecked());
        QCOMPARE(tree->header()->sortIndicatorOrder(), Qt::DescendingOrder);
        QTRY_COMPARE(tree->currentIndex().data(Imap::Mailbox::RoleMessageUid).toUInt(), 3u);
        QVERIFY(tree->selectionModel()->isSelected(tree->currentIndex()));

        // Declining the confirmation changes nothing
        answerConfirmation(QMessageBox::RejectRole);
        forAll->trigger();
        QVERIFY(resetFolder->isEnabled());
        QCOMPARE(storedViews().scope(QStringLiteral("Archive")), ViewStateStore::Scope::Global);

        // The view of this folder becomes the view of all folders of the account
        answerConfirmation(QMessageBox::AcceptRole);
        forAll->trigger();
        QVERIFY(!resetFolder->isEnabled());
        QVERIFY(storedViews().folders().isEmpty());
        QCOMPARE(storedViews().scope(QStringLiteral("INBOX")), ViewStateStore::Scope::Account);
        QCOMPARE(window.accountSettings()->value(Common::SettingsNames::guiMsgListGroupBy).toString(),
                 GroupingMsgListModel::groupingName(GroupingMsgListModel::Grouping::Date));
        goTo(QStringLiteral("Archive"));
        QCOMPARE(tree->model(), groupingModel());
        QCOMPARE(groupingModel()->grouping(), GroupingMsgListModel::Grouping::Date);
        QVERIFY(descending->isChecked());

        // A folder which has its own view again can go back to the view of the account
        action(QStringLiteral("messageListGroupBySender"))->trigger();
        QCOMPARE(groupingModel()->grouping(), GroupingMsgListModel::Grouping::Sender);
        QVERIFY(resetFolder->isEnabled());
        QCOMPARE(storedViews().folders(), QStringList{QStringLiteral("Archive")});
        resetFolder->trigger();
        QCOMPARE(groupingModel()->grouping(), GroupingMsgListModel::Grouping::Date);
        QVERIFY(action(QStringLiteral("messageListGroupByDate"))->isChecked());
        QVERIFY(!resetFolder->isEnabled());
        QVERIFY(storedViews().folders().isEmpty());

        // The other account has its own views and no folder open
        auto *nextAccount = action(QStringLiteral("action_next_account"));
        nextAccount->trigger();
        QCOMPARE(window.imapAccess()->credentialAccountId(), QStringLiteral("account-2"));
        QVERIFY(scope->isHidden());
        QCOMPARE(tree->model(), prettyModel());
        QVERIFY(action(QStringLiteral("messageListGroupByNone"))->isChecked());
        QVERIFY(!descending->isChecked());
        QVERIFY(!forAll->isEnabled());

        nextAccount->trigger();
        QCOMPARE(settings.value(QStringLiteral("accounts/active")).toString(), QStringLiteral("account-1"));
        QCOMPARE(openMailbox(), QStringLiteral("Archive"));
        QCOMPARE(scope->text(), QStringLiteral("Work › Archive"));
        QCOMPARE(tree->model(), groupingModel());
        QCOMPARE(groupingModel()->grouping(), GroupingMsgListModel::Grouping::Date);
        QVERIFY(descending->isChecked());

        // The position of INBOX was kept for the next session as well
        QVERIFY(QMetaObject::invokeMethod(&window, "flushUiState"));
        const QString positions = KConfigGroup(KSharedConfig::openStateConfig(), QStringLiteral("MessageListPositions"))
                                      .readEntry(window.imapAccess()->credentialAccountId(), QString());
        UiUtils::FolderPositionStore stored;
        stored.loadJson(positions);
        const auto inboxPosition = stored.position(QStringLiteral("INBOX"));
        QVERIFY(inboxPosition);
        QCOMPARE(inboxPosition->uidValidity, 7u);
        QCOMPARE(inboxPosition->uid, 3u);
    }

    void unifiedInboxOffline()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        {
            // All accounts open their disk cache under the same database connection name
            QSettings second(directory.filePath(QStringLiteral("main-account-2.conf")), QSettings::IniFormat);
            second.setValue(Common::SettingsNames::cacheOfflineKey, Common::SettingsNames::cacheOfflineNone);
        }
        QVERIFY(seedFirstAccountCache());
        auto unifiedState = []() {
            return KConfigGroup(KSharedConfig::openStateConfig(), QStringLiteral("UnifiedInbox")).readEntry("Enabled", false);
        };
        auto excludedAccounts = []() {
            KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
            config->reparseConfiguration();
            return KConfigGroup(config, QStringLiteral("UnifiedInbox")).readEntry("ExcludedAccounts", QStringList());
        };
        QVERIFY(!unifiedState());

        {
            Gui::MainWindow window(&settings);
            auto action = [&window](const QString &name) {
                return window.findChild<QAction *>(name);
            };
            auto *unified = action(QStringLiteral("action_unified_inbox"));
            QVERIFY(unified);
            QVERIFY(unified->isCheckable());
            QVERIFY(!unified->isChecked());
            QCOMPARE(unified->shortcut(), QKeySequence(QStringLiteral("Ctrl+Shift+I")));
            int uses = 0;
            for (QAction *candidate : window.findChildren<QAction *>()) {
                if (candidate->shortcuts().contains(unified->shortcut()))
                    ++uses;
            }
            QCOMPARE(uses, 1);
            auto *accountsMenu = window.findChild<QMenu *>(QStringLiteral("unifiedInboxAccountsMenu"));
            QVERIFY(accountsMenu);
            bool inViewMenu = false;
            for (QAction *menu : window.menuBar()->actions()) {
                if (menu->menu() && menu->menu()->actions().contains(unified) &&
                    menu->menu()->actions().contains(accountsMenu->menuAction()))
                    inViewMenu = true;
            }
            QVERIFY(inViewMenu);

            auto *messages = window.findChild<Gui::MessageListWidget *>();
            QVERIFY(messages);
            auto *tree = messages->tree;
            auto *mailboxes = window.findChild<Gui::MailBoxTreeView *>();
            QVERIFY(mailboxes);
            auto *scope = window.findChild<QLabel *>(QStringLiteral("messageListScope"));
            auto *status = window.findChild<QLabel *>(QStringLiteral("messageListStatus"));
            QVERIFY(scope);
            QVERIFY(status);
            auto *groupByMenu = window.findChild<QMenu *>(QStringLiteral("messageListGroupByMenu"));
            QVERIFY(groupByMenu);
            QAction *threading = nullptr;
            for (QAction *candidate : window.findChildren<QAction *>()) {
                if (candidate->text() == Gui::MainWindow::tr("Show Messages in &Threads"))
                    threading = candidate;
            }
            QVERIFY(threading);
            auto prettyModel = [&window]() {
                return window.findChild<Imap::Mailbox::PrettyMsgListModel *>(QStringLiteral("prettyMsgListModel"));
            };
            auto openMailbox = [&window]() {
                auto *msgList = qobject_cast<Imap::Mailbox::MsgListModel *>(window.imapAccess()->msgListModel());
                return msgList ? msgList->currentMailbox().data(Imap::Mailbox::RoleMailboxName).toString() : QString();
            };
            auto *nextAccount = action(QStringLiteral("action_next_account"));
            auto *moveMenu = window.findChild<QMenu *>(QStringLiteral("moveToMenu"));
            QVERIFY(moveMenu);
            auto offeredFolders = [moveMenu]() {
                emit moveMenu->aboutToShow();
                QStringList offered;
                for (QAction *candidate : moveMenu->actions()) {
                    if (!candidate->isSeparator() && candidate->objectName().isEmpty())
                        offered << candidate->data().toString();
                }
                return offered;
            };

            // The first account remembers a destination of its own, then the other account becomes the active one
            mailboxes->restoreMailbox(QStringLiteral("INBOX"));
            QTRY_COMPARE(openMailbox(), QStringLiteral("INBOX"));
            QTRY_VERIFY(window.imapModel()->mailboxIndexByName(QStringLiteral("Archive")).isValid());
            QVERIFY(QMetaObject::invokeMethod(&window, "rememberRecentFolder", Q_ARG(QString, QStringLiteral("Archive"))));
            QCOMPARE(offeredFolders(), QStringList{QStringLiteral("Archive")});
            nextAccount->trigger();
            QCOMPARE(window.imapAccess()->credentialAccountId(), QStringLiteral("account-2"));
            QVERIFY(scope->isHidden());
            QVERIFY(offeredFolders().isEmpty());
            const bool threadingWasEnabled = threading->isEnabled();

            // The INBOX of every account in one list, even of an account which is not the active one
            unified->trigger();
            QVERIFY(unified->isChecked());
            QVERIFY(unifiedState());
            QCOMPARE(tree->model()->objectName(), QStringLiteral("prettyUnifiedMsgListModel"));
            QTRY_COMPARE(tree->model()->rowCount(), 5);
            QCOMPARE(scope->text(), QStringLiteral("Unified Inbox"));
            QVERIFY(!scope->isHidden());
            QVERIFY(scope->toolTip().contains(QStringLiteral("Work")));
            QVERIFY(scope->toolTip().contains(QStringLiteral("Home")));
            QTRY_VERIFY2(status->text().contains(QStringLiteral("5 messages")), qPrintable(status->text()));
            QVERIFY2(status->text().contains(QStringLiteral("5 unread")), qPrintable(status->text()));
            // Neither account is online, but the cached messages are listed
            QVERIFY2(status->text().contains(QStringLiteral("offline: ")), qPrintable(status->text()));
            QVERIFY(status->text().contains(QStringLiteral("Home")));

            // Threads, groups and folder views belong to one folder
            QVERIFY(!threading->isEnabled());
            QVERIFY(!groupByMenu->menuAction()->isEnabled());
            QVERIFY(!action(QStringLiteral("messageListViewForAllFolders"))->isEnabled());
            QVERIFY(!action(QStringLiteral("messageListResetFolderView"))->isEnabled());

            // Actions on a message use the account which the message belongs to
            tree->selectionModel()->setCurrentIndex(tree->model()->index(0, 0),
                                                    QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            QCOMPARE(offeredFolders(), QStringList{QStringLiteral("Archive")});
            QCOMPARE(window.imapAccess()->credentialAccountId(), QStringLiteral("account-2"));

            // Accounts can be left out, but not all of them
            emit accountsMenu->aboutToShow();
            QCOMPARE(accountsMenu->actions().size(), 2);
            QAction *work = accountsMenu->actions().at(0);
            QAction *home = accountsMenu->actions().at(1);
            QCOMPARE(work->data().toString(), QStringLiteral("account-1"));
            QCOMPARE(home->data().toString(), QStringLiteral("account-2"));
            QVERIFY(work->isChecked() && work->isEnabled());
            QVERIFY(home->isChecked() && home->isEnabled());
            work->setChecked(false);
            QCOMPARE(excludedAccounts(), QStringList{QStringLiteral("account-1")});
            QTRY_COMPARE(tree->model()->rowCount(), 0);
            QVERIFY(!scope->toolTip().contains(QStringLiteral("Work")));
            emit accountsMenu->aboutToShow();
            work = accountsMenu->actions().at(0);
            home = accountsMenu->actions().at(1);
            QVERIFY(!work->isChecked() && work->isEnabled());
            QVERIFY(home->isChecked() && !home->isEnabled());
            work->setChecked(true);
            QVERIFY(excludedAccounts().isEmpty());
            QTRY_COMPARE(tree->model()->rowCount(), 5);

            // Leaving it restores the view of the active account
            unified->trigger();
            QVERIFY(!unified->isChecked());
            QVERIFY(!unifiedState());
            QCOMPARE(tree->model(), prettyModel());
            QCOMPARE(threading->isEnabled(), threadingWasEnabled);
            QVERIFY(groupByMenu->menuAction()->isEnabled());
            QVERIFY(scope->isHidden());
            QCOMPARE(window.imapAccess()->credentialAccountId(), QStringLiteral("account-2"));

            // Switching to another account leaves it as well
            unified->trigger();
            QVERIFY(unified->isChecked());
            nextAccount->trigger();
            QCOMPARE(settings.value(QStringLiteral("accounts/active")).toString(), QStringLiteral("account-1"));
            QVERIFY(!unified->isChecked());
            QCOMPARE(tree->model(), prettyModel());
            QTRY_COMPARE(openMailbox(), QStringLiteral("INBOX"));
            QCOMPARE(scope->text(), QStringLiteral("Work › INBOX"));

            // So does choosing a folder
            unified->trigger();
            QVERIFY(unified->isChecked());
            QCOMPARE(scope->text(), QStringLiteral("Unified Inbox"));
            const QModelIndex archive = mailboxes->model()->match(mailboxes->model()->index(0, 0), Imap::Mailbox::RoleMailboxName,
                                                                  QStringLiteral("Archive"), 1, Qt::MatchExactly | Qt::MatchRecursive)
                                            .value(0);
            QVERIFY(archive.isValid());
            emit mailboxes->clicked(archive);
            QVERIFY(!unified->isChecked());
            QVERIFY(!unifiedState());
            QCOMPARE(tree->model(), prettyModel());

            unified->trigger();
            QVERIFY(unifiedState());
        }
    }

    void unifiedInboxIsRestored()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        {
            QSettings second(directory.filePath(QStringLiteral("main-account-2.conf")), QSettings::IniFormat);
            second.setValue(Common::SettingsNames::cacheOfflineKey, Common::SettingsNames::cacheOfflineNone);
        }
        QVERIFY(seedFirstAccountCache());
        {
            KConfigGroup state(KSharedConfig::openStateConfig(), QStringLiteral("UnifiedInbox"));
            state.writeEntry("Enabled", true);
            QVERIFY(state.sync());
        }
        Gui::MainWindow window(&settings);
        auto *unified = window.findChild<QAction *>(QStringLiteral("action_unified_inbox"));
        QVERIFY(unified);
        QVERIFY(unified->isChecked());
        auto *tree = window.findChild<Gui::MessageListWidget *>()->tree;
        QCOMPARE(tree->model()->objectName(), QStringLiteral("prettyUnifiedMsgListModel"));
        QTRY_COMPARE(tree->model()->rowCount(), 5);
        QCOMPARE(window.findChild<QLabel *>(QStringLiteral("messageListScope"))->text(), QStringLiteral("Unified Inbox"));
        // Leaving it shows the list of the active account again
        unified->trigger();
        QVERIFY(!unified->isChecked());
        QCOMPARE(tree->model()->objectName(), QStringLiteral("prettyMsgListModel"));
    }

    void identitiesFollowTheAccount()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        {
            Composer::SenderIdentitiesModel legacy;
            legacy.appendIdentity(Composer::ItemSenderIdentity(QStringLiteral("Pat"), QStringLiteral("pat@example.test"),
                                                               QString(), QString()));
            legacy.saveToSettings(settings);
        }

        Gui::MainWindow window(&settings);
        auto *shared = window.sharedIdentitiesModel();
        auto *work = window.senderIdentitiesModel();
        QVERIFY(work != shared);
        QCOMPARE(work, window.accountIdentitiesModel());
        // Existing accounts start with a copy of the identities they used before.
        QCOMPARE(work->rowCount(), 1);
        QCOMPARE(work->index(0, Composer::SenderIdentitiesModel::COLUMN_EMAIL).data().toString(),
                 QStringLiteral("pat@example.test"));
        QVERIFY(!Composer::AccountIdentities::usesSharedIdentities(*window.accountSettings()));

        work->appendIdentity(Composer::ItemSenderIdentity(QStringLiteral("Pat"), QStringLiteral("pat@work.test"),
                                                          QString(), QString()));
        window.findChild<QAction *>(QStringLiteral("action_next_account"))->trigger();
        QCOMPARE(window.imapAccess()->credentialAccountId(), QStringLiteral("account-2"));
        auto *home = window.senderIdentitiesModel();
        QVERIFY(home != work);
        QVERIFY(home != shared);
        QCOMPARE(home->rowCount(), 1);
        QCOMPARE(shared->rowCount(), 1);

        Composer::AccountIdentities::setUsesSharedIdentities(*window.accountSettings(), true);
        QCOMPARE(window.senderIdentitiesModel(), shared);
        QCOMPARE(window.accountIdentitiesModel(), home);

        window.findChild<QAction *>(QStringLiteral("action_previous_account"))->trigger();
        QCOMPARE(window.senderIdentitiesModel(), work);
        QCOMPARE(work->rowCount(), 2);

        // An account without an identity offers none to the composer but keeps its own for later.
        Composer::AccountIdentities::setSource(*window.accountSettings(), Composer::AccountIdentities::Source::None);
        auto *none = window.senderIdentitiesModel();
        QVERIFY(none != work);
        QVERIFY(none != shared);
        QCOMPARE(none->rowCount(), 0);
        QCOMPARE(window.accountIdentitiesModel(), work);
        QCOMPARE(work->rowCount(), 2);
        Composer::AccountIdentities::setSource(*window.accountSettings(), Composer::AccountIdentities::Source::Own);
        QCOMPARE(window.senderIdentitiesModel(), work);
    }

    void composerSendsWithItsOwnAccount()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        sendThroughScript(directory, QStringLiteral("main-account-1.conf"), QStringLiteral("work.eml"));
        {
            // The other account would file the message in Sent on its offline server, so sending would not finish.
            QSettings home(directory.filePath(QStringLiteral("main-account-2.conf")), QSettings::IniFormat);
            home.setValue(Common::SettingsNames::msaMethodKey, Common::SettingsNames::methodSENDMAIL);
            home.setValue(Common::SettingsNames::sendmailKey, QStringLiteral("/bin/false"));
            home.setValue(Common::SettingsNames::composerSaveToImapKey, true);
            home.setValue(Common::SettingsNames::composerImapSentKey, QStringLiteral("Home Sent"));
        }

        Gui::MainWindow window(&settings);
        QPointer<Gui::ComposeWidget> composer = openComposer(window);
        QVERIFY(composer);
        window.findChild<QAction *>(QStringLiteral("action_next_account"))->trigger();
        QCOMPARE(window.imapAccess()->credentialAccountId(), QStringLiteral("account-2"));

        QStringList errors;
        QTimer dialogs;
        connect(&dialogs, &QTimer::timeout, this, [&errors] {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                errors << box->text();
                box->reject();
            }
        });
        dialogs.start(20);
        QVERIFY(QMetaObject::invokeMethod(composer, "send"));
        QTRY_VERIFY2_WITH_TIMEOUT(QFile::exists(directory.filePath(QStringLiteral("work.eml"))),
                                  qPrintable(errors.join(QLatin1Char('\n'))), 10000);
        QTRY_VERIFY2_WITH_TIMEOUT(!composer, qPrintable(errors.join(QLatin1Char('\n'))), 10000);
        QVERIFY2(errors.isEmpty(), qPrintable(errors.join(QLatin1Char('\n'))));
        QFile sent(directory.filePath(QStringLiteral("work.eml")));
        QVERIFY(sent.open(QIODevice::ReadOnly));
        QVERIFY(sent.readAll().contains("Subject: Synthetic"));

        // A composer whose account was removed refuses to send instead of using another account.
        composer = openComposer(window);
        QVERIFY(composer);
        QVERIFY(QMetaObject::invokeMethod(&window, "removeAccountNow", Q_ARG(QString, QStringLiteral("account-2"))));
        QCOMPARE(settings.value(QStringLiteral("accounts/active")).toString(), QStringLiteral("account-1"));
        QFile::remove(directory.filePath(QStringLiteral("work.eml")));
        QVERIFY(QMetaObject::invokeMethod(composer, "send"));
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, 5000);
        QVERIFY2(errors.first().contains(QLatin1String("removed")), qPrintable(errors.first()));
        QVERIFY(composer);
        QTest::qWait(200);
        QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("work.eml"))));
        delete composer;
    }

    void oauthAccountsOfferSignIn()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        {
            QSettings work(directory.filePath(QStringLiteral("main-account-1.conf")), QSettings::IniFormat);
            work.setValue(Common::SettingsNames::imapAuthMethodKey, Common::SettingsNames::authMethodOAuth);
            work.setValue(Common::SettingsNames::oauthProviderKey, QStringLiteral("microsoft"));
            work.setValue(Common::SettingsNames::oauthClientIdKey, QStringLiteral("work-client"));
            work.setValue(Common::SettingsNames::imapUserKey, QStringLiteral("pat@example.test"));
        }
        // Never open a real browser
        QList<QUrl> opened;
        OAuth::Session::setDefaultBrowserLauncher([&](const QUrl &url) {
            opened << url;
            return false;
        });

        Gui::MainWindow window(&settings);
        auto *switcher = accountSwitcher(window);
        auto *signIn = window.findChild<QToolButton *>(QStringLiteral("oauthSignInButton"));
        QVERIFY(switcher && signIn);
        QVERIFY(signIn->isHidden());

        // Only OAuth accounts offer to sign in from their menu
        QMenu *menu = openAccountMenu(window, QStringLiteral("account-2"));
        QVERIFY(menu);
        QVERIFY(!menu->findChild<QAction *>(QStringLiteral("action_account_menu_sign_in")));
        menu = openAccountMenu(window, QStringLiteral("account-1"));
        QVERIFY(menu);
        QAction *menuSignIn = menu->findChild<QAction *>(QStringLiteral("action_account_menu_sign_in"));
        QVERIFY(menuSignIn);

        // A failed OAuth sign-in offers to sign in again from the footer
        emit window.imapModel()->authAttemptFailed(OAuth::errorMessage(OAuth::Error::Revoked));
        QVERIFY(!signIn->isHidden());

        // Signing in again reports its failure on the account, without a dialog: the fake browser cannot open, or
        // without an encrypted credential store, the sign-in fails before it would
        QTRY_VERIFY(window.pluginManager()->password() != nullptr);
        const bool encrypted =
            window.pluginManager()->password()->features() & Plugins::PasswordPlugin::FeatureEncryptedStorage;
        const QString failure =
            OAuth::errorMessage(encrypted ? OAuth::Error::BrowserUnavailable : OAuth::Error::StorageUnavailable);
        signIn->click();
        QTRY_VERIFY(switcher->itemData(0, Qt::ToolTipRole).toString().contains(failure));
        QCOMPARE(opened.size(), encrypted ? 1 : 0);
        if (encrypted) {
            const QUrlQuery query(opened.first());
            QCOMPARE(opened.first().host(), QStringLiteral("login.microsoftonline.com"));
            QCOMPARE(query.queryItemValue(QStringLiteral("client_id")), QStringLiteral("work-client"));
            QCOMPARE(query.queryItemValue(QStringLiteral("login_hint"), QUrl::FullyDecoded), QStringLiteral("pat@example.test"));
        }
        QVERIFY(!signIn->isHidden());
        for (auto *widget : QApplication::topLevelWidgets()) {
            auto *box = qobject_cast<QMessageBox *>(widget);
            QVERIFY2(!box || !box->isVisible(), qPrintable(box ? box->text() : QString()));
        }
        // The menu starts the same sign-in
        emit window.imapModel()->authAttemptFailed(OAuth::errorMessage(OAuth::Error::Revoked));
        QVERIFY(!switcher->itemData(0, Qt::ToolTipRole).toString().contains(failure));
        menu = openAccountMenu(window, QStringLiteral("account-1"));
        QVERIFY(menu);
        menuSignIn = menu->findChild<QAction *>(QStringLiteral("action_account_menu_sign_in"));
        QVERIFY(menuSignIn);
        menuSignIn->trigger();
        QTRY_VERIFY(switcher->itemData(0, Qt::ToolTipRole).toString().contains(failure));
        QCOMPARE(opened.size(), encrypted ? 2 : 0);

        // Removing an account at a provider without revocation tells where to remove its access
        menu = openAccountMenu(window, QStringLiteral("account-1"));
        QVERIFY(menu);
        QString question;
        QTimer poll;
        connect(&poll, &QTimer::timeout, [&]() {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                question = box->text();
                box->button(QMessageBox::No)->click();
            }
        });
        poll.start(20);
        menu->findChild<QAction *>(QStringLiteral("action_account_menu_remove"))->trigger();
        QTRY_VERIFY(!question.isEmpty());
        QVERIFY(question.contains(OAuth::provider(QStringLiteral("microsoft")).accessManagementPage.toString()));
        QCOMPARE(settings.value(QStringLiteral("accounts/ids")).toStringList().size(), 2);

        // A password account which failed does not offer it
        switcher->setCurrentIndex(1);
        emit switcher->activated(1);
        emit window.imapModel()->authAttemptFailed(QStringLiteral("Rejected password"));
        QVERIFY(signIn->isHidden());
        OAuth::Session::setDefaultBrowserLauncher(nullptr);
    }

    void newAccountsStartWithoutIdentities()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings shared(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        QSettings account(directory.filePath(QStringLiteral("main-account-3.conf")), QSettings::IniFormat);
        Composer::SenderIdentitiesModel legacy;
        legacy.appendIdentity(Composer::ItemSenderIdentity(QStringLiteral("Pat"), QStringLiteral("pat@example.test"),
                                                           QString(), QString()));
        legacy.saveToSettings(shared);

        Composer::AccountIdentities::initializeNewAccount(account);
        // A recorded choice stops the start-up migration from copying the shared identities.
        QVERIFY(!Composer::AccountIdentities::migrateAccount(shared, account));
        Composer::SenderIdentitiesModel identities;
        identities.loadFromSettings(account);
        QCOMPARE(identities.rowCount(), 0);
        QVERIFY(!Composer::AccountIdentities::usesSharedIdentities(account));
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
