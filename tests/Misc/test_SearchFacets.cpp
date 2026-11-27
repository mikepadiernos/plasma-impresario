/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QStandardItemModel>
#include <QtTest>
#include "test_SearchFacets.h"
#include "Imap/Model/ItemRoles.h"
#include "UiUtils/SearchFacets.h"

using UiUtils::SearchFacets;
using Kind = UiUtils::SearchFacets::Kind;

namespace {

const QDate today(2026, 11, 26);

struct Message {
    QString from = QStringLiteral("ann");
    QDate date = today;
    bool attachments = false;
    QStringList flags;
    bool fetched = true;
    uint uid = 1;
    QString accountId;
    QString accountName;
};

/** @short A folder of a message list as the threading model presents it: mailbox, list, messages */
class Folder
{
public:
    explicit Folder(const QString &mailbox)
    {
        auto *box = new QStandardItem(mailbox);
        box->setData(mailbox, Imap::Mailbox::RoleMailboxName);
        m_list = new QStandardItem();
        box->appendRow(m_list);
        m_model.appendRow(box);
    }

    QModelIndex add(const Message &message)
    {
        using namespace Imap::Mailbox;
        auto *item = new QStandardItem();
        item->setData(message.fetched, RoleIsFetched);
        item->setData(message.uid, RoleMessageUid);
        item->setData(message.flags, RoleMessageFlags);
        if (message.fetched) {
            item->setData(QVariantList{QStringList{message.from.toUpper(), QString(), message.from, QStringLiteral("example.org")}},
                          RoleMessageFrom);
            item->setData(QDateTime(message.date, QTime(12, 0)), RoleMessageDate);
            item->setData(message.attachments, RoleMessageHasAttachments);
        }
        if (!message.accountId.isEmpty()) {
            item->setData(message.accountId, RoleMessageAccountId);
            item->setData(message.accountName, RoleMessageAccountName);
        }
        m_list->appendRow(item);
        return item->index();
    }

private:
    QStandardItemModel m_model;
    QStandardItem *m_list;
};

SearchFacets::Context context()
{
    return {QStringLiteral("work"), QStringLiteral("Work"), today};
}

QList<SearchFacets::Value> values(const QModelIndex &message, Kind kind)
{
    return SearchFacets::valuesOf(message, kind, context());
}

QStringList keys(const QList<SearchFacets::Value> &values)
{
    QStringList out;
    for (const auto &value : values)
        out << value.key;
    return out;
}

QString dateKey(const QDate &date)
{
    Folder folder(QStringLiteral("INBOX"));
    return values(folder.add({QStringLiteral("ann"), date}), Kind::Date).value(0).key;
}

QStringList counted(const QList<SearchFacets::Count> &counts)
{
    QStringList out;
    for (const auto &count : counts)
        out << QStringLiteral("%1=%2").arg(count.value.key).arg(count.count);
    return out;
}

}

void SearchFacetsTest::initTestCase()
{
    // Weeks start on Monday
    QLocale::setDefault(QLocale::c());
}

void SearchFacetsTest::readsAccountAndFolder()
{
    Folder folder(QStringLiteral("INBOX"));
    const QModelIndex own = folder.add({});
    QCOMPARE(keys(values(own, Kind::Account)), QStringList{QStringLiteral("work")});
    QCOMPARE(values(own, Kind::Account).value(0).label, QStringLiteral("Work"));
    // Messages of the Unified Inbox and of results across accounts name their own
    Message other;
    other.accountId = QStringLiteral("home");
    other.accountName = QStringLiteral("Home");
    const QModelIndex named = folder.add(other);
    QCOMPARE(keys(values(named, Kind::Account)), QStringList{QStringLiteral("home")});
    QCOMPARE(values(named, Kind::Account).value(0).label, QStringLiteral("Home"));
    QVERIFY(SearchFacets::valuesOf(own, Kind::Account, {}).isEmpty());

    QCOMPARE(keys(values(own, Kind::Folder)), QStringList{QStringLiteral("work\nINBOX")});
    QCOMPARE(values(own, Kind::Folder).value(0).label, QStringLiteral("Inbox"));
    QCOMPARE(keys(values(named, Kind::Folder)), QStringList{QStringLiteral("home\nINBOX")});
    Folder archive(QStringLiteral("Archive/2026"));
    QCOMPARE(values(archive.add({}), Kind::Folder).value(0).label, QStringLiteral("Archive/2026"));
}

void SearchFacetsTest::readsSenderDateAndAttachments()
{
    Folder folder(QStringLiteral("INBOX"));
    Message message;
    message.attachments = true;
    const QModelIndex index = folder.add(message);
    QCOMPARE(keys(values(index, Kind::Sender)), QStringList{QStringLiteral("ann@example.org")});
    QCOMPARE(values(index, Kind::Sender).value(0).label, QStringLiteral("ANN <ann@example.org>"));
    QCOMPARE(keys(values(index, Kind::Date)), QStringList{QStringLiteral("today")});
    QCOMPARE(keys(values(index, Kind::Attachments)), QStringList{QStringLiteral("with")});
    QCOMPARE(keys(values(folder.add({}), Kind::Attachments)), QStringList{QStringLiteral("without")});

    Message nobody;
    nobody.from.clear();
    const QModelIndex unknown = folder.add(nobody);
    QCOMPARE(keys(values(unknown, Kind::Sender)), QStringList{QString()});
    QVERIFY(!values(unknown, Kind::Sender).value(0).label.isEmpty());
    Message undated;
    undated.date = QDate();
    QCOMPARE(keys(values(folder.add(undated), Kind::Date)), QStringList{QStringLiteral("none")});
}

void SearchFacetsTest::groupsDates()
{
    // Thursday; the week started on Monday the 23rd
    QCOMPARE(dateKey(today.addDays(1)), QStringLiteral("future"));
    QCOMPARE(dateKey(today), QStringLiteral("today"));
    QCOMPARE(dateKey(today.addDays(-1)), QStringLiteral("yesterday"));
    QCOMPARE(dateKey(QDate(2026, 11, 23)), QStringLiteral("thisWeek"));
    QCOMPARE(dateKey(QDate(2026, 11, 22)), QStringLiteral("lastWeek"));
    QCOMPARE(dateKey(QDate(2026, 11, 16)), QStringLiteral("lastWeek"));
    QCOMPARE(dateKey(QDate(2026, 11, 15)), QStringLiteral("thisMonth"));
    QCOMPARE(dateKey(QDate(2026, 3, 1)), QStringLiteral("thisYear"));
    QCOMPARE(dateKey(QDate(2025, 12, 31)), QStringLiteral("older"));
}

void SearchFacetsTest::readsTagsWithoutSystemFlags()
{
    Folder folder(QStringLiteral("INBOX"));
    Message message;
    message.flags = QStringList{QStringLiteral("\\Seen"), QStringLiteral("$Forwarded"), QStringLiteral("Project"),
                                QStringLiteral("project"), QStringLiteral("$label1"), QStringLiteral("Junk")};
    const QList<SearchFacets::Value> tags = values(folder.add(message), Kind::Tag);
    QCOMPARE(keys(tags), (QStringList{QStringLiteral("project"), QStringLiteral("$label1")}));
    QCOMPARE(tags.value(0).label, QStringLiteral("Project"));
}

void SearchFacetsTest::unloadedMessagesHaveOnlyKnownValues()
{
    Folder folder(QStringLiteral("INBOX"));
    Message message;
    message.fetched = false;
    message.flags = QStringList{QStringLiteral("Project")};
    const QModelIndex index = folder.add(message);
    QCOMPARE(keys(values(index, Kind::Account)), QStringList{QStringLiteral("work")});
    QCOMPARE(keys(values(index, Kind::Folder)), QStringList{QStringLiteral("work\nINBOX")});
    QCOMPARE(keys(values(index, Kind::Tag)), QStringList{QStringLiteral("project")});
    QVERIFY(values(index, Kind::Sender).isEmpty());
    QVERIFY(values(index, Kind::Date).isEmpty());
    QVERIFY(values(index, Kind::Attachments).isEmpty());

    // Not kept by a facet whose value is not known yet
    SearchFacets facets;
    facets.setContext(context());
    facets.toggle({Kind::Attachments, QStringLiteral("without"), QString()});
    QVERIFY(!facets.matches(index));
}

void SearchFacetsTest::placeholdersHaveNoValues()
{
    Folder folder(QStringLiteral("INBOX"));
    Message missing;
    missing.uid = 0;
    const QModelIndex index = folder.add(missing);
    for (const Kind kind : SearchFacets::kinds())
        QVERIFY(values(index, kind).isEmpty());
    QVERIFY(values(QModelIndex(), Kind::Account).isEmpty());
}

void SearchFacetsTest::choosesAndRemovesValues()
{
    SearchFacets facets;
    QVERIFY(facets.isEmpty());
    const SearchFacets::Value ann{Kind::Sender, QStringLiteral("ann@example.org"), QStringLiteral("Ann")};
    const SearchFacets::Value today{Kind::Date, QStringLiteral("today"), QStringLiteral("Today")};
    facets.toggle(ann);
    facets.toggle(today);
    QVERIFY(facets.contains(ann));
    QCOMPARE(facets.chosen().size(), 2);
    QCOMPARE(facets.chosen().constFirst(), ann);
    facets.toggle(ann);
    QVERIFY(!facets.contains(ann));
    // The label does not tell values apart
    facets.toggle({Kind::Sender, QStringLiteral("ann@example.org"), QStringLiteral("Someone else")});
    QVERIFY(facets.contains(ann));
    facets.remove(ann);
    QCOMPARE(facets.chosen(), QList<SearchFacets::Value>{today});

    SearchFacets other;
    other.setContext(context());
    QVERIFY(facets != other);
    other.toggle(today);
    QVERIFY(facets == other);
    facets.clear();
    QVERIFY(facets.isEmpty());
}

void SearchFacetsTest::combinesKinds()
{
    Folder folder(QStringLiteral("INBOX"));
    const QModelIndex ann = folder.add({QStringLiteral("ann"), today, true});
    const QModelIndex bob = folder.add({QStringLiteral("bob"), today, false});
    const QModelIndex old = folder.add({QStringLiteral("ann"), QDate(2020, 1, 1), false});
    SearchFacets facets;
    facets.setContext(context());
    QVERIFY(facets.matches(ann));

    facets.toggle({Kind::Sender, QStringLiteral("ann@example.org"), QString()});
    QVERIFY(facets.matches(ann));
    QVERIFY(!facets.matches(bob));
    QVERIFY(facets.matches(old));
    // Values of one kind are alternatives
    facets.toggle({Kind::Sender, QStringLiteral("bob@example.org"), QString()});
    QVERIFY(facets.matches(bob));
    // Kinds are combined
    facets.toggle({Kind::Date, QStringLiteral("today"), QString()});
    QVERIFY(facets.matches(ann));
    QVERIFY(facets.matches(bob));
    QVERIFY(!facets.matches(old));
    facets.toggle({Kind::Attachments, QStringLiteral("with"), QString()});
    QVERIFY(facets.matches(ann));
    QVERIFY(!facets.matches(bob));
}

void SearchFacetsTest::countsAmongOtherKinds()
{
    Folder folder(QStringLiteral("INBOX"));
    const QModelIndexList messages{
        folder.add({QStringLiteral("ann"), today, true}),
        folder.add({QStringLiteral("ann"), today.addDays(-1), false}),
        folder.add({QStringLiteral("bob"), today, false}),
        folder.add({QStringLiteral("cid"), QDate(2020, 1, 1), false}),
    };
    SearchFacets facets;
    facets.setContext(context());
    QCOMPARE(counted(facets.counts(messages, Kind::Sender)),
             (QStringList{QStringLiteral("ann@example.org=2"), QStringLiteral("bob@example.org=1"), QStringLiteral("cid@example.org=1")}));

    facets.toggle({Kind::Date, QStringLiteral("today"), QString()});
    // Senders are counted among the messages of today; the dates themselves among all of them
    QCOMPARE(counted(facets.counts(messages, Kind::Sender)),
             (QStringList{QStringLiteral("ann@example.org=1"), QStringLiteral("bob@example.org=1")}));
    QCOMPARE(counted(facets.counts(messages, Kind::Date)),
             (QStringList{QStringLiteral("today=2"), QStringLiteral("yesterday=1"), QStringLiteral("older=1")}));

    // A chosen value stays offered, to be removed, even when no message has it any more
    facets.toggle({Kind::Sender, QStringLiteral("dan@example.org"), QStringLiteral("Dan")});
    const QList<SearchFacets::Count> senders = facets.counts(messages, Kind::Sender);
    QCOMPARE(counted(senders), (QStringList{QStringLiteral("ann@example.org=1"), QStringLiteral("bob@example.org=1"),
                                            QStringLiteral("dan@example.org=0")}));
    // No message is from Dan, so no account has any
    QVERIFY(facets.counts(messages, Kind::Account).isEmpty());
}

void SearchFacetsTest::ordersCounts()
{
    Folder folder(QStringLiteral("INBOX"));
    const QModelIndexList messages{
        folder.add({QStringLiteral("bob"), QDate(2020, 1, 1), false}),
        folder.add({QStringLiteral("bob"), today, false}),
        folder.add({QStringLiteral("ann"), today.addDays(2), true}),
        folder.add({QStringLiteral("cid"), today.addDays(-1), false}),
    };
    SearchFacets facets;
    facets.setContext(context());
    // The most messages first, then by name
    QCOMPARE(counted(facets.counts(messages, Kind::Sender)),
             (QStringList{QStringLiteral("bob@example.org=2"), QStringLiteral("ann@example.org=1"), QStringLiteral("cid@example.org=1")}));
    // Newest first
    QCOMPARE(counted(facets.counts(messages, Kind::Date)),
             (QStringList{QStringLiteral("future=1"), QStringLiteral("today=1"), QStringLiteral("yesterday=1"),
                          QStringLiteral("older=1")}));
    // With attachments first, however few
    QCOMPARE(counted(facets.counts(messages, Kind::Attachments)),
             (QStringList{QStringLiteral("with=1"), QStringLiteral("without=3")}));
}

QTEST_GUILESS_MAIN(SearchFacetsTest)
