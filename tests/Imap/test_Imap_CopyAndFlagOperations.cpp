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

#include "test_Imap_CopyAndFlagOperations.h"
#include "Utils/FakeCapabilitiesInjector.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/ThreadingMsgListModel.h"
#include "Imap/Tasks/CopyMoveMessagesTask.h"
#include "Imap/Tasks/ObtainSynchronizedMailboxTask.h"

using namespace Imap::Mailbox;

void CopyAndFlagTest::testMoveRfc3501()
{
    helperMove(JUST_3501);
}

void CopyAndFlagTest::testMoveUidPlus()
{
    helperMove(HAS_UIDPLUS);
}

void CopyAndFlagTest::testMoveRfcMove()
{
    helperMove(HAS_MOVE);
}

void CopyAndFlagTest::helperMove(const MoveFeatures serverFeatures)
{
    FakeCapabilitiesInjector injector(model);
    switch (serverFeatures) {
    case JUST_3501:
        // nothing is needed
        break;
    case HAS_UIDPLUS:
        injector.injectCapability(QStringLiteral("UIDPLUS"));
        break;
    case HAS_MOVE:
        injector.injectCapability(QStringLiteral("MOVE"));
        break;
    }

    // Push the data to the cache
    existsA = 3;
    uidNextA = 5;
    uidValidityA = 666;
    for (uint i = 1; i <= existsA; ++i)
        uidMapA << i;
    helperSyncAWithMessagesEmptyState();
    helperCheckCache();
    helperVerifyUidMapA();
    QString mailbox = QStringLiteral("a");
    QString del = QStringLiteral("\\Deleted");
    Q_FOREACH(const auto uid, uidMapA) {
        // No message shall be marked as deleted
        QVERIFY(!model->cache()->msgFlags(mailbox, uid).contains(del));
    }

    // FIXME: this API sucks rather hard; it will have to change to use indexes at some point, if only for sanity
    auto aMailboxPtr = dynamic_cast<TreeItemMailbox *>(Model::realTreeItem(idxA));
    Q_ASSERT(aMailboxPtr);
    model->copyMoveMessages(aMailboxPtr, QStringLiteral("b"), Imap::Uids() << 2, MOVE);

    if (serverFeatures == HAS_MOVE) {
        cClient(t.mk("UID MOVE 2 b\r\n"));
        cServer("* 2 EXPUNGE\r\n" + t.last("OK moved\r\n"));
        --existsA;
        uidMapA.remove(1);
        helperCheckCache();
        helperVerifyUidMapA();
        Q_FOREACH(const auto uid, uidMapA) {
            // None message shall be marked as deleted
            QVERIFY(!model->cache()->msgFlags(mailbox, uid).contains(del));
        }
        cEmpty();
    } else {
        cClient(t.mk("UID COPY 2 b\r\n"));
        cServer(t.last("OK copied\r\n"));
        cClient(t.mk("UID STORE 2 +FLAGS.SILENT \\Deleted\r\n"));
        cServer(t.last("OK stored\r\n"));
        QVERIFY(model->cache()->msgFlags(mailbox, 2).contains(del));
        if (serverFeatures == HAS_UIDPLUS) {
            cClient(t.mk("UID EXPUNGE 2\r\n"));
            cServer("* 2 EXPUNGE\r\n" + t.last("OK expunged\r\n"));
            --existsA;
            uidMapA.remove(1);
            helperCheckCache();
            helperVerifyUidMapA();
            Q_FOREACH(const auto uid, uidMapA) {
                // None message shall be marked as deleted
                QVERIFY(!model->cache()->msgFlags(mailbox, uid).contains(del));
            }
        }
        cEmpty();
    }
    justKeepTask();
}

void CopyAndFlagTest::testUpdateAllFlags()
{
    // Push the data to the cache
    existsA = 4;
    uidNextA = 5;
    uidValidityA = 666;
    for (uint i = 1; i <= existsA; ++i)
        uidMapA << i;
    // cannot use helperSyncAWithMessagesEmptyState(), we want to have control over the flags
    QCOMPARE(model->rowCount(msgListA), 0);
    cClient(t.mk("SELECT a\r\n"));
    cServer("* 4 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] UIDs valid\r\n"
            "* OK [UIDNEXT 5] Predicted next UID\r\n"
            + t.last("OK selected\r\n"));
    cClient(t.mk("UID SEARCH ALL\r\n"));
    cServer("* SEARCH 3 4 2 1\r\n" + t.last("OK search\r\n"));
    cClient(t.mk("FETCH 1:4 (FLAGS)\r\n"));
    cServer("* 1 FETCH (FLAGS ())\r\n"
            "* 2 FETCH (FLAGS (\\SEEN))\r\n"
            "* 3 FETCH (FLAGS (\\Answered \\Seen))\r\n"
            "* 4 FETCH (FLAGS (\\Answered))\r\n"
            + t.last("OK fetched\r\n"));
    helperCheckCache();
    helperVerifyUidMapA();
    QString mailbox = QStringLiteral("a");
    QString seen = QStringLiteral("\\Seen");
    QVERIFY(!msgListA.model()->index(0, 0, msgListA).data(RoleMessageIsMarkedRead).toBool());
    QVERIFY(!model->cache()->msgFlags(mailbox, 1).contains(seen));
    QVERIFY(msgListA.model()->index(1, 0, msgListA).data(RoleMessageIsMarkedRead).toBool());
    QVERIFY(model->cache()->msgFlags(mailbox, 2).contains(seen));
    QVERIFY(msgListA.model()->index(2, 0, msgListA).data(RoleMessageIsMarkedRead).toBool());
    QVERIFY(model->cache()->msgFlags(mailbox, 3).contains(seen));
    QVERIFY(!msgListA.model()->index(3, 0, msgListA).data(RoleMessageIsMarkedRead).toBool());
    QVERIFY(!model->cache()->msgFlags(mailbox, 4).contains(seen));

    // Mark all messages as read
    model->markMailboxAsRead(idxA);
    cClient(t.mk("STORE 1:* +FLAGS.SILENT \\Seen\r\n"));
    cServer(t.last("OK stored\r\n"));
    for (uint i = 0; i < existsA; ++i) {
        QVERIFY(msgListA.model()->index(i, 0, msgListA).data(RoleMessageIsMarkedRead).toBool());
        QVERIFY(model->cache()->msgFlags(mailbox, uidMapA[i]).contains(seen));
    }

    cEmpty();
    justKeepTask();
}

void CopyAndFlagTest::helperSyncThreeMessages()
{
    existsA = 3;
    uidNextA = 5;
    uidValidityA = 666;
    for (uint i = 1; i <= existsA; ++i)
        uidMapA << i;
    helperSyncAWithMessagesEmptyState();
    helperCheckCache();
    helperVerifyUidMapA();
}

void CopyAndFlagTest::testCopyUidFromMove()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("MOVE"));
    injector.injectCapability(QStringLiteral("UIDPLUS"));
    helperSyncThreeMessages();

    QModelIndexList messages;
    messages << msgListA.model()->index(1, 0, msgListA) << msgListA.model()->index(2, 0, msgListA);
    auto *task = qobject_cast<CopyMoveMessagesTask *>(model->copyMoveMessages(QStringLiteral("b"), messages, MOVE));
    QVERIFY(task);
    QSignalSpy copyUidSpy(task, &CopyMoveMessagesTask::copyUidReceived);
    QSignalSpy completedSpy(task, &ImapTask::completed);
    // The task deletes itself once it completes, so its results are read while it does
    uint validity = 0;
    Imap::Uids sourceUids, destinationUids;
    QString target;
    connect(task, &ImapTask::completed, this, [&]() {
        validity = task->copyUidValidity();
        sourceUids = task->copyUidSourceUids();
        destinationUids = task->copyUidDestinationUids();
        target = task->targetMailboxName();
    });

    cClient(t.mk("UID MOVE 2:3 b\r\n"));
    // A COPYUID for UIDs which were not part of this command must be left alone
    cServer("* OK [COPYUID 77 1 9] someone else's\r\n"
            "* OK [COPYUID 77 2:3 10:11] moving\r\n"
            "* 2 EXPUNGE\r\n"
            "* 2 EXPUNGE\r\n"
            + t.last("OK moved\r\n"));
    QCOMPARE(completedSpy.size(), 1);
    QCOMPARE(copyUidSpy.size(), 1);
    QCOMPARE(copyUidSpy.at(0).at(0).toUInt(), 77u);
    QCOMPARE(validity, 77u);
    QCOMPARE(sourceUids, Imap::Uids() << 2 << 3);
    QCOMPARE(destinationUids, Imap::Uids() << 10 << 11);
    QCOMPARE(target, QStringLiteral("b"));
    existsA = 1;
    uidMapA = Imap::Uids() << 1;
    helperCheckCache();
    helperVerifyUidMapA();
    cEmpty();
    justKeepTask();
}

void CopyAndFlagTest::testCopyUidFromCopy()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("UIDPLUS"));
    helperSyncThreeMessages();

    QModelIndexList messages;
    messages << msgListA.model()->index(1, 0, msgListA);
    auto *task = qobject_cast<CopyMoveMessagesTask *>(model->copyMoveMessages(QStringLiteral("b"), messages, MOVE));
    QVERIFY(task);
    QSignalSpy copyUidSpy(task, &CopyMoveMessagesTask::copyUidReceived);
    // The task deletes itself once the copy completes, so its results are read when they arrive
    Imap::Uids sourceUids, destinationUids;
    connect(task, &CopyMoveMessagesTask::copyUidReceived, this, [&]() {
        sourceUids = task->copyUidSourceUids();
        destinationUids = task->copyUidDestinationUids();
    });

    cClient(t.mk("UID COPY 2 b\r\n"));
    cServer(t.last("OK [COPYUID 77 2 5] copied\r\n"));
    QCOMPARE(copyUidSpy.size(), 1);
    QCOMPARE(sourceUids, Imap::Uids() << 2);
    QCOMPARE(destinationUids, Imap::Uids() << 5);
    cClient(t.mk("UID STORE 2 +FLAGS.SILENT \\Deleted\r\n"));
    cServer(t.last("OK stored\r\n"));
    cClient(t.mk("UID EXPUNGE 2\r\n"));
    cServer("* 2 EXPUNGE\r\n" + t.last("OK expunged\r\n"));
    cEmpty();
    justKeepTask();
}

void CopyAndFlagTest::testMoveByUid()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("MOVE"));
    helperSyncThreeMessages();
    // This is how the GUI finds the folder to undo a move from
    QCOMPARE(model->mailboxIndexByName(QStringLiteral("a")), QModelIndex(idxA));
    QVERIFY(!model->mailboxIndexByName(QStringLiteral("nonexistent")).isValid());
    QVERIFY(!model->mailboxIndexByName(QString()).isValid());

    auto *task = qobject_cast<CopyMoveMessagesTask *>(
                model->copyMoveMessagesByUid(idxA, Imap::Uids() << 3 << 1 << 3, QStringLiteral("b"), MOVE, 666));
    QVERIFY(task);
    QSignalSpy completedSpy(task, &ImapTask::completed);
    Imap::Uids destinationUids{0};
    connect(task, &ImapTask::completed, this, [&]() { destinationUids = task->copyUidDestinationUids(); });
    cClient(t.mk("UID MOVE 1,3 b\r\n"));
    cServer("* 3 EXPUNGE\r\n* 1 EXPUNGE\r\n" + t.last("OK moved\r\n"));
    QCOMPARE(completedSpy.size(), 1);
    // No COPYUID from this server
    QVERIFY(destinationUids.isEmpty());
    existsA = 1;
    uidMapA = Imap::Uids() << 2;
    helperCheckCache();
    helperVerifyUidMapA();
    cEmpty();
    justKeepTask();
}

void CopyAndFlagTest::testMoveByUidChecksUidValidity()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("MOVE"));
    helperSyncThreeMessages();

    auto *task = model->copyMoveMessagesByUid(idxA, Imap::Uids() << 1, QStringLiteral("b"), MOVE, 999);
    QVERIFY(task);
    QSignalSpy failedSpy(task, &ImapTask::failed);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(failedSpy.size(), 1);
    cEmpty();
    justKeepTask();
}

void CopyAndFlagTest::testDeletePermanentlyUidPlus()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("UIDPLUS"));
    helperSyncThreeMessages();

    bool expungeScheduled = false;
    QVERIFY(model->deleteMessagesPermanently(QModelIndexList() << msgListA.model()->index(1, 0, msgListA), &expungeScheduled));
    QVERIFY(expungeScheduled);
    cClient(t.mk("UID STORE 2 +FLAGS (\\Deleted)\r\n"));
    cServer("* 2 FETCH (FLAGS (\\Deleted))\r\n" + t.last("OK stored\r\n"));
    // Only the selected message is removed, not other messages which happen to be marked as deleted
    cClient(t.mk("UID EXPUNGE 2\r\n"));
    cServer("* 2 EXPUNGE\r\n" + t.last("OK expunged\r\n"));
    existsA = 2;
    uidMapA = Imap::Uids() << 1 << 3;
    helperCheckCache();
    helperVerifyUidMapA();
    cEmpty();
    justKeepTask();
}

void CopyAndFlagTest::testDeletePermanentlyWithoutUidPlus()
{
    helperSyncThreeMessages();

    bool expungeScheduled = true;
    QVERIFY(model->deleteMessagesPermanently(QModelIndexList() << msgListA.model()->index(1, 0, msgListA), &expungeScheduled));
    QVERIFY(!expungeScheduled);
    cClient(t.mk("UID STORE 2 +FLAGS (\\Deleted)\r\n"));
    cServer("* 2 FETCH (FLAGS (\\Deleted))\r\n" + t.last("OK stored\r\n"));
    QVERIFY(model->cache()->msgFlags(QStringLiteral("a"), 2).contains(QStringLiteral("\\Deleted")));
    cEmpty();
    justKeepTask();
}

void CopyAndFlagTest::testEmptyMailbox()
{
    helperSyncThreeMessages();
    QSignalSpy emptiedSpy(model, &Model::mailboxEmptied);
    QSignalSpy emptyFailedSpy(model, &Model::mailboxEmptyFailed);

    model->emptyMailbox(idxA);
    cClient(t.mk("STORE 1:* +FLAGS.SILENT \\Deleted\r\n"));
    cServer(t.last("OK stored\r\n"));
    cClient(t.mk("EXPUNGE\r\n"));
    cServer("* 1 EXPUNGE\r\n* 1 EXPUNGE\r\n* 1 EXPUNGE\r\n" + t.last("OK expunged\r\n"));
    QCOMPARE(emptiedSpy.size(), 1);
    QCOMPARE(emptiedSpy.at(0).at(0).toString(), QStringLiteral("a"));
    QVERIFY(emptyFailedSpy.isEmpty());
    existsA = 0;
    uidMapA.clear();
    helperCheckCache();
    helperVerifyUidMapA();
    cEmpty();
    justKeepTask();
}

void CopyAndFlagTest::testEmptyMailboxFailure()
{
    helperSyncThreeMessages();
    QSignalSpy emptiedSpy(model, &Model::mailboxEmptied);
    QSignalSpy emptyFailedSpy(model, &Model::mailboxEmptyFailed);

    model->emptyMailbox(idxA);
    cClient(t.mk("STORE 1:* +FLAGS.SILENT \\Deleted\r\n"));
    cServer(t.last("NO read-only\r\n"));
    // Nothing gets expunged when the messages could not be marked
    cEmpty();
    QCOMPARE(emptyFailedSpy.size(), 1);
    QCOMPARE(emptyFailedSpy.at(0).at(0).toString(), QStringLiteral("a"));
    // The server's explanation reaches the user
    QVERIFY(emptyFailedSpy.at(0).at(1).toString().contains(QLatin1String("read-only")));
    QVERIFY(emptiedSpy.isEmpty());
    justKeepTask();
}

/** @short The task behind marking messages as read tells when the server has confirmed the change */
void CopyAndFlagTest::testMarkReadReportsCompletion()
{
    helperSyncThreeMessages();

    const QModelIndexList messages{msgListA.model()->index(1, 0, msgListA), msgListA.model()->index(2, 0, msgListA)};
    ImapTask *task = model->markMessagesRead(messages, FLAG_ADD);
    QVERIFY(task);
    QSignalSpy completedSpy(task, &ImapTask::completed);
    QSignalSpy failedSpy(task, &ImapTask::failed);
    cClient(t.mk("UID STORE 2:3 +FLAGS (\\Seen)\r\n"));
    QVERIFY(completedSpy.isEmpty());
    cServer("* 2 FETCH (FLAGS (\\Seen))\r\n* 3 FETCH (FLAGS (\\Seen))\r\n" + t.last("OK stored\r\n"));
    QCOMPARE(completedSpy.size(), 1);
    QVERIFY(failedSpy.isEmpty());
    cEmpty();
    justKeepTask();
}

/** @short A refusal carries the server's explanation, so that it can be shown */
void CopyAndFlagTest::testMarkReadReportsFailure()
{
    helperSyncThreeMessages();

    ImapTask *task = model->markMessagesDeleted(QModelIndexList() << msgListA.model()->index(0, 0, msgListA), FLAG_ADD);
    QVERIFY(task);
    QSignalSpy completedSpy(task, &ImapTask::completed);
    QSignalSpy failedSpy(task, &ImapTask::failed);
    cClient(t.mk("UID STORE 1 +FLAGS (\\Deleted)\r\n"));
    cServer(t.last("NO [NOPERM] read-only folder\r\n"));
    QCOMPARE(failedSpy.size(), 1);
    QVERIFY(failedSpy.at(0).at(0).toString().contains(QLatin1String("read-only folder")));
    QVERIFY(completedSpy.isEmpty());
    cEmpty();
    justKeepTask();
}

/** @short Gmail's labels are fetched on request and offered through a role, also in the message list */
void CopyAndFlagTest::testGmailLabels()
{
    FakeCapabilitiesInjector injector(model);
    QVERIFY(!model->hasGmailLabels());
    injector.injectCapability(QStringLiteral("X-GM-EXT-1"));
    QVERIFY(model->hasGmailLabels());
    helperSyncThreeMessages();

    const QModelIndex message = msgListA.model()->index(1, 0, msgListA);
    QVERIFY(!message.data(RoleMessageGmailLabels).isValid());
    QSignalSpy changedSpy(model, &QAbstractItemModel::dataChanged);
    model->requestGmailLabels(message);
    cClient(t.mk("UID FETCH 2 (X-GM-LABELS)\r\n"));
    cServer("* 2 FETCH (UID 2 X-GM-LABELS (\\Inbox \"Trips/2026\"))\r\n" + t.last("OK fetched\r\n"));
    const QStringList expected{QStringLiteral("\\Inbox"), QStringLiteral("Trips/2026")};
    QCOMPARE(message.data(RoleMessageGmailLabels).toStringList(), expected);
    QVERIFY(!changedSpy.isEmpty());

    MsgListModel msgListModel(nullptr, model);
    msgListModel.setMailbox(idxA);
    QCOMPARE(msgListModel.index(1, 0).data(RoleMessageGmailLabels).toStringList(), expected);
    // Messages whose labels were never asked for have none yet
    QVERIFY(!msgListModel.index(0, 0).data(RoleMessageGmailLabels).isValid());

    // Labels are asked for again; an unchanged answer changes nothing, a new one replaces them
    changedSpy.clear();
    model->requestGmailLabels(message);
    cClient(t.mk("UID FETCH 2 (X-GM-LABELS)\r\n"));
    cServer("* 2 FETCH (X-GM-LABELS (\\Inbox \"Trips/2026\") UID 2)\r\n" + t.last("OK fetched\r\n"));
    QVERIFY(changedSpy.isEmpty());
    model->requestGmailLabels(message);
    cClient(t.mk("UID FETCH 2 (X-GM-LABELS)\r\n"));
    cServer("* 2 FETCH (UID 2 X-GM-LABELS ())\r\n" + t.last("OK fetched\r\n"));
    QVERIFY(!changedSpy.isEmpty());
    QVERIFY(message.data(RoleMessageGmailLabels).isValid());
    QVERIFY(message.data(RoleMessageGmailLabels).toStringList().isEmpty());
    cEmpty();
    justKeepTask();
}

void CopyAndFlagTest::testNoGmailLabelsWithoutExtension()
{
    helperSyncThreeMessages();
    QVERIFY(!model->hasGmailLabels());
    model->requestGmailLabels(msgListA.model()->index(0, 0, msgListA));
    cEmpty();
    justKeepTask();
}

QTEST_GUILESS_MAIN(CopyAndFlagTest)
