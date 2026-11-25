/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_Relevance.h"
#include "Imap/Model/ItemRoles.h"
#include "UiUtils/Relevance.h"
#include <QStandardItemModel>
#include <QtTest>

namespace Roles = Imap::Mailbox;

namespace {

QVariantList addresses(const QStringList &list) {
  QVariantList out;
  for (const QString &entry : list) {
    const QString name = entry.section(QLatin1Char('<'), 0, 0).trimmed();
    const QString address = entry.section(QLatin1Char('<'), 1).chopped(1);
    out << QStringList{name, QString(), address.section(QLatin1Char('@'), 0, 0),
                       address.section(QLatin1Char('@'), 1)};
  }
  return out;
}

QStandardItem *message(uint uid, const QString &subject, const QString &from,
                       const QString &to, const QDate &date,
                       bool fetched = true) {
  auto *item = new QStandardItem(subject);
  item->setData(uid, Roles::RoleMessageUid);
  item->setData(fetched, Roles::RoleIsFetched);
  item->setData(subject, Roles::RoleMessageSubject);
  item->setData(addresses({from}), Roles::RoleMessageFrom);
  item->setData(addresses({to}), Roles::RoleMessageTo);
  item->setData(QVariantList(), Roles::RoleMessageCc);
  item->setData(QDateTime(date, QTime(9, 0)), Roles::RoleMessageDate);
  return item;
}

} // namespace

void RelevanceTest::extractsTerms_data() {
  QTest::addColumn<QStringList>("conditions");
  QTest::addColumn<QStringList>("terms");

  QTest::newRow("empty") << QStringList() << QStringList();
  QTest::newRow("quick search")
      << QStringList{QStringLiteral("OR"), QStringLiteral("OR"),
                     QStringLiteral("FUZZY"), QStringLiteral("SUBJECT"),
                     QStringLiteral("Release  Plan"), QStringLiteral("FUZZY"),
                     QStringLiteral("BODY"), QStringLiteral("release plan"),
                     QStringLiteral("FROM"), QStringLiteral("Release Plan")}
      << QStringList{QStringLiteral("release plan")};
  QTest::newRow("field filter")
      << QStringList{QStringLiteral("FROM"), QStringLiteral("Ann@Example.org")}
      << QStringList{QStringLiteral("ann@example.org")};
  QTest::newRow("list header")
      << QStringList{QStringLiteral("HEADER"), QStringLiteral("List-Post"),
                     QStringLiteral("dev@lists.example.org")}
      << QStringList{QStringLiteral("dev@lists.example.org")};
  QTest::newRow("raw")
      << QStringList{QStringLiteral(
             "OR (SUBJECT \"say \\\"hi\\\"\" SINCE 1-Jan-2026) "
             "(from ann) UNSEEN HEADER X-Thing Value TEXT subject")}
      << QStringList{QStringLiteral("say \"hi\""), QStringLiteral("ann"),
                     QStringLiteral("value"), QStringLiteral("subject")};
  QTest::newRow("dangling key")
      << QStringList{QStringLiteral("SUBJECT")} << QStringList();
}

void RelevanceTest::extractsTerms() {
  QFETCH(QStringList, conditions);
  QFETCH(QStringList, terms);
  QCOMPARE(UiUtils::relevanceTerms(conditions), terms);
}

void RelevanceTest::scoresMessages() {
  QStandardItemModel model;
  const QDate date(2026, 11, 20);
  model.appendRow(message(1, QStringLiteral("Release plan for Q1"),
                          QStringLiteral("Ann <ann@example.org>"),
                          QStringLiteral("Team <team@example.org>"), date));
  model.appendRow(message(2, QStringLiteral("Plan the release"),
                          QStringLiteral("Bob <bob@example.org>"),
                          QStringLiteral("Team <team@example.org>"), date));
  model.appendRow(message(3, QStringLiteral("Lunch"),
                          QStringLiteral("Release Bot <bot@example.org>"),
                          QStringLiteral("Team <team@example.org>"), date));
  model.appendRow(message(4, QStringLiteral("Lunch"),
                          QStringLiteral("Bob <bob@example.org>"),
                          QStringLiteral("Release Plan <plan@example.org>"),
                          date));
  model.appendRow(message(5, QStringLiteral("Lunch"),
                          QStringLiteral("Bob <bob@example.org>"),
                          QStringLiteral("Team <team@example.org>"), date));
  const QStringList terms{QStringLiteral("release plan")};
  // The whole phrase in the subject: 4 * (2 words + 1)
  QCOMPARE(UiUtils::relevanceScore(model.index(0, 0), terms), 12);
  // Both words apart
  QCOMPARE(UiUtils::relevanceScore(model.index(1, 0), terms), 8);
  // One word in the sender
  QCOMPARE(UiUtils::relevanceScore(model.index(2, 0), terms), 2);
  // The phrase in a recipient's name
  QCOMPARE(UiUtils::relevanceScore(model.index(3, 0), terms), 3);
  // Matched only through the body, which is not known here
  QCOMPARE(UiUtils::relevanceScore(model.index(4, 0), terms), 0);
  QCOMPARE(UiUtils::relevanceScore(model.index(1, 0),
                                   {QStringLiteral("bob@example.org")}),
           4);
  QCOMPARE(UiUtils::relevanceScore(model.index(0, 0), {}), 0);
}

void RelevanceTest::ranksMessages() {
  QStandardItemModel model;
  model.appendRow(message(1, QStringLiteral("lunch"),
                          QStringLiteral("Ann <ann@example.org>"),
                          QStringLiteral("Team <team@example.org>"),
                          QDate(2026, 11, 1)));
  model.appendRow(message(2, QStringLiteral("Ann's lunch"),
                          QStringLiteral("Bob <bob@example.org>"),
                          QStringLiteral("Team <team@example.org>"),
                          QDate(2026, 11, 2)));
  model.appendRow(message(3, QStringLiteral("Hi"),
                          QStringLiteral("Carol <carol@example.org>"),
                          QStringLiteral("Ann <ann@example.org>"),
                          QDate(2026, 11, 3)));
  const QStringList terms =
      UiUtils::relevanceTerms({QStringLiteral("OR"), QStringLiteral("SUBJECT"),
                               QStringLiteral("ann"), QStringLiteral("FROM"),
                               QStringLiteral("ANN")});
  QCOMPARE(terms, QStringList{QStringLiteral("ann")});
  // Subject before sender before recipients
  QVERIFY(UiUtils::relevanceScore(model.index(1, 0), terms) >
          UiUtils::relevanceScore(model.index(0, 0), terms));
  QVERIFY(UiUtils::relevanceScore(model.index(0, 0), terms) >
          UiUtils::relevanceScore(model.index(2, 0), terms));
  QVERIFY(UiUtils::relevanceScore(model.index(2, 0), terms) > 0);
}

void RelevanceTest::skipsRowsWhichAreNotLoaded() {
  QStandardItemModel model;
  model.appendRow(message(1, QStringLiteral("release"),
                          QStringLiteral("Ann <ann@example.org>"),
                          QStringLiteral("Team <team@example.org>"),
                          QDate(2026, 11, 1), false));
  auto *placeholder = new QStandardItem(QStringLiteral("(missing)"));
  model.appendRow(placeholder);
  auto *noUid = message(0, QStringLiteral("release"),
                        QStringLiteral("Ann <ann@example.org>"),
                        QStringLiteral("Team <team@example.org>"),
                        QDate(2026, 11, 1));
  model.appendRow(noUid);
  const QStringList terms{QStringLiteral("release")};
  QCOMPARE(UiUtils::relevanceScore(model.index(0, 0), terms), -1);
  QCOMPARE(UiUtils::relevanceScore(model.index(1, 0), terms), -2);
  QCOMPARE(UiUtils::relevanceScore(model.index(2, 0), terms), -2);
  QCOMPARE(UiUtils::relevanceScore(QModelIndex(), terms), -2);
}

QTEST_GUILESS_MAIN(RelevanceTest)
