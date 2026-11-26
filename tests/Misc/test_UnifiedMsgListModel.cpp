/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QAbstractItemModelTester>
#include <QDateTime>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTimeZone>
#include <QtTest>
#include <memory>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/UnifiedMsgListModel.h"
#include "UiUtils/SearchFacets.h"
#include "test_UnifiedMsgListModel.h"

using Imap::Mailbox::MsgListModel;
using Imap::Mailbox::PrettyMsgListModel;
using Imap::Mailbox::PrettyUnifiedMsgListModel;
using Imap::Mailbox::UnifiedMsgListModel;

namespace {

struct Message {
  QString subject;
  QDateTime date;
  bool read = false;
  bool flagged = false;
  QString mailbox = QStringLiteral("INBOX");
};

QDateTime at(const int hour) {
  return QDateTime(QDate(2026, 11, 25), QTime(hour, 0), QTimeZone::UTC);
}

/** @short A message row as MsgListModel presents it, with the same data in
 * every column */
QList<QStandardItem *> row(const Message &message) {
  using namespace Imap::Mailbox;
  QList<QStandardItem *> items;
  for (int column = 0; column < MsgListModel::COLUMN_COUNT; ++column) {
    auto *item = new QStandardItem(message.subject);
    item->setData(true, RoleIsFetched);
    item->setData(1u, RoleMessageUid);
    item->setData(message.subject, RoleMessageSubject);
    item->setData(message.date, RoleMessageDate);
    item->setData(message.date, RoleMessageInternalDate);
    item->setData(message.read, RoleMessageIsMarkedRead);
    item->setData(!message.read, RoleMessageWasUnread);
    item->setData(message.flagged, RoleMessageIsMarkedFlagged);
    item->setData(false, RoleMessageHasAttachments);
    item->setData(message.mailbox, RoleMailboxName);
    items << item;
  }
  return items;
}

std::unique_ptr<QStandardItemModel> inbox(const QList<Message> &messages) {
  auto model =
      std::make_unique<QStandardItemModel>(0, MsgListModel::COLUMN_COUNT);
  for (const Message &message : messages)
    model->appendRow(row(message));
  return model;
}

QStringList subjects(const QAbstractItemModel &model) {
  QStringList names;
  for (int i = 0; i < model.rowCount(); ++i)
    names << model.index(i, MsgListModel::SUBJECT)
                 .data(Imap::Mailbox::RoleMessageSubject)
                 .toString();
  return names;
}

void setRead(QStandardItemModel &model, const int message, const bool read) {
  for (int column = 0; column < model.columnCount(); ++column) {
    QStandardItem *item = model.item(message, column);
    item->setData(read, Imap::Mailbox::RoleMessageIsMarkedRead);
    item->setData(!read, Imap::Mailbox::RoleMessageWasUnread);
  }
}

} // namespace

void UnifiedMsgListModelTest::initTestCase() {
  QStandardPaths::setTestModeEnabled(true);
}

void UnifiedMsgListModelTest::concatenatesAccounts() {
  auto work =
      inbox({{QStringLiteral("w1"), at(1)}, {QStringLiteral("w2"), at(2)}});
  auto home = inbox({{QStringLiteral("h1"), at(3)}});
  UnifiedMsgListModel unified;
  QVERIFY(unified.setAccountSource(QStringLiteral("work"),
                                   QStringLiteral("Work"), work.get()));
  QVERIFY(unified.setAccountSource(QStringLiteral("home"),
                                   QStringLiteral("Home"), home.get()));

  QCOMPARE(unified.rowCount(), 3);
  QCOMPARE(unified.columnCount(), int(MsgListModel::COLUMN_COUNT));
  QCOMPARE(subjects(unified),
           (QStringList{QStringLiteral("w1"), QStringLiteral("w2"),
                        QStringLiteral("h1")}));
  QCOMPARE(unified.accountIds(),
           (QStringList{QStringLiteral("work"), QStringLiteral("home")}));
  QVERIFY(!unified.hasChildren(unified.index(0, 0)));
}

void UnifiedMsgListModelTest::mapsToTheAccountsOwnMessages() {
  auto work =
      inbox({{QStringLiteral("w1"), at(1)}, {QStringLiteral("w2"), at(2)}});
  auto home = inbox({{QStringLiteral("h1"), at(3)}});
  UnifiedMsgListModel unified;
  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("Work"),
                           work.get());
  unified.setAccountSource(QStringLiteral("home"), QStringLiteral("Home"),
                           home.get());

  const QModelIndex h1 =
      unified.mapToSource(unified.index(2, MsgListModel::DATE));
  QCOMPARE(h1.model(), home.get());
  QCOMPARE(h1.row(), 0);
  QCOMPARE(h1.column(), int(MsgListModel::DATE));
  QCOMPARE(unified.mapFromSource(home->index(0, MsgListModel::DATE)),
           unified.index(2, MsgListModel::DATE));
  QCOMPARE(unified.mapFromSource(work->index(1, 0)), unified.index(1, 0));
  QCOMPARE(unified.accountIdOfSource(home.get()), QStringLiteral("home"));
  QCOMPARE(unified.accountSource(QStringLiteral("work")), work.get());

  QStandardItemModel stranger;
  stranger.appendRow(new QStandardItem());
  QVERIFY(!unified.mapFromSource(stranger.index(0, 0)).isValid());
  QVERIFY(unified.accountIdOfSource(&stranger).isEmpty());
}

void UnifiedMsgListModelTest::refusesAListShownForAnotherAccount() {
  auto shared = inbox({{QStringLiteral("m"), at(1)}});
  UnifiedMsgListModel unified;
  QVERIFY(unified.setAccountSource(QStringLiteral("a"), QStringLiteral("A"),
                                   shared.get()));
  QVERIFY(!unified.setAccountSource(QStringLiteral("b"), QStringLiteral("B"),
                                    shared.get()));
  // Showing it again for the same account changes nothing
  QVERIFY(unified.setAccountSource(QStringLiteral("a"), QStringLiteral("A"),
                                   shared.get()));
  QCOMPARE(unified.rowCount(), 1);
  QCOMPARE(unified.accountIds(), QStringList{QStringLiteral("a")});
}

void UnifiedMsgListModelTest::tellsWhereAMessageComesFrom() {
  using namespace Imap::Mailbox;
  Message archived{QStringLiteral("old"), at(1)};
  archived.mailbox = QStringLiteral("Archive");
  Message nowhere{QStringLiteral("?"), at(2)};
  nowhere.mailbox = QString();
  auto work = inbox({{QStringLiteral("new"), at(3)}, archived, nowhere});
  UnifiedMsgListModel unified;
  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("Work"),
                           work.get());

  const QModelIndex first = unified.index(0, 0);
  QCOMPARE(first.data(RoleMessageAccountId).toString(), QStringLiteral("work"));
  QCOMPARE(first.data(RoleMessageAccountName).toString(),
           QStringLiteral("Work"));
  QCOMPARE(first.data(RoleMessageSourceLabel).toString(),
           QStringLiteral("Work \u00B7 Inbox"));
  QCOMPARE(unified.index(1, 0).data(RoleMessageSourceLabel).toString(),
           QStringLiteral("Work \u00B7 Archive"));
  QCOMPARE(unified.index(2, 0).data(RoleMessageSourceLabel).toString(),
           QStringLiteral("Work"));
  // Other roles come from the account's own message
  QCOMPARE(first.data(RoleMessageSubject).toString(), QStringLiteral("new"));
  QVERIFY(unified.roleNames().contains(RoleMessageSourceLabel));
}

void UnifiedMsgListModelTest::rowLabelNamesFoldersOfSearchResults() {
  using namespace Imap::Mailbox;
  Message archived{QStringLiteral("old"), at(1)};
  archived.mailbox = QStringLiteral("Archive");
  auto work = inbox({{QStringLiteral("new"), at(2)}, archived});
  UnifiedMsgListModel unified;
  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("Work"),
                           work.get());

  // The Unified Inbox names only the account
  QVERIFY(!unified.showsFolders());
  QCOMPARE(unified.index(1, 0).data(RoleMessageRowLabel).toString(),
           QStringLiteral("Work"));

  QSignalSpy changed(&unified, &QAbstractItemModel::dataChanged);
  unified.setShowsFolders(true);
  QCOMPARE(changed.count(), 1);
  QCOMPARE(changed.at(0).at(2).value<QList<int>>(),
           QList<int>{RoleMessageRowLabel});
  QCOMPARE(unified.index(0, 0).data(RoleMessageRowLabel).toString(),
           QStringLiteral("Work \u00B7 Inbox"));
  QCOMPARE(unified.index(1, 0).data(RoleMessageRowLabel).toString(),
           QStringLiteral("Work \u00B7 Archive"));
  unified.setShowsFolders(true);
  QCOMPARE(changed.count(), 1);
  QVERIFY(unified.roleNames().contains(RoleMessageRowLabel));
}

void UnifiedMsgListModelTest::renamingAnAccountUpdatesItsLabels() {
  using namespace Imap::Mailbox;
  auto work = inbox({{QStringLiteral("w1"), at(1)}});
  auto home =
      inbox({{QStringLiteral("h1"), at(2)}, {QStringLiteral("h2"), at(3)}});
  UnifiedMsgListModel unified;
  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("Work"),
                           work.get());
  unified.setAccountSource(QStringLiteral("home"), QStringLiteral("Home"),
                           home.get());
  QSignalSpy changed(&unified, &QAbstractItemModel::dataChanged);
  QSignalSpy accounts(&unified, &UnifiedMsgListModel::accountsChanged);

  unified.setAccountName(QStringLiteral("home"), QStringLiteral("Family"));
  QCOMPARE(changed.count(), 1);
  QCOMPARE(changed.at(0).at(0).toModelIndex().row(), 1);
  QCOMPARE(changed.at(0).at(1).toModelIndex().row(), 2);
  QCOMPARE(accounts.count(), 1);
  QCOMPARE(unified.index(1, 0).data(RoleMessageSourceLabel).toString(),
           QStringLiteral("Family \u00B7 Inbox"));

  unified.setAccountName(QStringLiteral("home"), QStringLiteral("Family"));
  QCOMPARE(changed.count(), 1);
}

void UnifiedMsgListModelTest::forwardsInsertedAndRemovedMessages() {
  auto work = inbox({{QStringLiteral("w1"), at(1)}});
  auto home = inbox({{QStringLiteral("h1"), at(2)}});
  UnifiedMsgListModel unified;
  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("Work"),
                           work.get());
  unified.setAccountSource(QStringLiteral("home"), QStringLiteral("Home"),
                           home.get());
  QSignalSpy inserted(&unified, &QAbstractItemModel::rowsInserted);
  QSignalSpy removed(&unified, &QAbstractItemModel::rowsRemoved);

  work->appendRow(row({QStringLiteral("w2"), at(3)}));
  QCOMPARE(inserted.count(), 1);
  QCOMPARE(inserted.at(0).at(1).toInt(), 1);
  QCOMPARE(inserted.at(0).at(2).toInt(), 1);
  QCOMPARE(subjects(unified),
           (QStringList{QStringLiteral("w1"), QStringLiteral("w2"),
                        QStringLiteral("h1")}));

  home->appendRow(row({QStringLiteral("h2"), at(4)}));
  QCOMPARE(inserted.at(1).at(1).toInt(), 3);

  work->removeRow(0);
  QCOMPARE(removed.count(), 1);
  QCOMPARE(removed.at(0).at(1).toInt(), 0);
  QCOMPARE(subjects(unified),
           (QStringList{QStringLiteral("w2"), QStringLiteral("h1"),
                        QStringLiteral("h2")}));
}

void UnifiedMsgListModelTest::forwardsChangedData() {
  auto work = inbox({{QStringLiteral("w1"), at(1)}});
  auto home = inbox({{QStringLiteral("h1"), at(2)}});
  UnifiedMsgListModel unified;
  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("Work"),
                           work.get());
  unified.setAccountSource(QStringLiteral("home"), QStringLiteral("Home"),
                           home.get());
  QSignalSpy changed(&unified, &QAbstractItemModel::dataChanged);

  home->item(0, MsgListModel::SEEN)
      ->setData(true, Imap::Mailbox::RoleMessageIsMarkedRead);
  QCOMPARE(changed.count(), 1);
  QCOMPARE(changed.at(0).at(0).toModelIndex(),
           unified.index(1, MsgListModel::SEEN));
  QVERIFY(unified.index(1, MsgListModel::SEEN)
              .data(Imap::Mailbox::RoleMessageIsMarkedRead)
              .toBool());
}

void UnifiedMsgListModelTest::destroyedListRemovesOnlyItsMessages() {
  auto work = inbox({{QStringLiteral("w1"), at(1)}});
  auto *home =
      inbox({{QStringLiteral("h1"), at(2)}, {QStringLiteral("h2"), at(3)}})
          .release();
  auto later = inbox({{QStringLiteral("l1"), at(4)}});
  UnifiedMsgListModel unified;
  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("Work"),
                           work.get());
  unified.setAccountSource(QStringLiteral("home"), QStringLiteral("Home"),
                           home);
  unified.setAccountSource(QStringLiteral("later"), QStringLiteral("Later"),
                           later.get());
  QSignalSpy removed(&unified, &QAbstractItemModel::rowsRemoved);

  delete home;
  QCOMPARE(removed.count(), 1);
  QCOMPARE(removed.at(0).at(1).toInt(), 1);
  QCOMPARE(removed.at(0).at(2).toInt(), 2);
  QCOMPARE(subjects(unified),
           (QStringList{QStringLiteral("w1"), QStringLiteral("l1")}));
  // The account still takes part, without messages
  QVERIFY(unified.accountIds().contains(QStringLiteral("home")));
  QVERIFY(!unified.accountSource(QStringLiteral("home")));
  QCOMPARE(unified.messageCount(QStringLiteral("home")), 0);
}

void UnifiedMsgListModelTest::replacedListRecovers() {
  auto work = inbox({{QStringLiteral("w1"), at(1)}});
  auto *old = inbox({{QStringLiteral("h1"), at(2)}}).release();
  UnifiedMsgListModel unified;
  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("Work"),
                           work.get());
  unified.setAccountSource(QStringLiteral("home"), QStringLiteral("Home"), old);
  delete old;

  // Reconnecting creates the account's models again
  auto fresh =
      inbox({{QStringLiteral("h1"), at(2)}, {QStringLiteral("h2"), at(3)}});
  QVERIFY(unified.setAccountSource(QStringLiteral("home"),
                                   QStringLiteral("Home"), fresh.get()));
  QCOMPARE(subjects(unified),
           (QStringList{QStringLiteral("w1"), QStringLiteral("h1"),
                        QStringLiteral("h2")}));
  QCOMPARE(unified.accountIds(),
           (QStringList{QStringLiteral("work"), QStringLiteral("home")}));

  // Replacing a live list drops the rows of the previous one
  auto other = inbox({{QStringLiteral("w9"), at(9)}});
  QVERIFY(unified.setAccountSource(QStringLiteral("work"),
                                   QStringLiteral("Work"), other.get()));
  QCOMPARE(subjects(unified),
           (QStringList{QStringLiteral("w9"), QStringLiteral("h1"),
                        QStringLiteral("h2")}));
  work->appendRow(row({QStringLiteral("ignored"), at(1)}));
  QCOMPARE(unified.rowCount(), 3);
}

void UnifiedMsgListModelTest::accountStateKeepsMessages() {
  auto work = inbox({{QStringLiteral("w1"), at(1)}});
  UnifiedMsgListModel unified;
  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("Work"),
                           work.get());
  QSignalSpy accounts(&unified, &UnifiedMsgListModel::accountsChanged);

  unified.setAccountState(QStringLiteral("work"),
                          UnifiedMsgListModel::AccountState::Failed,
                          QStringLiteral("Sign-in failed"));
  QCOMPARE(accounts.count(), 1);
  QCOMPARE(unified.accountState(QStringLiteral("work")),
           UnifiedMsgListModel::AccountState::Failed);
  QCOMPARE(unified.accountStateDetail(QStringLiteral("work")),
           QStringLiteral("Sign-in failed"));
  QCOMPARE(unified.rowCount(), 1);

  unified.setAccountState(QStringLiteral("work"),
                          UnifiedMsgListModel::AccountState::Failed,
                          QStringLiteral("Sign-in failed"));
  QCOMPARE(accounts.count(), 1);
  QCOMPARE(unified.accountState(QStringLiteral("missing")),
           UnifiedMsgListModel::AccountState::Unavailable);

  // An account without a list takes part without messages
  QVERIFY(unified.setAccountSource(QStringLiteral("new"), QStringLiteral("New"),
                                   nullptr));
  QCOMPARE(unified.accountIds().size(), 2);
  QCOMPARE(unified.rowCount(), 1);
}

void UnifiedMsgListModelTest::removingAnAccount() {
  auto work = inbox({{QStringLiteral("w1"), at(1)}});
  auto home = inbox({{QStringLiteral("h1"), at(2)}});
  UnifiedMsgListModel unified;
  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("Work"),
                           work.get());
  unified.setAccountSource(QStringLiteral("home"), QStringLiteral("Home"),
                           home.get());

  unified.removeAccount(QStringLiteral("work"));
  QCOMPARE(subjects(unified), QStringList{QStringLiteral("h1")});
  QCOMPARE(unified.accountIds(), QStringList{QStringLiteral("home")});
  work->appendRow(row({QStringLiteral("ignored"), at(1)}));
  QCOMPARE(unified.rowCount(), 1);
  // Its list may now be shown for another account
  QVERIFY(unified.setAccountSource(QStringLiteral("other"),
                                   QStringLiteral("Other"), work.get()));
}

void UnifiedMsgListModelTest::countsMessages() {
  Message readOne{QStringLiteral("w2"), at(2)};
  readOne.read = true;
  auto work = inbox({{QStringLiteral("w1"), at(1)}, readOne});
  auto home = inbox({{QStringLiteral("h1"), at(3)}});
  UnifiedMsgListModel unified;
  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("Work"),
                           work.get());
  unified.setAccountSource(QStringLiteral("home"), QStringLiteral("Home"),
                           home.get());

  QCOMPARE(unified.messageCount(), 3);
  QCOMPARE(unified.messageCount(QStringLiteral("work")), 2);
  QCOMPARE(unified.unreadCount(), 2);
  QCOMPARE(unified.unreadCount(QStringLiteral("work")), 1);

  // Messages whose flags are not known yet do not count as unread
  QList<QStandardItem *> unknown = row({QStringLiteral("w3"), at(4)});
  for (QStandardItem *item : unknown)
    item->setData(QVariant(), Imap::Mailbox::RoleMessageIsMarkedRead);
  work->appendRow(unknown);
  QCOMPARE(unified.unreadCount(QStringLiteral("work")), 1);
}

void UnifiedMsgListModelTest::messagesCannotBeDragged() {
  auto work = inbox({{QStringLiteral("w1"), at(1)}});
  UnifiedMsgListModel unified;
  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("Work"),
                           work.get());

  QVERIFY(work->index(0, 0).flags().testFlag(Qt::ItemIsDragEnabled));
  const Qt::ItemFlags flags = unified.index(0, 0).flags();
  QVERIFY(flags.testFlag(Qt::ItemIsSelectable));
  QVERIFY(!flags.testFlag(Qt::ItemIsDragEnabled));
  QVERIFY(!unified.mimeData({unified.index(0, 0)}));
  QCOMPARE(unified.supportedDragActions(), Qt::IgnoreAction);
}

void UnifiedMsgListModelTest::sortsAllAccountsByDate() {
  auto work =
      inbox({{QStringLiteral("w-1"), at(1)}, {QStringLiteral("w-4"), at(4)}});
  auto home =
      inbox({{QStringLiteral("h-3"), at(3)}, {QStringLiteral("h-2"), at(2)}});
  UnifiedMsgListModel unified;
  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("Work"),
                           work.get());
  unified.setAccountSource(QStringLiteral("home"), QStringLiteral("Home"),
                           home.get());
  PrettyUnifiedMsgListModel pretty;
  pretty.setSourceModel(&unified);

  pretty.sort(MsgListModel::DATE, Qt::DescendingOrder);
  QCOMPARE(subjects(pretty),
           (QStringList{QStringLiteral("w-4"), QStringLiteral("h-3"),
                        QStringLiteral("h-2"), QStringLiteral("w-1")}));

  // New mail takes its place by date, whichever account it arrives in
  home->appendRow(row({QStringLiteral("h-5"), at(5)}));
  QCOMPARE(subjects(pretty).constFirst(), QStringLiteral("h-5"));

  pretty.sort(MsgListModel::RECEIVED_DATE, Qt::AscendingOrder);
  QCOMPARE(subjects(pretty).constFirst(), QStringLiteral("w-1"));
}

void UnifiedMsgListModelTest::sortsBySubject() {
  auto work = inbox(
      {{QStringLiteral("banana"), at(1)}, {QStringLiteral("cherry"), at(2)}});
  auto home = inbox(
      {{QStringLiteral("apple"), at(3)}, {QStringLiteral("banana"), at(4)}});
  UnifiedMsgListModel unified;
  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("Work"),
                           work.get());
  unified.setAccountSource(QStringLiteral("home"), QStringLiteral("Home"),
                           home.get());
  PrettyUnifiedMsgListModel pretty;
  pretty.setSourceModel(&unified);

  pretty.sort(MsgListModel::SUBJECT, Qt::AscendingOrder);
  QCOMPARE(subjects(pretty),
           (QStringList{QStringLiteral("apple"), QStringLiteral("banana"),
                        QStringLiteral("banana"), QStringLiteral("cherry")}));
  // Equal subjects go by date
  QCOMPARE(
      pretty.index(1, 0).data(Imap::Mailbox::RoleMessageAccountId).toString(),
      QStringLiteral("work"));
}

void UnifiedMsgListModelTest::sortsByRelevance() {
  auto work = inbox(
      {{QStringLiteral("budget"), at(1)}, {QStringLiteral("lunch"), at(4)}});
  auto home = inbox(
      {{QStringLiteral("budget"), at(3)}, {QStringLiteral("news"), at(2)}});
  // Relevance is only known for messages with a UID
  for (QStandardItemModel *model : {work.get(), home.get()}) {
    for (int message = 0; message < model->rowCount(); ++message) {
      for (int column = 0; column < model->columnCount(); ++column)
        model->item(message, column)
            ->setData(message + 1, Imap::Mailbox::RoleMessageUid);
    }
  }
  UnifiedMsgListModel unified;
  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("Work"),
                           work.get());
  unified.setAccountSource(QStringLiteral("home"), QStringLiteral("Home"),
                           home.get());
  PrettyUnifiedMsgListModel pretty;
  pretty.setSourceModel(&unified);
  pretty.sort(MsgListModel::DATE, Qt::DescendingOrder);

  pretty.setRelevanceTerms({QStringLiteral("budget")}, Qt::AscendingOrder);
  QVERIFY(pretty.isSortingByRelevance());
  QCOMPARE(pretty.relevanceTerms(), QStringList{QStringLiteral("budget")});
  // The best matches first; equal ones and the rest newest first
  QCOMPARE(subjects(pretty),
           (QStringList{QStringLiteral("budget"), QStringLiteral("budget"),
                        QStringLiteral("lunch"), QStringLiteral("news")}));
  QCOMPARE(
      pretty.index(0, 0).data(Imap::Mailbox::RoleMessageAccountId).toString(),
      QStringLiteral("home"));

  pretty.setRelevanceTerms({}, Qt::DescendingOrder);
  QVERIFY(!pretty.isSortingByRelevance());
  pretty.sort(MsgListModel::DATE, Qt::DescendingOrder);
  QCOMPARE(subjects(pretty),
           (QStringList{QStringLiteral("lunch"), QStringLiteral("budget"),
                        QStringLiteral("news"), QStringLiteral("budget")}));
}

void UnifiedMsgListModelTest::quickFilters() {
  Message flagged{QStringLiteral("flagged"), at(1)};
  flagged.flagged = true;
  flagged.read = true;
  Message readOne{QStringLiteral("read"), at(2)};
  readOne.read = true;
  auto work = inbox({flagged, readOne});
  auto home = inbox({{QStringLiteral("unread"), at(3)}});
  UnifiedMsgListModel unified;
  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("Work"),
                           work.get());
  unified.setAccountSource(QStringLiteral("home"), QStringLiteral("Home"),
                           home.get());
  PrettyUnifiedMsgListModel pretty;
  pretty.setSourceModel(&unified);
  QCOMPARE(pretty.visibleMessageCount(), 3);

  pretty.setFilters(PrettyMsgListModel::FilterUnread);
  QCOMPARE(subjects(pretty), QStringList{QStringLiteral("unread")});
  pretty.setFilters(PrettyMsgListModel::FilterFlagged);
  QCOMPARE(subjects(pretty), QStringList{QStringLiteral("flagged")});
  pretty.setFilters(PrettyMsgListModel::FilterUnread |
                    PrettyMsgListModel::FilterFlagged);
  QCOMPARE(pretty.rowCount(), 0);
  QCOMPARE(pretty.filters(), PrettyMsgListModel::FilterUnread |
                                 PrettyMsgListModel::FilterFlagged);
  pretty.setFilters(PrettyMsgListModel::NoFilter);
  QCOMPARE(pretty.rowCount(), 3);
}

void UnifiedMsgListModelTest::facetsNarrowAcrossAccounts() {
  using UiUtils::SearchFacets;
  Message flagged{QStringLiteral("flagged"), at(1)};
  flagged.flagged = true;
  auto work = inbox({flagged, {QStringLiteral("plain"), at(2)}});
  auto home = inbox({{QStringLiteral("home"), at(3)}});
  UnifiedMsgListModel unified;
  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("Work"),
                           work.get());
  unified.setAccountSource(QStringLiteral("home"), QStringLiteral("Home"),
                           home.get());
  PrettyUnifiedMsgListModel pretty;
  pretty.setSourceModel(&unified);
  pretty.sort(MsgListModel::DATE, Qt::AscendingOrder);

  SearchFacets facets;
  facets.toggle({SearchFacets::Kind::Account, QStringLiteral("work"), QString()});
  pretty.setFacets(facets);
  QCOMPARE(pretty.facets(), facets);
  QCOMPARE(subjects(pretty),
           (QStringList{QStringLiteral("flagged"), QStringLiteral("plain")}));
  // Facets are counted among every result, whatever the chosen ones
  QCOMPARE(pretty.facetCandidates().size(), 3);
  QCOMPARE(facets.counts(pretty.facetCandidates(), SearchFacets::Kind::Folder)
               .value(0)
               .count,
           2);

  // The quick filters still apply, and narrow the candidates
  pretty.setFilters(PrettyMsgListModel::FilterFlagged);
  QCOMPARE(subjects(pretty), QStringList{QStringLiteral("flagged")});
  QCOMPARE(pretty.facetCandidates().size(), 1);
  pretty.setFilters(PrettyMsgListModel::NoFilter);

  // Values of one kind are alternatives
  facets.toggle({SearchFacets::Kind::Account, QStringLiteral("home"), QString()});
  pretty.setFacets(facets);
  QCOMPARE(pretty.rowCount(), 3);
  facets.clear();
  facets.toggle({SearchFacets::Kind::Folder,
                 QStringLiteral("home\nINBOX"), QString()});
  pretty.setFacets(facets);
  QCOMPARE(subjects(pretty), QStringList{QStringLiteral("home")});
  pretty.setFacets(SearchFacets());
  QCOMPARE(pretty.rowCount(), 3);
}

void UnifiedMsgListModelTest::keptVisibleMessageStaysShown() {
  auto work = inbox({{QStringLiteral("w1"), at(1)}});
  auto home = inbox({{QStringLiteral("h1"), at(2)}});
  UnifiedMsgListModel unified;
  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("Work"),
                           work.get());
  unified.setAccountSource(QStringLiteral("home"), QStringLiteral("Home"),
                           home.get());
  PrettyUnifiedMsgListModel pretty;
  pretty.setSourceModel(&unified);
  pretty.setFilters(PrettyMsgListModel::FilterUnread);

  // Reading a message marks it read; it stays until another one is shown
  pretty.setKeptVisibleIndex(pretty.index(1, MsgListModel::SUBJECT));
  setRead(*home, 0, true);
  QCOMPARE(subjects(pretty),
           (QStringList{QStringLiteral("w1"), QStringLiteral("h1")}));

  pretty.setKeptVisibleIndex(pretty.index(0, 0));
  QCOMPARE(subjects(pretty), QStringList{QStringLiteral("w1")});

  // Indexes of other models are ignored
  QStandardItemModel stranger;
  stranger.appendRow(new QStandardItem());
  pretty.setKeptVisibleIndex(stranger.index(0, 0));
  setRead(*work, 0, true);
  QCOMPARE(pretty.rowCount(), 0);
}

void UnifiedMsgListModelTest::tooltipNamesTheAccount() {
  auto work = inbox({{QStringLiteral("Q&A <today>"), at(1)}});
  UnifiedMsgListModel unified;
  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("R&D"),
                           work.get());
  PrettyUnifiedMsgListModel pretty;
  pretty.setSourceModel(&unified);

  const QString subject =
      pretty.index(0, MsgListModel::SUBJECT).data(Qt::ToolTipRole).toString();
  // Escaped once: the subject's own tooltip is escaped already
  QVERIFY2(subject.contains(QLatin1String("Q&amp;A &lt;today&gt;")),
           qPrintable(subject));
  QVERIFY2(subject.contains(QLatin1String("<br/><i>")), qPrintable(subject));
  QVERIFY2(subject.contains(QStringLiteral("R&amp;D \u00B7 Inbox")),
           qPrintable(subject));
  QVERIFY(!subject.contains(QLatin1String("&amp;amp;")));
  // Columns without a tooltip of their own still name the account
  const QString seen =
      pretty.index(0, MsgListModel::SEEN).data(Qt::ToolTipRole).toString();
  QVERIFY2(seen.contains(QStringLiteral("R&amp;D \u00B7 Inbox")),
           qPrintable(seen));
  QVERIFY(!seen.contains(QLatin1String("<br/>")));
  // Other roles are presented like a single folder's list
  QCOMPARE(
      pretty.index(0, MsgListModel::SUBJECT).data(Qt::DisplayRole).toString(),
      QStringLiteral("Q&A <today>"));
}

void UnifiedMsgListModelTest::passesTheModelTester() {
  auto work =
      inbox({{QStringLiteral("w-b"), at(2)}, {QStringLiteral("w-a"), at(1)}});
  auto *home =
      inbox({{QStringLiteral("h-c"), at(3)}, {QStringLiteral("h-a"), at(4)}})
          .release();
  UnifiedMsgListModel unified;
  QAbstractItemModelTester tester(
      &unified, QAbstractItemModelTester::FailureReportingMode::QtTest);
  PrettyUnifiedMsgListModel pretty;
  pretty.setSourceModel(&unified);
  QAbstractItemModelTester prettyTester(
      &pretty, QAbstractItemModelTester::FailureReportingMode::QtTest);
  pretty.sort(MsgListModel::DATE, Qt::DescendingOrder);

  unified.setAccountSource(QStringLiteral("work"), QStringLiteral("Work"),
                           work.get());
  unified.setAccountSource(QStringLiteral("home"), QStringLiteral("Home"),
                           home);
  const QPersistentModelIndex hc(unified.index(2, 0));
  QCOMPARE(hc.data(Imap::Mailbox::RoleMessageSubject).toString(),
           QStringLiteral("h-c"));

  // Sorting a source changes its layout; persistent indexes follow their
  // messages
  home->sort(0, Qt::AscendingOrder);
  QCOMPARE(hc.row(), 3);
  QCOMPARE(hc.data(Imap::Mailbox::RoleMessageSubject).toString(),
           QStringLiteral("h-c"));

  work->appendRow(row({QStringLiteral("w-c"), at(5)}));
  work->removeRow(0);
  work->clear();
  work->setColumnCount(MsgListModel::COLUMN_COUNT);
  work->appendRow(row({QStringLiteral("w-d"), at(6)}));
  QCOMPARE(subjects(pretty).constFirst(), QStringLiteral("w-d"));

  delete home;
  QVERIFY(!hc.isValid());
  QCOMPARE(subjects(unified), QStringList{QStringLiteral("w-d")});
  unified.removeAccount(QStringLiteral("work"));
  QCOMPARE(pretty.rowCount(), 0);
}

QTEST_GUILESS_MAIN(UnifiedMsgListModelTest)
