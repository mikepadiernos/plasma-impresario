/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_SEARCHRESULTSMODEL_H
#define TEST_SEARCHRESULTSMODEL_H

#include <QObject>

/** @short Verify the flat list of search results which point at messages in folders of one account */
class SearchResultsModelTest : public QObject
{
    Q_OBJECT

private slots:
    void listsAddedMessagesOnce();
    void mapsToTheMessages();
    void presentsMessageListColumns();
    void forwardsChangedData();
    void providesSearchSnippets();
    void dropsRemovedMessages();
    void dropsMessagesOfRemovedFolders();
    void sourceResetAndDestructionEmptyTheList();
    void passesTheModelTester();
};

#endif
