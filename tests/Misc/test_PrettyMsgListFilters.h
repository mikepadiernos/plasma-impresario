/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_PRETTYMSGLISTFILTERS_H
#define TEST_PRETTYMSGLISTFILTERS_H

#include <QObject>

/** @short Verify the quick filters of the message list proxy */
class PrettyMsgListFiltersTest : public QObject {
  Q_OBJECT

private slots:
  void noFilterShowsEverything();
  void unreadFilter();
  void hideReadIsTheUnreadFilter();
  void filtersAreCombined();
  void unknownStateChangesVisibilityOnce();
  void attachmentsFilter();
  void threadsAreShownAsAWhole();
  void keptVisibleMessageStaysShown();
  void keptVisibleMessageKeepsItsThread();
  void keptVisibleMessageIsRevealed();
  void visibleMessageCount();
  void facetsNarrowTheList();
};

#endif
