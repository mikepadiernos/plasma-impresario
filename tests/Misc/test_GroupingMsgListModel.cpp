/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_GroupingMsgListModel.h"
#include "Imap/Model/FavoriteTagsModel.h"
#include "Imap/Model/GroupingMsgListModel.h"
#include "Imap/Model/ItemRoles.h"
#include <QAbstractItemModelTester>
#include <QLocale>
#include <QMimeData>
#include <QStandardItemModel>
#include <QUrl>
#include <QtTest>

using Imap::Mailbox::GroupingMsgListModel;
using Grouping = Imap::Mailbox::GroupingMsgListModel::Grouping;
namespace Roles = Imap::Mailbox;

namespace {

/** A Wednesday; the C locale starts its weeks on Monday */
const QDate referenceDate(2026, 11, 25);

struct Row {
  uint uid;
  QDate date;
  QString from;
  QString subject;
  QStringList flags;
  QString list;
  bool fetched = true;
};

QVariant addresses(const QString &address) {
  if (address.isEmpty())
    return QVariantList();
  const QString name = address.section(QLatin1Char('@'), 0, 0);
  QString display = name;
  display[0] = display[0].toUpper();
  return QVariantList{QStringList{display, QString(),
                                  address.section(QLatin1Char('@'), 0, 0),
                                  address.section(QLatin1Char('@'), 1)}};
}

QList<QStandardItem *> message(const Row &row) {
  auto *item = new QStandardItem(row.subject);
  item->setData(row.uid, Roles::RoleMessageUid);
  item->setData(row.fetched, Roles::RoleIsFetched);
  if (row.date.isValid())
    item->setData(QDateTime(row.date, QTime(12, 0)), Roles::RoleMessageDate);
  item->setData(addresses(row.from), Roles::RoleMessageFrom);
  item->setData(row.subject, Roles::RoleMessageSubject);
  item->setData(row.flags, Roles::RoleMessageFlags);
  item->setData(row.flags.contains(QStringLiteral("\\Seen")),
                Roles::RoleMessageIsMarkedRead);
  QVariantList listPost;
  if (!row.list.isEmpty())
    listPost << QUrl(QStringLiteral("mailto:") + row.list);
  item->setData(listPost, Roles::RoleMessageHeaderListPost);
  auto *second = new QStandardItem(QStringLiteral("col1-%1").arg(row.uid));
  second->setData(row.uid, Roles::RoleMessageUid);
  return {item, second};
}

void fill(QStandardItemModel &source, const QList<Row> &rows) {
  source.clear();
  source.setColumnCount(2);
  source.setHorizontalHeaderLabels(
      {QStringLiteral("Subject"), QStringLiteral("Second")});
  for (const Row &row : rows)
    source.appendRow(message(row));
}

QStringList labels(const GroupingMsgListModel &model) {
  QStringList out;
  for (int i = 0; i < model.rowCount(); ++i)
    out << model.groupLabel(model.index(i, 0));
  return out;
}

QList<int> counts(const GroupingMsgListModel &model) {
  QList<int> out;
  for (int i = 0; i < model.rowCount(); ++i)
    out << model.messageCount(model.index(i, 0));
  return out;
}

QList<uint> uidsIn(const GroupingMsgListModel &model, int group) {
  QList<uint> out;
  const QModelIndex header = model.index(group, 0);
  for (int i = 0; i < model.rowCount(header); ++i)
    out << model.index(i, 0, header).data(Roles::RoleMessageUid).toUInt();
  return out;
}

int groupOf(const GroupingMsgListModel &model, const QString &label) {
  return static_cast<int>(labels(model).indexOf(label));
}

QModelIndex sourceByUid(const QStandardItemModel &source, uint uid) {
  for (int row = 0; row < source.rowCount(); ++row) {
    if (source.index(row, 0).data(Roles::RoleMessageUid).toUInt() == uid)
      return source.index(row, 0);
  }
  return QModelIndex();
}

/** Every source row is shown exactly once, in source order within its group
 */
void verifyConsistency(const GroupingMsgListModel &model,
                       const QStandardItemModel &source) {
  int total = 0;
  QSet<int> seen;
  for (int group = 0; group < model.rowCount(); ++group) {
    const QModelIndex header = model.index(group, 0);
    QVERIFY(model.isGroupHeader(header));
    const int children = model.rowCount(header);
    QVERIFY(children > 0);
    QCOMPARE(model.messageCount(header), children);
    int unread = 0;
    int previous = -1;
    for (int i = 0; i < children; ++i) {
      const QModelIndex child = model.index(i, 1, header);
      QCOMPARE(child.parent(), header);
      const QModelIndex mapped = model.mapToSource(child);
      QVERIFY(mapped.isValid());
      QCOMPARE(mapped.column(), 1);
      QVERIFY(mapped.row() > previous);
      previous = mapped.row();
      QCOMPARE(model.mapFromSource(mapped), child);
      QVERIFY(!seen.contains(mapped.row()));
      seen.insert(mapped.row());
      const QModelIndex first = model.index(i, 0, header);
      if (first.data(Roles::RoleMessageUid).toUInt() &&
          !first.data(Roles::RoleMessageIsMarkedRead).toBool())
        ++unread;
    }
    QCOMPARE(model.unreadCount(header), unread);
    total += children;
  }
  QCOMPARE(total, source.rowCount());
}

QList<Row> dateRows() {
  return {
      {1, QDate(2026, 11, 25), QStringLiteral("ann@example.org"), QStringLiteral("a")},
      {2, QDate(2026, 11, 24), QStringLiteral("ann@example.org"), QStringLiteral("b")},
      {3, QDate(2026, 11, 23), QStringLiteral("ann@example.org"), QStringLiteral("c")},
      {4, QDate(2026, 11, 17), QStringLiteral("ann@example.org"), QStringLiteral("d")},
      {5, QDate(2026, 11, 16), QStringLiteral("ann@example.org"), QStringLiteral("e")},
      {6, QDate(2026, 11, 3), QStringLiteral("ann@example.org"), QStringLiteral("f")},
      {7, QDate(2026, 10, 15), QStringLiteral("ann@example.org"), QStringLiteral("g")},
      {8, QDate(2025, 11, 30), QStringLiteral("ann@example.org"), QStringLiteral("h")},
      {9, QDate(2026, 11, 27), QStringLiteral("ann@example.org"), QStringLiteral("i")},
      {10, QDate(), QStringLiteral("ann@example.org"), QStringLiteral("j")},
      {11, QDate(2026, 11, 25), QStringLiteral("ann@example.org"), QStringLiteral("k"), {QStringLiteral("\\Seen")}},
      {12, QDate(2026, 11, 25), QString(), QString(), {}, QString(), false},
  };
}

void prepare(GroupingMsgListModel &model, QStandardItemModel &source, Grouping grouping) {
  model.setReferenceDate(referenceDate);
  model.setGrouping(grouping);
  model.setSourceModel(&source);
}

} // namespace

void GroupingMsgListModelTest::initTestCase() {
  QLocale::setDefault(QLocale::c());
}

void GroupingMsgListModelTest::groupingNames() {
  for (Grouping grouping : {Grouping::None, Grouping::Conversation, Grouping::Date, Grouping::Sender,
                            Grouping::Subject, Grouping::Tag, Grouping::ReadState, Grouping::MailingList}) {
    QCOMPARE(GroupingMsgListModel::groupingFromName(GroupingMsgListModel::groupingName(grouping)), grouping);
  }
  QCOMPARE(GroupingMsgListModel::groupingName(Grouping::ReadState), QStringLiteral("readState"));
  QCOMPARE(GroupingMsgListModel::groupingFromName(QStringLiteral("bogus")), Grouping::None);
  QCOMPARE(GroupingMsgListModel::groupingFromName(QString()), Grouping::None);
  QVERIFY(!GroupingMsgListModel::isRealGrouping(Grouping::None));
  QVERIFY(!GroupingMsgListModel::isRealGrouping(Grouping::Conversation));
  QVERIFY(GroupingMsgListModel::isRealGrouping(Grouping::MailingList));
}

void GroupingMsgListModelTest::groupsByDate() {
  QStandardItemModel source;
  fill(source, dateRows());
  GroupingMsgListModel model;
  model.setGroupOrder(Qt::DescendingOrder);
  prepare(model, source, Grouping::Date);
  const QString loading = QStringLiteral("Loading\u2026");
  QCOMPARE(labels(model),
           (QStringList{QStringLiteral("Future"), QStringLiteral("Today"), QStringLiteral("Yesterday"),
                        QStringLiteral("Earlier This Week"), QStringLiteral("Last Week"),
                        QStringLiteral("Earlier This Month"), QStringLiteral("October 2026"),
                        QStringLiteral("November 2025"), QStringLiteral("No Date"), loading}));
  QCOMPARE(counts(model), (QList<int>{1, 2, 1, 1, 2, 1, 1, 1, 1, 1}));
  QCOMPARE(uidsIn(model, 1), (QList<uint>{1, 11}));
  QCOMPARE(uidsIn(model, 4), (QList<uint>{4, 5}));
  QCOMPARE(model.unreadCount(model.index(1, 0)), 1);
  verifyConsistency(model, source);

  // Ascending puts the oldest groups first; those without a date stay last
  model.sort(-1, Qt::AscendingOrder);
  QCOMPARE(model.groupOrder(), Qt::AscendingOrder);
  QCOMPARE(labels(model),
           (QStringList{QStringLiteral("November 2025"), QStringLiteral("October 2026"),
                        QStringLiteral("Earlier This Month"), QStringLiteral("Last Week"),
                        QStringLiteral("Earlier This Week"), QStringLiteral("Yesterday"), QStringLiteral("Today"),
                        QStringLiteral("Future"), QStringLiteral("No Date"), loading}));

  // On a Monday, yesterday belongs to last week but keeps its own group
  model.setReferenceDate(QDate(2026, 11, 23));
  QCOMPARE(labels(model).mid(0, 4),
           (QStringList{QStringLiteral("November 2025"), QStringLiteral("October 2026"),
                        QStringLiteral("Earlier This Month"), QStringLiteral("Last Week")}));
  QVERIFY(!labels(model).contains(QStringLiteral("Yesterday")));
  QCOMPARE(uidsIn(model, groupOf(model, QStringLiteral("Today"))), (QList<uint>{3}));
  QCOMPARE(uidsIn(model, groupOf(model, QStringLiteral("Future"))), (QList<uint>{1, 2, 9, 11}));
  verifyConsistency(model, source);
}

void GroupingMsgListModelTest::groupsBySender() {
  QStandardItemModel source;
  fill(source, {
                   {1, referenceDate, QStringLiteral("zoe@example.org"), QStringLiteral("a")},
                   {2, referenceDate, QStringLiteral("ann@example.org"), QStringLiteral("b")},
                   {3, referenceDate, QStringLiteral("ZOE@Example.org"), QStringLiteral("c")},
                   {4, referenceDate, QString(), QStringLiteral("d")},
               });
  GroupingMsgListModel model;
  prepare(model, source, Grouping::Sender);
  QCOMPARE(labels(model), (QStringList{QStringLiteral("Ann <ann@example.org>"),
                                       QStringLiteral("Zoe <zoe@example.org>"), QStringLiteral("Unknown Sender")}));
  QCOMPARE(uidsIn(model, 1), (QList<uint>{1, 3}));
  QCOMPARE(model.groupKey(model.index(1, 0)), QStringLiteral("s:zoe@example.org"));
  verifyConsistency(model, source);
}

void GroupingMsgListModelTest::groupsBySubject() {
  QStandardItemModel source;
  fill(source, {
                   {1, referenceDate, QStringLiteral("a@example.org"), QStringLiteral("Re: [dev] Release plan")},
                   {2, referenceDate, QStringLiteral("a@example.org"), QStringLiteral("Lunch")},
                   {3, referenceDate, QStringLiteral("a@example.org"), QStringLiteral("AW: release plan")},
                   {4, referenceDate, QStringLiteral("a@example.org"), QString()},
               });
  GroupingMsgListModel model;
  prepare(model, source, Grouping::Subject);
  QCOMPARE(labels(model),
           (QStringList{QStringLiteral("Lunch"), QStringLiteral("Release plan"), QStringLiteral("(no subject)")}));
  QCOMPARE(uidsIn(model, 1), (QList<uint>{1, 3}));
  verifyConsistency(model, source);
}

void GroupingMsgListModelTest::groupsByTag() {
  Imap::Mailbox::FavoriteTagsModel favorites;
  favorites.appendTag(Imap::Mailbox::ItemFavoriteTagItem(QStringLiteral("work"), QStringLiteral("#ff0000")));
  favorites.appendTag(Imap::Mailbox::ItemFavoriteTagItem(QStringLiteral("home"), QStringLiteral("#00ff00")));
  QStandardItemModel source;
  const QString a = QStringLiteral("a@example.org");
  fill(source, {
                   {1, referenceDate, a, QStringLiteral("a"), {QStringLiteral("\\Seen"), QStringLiteral("home"), QStringLiteral("work")}},
                   {2, referenceDate, a, QStringLiteral("b"), {QStringLiteral("$Forwarded"), QStringLiteral("zeta"), QStringLiteral("Alpha")}},
                   {3, referenceDate, a, QStringLiteral("c"), {QStringLiteral("\\Flagged"), QStringLiteral("$Junk")}},
                   {4, referenceDate, a, QStringLiteral("d"), {QStringLiteral("Home")}},
                   {5, referenceDate, a, QStringLiteral("e"), {QStringLiteral("zeta")}, QString(), false},
               });
  GroupingMsgListModel model;
  model.setFavoriteTags(&favorites);
  prepare(model, source, Grouping::Tag);
  // Flags are known before the envelope, so nothing is loading
  QCOMPARE(labels(model), (QStringList{QStringLiteral("work"), QStringLiteral("Home"), QStringLiteral("Alpha"),
                                       QStringLiteral("zeta"), QStringLiteral("No Tag")}));
  QCOMPARE(uidsIn(model, 0), (QList<uint>{1}));
  QCOMPARE(uidsIn(model, 1), (QList<uint>{4}));
  verifyConsistency(model, source);

  // Without the favourite, the message moves to its next favourite tag
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  favorites.removeTagAt(0);
  QCOMPARE(labels(model), (QStringList{QStringLiteral("home"), QStringLiteral("Alpha"), QStringLiteral("zeta"),
                                       QStringLiteral("No Tag")}));
  QCOMPARE(uidsIn(model, 0), (QList<uint>{1, 4}));
  verifyConsistency(model, source);
}

void GroupingMsgListModelTest::groupsByReadState() {
  QStandardItemModel source;
  const QString a = QStringLiteral("a@example.org");
  fill(source, {
                   {1, referenceDate, a, QStringLiteral("a"), {QStringLiteral("\\Seen")}},
                   {2, referenceDate, a, QStringLiteral("b")},
                   {3, referenceDate, a, QStringLiteral("c")},
               });
  GroupingMsgListModel model;
  prepare(model, source, Grouping::ReadState);
  QCOMPARE(labels(model), (QStringList{QStringLiteral("Unread"), QStringLiteral("Read")}));
  QCOMPARE(model.index(0, 0).data().toString(), QStringLiteral("Unread (2, 2 unread)"));
  QCOMPARE(model.index(1, 0).data().toString(), QStringLiteral("Read (1)"));

  // Reading a message moves it, and the last unread one takes its group along
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  QSignalSpy headerChanges(&model, &QAbstractItemModel::dataChanged);
  source.setData(source.index(1, 0), true, Roles::RoleMessageIsMarkedRead);
  QCOMPARE(counts(model), (QList<int>{1, 2}));
  QVERIFY(!headerChanges.isEmpty());
  QCOMPARE(model.index(1, 0).data().toString(), QStringLiteral("Read (2)"));
  source.setData(source.index(2, 0), true, Roles::RoleMessageIsMarkedRead);
  QCOMPARE(labels(model), (QStringList{QStringLiteral("Read")}));
  QCOMPARE(uidsIn(model, 0), (QList<uint>{1, 2, 3}));
  source.setData(source.index(0, 0), false, Roles::RoleMessageIsMarkedRead);
  QCOMPARE(labels(model), (QStringList{QStringLiteral("Unread"), QStringLiteral("Read")}));
  QCOMPARE(uidsIn(model, 0), (QList<uint>{1}));
  verifyConsistency(model, source);
}

void GroupingMsgListModelTest::groupsByMailingList() {
  QStandardItemModel source;
  const QString a = QStringLiteral("a@example.org");
  fill(source, {
                   {1, referenceDate, a, QStringLiteral("a"), {}, QStringLiteral("dev@lists.example.org")},
                   {2, referenceDate, a, QStringLiteral("b")},
                   {3, referenceDate, a, QStringLiteral("c"), {}, QStringLiteral("Announce@Lists.example.org")},
                   {4, referenceDate, a, QStringLiteral("d"), {}, QStringLiteral("dev@lists.example.org")},
               });
  GroupingMsgListModel model;
  prepare(model, source, Grouping::MailingList);
  QCOMPARE(labels(model), (QStringList{QStringLiteral("announce@lists.example.org"),
                                       QStringLiteral("dev@lists.example.org"), QStringLiteral("Not from a list")}));
  QCOMPARE(uidsIn(model, 1), (QList<uint>{1, 4}));
  verifyConsistency(model, source);
}

void GroupingMsgListModelTest::headerRowsAreNotMessages() {
  QStandardItemModel source;
  fill(source, dateRows());
  GroupingMsgListModel model;
  prepare(model, source, Grouping::Date);
  const QModelIndex header = model.index(groupOf(model, QStringLiteral("Today")), 0);
  QCOMPARE(model.groupLabel(header), QStringLiteral("Today"));
  QVERIFY(model.isGroupHeader(header));
  QVERIFY(header.data(Roles::RoleIsGroupHeader).toBool());
  QCOMPARE(header.data().toString(), QStringLiteral("Today (2, 1 unread)"));
  QCOMPARE(header.data(Qt::ToolTipRole).toString(), QStringLiteral("Today: 2 message(s), 1 unread"));
  for (int role : {int(Roles::RoleMessageUid), int(Roles::RoleIsFetched), int(Roles::RoleMessageSubject),
                   int(Roles::RoleMessageFrom), int(Roles::RoleMessageIsMarkedRead), int(Roles::RoleMessageFlags),
                   int(Qt::DecorationRole), int(Qt::EditRole)}) {
    QVERIFY2(!header.data(role).isValid(), qPrintable(QString::number(role)));
  }
  QVERIFY(!model.index(header.row(), 1).data().isValid());
  QCOMPARE(model.flags(header), Qt::ItemFlags(Qt::ItemIsEnabled));
  QVERIFY(!model.mapToSource(header).isValid());
  QVERIFY(!header.parent().isValid());
  QVERIFY(!model.itemData(header).contains(Roles::RoleMessageUid));
  QVERIFY(!model.mimeData({header}));

  const QModelIndex child = model.index(0, 0, header);
  QVERIFY(!model.isGroupHeader(child));
  QVERIFY(!child.data(Roles::RoleIsGroupHeader).toBool());
  QCOMPARE(child.data(Roles::RoleMessageUid).toUInt(), 1u);
  QVERIFY(model.flags(child) & Qt::ItemIsSelectable);
  QCOMPARE(model.rowCount(child), 0);
  QVERIFY(!model.hasChildren(child));
  QCOMPARE(model.headerData(1, Qt::Horizontal).toString(), QStringLiteral("Second"));
  QVERIFY(!model.headerData(0, Qt::Vertical).isValid());

  QItemSelection selection;
  selection.select(header, model.index(header.row(), 1));
  selection.select(child, model.index(1, 1, header));
  const QItemSelection mapped = model.mapSelectionToSource(selection);
  QCOMPARE(mapped.indexes().size(), 4);
  QCOMPARE(model.mapSelectionFromSource(mapped).indexes().size(), 4);
}

void GroupingMsgListModelTest::insertsAndRemovesRows() {
  QStandardItemModel source;
  const QString a = QStringLiteral("a@example.org");
  const QString b = QStringLiteral("b@example.org");
  fill(source, {
                   {1, referenceDate, a, QStringLiteral("a")},
                   {2, referenceDate, b, QStringLiteral("b")},
                   {3, referenceDate, a, QStringLiteral("c")},
               });
  GroupingMsgListModel model;
  prepare(model, source, Grouping::Sender);
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  const QPersistentModelIndex third = model.index(1, 0, model.index(0, 0));
  QCOMPARE(third.data(Roles::RoleMessageUid).toUInt(), 3u);

  // Into an existing group, before and between its messages
  source.insertRow(0, message({4, referenceDate, a, QStringLiteral("d")}));
  source.insertRow(2, message({5, referenceDate, a, QStringLiteral("e")}));
  QCOMPARE(uidsIn(model, 0), (QList<uint>{4, 1, 5, 3}));
  QCOMPARE(third.data(Roles::RoleMessageUid).toUInt(), 3u);
  // A new group arrives with its message
  source.appendRow(message({6, referenceDate, QStringLiteral("0@example.org"), QStringLiteral("f")}));
  QCOMPARE(labels(model).constFirst(), QStringLiteral("0 <0@example.org>"));
  verifyConsistency(model, source);

  // Several rows at once, across groups
  source.removeRows(1, 3);
  QCOMPARE(uidsIn(model, 1), (QList<uint>{4, 3}));
  QCOMPARE(third.data(Roles::RoleMessageUid).toUInt(), 3u);
  QCOMPARE(labels(model).size(), 2);
  verifyConsistency(model, source);

  source.removeRow(source.rowCount() - 1);
  QCOMPARE(labels(model), (QStringList{QStringLiteral("A <a@example.org>")}));
  source.removeRows(0, source.rowCount());
  QCOMPARE(model.rowCount(), 0);
  QVERIFY(!third.isValid());

  source.appendRow(message({7, referenceDate, b, QStringLiteral("g")}));
  QCOMPARE(labels(model), (QStringList{QStringLiteral("B <b@example.org>")}));
  source.clear();
  QCOMPARE(model.rowCount(), 0);
}

void GroupingMsgListModelTest::movesChangedRows() {
  QStandardItemModel source;
  const QString a = QStringLiteral("a@example.org");
  fill(source, {
                   {1, referenceDate, a, QStringLiteral("a")},
                   {2, referenceDate, a, QStringLiteral("b")},
               });
  GroupingMsgListModel model;
  prepare(model, source, Grouping::Sender);
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  const QPersistentModelIndex second = model.index(1, 1, model.index(0, 0));

  QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
  source.setData(source.index(0, 0), QStringLiteral("Other"), Roles::RoleMessageSubject);
  QCOMPARE(labels(model).size(), 1);
  QVERIFY(!changed.isEmpty());
  QCOMPARE(changed.constFirst().at(0).toModelIndex(), model.index(0, 0, model.index(0, 0)));

  // A new group is made for the message, and the old one goes when empty
  source.setData(source.index(1, 0), addresses(QStringLiteral("zed@example.org")), Roles::RoleMessageFrom);
  QCOMPARE(labels(model), (QStringList{QStringLiteral("A <a@example.org>"), QStringLiteral("Zed <zed@example.org>")}));
  QVERIFY(second.isValid());
  QCOMPARE(second.parent().row(), 1);
  QCOMPARE(second.column(), 1);
  source.setData(source.index(0, 0), addresses(QStringLiteral("zed@example.org")), Roles::RoleMessageFrom);
  QCOMPARE(labels(model), (QStringList{QStringLiteral("Zed <zed@example.org>")}));
  QCOMPARE(uidsIn(model, 0), (QList<uint>{1, 2}));
  QCOMPARE(second.row(), 1);
  verifyConsistency(model, source);

  // A message which is being removed has no UID any more and stays put
  source.setData(source.index(0, 0), 0u, Roles::RoleMessageUid);
  source.setData(source.index(0, 0), addresses(a), Roles::RoleMessageFrom);
  QCOMPARE(labels(model), (QStringList{QStringLiteral("Zed <zed@example.org>")}));
}

void GroupingMsgListModelTest::regroupsManyChangedRows() {
  QStandardItemModel source;
  QList<Row> rows;
  for (uint uid = 1; uid <= 30; ++uid)
    rows << Row{uid, referenceDate, QStringLiteral("a@example.org"), QStringLiteral("s")};
  fill(source, rows);
  GroupingMsgListModel model;
  prepare(model, source, Grouping::ReadState);
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  const QPersistentModelIndex tenth = model.index(9, 0, model.index(0, 0));
  {
    const QSignalBlocker blocker(&source);
    for (int row = 0; row < 25; ++row)
      source.setData(source.index(row, 0), true, Roles::RoleMessageIsMarkedRead);
  }
  QSignalSpy layout(&model, &QAbstractItemModel::layoutChanged);
  emit source.dataChanged(source.index(0, 0), source.index(29, 1));
  QCOMPARE(layout.size(), 1);
  QCOMPARE(labels(model), (QStringList{QStringLiteral("Unread"), QStringLiteral("Read")}));
  QCOMPARE(counts(model), (QList<int>{5, 25}));
  QCOMPARE(tenth.parent().row(), 1);
  QCOMPARE(tenth.data(Roles::RoleMessageUid).toUInt(), 10u);
  verifyConsistency(model, source);
}

void GroupingMsgListModelTest::loadsMissingEnvelopes() {
  QStandardItemModel source;
  fill(source, dateRows());
  GroupingMsgListModel model;
  prepare(model, source, Grouping::Date);
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  const QString loading = QStringLiteral("Loading\u2026");
  QCOMPARE(labels(model).constLast(), loading);
  const QModelIndex row = sourceByUid(source, 12);
  source.setData(row, QDateTime(QDate(2026, 10, 2), QTime(8, 0)), Roles::RoleMessageDate);
  QCOMPARE(labels(model).constLast(), loading);
  source.setData(row, true, Roles::RoleIsFetched);
  QVERIFY(!labels(model).contains(loading));
  QCOMPARE(uidsIn(model, groupOf(model, QStringLiteral("October 2026"))), (QList<uint>{7, 12}));
  verifyConsistency(model, source);

  // Grouping by read state does not need the envelope
  model.setGrouping(Grouping::ReadState);
  QCOMPARE(labels(model), (QStringList{QStringLiteral("Unread"), QStringLiteral("Read")}));
  verifyConsistency(model, source);
}

void GroupingMsgListModelTest::followsSourceLayout() {
  QStandardItemModel source;
  fill(source, dateRows());
  GroupingMsgListModel model;
  prepare(model, source, Grouping::Date);
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  const int today = groupOf(model, QStringLiteral("Today"));
  const QPersistentModelIndex header = model.index(today, 0);
  const QPersistentModelIndex message = model.index(1, 1, header);
  QCOMPARE(message.data(Roles::RoleMessageUid).toUInt(), 11u);

  // Sorting the source changes the order within the groups
  source.setSortRole(Roles::RoleMessageUid);
  model.sort(0, Qt::DescendingOrder);
  QCOMPARE(model.groupOrder(), Qt::DescendingOrder);
  QCOMPARE(labels(model).constFirst(), QStringLiteral("Future"));
  QCOMPARE(model.groupLabel(header), QStringLiteral("Today"));
  QCOMPARE(message.row(), 0);
  QCOMPARE(message.data(Roles::RoleMessageUid).toUInt(), 11u);
  QCOMPARE(uidsIn(model, header.row()), (QList<uint>{11, 1}));
  verifyConsistency(model, source);

  model.setGrouping(Grouping::Sender);
  QVERIFY(!header.isValid());
  QVERIFY(message.isValid());
  QCOMPARE(message.data(Roles::RoleMessageUid).toUInt(), 11u);
  verifyConsistency(model, source);

  model.setSourceModel(nullptr);
  QCOMPARE(model.rowCount(), 0);
  QCOMPARE(model.columnCount(), 0);
}

void GroupingMsgListModelTest::passesModelTester() {
  for (Grouping grouping : {Grouping::Date, Grouping::Sender, Grouping::Subject, Grouping::Tag, Grouping::ReadState,
                            Grouping::MailingList, Grouping::None}) {
    QStandardItemModel source;
    fill(source, dateRows());
    GroupingMsgListModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    prepare(model, source, grouping);
    verifyConsistency(model, source);
    source.insertRow(3, message({20, QDate(2026, 11, 20), QStringLiteral("x@example.org"), QStringLiteral("x"),
                                 {QStringLiteral("work")}, QStringLiteral("l@example.org")}));
    source.setData(source.index(0, 0), true, Roles::RoleMessageIsMarkedRead);
    source.setData(source.index(5, 0), QStringLiteral("Changed"), Roles::RoleMessageSubject);
    source.removeRows(1, 2);
    source.sort(0);
    model.setGroupOrder(Qt::DescendingOrder);
    QTest::qWait(0);
    verifyConsistency(model, source);
  }
}

QTEST_GUILESS_MAIN(GroupingMsgListModelTest)
