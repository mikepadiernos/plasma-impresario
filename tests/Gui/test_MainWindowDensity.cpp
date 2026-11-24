/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Composer/Submission.h"
#include "Gui/ComposeWidget.h"
#include "Gui/DensityDelegate.h"
#include "Gui/DensityMenu.h"
#include "Gui/MailBoxTreeView.h"
#include "Gui/MsgListView.h"
#include "Gui/Window.h"
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MsgListModel.h"
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
#include <QLabel>
#include <QScreen>
#include <QSettings>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QStatusBar>
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
        settings.setValue(Common::SettingsNames::msaMethodKey, Common::SettingsNames::methodSMTP);

        auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
        KConfigGroup(config, QStringLiteral("Appearance")).writeEntry("MailboxSidebarPosition", QStringLiteral("right"));
        QVERIFY(config->sync());
        config->reparseConfiguration();
        Gui::MainWindow window(&settings);
        auto *status = window.statusBar()->findChild<QLabel *>(QStringLiteral("connectionStatus"));
        QVERIFY(status);
        window.show();
        QVERIFY(status->isVisible());
        QCoreApplication::processEvents();
        QCOMPARE(status->text(), Gui::MainWindow::tr("Offline"));
        auto *messageModel = qobject_cast<Imap::Mailbox::MsgListModel *>(window.imapAccess()->msgListModel());
        QVERIFY(messageModel);
        QStandardItemModel mailboxModel;
        mailboxModel.appendRow(new QStandardItem(QStringLiteral("Archive")));
        const QModelIndex mailbox = mailboxModel.index(0, 0);
        mailboxModel.setData(mailbox, QStringLiteral("Archive"), Imap::Mailbox::RoleMailboxName);
        auto state = KSharedConfig::openStateConfig();
        KConfigGroup selection(state, QStringLiteral("MailboxSelection"));
        KConfigGroup accountSelection = selection.group(window.imapAccess()->credentialAccountId());
        KConfigGroup otherAccount = selection.group(QStringLiteral("another-account"));
        otherAccount.writeEntry("Mailbox", QStringLiteral("Other"));
        accountSelection.deleteGroup();
        messageModel->mailboxChanged(mailbox);
        QVERIFY(accountSelection.sync());
        state->reparseConfiguration();
        QCOMPARE(accountSelection.readEntry("Mailbox", QString()), QStringLiteral("Archive"));
        QCOMPARE(otherAccount.readEntry("Mailbox", QString()), QStringLiteral("Other"));
        QStandardItemModel selectedMessage(1, 1);
        const QModelIndex selectedIndex = selectedMessage.index(0, 0);
        selectedMessage.setData(selectedIndex, QStringLiteral("Archive"), Imap::Mailbox::RoleMailboxName);
        selectedMessage.setData(selectedIndex, 123u, Imap::Mailbox::RoleMailboxUidValidity);
        selectedMessage.setData(selectedIndex, 42u, Imap::Mailbox::RoleMessageUid);
        QVERIFY(QMetaObject::invokeMethod(&window, "saveMessageSelection", Q_ARG(QModelIndex, selectedIndex)));
        state->reparseConfiguration();
        QCOMPARE(accountSelection.readEntry("MessageMailbox", QString()), QStringLiteral("Archive"));
        QCOMPARE(accountSelection.readEntry("MessageUidValidity", 0u), 123u);
        QCOMPARE(accountSelection.readEntry("MessageUid", 0u), 42u);
        selectedMessage.setData(selectedIndex, 0u, Imap::Mailbox::RoleMailboxUidValidity);
        QVERIFY(QMetaObject::invokeMethod(&window, "saveMessageSelection", Q_ARG(QModelIndex, selectedIndex)));
        state->reparseConfiguration();
        QCOMPARE(accountSelection.readEntry("MessageUid", 0u), 0u);
        QCOMPARE(otherAccount.readEntry("Mailbox", QString()), QStringLiteral("Other"));
        window.imapModel()->connectionStateChanged(1u, Imap::CONN_STATE_NONE);
        QCOMPARE(status->text(), Gui::MainWindow::tr("Offline"));
        window.imapModel()->networkPolicyOnline();
        window.imapModel()->connectionStateChanged(1u, Imap::CONN_STATE_LOGIN);
        QCOMPARE(status->text(), Imap::connectionStateToString(Imap::CONN_STATE_LOGIN));
        window.imapModel()->networkError(QStringLiteral("Host lookup failed <details>"));
        QCOMPARE(status->text(), i18nc("@info:status", "Network error"));
        QVERIFY(status->toolTip().contains(QStringLiteral("&lt;details&gt;")));
        QCOMPARE(status->accessibleDescription(), QStringLiteral("Host lookup failed <details>"));
        window.imapModel()->connectionStateChanged(1u, Imap::CONN_STATE_NONE);
        QCOMPARE(status->text(), i18nc("@info:status", "Network error"));
        QCOMPARE(status->accessibleDescription(), QStringLiteral("Host lookup failed <details>"));
        window.imapModel()->authAttemptFailed(QStringLiteral("TLS rejected <details>"));
        QCOMPARE(status->text(), i18nc("@info:status", "Connection or authentication failed"));
        QVERIFY(status->toolTip().contains(QStringLiteral("&lt;details&gt;")));
        QCOMPARE(status->accessibleDescription(), QStringLiteral("TLS rejected <details>"));
        window.imapModel()->connectionStateChanged(1u, Imap::CONN_STATE_NONE);
        QCOMPARE(status->text(), i18nc("@info:status", "Connection or authentication failed"));
        window.imapModel()->connectionStateChanged(1u, Imap::CONN_STATE_SELECTED);
        window.imapModel()->connectionStateChanged(1u, Imap::CONN_STATE_SELECTED);
        QCOMPARE(status->text(), Imap::connectionStateToString(Imap::CONN_STATE_SELECTED));
        QVERIFY(status->toolTip().isEmpty());
        QVERIFY(status->accessibleDescription().isEmpty());
        window.imapModel()->networkPolicyOffline();

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

        const auto sizesBeforeDensityChange = splitter->sizes();
        menu->actions().at(1)->trigger();
        QVERIFY(mailboxDelegate->sizeHint(option, index).height() > mailboxHeight);
        QVERIFY(messageDelegate->sizeHint(option, index).height() > messageHeight);
        QCOMPARE(splitter->sizes(), sizesBeforeDensityChange);
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

        const QByteArray initialProfile = qgetenv("TROJITA_PROFILE");
        qputenv("TROJITA_PROFILE", "different-profile");
        auto *composer = Gui::ComposeWidget::createBlank(&window);
        if (initialProfile.isEmpty())
            qunsetenv("TROJITA_PROFILE");
        else
            qputenv("TROJITA_PROFILE", initialProfile);
        QVERIFY(composer);
        auto *submission = composer->findChild<Composer::Submission *>();
        QVERIFY(submission);
        QCOMPARE(submission->accountId(), initialProfile.isEmpty() ? QStringLiteral("account-0") : QString::fromUtf8(initialProfile));
        delete composer;

        splitter->setSizes({450, 250, 150});
        const QByteArray expectedSplitterState = splitter->saveState();
        splitter->splitterMoved(450, 1);
        window.close();
        settings.sync();
        const QString layoutGroup = Common::SettingsNames::guiSizesInMainWinWhenWide.arg(
            QString::number(window.screen()->availableGeometry().width()));
        settings.beginGroup(layoutGroup);
        QCOMPARE(settings.value(Common::SettingsNames::mainHSplitterState).toByteArray(), expectedSplitterState);
        settings.endGroup();
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
    qputenv("TROJITA_PROFILE", qgetenv("PLASMA_TEST_PROFILE") == "named" ? QByteArray("work-profile") : QByteArray());
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
