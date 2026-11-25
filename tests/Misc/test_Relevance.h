/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_RELEVANCE_H
#define TEST_RELEVANCE_H

#include <QObject>

/** @short Verify the client-side relevance of quick search results */
class RelevanceTest : public QObject {
  Q_OBJECT

private slots:
  void extractsTerms_data();
  void extractsTerms();
  void scoresMessages();
  void ranksMessages();
  void skipsRowsWhichAreNotLoaded();
};

#endif
