/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_Imap_Reliability.h"
#include "Imap/Model/DummyNetworkWatcher.h"
#include "Imap/Model/ItemRoles.h"
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

QTEST_GUILESS_MAIN(ImapReliabilityTest)
