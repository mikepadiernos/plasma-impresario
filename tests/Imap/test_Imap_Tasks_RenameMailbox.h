/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_IMAP_TASKS_RENAMEMAILBOX
#define TEST_IMAP_TASKS_RENAMEMAILBOX

#include "Utils/LibMailboxSync.h"

class QSignalSpy;

class ImapModelRenameMailboxTest : public LibMailboxSync
{
    Q_OBJECT
private slots:
    void init() override;
    void cleanup() override;
    void initTestCase() override;

    void testRename();
    void testRenameNested();
    void testRenameFail();
    void testRenameListFails();
    void testRenameOffline();

private:
    QSignalSpy *renamedSpy;
    QSignalSpy *failedSpy;

    void initWithMailboxes();
    QStringList topLevelNames() const;
};

#endif
