/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef TEST_SELECTIONAFTERREMOVAL_H
#define TEST_SELECTIONAFTERREMOVAL_H

#include <QObject>

/** @short Verify which message becomes current when the current one leaves the list */
class SelectionAfterRemovalTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void successor_data();
    void successor();
    void invalidInput();
    void withinRemovedRows();
};

#endif
