/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_RECENTFOLDERS_H
#define TEST_RECENTFOLDERS_H

#include <QObject>

/** @short Verify the list of recently used Move To and Copy To destinations */
class RecentFoldersTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void constructorNormalizes();
    void addMovesToFrontAndCaps();
    void addIgnoresEmptyNames();
    void pruneForgetsOnlyGoneFolders();
    void offeredSkipsGoneAndExcluded();
    void saveAndLoadRoundTrip();
    void saveRemovesEmptyList();
    void settingsAreIndependent();
    void stateFromModel();
    void stateFromEmptyModelIsUnknown();
};

#endif
