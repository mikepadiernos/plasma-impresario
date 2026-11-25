/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <algorithm>
#include <QItemSelectionModel>
#include <QtTest>
#include "test_Imap_ViewStatePreservation.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/PrettyMsgListModel.h"
#include "Imap/Model/ThreadingMsgListModel.h"
#include "Streams/FakeSocket.h"
#include "Utils/FakeCapabilitiesInjector.h"

using Imap::Mailbox::PrettyMsgListModel;
using Imap::Mailbox::ThreadingMsgListModel;

namespace {

QModelIndex findUid(const QAbstractItemModel *model, uint uid)
{
    const auto matches = model->match(model->index(0, 0), Imap::Mailbox::RoleMessageUid, uid, 1,
                                      Qt::MatchExactly | Qt::MatchRecursive);
    return matches.isEmpty() ? QModelIndex() : matches.constFirst();
}

Imap::Uids selectedUids(const QItemSelectionModel &selection)
{
    Imap::Uids uids;
    const auto rows = selection.selectedRows();
    for (const QModelIndex &row : rows)
        uids << row.data(Imap::Mailbox::RoleMessageUid).toUInt();
    std::sort(uids.begin(), uids.end());
    return uids;
}

Imap::Uids rowOrder(const QAbstractItemModel *model)
{
    Imap::Uids uids;
    for (int row = 0; row < model->rowCount(); ++row)
        uids << model->index(row, 0).data(Imap::Mailbox::RoleMessageUid).toUInt();
    return uids;
}

}

/** @short Open mailbox "a" with UIDs 6, 9 and 10 from the cache, on a server which can sort */
void ImapModelViewStatePreservationTest::helperOpenSortableMailbox()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("QRESYNC"));
    injector.injectCapability(QStringLiteral("SORT"));
    threadingModel->setUserWantsThreading(false);

    Imap::Mailbox::SyncState sync;
    sync.setExists(3);
    sync.setUidValidity(666);
    sync.setUidNext(15);
    sync.setHighestModSeq(33);
    sync.setUnSeenCount(3);
    sync.setRecent(0);
    model->cache()->setMailboxSyncState(QStringLiteral("a"), sync);
    model->cache()->setUidMapping(QStringLiteral("a"), Imap::Uids{6, 9, 10});
    model->cache()->setMsgFlags(QStringLiteral("a"), 6, QStringList());
    model->cache()->setMsgFlags(QStringLiteral("a"), 9, QStringList());
    model->cache()->setMsgFlags(QStringLiteral("a"), 10, QStringList());
    msgListModel->setMailbox(QStringLiteral("a"));
    cClient(t.mk("SELECT a (QRESYNC (666 33 (2 9)))\r\n"));
    cServer("* 3 EXISTS\r\n"
            "* OK [UIDVALIDITY 666] .\r\n"
            "* OK [UIDNEXT 15] .\r\n"
            "* OK [HIGHESTMODSEQ 33] .\r\n");
    cServer(t.last("OK selected\r\n"));
    cEmpty();
}

/** @short New mail, flag changes, filters, sorting again and expunges keep the selection of the message list */
void ImapModelViewStatePreservationTest::testSelectionSurvivesUpdates()
{
    helperOpenSortableMailbox();

    PrettyMsgListModel pretty(nullptr);
    pretty.setSourceModel(threadingModel);
    QItemSelectionModel selection(&pretty);
    QCOMPARE(rowOrder(&pretty), (Imap::Uids{6, 9, 10}));
    selection.select(findUid(&pretty, 6), QItemSelectionModel::Select | QItemSelectionModel::Rows);
    selection.setCurrentIndex(findUid(&pretty, 10), QItemSelectionModel::Select | QItemSelectionModel::Rows);
    const Imap::Uids selected{6, 10};
    QCOMPARE(selectedUids(selection), selected);

#define CHECK_SELECTION_KEPT() \
    { \
        QCOMPARE(selectedUids(selection), selected); \
        QCOMPARE(selection.currentIndex().data(Imap::Mailbox::RoleMessageUid).toUInt(), 10u); \
    }

    // The server sorts the messages
    pretty.sort(Imap::Mailbox::MsgListModel::SUBJECT, Qt::AscendingOrder);
    cClient(t.mk("UID SORT (SUBJECT) utf-8 ALL\r\n"));
    cServer("* SORT 10 6 9\r\n" + t.last("OK sorted\r\n"));
    QCOMPARE(rowOrder(&pretty), (Imap::Uids{10, 6, 9}));
    CHECK_SELECTION_KEPT();

    // ...and the same in the other direction, so that new mail comes in at the top
    pretty.sort(Imap::Mailbox::MsgListModel::SUBJECT, Qt::DescendingOrder);
    cEmpty();
    QCOMPARE(rowOrder(&pretty), (Imap::Uids{9, 6, 10}));
    CHECK_SELECTION_KEPT();

    // New mail arrives, and the server sorts it into the list
    cServer("* 4 EXISTS\r\n");
    cClient(t.mk("UID FETCH 15:* (FLAGS)\r\n"));
    cServer("* 4 FETCH (UID 15 FLAGS ())\r\n" + t.last("OK fetched\r\n"));
    CHECK_SELECTION_KEPT();
    cClient(t.mk("UID SORT (SUBJECT) utf-8 ALL\r\n"));
    cServer("* SORT 10 6 15 9\r\n" + t.last("OK sorted\r\n"));
    QCOMPARE(rowOrder(&pretty), (Imap::Uids{9, 15, 6, 10}));
    CHECK_SELECTION_KEPT();

    // A filter which all of them match, and the current message stays shown once it is read
    pretty.setKeptVisibleIndex(selection.currentIndex());
    pretty.setFilters(PrettyMsgListModel::FilterUnread);
    CHECK_SELECTION_KEPT();
    cServer("* 3 FETCH (UID 10 MODSEQ (34) FLAGS (\\Seen))\r\n");
    QVERIFY(findUid(&pretty, 10).data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool());
    CHECK_SELECTION_KEPT();
    pretty.setFilters(PrettyMsgListModel::NoFilter);
    CHECK_SELECTION_KEPT();

    // Another client flags a selected message
    cServer("* 1 FETCH (UID 6 MODSEQ (35) FLAGS (\\Flagged))\r\n");
    QVERIFY(findUid(&pretty, 6).data(Imap::Mailbox::RoleMessageIsMarkedFlagged).toBool());
    CHECK_SELECTION_KEPT();

    // Another client removes a message which is not selected
    cServer("* VANISHED 9\r\n");
    QCOMPARE(rowOrder(&pretty), (Imap::Uids{15, 6, 10}));
    CHECK_SELECTION_KEPT();

    // Sorting by something else is another layout change rather than a reset
    pretty.sort(-1, Qt::DescendingOrder);
    cEmpty();
    QCOMPARE(rowOrder(&pretty), (Imap::Uids{15, 10, 6}));
    CHECK_SELECTION_KEPT();

#undef CHECK_SELECTION_KEPT
    cEmpty();
    justKeepTask();
}

QTEST_GUILESS_MAIN(ImapModelViewStatePreservationTest)
