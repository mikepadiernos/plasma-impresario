/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/AccountStatus.h"
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Composer/AccountIdentities.h"
#include "Composer/SenderIdentitiesModel.h"
#include "Gui/AddAccountDialog.h"
#include "Gui/JmapSyncer.h"
#include "Gui/SearchIndexDialog.h"
#include "Gui/Window.h"
#include "UiUtils/BackgroundRunning.h"
#include "UiUtils/PlasmaSearch.h"
#include "Imap/Model/AccountCapabilities.h"
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/SearchIndexer.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "OAuth/TokenSource.h"
#include "Jmap/Submission.h"
#include "LocalMail/Store.h"
#include "../Jmap/FakeJmapServer.h"
#include "Pop3/FetchJob.h"
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
#include <QFormLayout>
#include <QFile>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QStandardPaths>
#include <QScrollArea>
#include <QTabWidget>
#include <QTemporaryDir>
#include <KSharedConfig>
#include <QGuiApplication>
#include <QTest>
#include <QTimer>
#include <functional>
#include <memory>

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
 * trigger blocks in exec()

An Add Account dialog on the way is filled in by @arg addAccount, or named "Travel" without one, and accepted.
*/
bool handleDialog(const std::function<void()> &trigger, const std::function<void(QDialog *)> &handler,
                  const std::function<void(Gui::AddAccountDialog *)> &addAccount = {})
{
    bool handled = false;
    bool added = false;
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, [&]() {
        auto *modal = QApplication::activeModalWidget();
        if (auto *add = qobject_cast<Gui::AddAccountDialog *>(modal)) {
            if (added)
                return;
            added = true;
            if (addAccount)
                addAccount(add);
            else
                add->findChild<QLineEdit *>(QStringLiteral("accountName"))->setText(QStringLiteral("Travel"));
            add->accept();
            return;
        }
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

        QStringList groups;
        QVERIFY(handleDialog([&]() { QMetaObject::invokeMethod(&window, "slotShowSettings", Qt::DirectConnection); },
            [&](QDialog *dialog) {
                const auto pages = dialog->findChildren<QScrollArea *>();
                for (const QScrollArea *page : pages) {
                    if (page->objectName().startsWith(QLatin1String("settingsGroup-")))
                        groups << page->objectName().mid(14);
                }
                save(dialog);
            }));
        QCOMPARE(groups,
                 (QStringList { QStringLiteral("general"), QStringLiteral("accounts"), QStringLiteral("appearance"),
                                QStringLiteral("theme"), QStringLiteral("reading"), QStringLiteral("composition"), QStringLiteral("privacy"),
                                QStringLiteral("notifications"), QStringLiteral("search"),
                                QStringLiteral("integrations"), QStringLiteral("workspaces"),
                                QStringLiteral("keyboard") }));
        // Saving application settings must not reconnect or rebuild the account's
        // models.
        QCOMPARE(window.imapAccess(), access);
        QCOMPARE(window.imapModel(), model);
    }

    void startAfterLoginNeedsBackgroundRunning()
    {
        namespace Background = UiUtils::BackgroundRunning;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        Gui::MainWindow window(&settings);
        auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
        const QString entry = Background::autostartPath(QGuiApplication::desktopFileName());
        QVERIFY(!QFile::exists(entry));

        auto checkBoxes = [](QDialog *dialog) {
            return std::pair(dialog->findChild<QCheckBox *>(QStringLiteral("guiRunInBackgroundCheckbox")),
                             dialog->findChild<QCheckBox *>(QStringLiteral("guiStartAfterLoginCheckbox")));
        };
        QVERIFY(handleDialog([&]() { QMetaObject::invokeMethod(&window, "slotShowSettings", Qt::DirectConnection); },
            [&](QDialog *dialog) {
                auto [background, login] = checkBoxes(dialog);
                QVERIFY(background && login);
                // Both are off by default, and starting after login waits for running in the background
                QVERIFY(!background->isChecked());
                QVERIFY(!login->isChecked());
                QVERIFY(!login->isEnabled());
                background->setChecked(true);
                QVERIFY(login->isEnabled());
                login->setChecked(true);
                save(dialog);
            }));
        QVERIFY(Background::runsInBackground(*config));
        QVERIFY(Background::startsAfterLogin(*config));
        QVERIFY(!qApp->quitOnLastWindowClosed());
        QVERIFY(QFile::exists(entry));
        QVERIFY(Background::isOwnAutostartEntry(entry));
        QFile file(entry);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().contains("--background"));
        file.close();

        // Turning running in the background off also stops starting after login and removes the entry
        QVERIFY(handleDialog([&]() { QMetaObject::invokeMethod(&window, "slotShowSettings", Qt::DirectConnection); },
            [&](QDialog *dialog) {
                auto [background, login] = checkBoxes(dialog);
                QVERIFY(background && login);
                QVERIFY(background->isChecked());
                QVERIFY(login->isChecked());
                background->setChecked(false);
                QVERIFY(!login->isChecked());
                QVERIFY(!login->isEnabled());
                save(dialog);
            }));
        QVERIFY(!Background::runsInBackground(*config));
        QVERIFY(!Background::startsAfterLogin(*config));
        QVERIFY(qApp->quitOnLastWindowClosed());
        QVERIFY(!QFile::exists(entry));
    }

    void plasmaSearchIsOptIn()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        Gui::MainWindow window(&settings);
        auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
        QVERIFY(!UiUtils::PlasmaSearch::isEnabled(*config));
        QVERIFY(window.plasmaSearchMatches(QStringLiteral("report")).isEmpty());

        for (const bool enable : {true, false}) {
            QVERIFY(handleDialog([&]() { QMetaObject::invokeMethod(&window, "slotShowSettings", Qt::DirectConnection); },
                [&](QDialog *dialog) {
                    auto *checkBox = dialog->findChild<QCheckBox *>(QStringLiteral("guiPlasmaSearchCheckbox"));
                    QVERIFY(checkBox);
                    QCOMPARE(checkBox->isChecked(), !enable);
                    checkBox->setChecked(enable);
                    save(dialog);
                }));
            QCOMPARE(UiUtils::PlasmaSearch::isEnabled(*config), enable);
        }
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

    void gmailKeepsItsOwnSentCopiesAndArchiveIsAutomatic()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        Gui::MainWindow window(&settings);

        bool archiveAutomatic = false;
        bool offForGmail = false;
        bool backElsewhere = false;
        QVERIFY(
            handleDialog([&]() { window.findChild<QAction *>(QStringLiteral("action_account_settings"))->trigger(); },
                [&](QDialog *dialog) {
                    auto *archive = dialog->findChild<QLineEdit *>(QStringLiteral("archiveFolderName"));
                    auto *smtpHost = dialog->findChild<QLineEdit *>(QStringLiteral("smtpHost"));
                    auto *saveToImap = dialog->findChild<QCheckBox *>(QStringLiteral("saveToImap"));
                    QVERIFY(archive && smtpHost && saveToImap);
                    archiveAutomatic = archive->text().isEmpty() && !archive->placeholderText().isEmpty();
                    archive->setText(QStringLiteral("  "));
                    auto setHost = [smtpHost](const QString &host) {
                        smtpHost->setText(host);
                        emit smtpHost->editingFinished();
                    };
                    // Gmail files sent mail itself, so the option is shown off without being changed
                    setHost(QStringLiteral("SMTP.gmail.com"));
                    offForGmail = !saveToImap->isChecked() && !saveToImap->isEnabled() && !saveToImap->toolTip().isEmpty();
                    setHost(QStringLiteral("smtp.work.example.test"));
                    backElsewhere = saveToImap->isChecked() && saveToImap->isEnabled() && saveToImap->toolTip().isEmpty();
                    setHost(QStringLiteral("smtp.gmail.com"));
                    save(dialog);
                }));
        QVERIFY(archiveAutomatic);
        QVERIFY(offForGmail);
        QVERIFY(backElsewhere);
        QSettings work(directory.filePath(QStringLiteral("main-account-1.conf")), QSettings::IniFormat);
        QCOMPARE(work.value(Common::SettingsNames::imapArchiveFolderName).toString(), QString());
        QCOMPARE(work.value(Common::SettingsNames::smtpHostKey).toString(), QStringLiteral("smtp.gmail.com"));
        QVERIFY(work.value(Common::SettingsNames::composerSaveToImapKey, true).toBool());
        QCOMPARE(window.imapModel()->archiveFolderName(), QString());
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

    void searchIndexAttachmentsAreChosenPerAccount()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        Gui::MainWindow window(&settings);

        bool followsIndex = false;
        bool limitFollowsChoice = false;
        QVERIFY(
            handleDialog([&]() { window.findChild<QAction *>(QStringLiteral("action_account_settings"))->trigger(); },
                [&](QDialog *dialog) {
                    auto *index = dialog->findChild<QCheckBox *>(QStringLiteral("searchIndex"));
                    auto *all = dialog->findChild<QRadioButton *>(QStringLiteral("offlineEverything"));
                    auto *attachments = dialog->findChild<QCheckBox *>(QStringLiteral("searchIndexAttachments"));
                    auto *limit = dialog->findChild<QSpinBox *>(QStringLiteral("searchIndexAttachmentLimit"));
                    QVERIFY(index && all && attachments && limit);
                    QCOMPARE(limit->value(), 10);
                    all->click();
                    followsIndex = !attachments->isChecked() && !attachments->isEnabled() && !limit->isEnabled();
                    index->setChecked(true);
                    followsIndex = followsIndex && attachments->isEnabled();
                    limitFollowsChoice = !limit->isEnabled();
                    attachments->setChecked(true);
                    limitFollowsChoice = limitFollowsChoice && limit->isEnabled();
                    limit->setValue(25);
                    auto *smtpHost = dialog->findChild<QLineEdit *>(QStringLiteral("smtpHost"));
                    QVERIFY(smtpHost);
                    smtpHost->setText(QStringLiteral("smtp.work.example.test"));
                    save(dialog);
                }));
        QVERIFY(followsIndex);
        QVERIFY(limitFollowsChoice);
        QSettings work(directory.filePath(QStringLiteral("main-account-1.conf")), QSettings::IniFormat);
        QCOMPARE(work.value(Common::SettingsNames::searchIndexAttachmentsKey).toBool(), true);
        QCOMPARE(work.value(Common::SettingsNames::searchIndexAttachmentLimitKey).toInt(), 25);
        QSettings home(directory.filePath(QStringLiteral("main-account-2.conf")), QSettings::IniFormat);
        QVERIFY(!home.contains(Common::SettingsNames::searchIndexAttachmentsKey));
        QVERIFY(!settings.contains(Common::SettingsNames::searchIndexAttachmentsKey));
        QTRY_VERIFY(window.imapAccess()->searchIndexer());
        QVERIFY(window.imapAccess()->searchIndexer()->indexesAttachments());
        QCOMPARE(window.imapAccess()->searchIndexer()->attachmentLimit(), quint64(25) * 1024 * 1024);
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
        QString shownHost;
        QString foundIn;
        bool oauthOffered = false;
        QVERIFY(handleDialog([&]() { QMetaObject::invokeMethod(&window, "slotAddAccount", Qt::DirectConnection); },
            [&](QDialog *dialog) {
                titles = tabTitles(dialog);
                windowTitle = dialog->windowTitle();
                if (auto *host = dialog->findChild<QLineEdit *>(QStringLiteral("imapHost")))
                    shownHost = host->text();
                dialog->reject();
            },
            [&](Gui::AddAccountDialog *add) {
                add->findChild<QLineEdit *>(QStringLiteral("realName"))->setText(QStringLiteral("Tess Traveller"));
                add->findChild<QLineEdit *>(QStringLiteral("emailAddress"))->setText(QStringLiteral("tess@gmail.com"));
                add->findChild<QLineEdit *>(QStringLiteral("accountName"))->setText(QStringLiteral("Travel"));
                // A built-in provider needs no network
                add->findChild<QPushButton *>(QStringLiteral("findSettings"))->click();
                QTRY_VERIFY(add->result().isValid());
                foundIn = add->findChild<QLabel *>(QStringLiteral("settingsSource"))->text();
                oauthOffered = !add->findChild<QCheckBox *>(QStringLiteral("useOAuth"))->isHidden();
                add->findChild<QCheckBox *>(QStringLiteral("useOAuth"))->setChecked(false);
            }));
        QVERIFY(titles.contains(QStringLiteral("IMAP")));
        QVERIFY(!titles.contains(QStringLiteral("General")));
        QVERIFY(windowTitle.contains(QStringLiteral("Travel")));
        QVERIFY(!foundIn.isEmpty());
        QVERIFY(oauthOffered);
        QCOMPARE(window.imapAccess()->credentialAccountId(), QStringLiteral("account-3"));
        // The settings dialog opens with the servers that were found
        QCOMPARE(shownHost, QStringLiteral("imap.gmail.com"));

        QSettings travel(directory.filePath(QStringLiteral("main-account-3.conf")), QSettings::IniFormat);
        QCOMPARE(travel.value(Common::SettingsNames::imapHostKey).toString(), QStringLiteral("imap.gmail.com"));
        QCOMPARE(travel.value(Common::SettingsNames::imapPortKey).toInt(), 993);
        QCOMPARE(travel.value(Common::SettingsNames::imapMethodKey).toString(), Common::SettingsNames::methodSSL);
        QCOMPARE(travel.value(Common::SettingsNames::imapUserKey).toString(), QStringLiteral("tess@gmail.com"));
        QCOMPARE(travel.value(Common::SettingsNames::smtpHostKey).toString(), QStringLiteral("smtp.gmail.com"));
        QCOMPARE(travel.value(Common::SettingsNames::msaMethodKey).toString(), Common::SettingsNames::methodSSMTP);
        // OAuth was turned off, so the account signs in with a password
        QVERIFY(!travel.contains(Common::SettingsNames::imapAuthMethodKey));
        Composer::SenderIdentitiesModel identities;
        identities.loadFromSettings(travel);
        QCOMPARE(identities.rowCount(), 1);
        QCOMPARE(identities.data(identities.index(0, Composer::SenderIdentitiesModel::COLUMN_NAME)).toString(),
                 QStringLiteral("Tess Traveller"));
        QCOMPARE(identities.data(identities.index(0, Composer::SenderIdentitiesModel::COLUMN_EMAIL)).toString(),
                 QStringLiteral("tess@gmail.com"));
    }

    void pop3AccountsHaveTheirOwnSettings()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        Gui::MainWindow window(&settings);

        QStringList titles;
        QString shownUser;
        QString shownSmtpUser;
        bool imapOnlySmtpHidden = false;
        bool daysFollowRetention = false;
        bool noneWarns = false;
        QVERIFY(handleDialog([&]() { QMetaObject::invokeMethod(&window, "slotAddAccount", Qt::DirectConnection); },
            [&](QDialog *dialog) {
                titles = tabTitles(dialog);
                QVERIFY(!dialog->findChild<QLineEdit *>(QStringLiteral("imapHost")));
                auto *host = dialog->findChild<QLineEdit *>(QStringLiteral("pop3Host"));
                auto *user = dialog->findChild<QLineEdit *>(QStringLiteral("pop3User"));
                auto *password = dialog->findChild<QLineEdit *>(QStringLiteral("pop3Password"));
                auto *security = dialog->findChild<QComboBox *>(QStringLiteral("pop3Security"));
                auto *warning = dialog->findChild<QLabel *>(QStringLiteral("pop3SecurityWarning"));
                auto *retention = dialog->findChild<QComboBox *>(QStringLiteral("pop3Retention"));
                auto *days = dialog->findChild<QSpinBox *>(QStringLiteral("pop3RetentionDays"));
                auto *interval = dialog->findChild<QSpinBox *>(QStringLiteral("pop3Interval"));
                auto *reuse = dialog->findChild<QCheckBox *>(QStringLiteral("smtpAuthReuseImapCreds"));
                auto *saveToImap = dialog->findChild<QCheckBox *>(QStringLiteral("saveToImap"));
                auto *smtpHost = dialog->findChild<QLineEdit *>(QStringLiteral("smtpHost"));
                QVERIFY(host && user && password && security && warning && retention && days && interval && reuse
                        && saveToImap && smtpHost);
                auto *smtpUser = dialog->findChild<QLineEdit *>(QStringLiteral("smtpUser"));
                QVERIFY(smtpUser);
                shownUser = user->text();
                shownSmtpUser = smtpUser->text();
                // A POP3 account has no IMAP sign-in to reuse, and no Sent folder to keep copies in
                auto *smtpAuth = dialog->findChild<QCheckBox *>(QStringLiteral("smtpAuth"));
                QVERIFY(smtpAuth);
                smtpAuth->setChecked(true);
                imapOnlySmtpHidden = reuse->isHidden() && saveToImap->isHidden();
                daysFollowRetention = !days->isEnabled();
                retention->setCurrentIndex(retention->findData(static_cast<int>(Pop3::Retention::AfterDays)));
                daysFollowRetention = daysFollowRetention && days->isEnabled();
                days->setValue(30);
                noneWarns = warning->isHidden();
                security->setCurrentIndex(security->findData(static_cast<int>(Pop3::Security::None)));
                noneWarns = noneWarns && !warning->isHidden();
                security->setCurrentIndex(security->findData(static_cast<int>(Pop3::Security::StartTls)));
                interval->setValue(0);
                // Nothing listens there, so the first check fails at once instead of asking for anything
                host->setText(QStringLiteral("127.0.0.1"));
                dialog->findChild<QSpinBox *>(QStringLiteral("pop3Port"))->setValue(1);
                password->setText(QStringLiteral("secret"));
                smtpHost->setText(QStringLiteral("smtp.example.test"));
                save(dialog);
            },
            [&](Gui::AddAccountDialog *add) {
                add->findChild<QLineEdit *>(QStringLiteral("emailAddress"))->setText(QStringLiteral("tess@example.test"));
                auto *protocol = add->findChild<QComboBox *>(QStringLiteral("accountProtocol"));
                protocol->setCurrentIndex(protocol->findData(static_cast<int>(Imap::AccountProtocol::Pop3)));
            }));
        QCOMPARE(titles, (QStringList{QStringLiteral("Identities"), QStringLiteral("POP3"), QStringLiteral("SMTP")}));
        QCOMPARE(shownUser, QStringLiteral("tess@example.test"));
        QCOMPARE(shownSmtpUser, QStringLiteral("tess@example.test"));
        QVERIFY(imapOnlySmtpHidden);
        QVERIFY(daysFollowRetention);
        QVERIFY(noneWarns);
        for (auto *widget : QApplication::topLevelWidgets()) {
            auto *box = qobject_cast<QMessageBox *>(widget);
            QVERIFY2(!box || !box->isVisible(), qPrintable(box ? box->text() : QString()));
        }

        QSettings travel(directory.filePath(QStringLiteral("main-account-3.conf")), QSettings::IniFormat);
        QCOMPARE(Imap::AccountCapabilities::forAccount(travel).protocol(), Imap::AccountProtocol::Pop3);
        const Pop3::Settings saved = Pop3::Settings::fromSettings(travel);
        QCOMPARE(saved.host, QStringLiteral("127.0.0.1"));
        QCOMPARE(saved.port, quint16(1));
        QCOMPARE(saved.security, Pop3::Security::StartTls);
        QCOMPARE(saved.user, QStringLiteral("tess@example.test"));
        QCOMPARE(saved.retention, Pop3::Retention::AfterDays);
        QCOMPARE(saved.retentionDays, 30);
        QCOMPARE(saved.interval, 0);
        QVERIFY(!travel.contains(Common::SettingsNames::imapMethodKey));
        QVERIFY(!travel.contains(Common::SettingsNames::imapHostKey));

        // The mail views read the account's local store
        QCOMPARE(window.imapAccess()->protocol(), Imap::AccountProtocol::Pop3);
        QVERIFY(window.imapAccess()->isConfigured());
        QVERIFY(window.imapAccess()->localStore());
        QTRY_VERIFY(window.imapModel()
                    && window.imapModel()->mailboxIndexByName(QStringLiteral("INBOX")).isValid());
        // Without a password store the shown account asks for the password, and the check's failure becomes its status
        auto *status = window.findChild<Common::AccountStatusTracker *>();
        QVERIFY(status);
        QDialog *ask = nullptr;
        QTRY_VERIFY((ask = window.findChild<QDialog *>(QStringLiteral("PasswordDialog"))) && ask->isVisible());
        QVERIFY(ask->findChild<QLabel *>(QStringLiteral("descriptionLabel"))->text().contains(QStringLiteral("POP3")));
        ask->findChild<QLineEdit *>(QStringLiteral("passwordLineEdit"))->setText(QStringLiteral("secret"));
        ask->accept();
        QTRY_COMPARE(status->status(QStringLiteral("account-3")).kind, Common::AccountStatusKind::NetworkError);
    }

    void jmapAccountsHaveTheirOwnSettings()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        Gui::MainWindow window(&settings);

        QStringList titles;
        QString shownUser;
        bool plainHttpRefused = false;
        bool tokenRelabels = false;
        bool smtpNoteFollowsSending = false;
        bool imapOnlySmtpHidden = false;
        QVERIFY(handleDialog([&]() { QMetaObject::invokeMethod(&window, "slotAddAccount", Qt::DirectConnection); },
            [&](QDialog *dialog) {
                titles = tabTitles(dialog);
                QVERIFY(!dialog->findChild<QLineEdit *>(QStringLiteral("imapHost")));
                QVERIFY(!dialog->findChild<QLineEdit *>(QStringLiteral("pop3Host")));
                auto *url = dialog->findChild<QLineEdit *>(QStringLiteral("jmapUrl"));
                auto *urlStatus = dialog->findChild<QLabel *>(QStringLiteral("jmapUrlStatus"));
                auto *user = dialog->findChild<QLineEdit *>(QStringLiteral("jmapUser"));
                auto *authentication = dialog->findChild<QComboBox *>(QStringLiteral("jmapAuthentication"));
                auto *password = dialog->findChild<QLineEdit *>(QStringLiteral("jmapPassword"));
                auto *interval = dialog->findChild<QSpinBox *>(QStringLiteral("jmapInterval"));
                auto *sending = dialog->findChild<QComboBox *>(QStringLiteral("jmapSending"));
                auto *smtpNote = dialog->findChild<QLabel *>(QStringLiteral("smtpJmapNote"));
                auto *reuse = dialog->findChild<QCheckBox *>(QStringLiteral("smtpAuthReuseImapCreds"));
                auto *smtpAuth = dialog->findChild<QCheckBox *>(QStringLiteral("smtpAuth"));
                QVERIFY(url && urlStatus && user && authentication && password && interval && sending && smtpNote
                        && reuse && smtpAuth);
                shownUser = user->text();

                url->setText(QStringLiteral("http://mail.example.test"));
                plainHttpRefused = !urlStatus->isHidden() && urlStatus->text().contains(QStringLiteral("https"));

                auto *form = qobject_cast<QFormLayout *>(password->parentWidget()->layout());
                QVERIFY(form);
                auto *label = qobject_cast<QLabel *>(form->labelForField(password));
                QVERIFY(label);
                authentication->setCurrentIndex(authentication->findData(static_cast<int>(Jmap::Authentication::Bearer)));
                tokenRelabels = label->text().contains(QStringLiteral("Token"));

                // Sending through JMAP needs nothing from the SMTP page, which says so
                smtpNoteFollowsSending = !smtpNote->isHidden();
                sending->setCurrentIndex(sending->findData(static_cast<int>(Jmap::Sending::Smtp)));
                smtpNoteFollowsSending = smtpNoteFollowsSending && smtpNote->isHidden();
                sending->setCurrentIndex(sending->findData(static_cast<int>(Jmap::Sending::Jmap)));
                smtpNoteFollowsSending = smtpNoteFollowsSending && !smtpNote->isHidden();
                smtpAuth->setChecked(true);
                imapOnlySmtpHidden = reuse->isHidden();

                interval->setValue(0);
                // Nothing listens there, so the first synchronization fails at once
                url->setText(QStringLiteral("http://127.0.0.1:1"));
                password->setText(QStringLiteral("secret"));
                save(dialog);
            },
            [&](Gui::AddAccountDialog *add) {
                add->findChild<QLineEdit *>(QStringLiteral("emailAddress"))->setText(QStringLiteral("tess@example.test"));
                auto *protocol = add->findChild<QComboBox *>(QStringLiteral("accountProtocol"));
                protocol->setCurrentIndex(protocol->findData(static_cast<int>(Imap::AccountProtocol::Jmap)));
            }));
        QCOMPARE(titles, (QStringList{QStringLiteral("Identities"), QStringLiteral("JMAP"), QStringLiteral("SMTP")}));
        QCOMPARE(shownUser, QStringLiteral("tess@example.test"));
        QVERIFY(plainHttpRefused);
        QVERIFY(tokenRelabels);
        QVERIFY(smtpNoteFollowsSending);
        QVERIFY(imapOnlySmtpHidden);
        for (auto *widget : QApplication::topLevelWidgets()) {
            auto *box = qobject_cast<QMessageBox *>(widget);
            QVERIFY2(!box || !box->isVisible(), qPrintable(box ? box->text() : QString()));
        }

        QSettings travel(directory.filePath(QStringLiteral("main-account-3.conf")), QSettings::IniFormat);
        QCOMPARE(Imap::AccountCapabilities::forAccount(travel).protocol(), Imap::AccountProtocol::Jmap);
        const Jmap::Settings saved = Jmap::Settings::fromSettings(travel);
        QCOMPARE(saved.url, QStringLiteral("http://127.0.0.1:1"));
        QCOMPARE(saved.user, QStringLiteral("tess@example.test"));
        QCOMPARE(saved.authentication, Jmap::Authentication::Bearer);
        QCOMPARE(saved.interval, 0);
        QCOMPARE(saved.sending, Jmap::Sending::Jmap);
        QVERIFY(!travel.contains(Common::SettingsNames::imapMethodKey));

        QCOMPARE(window.imapAccess()->protocol(), Imap::AccountProtocol::Jmap);
        QVERIFY(window.imapAccess()->isConfigured());
        QVERIFY(window.imapAccess()->localStore());
        QTRY_VERIFY(window.imapModel()
                    && window.imapModel()->mailboxIndexByName(QStringLiteral("INBOX")).isValid());
        // Without an SMTP server the account still sends, through its JMAP server
        std::unique_ptr<MSA::MSAFactory> factory(window.msaFactory());
        QVERIFY(dynamic_cast<Jmap::SubmissionFactory *>(factory.get()));

        auto *status = window.findChild<Common::AccountStatusTracker *>();
        QVERIFY(status);
        QDialog *ask = nullptr;
        QTRY_VERIFY((ask = window.findChild<QDialog *>(QStringLiteral("PasswordDialog"))) && ask->isVisible());
        QVERIFY(ask->findChild<QLabel *>(QStringLiteral("descriptionLabel"))->text().contains(QStringLiteral("token")));
        ask->findChild<QLineEdit *>(QStringLiteral("passwordLineEdit"))->setText(QStringLiteral("secret"));
        ask->accept();
        QTRY_COMPARE(status->status(QStringLiteral("account-3")).kind, Common::AccountStatusKind::NetworkError);
    }

    void jmapAccountsSynchronizeWithTheServer()
    {
        FakeJmapServer server;
        server.canSubmit = false;
        const QString inbox = server.addMailbox(QStringLiteral("Inbox"), QStringLiteral("inbox"));
        const QString email = server.addEmail("Subject: hello\r\n\r\nHi\r\n", {inbox});

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        seedTwoAccounts(directory, settings);
        settings.setValue(QStringLiteral("accounts/ids"),
                          QStringList{QStringLiteral("account-1"), QStringLiteral("account-2"), QStringLiteral("account-3")});
        settings.setValue(QStringLiteral("accounts/active"), QStringLiteral("account-3"));
        settings.setValue(QStringLiteral("accounts/next"), 4);
        settings.setValue(QStringLiteral("accounts/names/account-3"), QStringLiteral("Web"));
        {
            QSettings third(directory.filePath(QStringLiteral("main-account-3.conf")), QSettings::IniFormat);
            Imap::AccountCapabilities::setProtocol(third, Imap::AccountProtocol::Jmap);
            Jmap::Settings jmap;
            jmap.url = server.address();
            jmap.user = server.user;
            jmap.interval = 0;
            jmap.save(third);
            third.sync();
        }
        Gui::MainWindow window(&settings);
        QCOMPARE(window.imapAccess()->protocol(), Imap::AccountProtocol::Jmap);

        // Without a password store the shown account asks, and then the server's mail comes down
        QDialog *ask = nullptr;
        QTRY_VERIFY((ask = window.findChild<QDialog *>(QStringLiteral("PasswordDialog"))) && ask->isVisible());
        QVERIFY(ask->findChild<QLabel *>(QStringLiteral("descriptionLabel"))->text().contains(QStringLiteral("JMAP")));
        ask->findChild<QLineEdit *>(QStringLiteral("passwordLineEdit"))->setText(server.password);
        ask->accept();
        auto *status = window.findChild<Common::AccountStatusTracker *>();
        QVERIFY(status);
        QTRY_COMPARE(status->status(QStringLiteral("account-3")).kind, Common::AccountStatusKind::Connected);
        const auto store = window.imapAccess()->localStore();
        QVERIFY(store);
        QCOMPARE(store->messages(QStringLiteral("INBOX")).size(), 1);

        // What the server said about sending is remembered for the settings
        QSettings third(directory.filePath(QStringLiteral("main-account-3.conf")), QSettings::IniFormat);
        QTRY_VERIFY(!Jmap::Settings::fromSettings(third).canSubmit);

        // A change made here goes to the server by itself, shortly afterwards
        const LocalMail::MessageInfo message = store->messages(QStringLiteral("INBOX")).first();
        const int requestsBefore = server.requests;
        QVERIFY(store->setFlags(QStringLiteral("INBOX"), message.uid, {QStringLiteral("\\Seen")}));
        QTRY_VERIFY_WITH_TIMEOUT(server.emails.value(email).keywords.contains(QStringLiteral("$seen")), 20000);
        // ...and what that synchronization wrote here does not start another one
        QTRY_COMPARE(status->status(QStringLiteral("account-3")).kind, Common::AccountStatusKind::Connected);
        const int requestsAfter = server.requests;
        QVERIFY(requestsAfter > requestsBefore);
        QTest::qWait(Gui::JmapSyncer::changeDelay() + 1000);
        QCOMPARE(server.requests, requestsAfter);

        // A folder made on the server appears after checking for new mail
        server.addMailbox(QStringLiteral("Projects"));
        auto *check = window.findChild<QAction *>(QStringLiteral("action_resync_mailbox"));
        if (!check) {
            const auto actions = window.findChildren<QAction *>();
            for (QAction *action : actions) {
                if (action->text().remove(QLatin1Char('&')) == QLatin1String("Check for New Messages"))
                    check = action;
            }
        }
        QVERIFY(check);
        check->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(window.imapModel()->mailboxIndexByName(QStringLiteral("Projects")).isValid(), 10000);
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
