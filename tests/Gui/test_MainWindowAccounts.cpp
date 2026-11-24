/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/AccountStatus.h"
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Gui/Window.h"
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "configure.cmake.h"
#ifdef TROJITA_HAVE_WEBENGINE
#include "Gui/WebEngineMessageProfile.h"
#endif
#include "static_plugins.h"

#include <KLocalizedString>
#include <QApplication>
#include <QComboBox>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

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
