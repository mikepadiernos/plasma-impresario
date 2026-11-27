/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Gui/MailTabBar.h"
#include "Gui/Window.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "UiUtils/MailTabs.h"
#include "UiUtils/SettingsStore.h"
#include "configure.cmake.h"
#ifdef TROJITA_HAVE_WEBENGINE
#include "Gui/WebEngineMessageProfile.h"
#endif
#include "static_plugins.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QSettings>
#include <QSignalSpy>
#include <QSplitter>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using UiUtils::MailTabs::Region;
using UiUtils::MailTabs::Session;
using UiUtils::MailTabs::Tab;

namespace {
void offlineAccount(QSettings &settings) {
  settings.setValue(Common::SettingsNames::imapMethodKey,
                    Common::SettingsNames::methodSSL);
  settings.setValue(Common::SettingsNames::imapStartMode,
                    Common::SettingsNames::netOffline);
  settings.setValue(Common::SettingsNames::guiShowSystray, false);
  settings.setValue(Common::SettingsNames::msaMethodKey,
                    Common::SettingsNames::methodSMTP);
}

void setTabSettings(QSettings &settings, bool enabled, const QString &region,
                    const QString &orientation) {
  auto config =
      KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
  UiUtils::SettingsStore::setApplication(
      *config, &settings, QStringLiteral("Tabs/Enabled"), enabled);
  UiUtils::SettingsStore::setApplication(*config, &settings,
                                         QStringLiteral("Tabs/Region"), region);
  UiUtils::SettingsStore::setApplication(
      *config, &settings, QStringLiteral("Tabs/Orientation"), orientation);
  config->sync();
}

Tab tab(const QString &accountId, const QString &mailbox, uint uid = 0,
        const QString &subject = QString()) {
  Tab result;
  result.accountId = accountId;
  result.mailbox = mailbox;
  result.uidValidity = uid ? 7 : 0;
  result.uid = uid;
  result.subject = subject;
  return result;
}
} // namespace

class MainWindowTabsTest : public QObject {
  Q_OBJECT
private slots:
  void cleanup() {
    KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"))
        ->deleteGroup(QStringLiteral("Tabs"));
    KSharedConfig::openStateConfig()->deleteGroup(QStringLiteral("MailTabs"));
  }

  void tabBarShapesOpenTowardsTheContent() {
    QCOMPARE(Gui::MailTabBar::shapeFor(Region::Header, Qt::Horizontal),
             QTabBar::RoundedNorth);
    QCOMPARE(Gui::MailTabBar::shapeFor(Region::Footer, Qt::Horizontal),
             QTabBar::RoundedSouth);
    QCOMPARE(Gui::MailTabBar::shapeFor(Region::Left, Qt::Vertical),
             QTabBar::RoundedWest);
    QCOMPARE(Gui::MailTabBar::shapeFor(Region::Main, Qt::Vertical),
             QTabBar::RoundedWest);
    QCOMPARE(Gui::MailTabBar::shapeFor(Region::Right, Qt::Vertical),
             QTabBar::RoundedEast);
  }

  void settingTheTabsDoesNotActivateOne() {
    Gui::MailTabBar bar;
    QSignalSpy activated(&bar, &Gui::MailTabBar::tabActivated);
    bar.setEntries({{QStringLiteral("Inbox"), QStringLiteral("Inbox")}}, 0);
    QCOMPARE(bar.count(), 1);
    // A single tab is the view itself and cannot be closed
    QVERIFY(!bar.tabsClosable());
    bar.setEntries({{QStringLiteral("Inbox"), QString()},
                    {QStringLiteral("R&D"), QString()}},
                   1);
    QCOMPARE(bar.count(), 2);
    QCOMPARE(bar.currentIndex(), 1);
    QCOMPARE(bar.tabText(1), QStringLiteral("R&&D"));
    QVERIFY(bar.tabsClosable());
    QCOMPARE(activated.count(), 0);
    bar.setCurrentIndex(0);
    QCOMPARE(activated.count(), 1);
    QCOMPARE(activated.first().first().toInt(), 0);
  }

  void tabsAreOffByDefault() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("legacy.ini")),
                       QSettings::IniFormat);
    offlineAccount(settings);
    Gui::MainWindow window(&settings);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *bar =
        window.findChild<Gui::MailTabBar *>(QStringLiteral("mailTabBar"));
    QVERIFY(bar);
    QVERIFY(!bar->isVisible());
    for (const char *name :
         {"action_new_tab", "action_close_tab", "action_next_tab",
          "action_previous_tab", "action_open_in_new_tab"}) {
      auto *action = window.findChild<QAction *>(QLatin1String(name));
      QVERIFY2(action, name);
      QVERIFY2(!action->isVisible(), name);
    }
    // The side column holds no tab bar, so it stays as the panels need it
    auto *leftColumn =
        window.findChild<QSplitter *>(QStringLiteral("shellLeftColumn"));
    QVERIFY(leftColumn);
    QCOMPARE(leftColumn->indexOf(bar), -1);
  }

  void savedTabsComeBackInTheirRegion() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("legacy.ini")),
                       QSettings::IniFormat);
    offlineAccount(settings);
    QString accountId;
    {
      Gui::MainWindow window(&settings);
      QVERIFY(!window.accountIds().isEmpty());
      accountId = window.accountIds().constFirst();
    }

    Session saved;
    saved.tabs = {tab(accountId, QStringLiteral("INBOX")),
                  tab(QStringLiteral("gone-account"), QStringLiteral("INBOX")),
                  tab(accountId, QStringLiteral("Projects/Work"), 42,
                      QStringLiteral("Quarterly plan"))};
    saved.current = 0;
    auto state = KSharedConfig::openStateConfig();
    UiUtils::SettingsStore::setTabs(*state, saved);
    setTabSettings(settings, true, QStringLiteral("left"),
                   QStringLiteral("horizontal"));

    Gui::MainWindow window(&settings);
    window.resize(1000, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *bar =
        window.findChild<Gui::MailTabBar *>(QStringLiteral("mailTabBar"));
    QVERIFY(bar);
    QTRY_VERIFY(bar->isVisible());
    QCOMPARE(bar->parentWidget()->objectName(),
             QStringLiteral("shellLeftColumn"));
    QCOMPARE(bar->shape(), QTabBar::RoundedNorth);
    // The tab of the removed account is gone
    QCOMPARE(bar->count(), 2);
    QCOMPARE(bar->tabText(0), QStringLiteral("INBOX"));
    QCOMPARE(bar->tabText(1), QStringLiteral("Quarterly plan"));
    QVERIFY(bar->tabToolTip(1).contains(QStringLiteral("Projects/Work")));
    QVERIFY(bar->tabsClosable());

    // The session is saved without subjects
    const Session written = UiUtils::SettingsStore::tabs(*state);
    QCOMPARE(written.tabs.size(), 2);
    QCOMPARE(written.tabs.at(1).uid, 42u);
    QVERIFY(written.tabs.at(1).subject.isEmpty());
    QVERIFY(!KConfigGroup(state, QStringLiteral("MailTabs"))
                 .readEntry("Session", QString())
                 .contains(QStringLiteral("Quarterly")));

    auto *nextTab =
        window.findChild<QAction *>(QStringLiteral("action_next_tab"));
    auto *closeTab =
        window.findChild<QAction *>(QStringLiteral("action_close_tab"));
    QVERIFY(nextTab && closeTab);
    QVERIFY(nextTab->isVisible() && nextTab->isEnabled());
    QVERIFY(
        nextTab->shortcuts().contains(QKeySequence(Qt::CTRL | Qt::Key_Tab)));
    nextTab->trigger();
    QCOMPARE(bar->currentIndex(), 1);
    QCOMPARE(UiUtils::SettingsStore::tabs(*state).current, 1);
    closeTab->trigger();
    QCOMPARE(bar->count(), 1);
    QCOMPARE(bar->currentIndex(), 0);
    QVERIFY(!closeTab->isEnabled());
    QVERIFY(!bar->tabsClosable());
    // The last tab stays
    closeTab->trigger();
    QCOMPARE(bar->count(), 1);
    QCOMPARE(UiUtils::SettingsStore::tabs(*state).tabs.size(), 1);

    // Moved to the main region and stacked, the tabs sit beside the main column
    setTabSettings(settings, true, QStringLiteral("main"),
                   QStringLiteral("vertical"));
    QVERIFY(QMetaObject::invokeMethod(&window, "applyTabSettings"));
    QCOMPARE(bar->parentWidget()->objectName(),
             QStringLiteral("shellMainArea"));
    QCOMPARE(bar->shape(), QTabBar::RoundedWest);
    auto *mainLayout =
        qobject_cast<QBoxLayout *>(bar->parentWidget()->layout());
    QVERIFY(mainLayout);
    QCOMPARE(mainLayout->direction(), QBoxLayout::LeftToRight);
    QCOMPARE(mainLayout->indexOf(bar), 0);
    auto *leftColumn =
        window.findChild<QSplitter *>(QStringLiteral("shellLeftColumn"));
    QCOMPARE(leftColumn->indexOf(bar), -1);

    // In the header, the bar follows the header and comes before the shell body
    setTabSettings(settings, true, QStringLiteral("header"),
                   QStringLiteral("horizontal"));
    QVERIFY(QMetaObject::invokeMethod(&window, "applyTabSettings"));
    QCOMPARE(bar->parentWidget(), window.centralWidget());
    auto *shellLayout = window.centralWidget()->layout();
    auto *body = window.findChild<QSplitter *>(QStringLiteral("shellBody"));
    QCOMPARE(shellLayout->indexOf(bar) + 1, shellLayout->indexOf(body));
    QVERIFY(bar->isVisible());

    // Turned off, the bar and its shortcuts go, and the saved tabs stay for
    // later
    setTabSettings(settings, false, QStringLiteral("header"),
                   QStringLiteral("horizontal"));
    QVERIFY(QMetaObject::invokeMethod(&window, "applyTabSettings"));
    QVERIFY(!bar->isVisible());
    QVERIFY(!nextTab->isVisible());
    QCOMPARE(UiUtils::SettingsStore::tabs(*state).tabs.size(), 1);
  }
};

int main(int argc, char **argv) {
  QTemporaryDir home;
  if (!home.isValid())
    return 1;
  qputenv("HOME", home.path().toUtf8());
  qputenv("XDG_CONFIG_HOME", home.filePath(QStringLiteral("config")).toUtf8());
  qputenv("XDG_DATA_HOME", home.filePath(QStringLiteral("data")).toUtf8());
  qputenv("XDG_CACHE_HOME", home.filePath(QStringLiteral("cache")).toUtf8());
  qputenv("XDG_STATE_HOME", home.filePath(QStringLiteral("state")).toUtf8());
  qputenv("DBUS_SESSION_BUS_ADDRESS",
          "unix:path=/nonexistent/plasma-impresario-test");
  QStandardPaths::setTestModeEnabled(true);
#ifdef TROJITA_HAVE_WEBENGINE
  Gui::WebEngineMessageProfile::registerScheme();
#endif
  Common::registerMetaTypes();
  QApplication app(argc, argv);
  for (auto location :
       {QStandardPaths::GenericConfigLocation, QStandardPaths::AppDataLocation,
        QStandardPaths::CacheLocation}) {
    if (!QStandardPaths::writableLocation(location).startsWith(
            home.path() + QLatin1Char('/')))
      return 1;
  }
  Q_INIT_RESOURCE(icons);
  Q_INIT_RESOURCE(license);
  KLocalizedString::setApplicationDomain("plasma-impresario");
  if (!Imap::Mailbox::SystemNetworkWatcher::init())
    return 1;
  MainWindowTabsTest test;
  return QTest::qExec(&test, argc, argv);
}

#include "test_MainWindowTabs.moc"
