/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/AccountRegistry.h"
#include "Common/AccountStatus.h"
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Gui/AccountSidebar.h"
#include "Gui/CapabilityActionGate.h"
#include "Gui/MailBoxTreeView.h"
#include "Gui/MsgListView.h"
#include "Gui/ShellHeader.h"
#include "Gui/Window.h"
#include "Imap/Model/AccountCapabilities.h"
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/SystemNetworkWatcher.h"
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
#include <QScreen>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QSignalSpy>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <algorithm>
#include <functional>
#include <QTreeView>
#include <QVBoxLayout>

namespace {

void configureOffline(QSettings &settings, const QString &host) {
  settings.setValue(Common::SettingsNames::imapMethodKey,
                    Common::SettingsNames::methodSSL);
  settings.setValue(Common::SettingsNames::imapHostKey, host);
  settings.setValue(Common::SettingsNames::imapStartMode,
                    Common::SettingsNames::netOffline);
}

/** Two offline accounts, Work (active) and Home; Home uses @p secondProtocol */
void seedTwoAccounts(
    const QTemporaryDir &directory, QSettings &settings,
    Imap::AccountProtocol secondProtocol = Imap::AccountProtocol::Imap) {
  settings.setValue(Common::SettingsNames::guiShowSystray, false);
  settings.setValue(
      QStringLiteral("accounts/ids"),
      QStringList{QStringLiteral("account-1"), QStringLiteral("account-2")});
  settings.setValue(QStringLiteral("accounts/active"),
                    QStringLiteral("account-1"));
  settings.setValue(QStringLiteral("accounts/next"), 3);
  settings.setValue(QStringLiteral("accounts/legacy"),
                    QStringLiteral("account-1"));
  settings.setValue(QStringLiteral("accounts/names/account-1"),
                    QStringLiteral("Work"));
  settings.setValue(QStringLiteral("accounts/names/account-2"),
                    QStringLiteral("Home"));
  QSettings first(directory.filePath(QStringLiteral("main-account-1.conf")),
                  QSettings::IniFormat);
  configureOffline(first, QStringLiteral("work.example.test"));
  first.sync();
  QSettings second(directory.filePath(QStringLiteral("main-account-2.conf")),
                   QSettings::IniFormat);
  configureOffline(second, QStringLiteral("home.example.test"));
  Imap::AccountCapabilities::setProtocol(second, secondProtocol);
  second.sync();
}

QAction *action(Gui::MainWindow &window, const QString &shortcutName) {
  for (QAction *candidate : window.findChildren<QAction *>()) {
    if (candidate->objectName() == shortcutName)
      return candidate;
  }
  return nullptr;
}

QAction *actionByText(Gui::MainWindow &window, const QString &text) {
  for (QAction *candidate : window.findChildren<QAction *>()) {
    if (candidate->text() == text)
      return candidate;
  }
  return nullptr;
}

/** @short Answer the next modal dialog with @arg answer once it is shown */
void answerNextDialog(std::function<void(QWidget *)> answer) {
  auto *poll = new QTimer;
  QObject::connect(poll, &QTimer::timeout, poll, [poll, answer]() {
    if (QWidget *dialog = QApplication::activeModalWidget()) {
      poll->deleteLater();
      poll->stop();
      answer(dialog);
    }
  });
  poll->start(20);
}

QMenu *openAccountMenu(Gui::MainWindow &window, QToolButton *header) {
  emit header->customContextMenuRequested(QPoint(1, 1));
  QMenu *menu = nullptr;
  for (QMenu *candidate : window.findChildren<QMenu *>(
           QStringLiteral("accountMenu"))) {
    if (candidate->isVisible())
      menu = candidate;
  }
  return menu;
}

std::unique_ptr<Common::AccountRegistry>
registry(const QTemporaryDir &directory, QSettings &index) {
  const QString prefix = directory.filePath(QStringLiteral("account-"));
  return std::make_unique<Common::AccountRegistry>(
      &index, [prefix](const QString &accountId) {
        return std::make_unique<QSettings>(
            prefix + accountId + QStringLiteral(".conf"), QSettings::IniFormat);
      });
}

/** A splitter state as saved by the builds before the shell */
QByteArray legacySplitterState(Qt::Orientation orientation,
                               const QList<int> &sizes) {
  QSplitter splitter(orientation);
  int extent = splitter.handleWidth() * (int(sizes.size()) - 1);
  for (int size : sizes) {
    splitter.addWidget(new QWidget);
    extent += size;
  }
  splitter.resize(orientation == Qt::Horizontal ? QSize(extent, 400)
                                                : QSize(400, extent));
  splitter.show();
  if (!QTest::qWaitForWindowExposed(&splitter))
    return {};
  splitter.setSizes(sizes);
  return splitter.saveState();
}

/** Main settings for one offline account, with pane sizes of an older build
 * in @p layoutKey's group */
QString seedLegacyLayout(QSettings &settings, const QString &layoutKey,
                         const QByteArray &outer, const QByteArray &inner,
                         bool treeOnRight) {
  configureOffline(settings, QStringLiteral("work.example.test"));
  settings.setValue(Common::SettingsNames::guiShowSystray, false);
  settings.setValue(Common::SettingsNames::settingsVersion, 2);
  const QString group = layoutKey.arg(QString::number(
      QGuiApplication::primaryScreen()->availableGeometry().width()));
  settings.beginGroup(group);
  settings.setValue(Common::SettingsNames::mainHSplitterState, outer);
  if (!inner.isEmpty())
    settings.setValue(Common::SettingsNames::mainVSplitterState, inner);
  settings.setValue(QStringLiteral("mailboxSidebarOnRight"), treeOnRight);
  settings.endGroup();
  return group;
}

void verifyLegacyKeysRemoved(QSettings &settings, const QString &group) {
  settings.beginGroup(group);
  QVERIFY(!settings.contains(Common::SettingsNames::mainHSplitterState));
  QVERIFY(!settings.contains(Common::SettingsNames::mainVSplitterState));
  settings.endGroup();
}

} // namespace

class MainWindowShellTest : public QObject {
  Q_OBJECT
private slots:
  void capabilityGateHidesOnlyUnsupportedActions() {
    QAction folders(QStringLiteral("Create folder"));
    QAction move(QStringLiteral("Archive"));
    QAction local(QStringLiteral("Mark as read"));
    QAction hiddenElsewhere(QStringLiteral("Hidden for another reason"));
    hiddenElsewhere.setVisible(false);
    Gui::CapabilityActionGate gate;
    gate.addAction(&folders, Imap::AccountOperation::ServerFolders);
    gate.addAction(&move, Imap::AccountOperation::ServerSideMove);
    gate.addAction(&hiddenElsewhere, Imap::AccountOperation::ServerFolders);

    gate.apply(Imap::AccountCapabilities(Imap::AccountProtocol::Imap));
    QVERIFY(folders.isVisible());
    QVERIFY(move.isVisible());
    QVERIFY(!hiddenElsewhere.isVisible());

    gate.apply(Imap::AccountCapabilities(Imap::AccountProtocol::Pop3));
    QVERIFY(!folders.isVisible());
    QVERIFY(!move.isVisible());
    QVERIFY(local.isVisible());
    QVERIFY(gate.isHiddenByGate(&folders));
    QVERIFY(!gate.isHiddenByGate(&hiddenElsewhere));

    // Applying the same capabilities twice changes nothing
    gate.apply(Imap::AccountCapabilities(Imap::AccountProtocol::Pop3));
    QVERIFY(!folders.isVisible());

    gate.apply(Imap::AccountCapabilities(Imap::AccountProtocol::Jmap));
    QVERIFY(folders.isVisible());
    QVERIFY(move.isVisible());
    // An action hidden for another reason is not shown by the gate
    QVERIFY(!hiddenElsewhere.isVisible());
    QVERIFY(!gate.isHiddenByGate(&folders));
  }

  void unsupportedActionsFollowTheActiveAccount() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("main.ini")),
                       QSettings::IniFormat);
    seedTwoAccounts(directory, settings, Imap::AccountProtocol::Pop3);

    Gui::MainWindow window(&settings);
    QAction *archive = action(window, QStringLiteral("action_archive"));
    QAction *junk = action(window, QStringLiteral("action_mark_as_junk"));
    QAction *tag = action(window, QStringLiteral("action_tag_1"));
    QAction *read = action(window, QStringLiteral("action_mark_as_read"));
    QAction *createFolder =
        actionByText(window, QStringLiteral("Create &New Mailbox..."));
    QAction *subscribed =
        actionByText(window, QStringLiteral("Show Only S&ubscribed Folders"));
    QVERIFY(archive && junk && tag && read && createFolder && subscribed);
    for (QAction *each : {archive, junk, tag, read, createFolder, subscribed})
      QVERIFY2(each->isVisible(), qPrintable(each->text()));

    auto *sidebar = window.findChild<Gui::AccountSidebar *>();
    QVERIFY(sidebar);
    QCOMPARE(sidebar->headers().size(), 2);
    // Work is the migrated account, so it keeps the legacy credential id
    const QString workCredentials = window.imapAccess()->credentialAccountId();
    sidebar->headers().at(1)->click();
    QCOMPARE(window.imapAccess()->credentialAccountId(),
             QStringLiteral("account-2"));
    for (QAction *each : {archive, junk, tag, createFolder, subscribed})
      QVERIFY2(!each->isVisible(), qPrintable(each->text()));
    // Read state can be kept locally, so it stays available
    QVERIFY(read->isVisible());

    sidebar->headers().at(0)->click();
    QCOMPARE(window.imapAccess()->credentialAccountId(), workCredentials);
    for (QAction *each : {archive, junk, tag, read, createFolder, subscribed})
      QVERIFY2(each->isVisible(), qPrintable(each->text()));
  }

  void sidebarShowsHeadersOnlyForSeveralAccounts() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings index(directory.filePath(QStringLiteral("index.ini")),
                    QSettings::IniFormat);
    auto accounts = registry(directory, index);
    const QString work = accounts->addAccount(QStringLiteral("Work"));
    accounts->setActiveAccount(work);
    Common::AccountStatusTracker status;

    QWidget host;
    auto *layout = new QVBoxLayout(&host);
    auto *folders = new QTreeView;
    auto *sidebar = new Gui::AccountSidebar(folders);
    layout->addWidget(sidebar);
    sidebar->refresh(*accounts, status);
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));

    // A single account looks as before: only the folder tree
    QVERIFY(sidebar->headers().isEmpty());
    QVERIFY(folders->isVisible());
    QCOMPARE(sidebar->folders(), folders);

    const QString home = accounts->addAccount(QStringLiteral("Home"));
    const QString archive = accounts->addAccount(QStringLiteral("Archive"));
    accounts->setActiveAccount(home);
    status.setStatus(archive, Common::AccountStatusKind::AuthFailed,
                     QStringLiteral("Rejected password"));
    sidebar->refresh(*accounts, status);
    const QList<QToolButton *> headers = sidebar->headers();
    QCOMPARE(headers.size(), 3);
    QCOMPARE(headers.at(0)->text(), QStringLiteral("Work"));
    QCOMPARE(headers.at(1)->text(), QStringLiteral("Home"));
    QCOMPARE(headers.at(2)->text(), QStringLiteral("Archive"));
    // The folders follow the active account's header
    auto *sidebarLayout = sidebar->layout();
    QCOMPARE(sidebarLayout->indexOf(folders),
             sidebarLayout->indexOf(headers.at(1)) + 1);
    QVERIFY(sidebarLayout->indexOf(headers.at(2)) >
            sidebarLayout->indexOf(folders));
    QVERIFY(
        headers.at(2)->toolTip().contains(QStringLiteral("Rejected password")));
    QVERIFY(!headers.at(1)->toolTip().contains(QLatin1Char(':')));
    for (QToolButton *header : headers)
      QCOMPARE(header->focusPolicy(), Qt::StrongFocus);

    // A status change keeps the header widgets, so that keyboard focus stays
    // where it is
    status.setStatus(archive, Common::AccountStatusKind::Connected);
    sidebar->refresh(*accounts, status);
    QCOMPARE(sidebar->headers(), headers);
    QVERIFY(!headers.at(2)->toolTip().contains(
        QStringLiteral("Rejected password")));

    // The active header collapses and expands its folders
    headers.at(1)->click();
    QVERIFY(!sidebar->foldersExpanded());
    QVERIFY(!folders->isVisible());
    headers.at(1)->click();
    QVERIFY(folders->isVisible());

    // Another header asks to switch accounts
    QSignalSpy activated(sidebar, &Gui::AccountSidebar::accountActivated);
    headers.at(0)->click();
    QCOMPARE(activated.size(), 1);
    QCOMPARE(activated.at(0).at(0).toString(), work);

    // Keyboard: arrows move between headers, Return activates
    headers.at(1)->setFocus();
    QTest::keyClick(headers.at(1), Qt::Key_Down);
    QCOMPARE(QApplication::focusWidget(), headers.at(2));
    QTest::keyClick(headers.at(2), Qt::Key_Return);
    QCOMPARE(activated.size(), 2);
    QCOMPARE(activated.at(1).at(0).toString(), archive);
    QTest::keyClick(headers.at(2), Qt::Key_Up);
    QTest::keyClick(headers.at(1), Qt::Key_Up);
    QCOMPARE(QApplication::focusWidget(), headers.at(0));

    // A collapsed account opens again when it becomes active through other
    // means, such as the toolbar selector
    headers.at(1)->click();
    QVERIFY(!folders->isVisible());
    accounts->setActiveAccount(work);
    sidebar->refresh(*accounts, status);
    QVERIFY(folders->isVisible());
    QCOMPARE(sidebarLayout->indexOf(folders),
             sidebarLayout->indexOf(headers.at(0)) + 1);

    // Removing accounts down to one hides the headers again
    accounts->removeAccount(home);
    accounts->removeAccount(archive);
    sidebar->refresh(*accounts, status);
    QVERIFY(sidebar->headers().isEmpty());
    QVERIFY(folders->isVisible());
  }

  void sidebarGroupsAccountsInTheWindow() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("main.ini")),
                       QSettings::IniFormat);
    seedTwoAccounts(directory, settings);

    Gui::MainWindow window(&settings);
    window.resize(1000, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *sidebar = window.findChild<Gui::AccountSidebar *>();
    auto *tree = window.findChild<Gui::MailBoxTreeView *>();
    auto *leftColumn =
        window.findChild<QSplitter *>(QStringLiteral("shellLeftColumn"));
    QVERIFY(sidebar && tree && leftColumn);
    QCOMPARE(sidebar->folders(), tree);
    QCOMPARE(sidebar->parentWidget(), leftColumn);
    QCOMPARE(sidebar->headers().size(), 2);
    QCOMPARE(sidebar->headers().at(0)->text(), QStringLiteral("Work"));

    sidebar->headers().at(1)->click();
    QCOMPARE(settings.value(QStringLiteral("accounts/active")).toString(),
             QStringLiteral("account-2"));
    QVERIFY(tree->isVisible());
    QCOMPARE(sidebar->layout()->indexOf(tree),
             sidebar->layout()->indexOf(sidebar->headers().at(1)) + 1);

    // The sidebar, headers included, is the first page of the one-at-a-time
    // layout
    QVERIFY(QMetaObject::invokeMethod(&window, "slotLayoutOneAtTime"));
    auto *stack = qobject_cast<QStackedWidget *>(sidebar->parentWidget());
    QVERIFY(stack);
    QCOMPARE(stack->currentWidget(), sidebar);
    QVERIFY(QMetaObject::invokeMethod(&window, "slotLayoutCompact"));
    QCOMPARE(sidebar->parentWidget(), leftColumn);
    QVERIFY(sidebar->isVisible());

    // F6 enters the sidebar at the folder tree, and focus on a header counts as
    // being in the sidebar
    auto *next =
        window.findChild<QAction *>(QStringLiteral("action_focus_next_region"));
    auto *messages = window.findChild<Gui::MsgListView *>();
    QVERIFY(next && messages);
    window.activateWindow();
    sidebar->headers().at(0)->setFocus();
    next->trigger();
    QCOMPARE(window.focusWidget(), messages);
  }

  void sidebarHeaderMenuActsOnItsAccount() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("main.ini")),
                       QSettings::IniFormat);
    seedTwoAccounts(directory, settings);
    auto active = [&settings]() {
      return settings.value(QStringLiteral("accounts/active")).toString();
    };

    Gui::MainWindow window(&settings);
    window.resize(1000, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *sidebar = window.findChild<Gui::AccountSidebar *>();
    QVERIFY(sidebar);
    QCOMPARE(sidebar->headers().size(), 2);
    for (QToolButton *header : sidebar->headers())
      QCOMPARE(header->contextMenuPolicy(), Qt::CustomContextMenu);

    // Renaming another account leaves the active one alone
    QMenu *menu = openAccountMenu(window, sidebar->headers().at(1));
    QVERIFY(menu);
    auto *rename = menu->findChild<QAction *>(
        QStringLiteral("action_account_menu_rename"));
    auto *remove = menu->findChild<QAction *>(
        QStringLiteral("action_account_menu_remove"));
    QVERIFY(rename && remove);
    QVERIFY(remove->isEnabled());
    answerNextDialog([](QWidget *dialog) {
      auto *input = qobject_cast<QInputDialog *>(dialog);
      QVERIFY(input);
      input->setTextValue(QStringLiteral("Family"));
      input->accept();
    });
    rename->trigger();
    menu->close();
    QTRY_COMPARE(
        settings.value(QStringLiteral("accounts/names/account-2")).toString(),
        QStringLiteral("Family"));
    QCOMPARE(active(), QStringLiteral("account-1"));
    QCOMPARE(sidebar->headers().at(1)->text(), QStringLiteral("Family"));

    // The settings dialog edits the shown account, so it switches first
    menu = openAccountMenu(window, sidebar->headers().at(1));
    QVERIFY(menu);
    auto *accountSettings = menu->findChild<QAction *>(
        QStringLiteral("action_account_menu_settings"));
    QVERIFY(accountSettings);
    QString dialogTitle;
    answerNextDialog([&dialogTitle](QWidget *dialog) {
      dialogTitle = dialog->windowTitle();
      qobject_cast<QDialog *>(dialog)->reject();
    });
    accountSettings->trigger();
    menu->close();
    QTRY_VERIFY(dialogTitle.contains(QStringLiteral("Family")));
    QCOMPARE(active(), QStringLiteral("account-2"));

    // Removing the account that is not shown keeps the shown one
    menu = openAccountMenu(window, sidebar->headers().at(0));
    QVERIFY(menu);
    remove = menu->findChild<QAction *>(
        QStringLiteral("action_account_menu_remove"));
    QVERIFY(remove);
    QString question;
    answerNextDialog([&question](QWidget *dialog) {
      auto *box = qobject_cast<QMessageBox *>(dialog);
      QVERIFY(box);
      question = box->text();
      box->button(QMessageBox::Yes)->click();
    });
    remove->trigger();
    menu->close();
    QTRY_COMPARE(settings.value(QStringLiteral("accounts/ids")).toStringList(),
                 QStringList{QStringLiteral("account-2")});
    QVERIFY(question.contains(QStringLiteral("Work")));
    QCOMPARE(active(), QStringLiteral("account-2"));
    QVERIFY(sidebar->headers().isEmpty());
  }

  void headerRegionIsOptional() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("main.ini")),
                       QSettings::IniFormat);
    seedTwoAccounts(directory, settings);

    Gui::MainWindow window(&settings);
    window.resize(1000, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *header = window.findChild<Gui::ShellHeader *>();
    auto *body = window.findChild<QSplitter *>(QStringLiteral("shellBody"));
    QVERIFY(header && body);
    QCOMPARE(header->parentWidget(), window.centralWidget());
    QCOMPARE(body->parentWidget(), window.centralWidget());
    auto *shellLayout = window.centralWidget()->layout();
    QVERIFY(shellLayout->indexOf(header) < shellLayout->indexOf(body));

    // Off by default, so the default look is unchanged
    QVERIFY(!header->isVisible());
    QAction *toggle = header->toggleAction();
    QVERIFY(!toggle->isChecked());
    bool inViewMenu = false;
    for (QMenu *menu : window.menuBar()->findChildren<QMenu *>()) {
      if (menu->actions().contains(toggle))
        inViewMenu = true;
    }
    QVERIFY(inViewMenu);

    QCOMPARE(header->contextText(), QStringLiteral("Work"));
    toggle->trigger();
    QVERIFY(header->isVisible());
    auto config =
        KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
    config->reparseConfiguration();
    QVERIFY(KConfigGroup(config, QStringLiteral("Appearance"))
                .readEntry("ShowHeader", false));

    auto *sidebar = window.findChild<Gui::AccountSidebar *>();
    QVERIFY(sidebar);
    sidebar->headers().at(1)->click();
    QCOMPARE(header->contextText(), QStringLiteral("Home"));
    QVERIFY(header->statusText().isEmpty());

    // The header is a focus region of its own while it is shown
    auto *previous = window.findChild<QAction *>(
        QStringLiteral("action_focus_previous_region"));
    QVERIFY(previous);
    window.activateWindow();
    window.findChild<QAction *>(QStringLiteral("action_focus_mailbox_tree"))
        ->trigger();
    previous->trigger();
    // The toolbar's account selector comes between the header and the folders
    // when the toolbar shows it
    if (window.focusWidget() != header->focusTarget())
      previous->trigger();
    QCOMPARE(window.focusWidget(), header->focusTarget());

    toggle->trigger();
    QVERIFY(!header->isVisible());
    config->reparseConfiguration();
    QVERIFY(!KConfigGroup(config, QStringLiteral("Appearance"))
                 .readEntry("ShowHeader", true));
  }

  void legacyCompactPaneSizesCarryOver() {
    const QByteArray outer = legacySplitterState(Qt::Horizontal, {220, 780});
    const QByteArray inner = legacySplitterState(Qt::Vertical, {100, 500});
    QVERIFY(!outer.isEmpty() && !inner.isEmpty());
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("main.ini")),
                       QSettings::IniFormat);
    const QString group = seedLegacyLayout(
        settings, Common::SettingsNames::guiSizesInMainWinWhenCompact, outer,
        inner, false);

    Gui::MainWindow window(&settings);
    window.resize(1000, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *row = window.findChild<QSplitter *>(QStringLiteral("shellRow"));
    auto *mainColumn =
        window.findChild<QSplitter *>(QStringLiteral("shellMainColumn"));
    QVERIFY(row && mainColumn);
    QCOMPARE(mainColumn->orientation(), Qt::Vertical);
    QTRY_VERIFY2(qAbs(row->sizes().first() - 220) <= 2,
                 qPrintable(QString::number(row->sizes().first())));
    // The message list keeps about a sixth of the column
    const QList<int> panes = mainColumn->sizes();
    QCOMPARE(panes.size(), 2);
    QVERIFY2(panes.at(1) > panes.at(0) * 3,
             qPrintable(QStringLiteral("%1 and %2")
                            .arg(panes.at(0))
                            .arg(panes.at(1))));
    verifyLegacyKeysRemoved(settings, group);
  }

  void legacyWidePaneSizesCarryOver() {
    // The folder tree was on the right; the shell shows it on the left
    const QByteArray outer =
        legacySplitterState(Qt::Horizontal, {250, 550, 200});
    QVERIFY(!outer.isEmpty());
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("main.ini")),
                       QSettings::IniFormat);
    settings.setValue(Common::SettingsNames::guiMainWindowLayout,
                      Common::SettingsNames::guiMainWindowLayoutWide);
    const QString group = seedLegacyLayout(
        settings, Common::SettingsNames::guiSizesInMainWinWhenWide, outer,
        QByteArray(), true);

    Gui::MainWindow window(&settings);
    // Wide enough that the reader is not held at its minimum width
    window.resize(1600, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *row = window.findChild<QSplitter *>(QStringLiteral("shellRow"));
    auto *mainColumn =
        window.findChild<QSplitter *>(QStringLiteral("shellMainColumn"));
    auto *sidebar = window.findChild<Gui::AccountSidebar *>();
    QVERIFY(row && mainColumn && sidebar);
    QCOMPARE(sidebar->parentWidget()->objectName(),
             QStringLiteral("shellLeftColumn"));
    QCOMPARE(mainColumn->orientation(), Qt::Horizontal);
    QTRY_VERIFY2(qAbs(row->sizes().first() - 200) <= 2,
                 qPrintable(QString::number(row->sizes().first())));
    // The message list and the reader keep their 5:11 split instead of halves
    const QList<int> panes = mainColumn->sizes();
    QCOMPARE(panes.size(), 2);
    const double ratio = double(panes.at(0)) / std::max(1, panes.at(1));
    QVERIFY2(ratio > 0.35 && ratio < 0.6, qPrintable(QString::number(ratio)));
    verifyLegacyKeysRemoved(settings, group);
  }

  void headerShowsFolderAndStatus() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings index(directory.filePath(QStringLiteral("index.ini")),
                    QSettings::IniFormat);
    auto accounts = registry(directory, index);
    const QString work = accounts->addAccount(QStringLiteral("Work"));
    accounts->setActiveAccount(work);
    Common::AccountStatusTracker status;
    Gui::ShellHeader header(
        KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")));
    header.setAccount(*accounts, status);
    QCOMPARE(header.contextText(), QStringLiteral("Work"));
    header.setFolder(QStringLiteral("INBOX"));
    QCOMPARE(header.contextText(),
             i18nc("@label account name, then the folder shown from it",
                   "%1 › %2", QStringLiteral("Work"), QStringLiteral("INBOX")));
    status.setStatus(work, Common::AccountStatusKind::AuthRequired);
    header.setAccount(*accounts, status);
    QCOMPARE(header.statusText(), Common::AccountStatusTracker::label(
                                      Common::AccountStatusKind::AuthRequired));
    QVERIFY(header.focusTarget()->accessibleDescription().contains(
        header.statusText()));
    status.setStatus(work, Common::AccountStatusKind::Connected);
    header.setAccount(*accounts, status);
    QVERIFY(header.statusText().isEmpty());
    header.setFolder(QString());
    QCOMPARE(header.contextText(), QStringLiteral("Work"));
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
  qputenv("DBUS_SESSION_BUS_ADDRESS",
          "unix:path=/nonexistent/plasma-impresario-test");
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
  MainWindowShellTest test;
  return QTest::qExec(&test, argc, argv);
}

#include "test_MainWindowShell.moc"
