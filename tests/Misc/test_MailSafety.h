/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_MAILSAFETY_H
#define TEST_MAILSAFETY_H

#include <QObject>

/** @short Verify how junk verdicts and attachment scan results are presented */
class MailSafetyTest : public QObject {
  Q_OBJECT

private slots:
  void readsJunkVerdicts_data();
  void readsJunkVerdicts();
  void onlyCleanScansAreReportedClean();
  void decidesWhetherToOpen_data();
  void decidesWhetherToOpen();
};

#endif
