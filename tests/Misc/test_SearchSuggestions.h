/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_SEARCHSUGGESTIONS_H
#define TEST_SEARCHSUGGESTIONS_H

#include <QObject>

/** @short Verify the search history and the suggestions offered below the quick search */
class SearchSuggestionsTest : public QObject
{
    Q_OBJECT

private slots:
    void historyNormalizes();
    void historyAddMovesToFrontAndCaps();
    void profileKey();
    void currentToken_data();
    void currentToken();
    void contactQuery_data();
    void contactQuery();
    void recentSearchesMatchTheWholeText();
    void contactsCompleteTheLastWord();
    void tagsAndFolders();
    void nothingForRawOrQuotedText();
    void limitsEachKind();
    void knownFoldersSkipsUnlistedBranches();
};

#endif
