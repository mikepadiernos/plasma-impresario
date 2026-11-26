/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_QuickFilters.h"
#include "Imap/Model/ItemRoles.h"
#include "UiUtils/QuickFilters.h"
#include <QSet>
#include <QStandardItemModel>
#include <QUrl>
#include <QtTest>

using UiUtils::FieldFilter;
using UiUtils::SimilarMessages;

namespace {

QVariant address(const QString &mailbox,
                 const QString &host = QStringLiteral("example.org")) {
  return QVariantList{
      QStringList{QStringLiteral("Someone"), QString(), mailbox, host}};
}

struct Row {
  uint uid;
  QString from;
  QString subject;
  bool read = false;
  bool flagged = false;
  bool fetched = true;
};

QStandardItem *message(const Row &row) {
  auto *item = new QStandardItem(row.subject);
  item->setData(row.uid, Imap::Mailbox::RoleMessageUid);
  item->setData(row.fetched, Imap::Mailbox::RoleIsFetched);
  item->setData(address(row.from), Imap::Mailbox::RoleMessageFrom);
  item->setData(address(QStringLiteral("team")), Imap::Mailbox::RoleMessageTo);
  item->setData(row.subject, Imap::Mailbox::RoleMessageSubject);
  item->setData(row.read, Imap::Mailbox::RoleMessageIsMarkedRead);
  item->setData(row.flagged, Imap::Mailbox::RoleMessageIsMarkedFlagged);
  return item;
}

/** [1 [2, 3]], 4, 5, [missing parent [6]] */
void fillThreads(QStandardItemModel &model) {
  QStandardItem *first =
      message({1, QStringLiteral("ann"), QStringLiteral("Release plan")});
  first->appendRow(
      message({2, QStringLiteral("bob"),
               QStringLiteral("Re: [dev] Release plan"), true, true}));
  first->appendRow(message({3, QStringLiteral("ann"), QStringLiteral("Lunch"),
                            false, false, false}));
  model.appendRow(first);
  model.appendRow(message(
      {4, QStringLiteral("Ann"), QStringLiteral("AW: release  PLAN"), true}));
  model.appendRow(message({5, QStringLiteral("carol"),
                           QStringLiteral("[dev] Other"), false, true}));
  // Threading shows a placeholder without a UID for a parent which is not in
  // the folder
  auto *placeholder = new QStandardItem(QStringLiteral("(missing)"));
  placeholder->appendRow(message(
      {6, QStringLiteral("dave"), QStringLiteral("Fwd: Release plan"), true}));
  model.appendRow(placeholder);
}

QList<uint> uids(const QModelIndexList &indexes) {
  QList<uint> out;
  for (const QModelIndex &index : indexes)
    out << index.data(Imap::Mailbox::RoleMessageUid).toUInt();
  return out;
}

uint uidOf(const QModelIndex &index) {
  return index.isValid() ? index.data(Imap::Mailbox::RoleMessageUid).toUInt()
                         : 0;
}

QModelIndex byUid(const QStandardItemModel &model, uint uid) {
  const auto found =
      UiUtils::findMatchingMessages(&model, [uid](const QModelIndex &index) {
        return index.data(Imap::Mailbox::RoleMessageUid).toUInt() == uid;
      });
  return found.value(0);
}

} // namespace

void QuickFiltersTest::searchableSubject_data() {
  QTest::addColumn<QString>("subject");
  QTest::addColumn<QString>("expected");
  QTest::newRow("plain") << QStringLiteral("Release plan")
                         << QStringLiteral("Release plan");
  QTest::newRow("prefixes") << QStringLiteral("Re: Fwd: AW: SV: Release plan")
                            << QStringLiteral("Release plan");
  QTest::newRow("counted") << QStringLiteral("RE[3]: Release plan")
                           << QStringLiteral("Release plan");
  QTest::newRow("tag") << QStringLiteral("[dev] Release plan")
                       << QStringLiteral("Release plan");
  QTest::newRow("tag and prefixes mixed")
      << QStringLiteral("Re: [dev] Fwd: [Team-List] WG: Release   plan")
      << QStringLiteral("Release plan");
  QTest::newRow("keeps case")
      << QStringLiteral("re: MiXeD Case") << QStringLiteral("MiXeD Case");
  QTest::newRow("inner brackets stay") << QStringLiteral("Re: Fix [urgent] bug")
                                       << QStringLiteral("Fix [urgent] bug");
  QTest::newRow("word starting like a prefix")
      << QStringLiteral("Report: Q3") << QStringLiteral("Report: Q3");
  QTest::newRow("only a tag")
      << QStringLiteral("Re: [ANN]") << QStringLiteral("[ANN]");
  QTest::newRow("only prefixes") << QStringLiteral("Re: Fwd: ") << QString();
  QTest::newRow("empty") << QString() << QString();
}

void QuickFiltersTest::searchableSubject() {
  QFETCH(QString, subject);
  QFETCH(QString, expected);
  QCOMPARE(UiUtils::searchableSubject(subject), expected);
}

void QuickFiltersTest::fieldFilterTextRoundTrips_data() {
  QTest::addColumn<int>("field");
  QTest::addColumn<QString>("value");
  QTest::addColumn<QString>("text");
  QTest::newRow("from") << int(FieldFilter::From)
                        << QStringLiteral("ann@example.org")
                        << QStringLiteral("from:ann@example.org");
  QTest::newRow("to") << int(FieldFilter::To)
                      << QStringLiteral("team@example.org")
                      << QStringLiteral("to:team@example.org");
  QTest::newRow("subject with spaces")
      << int(FieldFilter::Subject) << QStringLiteral("Release plan")
      << QStringLiteral("subject:\"Release plan\"");
  QTest::newRow("quoted subject")
      << int(FieldFilter::Subject) << QStringLiteral("\"Hello\"")
      << QStringLiteral("subject:\"\\\"Hello\\\"\"");
  QTest::newRow("subject with a colon")
      << int(FieldFilter::Subject) << QStringLiteral("Agenda: Monday")
      << QStringLiteral("subject:\"Agenda: Monday\"");
  QTest::newRow("leading space")
      << int(FieldFilter::Subject) << QStringLiteral(" padded")
      << QStringLiteral("subject:\" padded\"");
  QTest::newRow("list") << int(FieldFilter::MailingList)
                        << QStringLiteral("dev@lists.example.org")
                        << QStringLiteral("list:dev@lists.example.org");
}

void QuickFiltersTest::fieldFilterTextRoundTrips() {
  QFETCH(int, field);
  QFETCH(QString, value);
  QFETCH(QString, text);
  QCOMPARE(UiUtils::fieldFilterText(FieldFilter(field), value), text);
  const auto parsed = UiUtils::parseFieldFilter(text);
  QCOMPARE(int(parsed.field), field);
  QCOMPARE(parsed.value, value);
}

void QuickFiltersTest::parsesFieldFilters() {
  auto parsed =
      UiUtils::parseFieldFilter(QStringLiteral("  FROM:ann@example.org "));
  QCOMPARE(parsed.field, FieldFilter::From);
  QCOMPARE(parsed.value, QStringLiteral("ann@example.org"));
  parsed =
      UiUtils::parseFieldFilter(QStringLiteral("Subject:\"Release plan\""));
  QCOMPARE(parsed.field, FieldFilter::Subject);
  QCOMPARE(parsed.value, QStringLiteral("Release plan"));

  // Not a field filter: plain text, other fields, a word which only starts
  // with a keyword, no value, more than one term or an invalid query
  for (const QString &text :
       {QStringLiteral("invoice"), QStringLiteral("cc:ann"),
        QStringLiteral("today: meeting"), QStringLiteral("from:"),
        QStringLiteral("FROM:  ann@example.org"),
        QStringLiteral("subject: \"\""), QStringLiteral("re: plans"),
        QStringLiteral("from:ann subject:plans"),
        QStringLiteral("from:ann OR from:bob"), QStringLiteral("-from:ann"),
        QStringLiteral("subject:Release plan"), QStringLiteral("(from:ann"),
        QStringLiteral(":=FROM ann"), QString()}) {
    QCOMPARE(UiUtils::parseFieldFilter(text).field, FieldFilter::None);
    QVERIFY(UiUtils::fieldFilterConditions(UiUtils::parseFieldFilter(text))
                .isEmpty());
  }
  QCOMPARE(UiUtils::fieldFilterText(FieldFilter::From, QString()), QString());
  QCOMPARE(UiUtils::fieldFilterText(FieldFilter::None, QStringLiteral("text")),
           QStringLiteral("text"));
}

void QuickFiltersTest::buildsSearchConditions() {
  using UiUtils::fieldFilterConditions;
  QCOMPARE(
      fieldFilterConditions(
          {FieldFilter::From, QStringLiteral("ann@example.org")}),
      QStringList({QStringLiteral("FROM"), QStringLiteral("ann@example.org")}));
  QCOMPARE(
      fieldFilterConditions(
          {FieldFilter::To, QStringLiteral("team@example.org")}),
      QStringList({QStringLiteral("TO"), QStringLiteral("team@example.org")}));
  QCOMPARE(
      fieldFilterConditions(
          {FieldFilter::Subject, QStringLiteral("Release plan")}),
      QStringList({QStringLiteral("SUBJECT"), QStringLiteral("Release plan")}));
  QCOMPARE(fieldFilterConditions({FieldFilter::MailingList,
                                  QStringLiteral("dev@lists.example.org")}),
           QStringList({QStringLiteral("HEADER"), QStringLiteral("List-Post"),
                        QStringLiteral("dev@lists.example.org")}));
  QVERIFY(fieldFilterConditions({FieldFilter::None, QStringLiteral("x")})
              .isEmpty());
}

void QuickFiltersTest::readsListPostAddress() {
  using UiUtils::listPostAddress;
  QCOMPARE(
      listPostAddress(QVariantList{
          QUrl(QStringLiteral("https://lists.example.org/post")),
          QUrl(QStringLiteral("mailto:Dev@Lists.Example.ORG?subject=help"))}),
      QStringLiteral("dev@lists.example.org"));
  QVERIFY(listPostAddress(QVariant()).isEmpty());
  QVERIFY(listPostAddress(QVariantList{}).isEmpty());
  QVERIFY(listPostAddress(QVariantList{QUrl(QStringLiteral(
                              "https://lists.example.org/post"))})
              .isEmpty());
}

void QuickFiltersTest::readsFieldFilterValues() {
  QStandardItemModel model;
  QStandardItem *item = message(
      {7, QStringLiteral("Ann"), QStringLiteral("Re: [dev] Release plan")});
  model.appendRow(item);
  const QModelIndex index = item->index();
  using UiUtils::fieldFilterValue;
  QCOMPARE(fieldFilterValue(index, FieldFilter::From),
           QStringLiteral("ann@example.org"));
  QCOMPARE(fieldFilterValue(index, FieldFilter::To),
           QStringLiteral("team@example.org"));
  QCOMPARE(fieldFilterValue(index, FieldFilter::Subject),
           QStringLiteral("Release plan"));
  // Without a List-Post header there is no mailing list to show
  QVERIFY(fieldFilterValue(index, FieldFilter::MailingList).isEmpty());
  item->setData(
      QVariantList{QUrl(QStringLiteral("mailto:dev@lists.example.org"))},
      Imap::Mailbox::RoleMessageHeaderListPost);
  QCOMPARE(fieldFilterValue(index, FieldFilter::MailingList),
           QStringLiteral("dev@lists.example.org"));
  QVERIFY(fieldFilterValue(index, FieldFilter::None).isEmpty());

  // Nothing is read from a message which is not fetched yet
  item->setData(false, Imap::Mailbox::RoleIsFetched);
  QVERIFY(fieldFilterValue(index, FieldFilter::From).isEmpty());
  QVERIFY(fieldFilterValue(QModelIndex(), FieldFilter::From).isEmpty());
}

void QuickFiltersTest::selectsSimilarMessages() {
  QStandardItemModel model;
  fillThreads(model);
  const QModelIndex first = byUid(model, 1);
  QVERIFY(first.isValid());

  // The unfetched message 3 from the same sender is not considered; addresses
  // ignore case
  QCOMPARE(uids(UiUtils::findSimilarMessages(&model, first,
                                             SimilarMessages::FromSender)),
           QList<uint>({1, 4}));
  // Prefixes, list tags, case and spacing do not matter; the placeholder parent
  // is skipped
  QCOMPARE(uids(UiUtils::findSimilarMessages(&model, first,
                                             SimilarMessages::WithSubject)),
           QList<uint>({1, 2, 4, 6}));
  // Flags are known for every row, fetched or not
  QCOMPARE(uids(UiUtils::findSimilarMessages(&model, QModelIndex(),
                                             SimilarMessages::Unread)),
           QList<uint>({1, 3, 5}));
  QCOMPARE(uids(UiUtils::findSimilarMessages(&model, QModelIndex(),
                                             SimilarMessages::Flagged)),
           QList<uint>({2, 5}));

  // Sender and subject need a fetched reference message
  const QModelIndex unfetched = byUid(model, 3);
  QVERIFY(UiUtils::findSimilarMessages(&model, unfetched,
                                       SimilarMessages::FromSender)
              .isEmpty());
  QVERIFY(UiUtils::findSimilarMessages(&model, unfetched,
                                       SimilarMessages::WithSubject)
              .isEmpty());
  QVERIFY(UiUtils::findSimilarMessages(&model, QModelIndex(),
                                       SimilarMessages::FromSender)
              .isEmpty());
  QVERIFY(UiUtils::findSimilarMessages(nullptr, first, SimilarMessages::Unread)
              .isEmpty());
}

void QuickFiltersTest::findsMatchingMessagesInDisplayOrder() {
  QStandardItemModel model;
  fillThreads(model);
  QCOMPARE(uids(UiUtils::findMatchingMessages(
               &model, [](const QModelIndex &) { return true; })),
           QList<uint>({1, 2, 3, 4, 5, 6}));
  QVERIFY(UiUtils::findMatchingMessages(&model, [](const QModelIndex &) {
            return false;
          }).isEmpty());
}

void QuickFiltersTest::stepsToAdjacentMessages() {
  QStandardItemModel model;
  fillThreads(model);
  const auto expanded = [](const QModelIndex &) { return true; };
  const auto collapsed = [](const QModelIndex &) { return false; };
  auto next =
      [&model](uint uid,
               const std::function<bool(const QModelIndex &)> &isExpanded) {
        return uidOf(UiUtils::adjacentMessage(
            &model, uid ? byUid(model, uid) : QModelIndex(), true, isExpanded));
      };
  auto previous =
      [&model](uint uid,
               const std::function<bool(const QModelIndex &)> &isExpanded) {
        return uidOf(UiUtils::adjacentMessage(
            &model, uid ? byUid(model, uid) : QModelIndex(), false,
            isExpanded));
      };

  // Expanded threads: 1, 2, 3, 4, 5, (placeholder), 6
  QCOMPARE(next(0, expanded), 1u);
  QCOMPARE(next(1, expanded), 2u);
  QCOMPARE(next(3, expanded), 4u);
  QCOMPARE(next(5, expanded), 6u);
  QCOMPARE(next(6, expanded), 0u);
  QCOMPARE(previous(0, expanded), 6u);
  QCOMPARE(previous(6, expanded), 5u);
  QCOMPARE(previous(4, expanded), 3u);
  QCOMPARE(previous(2, expanded), 1u);
  QCOMPARE(previous(1, expanded), 0u);

  // Collapsed threads only show their roots, and a collapsed placeholder has no
  // message to stop at
  QCOMPARE(next(1, collapsed), 4u);
  QCOMPARE(previous(4, collapsed), 1u);
  QCOMPARE(next(5, collapsed), 0u);
  QCOMPARE(previous(0, collapsed), 5u);

  // A message inside a collapsed thread continues after its thread
  QCOMPARE(next(2, collapsed), 3u);
  QCOMPARE(next(3, collapsed), 4u);

  // Another column of the current row works as well
  QCOMPARE(uidOf(UiUtils::adjacentMessage(
               &model, byUid(model, 4).siblingAtColumn(0), true, expanded)),
           5u);
  QStandardItemModel empty;
  QVERIFY(!UiUtils::adjacentMessage(&empty, QModelIndex(), true, expanded)
               .isValid());
  QVERIFY(!UiUtils::adjacentMessage(nullptr, QModelIndex(), false, expanded)
               .isValid());
}

QTEST_GUILESS_MAIN(QuickFiltersTest)
