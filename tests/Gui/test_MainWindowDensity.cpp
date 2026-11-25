/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Composer/Submission.h"
#include "Gui/AccountSidebar.h"
#include "Gui/AttachmentPanel.h"
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

#include <algorithm>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QScreen>
#include <QSettings>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStackedWidget>
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
        auto *sidebar = window.findChild<Gui::AccountSidebar *>();
        QVERIFY(sidebar);
        QVERIFY(sidebar->isAncestorOf(mailboxes));
        QVERIFY(messages);
        QVERIFY(left);
        QVERIFY(right);
        auto *body = window.findChild<QSplitter *>(QStringLiteral("shellBody"));
        auto *row = window.findChild<QSplitter *>(QStringLiteral("shellRow"));
        auto *leftColumn = window.findChild<QSplitter *>(QStringLiteral("shellLeftColumn"));
        auto *rightColumn = window.findChild<QSplitter *>(QStringLiteral("shellRightColumn"));
        auto *mainColumn = window.findChild<QSplitter *>(QStringLiteral("shellMainColumn"));
        auto *footer = window.findChild<QSplitter *>(QStringLiteral("shellFooter"));
        QVERIFY(body && row && leftColumn && rightColumn && mainColumn && footer);
        QCOMPARE(body->parentWidget(), window.centralWidget());
        QCOMPARE(row->indexOf(leftColumn), 0);
        QCOMPARE(row->indexOf(mainColumn), 1);
        QCOMPARE(row->indexOf(rightColumn), 2);
        QCOMPARE(body->indexOf(footer), 1);
        QCOMPARE(sidebar->parentWidget(), rightColumn);
        QVERIFY(right->isChecked());
        window.resize(1000, 700);
        window.show();
        QCoreApplication::processEvents();
        QVERIFY(rightColumn->isVisible());
        QVERIFY(!leftColumn->isVisible());
        QVERIFY(!footer->isVisible());
        QVERIFY(row->sizes().at(1) > row->sizes().last());
        row->setSizes({0, 700, 200});
        const int sidebarWidth = row->sizes().last();
        QVERIFY(sidebarWidth > 0);
        left->trigger();
        QCOMPARE(sidebar->parentWidget(), leftColumn);
        QVERIFY(leftColumn->isVisible());
        QVERIFY(!rightColumn->isVisible());
        QVERIFY(qAbs(row->sizes().first() - sidebarWidth) <= 2);
        QCOMPARE(row->sizes().last(), 0);
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

        const auto sizesBeforeDensityChange = row->sizes();
        menu->actions().at(1)->trigger();
        QVERIFY(mailboxDelegate->sizeHint(option, index).height() > mailboxHeight);
        QVERIFY(messageDelegate->sizeHint(option, index).height() > messageHeight);
        QCOMPARE(row->sizes(), sizesBeforeDensityChange);
        QVERIFY(QMetaObject::invokeMethod(&window, "slotLayoutWide"));
        QCOMPARE(sidebar->parentWidget(), leftColumn);
        QCOMPARE(mainColumn->orientation(), Qt::Horizontal);
        QCOMPARE(mainColumn->count(), 2);
        auto *readerColumn = window.findChild<QSplitter *>(QStringLiteral("readerColumn"));
        QVERIFY(readerColumn);
        QCOMPARE(readerColumn->orientation(), Qt::Vertical);
        QCOMPARE(readerColumn->parentWidget(), mainColumn);
        QCOMPARE(mainColumn->indexOf(readerColumn), 1);
        QVERIFY(mainColumn->isAncestorOf(messages));
#ifdef TROJITA_HAVE_WEBENGINE
        auto *attachmentPanel = window.findChild<Gui::AttachmentPanel *>(QStringLiteral("attachmentPanel"));
        QVERIFY(attachmentPanel);
        QCOMPARE(attachmentPanel->parentWidget(), readerColumn);
        QCOMPARE(readerColumn->indexOf(attachmentPanel), 1);
        QVERIFY(attachmentPanel->isVisible());
        QCOMPARE(attachmentPanel->attachmentCount(), 0);
        auto *attachmentsAction = window.findChild<QAction *>(QStringLiteral("embeddedPanelAttachments"));
        auto *autoHideAction = window.findChild<QAction *>(QStringLiteral("embeddedPanelAttachmentsAutoHide"));
        QVERIFY(attachmentsAction);
        QVERIFY(autoHideAction);
        QVERIFY(attachmentsAction->isChecked());
        QVERIFY(!autoHideAction->isChecked());
        autoHideAction->trigger();
        QVERIFY(attachmentPanel->isHidden());
        autoHideAction->trigger();
        QVERIFY(attachmentPanel->isVisible());

        // The other embedded panels are off by default, so the shell looks like the classic layout
        for (const char *name : {"messageDetailsPanel", "contactPanel", "relatedMessagesPanel"}) {
            auto *panel = window.findChild<QWidget *>(QString::fromLatin1(name));
            QVERIFY2(panel, name);
            QVERIFY2(panel->isHidden(), name);
            QCOMPARE(panel->parentWidget(), rightColumn);
        }
        QVERIFY(!rightColumn->isVisible());

        auto *toRight = window.findChild<QAction *>(QStringLiteral("embeddedPanelAttachmentsPlacementRightColumn"));
        auto *toReader = window.findChild<QAction *>(QStringLiteral("embeddedPanelAttachmentsPlacementBelowReader"));
        QVERIFY(toRight);
        QVERIFY(toReader);
        toRight->trigger();
        QCOMPARE(attachmentPanel->parentWidget(), rightColumn);
        QCOMPARE(rightColumn->indexOf(attachmentPanel), 0);
        QVERIFY(attachmentPanel->isVisible());
        QVERIFY(rightColumn->isVisible());
        QVERIFY(row->sizes().last() > 0);
        toReader->trigger();
        QCOMPARE(attachmentPanel->parentWidget(), readerColumn);
        QCOMPARE(readerColumn->indexOf(attachmentPanel), 1);
        QVERIFY(attachmentPanel->isVisible());
        QVERIFY(!rightColumn->isVisible());
#endif
        right->trigger();
        QCOMPARE(sidebar->parentWidget(), rightColumn);
        QCOMPARE(rightColumn->indexOf(sidebar), 0);
        QVERIFY(!leftColumn->isVisible());
        QVERIFY(QMetaObject::invokeMethod(&window, "slotLayoutCompact"));
        QCOMPARE(sidebar->parentWidget(), rightColumn);
        QCOMPARE(mainColumn->orientation(), Qt::Vertical);
        QCOMPARE(readerColumn->parentWidget(), mainColumn);
        QVERIFY(row->sizes().last() < row->sizes().at(1));
        QVERIFY(QMetaObject::invokeMethod(&window, "slotLayoutOneAtTime"));
        QVERIFY(qobject_cast<QStackedWidget *>(readerColumn->parentWidget()));
        QVERIFY(qobject_cast<QStackedWidget *>(sidebar->parentWidget()));
        QVERIFY(!rightColumn->isVisible());
        QVERIFY(QMetaObject::invokeMethod(&window, "slotLayoutWide"));
        QCOMPARE(sidebar->parentWidget(), rightColumn);
        QVERIFY(rightColumn->isVisible());
        QCOMPARE(readerColumn->parentWidget(), mainColumn);
        QCOMPARE(mainColumn->orientation(), Qt::Horizontal);
        QVERIFY(menu->actions().at(1)->isChecked());
#ifdef TROJITA_HAVE_WEBENGINE
        // A panel sharing a column with the mailbox tree gets the room it asks for, up to two thirds of the column
        auto *detailsPanel = window.findChild<QWidget *>(QStringLiteral("messageDetailsPanel"));
        auto *detailsAction = window.findChild<QAction *>(QStringLiteral("embeddedPanelMessageDetails"));
        QVERIFY(detailsPanel);
        QVERIFY(detailsAction);
        QVERIFY(!detailsAction->isChecked());
        detailsAction->trigger();
        QCoreApplication::processEvents();
        QVERIFY(detailsPanel->isVisible());
        QCOMPARE(detailsPanel->parentWidget(), rightColumn);
        const int detailsIndex = rightColumn->indexOf(detailsPanel);
        const int mailboxIndex = rightColumn->indexOf(sidebar);
        const int columnSpace = rightColumn->height() - rightColumn->handleWidth();
        const int detailsHeight = rightColumn->sizes().at(detailsIndex);
        QVERIFY2(detailsHeight >= std::min(detailsPanel->sizeHint().height(), columnSpace * 2 / 3) - 2,
                 qPrintable(QStringLiteral("%1 of %2").arg(detailsHeight).arg(columnSpace)));
        QVERIFY(rightColumn->sizes().at(mailboxIndex) >= columnSpace / 3 - 2);

        // A divider moved by the user is kept when the panel is hidden and shown again
        QList<int> dragged = rightColumn->sizes();
        const int draggedHeight = detailsHeight > 200 ? detailsHeight - 60 : detailsHeight + 60;
        dragged[mailboxIndex] += dragged.at(detailsIndex) - draggedHeight;
        dragged[detailsIndex] = draggedHeight;
        rightColumn->setSizes(dragged);
        const int rememberedDetailsHeight = rightColumn->sizes().at(detailsIndex);
        QVERIFY(qAbs(rememberedDetailsHeight - detailsHeight) > 10);
        rightColumn->splitterMoved(rightColumn->handle(detailsIndex)->y(), detailsIndex);
        detailsAction->trigger();
        QVERIFY(detailsPanel->isHidden());
        detailsAction->trigger();
        QCoreApplication::processEvents();
        QVERIFY(detailsPanel->isVisible());
        QVERIFY2(qAbs(rightColumn->sizes().at(detailsIndex) - rememberedDetailsHeight) <= 2,
                 qPrintable(QStringLiteral("%1 instead of %2").arg(rightColumn->sizes().at(detailsIndex)).arg(rememberedDetailsHeight)));
#endif

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

        row->setSizes({0, 760, 230});
        const int expectedWidth = row->sizes().last();
        QVERIFY(expectedWidth > 0);
        row->splitterMoved(760, 2);
        window.close();
        settings.sync();
        const QString layoutGroup = Common::SettingsNames::guiSizesInMainWinWhenWide.arg(
            QString::number(window.screen()->availableGeometry().width()));
        settings.beginGroup(layoutGroup);
        QCOMPARE(settings.value(QStringLiteral("shellRightColumnWidth")).toInt(), expectedWidth);
        QVERIFY(!settings.value(QStringLiteral("shellMainColumnState")).toByteArray().isEmpty());
#ifdef TROJITA_HAVE_WEBENGINE
        // Only sizes picked by moving a divider are saved
        QCOMPARE(settings.value(QStringLiteral("embeddedPanelMessageDetailsExtent")).toInt(), rememberedDetailsHeight);
        QVERIFY(!settings.contains(QStringLiteral("embeddedPanelContactExtent")));
        QVERIFY(!settings.contains(QStringLiteral("embeddedPanelAttachmentsExtent")));
#endif
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
