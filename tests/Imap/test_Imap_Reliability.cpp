/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_Imap_Reliability.h"
#include "Imap/Model/ConnectionStates.h"
#include "Imap/Model/DummyNetworkWatcher.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Tasks/ImapTask.h"
#include "Utils/FakeCapabilitiesInjector.h"
#include "Streams/FakeSocket.h"

using namespace Imap::Mailbox;

namespace {

/** The test suite has no ImapAccess, so the public policy slots would refuse to
 * connect */
class PolicyWatcher : public DummyNetworkWatcher
{
public:
    explicit PolicyWatcher(Model *model)
        : DummyNetworkWatcher(nullptr, model)
    {
        // The middle of the jitter, so that each delay is the plain doubled one
        backoff().setRandomSource([] { return 0.5; });
    }
    void choose(NetworkPolicy policy)
    {
        setDesiredNetworkPolicy(policy);
    }
};

void settle()
{
    for (int i = 0; i < 10; ++i)
        QCoreApplication::processEvents();
}

} // namespace

void ImapReliabilityTest::init()
{
    // Logging in needs the real connection task; the other tests start from an
    // authenticated connection
    const bool login = QByteArray(QTest::currentTestFunction()).startsWith("testLogin");
    m_fakeOpenTask = !login;
    m_fakeListCommand = !login;
    m_initialConnectionState = login ? Imap::CONN_STATE_CONNECTED_PRETLS_PRECAPS : Imap::CONN_STATE_AUTHENTICATED;
    LibMailboxSync::init();
    t.reset();
}

/** @short A dropped connection schedules the first delay, and the synced
 * messages stay readable */
void ImapReliabilityTest::testDisconnectSchedulesABackoff()
{
    PolicyWatcher watcher(model);
    watcher.choose(NETWORK_ONLINE);
    initialMessages(3);
    cEmpty();
    QSignalSpy scheduled(&watcher, &NetworkWatcher::reconnectAttemptScheduled);

    SOCK->fakeDisconnect(QStringLiteral("Connection reset by peer"));
    settle();

    QCOMPARE(netErrorSpy->size(), 1);
    QCOMPARE(scheduled.size(), 1);
    QCOMPARE(scheduled.first().first().toInt(), MIN_RECONNECT_TIMEOUT);
    QVERIFY(watcher.isReconnectScheduled());
    QVERIFY(watcher.reconnectRemaining() > 0);
    QCOMPARE(watcher.reconnectAttempts(), 1);
    QVERIFY(!model->isNetworkAvailable());
    QCOMPARE(watcher.desiredNetworkPolicy(), NETWORK_ONLINE);
    // Nothing is lost while the account waits
    QCOMPARE(model->rowCount(msgListA), 3);
    QCOMPARE(model->index(2, 0, msgListA).data(RoleMessageUid).toUInt(), 3u);
}

/** @short Each failure doubles the delay up to five minutes, and the jitter
 * keeps it within the limits */
void ImapReliabilityTest::testDelaysDoubleUpToTheLimit()
{
    PolicyWatcher watcher(model);
    watcher.choose(NETWORK_ONLINE);
    QSignalSpy scheduled(&watcher, &NetworkWatcher::reconnectAttemptScheduled);
    for (int i = 0; i < 9; ++i)
        QVERIFY(QMetaObject::invokeMethod(&watcher, "attemptReconnect"));
    QList<int> delays;
    for (const auto &arguments : scheduled)
        delays << arguments.first().toInt();
    QCOMPARE(delays, (QList<int>{5000, 10000, 20000, 40000, 80000, 160000, 300000, 300000, 300000}));
    QCOMPARE(watcher.reconnectAttempts(), 9);

    // At the ends of the jitter, the delays are spread by a fifth but never
    // shorter than the first nor over the limit
    scheduled.clear();
    watcher.reconnectNow();
    QCOMPARE(watcher.reconnectAttempts(), 0);
    watcher.backoff().setRandomSource([] { return 0.0; });
    QVERIFY(QMetaObject::invokeMethod(&watcher, "attemptReconnect"));
    QVERIFY(QMetaObject::invokeMethod(&watcher, "attemptReconnect"));
    watcher.backoff().setRandomSource([] { return 1.0; });
    QVERIFY(QMetaObject::invokeMethod(&watcher, "attemptReconnect"));
    for (int i = 0; i < 6; ++i)
        QVERIFY(QMetaObject::invokeMethod(&watcher, "attemptReconnect"));
    delays.clear();
    for (const auto &arguments : scheduled)
        delays << arguments.first().toInt();
    QCOMPARE(delays.at(0), 5000);
    QCOMPARE(delays.at(1), 8000);
    QCOMPARE(delays.at(2), 24000);
    QCOMPARE(delays.last(), MAX_RECONNECT_TIMEOUT);
}

/** @short Trying again right away connects, resynchronizes the open folder and
 * starts the delays from the first */
void ImapReliabilityTest::testReconnectNowConnectsAgain()
{
    PolicyWatcher watcher(model);
    watcher.choose(NETWORK_ONLINE);
    initialMessages(3);
    cEmpty();
    SOCK->fakeDisconnect(QStringLiteral("Connection timed out"));
    settle();
    QVERIFY(watcher.isReconnectScheduled());
    QSignalSpy reset(&watcher, &NetworkWatcher::resetReconnectState);

    watcher.reconnectNow();
    QVERIFY(!watcher.isReconnectScheduled());
    QCOMPARE(watcher.reconnectRemaining(), -1);
    QCOMPARE(watcher.reconnectAttempts(), 0);
    QVERIFY(model->isNetworkOnline());
    QVERIFY(!reset.isEmpty());

    // The new connection starts its tags again, selects the folder and keeps the
    // messages known before
    t.reset();
    // Going online lists the folders again, and the folder fills from the cache before the server answers
    settle();
    idxA = model->index(1, 0, QModelIndex());
    QCOMPARE(idxA.data(Qt::DisplayRole).toString(), QStringLiteral("a"));
    msgListA = model->index(0, 0, idxA);
    model->switchToMailbox(idxA);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 3 EXISTS\r\n* OK [UIDVALIDITY 333] .\r\n* OK [UIDNEXT 4] .\r\n" + t.last("OK selected\r\n"));
    cClient(t.mk("FETCH 1:3 (FLAGS)\r\n"));
    cServer(
        "* 1 FETCH (FLAGS (\\Seen))\r\n* 2 FETCH (FLAGS ())\r\n* 3 FETCH (FLAGS ())\r\n" + t.last("OK fetched\r\n"));
    QCOMPARE(model->rowCount(msgListA), 3);
    QCOMPARE(model->index(2, 0, msgListA).data(RoleMessageUid).toUInt(), 3u);
    cEmpty();
    QCOMPARE(netErrorSpy->size(), 1);
    QCOMPARE(watcher.reconnectAttempts(), 0);
}

/** @short Neither an account which the user took offline nor a sleeping system
 * connects on request */
void ImapReliabilityTest::testReconnectNowRespectsTheUser()
{
    PolicyWatcher watcher(model);
    watcher.choose(NETWORK_OFFLINE);
    cClient(t.mk("LOGOUT\r\n"));
    cServer(t.last("OK logged out\r\n") + "* BYE see ya\r\n");

    watcher.reconnectNow();
    QVERIFY(QMetaObject::invokeMethod(&watcher, "attemptReconnect"));
    QVERIFY(!watcher.isReconnectScheduled());
    QVERIFY(!model->isNetworkAvailable());

    watcher.setSystemSleeping(true);
    watcher.choose(NETWORK_ONLINE);
    watcher.reconnectNow();
    QVERIFY(QMetaObject::invokeMethod(&watcher, "attemptReconnect"));
    QVERIFY(!watcher.isReconnectScheduled());
    QVERIFY(!model->isNetworkAvailable());

    watcher.setSystemSleeping(false);
    QVERIFY(model->isNetworkOnline());
    QVERIFY(netErrorSpy->isEmpty());
}

/** @short A server which shuts down or drops idle clients costs a reconnect,
 * not a protocol error */
void ImapReliabilityTest::testServerByeIsALostConnection()
{
    PolicyWatcher watcher(model);
    watcher.choose(NETWORK_ONLINE);
    initialMessages(3);
    cEmpty();
    QSignalSpy scheduled(&watcher, &NetworkWatcher::reconnectAttemptScheduled);

    cServer("* BYE Server shutting down.\r\n");
    settle();

    QVERIFY(errorSpy->isEmpty());
    QCOMPARE(netErrorSpy->size(), 1);
    QVERIFY(netErrorSpy->first().first().toString().contains(QLatin1String("Server shutting down.")));
    QCOMPARE(scheduled.size(), 1);
    QVERIFY(!model->isNetworkAvailable());
    QCOMPARE(model->rowCount(msgListA), 3);
}

/** @short Rejected credentials wait for the user instead of logging in again
 * with the same password */
void ImapReliabilityTest::testLoginRejectedWaitsForTheUser()
{
    PolicyWatcher watcher(model);
    QObject context;
    int asked = 0;
    connect(
        model, &Model::authRequested, &context,
        [this, &asked]() {
            if (++asked > 1)
                return;
            model->setImapUser(QStringLiteral("luzr"));
            model->setImapPassword(QStringLiteral("expired"));
        },
        Qt::QueuedConnection);
    QSignalSpy rejected(model, &Model::authAttemptFailed);
    QSignalSpy scheduled(&watcher, &NetworkWatcher::reconnectAttemptScheduled);
    watcher.choose(NETWORK_ONLINE);

    model->rowCount(QModelIndex());
    settle();
    cServer("* OK [CAPABILITY IMAP4rev1] Service Ready\r\n");
    cClient(t.mk("LOGIN luzr expired\r\n"));
    cServer(t.last("NO [AUTHENTICATIONFAILED] Invalid credentials\r\n"));
    settle();

    QCOMPARE(rejected.size(), 1);
    QCOMPARE(asked, 2);
    QVERIFY(model->imapAuthError().contains(QLatin1String("Invalid credentials")));
    // Nothing is sent with the rejected password, and the failure is not a lost
    // connection
    cEmpty();
    QVERIFY(netErrorSpy->isEmpty());
    QVERIFY(scheduled.isEmpty());
    QVERIFY(!watcher.isReconnectScheduled());

    // Once the user signs in again, the waiting connection logs in
    model->setImapPassword(QStringLiteral("renewed"));
    cClient(t.mk("LOGIN luzr renewed\r\n"));
    cServer(t.last("OK [CAPABILITY IMAP4rev1] logged in\r\n"));
    cClient(t.mk("LIST \"\" \"%\"\r\n"));
    cServer(t.last("OK listed\r\n"));
    cEmpty();
    QCOMPARE(rejected.size(), 1);
}

/** @short An account is connected while any of its connections is logged in */
void ImapReliabilityTest::testConnectionStatesCombine()
{
    using Summary = ConnectionStates::Summary;
    ConnectionStates states;
    QCOMPARE(states.summary(), Summary::Disconnected);
    QCOMPARE(states.furthest(), Imap::CONN_STATE_NONE);

    QVERIFY(states.update(1, Imap::CONN_STATE_CONNECTING));
    QCOMPARE(states.summary(), Summary::Connecting);
    QVERIFY(!states.update(1, Imap::CONN_STATE_LOGIN));
    QVERIFY(states.update(1, Imap::CONN_STATE_AUTHENTICATED));
    QCOMPARE(states.summary(), Summary::Connected);

    // A second connection logging in, then the first one selecting a folder and
    // closing, keeps the account connected
    QVERIFY(!states.update(2, Imap::CONN_STATE_HOST_LOOKUP));
    QVERIFY(!states.update(1, Imap::CONN_STATE_SELECTED));
    QCOMPARE(states.furthest(), Imap::CONN_STATE_SELECTED);
    QCOMPARE(states.count(), 2);
    QVERIFY(states.update(1, Imap::CONN_STATE_LOGOUT));
    QCOMPARE(states.summary(), Summary::Connecting);
    QCOMPARE(states.furthest(), Imap::CONN_STATE_HOST_LOOKUP);
    QVERIFY(states.update(2, Imap::CONN_STATE_AUTHENTICATED));
    QCOMPARE(states.summary(), Summary::Connected);

    // Logging out is the end of a connection, not a state past being logged in
    QVERIFY(states.update(2, Imap::CONN_STATE_LOGOUT));
    QCOMPARE(states.summary(), Summary::Disconnected);
    QCOMPARE(states.count(), 0);
    QVERIFY(!states.update(2, Imap::CONN_STATE_NONE));

    states.update(3, Imap::CONN_STATE_SELECTED);
    states.clear();
    QCOMPARE(states.summary(), Summary::Disconnected);
}

/** @short A connection which the server breaks, or which stops making sense,
 * reports its end instead of looking open */
void ImapReliabilityTest::testBrokenConnectionReportsItsEnd()
{
    PolicyWatcher watcher(model);
    watcher.choose(NETWORK_ONLINE);
    initialMessages(3);
    cEmpty();
    QSignalSpy changed(model, &Model::connectionStateChanged);
    QSignalSpy fatal(model, &Model::logParserFatalError);

    m_expectsError = true;
    cServer("* 1 FETCH (UID x)\r\n");
    settle();

    QCOMPARE(fatal.size(), 1);
    QCOMPARE(changed.size(), 1);
    QCOMPARE(changed.first().at(0).toUInt(), fatal.first().at(0).toUInt());
    QCOMPARE(changed.first().at(1).value<Imap::ConnectionState>(), Imap::CONN_STATE_LOGOUT);
    ConnectionStates states;
    states.update(changed.first().at(0).toUInt(), Imap::CONN_STATE_SELECTED);
    states.update(changed.first().at(0).toUInt(), changed.first().at(1).value<Imap::ConnectionState>());
    QCOMPARE(states.summary(), ConnectionStates::Summary::Disconnected);
    // The synced messages stay readable
    QCOMPARE(model->rowCount(msgListA), 3);
    QVERIFY(!m_expectsError);
    QCOMPARE(errorSpy->size(), 1);
    errorSpy->clear();
}

/** @short A server which refuses to copy, as when the target is over quota,
 * leaves the message where it was and says why */
void ImapReliabilityTest::testRefusedCopyKeepsTheMessage()
{
    initialMessages(3);
    const QModelIndex message = model->index(1, 0, msgListA);
    ImapTask *task = model->copyMoveMessages(QStringLiteral("b"), {message}, MOVE);
    QVERIFY(task);
    QSignalSpy failed(task, &ImapTask::failed);

    cClient(t.mk("UID COPY 2 b\r\n"));
    cServer(t.last("NO [OVERQUOTA] Quota exceeded\r\n"));
    // Nothing is marked as deleted after a copy which did not happen
    cEmpty();
    QCOMPARE(failed.size(), 1);
    QVERIFY(failed.first().first().toString().contains(QLatin1String("Quota exceeded")));
    QCOMPARE(model->rowCount(msgListA), 3);
    QVERIFY(!model->cache()->msgFlags(QStringLiteral("a"), 2).contains(QStringLiteral("\\Deleted")));
    QVERIFY(!model->index(1, 0, msgListA).data(RoleMessageIsMarkedDeleted).toBool());
    justKeepTask();
}

/** @short A refused UID MOVE leaves the message where it was */
void ImapReliabilityTest::testRefusedMoveKeepsTheMessage()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("MOVE"));
    initialMessages(3);
    ImapTask *task = model->copyMoveMessages(QStringLiteral("b"), {model->index(2, 0, msgListA)}, MOVE);
    QVERIFY(task);
    QSignalSpy failed(task, &ImapTask::failed);

    cClient(t.mk("UID MOVE 3 b\r\n"));
    cServer(t.last("NO [TRYCREATE] No such mailbox\r\n"));
    cEmpty();
    QCOMPARE(failed.size(), 1);
    QVERIFY(failed.first().first().toString().contains(QLatin1String("No such mailbox")));
    QCOMPARE(model->rowCount(msgListA), 3);
    QCOMPARE(model->index(2, 0, msgListA).data(RoleMessageUid).toUInt(), 3u);
    justKeepTask();
}

/** @short A connection lost after the copy but before the original is removed
 * leaves the message in both folders rather than in neither */
void ImapReliabilityTest::testLostConnectionDuringMoveKeepsTheMessage()
{
    PolicyWatcher watcher(model);
    watcher.choose(NETWORK_ONLINE);
    initialMessages(3);
    ImapTask *task = model->copyMoveMessages(QStringLiteral("b"), {model->index(0, 0, msgListA)}, MOVE);
    QVERIFY(task);

    cClient(t.mk("UID COPY 1 b\r\n"));
    cServer(t.last("OK copied\r\n"));
    cClient(t.mk("UID STORE 1 +FLAGS.SILENT \\Deleted\r\n"));
    SOCK->fakeDisconnect(QStringLiteral("Connection reset by peer"));
    settle();

    QCOMPARE(netErrorSpy->size(), 1);
    // The message was not expunged, so it is still in its folder
    QCOMPARE(model->rowCount(msgListA), 3);
    QCOMPARE(model->index(0, 0, msgListA).data(RoleMessageUid).toUInt(), 1u);
    QVERIFY(watcher.isReconnectScheduled());
}

/** @short A flag change cut off by a lost connection is reported, and the
 * message keeps the state which the server confirmed */
void ImapReliabilityTest::testLostConnectionDuringFlagChangeIsReported()
{
    PolicyWatcher watcher(model);
    watcher.choose(NETWORK_ONLINE);
    initialMessages(3);
    const QModelIndex message = model->index(2, 0, msgListA);
    QVERIFY(message.data(RoleMessageIsMarkedRead).toBool());
    ImapTask *task = model->markMessagesRead({message}, FLAG_REMOVE);
    QVERIFY(task);
    QSignalSpy failed(task, &ImapTask::failed);
    QSignalSpy completed(task, &ImapTask::completed);

    cClient(t.mk("UID STORE 3 -FLAGS (\\Seen)\r\n"));
    SOCK->fakeDisconnect(QStringLiteral("Connection timed out"));
    settle();

    QCOMPARE(netErrorSpy->size(), 1);
    QCOMPARE(failed.size(), 1);
    QVERIFY(completed.isEmpty());
    QCOMPARE(model->rowCount(msgListA), 3);
    // The unconfirmed change is undone, so the message shows what the server has
    QVERIFY(model->index(2, 0, msgListA).data(RoleMessageIsMarkedRead).toBool());
    QVERIFY(model->cache()->msgFlags(QStringLiteral("a"), 3).contains(QStringLiteral("\\Seen")));
}

/** @short A folder which the server refuses to open says so through
 * mailboxSyncFailed, and the messages known before stay */
void ImapReliabilityTest::testRefusedSelectIsReported()
{
    initialMessages(3);
    QSignalSpy syncFailed(model, &Model::mailboxSyncFailed);
    const QModelIndex idxB = model->index(2, 0, QModelIndex());
    QCOMPARE(idxB.data(RoleMailboxName).toString(), QStringLiteral("b"));
    model->switchToMailbox(idxB);
    cClient(t.mk("SELECT b\r\n"));
    cServer(t.last("NO [UNAVAILABLE] Mailbox is locked\r\n"));
    settle();

    QCOMPARE(syncFailed.size(), 1);
    QCOMPARE(syncFailed.first().at(0).toString(), QStringLiteral("b"));
    QVERIFY(syncFailed.first().at(1).toString().contains(QLatin1String("Mailbox is locked")));
    QCOMPARE(model->rowCount(msgListA), 3);
}

QTEST_GUILESS_MAIN(ImapReliabilityTest)
