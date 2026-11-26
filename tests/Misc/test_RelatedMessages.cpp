/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTimeZone>
#include <QtTest>
#include "test_RelatedMessages.h"
#include "Imap/Model/ItemRoles.h"
#include "UiUtils/Conversations.h"
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
    QDateTime date = QDateTime();
};

QStandardItem *addRow(QStandardItemModel &model, const Row &row)
{
    auto *item = new QStandardItem(row.subject);
    item->setData(row.fetched, Imap::Mailbox::RoleIsFetched);
    item->setData(row.subject, Imap::Mailbox::RoleMessageSubject);
    item->setData(address(row.from, QStringLiteral("example.org")), Imap::Mailbox::RoleMessageFrom);
    item->setData(row.messageId, Imap::Mailbox::RoleMessageMessageId);
    item->setData(QVariant::fromValue(row.inReplyTo), Imap::Mailbox::RoleMessageInReplyTo);
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
    const QDateTime day(QDate(2026, 11, 2), QTime(9, 0), QTimeZone::UTC);
    QStandardItemModel model;
    // Replies without links join the earlier message of their subject, so the current message starts the conversation
    for (int i = 1; i <= 5; ++i)
        addRow(model, {QStringLiteral("Re: Plans"), QStringLiteral("bob"), {}, {}, true, day.addDays(i)});
    QStandardItem *current = addRow(model, {QStringLiteral("Plans"), QStringLiteral("ann"), {}, {}, true, day});

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

    // The related messages panel shows the whole conversation too, last row first
    QCOMPARE(subjects(UiUtils::collectRelatedMessages(&model, reply->index()).conversation),
             QStringList({QStringLiteral("Numbers"), QStringLiteral("Budget"), QStringLiteral("Plans")}));
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

void RelatedMessagesTest::stripsLocalizedPrefixes()
{
    QCOMPARE(UiUtils::strippedSubject(QStringLiteral("AW: WG: Angebot"), true), QStringLiteral("Angebot"));
    QCOMPARE(UiUtils::strippedSubject(QStringLiteral("Re[2]: Re(3):Status"), true), QStringLiteral("Status"));
    QCOMPARE(UiUtils::strippedSubject(QStringLiteral("[team] Re: [team]  Status  update"), true),
             QStringLiteral("Status update"));
    QCOMPARE(UiUtils::strippedSubject(QStringLiteral("[team] Re: Status"), false), QStringLiteral("[team] Re: Status"));
    QCOMPARE(UiUtils::strippedSubject(QStringLiteral("回复：会议"), true), QStringLiteral("会议"));
    QCOMPARE(UiUtils::strippedSubject(QStringLiteral("SV: Vs: VB: Odp: Tilbud"), true), QStringLiteral("Tilbud"));
    // A word which merely starts like a prefix is kept
    QCOMPARE(UiUtils::strippedSubject(QStringLiteral("Report: done"), true), QStringLiteral("Report: done"));
    QCOMPARE(UiUtils::normalizedSubject(QStringLiteral("[team] AW: STATUS update")), QStringLiteral("status update"));

    QVERIFY(UiUtils::hasReplyPrefix(QStringLiteral("Re: Status")));
    QVERIFY(UiUtils::hasReplyPrefix(QStringLiteral("[team] Fwd: Status")));
    QVERIFY(UiUtils::hasReplyPrefix(QStringLiteral("re[2]:Status")));
    QVERIFY(UiUtils::hasReplyPrefix(QStringLiteral("回复: 会议")));
    QVERIFY(!UiUtils::hasReplyPrefix(QStringLiteral("[team] Status")));
    QVERIFY(!UiUtils::hasReplyPrefix(QStringLiteral("Report: done")));
}

void RelatedMessagesTest::groupsConversationsByLinksAndSubject()
{
    const QDateTime day(QDate(2026, 11, 2), QTime(9, 0), QTimeZone::UTC);
    const auto message = [](const QString &subject, const QDateTime &date, const QByteArray &id = {},
                            const QList<QByteArray> &inReplyTo = {}, const QList<QByteArray> &references = {}) {
        return UiUtils::ConversationMessage{subject, date, id, inReplyTo, references};
    };
    const QList<UiUtils::ConversationMessage> messages{
        /* 0 */ message(QStringLiteral("[team] Status update"), day),
        /* 1 */ message(QStringLiteral("AW: [team] Status update"), day.addDays(1)),
        /* 2 */ message(QStringLiteral("Re[2]: Status update"), day.addDays(3)),
        // Too long before the conversation, and nothing earlier to join
        /* 3 */ message(QStringLiteral("Re: Status update"), day.addDays(-40)),
        // Unrelated mail of the same subject without prefixes stays apart
        /* 4 */ message(QStringLiteral("Hello"), day),
        /* 5 */ message(QStringLiteral("hello"), day.addDays(1)),
        // A reply which links one message is not joined to another one by its subject
        /* 6 */ message(QStringLiteral("Minutes"), day, "<minutes@example.org>"),
        /* 7 */ message(QStringLiteral("Agenda"), day, "<agenda@example.org>"),
        /* 8 */ message(QStringLiteral("Re: Minutes and agenda"), day.addDays(1), "<both@example.org>", {},
                        {"<minutes@example.org>", "<agenda@example.org>"}),
        /* 9 */ message(QStringLiteral("Re: Hello"), day.addDays(2), {}, {"<elsewhere@example.org>"}),
        // Without a date, a reply joins the earliest message of its subject
        /* 10 */ message(QStringLiteral("Fwd: Hello"), QDateTime()),
    };
    const QList<qsizetype> groups = UiUtils::groupConversations(messages);
    QCOMPARE(groups.size(), messages.size());
    QCOMPARE(groups.at(1), groups.at(0));
    QCOMPARE(groups.at(2), groups.at(0));
    QVERIFY(groups.at(3) != groups.at(0));
    QVERIFY(groups.at(4) != groups.at(5));
    QCOMPARE(groups.at(7), groups.at(6));
    QCOMPARE(groups.at(8), groups.at(6));
    QVERIFY(groups.at(9) != groups.at(4) && groups.at(9) != groups.at(5));
    QVERIFY(groups.at(10) == groups.at(4) || groups.at(10) == groups.at(5));
    // Each group is known by its first message
    QCOMPARE(groups.at(0), qsizetype(0));
    QCOMPARE(groups.at(8), qsizetype(6));

    // A wider window takes in the old reply
    QCOMPARE(UiUtils::groupConversations(messages, 60).at(3), qsizetype(0));
    QVERIFY(UiUtils::groupConversations({}).isEmpty());
}

namespace {

QString describeThreads(const QVector<Imap::Responses::ThreadingNode> &threads)
{
    QStringList out;
    for (const auto &node : threads) {
        QString text = QString::number(node.num);
        if (!node.children.isEmpty())
            text += QLatin1Char('(') + describeThreads(node.children) + QLatin1Char(')');
        out << text;
    }
    return out.join(QLatin1Char(' '));
}

}

void RelatedMessagesTest::mergesConversationThreads()
{
    using Imap::Responses::ThreadingNode;
    const QDateTime day(QDate(2026, 11, 2), QTime(9, 0), QTimeZone::UTC);
    QHash<uint, UiUtils::ConversationMessage> loaded{
        {1, {QStringLiteral("Status"), day, "<1@example.org>", {}, {}}},
        {2, {QStringLiteral("Re: Status"), day.addDays(1), "<2@example.org>", {"<1@example.org>"}, {}}},
        {3, {QStringLiteral("Hello"), day, "<3@example.org>", {}, {}}},
        // A reply without links, which the server put in a thread of its own
        {4, {QStringLiteral("AW: Status"), day.addDays(2), "<4@example.org>", {}, {}}},
        {5, {QStringLiteral("Hello"), day.addDays(1), "<5@example.org>", {}, {}}},
    };
    const auto describe = [&loaded](uint uid) -> std::optional<UiUtils::ConversationMessage> {
        const auto it = loaded.constFind(uid);
        return it == loaded.constEnd() ? std::nullopt : std::optional(*it);
    };
    // The server's threads: 1 with its reply 2, below a missing parent; 3; 4; 5; 6, which is not loaded
    const QVector<ThreadingNode> threads{
        ThreadingNode(4),
        ThreadingNode(0, {ThreadingNode(1, {ThreadingNode(2)})}),
        ThreadingNode(3),
        ThreadingNode(5),
        ThreadingNode(6),
    };
    QCOMPARE(describeThreads(UiUtils::mergeConversationThreads(threads, describe)), QStringLiteral("0(1(2) 4) 3 5 6"));

    // Nothing to merge gives the threads unchanged
    loaded.remove(4);
    QCOMPARE(UiUtils::mergeConversationThreads(threads, describe), threads);
    QCOMPARE(UiUtils::mergeConversationThreads({}, describe), QVector<ThreadingNode>());
}

QTEST_GUILESS_MAIN(RelatedMessagesTest)
