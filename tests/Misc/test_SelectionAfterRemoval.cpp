/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QStandardItemModel>
#include <QStandardPaths>
#include <QtTest>

#include "test_SelectionAfterRemoval.h"
#include "UiUtils/SelectionAfterRemoval.h"

using namespace UiUtils::SelectionAfterRemoval;

namespace {

/** @short A flat list of @arg count rows, numbered from 1 */
struct FlatList {
    QStandardItemModel model;

    explicit FlatList(const int count)
    {
        for (int i = 1; i <= count; ++i)
            model.appendRow(new QStandardItem(QString::number(i)));
    }

    QModelIndex row(const int number) const
    {
        return model.index(number - 1, 0);
    }

    int number(const QModelIndex &index) const
    {
        return index.isValid() ? index.row() + 1 : 0;
    }

    Step below() const
    {
        return [this](const QModelIndex &index) { return model.index(index.row() + 1, 0); };
    }

    Step above() const
    {
        return [this](const QModelIndex &index) { return index.row() > 0 ? model.index(index.row() - 1, 0) : QModelIndex(); };
    }

    Test numbers(const QList<int> &listed) const
    {
        return [this, listed](const QModelIndex &index) { return listed.contains(number(index)); };
    }

    Test allBut(const QList<int> &excluded) const
    {
        return [this, excluded](const QModelIndex &index) { return !excluded.contains(number(index)); };
    }
};

}

void SelectionAfterRemovalTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void SelectionAfterRemovalTest::successor_data()
{
    QTest::addColumn<int>("count");
    QTest::addColumn<int>("current");
    QTest::addColumn<QList<int>>("removed");
    QTest::addColumn<QList<int>>("pending");
    QTest::addColumn<int>("expected");
    QTest::addColumn<int>("withoutPending");

    QTest::newRow("single in the middle") << 5 << 3 << QList<int>{3} << QList<int>{} << 4 << 4;
    QTest::newRow("block, current first") << 6 << 2 << QList<int>{2, 3, 4} << QList<int>{} << 5 << 5;
    QTest::newRow("block, current last") << 6 << 4 << QList<int>{2, 3, 4} << QList<int>{} << 5 << 5;
    QTest::newRow("last row") << 5 << 5 << QList<int>{5} << QList<int>{} << 4 << 4;
    QTest::newRow("block at the end") << 6 << 4 << QList<int>{4, 5, 6} << QList<int>{} << 3 << 3;
    QTest::newRow("first row") << 4 << 1 << QList<int>{1} << QList<int>{} << 2 << 2;
    // Removal often arrives one message at a time: the rest of the selection is skipped while it is still listed
    QTest::newRow("pending rest of the selection") << 6 << 2 << QList<int>{2} << QList<int>{3, 4} << 5 << 3;
    QTest::newRow("pending at the end") << 6 << 4 << QList<int>{4} << QList<int>{5, 6} << 3 << 5;
    QTest::newRow("pending gap") << 7 << 2 << QList<int>{2} << QList<int>{3, 5} << 4 << 3;
    QTest::newRow("only pending rows left") << 3 << 1 << QList<int>{1} << QList<int>{2, 3} << 2 << 2;
    QTest::newRow("everything removed") << 3 << 2 << QList<int>{1, 2, 3} << QList<int>{} << 0 << 0;
    QTest::newRow("only row") << 1 << 1 << QList<int>{1} << QList<int>{} << 0 << 0;
}

void SelectionAfterRemovalTest::successor()
{
    QFETCH(int, count);
    QFETCH(int, current);
    QFETCH(QList<int>, removed);
    QFETCH(QList<int>, pending);
    QFETCH(int, expected);
    QFETCH(int, withoutPending);

    const FlatList list(count);
    const QModelIndex next = UiUtils::SelectionAfterRemoval::successor(list.row(current), list.below(), list.above(),
                                                                       list.allBut(removed), list.numbers(pending));
    QCOMPARE(list.number(next), expected);
    const QModelIndex plain = UiUtils::SelectionAfterRemoval::successor(list.row(current), list.below(), list.above(),
                                                                        list.allBut(removed));
    QCOMPARE(list.number(plain), withoutPending);
}

void SelectionAfterRemovalTest::invalidInput()
{
    const FlatList list(3);
    QVERIFY(!UiUtils::SelectionAfterRemoval::successor(QModelIndex(), list.below(), list.above(), list.allBut({})).isValid());
    QVERIFY(!UiUtils::SelectionAfterRemoval::successor(list.row(1), Step(), list.above(), list.allBut({})).isValid());
    QVERIFY(!UiUtils::SelectionAfterRemoval::successor(list.row(1), list.below(), list.above(), Test()).isValid());
}

void SelectionAfterRemovalTest::withinRemovedRows()
{
    // [1 [2, 3]], 4
    QStandardItemModel model;
    auto *thread = new QStandardItem(QStringLiteral("1"));
    thread->appendRow(new QStandardItem(QStringLiteral("2")));
    thread->appendRow(new QStandardItem(QStringLiteral("3")));
    model.appendRow(thread);
    model.appendRow(new QStandardItem(QStringLiteral("4")));
    const QModelIndex one = model.index(0, 0);
    const QModelIndex two = model.index(0, 0, one);
    const QModelIndex three = model.index(1, 0, one);
    const QModelIndex four = model.index(1, 0);

    QVERIFY(isWithinRemovedRows(one, QModelIndex(), 0, 0));
    // Removing a thread removes the replies inside it
    QVERIFY(isWithinRemovedRows(two, QModelIndex(), 0, 0));
    QVERIFY(isWithinRemovedRows(three, QModelIndex(), 0, 0));
    QVERIFY(!isWithinRemovedRows(four, QModelIndex(), 0, 0));
    QVERIFY(isWithinRemovedRows(four, QModelIndex(), 0, 1));
    QVERIFY(isWithinRemovedRows(three, one, 1, 1));
    QVERIFY(!isWithinRemovedRows(two, one, 1, 1));
    QVERIFY(!isWithinRemovedRows(one, one, 0, 1));
    QVERIFY(!isWithinRemovedRows(QModelIndex(), QModelIndex(), 0, 1));
}

QTEST_GUILESS_MAIN(SelectionAfterRemovalTest)
