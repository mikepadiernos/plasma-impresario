/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Gui/AttachmentPanel.h"
#include "Gui/MailBoxTreeView.h"
#include "Gui/MsgListView.h"
#include "Gui/Window.h"
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "Imap/Model/Utils.h"
#include "configure.cmake.h"
#ifdef IMPRESARIO_HAVE_WEBENGINE
#include "Gui/WebEngineMessageProfile.h"
#endif
#include "static_plugins.h"

#include <KLocalizedString>
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

namespace {

// Synthetic credentials for a disposable loopback server; never point this test at a real account.
const QString syntheticPassword = QStringLiteral("synthetic-test-only");
const QString userAddress = QStringLiteral("workload@example.test");

int sizeFromEnvironment(const char *name, int fallback)
{
    bool ok = false;
    const int value = qEnvironmentVariableIntValue(name, &ok);
    return ok && value > 0 ? value : fallback;
}

/** A value from /proc/self/status in KiB, or -1 where it is not available */
qint64 procStatusKib(const QByteArray &key)
{
    QFile status(QStringLiteral("/proc/self/status"));
    if (!status.open(QIODevice::ReadOnly))
        return -1;
    for (const QByteArray &line : status.readAll().split('\n')) {
        if (line.startsWith(QByteArray(key + ':')))
            return line.mid(key.size() + 1).trimmed().split(' ').value(0).toLongLong();
    }
    return -1;
}

QModelIndex findMailbox(const QAbstractItemModel *model, const QString &name)
{
    if (!model || model->rowCount() == 0)
        return {};
    const QModelIndexList matches
        = model->match(model->index(0, 0), Imap::Mailbox::RoleMailboxName, name, 1, Qt::MatchExactly);
    return matches.isEmpty() ? QModelIndex() : QModelIndex(matches.first());
}

QModelIndex findMessage(Gui::MsgListView *view, const QString &subject)
{
    QAbstractItemModel *model = view->model();
    if (!model || model->rowCount() == 0)
        return {};
    const QModelIndexList matches = model->match(
        model->index(0, 0), Imap::Mailbox::RoleMessageSubject, subject, 1, Qt::MatchExactly | Qt::MatchRecursive);
    return matches.isEmpty() ? QModelIndex() : QModelIndex(matches.first());
}

void clickMailbox(Gui::MailBoxTreeView *tree, const QModelIndex &mailbox)
{
    tree->scrollTo(mailbox);
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualRect(mailbox).center());
}

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

int listedMessages(Gui::MainWindow &window)
{
    return window.imapAccess()->msgListModel()->rowCount();
}

/** The number of INBOX messages stored without the Maildir flag S, which is IMAP's \Seen */
int storedUnread()
{
    const QDir cur(qEnvironmentVariable("PLASMA_TEST_MAILDIR") + QStringLiteral("/cur"));
    int count = 0;
    for (const QString &file : cur.entryList(QDir::Files)) {
        const int info = file.lastIndexOf(QLatin1String(":2,"));
        if (info < 0 || !file.mid(info + 3).contains(QLatin1Char('S')))
            ++count;
    }
    return count
        + int(QDir(qEnvironmentVariable("PLASMA_TEST_MAILDIR") + QStringLiteral("/new")).entryList(QDir::Files).size());
}

/** Depth of the chain that starts at @arg index, following each message's first reply */
int chainDepth(const QAbstractItemModel *model, QModelIndex index)
{
    int depth = 0;
    while (index.isValid()) {
        ++depth;
        index = model->index(0, 0, index);
    }
    return depth;
}

/** The first body part with @arg mimeType below @arg index, depth first */
QModelIndex findPart(const QAbstractItemModel *model, const QModelIndex &index, const QString &mimeType)
{
    for (int row = 0, rows = model->rowCount(index); row < rows; ++row) {
        const QModelIndex child = model->index(row, 0, index);
        if (child.data(Imap::Mailbox::RolePartMimeType).toString() == mimeType)
            return child;
        const QModelIndex found = findPart(model, child, mimeType);
        if (found.isValid())
            return found;
    }
    return {};
}

/** A profile with one account on the synthetic server, with threads off */
class Profile
{
public:
    explicit Profile(QTemporaryDir &directory, int port)
        : settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat)
    {
        settings.setValue(Common::SettingsNames::guiShowSystray, false);
        settings.setValue(Common::SettingsNames::guiMsgListShowThreading, false);
        settings.setValue(Common::SettingsNames::passwordPlugin, QStringLiteral("cleartextpassword"));
        settings.setValue(QStringLiteral("accounts/ids"), QStringList{QStringLiteral("account-1")});
        settings.setValue(QStringLiteral("accounts/active"), QStringLiteral("account-1"));
        settings.setValue(QStringLiteral("accounts/next"), 2);
        settings.setValue(QStringLiteral("accounts/names/account-1"), QStringLiteral("Workload"));
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

/** Accepts the synthetic server's certificate and records any other dialog */
class Dialogs : public QObject
{
public:
    QStringList errors;

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
                    message->button(QMessageBox::Yes)->click();
                } else {
                    errors << message->text();
                    message->reject();
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

/** Measures one step: its time, the longest gap between ticks of a 5 ms timer, which is how long the window stopped
 * responding, and the memory it added */
class Step
{
public:
    Step(const char *name, int items)
        : m_name(name)
        , m_items(items)
        , m_rssBefore(procStatusKib("VmRSS"))
    {
        m_ticks.setTimerType(Qt::PreciseTimer);
        m_ticks.setInterval(5);
        QObject::connect(&m_ticks, &QTimer::timeout, &m_ticks,
            [this] { m_longestGap = std::max(m_longestGap, m_sinceTick.restart()); });
        m_sinceTick.start();
        m_elapsed.start();
        m_ticks.start();
    }

    void finish()
    {
        const qint64 elapsed = m_elapsed.elapsed();
        m_ticks.stop();
        m_longestGap = std::max(m_longestGap, m_sinceTick.elapsed());
        const qint64 rss = procStatusKib("VmRSS");
        const qint64 rssDelta = rss >= 0 && m_rssBefore >= 0 ? rss - m_rssBefore : -1;
        qInfo().noquote() << QStringLiteral("workload=%1 items=%2 ms=%3 longest_gap_ms=%4 rss_delta_kib=%5 rss_kib=%6")
                                 .arg(QLatin1String(m_name))
                                 .arg(m_items)
                                 .arg(elapsed)
                                 .arg(m_longestGap)
                                 .arg(rssDelta)
                                 .arg(rss);
        const QString report = qEnvironmentVariable("PLASMA_TEST_WORKLOAD_REPORT");
        if (!report.isEmpty()) {
            QFile out(report);
            if (out.open(QIODevice::Append | QIODevice::Text)) {
                out.write(QStringLiteral("%1\t%2\t%3\t%4\t%5\t%6\n")
                        .arg(QLatin1String(m_name))
                        .arg(m_items)
                        .arg(elapsed)
                        .arg(m_longestGap)
                        .arg(rssDelta)
                        .arg(rss)
                        .toUtf8());
            }
        }
    }

private:
    const char *m_name;
    int m_items;
    qint64 m_rssBefore;
    qint64 m_longestGap = 0;
    QElapsedTimer m_elapsed;
    QElapsedTimer m_sinceTick;
    QTimer m_ticks;
};

}

class LiveImapWorkloadsTest : public QObject
{
    Q_OBJECT
private slots:
    /**
     * Expects a loopback IMAPS server on PLASMA_TEST_IMAPS_PORT with the user workload@example.test, using the
     * synthetic password, seeded by tools/dev-container/seed-workload-mail.py with the sizes in PLASMA_TEST_WORKLOAD_*.
     * PLASMA_TEST_MAILDIR is that user's Maildir and PLASMA_TEST_DOVEADM runs doveadm against that server, as another
     * client would change the mailbox. See tools/dev-container/live-imap-workloads.sh and docs/performance.md.
     */
    void realWorldWorkloads()
    {
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
        const QStringList doveadm
            = qEnvironmentVariable("PLASMA_TEST_DOVEADM").split(QLatin1Char(' '), Qt::SkipEmptyParts);
        QVERIFY(!doveadm.isEmpty());
        const int size = sizeFromEnvironment("PLASMA_TEST_WORKLOAD_MESSAGES", 50000);
        const int conversation = sizeFromEnvironment("PLASMA_TEST_WORKLOAD_CONVERSATION", 500);
        const int attachments = sizeFromEnvironment("PLASMA_TEST_WORKLOAD_ATTACHMENTS", 200);
        const int mib = sizeFromEnvironment("PLASMA_TEST_WORKLOAD_MIB", 20);
        const int unread = size / 10;
        const int removed = size / 5;
        // Generous limits for a loaded container; docs/performance.md records the measured times
        const int timeout = 180000;

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Profile profile(directory, port);
        Dialogs dialogs;
        QCOMPARE(storedUnread(), unread);

        Gui::MainWindow window(&profile.settings);
        window.resize(1400, 900);
        window.show();
        auto *tree = window.findChild<Gui::MailBoxTreeView *>();
        auto *messages = window.findChild<Gui::MsgListView *>(QStringLiteral("messageListView"));
        auto *attachmentPanel = window.findChild<Gui::AttachmentPanel *>();
        auto *filterUnread = window.findChild<QAction *>(QStringLiteral("messageListFilterUnread"));
        QAction *threads = nullptr;
        for (QAction *action : window.findChildren<QAction *>()) {
            if (action->text() == Gui::MainWindow::tr("Show Messages in &Threads"))
                threads = action;
        }
        QVERIFY(tree && messages && attachmentPanel && filterUnread && threads);

        QModelIndex inbox;
        QTRY_VERIFY_WITH_TIMEOUT((inbox = findMailbox(tree->model(), QStringLiteral("INBOX"))).isValid(), 30000);

        // Opening a large INBOX: every message is listed, the visible ones have their headers, and the count is right
        {
            Step step("open-large-folder", size);
            clickMailbox(tree, inbox);
            QTRY_COMPARE_WITH_TIMEOUT(currentMailbox(window), QStringLiteral("INBOX"), 10000);
            QTRY_COMPARE_WITH_TIMEOUT(listedMessages(window), size, timeout);
            QTRY_VERIFY_WITH_TIMEOUT(
                !messages->model()->index(0, 0).data(Imap::Mailbox::RoleMessageSubject).toString().isEmpty(), timeout);
            QTRY_COMPARE_WITH_TIMEOUT(inbox.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), unread, timeout);
            step.finish();
        }
        const bool qresync = window.imapModel()->capabilities().contains(QStringLiteral("QRESYNC"));
        qInfo() << "server QRESYNC:" << qresync;

        // The Unread quick filter, on and off
        QCOMPARE(messages->model()->rowCount(), size);
        {
            Step step("filter-unread", size);
            filterUnread->trigger();
            QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), unread, timeout);
            filterUnread->trigger();
            QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), size, timeout);
            step.finish();
        }

        // Mark Mailbox as Read reaches the server and every count
        {
            Step step("mark-folder-read", unread);
            QVERIFY(QMetaObject::invokeMethod(&window, "slotMarkCurrentMailboxRead"));
            QTRY_COMPARE_WITH_TIMEOUT(inbox.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 0, timeout);
            step.finish();
        }
        QTRY_COMPARE_WITH_TIMEOUT(storedUnread(), 0, 30000);

        // Another client removes a fifth of the INBOX; the server reports it while the folder is open. The first step
        // includes the other client's own work; the second starts once it has finished.
        {
            Step serverStep("remove-by-other-client-server", removed);
            QProcess expunge;
            expunge.start(doveadm.first(),
                doveadm.mid(1)
                    + QStringList{QStringLiteral("expunge"), QStringLiteral("-u"), userAddress,
                        QStringLiteral("mailbox"), QStringLiteral("INBOX"), QStringLiteral("uid"),
                        QStringLiteral("1:%1").arg(removed)});
            QVERIFY(expunge.waitForStarted());
            QTRY_VERIFY_WITH_TIMEOUT(expunge.state() == QProcess::NotRunning, timeout);
            QVERIFY2(expunge.exitCode() == 0, expunge.readAllStandardError().constData());
            serverStep.finish();
            Step step(qresync ? "remove-by-other-client-vanished" : "remove-by-other-client-expunge", removed);
            QTRY_COMPARE_WITH_TIMEOUT(listedMessages(window), size - removed, timeout);
            step.finish();
        }
        QCOMPARE(
            window.imapAccess()->msgListModel()->index(0, 0).data(Imap::Mailbox::RoleMessageUid).toInt(), removed + 1);

        // A long reply chain, shown in threads
        QModelIndex conversationFolder;
        QTRY_VERIFY_WITH_TIMEOUT(
            (conversationFolder = findMailbox(tree->model(), QStringLiteral("Conversation"))).isValid(), 10000);
        clickMailbox(tree, conversationFolder);
        QTRY_COMPARE_WITH_TIMEOUT(currentMailbox(window), QStringLiteral("Conversation"), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(listedMessages(window), conversation, timeout);
        QTRY_VERIFY_WITH_TIMEOUT(threads->isEnabled(), 30000);
        QVERIFY(!threads->isChecked());
        {
            Step step("thread-long-conversation", conversation);
            threads->trigger();
            QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 1, timeout);
            QTRY_COMPARE_WITH_TIMEOUT(
                chainDepth(messages->model(), messages->model()->index(0, 0)), conversation, timeout);
            step.finish();
        }
        threads->trigger();

        // A message with many attachments lists every one of them
        QModelIndex attachmentFolder;
        QTRY_VERIFY_WITH_TIMEOUT(
            (attachmentFolder = findMailbox(tree->model(), QStringLiteral("Attachments"))).isValid(), 10000);
        clickMailbox(tree, attachmentFolder);
        QTRY_COMPARE_WITH_TIMEOUT(currentMailbox(window), QStringLiteral("Attachments"), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(
            findMessage(messages, QStringLiteral("Synthetic large attachment")).isValid(), timeout);
        // Open the reader once, so that starting the web engine is not counted
        QVERIFY(clickMessage(messages, QStringLiteral("Synthetic large attachment")));
        QTRY_COMPARE_WITH_TIMEOUT(attachmentPanel->attachmentCount(), 1, timeout);
        {
            Step step("open-many-attachments", attachments);
            QVERIFY(clickMessage(messages, QStringLiteral("Synthetic many attachments")));
            QTRY_COMPARE_WITH_TIMEOUT(attachmentPanel->attachmentCount(), attachments, timeout);
            step.finish();
        }

        // A large attachment is downloaded whole
        QTRY_VERIFY_WITH_TIMEOUT(clickMessage(messages, QStringLiteral("Synthetic large attachment")), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(attachmentPanel->attachmentCount(), 1, timeout);
        const QModelIndex message = Imap::deproxifiedIndex(messages->currentIndex());
        QVERIFY(message.isValid());
        QModelIndex part;
        QTRY_VERIFY_WITH_TIMEOUT(
            (part = findPart(message.model(), message, QStringLiteral("application/octet-stream"))).isValid(), timeout);
        {
            Step step("download-large-attachment", mib);
            part.data(Imap::Mailbox::RolePartData);
            QTRY_VERIFY_WITH_TIMEOUT(part.data(Imap::Mailbox::RoleIsFetched).toBool(), timeout);
            step.finish();
        }
        QCOMPARE(part.data(Imap::Mailbox::RolePartData).toByteArray().size(), qsizetype(mib) * 1024 * 1024);

        QVERIFY2(dialogs.errors.isEmpty(), qPrintable(dialogs.errors.join(QLatin1String(" | "))));
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
    qunsetenv("PLASMA_IMPRESARIO_PROFILE");
    QStandardPaths::setTestModeEnabled(true);
#ifdef IMPRESARIO_HAVE_WEBENGINE
    Gui::WebEngineMessageProfile::registerScheme();
#endif
    Common::registerMetaTypes();
    QApplication app(argc, argv);
    for (auto location :
        {QStandardPaths::GenericConfigLocation, QStandardPaths::AppDataLocation, QStandardPaths::CacheLocation}) {
        if (!QStandardPaths::writableLocation(location).startsWith(homePath + QLatin1Char('/')))
            return 1;
    }
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(license);
    KLocalizedString::setApplicationDomain("plasma-impresario");
    if (!Imap::Mailbox::SystemNetworkWatcher::init())
        return 1;
    LiveImapWorkloadsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_LiveImapWorkloads.moc"
