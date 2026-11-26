/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_SEARCHSNIPPETS_H
#define TEST_SEARCHSNIPPETS_H

#include <QObject>

/** @short Verify the snippets of downloaded text which search results show */
class SearchSnippetsTest : public QObject
{
    Q_OBJECT

private slots:
    void cutsAroundTheFirstMatch();
    void keepsShortTextWhole();
    void marksEveryMatchCaseInsensitively();
    void fallsBackToTheWordsOfAPhrase();
    void mergesOverlappingMatches();
    void startsAtTheTextWithoutAMatch();
    void emptyText();
    void snippetsOfMessages();
};

#endif
