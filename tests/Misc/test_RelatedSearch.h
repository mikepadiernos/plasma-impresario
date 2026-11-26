/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_RELATEDSEARCH_H
#define TEST_RELATEDSEARCH_H

#include <QObject>

/** @short Verify the IMAP SEARCH keys and the sorting of the server search for related messages */
class RelatedSearchTest : public QObject
{
    Q_OBJECT

private slots:
    void buildsNothingWithoutData();
    void searchesTheSenderAlone();
    void searchesThreadAndSender();
    void stripsBracketsAndDuplicates();
    void limitsReferences();
    void classifiesConversationAndSender();
    void dropsSubstringMatches();
    void sortsAndLimitsGroups();
    void refusesUnloadedMessages();
};

#endif
