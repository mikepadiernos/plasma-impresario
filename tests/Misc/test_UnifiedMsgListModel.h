/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_UNIFIEDMSGLISTMODEL_H
#define TEST_UNIFIEDMSGLISTMODEL_H

#include <QObject>

/** @short Verify the unified inbox: several accounts' messages in one list */
class UnifiedMsgListModelTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void concatenatesAccounts();
  void mapsToTheAccountsOwnMessages();
  void refusesAListShownForAnotherAccount();
  void tellsWhereAMessageComesFrom();
  void renamingAnAccountUpdatesItsLabels();
  void forwardsInsertedAndRemovedMessages();
  void forwardsChangedData();
  void destroyedListRemovesOnlyItsMessages();
  void replacedListRecovers();
  void accountStateKeepsMessages();
  void removingAnAccount();
  void countsMessages();
  void messagesCannotBeDragged();
  void sortsAllAccountsByDate();
  void sortsBySubject();
  void quickFilters();
  void keptVisibleMessageStaysShown();
  void tooltipNamesTheAccount();
  void passesTheModelTester();
};

#endif
