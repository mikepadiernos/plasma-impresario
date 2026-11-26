/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_SEARCHFACETS_H
#define TEST_SEARCHFACETS_H

#include <QObject>

/** @short Verify the values, matching and counts of the facets which narrow search results */
class SearchFacetsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void readsAccountAndFolder();
    void readsSenderDateAndAttachments();
    void groupsDates();
    void readsTagsWithoutSystemFlags();
    void unloadedMessagesHaveOnlyKnownValues();
    void placeholdersHaveNoValues();
    void choosesAndRemovesValues();
    void combinesKinds();
    void countsAmongOtherKinds();
    void ordersCounts();
};

#endif
