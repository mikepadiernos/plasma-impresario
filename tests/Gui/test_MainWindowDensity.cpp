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

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QApplication>
#include <QSettings>
#include <QSplitter>
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

        auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
        KConfigGroup(config, QStringLiteral("Appearance")).writeEntry("MailboxSidebarPosition", QStringLiteral("right"));
        QVERIFY(config->sync());
        config->reparseConfiguration();
        Gui::MainWindow window(&settings);
        auto *menu = window.findChild<Gui::DensityMenu *>();
        auto *mailboxes = window.findChild<Gui::MailBoxTreeView *>();
        auto *messages = window.findChild<Gui::MsgListView *>();
        auto *left = window.findChild<QAction *>(QStringLiteral("mailboxSidebarLeft"));
        auto *right = window.findChild<QAction *>(QStringLiteral("mailboxSidebarRight"));
        QVERIFY(menu);
        QVERIFY(mailboxes);
        QVERIFY(messages);
        QVERIFY(left);
        QVERIFY(right);
        auto *splitter = qobject_cast<QSplitter *>(mailboxes->parentWidget());
        QVERIFY(splitter);
        QCOMPARE(splitter->indexOf(mailboxes), splitter->count() - 1);
        QVERIFY(right->isChecked());
        window.resize(1000, 700);
        window.show();
        QCoreApplication::processEvents();
        splitter->setSizes({700, 200});
        const int sidebarWidth = splitter->sizes().last();
        QVERIFY(sidebarWidth > 0);
        left->trigger();
        QCOMPARE(splitter->indexOf(mailboxes), 0);
        QVERIFY(qAbs(splitter->sizes().first() - sidebarWidth) <= 2);
        config->reparseConfiguration();
        QCOMPARE(KConfigGroup(config, QStringLiteral("Appearance")).readEntry("MailboxSidebarPosition", QString()),
                 QStringLiteral("left"));
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
        splitter = qobject_cast<QSplitter *>(mailboxes->parentWidget());
        QVERIFY(splitter);
        QCOMPARE(splitter->count(), 3);
        QCOMPARE(splitter->indexOf(mailboxes), 0);
        right->trigger();
        QCOMPARE(splitter->indexOf(mailboxes), splitter->count() - 1);
        QVERIFY(QMetaObject::invokeMethod(&window, "slotLayoutCompact"));
        splitter = qobject_cast<QSplitter *>(mailboxes->parentWidget());
        QVERIFY(splitter);
        QCOMPARE(splitter->indexOf(mailboxes), splitter->count() - 1);
        QVERIFY(splitter->sizes().last() < splitter->sizes().first());
        QVERIFY(QMetaObject::invokeMethod(&window, "slotLayoutOneAtTime"));
        QVERIFY(QMetaObject::invokeMethod(&window, "slotLayoutWide"));
        splitter = qobject_cast<QSplitter *>(mailboxes->parentWidget());
        QVERIFY(splitter);
        QCOMPARE(splitter->indexOf(mailboxes), splitter->count() - 1);
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
