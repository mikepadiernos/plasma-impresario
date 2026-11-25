/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_QUICKFILTERS_H
#define TEST_QUICKFILTERS_H

#include <QObject>

/** @short Verify the helpers behind Show Related, Select Similar and the triage
 * navigation */
class QuickFiltersTest : public QObject {
  Q_OBJECT

private slots:
  void searchableSubject_data();
  void searchableSubject();
  void fieldFilterTextRoundTrips_data();
  void fieldFilterTextRoundTrips();
  void parsesFieldFilters();
  void buildsSearchConditions();
  void readsListPostAddress();
  void readsFieldFilterValues();
  void selectsSimilarMessages();
  void findsMatchingMessagesInDisplayOrder();
  void stepsToAdjacentMessages();
};

#endif
