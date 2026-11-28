/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_IMAP_RELIABILITY_H
#define TEST_IMAP_RELIABILITY_H

#include "Utils/LibMailboxSync.h"

/** @short Lost connections, server shutdowns and rejected credentials, and what
 * the account keeps meanwhile */
class ImapReliabilityTest : public LibMailboxSync
{
    Q_OBJECT

private slots:
    void init() override;
    void testDisconnectSchedulesABackoff();
    void testDelaysDoubleUpToTheLimit();
    void testReconnectNowConnectsAgain();
    void testReconnectNowRespectsTheUser();
    void testServerByeIsALostConnection();
    void testLoginRejectedWaitsForTheUser();
    void testConnectionStatesCombine();
    void testBrokenConnectionReportsItsEnd();
};

#endif
