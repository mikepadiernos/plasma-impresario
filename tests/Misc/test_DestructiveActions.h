/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef TEST_DESTRUCTIVEACTIONS_H
#define TEST_DESTRUCTIVEACTIONS_H

#include <QObject>

/** @short Verify the Delete choice and the "Do not ask again" confirmations */
class DestructiveActionsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void chooseDeleteAction_data();
    void chooseDeleteAction();
    void keysAreDistinct();
    void asksByDefault();
    void cancelDoesNotPersist();
    void cancelWithDontAskDoesNotPersist();
    void acceptWithoutDontAskKeepsAsking();
    void acceptWithDontAskPersists();
    void confirmationsAreIndependent();
};

#endif
