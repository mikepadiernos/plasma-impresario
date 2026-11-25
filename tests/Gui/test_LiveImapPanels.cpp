/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Gui/AttachmentPanel.h"
#include "Gui/CompleteMessageWidget.h"
#include "Gui/ContactPanel.h"
#include "Gui/EmbeddedWebView.h"
#include "Gui/ExternalElementsWidget.h"
#include "Gui/FindBar.h"
#include "Gui/MailBoxTreeView.h"
#include "Gui/MessageDetailsPanel.h"
#include "Gui/MsgListView.h"
#include "Gui/ReaderActionBar.h"
#include "Gui/RelatedMessagesPanel.h"
#include "Gui/Window.h"
#include "Gui/InlineNotificationBar.h"
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/NetworkWatcher.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "configure.cmake.h"
#ifdef TROJITA_HAVE_WEBENGINE
#include "Gui/WebEngineMessageProfile.h"
#endif
#include "static_plugins.h"

#include <KLocalizedString>
#include <algorithm>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDir>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QWebEngineScript>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <memory>

namespace {

// Synthetic credentials for a disposable loopback server; never point this test at a real account.
const QString syntheticPassword = QStringLiteral("synthetic-test-only");

void configureAccount(const QString &fileName, int port, const QString &user)
{
    QSettings account(fileName, QSettings::IniFormat);
    account.setValue(Common::SettingsNames::imapMethodKey, Common::SettingsNames::methodSSL);
    account.setValue(Common::SettingsNames::imapHostKey, QStringLiteral("127.0.0.1"));
    account.setValue(Common::SettingsNames::imapPortKey, port);
    account.setValue(Common::SettingsNames::imapUserKey, user);
    account.setValue(Common::SettingsNames::imapStartMode, Common::SettingsNames::netOnline);
    account.setValue(Common::SettingsNames::imapNeedsNetwork, false);
    account.setValue(Common::SettingsNames::imapUseSystemProxy, false);
    account.setValue(Common::SettingsNames::cacheOfflineKey, Common::SettingsNames::cacheOfflineNone);
    account.setValue(Common::SettingsNames::msaMethodKey, Common::SettingsNames::methodSENDMAIL);
    account.sync();
}

QModelIndex findMailbox(const QAbstractItemModel *model, const QString &name)
{
    if (!model || model->rowCount() == 0)
        return {};
    const QModelIndexList matches = model->match(model->index(0, 0), Imap::Mailbox::RoleMailboxName, name, 1, Qt::MatchExactly);
    return matches.isEmpty() ? QModelIndex() : QModelIndex(matches.first());
}

QModelIndex findMessage(Gui::MsgListView *view, const QString &subject)
{
    QAbstractItemModel *model = view->model();
    if (!model || model->rowCount() == 0)
        return {};
    const QModelIndexList matches = model->match(model->index(0, 0), Imap::Mailbox::RoleMessageSubject, subject, 1,
                                                 Qt::MatchExactly | Qt::MatchRecursive);
    return matches.isEmpty() ? QModelIndex() : QModelIndex(matches.first());
}

QAction *action(Gui::MainWindow &window, const QString &name)
{
    auto *result = window.findChild<QAction *>(name);
    if (!result)
        qWarning() << "No action" << name;
    return result;
}

void setChecked(QAction *action, bool checked)
{
    if (action && action->isChecked() != checked)
        action->trigger();
}

void place(Gui::MainWindow &window, const QString &panelId, const QString &placement)
{
    if (auto *placementAction = action(window, QStringLiteral("embeddedPanel%1Placement%2").arg(panelId, placement)))
        placementAction->trigger();
}

QStringList contacts(const Gui::ContactPanel *panel)
{
    QStringList result;
    for (int row = 0; row < panel->contactCount(); ++row)
        result << panel->contactText(row);
    return result;
}

QStringList subjects(const QList<QPersistentModelIndex> &messages)
{
    QStringList result;
    for (const QPersistentModelIndex &message : messages)
        result << message.data(Imap::Mailbox::RoleMessageSubject).toString();
    result.sort();
    return result;
}

/** Saves a picture of the window when PLASMA_TEST_SCREENSHOT_DIR is set */
void snapshot(QWidget &window, const QString &name)
{
    const QString directory = qEnvironmentVariable("PLASMA_TEST_SCREENSHOT_DIR");
    if (directory.isEmpty())
        return;
    // Let the shell apply pending sizes and the panels refresh before grabbing.
    QTest::qWait(500);
    QVERIFY(QDir().mkpath(directory));
    const QString fileName = QDir(directory).filePath(name + QLatin1String(".png"));
    QVERIFY2(window.grab().save(fileName), qPrintable(fileName));
    qInfo("Saved %s", qPrintable(fileName));
}

}

class LiveImapPanelsTest : public QObject
{
    Q_OBJECT
private slots:
    /**
     * Expects a loopback IMAPS server on PLASMA_TEST_IMAPS_PORT with the user panels@example.test, using the synthetic
     * password, whose INBOX holds the messages of tools/dev-container/seed-panel-mail.py and nothing else.
     * See tools/dev-container/live-imap-panels.sh.
     */
    void panelsFollowLiveMessages()
    {
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        settings.setValue(Common::SettingsNames::guiShowSystray, false);
        settings.setValue(Common::SettingsNames::passwordPlugin, QStringLiteral("cleartextpassword"));
        settings.setValue(QStringLiteral("accounts/ids"), QStringList{QStringLiteral("account-1")});
        settings.setValue(QStringLiteral("accounts/active"), QStringLiteral("account-1"));
        settings.setValue(QStringLiteral("accounts/next"), 2);
        settings.setValue(QStringLiteral("accounts/names/account-1"), QStringLiteral("Panels"));
        settings.setValue(QStringLiteral("accountPasswords/account-1/imap"), syntheticPassword);
        settings.sync();
        configureAccount(directory.filePath(QStringLiteral("main-account-1.conf")), port, QStringLiteral("panels@example.test"));

        int passwordPrompts = 0;
        QTimer dialogs;
        connect(&dialogs, &QTimer::timeout, this, [&] {
            for (QWidget *widget : QApplication::topLevelWidgets()) {
                if (!widget->isVisible())
                    continue;
                if (auto *message = qobject_cast<QMessageBox *>(widget);
                    message && message->standardButtons() == (QMessageBox::Yes | QMessageBox::No)) {
                    message->button(QMessageBox::Yes)->click();
                } else if (auto *dialog = qobject_cast<QDialog *>(widget);
                           dialog && dialog->findChild<QLineEdit *>(QStringLiteral("passwordLineEdit"))) {
                    ++passwordPrompts;
                    dialog->reject();
                }
            }
        });
        dialogs.start(20);

        Gui::MainWindow window(&settings);
        window.resize(1400, 900);
        window.show();
        auto *tree = window.findChild<Gui::MailBoxTreeView *>();
        auto *messages = window.findChild<Gui::MsgListView *>();
        auto *details = window.findChild<Gui::MessageDetailsPanel *>();
        auto *contact = window.findChild<Gui::ContactPanel *>();
        auto *related = window.findChild<Gui::RelatedMessagesPanel *>();
        auto *attachments = window.findChild<Gui::AttachmentPanel *>();
        QVERIFY(tree);
        QVERIFY(messages);
        QVERIFY(details);
        QVERIFY(contact);
        QVERIFY(related);
        QVERIFY(attachments);
        QSignalSpy imapErrors(window.imapModel(), &Imap::Mailbox::Model::imapError);

        QModelIndex inbox;
        QTRY_VERIFY_WITH_TIMEOUT((inbox = findMailbox(tree->model(), QStringLiteral("INBOX"))).isValid(), 20000);
        tree->scrollTo(inbox);
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualRect(inbox).center());
        QTRY_COMPARE_WITH_TIMEOUT(inbox.data(Imap::Mailbox::RoleTotalMessageCount).toInt(), 8, 10000);
        for (const auto *subject : {"Quarterly plan", "Re: Quarterly plan", "Budget numbers", "Signed release note", "Lunch on Friday",
                                    "Weekly news", "Travel plans", "Release digest"})
            QTRY_VERIFY2_WITH_TIMEOUT(findMessage(messages, QString::fromUtf8(subject)).isValid(), subject, 10000);

        const auto open = [&](const QString &subject) {
            // The list is filled again after a reconnect
            QModelIndex message;
            QTRY_VERIFY2_WITH_TIMEOUT((message = findMessage(messages, subject)).isValid(), qPrintable(subject), 10000);
            messages->expandAll();
            messages->scrollTo(message);
            // Click inside the visible part of the row; a narrow list column clips long rows.
            const QRect row = messages->visualRect(message).intersected(messages->viewport()->rect());
            QVERIFY2(!row.isEmpty(), qPrintable(subject));
            QTest::mouseClick(messages->viewport(), Qt::LeftButton, Qt::NoModifier, row.center());
            QTRY_COMPARE_WITH_TIMEOUT(details->message().data(Imap::Mailbox::RoleMessageSubject).toString(), subject, 10000);
        };

        // The default shell: folders on the left, list above the reader, attachments below it, no other panels.
        QVERIFY(QMetaObject::invokeMethod(&window, "slotLayoutCompact"));
        open(QStringLiteral("Quarterly plan"));
        QTRY_COMPARE_WITH_TIMEOUT(attachments->attachmentCount(), 2, 15000);
        QVERIFY(attachments->isVisible());
        QVERIFY(!details->isVisible());
        QVERIFY(!contact->isVisible());
        QVERIFY(!related->isVisible());
        snapshot(window, QStringLiteral("01-compact-default"));
        QVERIFY(QMetaObject::invokeMethod(&window, "slotLayoutWide"));
        snapshot(window, QStringLiteral("02-wide-default"));
        setChecked(action(window, QStringLiteral("mailboxSidebarRight")), true);
        snapshot(window, QStringLiteral("03-wide-sidebar-right"));
        setChecked(action(window, QStringLiteral("mailboxSidebarLeft")), true);

        // The new panels go to the right column by default and follow the open message.
        for (const auto *id : {"MessageDetails", "Contact", "RelatedMessages"})
            setChecked(action(window, QStringLiteral("embeddedPanel%1").arg(QLatin1String(id))), true);
        QTRY_VERIFY(details->isVisible());
        QVERIFY(contact->isVisible());
        QVERIFY(related->isVisible());
        QTRY_COMPARE_WITH_TIMEOUT(details->fieldText(QStringLiteral("Subject")), QStringLiteral("Quarterly plan"), 10000);
        QVERIFY2(details->fieldText(QStringLiteral("From")).contains(QLatin1String("alice@example.test")),
                 qPrintable(details->fieldText(QStringLiteral("From"))));
        QCOMPARE(details->fieldText(QStringLiteral("Folder")), QStringLiteral("INBOX"));
        QVERIFY2(details->fieldText(QStringLiteral("Status")).contains(QLatin1String("Flagged")),
                 qPrintable(details->fieldText(QStringLiteral("Status"))));
        QVERIFY(details->fieldText(QStringLiteral("Security")).isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(contact->contactCount(), 3, 10000);
        QVERIFY2(contacts(contact).join(QLatin1Char('\n')).contains(QLatin1String("bob@example.test")),
                 qPrintable(contacts(contact).join(QLatin1String(" | "))));
        QTRY_COMPARE_WITH_TIMEOUT(subjects(related->conversation()),
                                  (QStringList{QStringLiteral("Budget numbers"), QStringLiteral("Re: Quarterly plan")}), 10000);
        QCOMPARE(subjects(related->sameSender()), QStringList{QStringLiteral("Lunch on Friday")});
        snapshot(window, QStringLiteral("04-wide-panels-right"));
        QVERIFY(QMetaObject::invokeMethod(&window, "slotLayoutCompact"));
        snapshot(window, QStringLiteral("05-compact-panels-right"));
        setChecked(action(window, QStringLiteral("mailboxSidebarRight")), true);
        snapshot(window, QStringLiteral("06-compact-sidebar-and-panels-right"));
        setChecked(action(window, QStringLiteral("mailboxSidebarLeft")), true);

        // Panels spread over every region.
        place(window, QStringLiteral("MessageDetails"), QStringLiteral("LeftColumn"));
        place(window, QStringLiteral("RelatedMessages"), QStringLiteral("Footer"));
        place(window, QStringLiteral("Attachments"), QStringLiteral("Footer"));
        QVERIFY(QMetaObject::invokeMethod(&window, "slotLayoutWide"));
        QTRY_VERIFY(details->isVisible() && related->isVisible() && attachments->isVisible() && contact->isVisible());
        snapshot(window, QStringLiteral("07-wide-panels-left-right-footer"));
        QVERIFY(QMetaObject::invokeMethod(&window, "slotLayoutOneAtTime"));
        snapshot(window, QStringLiteral("08-one-at-a-time-panels"));
        QVERIFY(QMetaObject::invokeMethod(&window, "slotLayoutWide"));
        place(window, QStringLiteral("MessageDetails"), QStringLiteral("RightColumn"));
        place(window, QStringLiteral("RelatedMessages"), QStringLiteral("RightColumn"));
        place(window, QStringLiteral("Attachments"), QStringLiteral("BelowReader"));

        // A reply with Reply-To, an unrelated sender, and no attachments.
        setChecked(action(window, QStringLiteral("embeddedPanelAttachmentsAutoHide")), true);
        open(QStringLiteral("Re: Quarterly plan"));
        QTRY_COMPARE_WITH_TIMEOUT(contact->contactCount(), 4, 10000);
        QVERIFY2(contacts(contact).join(QLatin1Char('\n')).contains(QLatin1String("bob-replies@example.test")),
                 qPrintable(contacts(contact).join(QLatin1String(" | "))));
        QTRY_COMPARE_WITH_TIMEOUT(subjects(related->conversation()), QStringList{QStringLiteral("Quarterly plan")}, 10000);
        QVERIFY(related->sameSender().isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(attachments->attachmentCount(), 0, 15000);
        QTRY_VERIFY(!attachments->isVisible());

        // A reply with another subject still joins the conversation through In-Reply-To.
        open(QStringLiteral("Budget numbers"));
        QTRY_COMPARE_WITH_TIMEOUT(subjects(related->conversation()), QStringList{QStringLiteral("Quarterly plan")}, 10000);

        // The reader's verification of the synthetic signature is reported, never as a valid signature.
        open(QStringLiteral("Signed release note"));
        QTRY_VERIFY_WITH_TIMEOUT(details->security().signature != UiUtils::MessageSecurity::Signature::None
                                     && !details->security().isPending(),
                                 15000);
        QVERIFY2(details->security().signature != UiUtils::MessageSecurity::Signature::Trusted
                     && details->security().signature != UiUtils::MessageSecurity::Signature::Untrusted,
                 qPrintable(details->fieldText(QStringLiteral("Security"))));
        QVERIFY(details->fieldText(QStringLiteral("Security")).startsWith(QLatin1String("Signed")));
        snapshot(window, QStringLiteral("09-wide-signed-message"));

        // Another message from the same sender.
        open(QStringLiteral("Lunch on Friday"));
        QTRY_COMPARE_WITH_TIMEOUT(subjects(related->sameSender()), QStringList{QStringLiteral("Quarterly plan")}, 10000);
        QVERIFY(related->conversation().isEmpty());

        // A message opened from a body search shows the term in the find bar, and the focus stays in the list.
        auto *reader = window.findChild<Gui::CompleteMessageWidget *>();
        QVERIFY(reader);
        auto *findBar = reader->findChild<Gui::FindBar *>();
        QVERIFY(findBar);
        auto *findText = findBar->findChild<QLineEdit *>();
        auto *search = window.findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        auto *searchInBody = window.findChild<QAction *>(QStringLiteral("searchInBody"));
        QVERIFY(findText);
        QVERIFY(search);
        QVERIFY(searchInBody);
        searchInBody->setChecked(true);
        search->setText(QStringLiteral("shared inbox"));
        QTest::keyClick(search, Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT(!findMessage(messages, QStringLiteral("Lunch on Friday")).isValid(), 10000);
        QVERIFY(!findBar->isVisible());
        open(QStringLiteral("Re: Quarterly plan"));
        QTRY_VERIFY_WITH_TIMEOUT(findBar->isVisible(), 15000);
        QCOMPARE(findText->text(), QStringLiteral("shared inbox"));
        QVERIFY(!findText->hasFocus());
        snapshot(window, QStringLiteral("10-wide-search-highlight"));

        // Without a search, the next message opens without the find bar.
        search->clear();
        QTest::keyClick(search, Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT(findMessage(messages, QStringLiteral("Lunch on Friday")).isValid(), 10000);
        open(QStringLiteral("Lunch on Friday"));
        QTRY_VERIFY(!findBar->isVisible());

        // Remote content stays blocked, and the bar names the server it would contact.
        auto *remote = reader->findChild<Gui::ExternalElementsWidget *>();
        QVERIFY(remote);
        QVERIFY(!remote->isVisible());
        open(QStringLiteral("Weekly news"));
        QTRY_VERIFY_WITH_TIMEOUT(remote->isVisible(), 15000);
        QCOMPARE(remote->blockedHosts(), QStringList{QStringLiteral("images.example.test")});
        QVERIFY2(remote->message().contains(QLatin1String("images.example.test")), qPrintable(remote->message()));
        snapshot(window, QStringLiteral("11-wide-remote-content-blocked"));

        // A link whose text shows another site asks before opening; Cancel keeps it closed.
        auto *body = reader->findChild<Gui::EmbeddedWebView *>();
        QVERIFY(body);
        QString confirmation;
        QTimer answer;
        connect(&answer, &QTimer::timeout, &answer, [&]() {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!box || box->objectName() != QLatin1String("linkConfirmation"))
                return;
            confirmation = box->text() + QLatin1Char('\n') + box->informativeText();
            box->button(QMessageBox::Cancel)->click();
        });
        answer.start(50);
        body->page()->runJavaScript(QStringLiteral("document.getElementById('sign-in').click()"),
                                    QWebEngineScript::ApplicationWorld);
        QTRY_VERIFY_WITH_TIMEOUT(!confirmation.isEmpty(), 10000);
        answer.stop();
        QVERIFY2(confirmation.contains(QLatin1String("sign-in.example.test.invalid"))
                     && confirmation.contains(QLatin1String("bank.example.test")),
                 qPrintable(confirmation));

        // Consent applies to this message only.
        QTest::mouseClick(remote->findChild<QPushButton *>(QStringLiteral("loadRemoteContent")), Qt::LeftButton);
        QTRY_VERIFY(!remote->isVisible());
        open(QStringLiteral("Lunch on Friday"));
        QVERIFY(remote->blockedHosts().isEmpty());

        // The bar above the reader works on the open message, whatever is selected in the list.
        auto *bar = window.findChild<Gui::ReaderActionBar *>(QStringLiteral("readerActions"));
        QVERIFY(bar);
        const auto barButton = [bar](const char *name) {
            auto *button = bar->findChild<QToolButton *>(QLatin1String(name));
            Q_ASSERT(button);
            return button;
        };
        open(QStringLiteral("Quarterly plan"));
        QVERIFY(bar->isVisible());
        QTRY_COMPARE_WITH_TIMEOUT(bar->attachmentCount(), 2, 15000);
        QVERIFY(bar->isAttachmentChipShown());
        QTRY_VERIFY_WITH_TIMEOUT(barButton("readerFlag")->isEnabled(), 10000);
        QVERIFY(barButton("readerFlag")->isChecked());
        QVERIFY(barButton("readerArchive")->isEnabled());
        QVERIFY(barButton("readerReply")->isEnabled());
        const QModelIndex plan = findMessage(messages, QStringLiteral("Quarterly plan"));
        const QModelIndex lunch = findMessage(messages, QStringLiteral("Lunch on Friday"));
        QVERIFY(!lunch.data(Imap::Mailbox::RoleMessageIsMarkedFlagged).toBool());
        messages->selectionModel()->select(lunch, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        QCOMPARE(details->message().data(Imap::Mailbox::RoleMessageSubject).toString(), QStringLiteral("Quarterly plan"));
        barButton("readerFlag")->click();
        QTRY_VERIFY_WITH_TIMEOUT(!plan.data(Imap::Mailbox::RoleMessageIsMarkedFlagged).toBool(), 10000);
        QTRY_VERIFY(!barButton("readerFlag")->isChecked());
        QVERIFY(!lunch.data(Imap::Mailbox::RoleMessageIsMarkedFlagged).toBool());
        barButton("readerRead")->click();
        QTRY_VERIFY_WITH_TIMEOUT(!plan.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool(), 10000);
        QVERIFY(lunch.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool());
        barButton("readerRead")->click();
        barButton("readerFlag")->click();
        QTRY_VERIFY_WITH_TIMEOUT(plan.data(Imap::Mailbox::RoleMessageIsMarkedFlagged).toBool(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(plan.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool(), 10000);
        QTRY_VERIFY(barButton("readerFlag")->isChecked() && barButton("readerRead")->isChecked());

        // The chip brings the attachments panel back, even when it was turned off.
        setChecked(action(window, QStringLiteral("embeddedPanelAttachments")), false);
        QTRY_VERIFY(!attachments->isVisible());
        barButton("readerAttachments")->click();
        QTRY_VERIFY(attachments->isVisible());
        QVERIFY(action(window, QStringLiteral("embeddedPanelAttachments"))->isChecked());
        QTRY_COMPARE(window.focusWidget(), attachments->listView());
        snapshot(window, QStringLiteral("13-wide-reader-actions"));

        // Escape in a panel goes back to the message list. Shortcuts need the active window, which the certificate
        // prompt took away.
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        QTest::keyClick(attachments->listView(), Qt::Key_Escape);
        QTRY_COMPARE(window.focusWidget(), static_cast<QWidget *>(messages));

        // F6 follows the screen: the reader, the right column from top to bottom, then the footer
        place(window, QStringLiteral("MessageDetails"), QStringLiteral("Footer"));
        QTRY_VERIFY(details->isVisible() && contact->isVisible() && related->isVisible());
        const auto focusIn = [&window](QWidget *container) {
            QWidget *focus = window.focusWidget();
            return focus && (focus == container || container->isAncestorOf(focus));
        };
        QList<QWidget *> visited;
        action(window, QStringLiteral("action_focus_msg_list"))->trigger();
        for (int i = 0; i < 16; ++i) {
            action(window, QStringLiteral("action_focus_next_region"))->trigger();
            if (focusIn(messages))
                break;
            for (QWidget *pane : {static_cast<QWidget *>(reader), static_cast<QWidget *>(contact), static_cast<QWidget *>(related),
                                  static_cast<QWidget *>(details)}) {
                if (focusIn(pane) && !visited.contains(pane))
                    visited << pane;
            }
        }
        QList<QWidget *> rightColumn{contact, related};
        std::sort(rightColumn.begin(), rightColumn.end(),
                  [](QWidget *a, QWidget *b) { return a->mapToGlobal(QPoint()).y() < b->mapToGlobal(QPoint()).y(); });
        QCOMPARE(visited, QList<QWidget *>{reader} + rightColumn + QList<QWidget *>{details});
        QVERIFY(focusIn(messages));
        place(window, QStringLiteral("MessageDetails"), QStringLiteral("RightColumn"));

        // Triage from the list: S flags the current message, and the Menu key opens the menu of the current row.
        // The platform turns the Menu key into a keyboard context menu event at the cursor rectangle; the offscreen
        // platform does not, so the test sends that event the way QWidgetWindow does.
        open(QStringLiteral("Lunch on Friday"));
        const QModelIndex lunchRow = findMessage(messages, QStringLiteral("Lunch on Friday"));
        QVERIFY(!lunchRow.data(Imap::Mailbox::RoleMessageIsMarkedFlagged).toBool());
        messages->setFocus();
        QTest::keyClick(messages, Qt::Key_S);
        QTRY_VERIFY_WITH_TIMEOUT(lunchRow.data(Imap::Mailbox::RoleMessageIsMarkedFlagged).toBool(), 10000);
        QList<QAction *> menuActions;
        bool menuFlagged = false;
        QTimer menus;
        connect(&menus, &QTimer::timeout, this, [&] {
            if (auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget())) {
                menuActions = menu->actions();
                menuFlagged = action(window, QStringLiteral("action_mark_as_flagged"))->isChecked();
                menu->close();
            }
        });
        menus.start(20);
        QTRY_COMPARE(QApplication::focusWidget(), static_cast<QWidget *>(messages));
        const QPoint menuAt = messages->inputMethodQuery(Qt::ImCursorRectangle).toRect().center();
        QVERIFY(messages->visualRect(lunchRow).contains(menuAt));
        QContextMenuEvent menuKey(QContextMenuEvent::Keyboard, menuAt, messages->mapToGlobal(menuAt));
        QApplication::sendEvent(messages, &menuKey);
        QTRY_VERIFY_WITH_TIMEOUT(!menuActions.isEmpty(), 5000);
        menus.stop();
        QVERIFY(menuActions.contains(action(window, QStringLiteral("action_mark_as_flagged"))));
        QVERIFY(menuFlagged);
        QTest::keyClick(messages, Qt::Key_S);
        QTRY_VERIFY_WITH_TIMEOUT(!lunchRow.data(Imap::Mailbox::RoleMessageIsMarkedFlagged).toBool(), 10000);

        // Zoom and the plain-text preference apply to the open message.
        const auto shownViews = [reader]() {
            QList<Gui::EmbeddedWebView *> shown;
            for (auto *view : reader->findChildren<Gui::EmbeddedWebView *>()) {
                if (view->isVisible())
                    shown << view;
            }
            return shown;
        };
        const auto shownText = std::make_shared<QString>();
        const auto readShown = [&]() {
            for (auto *view : shownViews())
                view->page()->toPlainText([shownText](const QString &text) { *shownText = text; });
            return *shownText;
        };
        open(QStringLiteral("Release digest"));
        QVERIFY(barButton("readerPlainText")->isChecked());
        QTRY_VERIFY_WITH_TIMEOUT(readShown().contains(QLatin1String("plain text version")), 15000);
        barButton("readerZoomIn")->click();
        QTRY_VERIFY(!shownViews().isEmpty() && shownViews().constFirst()->zoomFactor() > 1.0);
        barButton("readerZoomReset")->click();
        QTRY_VERIFY(!shownViews().isEmpty() && qFuzzyCompare(shownViews().constFirst()->zoomFactor(), 1.0));
        barButton("readerPlainText")->click();
        QVERIFY(!settings.value(Common::SettingsNames::guiPreferPlaintextRendering).toBool());
        shownText->clear();
        QTRY_VERIFY_WITH_TIMEOUT(readShown().contains(QLatin1String("HTML version")), 15000);
        QCOMPARE(details->message().data(Imap::Mailbox::RoleMessageSubject).toString(), QStringLiteral("Release digest"));
        barButton("readerPlainText")->click();
        shownText->clear();
        QTRY_VERIFY_WITH_TIMEOUT(readShown().contains(QLatin1String("plain text version")), 15000);

        // Offline, a body which was never downloaded is reported instead of a blank or a Chromium error page,
        // and it loads on its own when the network comes back.
        auto *watcher = qobject_cast<Imap::Mailbox::NetworkWatcher *>(window.imapAccess()->networkWatcher());
        auto *partErrors = reader->findChild<Gui::InlineNotificationBar *>(QStringLiteral("partLoadErrors"));
        QVERIFY(watcher);
        QVERIFY(partErrors);
        QVERIFY(!partErrors->isVisible());
        watcher->setNetworkOffline();
        QTRY_VERIFY_WITH_TIMEOUT(!window.imapModel()->isNetworkAvailable(), 10000);
        open(QStringLiteral("Travel plans"));
        QTRY_VERIFY_WITH_TIMEOUT(partErrors->isVisible(), 15000);
        QCOMPARE(partErrors->kind(), Gui::InlineNotificationBar::Kind::Error);
        QVERIFY2(partErrors->text().contains(QLatin1String("offline")), qPrintable(partErrors->text()));
        QVERIFY(partErrors->actionButtons().isEmpty());
        snapshot(window, QStringLiteral("12-wide-part-unavailable-offline"));
        QSignalSpy foldersListed(window.imapModel(), &QAbstractItemModel::rowsInserted);
        watcher->setNetworkOnline();
        QTRY_VERIFY_WITH_TIMEOUT(!partErrors->isVisible(), 20000);
        // The reader can rebuild its views while the folder is opened again, so they are looked up on each try.
        shownText->clear();
        QTRY_VERIFY_WITH_TIMEOUT(readShown().contains(QLatin1String("The train leaves at nine")), 20000);

        // Going online again reloads the folder list; the shown folder is opened again rather than left empty.
        // Until the server answers, the list still shows the folder's old rows.
        QTRY_VERIFY_WITH_TIMEOUT(std::any_of(foldersListed.cbegin(), foldersListed.cend(),
                                             [](const QList<QVariant> &args) { return !args.first().toModelIndex().isValid(); }),
                                 20000);
        QTRY_VERIFY_WITH_TIMEOUT(findMessage(messages, QStringLiteral("Quarterly plan")).isValid(), 20000);
        QTRY_COMPARE_WITH_TIMEOUT(tree->currentIndex().data(Imap::Mailbox::RoleMailboxName).toString(), QStringLiteral("INBOX"), 10000);
        open(QStringLiteral("Budget numbers"));

        // Space in the list pages through the open message, and at its end opens the next unread one.
        open(QStringLiteral("Lunch on Friday"));
        QTRY_VERIFY_WITH_TIMEOUT(barButton("readerRead")->isEnabled() && barButton("readerRead")->isChecked(), 10000);
        barButton("readerRead")->click();
        const QModelIndex unreadLunch = findMessage(messages, QStringLiteral("Lunch on Friday"));
        QTRY_VERIFY_WITH_TIMEOUT(!unreadLunch.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool(), 10000);
        messages->expandAll();
        const QString previousSubject = messages->indexAbove(unreadLunch).data(Imap::Mailbox::RoleMessageSubject).toString();
        QVERIFY(!previousSubject.isEmpty());
        open(previousSubject);
        messages->setFocus();
        reader->area->verticalScrollBar()->setValue(reader->area->verticalScrollBar()->maximum());
        QTest::keyClick(messages, Qt::Key_Space);
        QTRY_COMPARE_WITH_TIMEOUT(details->message().data(Imap::Mailbox::RoleMessageSubject).toString(),
                                  QStringLiteral("Lunch on Friday"), 10000);
        QCOMPARE(messages->currentIndex().data(Imap::Mailbox::RoleMessageSubject).toString(), QStringLiteral("Lunch on Friday"));

        // Alt+Down and Alt+Up step through the conversation from the oldest message to the newest, threaded or not.
        // "Budget numbers" replies to "Quarterly plan" under another subject, and is newer than "Re: Quarterly plan".
        QAction *groupedBefore = nullptr;
        for (QAction *groupBy : window.findChildren<QAction *>()) {
            if (groupBy->objectName().startsWith(QLatin1String("messageListGroupBy")) && groupBy->isCheckable() && groupBy->isChecked())
                groupedBefore = groupBy;
        }
        QVERIFY(groupedBefore);
        const QStringList conversationOrder{QStringLiteral("Quarterly plan"), QStringLiteral("Re: Quarterly plan"),
                                            QStringLiteral("Budget numbers")};
        const auto stepThrough = [&](const char *grouping) {
            action(window, QLatin1String("messageListGroupBy") + QLatin1String(grouping))->trigger();
            QTRY_VERIFY_WITH_TIMEOUT(findMessage(messages, conversationOrder.constFirst()).isValid(), 10000);
            // The server sends the threads after the list is shown
            const bool threaded = qstrcmp(grouping, "Conversation") == 0;
            QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(findMessage(messages, conversationOrder.constFirst())),
                                      threaded ? 2 : 0, 10000);
            open(conversationOrder.constFirst());
            const auto shown = [&]() { return details->message().data(Imap::Mailbox::RoleMessageSubject).toString(); };
            for (int i = 1; i < conversationOrder.size(); ++i) {
                action(window, QStringLiteral("action_go_to_next_in_conversation"))->trigger();
                QTRY_COMPARE_WITH_TIMEOUT(shown(), conversationOrder.at(i), 10000);
                QCOMPARE(messages->currentIndex().data(Imap::Mailbox::RoleMessageSubject).toString(), conversationOrder.at(i));
            }
            // No wrap-around at either end
            action(window, QStringLiteral("action_go_to_next_in_conversation"))->trigger();
            QTest::qWait(200);
            QCOMPARE(shown(), conversationOrder.constLast());
            for (int i = conversationOrder.size() - 2; i >= 0; --i) {
                action(window, QStringLiteral("action_go_to_previous_in_conversation"))->trigger();
                QTRY_COMPARE_WITH_TIMEOUT(shown(), conversationOrder.at(i), 10000);
            }
            action(window, QStringLiteral("action_go_to_previous_in_conversation"))->trigger();
            QTest::qWait(200);
            QCOMPARE(shown(), conversationOrder.constFirst());
        };
        stepThrough("Conversation");
        QVERIFY(action(window, QStringLiteral("action_collapse_all_conversations"))->isEnabled());
        // Collapsed threads open again on the way to the next message
        const QModelIndex planThread = findMessage(messages, conversationOrder.constFirst());
        QVERIFY(planThread.isValid() && messages->model()->hasChildren(planThread));
        action(window, QStringLiteral("action_collapse_all_conversations"))->trigger();
        QVERIFY(!messages->isExpanded(planThread));
        action(window, QStringLiteral("action_go_to_next_in_conversation"))->trigger();
        QTRY_COMPARE_WITH_TIMEOUT(details->message().data(Imap::Mailbox::RoleMessageSubject).toString(), conversationOrder.at(1), 10000);
        QVERIFY(messages->isExpanded(planThread));
        action(window, QStringLiteral("action_collapse_all_conversations"))->trigger();
        action(window, QStringLiteral("action_expand_all_conversations"))->trigger();
        QVERIFY(messages->isExpanded(planThread));
        stepThrough("None");
        QVERIFY(!action(window, QStringLiteral("action_collapse_all_conversations"))->isEnabled());
        groupedBefore->trigger();

        QCOMPARE(passwordPrompts, 0);
        QVERIFY2(imapErrors.isEmpty(), qPrintable(imapErrors.isEmpty() ? QString() : imapErrors.first().first().toString()));
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir home;
    if (!home.isValid())
        return 1;
    const QString homePath = home.path();
    qputenv("HOME", homePath.toUtf8());
    qputenv("XDG_CONFIG_HOME", (homePath + QStringLiteral("/config")).toUtf8());
    qputenv("XDG_DATA_HOME", (homePath + QStringLiteral("/data")).toUtf8());
    qputenv("XDG_CACHE_HOME", (homePath + QStringLiteral("/cache")).toUtf8());
    qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent/plasma-impresario-live-test");
    qunsetenv("TROJITA_PROFILE");
    QStandardPaths::setTestModeEnabled(true);
#ifdef TROJITA_HAVE_WEBENGINE
    Gui::WebEngineMessageProfile::registerScheme();
#endif
    Common::registerMetaTypes();
    QApplication app(argc, argv);
    for (auto location : {QStandardPaths::GenericConfigLocation, QStandardPaths::AppDataLocation, QStandardPaths::CacheLocation}) {
        if (!QStandardPaths::writableLocation(location).startsWith(homePath + QLatin1Char('/')))
            return 1;
    }
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(license);
    KLocalizedString::setApplicationDomain("plasma-impresario");
    if (!Imap::Mailbox::SystemNetworkWatcher::init())
        return 1;
    LiveImapPanelsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_LiveImapPanels.moc"
