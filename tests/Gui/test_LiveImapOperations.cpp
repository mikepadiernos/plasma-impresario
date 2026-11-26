/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/Paths.h"
#include "Common/SettingsNames.h"
#include "Gui/InlineNotificationBar.h"
#include "Gui/MailBoxTreeView.h"
#include "Gui/MsgListView.h"
#include "Gui/Window.h"
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "UiUtils/DestructiveActions.h"
#include "configure.cmake.h"
#ifdef TROJITA_HAVE_WEBENGINE
#include "Gui/WebEngineMessageProfile.h"
#endif
#include "static_plugins.h"

#include <KLocalizedString>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QInputDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

namespace {

using UiUtils::DestructiveActions::Confirmation;

// Synthetic credentials for a disposable loopback server; never point this test at a real account.
const QString syntheticPassword = QStringLiteral("synthetic-test-only");
const QString userAddress = QStringLiteral("operations@example.test");

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

void clickMailbox(Gui::MailBoxTreeView *tree, const QModelIndex &mailbox)
{
    tree->scrollTo(mailbox);
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualRect(mailbox).center());
}

/** Click the message with @arg subject, as a user would; false when the list does not show it */
bool clickMessage(Gui::MsgListView *messages, const QString &subject)
{
    const QModelIndex message = findMessage(messages, subject);
    if (!message.isValid())
        return false;
    messages->scrollTo(message);
    QTest::mouseClick(messages->viewport(), Qt::LeftButton, Qt::NoModifier,
                      messages->visualRect(message).intersected(messages->viewport()->rect()).center());
    return messages->currentIndex().data(Imap::Mailbox::RoleMessageSubject).toString() == subject;
}

QString currentMailbox(Gui::MainWindow &window)
{
    auto *list = qobject_cast<Imap::Mailbox::MsgListModel *>(window.imapAccess()->msgListModel());
    return list ? list->currentMailbox().data(Imap::Mailbox::RoleMailboxName).toString() : QString();
}

QDir maildirFolder(const QString &folder)
{
    return QDir(qEnvironmentVariable("PLASMA_TEST_MAILDIR") + QLatin1Char('/') + folder);
}

/** The file names of the messages which the server stores in a Maildir folder, such as "." for INBOX or ".Trash" */
QStringList storedFiles(const QString &folder)
{
    const QDir maildir = maildirFolder(folder);
    return QDir(maildir.filePath(QStringLiteral("cur"))).entryList(QDir::Files)
           + QDir(maildir.filePath(QStringLiteral("new"))).entryList(QDir::Files);
}

int storedMessages(const QString &folder)
{
    return int(storedFiles(folder).size());
}

/** The number of stored messages in @arg folder with the Maildir flag T, which is IMAP's \Deleted */
int storedDeleted(const QString &folder)
{
    int count = 0;
    for (const QString &file : storedFiles(folder)) {
        const int info = file.lastIndexOf(QLatin1String(":2,"));
        if (info >= 0 && file.mid(info + 3).contains(QLatin1Char('T')))
            ++count;
    }
    return count;
}

/** A profile with one account on the synthetic server */
class Profile
{
public:
    explicit Profile(QTemporaryDir &directory, int port)
        : settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat)
    {
        settings.setValue(Common::SettingsNames::guiShowSystray, false);
        settings.setValue(Common::SettingsNames::passwordPlugin, QStringLiteral("cleartextpassword"));
        settings.setValue(QStringLiteral("accounts/ids"), QStringList{QStringLiteral("account-1")});
        settings.setValue(QStringLiteral("accounts/active"), QStringLiteral("account-1"));
        settings.setValue(QStringLiteral("accounts/next"), 2);
        settings.setValue(QStringLiteral("accounts/names/account-1"), QStringLiteral("Operations"));
        settings.setValue(QStringLiteral("accountPasswords/account-1/imap"), syntheticPassword);
        settings.sync();

        QSettings account(directory.filePath(QStringLiteral("main-account-1.conf")), QSettings::IniFormat);
        account.setValue(Common::SettingsNames::imapMethodKey, Common::SettingsNames::methodSSL);
        account.setValue(Common::SettingsNames::imapHostKey, QStringLiteral("127.0.0.1"));
        account.setValue(Common::SettingsNames::imapPortKey, port);
        account.setValue(Common::SettingsNames::imapUserKey, userAddress);
        account.setValue(Common::SettingsNames::imapStartMode, Common::SettingsNames::netOnline);
        account.setValue(Common::SettingsNames::imapNeedsNetwork, false);
        account.setValue(Common::SettingsNames::imapUseSystemProxy, false);
        account.setValue(Common::SettingsNames::cacheOfflineKey, Common::SettingsNames::cacheOfflineNone);
        account.setValue(Common::SettingsNames::msaMethodKey, Common::SettingsNames::methodSENDMAIL);
        account.sync();
    }

    QSettings settings;
};

/** Answers the dialogs which a test cannot click through itself */
class Dialogs : public QObject
{
public:
    QStringList errors;
    /** The text to type into the next "Rename Folder" dialog */
    QString renameTo;

    Dialogs()
    {
        connect(&m_timer, &QTimer::timeout, this, [this] { answer(); });
        m_timer.start(20);
    }

private:
    void answer()
    {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (!widget->isVisible())
                continue;
            if (auto *message = qobject_cast<QMessageBox *>(widget)) {
                if (message->standardButtons() == (QMessageBox::Yes | QMessageBox::No)) {
                    // The certificate of the synthetic server
                    message->button(QMessageBox::Yes)->click();
                } else {
                    errors << message->text();
                    message->reject();
                }
            } else if (auto *input = qobject_cast<QInputDialog *>(widget)) {
                if (renameTo.isEmpty()) {
                    errors << input->labelText();
                    input->reject();
                } else {
                    input->setTextValue(renameTo);
                    renameTo.clear();
                    input->accept();
                }
            } else if (auto *dialog = qobject_cast<QDialog *>(widget);
                       dialog && dialog->findChild<QLineEdit *>(QStringLiteral("passwordLineEdit"))) {
                errors << QStringLiteral("password prompt");
                dialog->reject();
            }
        }
    }

    QTimer m_timer;
};

QString barState(const Gui::InlineNotificationBar *bar)
{
    if (bar->isHidden())
        return QStringLiteral("hidden");
    return (bar->kind() == Gui::InlineNotificationBar::Kind::Error ? QStringLiteral("error: ") : QStringLiteral("shown: "))
           + bar->text();
}

bool barShowsError(const Gui::InlineNotificationBar *bar)
{
    return !bar->isHidden() && bar->kind() == Gui::InlineNotificationBar::Kind::Error;
}

}

class LiveImapOperationsTest : public QObject
{
    Q_OBJECT
private slots:
    /**
     * Expects a loopback IMAPS server on PLASMA_TEST_IMAPS_PORT with the user operations@example.test, using the
     * synthetic password. INBOX holds "Synthetic keep", "Synthetic trash", "Synthetic purge" and "Synthetic expunge";
     * Trash and Junk hold two messages each, and Projects one. PLASMA_TEST_MAILDIR is that user's Maildir. The server's
     * files are the evidence: each step waits for them to change. See tools/dev-container/live-imap-operations.sh.
     */
    void destructiveActions()
    {
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Profile profile(directory, port);
        Dialogs dialogs;

        const QString inbox = QStringLiteral(".");
        const QString trash = QStringLiteral(".Trash");
        const QString junk = QStringLiteral(".Junk");
        QCOMPARE(storedMessages(inbox), 4);
        QCOMPARE(storedMessages(trash), 2);
        QCOMPARE(storedMessages(junk), 2);
        QCOMPARE(storedMessages(QStringLiteral(".Projects")), 1);

        Gui::MainWindow window(&profile.settings);
        QList<Confirmation> asked;
        window.setConfirmationPrompt([&asked](Confirmation kind, const QString &, const QString &) {
            asked << kind;
            UiUtils::DestructiveActions::ConfirmationAnswer answer;
            answer.accepted = true;
            return answer;
        });
        window.resize(1400, 900);
        window.show();
        auto *tree = window.findChild<Gui::MailBoxTreeView *>();
        auto *messages = window.findChild<Gui::MsgListView *>(QStringLiteral("messageListView"));
        auto *bar = window.findChild<Gui::InlineNotificationBar *>(QStringLiteral("messageNotificationBar"));
        auto *markAsDeleted = window.findChild<QAction *>(QStringLiteral("action_mark_as_deleted"));
        auto *deletePermanently = window.findChild<QAction *>(QStringLiteral("action_delete_permanently"));
        auto *movesToTrash = window.findChild<QAction *>(QStringLiteral("deleteMovesToTrash"));
        auto *emptyTrash = window.findChild<QAction *>(QStringLiteral("emptyTrash"));
        auto *emptyJunk = window.findChild<QAction *>(QStringLiteral("emptyJunk"));
        auto *rename = window.findChild<QAction *>(QStringLiteral("renameMailbox"));
        QVERIFY(tree && messages && bar && markAsDeleted && deletePermanently && movesToTrash && emptyTrash && emptyJunk && rename);
        QVERIFY(movesToTrash->isChecked());

        QModelIndex inboxIndex;
        QTRY_VERIFY_WITH_TIMEOUT((inboxIndex = findMailbox(tree->model(), QStringLiteral("INBOX"))).isValid(), 20000);
        clickMailbox(tree, inboxIndex);
        QTRY_COMPARE_WITH_TIMEOUT(currentMailbox(window), QStringLiteral("INBOX"), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(findMessage(messages, QStringLiteral("Synthetic expunge")).isValid(), 20000);
        // Trash and Junk are recognised by name, as the server does not advertise SPECIAL-USE
        QTRY_VERIFY_WITH_TIMEOUT(findMailbox(tree->model(), QStringLiteral("Trash")).isValid(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(findMailbox(tree->model(), QStringLiteral("Junk")).isValid(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(emptyTrash->isEnabled() && emptyJunk->isEnabled(), 10000);

        // Delete moves the message to Trash on the server, and Undo moves it back
        QVERIFY(clickMessage(messages, QStringLiteral("Synthetic trash")));
        QTRY_VERIFY_WITH_TIMEOUT(!markAsDeleted->isCheckable(), 10000);
        markAsDeleted->trigger();
        QTRY_COMPARE_WITH_TIMEOUT(storedMessages(trash), 3, 15000);
        QTRY_COMPARE_WITH_TIMEOUT(storedMessages(inbox), 3, 15000);
        QPushButton *undo = nullptr;
        QTRY_VERIFY2_WITH_TIMEOUT((undo = bar->findChild<QPushButton *>(QStringLiteral("undoMove"))) && undo->isVisible(),
                                  qPrintable(barState(bar)), 15000);
        QVERIFY(bar->text().contains(QLatin1String("Trash")));
        QTest::mouseClick(undo, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(storedMessages(trash), 2, 15000);
        QTRY_COMPARE_WITH_TIMEOUT(storedMessages(inbox), 4, 15000);
        QTRY_VERIFY2_WITH_TIMEOUT(!bar->isHidden() && bar->text().contains(QLatin1String("back to INBOX")), qPrintable(barState(bar)),
                                  15000);
        QVERIFY(asked.isEmpty());

        // Delete Permanently asks, then removes the message from the server without going through Trash
        QTRY_VERIFY_WITH_TIMEOUT(clickMessage(messages, QStringLiteral("Synthetic purge")), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(deletePermanently->isEnabled(), 10000);
        deletePermanently->trigger();
        QCOMPARE(asked, QList<Confirmation>{Confirmation::DeletePermanently});
        QTRY_COMPARE_WITH_TIMEOUT(storedMessages(inbox), 3, 15000);
        QCOMPARE(storedMessages(trash), 2);
        QTRY_VERIFY2_WITH_TIMEOUT(!bar->isHidden() && bar->text().contains(QLatin1String("Permanently deleted 1 message")),
                                  qPrintable(barState(bar)), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(!findMessage(messages, QStringLiteral("Synthetic purge")).isValid(), 10000);

        // With "Move Deleted Messages to Trash" off, Delete only marks the message, and Expunge removes it
        movesToTrash->setChecked(false);
        QTRY_VERIFY_WITH_TIMEOUT(clickMessage(messages, QStringLiteral("Synthetic expunge")), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(markAsDeleted->isCheckable(), 10000);
        QVERIFY(!markAsDeleted->isChecked());
        markAsDeleted->trigger();
        QTRY_COMPARE_WITH_TIMEOUT(storedDeleted(inbox), 1, 15000);
        QCOMPARE(storedMessages(inbox), 3);
        QCOMPARE(storedMessages(trash), 2);
        QVERIFY(QMetaObject::invokeMethod(&window, "slotExpunge"));
        QCOMPARE(asked.size(), 2);
        QCOMPARE(asked.last(), Confirmation::Expunge);
        QTRY_COMPARE_WITH_TIMEOUT(storedMessages(inbox), 2, 15000);
        QCOMPARE(storedDeleted(inbox), 0);
        QTRY_VERIFY_WITH_TIMEOUT(!findMessage(messages, QStringLiteral("Synthetic expunge")).isValid(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(findMessage(messages, QStringLiteral("Synthetic trash")).isValid(), 10000);
        movesToTrash->setChecked(true);
        QVERIFY2(!barShowsError(bar), qPrintable(barState(bar)));

        // Empty Trash and Empty Junk ask, then leave both folders empty; the other folders keep their messages
        emptyTrash->trigger();
        QCOMPARE(asked.size(), 3);
        QCOMPARE(asked.last(), Confirmation::EmptyFolder);
        QTRY_COMPARE_WITH_TIMEOUT(storedMessages(trash), 0, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(emptyJunk->isEnabled(), 10000);
        emptyJunk->trigger();
        QCOMPARE(asked.size(), 4);
        QCOMPARE(asked.last(), Confirmation::EmptyFolder);
        QTRY_COMPARE_WITH_TIMEOUT(storedMessages(junk), 0, 15000);
        QCOMPARE(storedMessages(inbox), 2);
        QCOMPARE(storedMessages(QStringLiteral(".Projects")), 1);
        QVERIFY2(!barShowsError(bar), qPrintable(barState(bar)));

        // Renaming the open folder renames it on the server, keeps its messages and keeps it open
        QModelIndex projects;
        QTRY_VERIFY_WITH_TIMEOUT((projects = findMailbox(tree->model(), QStringLiteral("Projects"))).isValid(), 10000);
        clickMailbox(tree, projects);
        QTRY_COMPARE_WITH_TIMEOUT(currentMailbox(window), QStringLiteral("Projects"), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(findMessage(messages, QStringLiteral("Synthetic plan")).isValid(), 15000);
        // Rename Folder is offered from the folder menu, which checks the folder when it opens
        QMenu *folderMenu = nullptr;
        for (QAction *top : window.menuBar()->actions()) {
            if (top->menu() && top->menu()->actions().contains(rename))
                folderMenu = top->menu();
        }
        QVERIFY(folderMenu);
        emit folderMenu->aboutToShow();
        QVERIFY(rename->isEnabled());
        dialogs.renameTo = QStringLiteral("Clients");
        rename->trigger();
        QVERIFY(dialogs.renameTo.isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(maildirFolder(QStringLiteral(".Clients")).exists(), 15000);
        QVERIFY(!maildirFolder(QStringLiteral(".Projects")).exists());
        QCOMPARE(storedMessages(QStringLiteral(".Clients")), 1);
        QTRY_VERIFY_WITH_TIMEOUT(findMailbox(tree->model(), QStringLiteral("Clients")).isValid(), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(!findMailbox(tree->model(), QStringLiteral("Projects")).isValid(), 15000);
        QTRY_COMPARE_WITH_TIMEOUT(currentMailbox(window), QStringLiteral("Clients"), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(findMessage(messages, QStringLiteral("Synthetic plan")).isValid(), 15000);

        QVERIFY2(!barShowsError(bar), qPrintable(barState(bar)));
        QVERIFY2(dialogs.errors.isEmpty(), qPrintable(dialogs.errors.join(QLatin1String(" | "))));
        QCOMPARE(asked.size(), 4);
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
    LiveImapOperationsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_LiveImapOperations.moc"
