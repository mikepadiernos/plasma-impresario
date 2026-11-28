/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_Imap_MultiAccountUnread.h"
#include "Imap/Model/DummyNetworkWatcher.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/ThreadingMsgListModel.h"
#include "Imap/Model/UnifiedMsgListModel.h"
#include "Imap/Model/Utils.h"
#include "Imap/Tasks/ImapTask.h"
#include "Streams/FakeSocket.h"
#include "UiUtils/DesktopFeedback.h"

using namespace Imap::Mailbox;

namespace {

const QString first = QStringLiteral("first");
const QString second = QStringLiteral("second");

void settle()
{
    for (int i = 0; i < 10; ++i)
        QCoreApplication::processEvents();
}

/** Unread messages counted from the flags the list shows */
int countedFromFlags(const QAbstractItemModel *list)
{
    int count = 0;
    for (int row = 0; row < list->rowCount(); ++row) {
        const QVariant read = list->index(row, 0).data(RoleMessageIsMarkedRead);
        if (read.isValid() && !read.toBool())
            ++count;
    }
    return count;
}

} // namespace

void ImapMultiAccountUnreadTest::swapFixture()
{
    std::swap(model, m_other.model);
    std::swap(msgListModel, m_other.msgListModel);
    std::swap(threadingModel, m_other.threadingModel);
    std::swap(factory, m_other.factory);
    std::swap(taskFactoryUnsafe, m_other.taskFactoryUnsafe);
    std::swap(errorSpy, m_other.errorSpy);
    std::swap(netErrorSpy, m_other.netErrorSpy);
    std::swap(idxA, m_other.idxA);
    std::swap(idxB, m_other.idxB);
    std::swap(idxC, m_other.idxC);
    std::swap(msgListA, m_other.msgListA);
    std::swap(msgListB, m_other.msgListB);
    std::swap(msgListC, m_other.msgListC);
    std::swap(t, m_other.t);
    std::swap(existsA, m_other.existsA);
    std::swap(uidValidityA, m_other.uidValidityA);
    std::swap(uidNextA, m_other.uidNextA);
    std::swap(uidMapA, m_other.uidMapA);
    m_active = 1 - m_active;
}

void ImapMultiAccountUnreadTest::useAccount(int account)
{
    if (account != m_active)
        swapFixture();
}

void ImapMultiAccountUnreadTest::init()
{
    m_fakeOpenTask = true;
    m_fakeListCommand = true;
    m_initialConnectionState = Imap::CONN_STATE_AUTHENTICATED;
    m_active = 0;

    // Each account gets its own model, server and cache; setupLogging() would
    // delete the first account's spies, so they are put aside first
    LibMailboxSync::init();
    t.reset();
    swapFixture();
    errorSpy = nullptr;
    netErrorSpy = nullptr;
    LibMailboxSync::init();
    t.reset();
    // The second account is active; sync it, then the first
    initialMessages(4);
    cEmpty();
    m_models[1] = model;
    m_lists[1] = msgListModel;
    useAccount(0);
    initialMessages(3);
    cEmpty();
    m_models[0] = model;
    m_lists[0] = msgListModel;

    m_unified = new UnifiedMsgListModel(this);
    QVERIFY(m_unified->setAccountSource(first, QStringLiteral("First"), m_lists[0]));
    QVERIFY(m_unified->setAccountSource(second, QStringLiteral("Second"), m_lists[1]));
    QCOMPARE(m_unified->messageCount(), 7);
}

void ImapMultiAccountUnreadTest::cleanup()
{
    delete m_unified;
    m_unified = nullptr;
    m_lists[0] = m_lists[1] = nullptr;
    m_models[0] = m_models[1] = nullptr;
    LibMailboxSync::cleanup();
    swapFixture();
    LibMailboxSync::cleanup();
    m_active = 0;
}

int ImapMultiAccountUnreadTest::checkCounts(int expectedFirst, int expectedSecond)
{
    const int expected[2] = {expectedFirst, expectedSecond};
    const QString ids[2] = {first, second};
    QList<int> counts;
    for (int account = 0; account < 2; ++account) {
        const QModelIndex inbox = m_lists[account]->currentMailbox();
        const int folderCount = inbox.data(RoleUnreadMessageCount).toInt();
        // The folder counter, the flags in the list and the unified inbox agree
        if (folderCount != expected[account] || countedFromFlags(m_lists[account]) != expected[account]
            || m_unified->unreadCount(ids[account]) != expected[account]) {
            qWarning() << "account" << account << "folder" << folderCount << "flags"
                       << countedFromFlags(m_lists[account]) << "unified" << m_unified->unreadCount(ids[account])
                       << "expected" << expected[account];
            return -1;
        }
        counts << folderCount;
    }
    const int total = m_unified->unreadCount();
    // The task manager badge adds up the same counts
    if (total != expectedFirst + expectedSecond || UiUtils::DesktopFeedback::totalUnread(counts) != total)
        return -1;
    return total;
}

/** @short Flags which one server changes affect only the count of its account */
void ImapMultiAccountUnreadTest::testServerChangesStayInTheirAccount()
{
    QCOMPARE(checkCounts(0, 0), 0);
    QSignalSpy changed(m_unified, &QAbstractItemModel::dataChanged);
    // The main window refreshes the sidebar and the task manager badge on this signal
    QSignalSpy firstCounts(m_models[0], &Model::messageCountPossiblyChanged);
    QSignalSpy secondCounts(m_models[1], &Model::messageCountPossiblyChanged);

    useAccount(0);
    cServer("* 2 FETCH (FLAGS ())\r\n");
    QCOMPARE(checkCounts(1, 0), 1);
    QVERIFY(!changed.isEmpty());
    QVERIFY(!firstCounts.isEmpty());
    QCOMPARE(firstCounts.last().first().toModelIndex(), QModelIndex(idxA));
    QVERIFY(secondCounts.isEmpty());
    firstCounts.clear();

    useAccount(1);
    cServer("* 1 FETCH (FLAGS (\\Answered))\r\n* 4 FETCH (FLAGS ())\r\n");
    QCOMPARE(checkCounts(1, 2), 3);
    QVERIFY(!secondCounts.isEmpty());
    QVERIFY(firstCounts.isEmpty());

    // Another client reads a message of the first account
    useAccount(0);
    cServer("* 2 FETCH (FLAGS (\\Seen))\r\n");
    QCOMPARE(checkCounts(0, 2), 2);
    // A repeated update changes nothing
    cServer("* 2 FETCH (FLAGS (\\Seen))\r\n");
    QCOMPARE(checkCounts(0, 2), 2);
    useAccount(1);
    cEmpty();
    useAccount(0);
    cEmpty();
}

/** @short Changes which the user makes in both accounts count once each
 * server confirms them, in the account they belong to

The model shows the flags which the server reports rather than the ones it asked
for, so a change never counts twice or before it happened. */
void ImapMultiAccountUnreadTest::testLocalChangesCountWhenConfirmed()
{
    m_models[0]->markMessagesRead({Imap::deproxifiedIndex(m_lists[0]->index(0, 0))}, FLAG_REMOVE);
    m_models[1]->markMessagesRead(
        {Imap::deproxifiedIndex(m_lists[1]->index(1, 0)), Imap::deproxifiedIndex(m_lists[1]->index(2, 0))},
        FLAG_REMOVE);
    QCOMPARE(checkCounts(0, 0), 0);

    useAccount(0);
    cClient(t.mk("UID STORE 1 -FLAGS (\\Seen)\r\n"));
    useAccount(1);
    cClient(t.mk("UID STORE 2:3 -FLAGS (\\Seen)\r\n"));
    // The second server answers first
    cServer("* 2 FETCH (FLAGS (\\Answered))\r\n* 3 FETCH (FLAGS ())\r\n" + t.last("OK stored\r\n"));
    QCOMPARE(checkCounts(0, 2), 2);
    useAccount(0);
    cServer("* 1 FETCH (FLAGS ())\r\n" + t.last("OK stored\r\n"));
    QCOMPARE(checkCounts(1, 2), 3);

    // Marking a message of the unified inbox reaches the message's own account
    const QModelIndex inUnified = m_unified->index(4, 0);
    QCOMPARE(m_unified->data(inUnified, RoleMessageAccountId).toString(), second);
    const QModelIndex source = m_unified->mapToSource(inUnified);
    QCOMPARE(source.model(), m_lists[1]);
    m_models[1]->markMessagesRead({Imap::deproxifiedIndex(source)}, FLAG_ADD);
    useAccount(1);
    cClient(t.mk("UID STORE 2 +FLAGS (\\Seen)\r\n"));
    cServer("* 2 FETCH (FLAGS (\\Seen \\Answered))\r\n" + t.last("OK stored\r\n"));
    QCOMPARE(checkCounts(1, 1), 2);
    cEmpty();
    useAccount(0);
    cEmpty();
}

/** @short An update from another client which arrives while the user's own
 * change is on its way is counted once */
void ImapMultiAccountUnreadTest::testServerUpdateDuringALocalChange()
{
    useAccount(1);
    m_models[1]->markMessagesRead({Imap::deproxifiedIndex(m_lists[1]->index(0, 0))}, FLAG_REMOVE);
    cClient(t.mk("UID STORE 1 -FLAGS (\\Seen)\r\n"));
    // Before the answer, another client marks the same message and one more as unread
    cServer("* 1 FETCH (FLAGS ())\r\n* 3 FETCH (FLAGS (\\Answered))\r\n");
    QCOMPARE(checkCounts(0, 2), 2);
    // The server's own answer repeats the state of the message
    cServer("* 1 FETCH (FLAGS ())\r\n" + t.last("OK stored\r\n"));
    QCOMPARE(checkCounts(0, 2), 2);

    // Meanwhile the other account changes as well
    useAccount(0);
    cServer("* 3 FETCH (FLAGS ())\r\n");
    QCOMPARE(checkCounts(1, 2), 3);
    useAccount(1);
    cEmpty();
    useAccount(0);
    cEmpty();
}

/** @short A change which the server refuses leaves the count as the server has it */
void ImapMultiAccountUnreadTest::testRefusedChangeKeepsTheCount()
{
    useAccount(0);
    cServer("* 3 FETCH (FLAGS ())\r\n");
    QCOMPARE(checkCounts(1, 0), 1);
    ImapTask *task = m_models[0]->markMessagesRead({Imap::deproxifiedIndex(m_lists[0]->index(2, 0))}, FLAG_ADD);
    QVERIFY(task);
    QSignalSpy failed(task, &ImapTask::failed);
    cClient(t.mk("UID STORE 3 +FLAGS (\\Seen)\r\n"));
    cServer(t.last("NO [CANNOT] Read-only folder\r\n"));
    QCOMPARE(failed.size(), 1);
    QCOMPARE(checkCounts(1, 0), 1);
    cEmpty();
}

/** @short Removed unread messages and new arrivals update their account only */
void ImapMultiAccountUnreadTest::testExpungeAndArrivalsInBothAccounts()
{
    useAccount(0);
    cServer("* 1 FETCH (FLAGS ())\r\n* 3 FETCH (FLAGS ())\r\n");
    useAccount(1);
    cServer("* 2 FETCH (FLAGS ())\r\n");
    QCOMPARE(checkCounts(2, 1), 3);

    QSignalSpy firstCounts(m_models[0], &Model::messageCountPossiblyChanged);
    QSignalSpy secondCounts(m_models[1], &Model::messageCountPossiblyChanged);

    // An unread message is removed by another client
    useAccount(0);
    cServer("* 1 EXPUNGE\r\n");
    QCOMPARE(m_unified->messageCount(first), 2);
    QCOMPARE(checkCounts(1, 1), 2);
    QVERIFY(!firstCounts.isEmpty());
    QVERIFY(secondCounts.isEmpty());

    // A new unread message arrives in the second account
    useAccount(1);
    cServer("* 5 EXISTS\r\n");
    cClient(t.mk("UID FETCH 5:* (FLAGS)\r\n"));
    cServer("* 5 FETCH (UID 5 FLAGS ())\r\n" + t.last("OK fetched\r\n"));
    QCOMPARE(m_unified->messageCount(second), 5);
    QCOMPARE(checkCounts(1, 2), 3);
    QVERIFY(!secondCounts.isEmpty());

    // A read one arrives in the first account
    useAccount(0);
    cServer("* 3 EXISTS\r\n");
    cClient(t.mk("UID FETCH 4:* (FLAGS)\r\n"));
    cServer("* 3 FETCH (UID 4 FLAGS (\\Seen))\r\n" + t.last("OK fetched\r\n"));
    QCOMPARE(m_unified->messageCount(), 8);
    QCOMPARE(checkCounts(1, 2), 3);

    // Expunging read messages leaves the counts alone
    useAccount(1);
    cServer("* 1 EXPUNGE\r\n");
    QCOMPARE(checkCounts(1, 2), 3);
    useAccount(1);
    cEmpty();
    useAccount(0);
    cEmpty();
}

/** @short An account which loses its connection keeps the count it last knew,
 * and the other account goes on */
void ImapMultiAccountUnreadTest::testOfflineAccountKeepsItsCount()
{
    useAccount(0);
    cServer("* 2 FETCH (FLAGS ())\r\n");
    useAccount(1);
    cServer("* 3 FETCH (FLAGS ())\r\n");
    QCOMPARE(checkCounts(1, 1), 2);

    useAccount(0);
    SOCK->fakeDisconnect(QStringLiteral("Connection reset by peer"));
    settle();
    QCOMPARE(netErrorSpy->size(), 1);
    netErrorSpy->clear();
    m_unified->setAccountState(first, UnifiedMsgListModel::AccountState::Offline);
    QCOMPARE(m_unified->messageCount(first), 3);
    QCOMPARE(checkCounts(1, 1), 2);

    useAccount(1);
    cServer("* 4 FETCH (FLAGS ())\r\n");
    QCOMPARE(checkCounts(1, 2), 3);
    cEmpty();
}

QTEST_GUILESS_MAIN(ImapMultiAccountUnreadTest)
