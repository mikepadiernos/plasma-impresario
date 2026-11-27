/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QStandardItemModel>
#include <QTimeZone>
#include <QtTest>
#include "test_RelatedSearch.h"
#include "Imap/Model/ItemRoles.h"
#include "UiUtils/RelatedSearch.h"

namespace {

QVariant address(const QString &mailbox, const QString &host = QStringLiteral("example.org"))
{
    return QVariantList{QStringList{QStringLiteral("Someone"), QString(), mailbox, host}};
}

struct Row {
    QString subject;
    QVariant from;
    QByteArray messageId;
    QList<QByteArray> inReplyTo;
    QList<QByteArray> references;
    QDateTime date = QDateTime();
    bool fetched = true;
};

QStandardItem *addRow(QStandardItemModel &model, const Row &row)
{
    auto *item = new QStandardItem(row.subject);
    item->setData(row.fetched, Imap::Mailbox::RoleIsFetched);
    item->setData(row.subject, Imap::Mailbox::RoleMessageSubject);
    item->setData(row.from, Imap::Mailbox::RoleMessageFrom);
    item->setData(row.messageId, Imap::Mailbox::RoleMessageMessageId);
    item->setData(QVariant::fromValue(row.inReplyTo), Imap::Mailbox::RoleMessageInReplyTo);
    item->setData(QVariant::fromValue(row.references), Imap::Mailbox::RoleMessageHeaderReferences);
    item->setData(row.date, Imap::Mailbox::RoleMessageDate);
    model.appendRow(item);
    return item;
}

QStringList subjects(const QList<QPersistentModelIndex> &messages)
{
    QStringList out;
    for (const auto &index : messages)
        out << index.data(Imap::Mailbox::RoleMessageSubject).toString();
    return out;
}

QList<QPersistentModelIndex> allRows(const QStandardItemModel &model)
{
    QList<QPersistentModelIndex> out;
    for (int row = 0; row < model.rowCount(); ++row)
        out << QPersistentModelIndex(model.index(row, 0));
    return out;
}

const QString OR = QStringLiteral("OR");
const QString HEADER = QStringLiteral("HEADER");

}

void RelatedSearchTest::buildsNothingWithoutData()
{
    QVERIFY(UiUtils::relatedSearchConditions(QModelIndex()).isEmpty());
    QStandardItemModel model;
    QStandardItem *empty = addRow(model, {QStringLiteral("Nothing"), QVariant(), {}, {}, {}});
    QVERIFY(UiUtils::relatedSearchConditions(empty->index()).isEmpty());
}

void RelatedSearchTest::searchesTheSenderAlone()
{
    QStandardItemModel model;
    QStandardItem *item = addRow(model, {QStringLiteral("Hello"), address(QStringLiteral("Ann"), QStringLiteral("Example.org")), {}, {}, {}});
    // A single key needs no OR
    QCOMPARE(UiUtils::relatedSearchConditions(item->index()), QStringList({QStringLiteral("FROM"), QStringLiteral("ann@example.org")}));
}

void RelatedSearchTest::searchesThreadAndSender()
{
    QStandardItemModel model;
    QStandardItem *item = addRow(model, {QStringLiteral("Re: Plans"), address(QStringLiteral("ann")), "<own@example.org>",
                                         {"<parent@example.org>"}, {"<root@example.org>", "<parent@example.org>"}});
    const QStringList expected{
        OR, OR, OR, OR, OR,
        HEADER, QStringLiteral("Message-ID"), QStringLiteral("root@example.org"),
        HEADER, QStringLiteral("Message-ID"), QStringLiteral("parent@example.org"),
        HEADER, QStringLiteral("References"), QStringLiteral("root@example.org"),
        HEADER, QStringLiteral("In-Reply-To"), QStringLiteral("own@example.org"),
        HEADER, QStringLiteral("References"), QStringLiteral("own@example.org"),
        QStringLiteral("FROM"), QStringLiteral("ann@example.org"),
    };
    QCOMPARE(UiUtils::relatedSearchConditions(item->index()), expected);
}

void RelatedSearchTest::stripsBracketsAndDuplicates()
{
    QStandardItemModel model;
    // The parent is named twice, once without brackets, and the message names itself among its references
    QStandardItem *item = addRow(model, {QStringLiteral("Re: Plans"), QVariant(), " <own@example.org> ",
                                         {"parent@example.org"}, {"<parent@example.org>", "<own@example.org>"}});
    const QStringList expected{
        OR, OR, OR,
        HEADER, QStringLiteral("Message-ID"), QStringLiteral("parent@example.org"),
        HEADER, QStringLiteral("References"), QStringLiteral("parent@example.org"),
        HEADER, QStringLiteral("In-Reply-To"), QStringLiteral("own@example.org"),
        HEADER, QStringLiteral("References"), QStringLiteral("own@example.org"),
    };
    QCOMPARE(UiUtils::relatedSearchConditions(item->index()), expected);
}

void RelatedSearchTest::limitsReferences()
{
    QStandardItemModel model;
    QList<QByteArray> references;
    for (int i = 0; i < 6; ++i)
        references << QByteArray("<ref") + QByteArray::number(i) + QByteArray("@example.org>");
    QStandardItem *item = addRow(model, {QStringLiteral("Re: Long"), QVariant(), {}, {}, references});
    const QStringList conditions = UiUtils::relatedSearchConditions(item->index(), 2);
    // The last two references, then the first one as the root of the thread
    const QStringList expected{
        OR, OR,
        HEADER, QStringLiteral("Message-ID"), QStringLiteral("ref4@example.org"),
        HEADER, QStringLiteral("Message-ID"), QStringLiteral("ref5@example.org"),
        HEADER, QStringLiteral("References"), QStringLiteral("ref0@example.org"),
    };
    QCOMPARE(conditions, expected);
}

void RelatedSearchTest::classifiesConversationAndSender()
{
    const QDateTime day(QDate(2026, 11, 2), QTime(9, 0), QTimeZone::UTC);
    QStandardItemModel model;
    addRow(model, {QStringLiteral("Plans"), address(QStringLiteral("bob")), "<root@example.org>", {}, {}, day});
    addRow(model, {QStringLiteral("Something new"), address(QStringLiteral("carol")), "<reply@example.org>",
                   {"<own@example.org>"}, {"<root@example.org>", "<own@example.org>"}, day.addDays(2)});
    addRow(model, {QStringLiteral("Lunch"), address(QStringLiteral("ann")), "<lunch@example.org>", {}, {}, day.addDays(3)});
    QStandardItemModel current;
    QStandardItem *own = addRow(current, {QStringLiteral("Re: Plans"), address(QStringLiteral("ann")), "<own@example.org>",
                                          {"<root@example.org>"}, {"<root@example.org>"}, day.addDays(1)});
    QList<QPersistentModelIndex> found = allRows(model);
    // The current message comes back from the server too, and is left out
    found << QPersistentModelIndex(own->index());

    const auto related = UiUtils::classifyRelated(own->index(), found);
    QCOMPARE(subjects(related.conversation), QStringList({QStringLiteral("Something new"), QStringLiteral("Plans")}));
    QCOMPARE(subjects(related.sameSender), QStringList({QStringLiteral("Lunch")}));
    QVERIFY(!related.truncated);
}

void RelatedSearchTest::dropsSubstringMatches()
{
    QStandardItemModel model;
    // FROM and HEADER match substrings, so these come back from the server without being related
    addRow(model, {QStringLiteral("Other"), address(QStringLiteral("joann")), "<x@example.org>", {}, {}});
    addRow(model, {QStringLiteral("Unfetched"), address(QStringLiteral("ann")), {}, {}, {}, QDateTime(), false});
    QStandardItemModel current;
    QStandardItem *own = addRow(current, {QStringLiteral("Hello"), address(QStringLiteral("ann")), "<own@example.org>", {}, {}});

    const auto related = UiUtils::classifyRelated(own->index(), allRows(model));
    QVERIFY(related.conversation.isEmpty());
    QVERIFY(related.sameSender.isEmpty());

    QStandardItem *unfetched = addRow(current, {QStringLiteral("Later"), address(QStringLiteral("ann")), {}, {}, {}, QDateTime(), false});
    QVERIFY(UiUtils::classifyRelated(unfetched->index(), allRows(model)).sameSender.isEmpty());
}

void RelatedSearchTest::sortsAndLimitsGroups()
{
    const QDateTime day(QDate(2026, 11, 2), QTime(9, 0), QTimeZone::UTC);
    QStandardItemModel model;
    for (int i = 1; i <= 4; ++i)
        addRow(model, {QStringLiteral("Note %1").arg(i), address(QStringLiteral("ann")), QByteArray("<n") + QByteArray::number(i) + "@example.org>",
                       {}, {}, day.addDays(i)});
    QStandardItemModel current;
    QStandardItem *own = addRow(current, {QStringLiteral("Hello"), address(QStringLiteral("ann")), "<own@example.org>", {}, {}, day});

    const auto related = UiUtils::classifyRelated(own->index(), allRows(model), 2);
    QCOMPARE(subjects(related.sameSender), QStringList({QStringLiteral("Note 4"), QStringLiteral("Note 3")}));
    QVERIFY(related.truncated);
}

void RelatedSearchTest::refusesUnloadedMessages()
{
    UiUtils::RelatedSearch search;
    QSignalSpy changed(&search, &UiUtils::RelatedSearch::changed);
    QVERIFY(!search.start(QModelIndex(), UiUtils::RelatedSearch::Scope::MessageFolder));
    QVERIFY(!search.problem().isEmpty());
    QVERIFY(!search.isRunning());
    QCOMPARE(changed.count(), 1);

    // Only messages of an IMAP model can be searched for
    QStandardItemModel model;
    QStandardItem *item = addRow(model, {QStringLiteral("Hello"), address(QStringLiteral("ann")), "<own@example.org>", {}, {}});
    QVERIFY(!search.start(item->index(), UiUtils::RelatedSearch::Scope::AllFolders));
    QVERIFY(!search.problem().isEmpty());
    QVERIFY(search.results().conversation.isEmpty());
    QVERIFY(search.failures().isEmpty());
    QCOMPARE(search.folderCount(), 0);
    QVERIFY(!search.isTruncated());
}

QTEST_GUILESS_MAIN(RelatedSearchTest)
