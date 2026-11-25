/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/AccountStatus.h"
#include "Common/MetaTypes.h"
#include "Common/Paths.h"
#include "Common/SettingsNames.h"
#include "Composer/AccountIdentities.h"
#include "Composer/SenderIdentitiesModel.h"
#include "Gui/ComposeWidget.h"
#include "Gui/MailBoxTreeView.h"
#include "Gui/MessageListWidget.h"
#include "Gui/MsgListView.h"
#include "Gui/Window.h"
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/Model.h"
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
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

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
