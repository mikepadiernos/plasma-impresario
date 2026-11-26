/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTimeZone>
#include <QtTest>
#include "test_RelatedMessages.h"
#include "Imap/Model/ItemRoles.h"
#include "UiUtils/RelatedMessages.h"

namespace {

QVariant address(const QString &mailbox, const QString &host)
{
    return QVariantList{QStringList{QStringLiteral("Someone"), QString(), mailbox, host}};
}

struct Row {
    QString subject;
    QString from;
    QByteArray messageId;
    QList<QByteArray> inReplyTo;
    bool fetched = true;
};

QStandardItem *addRow(QStandardItemModel &model, const Row &row)
{
    auto *item = new QStandardItem(row.subject);
    item->setData(row.fetched, Imap::Mailbox::RoleIsFetched);
    item->setData(row.subject, Imap::Mailbox::RoleMessageSubject);
    item->setData(address(row.from, QStringLiteral("example.org")), Imap::Mailbox::RoleMessageFrom);
    item->setData(row.messageId, Imap::Mailbox::RoleMessageMessageId);
    item->setData(QVariant::fromValue(row.inReplyTo), Imap::Mailbox::RoleMessageInReplyTo);
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

}

void RelatedMessagesTest::normalizesSubjects()
{
    QCOMPARE(UiUtils::normalizedSubject(QStringLiteral("Re: Fwd: AW:  Hello   World")), QStringLiteral("hello world"));
    QCOMPARE(UiUtils::normalizedSubject(QStringLiteral("RE[2]: hello world")), QStringLiteral("hello world"));
    QCOMPARE(UiUtils::normalizedSubject(QStringLiteral("Report")), QStringLiteral("report"));
    QVERIFY(UiUtils::normalizedSubject(QStringLiteral("Re: ")).isEmpty());
}

void RelatedMessagesTest::readsFirstAddress()
{
    QCOMPARE(UiUtils::firstAddress(address(QStringLiteral("Ann"), QStringLiteral("Example.ORG"))), QStringLiteral("ann@example.org"));
    QVERIFY(UiUtils::firstAddress(QVariant()).isEmpty());
    QVERIFY(UiUtils::firstAddress(QVariantList{QStringList{QStringLiteral("group")}}).isEmpty());
}

void RelatedMessagesTest::needsFetchedCurrentMessage()
{
    QStandardItemModel model;
    addRow(model, {QStringLiteral("Plans"), QStringLiteral("ann"), {}, {}});
    QStandardItem *current = addRow(model, {QStringLiteral("Plans"), QStringLiteral("ann"), {}, {}, false});
    const auto related = UiUtils::collectRelatedMessages(&model, current->index());
    QVERIFY(related.conversation.isEmpty());
    QVERIFY(related.sameSender.isEmpty());
    QVERIFY(UiUtils::collectRelatedMessages(nullptr, current->index()).conversation.isEmpty());
}

void RelatedMessagesTest::groupsConversationAndSender()
{
    QStandardItemModel model;
    addRow(model, {QStringLiteral("Plans"), QStringLiteral("ann"), {}, {}});
    addRow(model, {QStringLiteral("Lunch"), QStringLiteral("ann"), {}, {}});
    addRow(model, {QStringLiteral("Re: Plans"), QStringLiteral("bob"), {}, {}});
    addRow(model, {QStringLiteral("Unrelated"), QStringLiteral("carol"), {}, {}});
    addRow(model, {QStringLiteral("Invoice"), QStringLiteral("ann"), {}, {}});
    QStandardItem *current = addRow(model, {QStringLiteral("Fwd: plans"), QStringLiteral("ann"), {}, {}});

    const auto related = UiUtils::collectRelatedMessages(&model, current->index());
    // Newest rows come first and the current message is skipped
    QCOMPARE(subjects(related.conversation), QStringList({QStringLiteral("Re: Plans"), QStringLiteral("Plans")}));
    QCOMPARE(subjects(related.sameSender), QStringList({QStringLiteral("Invoice"), QStringLiteral("Lunch")}));
    QVERIFY(!related.truncated);
}

void RelatedMessagesTest::linksThroughMessageIds()
{
    QStandardItemModel model;
    addRow(model, {QStringLiteral("Original"), QStringLiteral("bob"), "<parent@example.org>", {}});
    addRow(model, {QStringLiteral("Changed subject"), QStringLiteral("carol"), "<child@example.org>", {"<current@example.org>"}});
    addRow(model, {QStringLiteral("Something else"), QStringLiteral("dave"), "<other@example.org>", {}});
    QStandardItem *current = addRow(model, {QStringLiteral("New topic"), QStringLiteral("ann"), "<current@example.org>",
                                            {"<parent@example.org>"}});

    const auto related = UiUtils::collectRelatedMessages(&model, current->index());
    QCOMPARE(subjects(related.conversation), QStringList({QStringLiteral("Changed subject"), QStringLiteral("Original")}));
    QVERIFY(related.sameSender.isEmpty());
}

void RelatedMessagesTest::skipsUnfetchedRows()
{
    QStandardItemModel model;
    addRow(model, {QStringLiteral("Plans"), QStringLiteral("ann"), {}, {}, false});
    addRow(model, {QStringLiteral("Re: Plans"), QStringLiteral("bob"), {}, {}});
    QStandardItem *current = addRow(model, {QStringLiteral("Plans"), QStringLiteral("ann"), {}, {}});

    const auto related = UiUtils::collectRelatedMessages(&model, current->index());
    QCOMPARE(subjects(related.conversation), QStringList({QStringLiteral("Re: Plans")}));
}

void RelatedMessagesTest::honoursLimits()
{
    QStandardItemModel model;
    for (int i = 0; i < 5; ++i)
        addRow(model, {QStringLiteral("Plans"), QStringLiteral("bob"), {}, {}});
    QStandardItem *current = addRow(model, {QStringLiteral("Plans"), QStringLiteral("ann"), {}, {}});

    auto related = UiUtils::collectRelatedMessages(&model, current->index(), 3);
    QCOMPARE(related.conversation.size(), 3);
    QVERIFY(related.truncated);

    related = UiUtils::collectRelatedMessages(&model, current->index(), 25, 3);
    QCOMPARE(related.conversation.size(), 2);
    QVERIFY(related.truncated);
}

void RelatedMessagesTest::followsTheWholeConversation()
{
    QStandardItemModel model;
    QStandardItem *plans = addRow(model, {QStringLiteral("Plans"), QStringLiteral("ann"), "<plans@example.org>", {}});
    QStandardItem *reply = addRow(model, {QStringLiteral("Re: Plans"), QStringLiteral("bob"), "<reply@example.org>", {}});
    // Linked to the first message only, with a subject of its own, and a reply to it with yet another subject
    QStandardItem *budget = addRow(model, {QStringLiteral("Budget"), QStringLiteral("carol"), "<budget@example.org>",
                                           {"<plans@example.org>"}});
    QStandardItem *numbers = addRow(model, {QStringLiteral("Numbers"), QStringLiteral("ann"), {}, {"<budget@example.org>"}});
    addRow(model, {QStringLiteral("Unrelated"), QStringLiteral("ann"), "<other@example.org>", {"<missing@example.org>"}});
    addRow(model, {QStringLiteral("Budget"), QStringLiteral("dave"), {}, {}, false});

    // The related messages panel only sees direct links, so the reply does not reach the budget
    QCOMPARE(subjects(UiUtils::collectRelatedMessages(&model, reply->index()).conversation), QStringList{QStringLiteral("Plans")});
    for (QStandardItem *from : {plans, reply, budget, numbers}) {
        QStringList found = subjects(UiUtils::collectConversation(&model, from->index()));
        found << from->text();
        found.sort();
        QCOMPARE(found, QStringList({QStringLiteral("Budget"), QStringLiteral("Numbers"), QStringLiteral("Plans"),
                                     QStringLiteral("Re: Plans")}));
    }
    QCOMPARE(UiUtils::collectConversation(&model, reply->index(), 2).size(), 2);
    QVERIFY(UiUtils::collectConversation(nullptr, reply->index()).isEmpty());
}

namespace {

QStandardItem *message(const QString &subject, uint uid, const QDateTime &date = QDateTime())
{
    auto *item = new QStandardItem(subject);
    item->setData(subject, Imap::Mailbox::RoleMessageSubject);
    item->setData(uid, Imap::Mailbox::RoleMessageUid);
    item->setData(date, Imap::Mailbox::RoleMessageDate);
    return item;
}

QString subjectOf(const QModelIndex &index)
{
    return index.isValid() ? index.data(Imap::Mailbox::RoleMessageSubject).toString() : QStringLiteral("(none)");
}

}

void RelatedMessagesTest::stepsThroughThreads()
{
    const QDateTime day(QDate(2026, 11, 2), QTime(9, 0), QTimeZone::UTC);
    QStandardItemModel model;
    // A thread whose first message is missing, and a thread of one message. The nested reply is the newest, so it
    // comes last although it is shown above the second reply.
    QStandardItem *missing = message(QStringLiteral("missing"), 0);
    QStandardItem *first = message(QStringLiteral("first"), 1, day);
    QStandardItem *nested = message(QStringLiteral("nested"), 2, day.addDays(2));
    QStandardItem *second = message(QStringLiteral("second"), 3, day.addDays(1));
    first->appendRow(nested);
    missing->appendRow(first);
    missing->appendRow(second);
    model.appendRow(missing);
    QStandardItem *alone = message(QStringLiteral("alone"), 4);
    model.appendRow(alone);
    // A group header is not part of the thread below it
    auto *header = new QStandardItem(QStringLiteral("Today"));
    header->setData(true, Imap::Mailbox::RoleIsGroupHeader);
    QStandardItem *grouped = message(QStringLiteral("grouped"), 5);
    QStandardItem *groupedReply = message(QStringLiteral("grouped reply"), 6);
    grouped->appendRow(groupedReply);
    header->appendRow(grouped);
    model.appendRow(header);

    const auto step = [&model](QStandardItem *from, bool forward) {
        return subjectOf(UiUtils::adjacentInThread(&model, from->index(), forward));
    };
    QCOMPARE(step(first, true), QStringLiteral("second"));
    QCOMPARE(step(second, true), QStringLiteral("nested"));
    QCOMPARE(step(nested, true), QStringLiteral("(none)"));
    QCOMPARE(step(nested, false), QStringLiteral("second"));
    // The missing parent is not a message to open
    QCOMPARE(step(first, false), QStringLiteral("(none)"));
    QCOMPARE(step(alone, true), QStringLiteral("(none)"));
    QCOMPARE(step(alone, false), QStringLiteral("(none)"));
    QCOMPARE(step(groupedReply, false), QStringLiteral("grouped"));
    QCOMPARE(step(grouped, false), QStringLiteral("(none)"));
    QCOMPARE(step(header, true), QStringLiteral("(none)"));
    QVERIFY(!UiUtils::adjacentInThread(nullptr, first->index(), true).isValid());
    QVERIFY(!UiUtils::adjacentInThread(&model, QModelIndex(), true).isValid());
    // An index of another model is not a row of the list
    QStandardItemModel other;
    other.appendRow(message(QStringLiteral("elsewhere"), 7));
    QVERIFY(!UiUtils::adjacentInThread(&model, other.index(0, 0), true).isValid());
}

void RelatedMessagesTest::stepsThroughConversationsByDate()
{
    const QDateTime day(QDate(2026, 11, 2), QTime(9, 0), QTimeZone::UTC);
    QStandardItemModel model;
    QStandardItem *reply = message(QStringLiteral("Re: Plans"), 1, day.addDays(2));
    QStandardItem *other = message(QStringLiteral("Other"), 2, day.addDays(1));
    QStandardItem *plans = message(QStringLiteral("Plans"), 3, day);
    QStandardItem *followUp = message(QStringLiteral("Re: Re: Plans"), 4, day.addDays(3));
    for (QStandardItem *item : {reply, other, plans, followUp})
        model.appendRow(item);
    // The list shows the messages through a filtering proxy, which hides the follow-up; the conversation and the
    // result are messages of the source
    QSortFilterProxyModel view;
    view.setSourceModel(&model);
    view.setFilterRegularExpression(QStringLiteral("^(Plans|Re: Plans|Other)$"));
    view.sort(0, Qt::DescendingOrder);
    QCOMPARE(view.rowCount(), 3);
    const auto conversationOf = [&](QStandardItem *from) {
        QList<QPersistentModelIndex> result;
        for (QStandardItem *item : {reply, followUp, plans}) {
            // A hidden message is known by its source index
            const QModelIndex shown = view.mapFromSource(item->index());
            if (item != from)
                result << (shown.isValid() ? shown : item->index());
        }
        return result;
    };

    const auto step = [&](QStandardItem *from, bool forward) {
        const QModelIndex at = view.mapFromSource(from->index());
        const QModelIndex target = UiUtils::adjacentByDate(at.isValid() ? at : from->index(), forward, conversationOf(from));
        Q_ASSERT(!target.isValid() || target.model() == &model);
        return subjectOf(target);
    };
    QCOMPARE(step(plans, true), QStringLiteral("Re: Plans"));
    QCOMPARE(step(reply, true), QStringLiteral("Re: Re: Plans"));
    QCOMPARE(step(followUp, true), QStringLiteral("(none)"));
    QCOMPARE(step(followUp, false), QStringLiteral("Re: Plans"));
    QCOMPARE(step(plans, false), QStringLiteral("(none)"));
    // A message outside the conversation has nowhere to go
    QCOMPARE(UiUtils::adjacentByDate(view.mapFromSource(other->index()), true, {}), QModelIndex());
    QCOMPARE(UiUtils::adjacentByDate(QModelIndex(), true, conversationOf(plans)), QModelIndex());
}

QTEST_GUILESS_MAIN(RelatedMessagesTest)
