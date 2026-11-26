/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Composer/AccountIdentities.h"
#include "Composer/SenderIdentitiesModel.h"
#include "Gui/SearchIndexDialog.h"
#include "Gui/Window.h"
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/SearchIndexer.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "OAuth/TokenSource.h"
#include "configure.cmake.h"
#ifdef TROJITA_HAVE_WEBENGINE
#include "Gui/WebEngineMessageProfile.h"
#endif
#include "static_plugins.h"

#include <KLocalizedString>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFile>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSqlDatabase>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <functional>

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
    settings.setValue(
        QStringLiteral("accounts/ids"), QStringList { QStringLiteral("account-1"), QStringLiteral("account-2") });
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

QStringList tabTitles(QDialog *dialog)
{
    QStringList titles;
    if (auto *tabs = dialog->findChild<QTabWidget *>()) {
        for (int i = 0; i < tabs->count(); ++i)
            titles << tabs->tabText(i).remove(QLatin1Char('&'));
    }
    return titles;
}

/** @short Run @arg handler on the first settings dialog that appears while @arg
 * trigger blocks in exec() */
bool handleDialog(const std::function<void()> &trigger, const std::function<void(QDialog *)> &handler)
{
    bool handled = false;
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, [&]() {
        auto *modal = QApplication::activeModalWidget();
        if (auto *input = qobject_cast<QInputDialog *>(modal)) {
            input->setTextValue(QStringLiteral("Travel"));
            input->accept();
            return;
        }
        auto *dialog = qobject_cast<QDialog *>(modal);
        if (handled || !dialog || !dialog->findChild<QTabWidget *>())
            return;
        handled = true;
        handler(dialog);
    });
    poll.start(20);
    trigger();
    return handled;
}

/** @short The tables of the SQLite file @arg path, read through a connection of their own */
QStringList tablesOf(const QString &path)
{
    const QString name = QStringLiteral("account-settings-test-tables");
    QStringList tables;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        db.setDatabaseName(path);
        if (db.open())
            tables = db.tables();
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
    return tables;
}

void answerQuestion(QMessageBox::StandardButton button)
{
    QTimer::singleShot(0, [button]() {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        QVERIFY(box);
        box->button(button)->click();
    });
}

void save(QDialog *dialog)
{
    auto *buttons = dialog->findChild<QDialogButtonBox *>();
    QVERIFY(buttons && buttons->button(QDialogButtonBox::Save));
    buttons->button(QDialogButtonBox::Save)->click();
}

} // namespace

class AccountSettingsDialogTest : public QObject {
    Q_OBJECT
private slots:
    void accountsKeepTheirOwnCaches()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        Gui::MainWindow window(&settings);
        QCoreApplication::processEvents();
        for (auto *widget : QApplication::topLevelWidgets()) {
            auto *box = qobject_cast<QMessageBox *>(widget);
            QVERIFY2(!box || !box->isVisible(), qPrintable(box ? box->text() : QString()));
        }
    }

    void applicationSettingsLeaveTheAccountAlone()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        Gui::MainWindow window(&settings);
        auto *access = window.imapAccess();
        auto *model = window.imapModel();

        QStringList titles;
        QVERIFY(handleDialog([&]() { QMetaObject::invokeMethod(&window, "slotShowSettings", Qt::DirectConnection); },
            [&](QDialog *dialog) {
                titles = tabTitles(dialog);
                save(dialog);
            }));
        QCOMPARE(titles, (QStringList { QStringLiteral("General"), QStringLiteral("Favorite tags") }));
        // Saving application settings must not reconnect or rebuild the account's
        // models.
        QCOMPARE(window.imapAccess(), access);
        QCOMPARE(window.imapModel(), model);
    }

    void accountSettingsEditOnlyTheActiveAccount()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        Gui::MainWindow window(&settings);
        window.findChild<QAction *>(QStringLiteral("action_next_account"))->trigger();
        QCOMPARE(window.imapAccess()->credentialAccountId(), QStringLiteral("account-2"));

        auto *action = window.findChild<QAction *>(QStringLiteral("action_account_settings"));
        QVERIFY(action);
        QStringList titles;
        QString windowTitle;
        QString shownHost;
        QVERIFY(handleDialog([&]() { action->trigger(); },
            [&](QDialog *dialog) {
                titles = tabTitles(dialog);
                windowTitle = dialog->windowTitle();
                auto *host = dialog->findChild<QLineEdit *>(QStringLiteral("imapHost"));
                QVERIFY(host);
                shownHost = host->text();
                host->setText(QStringLiteral("new-home.example.test"));
                auto *smtpHost = dialog->findChild<QLineEdit *>(QStringLiteral("smtpHost"));
                QVERIFY(smtpHost);
                smtpHost->setText(QStringLiteral("smtp.home.example.test"));
                auto *identitySource = dialog->findChild<QComboBox *>(QStringLiteral("identitySourceCombo"));
                QVERIFY(identitySource);
                QCOMPARE(identitySource->currentData().toInt(),
                    static_cast<int>(Composer::AccountIdentities::Source::Own));
                identitySource->setCurrentIndex(
                    identitySource->findData(static_cast<int>(Composer::AccountIdentities::Source::None)));

                // Password authentication stays the default; OAuth replaces the password with a provider sign-in.
                auto *authMethod = dialog->findChild<QComboBox *>(QStringLiteral("imapAuthMethod"));
                auto *provider = dialog->findChild<QComboBox *>(QStringLiteral("oauthProvider"));
                auto *clientId = dialog->findChild<QLineEdit *>(QStringLiteral("oauthClientId"));
                auto *password = dialog->findChild<QLineEdit *>(QStringLiteral("imapPass"));
                auto *smtpAuthMethod = dialog->findChild<QComboBox *>(QStringLiteral("smtpAuthMethod"));
                QVERIFY(authMethod && provider && clientId && password && smtpAuthMethod);
                QCOMPARE(authMethod->currentData().toString(), Common::SettingsNames::authMethodPassword);
                QCOMPARE(smtpAuthMethod->currentData().toString(), Common::SettingsNames::authMethodPassword);
                QVERIFY(provider->isHidden());
                QVERIFY(!password->isHidden());
                authMethod->setCurrentIndex(authMethod->findData(Common::SettingsNames::authMethodOAuth));
                QVERIFY(!provider->isHidden());
                QVERIFY(password->isHidden());
                provider->setCurrentIndex(provider->findData(QStringLiteral("microsoft")));
                clientId->setText(QStringLiteral("home-client"));
                save(dialog);
            }));
        QCOMPARE(titles,
            (QStringList { QStringLiteral("Identities"), QStringLiteral("IMAP"), QStringLiteral("Offline"),
                QStringLiteral("SMTP") }));
        QVERIFY(windowTitle.contains(QStringLiteral("Home")));
        QCOMPARE(shownHost, QStringLiteral("home.example.test"));
        QCOMPARE(window.imapAccess()->server(), QStringLiteral("new-home.example.test"));
        QSettings home(directory.filePath(QStringLiteral("main-account-2.conf")), QSettings::IniFormat);
        QCOMPARE(home.value(Common::SettingsNames::imapHostKey).toString(), QStringLiteral("new-home.example.test"));
        QSettings work(directory.filePath(QStringLiteral("main-account-1.conf")), QSettings::IniFormat);
        QCOMPARE(work.value(Common::SettingsNames::imapHostKey).toString(), QStringLiteral("work.example.test"));
        QVERIFY(!settings.contains(Common::SettingsNames::imapHostKey));
        // Only the edited account stops using an identity.
        QCOMPARE(Composer::AccountIdentities::source(home), Composer::AccountIdentities::Source::None);
        QCOMPARE(Composer::AccountIdentities::source(work), Composer::AccountIdentities::Source::Own);
        QCOMPARE(window.senderIdentitiesModel()->rowCount(), 0);
        // Only the edited account signs in with OAuth, and its SMTP keeps using a password.
        QCOMPARE(home.value(Common::SettingsNames::imapAuthMethodKey).toString(),
            Common::SettingsNames::authMethodOAuth);
        QCOMPARE(home.value(Common::SettingsNames::oauthProviderKey).toString(), QStringLiteral("microsoft"));
        QCOMPARE(home.value(Common::SettingsNames::oauthClientIdKey).toString(), QStringLiteral("home-client"));
        QVERIFY(!home.contains(Common::SettingsNames::smtpAuthMethodKey));
        for (const auto &key : { Common::SettingsNames::imapAuthMethodKey, Common::SettingsNames::smtpAuthMethodKey,
                 Common::SettingsNames::oauthProviderKey, Common::SettingsNames::oauthClientIdKey }) {
            QVERIFY(!work.contains(key));
            QVERIFY(!settings.contains(key));
        }
        QVERIFY(OAuth::TokenSource::find(window.imapModel()));
    }

    void searchIndexIsChosenPerAccount()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        Gui::MainWindow window(&settings);
        QVERIFY(!window.imapAccess()->searchIndexer());

        bool shownOff = false;
        bool followsCache = false;
        bool decryptedFollowsIndex = false;
        QVERIFY(
            handleDialog([&]() { window.findChild<QAction *>(QStringLiteral("action_account_settings"))->trigger(); },
                [&](QDialog *dialog) {
                    auto *index = dialog->findChild<QCheckBox *>(QStringLiteral("searchIndex"));
                    auto *none = dialog->findChild<QRadioButton *>(QStringLiteral("offlineNope"));
                    auto *all = dialog->findChild<QRadioButton *>(QStringLiteral("offlineEverything"));
                    QVERIFY(index && none && all && dialog->findChild<QLabel *>(QStringLiteral("searchIndexNote")));
                    auto *decrypted = dialog->findChild<QCheckBox *>(QStringLiteral("searchIndexDecrypted"));
                    auto *note = dialog->findChild<QLabel *>(QStringLiteral("searchIndexNote"));
                    QVERIFY(decrypted);
                    shownOff = !index->isChecked() && index->isEnabled() && !decrypted->isChecked();
                    decryptedFollowsIndex = !decrypted->isEnabled();
                    // Without an offline cache there is nothing to index
                    none->click();
                    followsCache = !index->isEnabled() && note->text().contains(QStringLiteral("nothing to index"));
                    all->click();
                    followsCache = followsCache && index->isEnabled()
                        && !note->text().contains(QStringLiteral("nothing to index"));
                    index->setChecked(true);
                    decryptedFollowsIndex = decryptedFollowsIndex && decrypted->isEnabled();
                    decrypted->setChecked(true);
                    auto *smtpHost = dialog->findChild<QLineEdit *>(QStringLiteral("smtpHost"));
                    QVERIFY(smtpHost);
                    smtpHost->setText(QStringLiteral("smtp.work.example.test"));
                    save(dialog);
                }));
        QVERIFY(shownOff);
        QVERIFY(followsCache);
        QVERIFY(decryptedFollowsIndex);
        QSettings work(directory.filePath(QStringLiteral("main-account-1.conf")), QSettings::IniFormat);
        QCOMPARE(work.value(Common::SettingsNames::searchIndexKey).toBool(), true);
        QCOMPARE(work.value(Common::SettingsNames::searchIndexDecryptedKey).toBool(), true);
        QSettings home(directory.filePath(QStringLiteral("main-account-2.conf")), QSettings::IniFormat);
        QVERIFY(!home.contains(Common::SettingsNames::searchIndexKey));
        QVERIFY(!settings.contains(Common::SettingsNames::searchIndexKey));
        QTRY_VERIFY(window.imapAccess()->searchIndexer());
        QVERIFY2(window.imapAccess()->searchIndexer()->isAvailable(),
            qPrintable(window.imapAccess()->searchIndexer()->unavailableReason()));
        QVERIFY(window.imapAccess()->searchIndexer()->indexesDecrypted());
    }

    void searchIndexPrivacyIsKeptPerAccount()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        {
            QSettings work(directory.filePath(QStringLiteral("main-account-1.conf")), QSettings::IniFormat);
            work.setValue(Common::SettingsNames::searchIndexKey, true);
            work.setValue(Common::SettingsNames::searchIndexExcludedKey, QStringList{QStringLiteral("Archive")});
        }
        Gui::MainWindow window(&settings);
        Imap::ImapAccess *access = window.imapAccess();
        QVERIFY(access->searchIndexer());
        QVERIFY2(access->searchIndexer()->isAvailable(), qPrintable(access->searchIndexer()->unavailableReason()));
        QCOMPARE(access->searchIndexer()->excludedFolders(), QStringList{QStringLiteral("Archive")});
        QVERIFY(!access->searchIndexer()->indexesDecrypted());
        const QString cacheFile = access->cacheDirectory() + QLatin1String("imap.cache.sqlite");
        QVERIFY(tablesOf(cacheFile).contains(QStringLiteral("search_docs")));

        window.findChild<QAction *>(QStringLiteral("action_search_index_status"))->trigger();
        auto *dialog = window.findChild<Gui::SearchIndexDialog *>();
        QVERIFY(dialog);
        emit dialog->excludedFoldersChanged({QStringLiteral("Junk"), QStringLiteral("Trash")});
        QCOMPARE(window.accountSettings()->value(Common::SettingsNames::searchIndexExcludedKey).toStringList(),
            (QStringList{QStringLiteral("Junk"), QStringLiteral("Trash")}));
        QCOMPARE(access->searchIndexer()->excludedFolders(), (QStringList{QStringLiteral("Junk"), QStringLiteral("Trash")}));
        emit dialog->excludedFoldersChanged({});
        QVERIFY(!window.accountSettings()->contains(Common::SettingsNames::searchIndexExcludedKey));

        auto *remove = dialog->findChild<QPushButton *>(QStringLiteral("indexDelete"));
        QVERIFY(remove && remove->isEnabled());
        answerQuestion(QMessageBox::Yes);
        remove->click();
        QVERIFY(!access->searchIndexer());
        QCOMPARE(window.accountSettings()->value(Common::SettingsNames::searchIndexKey).toBool(), false);
        QVERIFY(!dialog->indexer());
        QVERIFY(dialog->findChild<QLabel *>(QStringLiteral("indexState"))->text().contains(QStringLiteral("was deleted")));
        QVERIFY(!tablesOf(cacheFile).contains(QStringLiteral("search_docs")));
        // The offline mail stays
        QVERIFY(QFileInfo::exists(cacheFile));
        for (auto *widget : QApplication::topLevelWidgets()) {
            auto *box = qobject_cast<QMessageBox *>(widget);
            QVERIFY2(!box || !box->isVisible(), qPrintable(box ? box->text() : QString()));
        }
    }

    void memoryOnlyCacheLeavesNoIndex()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        {
            QSettings work(directory.filePath(QStringLiteral("main-account-1.conf")), QSettings::IniFormat);
            work.setValue(Common::SettingsNames::searchIndexKey, true);
        }
        Gui::MainWindow window(&settings);
        QVERIFY(window.imapAccess()->searchIndexer());
        const QString cacheFile = window.imapAccess()->cacheDirectory() + QLatin1String("imap.cache.sqlite");
        QVERIFY(tablesOf(cacheFile).contains(QStringLiteral("search_docs")));

        QVERIFY(
            handleDialog([&]() { window.findChild<QAction *>(QStringLiteral("action_account_settings"))->trigger(); },
                [&](QDialog *dialog) {
                    auto *none = dialog->findChild<QRadioButton *>(QStringLiteral("offlineNope"));
                    QVERIFY(none);
                    none->click();
                    auto *smtpHost = dialog->findChild<QLineEdit *>(QStringLiteral("smtpHost"));
                    QVERIFY(smtpHost);
                    smtpHost->setText(QStringLiteral("smtp.work.example.test"));
                    save(dialog);
                }));
        QTRY_VERIFY(!window.imapAccess()->searchIndexer());
        QTRY_VERIFY(!tablesOf(cacheFile).contains(QStringLiteral("search_docs")));
    }

    void cancellingAccountSettingsKeepsTheAccount()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        Gui::MainWindow window(&settings);
        auto *access = window.imapAccess();

        QVERIFY(
            handleDialog([&]() { window.findChild<QAction *>(QStringLiteral("action_account_settings"))->trigger(); },
                [](QDialog *dialog) {
                    auto *host = dialog->findChild<QLineEdit *>(QStringLiteral("imapHost"));
                    QVERIFY(host);
                    host->setText(QStringLiteral("discarded.example.test"));
                    dialog->reject();
                }));
        QCOMPARE(window.imapAccess(), access);
        QCOMPARE(window.imapAccess()->server(), QStringLiteral("work.example.test"));
        QCOMPARE(window.accountSettings()->value(Common::SettingsNames::imapHostKey).toString(),
            QStringLiteral("work.example.test"));
    }

    void clientSecretStaysOutOfTheSettings()
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
            // Saved by an older version, before the secret moved into the stored grant
            work.setValue(Common::SettingsNames::oauthClientSecretKey, QStringLiteral("legacy-secret"));
        }
        Gui::MainWindow window(&settings);

        bool blocked = false;
        QString legacyPlaceholder;
        QVERIFY(
            handleDialog([&]() { window.findChild<QAction *>(QStringLiteral("action_account_settings"))->trigger(); },
                [&](QDialog *dialog) {
                    auto *secret = dialog->findChild<QLineEdit *>(QStringLiteral("oauthClientSecret"));
                    QVERIFY(secret);
                    // The field never shows a saved secret
                    QVERIFY(secret->text().isEmpty());
                    legacyPlaceholder = secret->placeholderText();
                    auto *smtpHost = dialog->findChild<QLineEdit *>(QStringLiteral("smtpHost"));
                    QVERIFY(smtpHost);
                    smtpHost->setText(QStringLiteral("smtp.work.example.test"));
                    // A new secret is only kept by signing in, so saving it unsigned is refused
                    secret->setText(QStringLiteral("typed-secret"));
                    save(dialog);
                    blocked = dialog->isVisible();
                    secret->clear();
                    save(dialog);

                }));
        QVERIFY(blocked);
        QVERIFY(!legacyPlaceholder.isEmpty());
        QFile file(directory.filePath(QStringLiteral("main-account-1.conf")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray saved = file.readAll();
        QVERIFY(!saved.contains("typed-secret"));
        // Without a grant to move it into, the legacy secret stays for the next connection
        QSettings work(directory.filePath(QStringLiteral("main-account-1.conf")), QSettings::IniFormat);
        QCOMPARE(work.value(Common::SettingsNames::oauthClientSecretKey).toString(), QStringLiteral("legacy-secret"));
        QCOMPARE(work.value(Common::SettingsNames::oauthClientIdKey).toString(), QStringLiteral("work-client"));
    }

    void addingAnAccountOpensItsAccountSettings()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        Gui::MainWindow window(&settings);

        QStringList titles;
        QString windowTitle;
        QVERIFY(handleDialog([&]() { QMetaObject::invokeMethod(&window, "slotAddAccount", Qt::DirectConnection); },
            [&](QDialog *dialog) {
                titles = tabTitles(dialog);
                windowTitle = dialog->windowTitle();
                dialog->reject();
            }));
        QVERIFY(titles.contains(QStringLiteral("IMAP")));
        QVERIFY(!titles.contains(QStringLiteral("General")));
        QVERIFY(windowTitle.contains(QStringLiteral("Travel")));
        QCOMPARE(window.imapAccess()->credentialAccountId(), QStringLiteral("account-3"));
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
    AccountSettingsDialogTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_AccountSettingsDialog.moc"
