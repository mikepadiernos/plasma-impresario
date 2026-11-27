/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_SEARCHQUERY_H
#define TEST_SEARCHQUERY_H

#include <QObject>

/** @short Verify the quick search's query syntax: parsing, errors and the translation to IMAP SEARCH keys */
class SearchQueryTest : public QObject
{
    Q_OBJECT

private slots:
    void treatsBlankTextAsEmpty();
    void normalizes_data();
    void normalizes();
    void translatesToImap_data();
    void translatesToImap();
    void searchesTheChosenFields();
    void needsAFieldForPlainWords();
    void rejectsInvalidQueries_data();
    void rejectsInvalidQueries();
    void tracksRelativeDates();
    void quotesValues_data();
    void quotesValues();
    void namesFields();
};

#endif
