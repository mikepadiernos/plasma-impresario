/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_SEARCHBUILDER_H
#define TEST_SEARCHBUILDER_H

#include <QObject>

/** @short Verify the search builder's conditions and their translation from and to the query syntax */
class SearchBuilderTest : public QObject
{
    Q_OBJECT

private slots:
    void offersOperatorsPerField();
    void writesConditions_data();
    void writesConditions();
    void joinsConditions();
    void opensQueries_data();
    void opensQueries();
    void explainsQueriesWithoutBuilderForm_data();
    void explainsQueriesWithoutBuilderForm();
    void keepsTheMeaningOfOpenedQueries_data();
    void keepsTheMeaningOfOpenedQueries();
};

#endif
