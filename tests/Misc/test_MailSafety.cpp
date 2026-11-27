/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_MailSafety.h"
#include "UiUtils/MailSafety.h"
#include <QtTest>

using namespace UiUtils::MailSafety;

Q_DECLARE_METATYPE(UiUtils::MailSafety::JunkVerdict)
Q_DECLARE_METATYPE(UiUtils::MailSafety::ScanResult)
Q_DECLARE_METATYPE(UiUtils::MailSafety::OpenDecision)

void MailSafetyTest::readsJunkVerdicts_data() {
  QTest::addColumn<QStringList>("flags");
  QTest::addColumn<JunkVerdict>("verdict");

  QTest::newRow("no flags") << QStringList() << JunkVerdict::Unclassified;
  QTest::newRow("other flags")
      << QStringList{QStringLiteral("\\Seen"), QStringLiteral("$Forwarded"),
                     QStringLiteral("JunkMail")}
      << JunkVerdict::Unclassified;
  QTest::newRow("$Junk") << QStringList{QStringLiteral("$Junk")}
                         << JunkVerdict::Junk;
  QTest::newRow("other case") << QStringList{QStringLiteral("$junk")}
                              << JunkVerdict::Junk;
  QTest::newRow("older keyword") << QStringList{QStringLiteral("Junk")}
                                 << JunkVerdict::Junk;
  QTest::newRow("$NotJunk") << QStringList{QStringLiteral("$NotJunk")}
                            << JunkVerdict::NotJunk;
  QTest::newRow("NonJunk") << QStringList{QStringLiteral("NonJunk")}
                           << JunkVerdict::NotJunk;
  QTest::newRow("NotJunk") << QStringList{QStringLiteral("notjunk")}
                           << JunkVerdict::NotJunk;
  // The user's override wins, whichever order the server reports
  QTest::newRow("both")
      << QStringList{QStringLiteral("$Junk"), QStringLiteral("$NotJunk")}
      << JunkVerdict::NotJunk;
  QTest::newRow("both reversed")
      << QStringList{QStringLiteral("NonJunk"), QStringLiteral("$Junk")}
      << JunkVerdict::NotJunk;
}

void MailSafetyTest::readsJunkVerdicts() {
  QFETCH(QStringList, flags);
  QFETCH(JunkVerdict, verdict);
  QCOMPARE(junkVerdict(flags), verdict);
}

void MailSafetyTest::onlyCleanScansAreReportedClean() {
  QVERIFY(isReportedClean(ScanResult::Clean));
  for (ScanResult result : {ScanResult::NotScanned, ScanResult::Infected,
                            ScanResult::Failed, ScanResult::Unavailable})
    QVERIFY(!isReportedClean(result));
}

void MailSafetyTest::decidesWhetherToOpen_data() {
  QTest::addColumn<JunkVerdict>("verdict");
  QTest::addColumn<ScanResult>("scan");
  QTest::addColumn<OpenDecision>("decision");

  QTest::newRow("ordinary mail, scanning off")
      << JunkVerdict::Unclassified << ScanResult::NotScanned
      << OpenDecision::Open;
  QTest::newRow("ordinary mail, clean")
      << JunkVerdict::Unclassified << ScanResult::Clean << OpenDecision::Open;
  QTest::newRow("overridden junk")
      << JunkVerdict::NotJunk << ScanResult::NotScanned << OpenDecision::Open;
  QTest::newRow("junk") << JunkVerdict::Junk << ScanResult::NotScanned
                        << OpenDecision::Confirm;
  QTest::newRow("junk, clean")
      << JunkVerdict::Junk << ScanResult::Clean << OpenDecision::Confirm;
  QTest::newRow("scanner unavailable")
      << JunkVerdict::Unclassified << ScanResult::Unavailable
      << OpenDecision::Confirm;
  QTest::newRow("scan failed") << JunkVerdict::NotJunk << ScanResult::Failed
                               << OpenDecision::Confirm;
  QTest::newRow("infected") << JunkVerdict::Unclassified
                            << ScanResult::Infected << OpenDecision::Block;
  // Overriding the junk mark does not override the scanner
  QTest::newRow("overridden junk, infected")
      << JunkVerdict::NotJunk << ScanResult::Infected << OpenDecision::Block;
  QTest::newRow("junk, infected")
      << JunkVerdict::Junk << ScanResult::Infected << OpenDecision::Block;
}

void MailSafetyTest::decidesWhetherToOpen() {
  QFETCH(JunkVerdict, verdict);
  QFETCH(ScanResult, scan);
  QFETCH(OpenDecision, decision);
  QCOMPARE(openDecision(verdict, scan), decision);
}

QTEST_GUILESS_MAIN(MailSafetyTest)
