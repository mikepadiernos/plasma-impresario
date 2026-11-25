/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "Utils/LibMailboxSync.h"

/** @short The selected and current messages of the message list stay put while the mailbox changes */
class ImapModelViewStatePreservationTest : public LibMailboxSync
{
    Q_OBJECT
private slots:
    void testSelectionSurvivesUpdates();
private:
    void helperOpenSortableMailbox();
};
