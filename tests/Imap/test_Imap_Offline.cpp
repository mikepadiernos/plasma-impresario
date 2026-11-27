/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "test_Imap_Offline.h"
#include "data.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/DummyNetworkWatcher.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/ThreadingMsgListModel.h"
#include "Imap/Tasks/ObtainSynchronizedMailboxTask.h"

using namespace Imap::Mailbox;

namespace {

/** The test suite has no ImapAccess, so the public policy slots would refuse to connect */
class PolicyWatcher : public DummyNetworkWatcher
{
public:
    using DummyNetworkWatcher::DummyNetworkWatcher;
    void choose(NetworkPolicy policy) { setDesiredNetworkPolicy(policy); }
};

}

void OfflineTest::init()
{
    LibMailboxSync::init();
    MsgListModel *msgListModelA = new MsgListModel(model, model);
    ThreadingMsgListModel *threadingA = new ThreadingMsgListModel(msgListModelA);
    threadingA->setSourceModel(msgListModelA);
    MsgListModel *msgListModelB = new MsgListModel(model, model);
    ThreadingMsgListModel *threadingB = new ThreadingMsgListModel(msgListModelB);
    threadingB->setSourceModel(msgListModelB);
}

/** @short Check that the STATUS processing does not break offline "sync" */
void OfflineTest::testStatusVsExistsCached()
{
    QString mailbox = QStringLiteral("a");

    // Push the data to the cache
    existsA = 10;
    uidNextA = 11;
    uidValidityA = 666;
    SyncState sync;
    sync.setExists(existsA);
    sync.setUidValidity(uidValidityA);
    sync.setUidNext(uidNextA);
    for (uint i = 1; i <= existsA; ++i)
        uidMapA << i;
    model->cache()->setMailboxSyncState(mailbox, sync);
    model->cache()->setUidMapping(mailbox, uidMapA);

    // Request the STATUS command to see how many messages are in there. This is not a synchronization, though
    // -- Trojita is *not* opening that mailbox, so the previously cached data remain.
    QCOMPARE(idxA.data(RoleTotalMessageCount).toInt(), 0);
    cClient(t.mk("STATUS a (MESSAGES UNSEEN RECENT)\r\n"));
    cServer("* STATUS a (MESSAGES 42 UNSEEN 2 RECENT 3)\r\n" + t.last("OK status\r\n"));
    QCOMPARE(idxA.data(RoleTotalMessageCount).toInt(), 42);
    LibMailboxSync::setModelNetworkPolicy(model, NETWORK_OFFLINE);
    cClient(t.mk("LOGOUT\r\n"));
    cServer(t.last("OK logged out\r\n") + "* BYE see ya\r\n");
    model->resyncMailbox(idxA);
    for (int i = 0; i < 10; ++i)
        QCoreApplication::processEvents();
    QCOMPARE(model->rowCount(msgListA), static_cast<int>(existsA));
    helperVerifyUidMapA();
    helperCheckCache();
}

void OfflineTest::testFinishedFetchDoesNotLeaveLogout()
{
    model->setProperty("trojita-imap-delayed-fetch-part", 0);
    helperSyncBNoMessages();
    cServer("* 1 EXISTS\r\n");
    cClient(t.mk("UID FETCH 1:* (FLAGS)\r\n"));
    cServer("* 1 FETCH (UID 333 FLAGS ())\r\n" + t.last("OK fetched\r\n"));
    const QModelIndex message = model->index(0, 0, msgListB);
    QCOMPARE(model->rowCount(message), 0);
    cClient(t.mk("UID FETCH 333 (" FETCH_METADATA_ITEMS ")\r\n"));
    cServer("* 1 FETCH (UID 333 BODYSTRUCTURE (" + bsPlaintext + "))\r\n" + t.last("OK fetched\r\n"));
    const QModelIndex part = model->index(0, 0, message);
    QVERIFY(part.isValid());
    part.data(RolePartData);
    cClient(t.mk("UID FETCH 333 (BODY.PEEK[1])\r\n"));
    const QByteArray fetchComplete = t.last("OK fetched\r\n");
    LibMailboxSync::setModelNetworkPolicy(model, NETWORK_OFFLINE);
    cClient(t.mk("LOGOUT\r\n"));
    QSignalSpy stateChanges(model, &Model::connectionStateChanged);
    cServer(fetchComplete);
    for (const auto &state : stateChanges)
        QVERIFY(state.at(1).value<Imap::ConnectionState>() != Imap::CONN_STATE_SELECTED);
    cServer("* BYE logging out\r\n" + t.last("OK logged out\r\n"));
    QVERIFY(errorSpy->isEmpty());
}

/** @short Sleeping logs out and waking connects again, without changing what the user chose */
void OfflineTest::testSystemSleepKeepsThePolicy()
{
    PolicyWatcher watcher(nullptr, model);
    watcher.choose(NETWORK_EXPENSIVE);
    QVERIFY(model->isNetworkAvailable());
    QVERIFY(!model->isNetworkOnline());
    QSignalSpy chosen(&watcher, &NetworkWatcher::desiredNetworkPolicyChanged);

    watcher.setSystemSleeping(true);
    QVERIFY(watcher.isSystemSleeping());
    cClient(t.mk("LOGOUT\r\n"));
    cServer(t.last("OK logged out\r\n") + "* BYE see ya\r\n");
    QVERIFY(!model->isNetworkAvailable());
    QCOMPARE(watcher.desiredNetworkPolicy(), NETWORK_EXPENSIVE);

    // Neither a scheduled reconnect nor a policy change connects while the system sleeps
    watcher.setSystemSleeping(true);
    QVERIFY(QMetaObject::invokeMethod(&watcher, "tryReconnect"));
    watcher.choose(NETWORK_ONLINE);
    QVERIFY(!model->isNetworkAvailable());

    watcher.setSystemSleeping(false);
    QVERIFY(!watcher.isSystemSleeping());
    QVERIFY(model->isNetworkOnline());
    QCOMPARE(watcher.desiredNetworkPolicy(), NETWORK_ONLINE);
    QVERIFY(chosen.isEmpty());
    QVERIFY(errorSpy->isEmpty());
}

/** @short Waking does not connect an account which the user took offline */
void OfflineTest::testSystemSleepWhileOffline()
{
    PolicyWatcher watcher(nullptr, model);
    watcher.choose(NETWORK_OFFLINE);
    cClient(t.mk("LOGOUT\r\n"));
    cServer(t.last("OK logged out\r\n") + "* BYE see ya\r\n");
    watcher.setSystemSleeping(true);
    watcher.setSystemSleeping(false);
    QVERIFY(!model->isNetworkAvailable());
}

QTEST_GUILESS_MAIN(OfflineTest)
