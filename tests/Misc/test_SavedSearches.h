/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_SAVEDSEARCHES_H
#define TEST_SAVEDSEARCHES_H

#include <QObject>
#include <QTemporaryDir>

/** @short Verify how saved searches and the counts of their virtual folders are stored */
class SavedSearchesTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void roundTrip();
    void readsSearchesFromBeforeScopes();
    void validity();
    void dependsOnOwner();
    void loadSortsAcrossOwners();
    void counts();
    void pruneCounts();

private:
    QTemporaryDir m_dir;
};

#endif
