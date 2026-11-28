/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef TEST_IMAP_MULTIACCOUNTUNREAD_H
#define TEST_IMAP_MULTIACCOUNTUNREAD_H

#include "Utils/LibMailboxSync.h"

namespace Imap {
namespace Mailbox {
class UnifiedMsgListModel;
}
}

/** @short Verify that unread counts stay consistent across two accounts and
 * changes which arrive from the server and the user at the same time

Each test runs two IMAP models, each with its own fake server. The fixture
state of LibMailboxSync belongs to the active account; useAccount() swaps it.
*/
class ImapMultiAccountUnreadTest : public LibMailboxSync
{
    Q_OBJECT
private slots:
    void init() override;
    void cleanup() override;

    void testServerChangesStayInTheirAccount();
    void testLocalChangesCountWhenConfirmed();
    void testServerUpdateDuringALocalChange();
    void testRefusedChangeKeepsTheCount();
    void testExpungeAndArrivalsInBothAccounts();
    void testOfflineAccountKeepsItsCount();

private:
    struct Fixture {
        Imap::Mailbox::Model *model = nullptr;
        Imap::Mailbox::MsgListModel *msgListModel = nullptr;
        Imap::Mailbox::ThreadingMsgListModel *threadingModel = nullptr;
        Streams::FakeSocketFactory *factory = nullptr;
        Imap::Mailbox::TestingTaskFactory *taskFactoryUnsafe = nullptr;
        QSignalSpy *errorSpy = nullptr;
        QSignalSpy *netErrorSpy = nullptr;
        QPersistentModelIndex idxA, idxB, idxC, msgListA, msgListB, msgListC;
        TagGenerator t;
        uint existsA = 0, uidValidityA = 0, uidNextA = 0;
        Imap::Uids uidMapA;
    };

    void swapFixture();
    /** @short Make @p account (0 or 1) the one which the LibMailboxSync helpers drive */
    void useAccount(int account);
    /** @short Check that every view of the unread messages agrees, and return the total */
    int checkCounts(int expectedFirst, int expectedSecond);

    Fixture m_other;
    int m_active = 0;
    Imap::Mailbox::UnifiedMsgListModel *m_unified = nullptr;
    Imap::Mailbox::MsgListModel *m_lists[2] = {nullptr, nullptr};
    Imap::Mailbox::Model *m_models[2] = {nullptr, nullptr};
};

#endif
