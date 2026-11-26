/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QAbstractItemModelTester>
#include <QStandardItemModel>
#include <QtTest>

#include "test_SearchResultsModel.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/SearchResultsModel.h"
#include "Imap/Model/SearchSnippets.h"

using namespace Imap::Mailbox;

namespace {

/** @short An account-like tree: folders, each with a message list whose rows are messages */
struct Account {
    QStandardItemModel model;

    QStandardItem *folder(const QString &name)
    {
        auto *folder = new QStandardItem(name);
        folder->appendRow(new QStandardItem(QStringLiteral("[messages]")));
        model.appendRow(folder);
        return folder;
    }

    QModelIndex message(QStandardItem *folder, const QString &subject, uint uid)
    {
        auto *message = new QStandardItem(subject);
        message->setData(uid, RoleMessageUid);
        message->setData(subject, RoleMessageSubject);
        folder->child(0)->appendRow(message);
        return message->index();
    }
};

}

void SearchResultsModelTest::listsAddedMessagesOnce()
{
    Account account;
    QStandardItem *inbox = account.folder(QStringLiteral("INBOX"));
    const QModelIndex a = account.message(inbox, QStringLiteral("a"), 1);
    const QModelIndex b = account.message(inbox, QStringLiteral("b"), 2);
    SearchResultsModel results;
    results.setSourceModel(&account.model);
    QSignalSpy inserted(&results, &QAbstractItemModel::rowsInserted);

    results.addMessages({a, b, a});
    QCOMPARE(results.rowCount(), 2);
    QCOMPARE(inserted.count(), 1);
    // Already listed, of another model or invalid: nothing to add
    QStandardItemModel other;
    other.appendRow(new QStandardItem(QStringLiteral("x")));
    results.addMessages({b, b.siblingAtColumn(0), other.index(0, 0), QModelIndex()});
    QCOMPARE(results.rowCount(), 2);
    QCOMPARE(inserted.count(), 1);
    QCOMPARE(results.messages(), (QModelIndexList{a, b}));

    results.clear();
    QCOMPARE(results.rowCount(), 0);
}

void SearchResultsModelTest::mapsToTheMessages()
{
    Account account;
    QStandardItem *inbox = account.folder(QStringLiteral("INBOX"));
    QStandardItem *archive = account.folder(QStringLiteral("Archive"));
    const QModelIndex a = account.message(inbox, QStringLiteral("a"), 1);
    const QModelIndex b = account.message(archive, QStringLiteral("b"), 7);
    SearchResultsModel results;
    results.setSourceModel(&account.model);
    results.addMessages({b, a});

    QCOMPARE(results.mapToSource(results.index(0, 0)), QModelIndex(b));
    QCOMPARE(results.mapToSource(results.index(1, 0)), QModelIndex(a));
    QCOMPARE(results.mapFromSource(a), results.index(1, 0));
    QVERIFY(!results.mapFromSource(inbox->index()).isValid());
    QCOMPARE(results.index(0, 0).data(RoleMessageUid).toUInt(), 7u);
    QVERIFY(!results.hasChildren(results.index(0, 0)));
    QCOMPARE(results.flags(results.index(0, 0)), Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren);
}

void SearchResultsModelTest::presentsMessageListColumns()
{
    Account account;
    QStandardItem *inbox = account.folder(QStringLiteral("INBOX"));
    const QModelIndex a = account.message(inbox, QStringLiteral("hello"), 1);
    account.model.setData(a, QDateTime(QDate(2026, 3, 5), QTime(9, 0)), RoleMessageDate);
    SearchResultsModel results;
    results.setSourceModel(&account.model);
    results.addMessages({a});

    QCOMPARE(results.columnCount(), int(MsgListModel::COLUMN_COUNT));
    QCOMPARE(results.index(0, MsgListModel::SUBJECT).data().toString(), QStringLiteral("hello"));
    QCOMPARE(results.index(0, MsgListModel::FROM).data().toString(), QStringLiteral("[from]"));
    QCOMPARE(results.index(0, MsgListModel::DATE).data().toDateTime(), QDateTime(QDate(2026, 3, 5), QTime(9, 0)));
    QCOMPARE(results.headerData(MsgListModel::SUBJECT, Qt::Horizontal),
             MsgListModel::columnHeader(MsgListModel::SUBJECT, Qt::DisplayRole));
}

void SearchResultsModelTest::forwardsChangedData()
{
    Account account;
    QStandardItem *inbox = account.folder(QStringLiteral("INBOX"));
    account.message(inbox, QStringLiteral("a"), 1);
    const QModelIndex b = account.message(inbox, QStringLiteral("b"), 2);
    SearchResultsModel results;
    results.setSourceModel(&account.model);
    results.addMessages({b});
    QSignalSpy changed(&results, &QAbstractItemModel::dataChanged);

    account.model.setData(b, QStringLiteral("b2"), RoleMessageSubject);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.at(0).at(0).toModelIndex(), results.index(0, 0));
    QCOMPARE(results.index(0, 0).data(RoleMessageSubject).toString(), QStringLiteral("b2"));
    // The same row of another folder is not a listed message
    QStandardItem *archive = account.folder(QStringLiteral("Archive"));
    const QModelIndex other = account.message(archive, QStringLiteral("o"), 2);
    account.message(archive, QStringLiteral("p"), 3);
    account.model.setData(other.siblingAtRow(1), QStringLiteral("changed"), RoleMessageSubject);
    QCOMPARE(changed.count(), 1);
}

void SearchResultsModelTest::providesSearchSnippets()
{
    Account account;
    QStandardItem *inbox = account.folder(QStringLiteral("INBOX"));
    inbox->setData(QStringLiteral("INBOX"), RoleMailboxName);
    const QModelIndex a = account.message(inbox, QStringLiteral("a"), 4);
    SearchResultsModel results;
    results.setSourceModel(&account.model);
    results.addMessages({a});
    // No terms, no snippets
    QVERIFY(!results.index(0, 0).data(RoleMessageSearchSnippet).isValid());

    results.snippets()->setTextSource([](const QString &, uint uid) {
        return uid == 4 ? QStringLiteral("See you at noon") : QString();
    });
    QSignalSpy changed(&results, &QAbstractItemModel::dataChanged);
    results.snippets()->setTerms({QStringLiteral("noon")});
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.at(0).at(2).value<QList<int>>(), QList<int>{RoleMessageSearchSnippet});
    const auto snippet = results.index(0, MsgListModel::SUBJECT).data(RoleMessageSearchSnippet).value<SearchSnippet>();
    QCOMPARE(snippet.text, QStringLiteral("See you at noon"));
    QCOMPARE(snippet.matches, (QList<std::pair<int, int>>{{11, 4}}));
}

void SearchResultsModelTest::dropsRemovedMessages()
{
    Account account;
    QStandardItem *inbox = account.folder(QStringLiteral("INBOX"));
    const QModelIndex a = account.message(inbox, QStringLiteral("a"), 1);
    const QModelIndex b = account.message(inbox, QStringLiteral("b"), 2);
    const QModelIndex c = account.message(inbox, QStringLiteral("c"), 3);
    SearchResultsModel results;
    results.setSourceModel(&account.model);
    results.addMessages({c, a, b});

    // As after an expunge of the first two
    account.model.removeRows(0, 2, a.parent());
    QCOMPARE(results.rowCount(), 1);
    QCOMPARE(results.index(0, 0).data(RoleMessageSubject).toString(), QStringLiteral("c"));
}

void SearchResultsModelTest::dropsMessagesOfRemovedFolders()
{
    Account account;
    QStandardItem *inbox = account.folder(QStringLiteral("INBOX"));
    QStandardItem *archive = account.folder(QStringLiteral("Archive"));
    const QModelIndex a = account.message(inbox, QStringLiteral("a"), 1);
    const QModelIndex b = account.message(archive, QStringLiteral("b"), 2);
    SearchResultsModel results;
    results.setSourceModel(&account.model);
    results.addMessages({a, b});

    account.model.removeRow(archive->row());
    QCOMPARE(results.rowCount(), 1);
    QCOMPARE(results.mapToSource(results.index(0, 0)), QModelIndex(a));
}

void SearchResultsModelTest::sourceResetAndDestructionEmptyTheList()
{
    SearchResultsModel results;
    {
        Account account;
        QStandardItem *inbox = account.folder(QStringLiteral("INBOX"));
        results.setSourceModel(&account.model);
        results.addMessages({account.message(inbox, QStringLiteral("a"), 1)});
        QSignalSpy reset(&results, &QAbstractItemModel::modelReset);
        account.model.clear();
        QCOMPARE(reset.count(), 1);
        QCOMPARE(results.rowCount(), 0);

        inbox = account.folder(QStringLiteral("INBOX"));
        results.addMessages({account.message(inbox, QStringLiteral("b"), 2)});
        QCOMPARE(results.rowCount(), 1);
    }
    QCOMPARE(results.rowCount(), 0);
}

void SearchResultsModelTest::passesTheModelTester()
{
    Account account;
    QStandardItem *inbox = account.folder(QStringLiteral("INBOX"));
    QStandardItem *archive = account.folder(QStringLiteral("Archive"));
    SearchResultsModel results;
    results.setSourceModel(&account.model);
    QAbstractItemModelTester tester(&results, QAbstractItemModelTester::FailureReportingMode::QtTest);

    results.addMessages({account.message(inbox, QStringLiteral("a"), 1), account.message(archive, QStringLiteral("b"), 2)});
    results.addMessages({account.message(inbox, QStringLiteral("c"), 3)});
    account.model.setData(results.mapToSource(results.index(2, 0)), QStringLiteral("c2"), RoleMessageSubject);
    account.model.removeRow(0, inbox->child(0)->index());
    account.model.removeRow(archive->row());
    QCOMPARE(results.rowCount(), 1);
    results.clear();
}

QTEST_GUILESS_MAIN(SearchResultsModelTest)
