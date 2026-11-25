/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QStandardItemModel>
#include <QtTest>

#include "test_UnreadFolders.h"
#include "Imap/Model/ItemRoles.h"
#include "UiUtils/UnreadFolders.h"

using UiUtils::nextUnreadMailbox;

namespace {

/** @short A folder with @arg unread messages; a negative count means the count is not known yet */
QStandardItem *folder(const QString &name, const int unread, const bool selectable = true)
{
    auto *item = new QStandardItem(name.section(QLatin1Char('/'), -1));
    item->setData(name, Imap::Mailbox::RoleMailboxName);
    item->setData(selectable, Imap::Mailbox::RoleMailboxIsSelectable);
    if (unread >= 0)
        item->setData(unread, Imap::Mailbox::RoleUnreadMessageCount);
    return item;
}

QString nameOf(const QModelIndex &index)
{
    return index.data(Imap::Mailbox::RoleMailboxName).toString();
}

}

void UnreadFoldersTest::walksTreeOrderAndWraps()
{
    QStandardItemModel model;
    model.appendRow(folder(QStringLiteral("INBOX"), 2));
    auto *projects = folder(QStringLiteral("Projects"), 0);
    projects->appendRow(folder(QStringLiteral("Projects/Alpha"), 0));
    projects->appendRow(folder(QStringLiteral("Projects/Beta"), 4));
    model.appendRow(projects);
    model.appendRow(folder(QStringLiteral("Lists"), 1));

    QCOMPARE(nameOf(nextUnreadMailbox(&model, QString())), QStringLiteral("INBOX"));
    QCOMPARE(nameOf(nextUnreadMailbox(&model, QStringLiteral("INBOX"))), QStringLiteral("Projects/Beta"));
    QCOMPARE(nameOf(nextUnreadMailbox(&model, QStringLiteral("Projects/Beta"))), QStringLiteral("Lists"));
    QCOMPARE(nameOf(nextUnreadMailbox(&model, QStringLiteral("Lists"))), QStringLiteral("INBOX"));
    // A folder that is not in the tree, such as one of another account, starts from the top
    QCOMPARE(nameOf(nextUnreadMailbox(&model, QStringLiteral("Elsewhere"))), QStringLiteral("INBOX"));
}

void UnreadFoldersTest::skipsUnknownAndUnselectable()
{
    QStandardItemModel model;
    model.appendRow(folder(QStringLiteral("INBOX"), 0));
    model.appendRow(folder(QStringLiteral("Loading"), -1));
    auto *shared = folder(QStringLiteral("Shared"), 5, false);
    shared->appendRow(folder(QStringLiteral("Shared/Team"), 3));
    model.appendRow(shared);

    QCOMPARE(nameOf(nextUnreadMailbox(&model, QStringLiteral("INBOX"))), QStringLiteral("Shared/Team"));
}

void UnreadFoldersTest::doesNotDescendIntoUnlistedBranches()
{
    QStandardItemModel model;
    model.appendRow(folder(QStringLiteral("INBOX"), 0));
    auto *lazy = folder(QStringLiteral("Lazy"), 0);
    lazy->setData(false, Imap::Mailbox::RoleIsFetched);
    lazy->appendRow(folder(QStringLiteral("Lazy/Hidden"), 9));
    model.appendRow(lazy);

    QVERIFY(!nextUnreadMailbox(&model, QStringLiteral("INBOX")).isValid());
    lazy->setData(true, Imap::Mailbox::RoleIsFetched);
    QCOMPARE(nameOf(nextUnreadMailbox(&model, QStringLiteral("INBOX"))), QStringLiteral("Lazy/Hidden"));
}

void UnreadFoldersTest::noOtherUnreadFolder()
{
    QStandardItemModel model;
    QVERIFY(!nextUnreadMailbox(&model, QString()).isValid());
    QVERIFY(!nextUnreadMailbox(nullptr, QString()).isValid());
    model.appendRow(folder(QStringLiteral("INBOX"), 3));
    model.appendRow(folder(QStringLiteral("Sent"), 0));
    // The current folder is never its own successor
    QVERIFY(!nextUnreadMailbox(&model, QStringLiteral("INBOX")).isValid());
    QCOMPARE(nameOf(nextUnreadMailbox(&model, QStringLiteral("Sent"))), QStringLiteral("INBOX"));
}

QTEST_GUILESS_MAIN(UnreadFoldersTest)
