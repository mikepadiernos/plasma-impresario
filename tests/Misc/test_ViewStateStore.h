/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_VIEWSTATESTORE_H
#define TEST_VIEWSTATESTORE_H

#include <QObject>

/** @short Verify the per-folder arrangement and position of the message list */
class ViewStateStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void fallsBackFromFolderToAccountToGlobal();
    void legacyGroupingIsTheAccountDefault();
    void folderChangesStayInTheFolder();
    void keepsAtMostTheLimitDroppingLeastRecentlyUsed();
    void resetFolderFollowsTheAccountAgain();
    void applyToAllFoldersClearsOverrides();
    void renameMovesFolderAndChildren();
    void pruneForgetsGoneFolders();
    void saveAndLoadRoundTrip();
    void saveKeepsLegacyGrouping();
    void loadIgnoresMalformedEntries();
    void positionsRoundTripAndBound();
    void invalidPositionForgetsFolder();
};

#endif
