/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Gui/AttachmentPanel.h"
#include "Gui/ContactPanel.h"
#include "Gui/MailBoxTreeView.h"
#include "Gui/MessageDetailsPanel.h"
#include "Gui/MsgListView.h"
#include "Gui/RelatedMessagesPanel.h"
#include "Gui/Window.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "configure.cmake.h"
#ifdef TROJITA_HAVE_WEBENGINE
#include "Gui/WebEngineMessageProfile.h"
#endif
#include "static_plugins.h"

#include <KLocalizedString>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

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
        QTRY_COMPARE_WITH_TIMEOUT(inbox.data(Imap::Mailbox::RoleTotalMessageCount).toInt(), 5, 10000);
        for (const auto *subject : {"Quarterly plan", "Re: Quarterly plan", "Budget numbers", "Signed release note", "Lunch on Friday"})
            QTRY_VERIFY2_WITH_TIMEOUT(findMessage(messages, QString::fromUtf8(subject)).isValid(), subject, 10000);

        const auto open = [&](const QString &subject) {
            const QModelIndex message = findMessage(messages, subject);
            QVERIFY2(message.isValid(), qPrintable(subject));
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

        // MIME-level signing is reported without claiming that the signature is valid.
        open(QStringLiteral("Signed release note"));
        QTRY_COMPARE_WITH_TIMEOUT(details->fieldText(QStringLiteral("Security")), QStringLiteral("Signed"), 15000);
        snapshot(window, QStringLiteral("09-wide-signed-message"));

        // Another message from the same sender.
        open(QStringLiteral("Lunch on Friday"));
        QTRY_COMPARE_WITH_TIMEOUT(subjects(related->sameSender()), QStringList{QStringLiteral("Quarterly plan")}, 10000);
        QVERIFY(related->conversation().isEmpty());

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
