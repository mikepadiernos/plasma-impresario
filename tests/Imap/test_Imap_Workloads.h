/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef TEST_IMAP_WORKLOADS_H
#define TEST_IMAP_WORKLOADS_H

#include <QElapsedTimer>
#include <functional>

#include "Utils/LibMailboxSync.h"

/** @short Measure responsiveness and memory for large mailboxes, long threads,
 * bulk actions and attachments

Each workload reports its time, the longest single pass through the event loop
(how long the window would stop responding), and the change in resident memory.
See docs/performance.md for the results and how to run them.

IMPRESARIO_WORKLOAD_SCALE multiplies the sizes (default 1). With
IMPRESARIO_WORKLOAD_BUDGETS=1 the time budgets are enforced; otherwise they are
only reported, so that a busy machine does not fail the ordinary test run.
IMPRESARIO_WORKLOAD_REPORT names a file to which a tab-separated line per
workload is appended.
*/
class ImapWorkloadsTest : public LibMailboxSync
{
    Q_OBJECT
private slots:
    void init() override;

    void testLargeMailboxSync();
    void testFilterLargeList();
    void testLongThread();
    void testBulkFlagChange();
    void testBulkExpunge();
    void testBulkVanished();
    void testManyAttachments();
    void testLargeAttachment();

private:
    struct Sample {
        qint64 rssKib = -1;
        qint64 peakKib = -1;
    };
    static Sample sample();
    static int scaled(int size);

    void begin();
    /** @short Run the event loop until @p done, recording the longest pass; false after @p timeoutMs */
    bool pump(const std::function<bool()> &done, int timeoutMs = 120000);
    /** @short Run @p work, which blocks the event loop, counting it as one pass */
    void blocking(const std::function<void()> &work);
    /** @short Report the workload measured since begin() and check its budgets

    @p passBudgetMs limits the longest single pass through the event loop, which
    is how long the window would not respond; -1 in the report means the
    workload ran through the fixture's own loop and was not measured.
    */
    void finish(const char *name, int items, qint64 budgetMs, qint64 passBudgetMs = 250);

    QElapsedTimer m_timer;
    qint64 m_longestPassMs = 0;
    Sample m_before;
};

#endif
