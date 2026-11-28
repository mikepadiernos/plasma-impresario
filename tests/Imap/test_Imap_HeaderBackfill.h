/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef TEST_IMAP_HEADERBACKFILL_H
#define TEST_IMAP_HEADERBACKFILL_H

#include "Utils/LibMailboxSync.h"

namespace Imap {
namespace Mailbox {
class HeaderBackfill;
}
}

/** @short Loading the headers of the open folder in the background, recent ones first */
class ImapHeaderBackfillTest : public LibMailboxSync
{
    Q_OBJECT

private slots:
    void init() override;
    void cleanup() override;
    void testRecentFirstThenOlderInBatches();
    void testCachedHeadersSkipTheServer();
    void testWaitsForTheUser();
    void testWaitsForOtherWork();
    void testStopsAtTheLimit();
    void testFailedSearchLoadsNewestFirst();
    void testPausesOnExpensiveNetwork();
    void testStopsOffline();
    void testFollowsTheOpenFolder();

private:
    /** @short Synchronize folder a with @p messages messages, holding the backfill meanwhile */
    void open(uint messages);

    Imap::Mailbox::HeaderBackfill *backfill = nullptr;
    bool otherWork = false;
};

#endif
