/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QDataStream>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>
#include <memory>

#include "Imap/Model/CombinedCache.h"
#include "Imap/Model/MailboxMetadata.h"
#include "Imap/Model/SearchIndex.h"
#include "Imap/Model/SearchIndexer.h"
#include "Imap/Parser/LowLevelParser.h"
#include "Imap/Parser/Message.h"
#include "UiUtils/Agenda.h"

using namespace UiUtils::Agenda;
using namespace Imap::Mailbox;

namespace {

const QDate today(2026, 11, 27);

QDateTime at(const QDate &day, int hour, int minute = 0)
{
    return QDateTime(day, QTime(hour, minute));
}

Item meeting(const QString &title, const QDateTime &start, const QDateTime &end, bool allDay = false)
{
    Item item;
    item.kind = Kind::Event;
    item.title = title;
    item.start = start;
    item.end = end;
    item.allDay = allDay;
    return item;
}

Item task(const QString &title, const QDateTime &due, bool allDay = false)
{
    Item item;
    item.kind = Kind::Task;
    item.title = title;
    item.start = due;
    item.allDay = allDay;
    return item;
}

Item mail(const QString &title, const QDate &sent, const QString &mailbox = QStringLiteral("INBOX"), uint uid = 1)
{
    Item item;
    item.kind = Kind::Message;
    item.title = title;
    item.sent = sent;
    item.accountId = QStringLiteral("work");
    item.mailbox = mailbox;
    item.uid = uid;
    return item;
}

QStringList titles(const QList<Item> &items)
{
    QStringList out;
    for (const Item &item : items)
        out << item.title;
    return out;
}

QByteArray plainStructure()
{
    int start = 0;
    const QVariantList list = Imap::LowLevelParser::parseList(
        '(', ')', "(\"text\" \"plain\" (\"charset\" \"utf-8\") NIL NIL \"8bit\" 40 1 NIL NIL NIL NIL)", start);
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_4_6);
    stream << list;
    return buffer;
}

AbstractCache::MessageDataBundle cachedMessage(uint uid, const QString &subject, const QDate &sent)
{
    AbstractCache::MessageDataBundle data;
    data.uid = uid;
    const QList<Imap::Message::MailAddress> sender{
        Imap::Message::MailAddress(QStringLiteral("Alice"), QString(), QStringLiteral("alice"), QStringLiteral("example.org"))};
    data.envelope = Imap::Message::Envelope(QDateTime(sent, QTime(9, 0), QTimeZone::utc()), subject, sender, sender, sender,
                                            sender, {}, {}, {}, QByteArray("<") + QByteArray::number(uid) + "@example.org>");
    data.internalDate = QDateTime(sent, QTime(10, 0), QTimeZone::utc());
    data.size = 1000;
    data.serializedBodyStructure = plainStructure();
    data.hdrListPostNo = false;
    return data;
}

SyncState syncState(uint uidValidity, uint exists, uint uidNext)
{
    SyncState state;
    state.setUidValidity(uidValidity);
    state.setExists(exists);
    state.setUidNext(uidNext);
    state.setRecent(0);
    state.setUnSeenCount(0);
    return state;
}

}

class AgendaTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void testSorted()
    {
        const QList<Item> items{
            mail(QStringLiteral("older mail"), QDate(2026, 11, 20)),
            task(QStringLiteral("due today"), QDateTime(today, QTime(0, 0)), true),
            meeting(QStringLiteral("afternoon"), at(today, 14), at(today, 15)),
            mail(QStringLiteral("newer mail"), QDate(2026, 11, 26)),
            task(QStringLiteral("due at three"), at(today, 15)),
            meeting(QStringLiteral("morning"), at(today, 9), at(today, 10)),
            task(QStringLiteral("overdue"), QDateTime(QDate(2026, 11, 25), QTime(0, 0)), true),
            mail(QStringLiteral("undated mail"), QDate()),
            meeting(QStringLiteral("holiday"), at(today, 0), at(today, 0), true),
            task(QStringLiteral("long overdue"), at(QDate(2026, 11, 1), 12)),
        };
        QCOMPARE(titles(sorted(items, today)),
                 (QStringList{QStringLiteral("holiday"), QStringLiteral("morning"), QStringLiteral("afternoon"),
                              QStringLiteral("long overdue"), QStringLiteral("overdue"), QStringLiteral("due at three"),
                              QStringLiteral("due today"), QStringLiteral("newer mail"), QStringLiteral("older mail"),
                              QStringLiteral("undated mail")}));
        QVERIFY(sorted({}, today).isEmpty());
    }

    void testIsOverdue()
    {
        QVERIFY(isOverdue(task(QString(), at(today.addDays(-1), 23, 59)), today));
        QVERIFY(!isOverdue(task(QString(), at(today, 0)), today));
        QVERIFY(!isOverdue(task(QString(), at(today, 23, 59)), today));
        QVERIFY(!isOverdue(task(QString(), QDateTime()), today));
        QVERIFY(!isOverdue(meeting(QString(), at(today.addDays(-1), 9), at(today.addDays(-1), 10)), today));
        QVERIFY(!isOverdue(mail(QString(), today.addDays(-3)), today));
    }

    void testNextEvent()
    {
        const QList<Item> items{
            meeting(QStringLiteral("holiday"), at(today, 0), at(today, 0), true),
            meeting(QStringLiteral("standup"), at(today, 9), at(today, 10)),
            task(QStringLiteral("report"), at(today, 10, 30)),
            meeting(QStringLiteral("review"), at(today, 11), at(today, 12)),
            meeting(QStringLiteral("reminder"), at(today, 13), at(today, 13)),
        };
        // An event going on now comes first; all-day events and tasks are never the next event
        QCOMPARE(nextEvent(items, at(today, 8)), 1);
        QCOMPARE(nextEvent(items, at(today, 9, 30)), 1);
        QCOMPARE(nextEvent(items, at(today, 10)), 3);
        QCOMPARE(nextEvent(items, at(today, 12, 30)), 4);
        // An event without a duration is next until its time has passed
        QCOMPARE(nextEvent(items, at(today, 13)), 4);
        QCOMPARE(nextEvent(items, at(today, 13, 1)), -1);
        QCOMPARE(nextEvent({}, at(today, 8)), -1);
    }

    void testCounts()
    {
        const QList<Item> items{
            task(QStringLiteral("overdue"), at(today.addDays(-2), 9)),
            task(QStringLiteral("today"), at(today, 9)),
            task(QStringLiteral("undated"), QDateTime()),
            meeting(QStringLiteral("standup"), at(today, 9), at(today, 10)),
            mail(QStringLiteral("a"), today),
            mail(QStringLiteral("b"), today, QStringLiteral("INBOX"), 2),
        };
        QCOMPARE(dueTasks(items, today), 2);
        QCOMPARE(messages(items), 2);
    }

    void testMsecsUntilNextDay()
    {
        const qint64 hour = 60 * 60 * 1000;
        const QTimeZone berlin("Europe/Berlin");
        QVERIFY(berlin.isValid());
        QCOMPARE(msecsUntilNextDay(QDateTime(QDate(2026, 11, 27), QTime(23, 0), berlin)), hour);
        // Clocks go forward at two in the morning: that day is an hour shorter
        QCOMPARE(msecsUntilNextDay(QDateTime(QDate(2026, 3, 29), QTime(0, 30), berlin)), 22 * hour + hour / 2);
        // And back in autumn: an hour longer
        QCOMPARE(msecsUntilNextDay(QDateTime(QDate(2026, 10, 25), QTime(0, 30), berlin)), 24 * hour + hour / 2);
        QCOMPARE(msecsUntilNextDay(QDateTime(QDate(2026, 11, 27), QTime(23, 59, 59, 999), berlin)), qint64(1));

        // Chile moves its clocks at midnight, so that day starts at one; the timer still ends up on the next day
        const QTimeZone santiago("America/Santiago");
        QVERIFY(santiago.isValid());
        const QDateTime saturday(QDate(2026, 9, 5), QTime(23, 0), santiago);
        const qint64 untilSunday = msecsUntilNextDay(saturday);
        QVERIFY(untilSunday > 0);
        QVERIFY(untilSunday <= hour);
        QCOMPARE(saturday.addMSecs(untilSunday).date(), QDate(2026, 9, 6));
    }

    void testFindMeetingUrl_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QUrl>("expected");
        QTest::newRow("plain") << QStringLiteral("https://meet.example.org/abc-def") << QUrl(QStringLiteral("https://meet.example.org/abc-def"));
        QTest::newRow("in a sentence") << QStringLiteral("Join at https://meet.example.org/abc. See you!")
                                       << QUrl(QStringLiteral("https://meet.example.org/abc"));
        QTest::newRow("in parentheses") << QStringLiteral("Video (https://video.example.org/j/123?pwd=x)")
                                        << QUrl(QStringLiteral("https://video.example.org/j/123?pwd=x"));
        QTest::newRow("upper case") << QStringLiteral("HTTPS://Zoom.Example.org/j/1,") << QUrl(QStringLiteral("HTTPS://Zoom.Example.org/j/1"));
        QTest::newRow("the first of two") << QStringLiteral("http://a.example.org/1 or http://b.example.org/2")
                                          << QUrl(QStringLiteral("http://a.example.org/1"));
        QTest::newRow("in angle brackets") << QStringLiteral("<https://meet.example.org/x>") << QUrl(QStringLiteral("https://meet.example.org/x"));
        QTest::newRow("no link") << QStringLiteral("Room 4, second floor") << QUrl();
        QTest::newRow("no host") << QStringLiteral("https:// is not a link") << QUrl();
        QTest::newRow("other scheme") << QStringLiteral("file:///etc/passwd") << QUrl();
        QTest::newRow("empty") << QString() << QUrl();
    }

    void testFindMeetingUrl()
    {
        QFETCH(QString, text);
        QFETCH(QUrl, expected);
        QCOMPARE(findMeetingUrl(text), expected);
    }

    void testFollowUps()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto cache = std::make_shared<CombinedCache>(QStringLiteral("agenda-test"), dir.path());
        cache->setErrorHandler([](const QString &error) { QFAIL(qPrintable(error)); });
        QVERIFY(cache->open());
        const QString inbox = QStringLiteral("INBOX");
        const QString archive = QStringLiteral("Archive");
        cache->setChildMailboxes(QString(), {
            MailboxMetadata(inbox, QStringLiteral("/"), {QStringLiteral("\\HasNoChildren")}),
            MailboxMetadata(archive, QStringLiteral("/"), {QStringLiteral("\\HasNoChildren")}),
        });
        cache->setMailboxSyncState(inbox, syncState(100, 4, 5));
        cache->setUidMapping(inbox, Imap::Uids{1, 2, 3, 4});
        const QList<std::pair<QString, QStringList>> inboxMessages{
            {QStringLiteral("Flagged"), {QStringLiteral("\\Flagged")}},
            {QStringLiteral("To do"), {QStringLiteral("$ToDo"), QStringLiteral("\\Seen")}},
            {QStringLiteral("Plain"), {QStringLiteral("\\Seen")}},
            {QStringLiteral("Deleted"), {QStringLiteral("\\Flagged"), QStringLiteral("\\Deleted")}},
        };
        for (uint uid = 1; uid <= 4; ++uid) {
            cache->setMessageMetadata(inbox, uid, cachedMessage(uid, inboxMessages.at(uid - 1).first, QDate(2026, 11, 19 + uid)));
            cache->setMsgFlags(inbox, uid, inboxMessages.at(uid - 1).second);
        }
        cache->setMailboxSyncState(archive, syncState(7, 1, 6));
        cache->setUidMapping(archive, Imap::Uids{5});
        cache->setMessageMetadata(archive, 5, cachedMessage(5, QStringLiteral("Archived"), QDate(2026, 10, 1)));
        cache->setMsgFlags(archive, 5, {QStringLiteral("\\Flagged")});

        SearchIndexer indexer([cache]() { return std::static_pointer_cast<AbstractCache>(cache); });
        indexer.setTimings(0, 0, 2);
        QVERIFY2(indexer.open(), qPrintable(indexer.unavailableReason()));
        QSignalSpy finished(&indexer, &SearchIndexer::finished);
        indexer.update();
        QVERIFY(finished.wait(5000));

        const QList<MessageSource> sources{{QStringLiteral("work"), indexer.index()}, {QStringLiteral("none"), nullptr}};
        const auto locations = [](const QList<Item> &items) {
            QStringList out;
            for (const Item &item : items)
                out << QStringLiteral("%1:%2:%3").arg(item.mailbox, QString::number(item.uid), item.toDo ? QStringLiteral("todo") : QStringLiteral("flag"));
            return out;
        };

        // Newest first; deleted and plain messages are left out
        const QList<Item> indexed = followUps(sources, ShownFolder(), maximumMessages);
        QCOMPARE(locations(indexed), (QStringList{QStringLiteral("INBOX:2:todo"), QStringLiteral("INBOX:1:flag"),
                                                  QStringLiteral("Archive:5:flag")}));
        QCOMPARE(indexed.constFirst().kind, Kind::Message);
        QCOMPARE(indexed.constFirst().accountId, QStringLiteral("work"));
        QCOMPARE(indexed.constFirst().title, QStringLiteral("To do"));
        QCOMPARE(indexed.constFirst().uidValidity, 100u);
        QCOMPARE(indexed.constFirst().sent, QDate(2026, 11, 21));
        QVERIFY(indexed.constFirst().messageId.contains(QLatin1String("2@example.org")));
        QVERIFY(indexed.constFirst().sender.contains(QLatin1String("Alice")));
        QCOMPARE(locations(followUps(sources, ShownFolder(), 1)), QStringList{QStringLiteral("INBOX:2:todo")});
        QVERIFY(followUps(sources, ShownFolder(), 0).isEmpty());
        QVERIFY(followUps({}, ShownFolder(), maximumMessages).isEmpty());

        // The shown folder knows newer flags: the flag of the first message was removed and the third one was flagged
        ShownFolder shown;
        shown.accountId = QStringLiteral("work");
        shown.mailbox = inbox;
        shown.uidValidity = 100;
        shown.loaded = {1, 2, 3};
        Item third = mail(QStringLiteral("Plain"), QDate(2026, 11, 22), inbox, 3);
        shown.followUps = {third, mail(QStringLiteral("To do"), QDate(2026, 11, 21), inbox, 2)};
        shown.followUps.last().toDo = true;
        QCOMPARE(locations(followUps(sources, shown, maximumMessages)),
                 (QStringList{QStringLiteral("INBOX:3:flag"), QStringLiteral("INBOX:2:todo"), QStringLiteral("Archive:5:flag")}));

        // The same folder name in another account is not the shown folder
        shown.accountId = QStringLiteral("home");
        QCOMPARE(locations(followUps(sources, shown, maximumMessages)),
                 (QStringList{QStringLiteral("INBOX:3:flag"), QStringLiteral("INBOX:2:todo"), QStringLiteral("INBOX:1:flag"),
                              QStringLiteral("Archive:5:flag")}));

        // The server renumbered the shown folder, so what the index has for it names other messages
        shown.accountId = QStringLiteral("work");
        shown.uidValidity = 101;
        shown.loaded.clear();
        shown.followUps.clear();
        QCOMPARE(locations(followUps(sources, shown, maximumMessages)), QStringList{QStringLiteral("Archive:5:flag")});
    }
};

QTEST_GUILESS_MAIN(AgendaTest)

#include "test_Agenda.moc"
