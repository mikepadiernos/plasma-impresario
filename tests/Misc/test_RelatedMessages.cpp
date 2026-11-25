/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QStandardItemModel>
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

QTEST_GUILESS_MAIN(RelatedMessagesTest)
