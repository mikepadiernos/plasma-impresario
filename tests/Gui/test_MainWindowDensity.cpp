/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Gui/DensityDelegate.h"
#include "Gui/DensityMenu.h"
#include "Gui/MailBoxTreeView.h"
#include "Gui/MsgListView.h"
#include "Gui/Window.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "configure.cmake.h"
#ifdef TROJITA_HAVE_WEBENGINE
#include "Gui/WebEngineMessageProfile.h"
#endif
#include "static_plugins.h"

#include <KLocalizedString>
#include <QApplication>
#include <QSettings>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class MainWindowDensityTest : public QObject
{
    Q_OBJECT
private slots:
    void densityReachesBothMailboxViews()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("legacy.ini")), QSettings::IniFormat);
        settings.setValue(Common::SettingsNames::imapMethodKey, Common::SettingsNames::methodSSL);
        settings.setValue(Common::SettingsNames::imapStartMode, Common::SettingsNames::netOffline);
        settings.setValue(Common::SettingsNames::guiShowSystray, false);

        Gui::MainWindow window(&settings);
        auto *menu = window.findChild<Gui::DensityMenu *>();
        auto *mailboxes = window.findChild<Gui::MailBoxTreeView *>();
        auto *messages = window.findChild<Gui::MsgListView *>();
        QVERIFY(menu);
        QVERIFY(mailboxes);
        QVERIFY(messages);
        QCOMPARE(menu->actions().size(), 2);

        QStandardItemModel model(1, 1);
        model.setData(model.index(0, 0), QStringLiteral("Inbox"));
        QStyleOptionViewItem option;
        option.initFrom(mailboxes);
        const auto index = model.index(0, 0);
        auto *mailboxDelegate = qobject_cast<Gui::DensityDelegate *>(mailboxes->itemDelegate());
        auto *messageDelegate = qobject_cast<Gui::DensityDelegate *>(messages->itemDelegate());
        QVERIFY(mailboxDelegate);
        QVERIFY(messageDelegate);
        const int mailboxHeight = mailboxDelegate->sizeHint(option, index).height();
        const int messageHeight = messageDelegate->sizeHint(option, index).height();

        menu->actions().at(1)->trigger();
        QVERIFY(mailboxDelegate->sizeHint(option, index).height() > mailboxHeight);
        QVERIFY(messageDelegate->sizeHint(option, index).height() > messageHeight);
        QVERIFY(QMetaObject::invokeMethod(&window, "slotLayoutWide"));
        QVERIFY(menu->actions().at(1)->isChecked());
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
    for (auto location : {QStandardPaths::GenericConfigLocation, QStandardPaths::AppDataLocation,
                          QStandardPaths::CacheLocation}) {
        if (!QStandardPaths::writableLocation(location).startsWith(home.path() + QLatin1Char('/')))
            return 1;
    }
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(license);
    KLocalizedString::setApplicationDomain("plasma-impresario");
    if (!Imap::Mailbox::SystemNetworkWatcher::init())
        return 1;
    MainWindowDensityTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_MainWindowDensity.moc"
