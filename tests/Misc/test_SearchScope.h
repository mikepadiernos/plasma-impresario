/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_SEARCHSCOPE_H
#define TEST_SEARCHSCOPE_H

#include <QObject>

/** @short Verify how the quick search turns fields and date ranges into IMAP SEARCH keys */
class SearchScopeTest : public QObject
{
    Q_OBJECT

private slots:
    void formatsImapDates_data();
    void formatsImapDates();
    void resolvesPresets_data();
    void resolvesPresets();
    void resolvesCustomRanges();
    void buildsDateConditions();
    void buildsTextConditions();
    void combinesTextAndDates();
    void extendsRawQueries();
    void configNamesRoundTrip();
    void placeNamesRoundTrip();
    void folderKeysRoundTrip();
    void searchesTheShownFolders();
    void searchesSelectedFolders();
    void searchesWholeAccounts();
    void searchesInboxFirst();
};

#endif
