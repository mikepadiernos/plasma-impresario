/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QSignalSpy>
#include <QStandardItemModel>
#include <QtTest>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/PrettyMsgListModel.h"
#include "test_PrettyMsgListFilters.h"

using Imap::Mailbox::PrettyMsgListModel;

namespace {

struct Message {
  bool fetched = true;
  bool read = false;
  bool wasUnread = false;
  bool flagged = false;
  bool attachments = false;
};

/** @short A message as the threading model presents it; flags are read without
 * fetching anything */
QStandardItem *message(const QString &name, const Message &state) {
  using namespace Imap::Mailbox;
  auto *item = new QStandardItem(name);
  item->setData(state.fetched, RoleIsFetched);
  item->setData(state.read, RoleMessageIsMarkedRead);
  item->setData(state.wasUnread || !state.read, RoleMessageWasUnread);
  item->setData(state.flagged, RoleMessageIsMarkedFlagged);
  // Like the real model, nothing is known about attachments before the message
  // is fetched
  item->setData(state.fetched && state.attachments, RoleMessageHasAttachments);
  return item;
}

QStandardItem *read(const QString &name) {
  return message(name, Message{true, true, false, false, false});
}

QStandardItem *unread(const QString &name) {
  return message(name, Message{true, false, true, false, false});
}

/** @short A thread member which is missing from the mailbox; it has no data at
 * all */
QStandardItem *placeholder() { return new QStandardItem(); }

void collect(const PrettyMsgListModel &proxy, const QModelIndex &parent,
             QStringList &names) {
  for (int row = 0; row < proxy.rowCount(parent); ++row) {
    const QModelIndex index = proxy.index(row, 0, parent);
    const QString name =
        proxy.mapToSource(index).data(Qt::DisplayRole).toString();
    names << (name.isEmpty() ? QStringLiteral("?") : name);
    collect(proxy, index, names);
  }
}

QStringList visible(const PrettyMsgListModel &proxy) {
  QStringList names;
  collect(proxy, QModelIndex(), names);
  return names;
}

void markRead(QStandardItem *item) {
  item->setData(true, Imap::Mailbox::RoleMessageIsMarkedRead);
  // A new mailbox session forgets that it was unread
  item->setData(false, Imap::Mailbox::RoleMessageWasUnread);
}

} // namespace

void PrettyMsgListFiltersTest::noFilterShowsEverything() {
  QStandardItemModel source;
  source.appendRow(read(QStringLiteral("read")));
  source.appendRow(unread(QStringLiteral("unread")));
  source.appendRow(message(QStringLiteral("unknown"), Message{false}));
  PrettyMsgListModel proxy(nullptr);
  proxy.setSourceModel(&source);
  QCOMPARE(proxy.filters(),
           PrettyMsgListModel::Filters(PrettyMsgListModel::NoFilter));
  QCOMPARE(visible(proxy),
           (QStringList{QStringLiteral("read"), QStringLiteral("unread"),
                        QStringLiteral("unknown")}));
}

void PrettyMsgListFiltersTest::unreadFilter() {
  QStandardItemModel source;
  source.appendRow(read(QStringLiteral("read")));
  source.appendRow(unread(QStringLiteral("unread")));
  // Read in this session, so it stays until another mailbox is opened
  source.appendRow(
      message(QStringLiteral("was unread"), Message{true, true, true}));
  // Flags are not known yet, which looks like unread
  source.appendRow(message(QStringLiteral("unknown"), Message{false}));
  PrettyMsgListModel proxy(nullptr);
  proxy.setSourceModel(&source);
  proxy.setFilterEnabled(PrettyMsgListModel::FilterUnread, true);
  QCOMPARE(visible(proxy),
           (QStringList{QStringLiteral("unread"), QStringLiteral("was unread"),
                        QStringLiteral("unknown")}));
}

void PrettyMsgListFiltersTest::hideReadIsTheUnreadFilter() {
  QStandardItemModel source;
  source.appendRow(read(QStringLiteral("read")));
  source.appendRow(unread(QStringLiteral("unread")));
  PrettyMsgListModel proxy(nullptr);
  proxy.setSourceModel(&source);
  QSignalSpy changed(&proxy, &PrettyMsgListModel::filtersChanged);

  proxy.setHideRead(true);
  QVERIFY(proxy.hideRead());
  QCOMPARE(proxy.filters(),
           PrettyMsgListModel::Filters(PrettyMsgListModel::FilterUnread));
  QCOMPARE(visible(proxy), QStringList{QStringLiteral("unread")});
  QCOMPARE(changed.size(), 1);
  // Nothing changes, nothing is announced
  proxy.setHideRead(true);
  proxy.setFilterEnabled(PrettyMsgListModel::FilterUnread, true);
  QCOMPARE(changed.size(), 1);

  proxy.setFilterEnabled(PrettyMsgListModel::FilterFlagged, true);
  proxy.setHideRead(false);
  QVERIFY(!proxy.hideRead());
  QCOMPARE(proxy.filters(),
           PrettyMsgListModel::Filters(PrettyMsgListModel::FilterFlagged));
  QCOMPARE(changed.size(), 3);

  proxy.setFilters(PrettyMsgListModel::NoFilter);
  proxy.setFilterEnabled(PrettyMsgListModel::FilterUnread, true);
  QVERIFY(proxy.hideRead());
  QCOMPARE(visible(proxy), QStringList{QStringLiteral("unread")});
}

void PrettyMsgListFiltersTest::filtersAreCombined() {
  QStandardItemModel source;
  source.appendRow(read(QStringLiteral("read")));
  source.appendRow(unread(QStringLiteral("unread")));
  source.appendRow(message(QStringLiteral("read flagged"),
                           Message{true, true, false, true}));
  source.appendRow(message(QStringLiteral("unread flagged"),
                           Message{true, false, true, true}));
  source.appendRow(message(QStringLiteral("unread flagged attachment"),
                           Message{true, false, true, true, true}));
  PrettyMsgListModel proxy(nullptr);
  proxy.setSourceModel(&source);

  proxy.setFilters(PrettyMsgListModel::FilterFlagged);
  QCOMPARE(visible(proxy),
           (QStringList{QStringLiteral("read flagged"),
                        QStringLiteral("unread flagged"),
                        QStringLiteral("unread flagged attachment")}));
  proxy.setFilters(PrettyMsgListModel::FilterFlagged |
                   PrettyMsgListModel::FilterUnread);
  QCOMPARE(visible(proxy),
           (QStringList{QStringLiteral("unread flagged"),
                        QStringLiteral("unread flagged attachment")}));
  proxy.setFilterEnabled(PrettyMsgListModel::FilterAttachments, true);
  QCOMPARE(visible(proxy),
           QStringList{QStringLiteral("unread flagged attachment")});
  proxy.setFilters(PrettyMsgListModel::NoFilter);
  QCOMPARE(source.rowCount(), proxy.rowCount());
}

void PrettyMsgListFiltersTest::unknownStateChangesVisibilityOnce() {
  QStandardItemModel source;
  auto *pending = message(QStringLiteral("pending"), Message{false});
  source.appendRow(pending);
  PrettyMsgListModel proxy(nullptr);
  proxy.setSourceModel(&source);

  // Without known flags a message is not flagged, so it shows up only when its
  // flags say so
  proxy.setFilters(PrettyMsgListModel::FilterFlagged);
  QCOMPARE(proxy.rowCount(), 0);
  pending->setData(true, Imap::Mailbox::RoleMessageIsMarkedFlagged);
  QCOMPARE(proxy.rowCount(), 1);

  // ...and an unknown message counts as unread, so it goes away only when its
  // flags say so
  proxy.setFilters(PrettyMsgListModel::FilterUnread);
  QCOMPARE(proxy.rowCount(), 1);
  markRead(pending);
  QCOMPARE(proxy.rowCount(), 0);
}

void PrettyMsgListFiltersTest::attachmentsFilter() {
  QStandardItemModel source;
  source.appendRow(message(QStringLiteral("plain"), Message{true}));
  source.appendRow(message(QStringLiteral("attachment"),
                           Message{true, false, true, false, true}));
  auto *pending = message(QStringLiteral("pending"), Message{false});
  source.appendRow(pending);
  PrettyMsgListModel proxy(nullptr);
  proxy.setSourceModel(&source);
  proxy.setFilters(PrettyMsgListModel::FilterAttachments);
  // The structure of "pending" is not known, so it is shown rather than
  // flickering in later
  QCOMPARE(visible(proxy), (QStringList{QStringLiteral("attachment"),
                                        QStringLiteral("pending")}));
  pending->setData(true, Imap::Mailbox::RoleIsFetched);
  QCOMPARE(visible(proxy), QStringList{QStringLiteral("attachment")});
}

void PrettyMsgListFiltersTest::threadsAreShownAsAWhole() {
  QStandardItemModel source;
  auto *readThread = read(QStringLiteral("root"));
  auto *reply = read(QStringLiteral("reply"));
  readThread->appendRow(reply);
  reply->appendRow(unread(QStringLiteral("nested unread")));
  readThread->appendRow(read(QStringLiteral("other reply")));
  source.appendRow(readThread);
  auto *allRead = read(QStringLiteral("old"));
  allRead->appendRow(read(QStringLiteral("old reply")));
  source.appendRow(allRead);
  auto *missing = placeholder();
  missing->appendRow(unread(QStringLiteral("orphan")));
  source.appendRow(missing);
  auto *missingRead = placeholder();
  missingRead->appendRow(read(QStringLiteral("read orphan")));
  source.appendRow(missingRead);
  PrettyMsgListModel proxy(nullptr);
  proxy.setSourceModel(&source);

  proxy.setHideRead(true);
  QCOMPARE(visible(proxy),
           (QStringList{QStringLiteral("root"), QStringLiteral("reply"),
                        QStringLiteral("nested unread"),
                        QStringLiteral("other reply"), QStringLiteral("?"),
                        QStringLiteral("orphan")}));
}

void PrettyMsgListFiltersTest::keptVisibleMessageStaysShown() {
  QStandardItemModel source;
  auto *first =
      message(QStringLiteral("first"), Message{true, false, true, true});
  auto *second =
      message(QStringLiteral("second"), Message{true, false, true, true});
  source.appendRow(first);
  source.appendRow(second);
  source.appendRow(read(QStringLiteral("read")));
  PrettyMsgListModel proxy(nullptr);
  proxy.setSourceModel(&source);
  proxy.setFilters(PrettyMsgListModel::FilterFlagged);

  // The selected message is given as an index of the proxy
  proxy.setKeptVisibleIndex(proxy.index(0, 0));
  QCOMPARE(proxy.keptVisibleIndex(), source.indexFromItem(first));
  first->setData(false, Imap::Mailbox::RoleMessageIsMarkedFlagged);
  QCOMPARE(visible(proxy),
           (QStringList{QStringLiteral("first"), QStringLiteral("second")}));
  // Selecting another message lets the previous one go
  proxy.setKeptVisibleIndex(source.indexFromItem(second));
  QCOMPARE(visible(proxy), QStringList{QStringLiteral("second")});
  // A kept message which still matches does not need to stay
  proxy.setKeptVisibleIndex(QModelIndex());
  QCOMPARE(visible(proxy), QStringList{QStringLiteral("second")});
  QVERIFY(!proxy.keptVisibleIndex().isValid());

  // Also when the message is read under the unread filter
  proxy.setFilters(PrettyMsgListModel::FilterUnread);
  proxy.setKeptVisibleIndex(proxy.index(1, 0));
  QCOMPARE(proxy.keptVisibleIndex(), source.indexFromItem(second));
  markRead(second);
  QCOMPARE(visible(proxy),
           (QStringList{QStringLiteral("first"), QStringLiteral("second")}));
  proxy.setKeptVisibleIndex(proxy.index(0, 0));
  QCOMPARE(visible(proxy), QStringList{QStringLiteral("first")});

  // An index of an unrelated model is ignored
  QStandardItemModel other;
  other.appendRow(read(QStringLiteral("elsewhere")));
  proxy.setKeptVisibleIndex(other.index(0, 0));
  QVERIFY(!proxy.keptVisibleIndex().isValid());
}

void PrettyMsgListFiltersTest::keptVisibleMessageKeepsItsThread() {
  QStandardItemModel source;
  auto *root = read(QStringLiteral("root"));
  auto *reply = unread(QStringLiteral("reply"));
  root->appendRow(reply);
  source.appendRow(root);
  source.appendRow(unread(QStringLiteral("unread")));
  PrettyMsgListModel proxy(nullptr);
  proxy.setSourceModel(&source);
  proxy.setHideRead(true);
  QCOMPARE(visible(proxy),
           (QStringList{QStringLiteral("root"), QStringLiteral("reply"),
                        QStringLiteral("unread")}));

  proxy.setKeptVisibleIndex(source.indexFromItem(reply));
  markRead(reply);
  // The threading model reports changes of a thread member on its root
  emit source.dataChanged(source.indexFromItem(root),
                          source.indexFromItem(root));
  QCOMPARE(visible(proxy),
           (QStringList{QStringLiteral("root"), QStringLiteral("reply"),
                        QStringLiteral("unread")}));
  proxy.setKeptVisibleIndex(source.index(1, 0));
  QCOMPARE(visible(proxy), QStringList{QStringLiteral("unread")});
}

void PrettyMsgListFiltersTest::visibleMessageCount() {
  QStandardItemModel source;
  auto *root = read(QStringLiteral("root"));
  root->appendRow(unread(QStringLiteral("reply")));
  root->appendRow(read(QStringLiteral("other reply")));
  source.appendRow(root);
  source.appendRow(read(QStringLiteral("read")));
  auto *missing = placeholder();
  missing->appendRow(read(QStringLiteral("orphan")));
  source.appendRow(missing);
  PrettyMsgListModel proxy(nullptr);
  QCOMPARE(proxy.visibleMessageCount(), 0);
  proxy.setSourceModel(&source);
  // Placeholders of missing messages are not counted
  QCOMPARE(proxy.visibleMessageCount(), 5);
  proxy.setHideRead(true);
  QCOMPARE(proxy.visibleMessageCount(), 3);
  proxy.setFilters(PrettyMsgListModel::FilterFlagged);
  QCOMPARE(proxy.visibleMessageCount(), 0);
}

QTEST_GUILESS_MAIN(PrettyMsgListFiltersTest)
