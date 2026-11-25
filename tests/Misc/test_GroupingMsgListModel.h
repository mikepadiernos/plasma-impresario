/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_GROUPINGMSGLISTMODEL_H
#define TEST_GROUPINGMSGLISTMODEL_H

#include <QObject>

/** @short Verify the group headers of the message list */
class GroupingMsgListModelTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void groupingNames();
  void groupsByDate();
  void groupsBySender();
  void groupsBySubject();
  void groupsByTag();
  void groupsByReadState();
  void groupsByMailingList();
  void headerRowsAreNotMessages();
  void insertsAndRemovesRows();
  void movesChangedRows();
  void regroupsManyChangedRows();
  void loadsMissingEnvelopes();
  void followsSourceLayout();
  void passesModelTester();
};

#endif
