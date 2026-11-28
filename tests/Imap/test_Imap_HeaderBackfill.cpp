/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_Imap_HeaderBackfill.h"

#include <QDataStream>
#include <QSignalSpy>
#include <QTimeZone>

#include "Imap/Model/HeaderBackfill.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "Imap/Parser/LowLevelParser.h"
#include "Streams/FakeSocket.h"
#include "Utils/FakeCapabilitiesInjector.h"

using namespace Imap::Mailbox;

namespace {

/** The SEARCH key for the thirty days before the reference date of the tests */
const char searchRecent[] = "UID SEARCH CHARSET utf-8 SINCE 29-Oct-2026\r\n";

QByteArray fetchCommand(const QByteArray &uids)
{
    return "UID FETCH " + uids + " (" FETCH_METADATA_ITEMS ")\r\n";
}

AbstractCache::MessageDataBundle cachedMessage(uint uid, const QString &subject)
{
    AbstractCache::MessageDataBundle data;
    data.uid = uid;
    data.envelope = Imap::Message::Envelope(QDateTime(QDate(2026, 11, 20), QTime(9, 0), QTimeZone::utc()), subject, {},
        {}, {}, {}, {}, {}, {}, QByteArray("<") + QByteArray::number(uid) + "@example.org>");
    data.size = 1000;
    int start = 0;
    const QVariantList list = Imap::LowLevelParser::parseList(
        '(', ')', "(\"text\" \"plain\" (\"charset\" \"utf-8\") NIL NIL \"8bit\" 40 1 NIL NIL NIL NIL)", start);
    QDataStream stream(&data.serializedBodyStructure, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_4_6);
    stream << list;
    return data;
}

}

void ImapHeaderBackfillTest::open(uint messages)
{
    // The fixture expects a quiet connection right after the synchronization
    const bool held = otherWork;
    otherWork = true;
    initialMessages(messages);
    otherWork = held;
}

void ImapHeaderBackfillTest::init()
{
    LibMailboxSync::init();
    otherWork = false;
    backfill = new HeaderBackfill(model, model);
    backfill->setReferenceDate(QDate(2026, 11, 28));
    backfill->setRecentDays(30);
    // Look on each pass through the event loop, so that the fixture's loops drive it
    backfill->setPollInterval(0);
    backfill->setOtherWorkPending([this]() { return otherWork; });
}

void ImapHeaderBackfillTest::cleanup()
{
    // Goes away with the model
    backfill = nullptr;
    LibMailboxSync::cleanup();
}

/** @short The messages which the server calls recent come first, newest first, then the rest from the end of the list
 */
void ImapHeaderBackfillTest::testRecentFirstThenOlderInBatches()
{
    backfill->setBatchSizes(2, 3);
    QSignalSpy recent(backfill, &HeaderBackfill::recentLoaded);
    QSignalSpy finished(backfill, &HeaderBackfill::finished);
    open(10);
    QCOMPARE(backfill->mailbox(), QStringLiteral("a"));
    cClient(t.mk(searchRecent));
    cServer("* SEARCH 7 9 10\r\n" + t.last("OK searched\r\n"));

    cClient(t.mk(fetchCommand("9:10").constData()));
    cServer(helperCreateTrivialEnvelope(9, 9, QStringLiteral("nine"))
        + helperCreateTrivialEnvelope(10, 10, QStringLiteral("ten")) + t.last("OK fetched\r\n"));
    QCOMPARE(model->index(9, 0, msgListA).data(RoleMessageSubject).toString(), QStringLiteral("ten"));
    QVERIFY(recent.isEmpty());
    cClient(t.mk(fetchCommand("7").constData()));
    cServer(helperCreateTrivialEnvelope(7, 7, QStringLiteral("seven")) + t.last("OK fetched\r\n"));
    QCOMPARE(backfill->recentCount(), 3);

    // The older messages follow from the newest one down, skipping those already loaded
    cClient(t.mk(fetchCommand("5:6,8").constData()));
    QCOMPARE(recent.size(), 1);
    QCOMPARE(recent.first().first().toString(), QStringLiteral("a"));
    cServer(helperCreateTrivialEnvelope(5, 5, QStringLiteral("five"))
        + helperCreateTrivialEnvelope(6, 6, QStringLiteral("six"))
        + helperCreateTrivialEnvelope(8, 8, QStringLiteral("eight")) + t.last("OK fetched\r\n"));
    cClient(t.mk(fetchCommand("2:4").constData()));
    cServer(helperCreateTrivialEnvelope(2, 2, QStringLiteral("two"))
        + helperCreateTrivialEnvelope(3, 3, QStringLiteral("three"))
        + helperCreateTrivialEnvelope(4, 4, QStringLiteral("four")) + t.last("OK fetched\r\n"));
    cClient(t.mk(fetchCommand("1").constData()));
    QCOMPARE(finished.size(), 1);
    QVERIFY(!backfill->isRunning());
    cServer(helperCreateTrivialEnvelope(1, 1, QStringLiteral("one")) + t.last("OK fetched\r\n"));
    QCOMPARE(backfill->requestedCount(), 10);
    cEmpty();
    // The fixture's envelopes leave out a few items, so the messages stay loading but have their subjects
    for (int row = 0; row < 10; ++row)
        QVERIFY(!model->index(row, 0, msgListA).data(RoleMessageSubject).toString().isEmpty());
    cEmpty();
    justKeepTask();
}

/** @short Headers which the offline cache has are taken from there */
void ImapHeaderBackfillTest::testCachedHeadersSkipTheServer()
{
    QSignalSpy recent(backfill, &HeaderBackfill::recentLoaded);
    open(4);
    cClient(t.mk(searchRecent));
    model->cache()->setMessageMetadata(QStringLiteral("a"), 3, cachedMessage(3, QStringLiteral("cached three")));
    model->cache()->setMessageMetadata(QStringLiteral("a"), 4, cachedMessage(4, QStringLiteral("cached four")));
    cServer("* SEARCH 3 4\r\n" + t.last("OK searched\r\n"));
    cClient(t.mk(fetchCommand("1:2").constData()));
    QCOMPARE(recent.size(), 1);
    QCOMPARE(model->index(3, 0, msgListA).data(RoleMessageSubject).toString(), QStringLiteral("cached four"));
    cServer(helperCreateTrivialEnvelope(1, 1, QStringLiteral("one"))
        + helperCreateTrivialEnvelope(2, 2, QStringLiteral("two")) + t.last("OK fetched\r\n"));
    cEmpty();
}

/** @short A flag change of the user goes first; the next batch waits until the server answered it */
void ImapHeaderBackfillTest::testWaitsForTheUser()
{
    backfill->setBatchSizes(5, 5);
    open(10);
    cClient(t.mk(searchRecent));
    model->markMessagesRead({model->index(0, 0, msgListA)}, FLAG_ADD);
    cClient(t.mk("UID STORE 1 +FLAGS (\\Seen)\r\n"));
    cServer("* SEARCH\r\n" + t.prev("OK searched\r\n"));
    cEmpty();
    cServer("* 1 FETCH (FLAGS (\\Seen))\r\n" + t.last("OK stored\r\n"));
    cClient(t.mk(fetchCommand("6:10").constData()));

    // Showing a message while a batch loads goes out straight away, with the neighbours which the list preloads
    QSignalSpy finished(backfill, &HeaderBackfill::finished);
    model->index(0, 0, msgListA).data(RoleMessageSubject);
    cClient(t.mk(fetchCommand("1:5").constData()));
    QByteArray answer;
    for (uint uid = 1; uid <= 5; ++uid)
        answer += helperCreateTrivialEnvelope(uid, uid, QStringLiteral("shown"));
    cServer(answer + t.last("OK fetched\r\n"));
    cEmpty();
    answer.clear();
    for (uint uid = 6; uid <= 10; ++uid)
        answer += helperCreateTrivialEnvelope(uid, uid, QStringLiteral("message"));
    cServer(answer + t.prev("OK fetched\r\n"));
    // The messages which the list loaded are not asked for again
    cEmpty();
    QCOMPARE(finished.size(), 1);
    QCOMPARE(backfill->requestedCount(), 5);
}

/** @short Nothing starts while other work of the account, such as a message to send, is due */
void ImapHeaderBackfillTest::testWaitsForOtherWork()
{
    otherWork = true;
    open(3);
    cEmpty();
    QVERIFY(backfill->isRunning());
    otherWork = false;
    cClient(t.mk(searchRecent));
    otherWork = true;
    cServer("* SEARCH 3\r\n" + t.last("OK searched\r\n"));
    cEmpty();
    otherWork = false;
    cClient(t.mk(fetchCommand("3").constData()));
}

/** @short At most the limit of headers is asked for */
void ImapHeaderBackfillTest::testStopsAtTheLimit()
{
    backfill->setBatchSizes(2, 2);
    backfill->setMessageLimit(3);
    QSignalSpy finished(backfill, &HeaderBackfill::finished);
    open(10);
    cClient(t.mk(searchRecent));
    cServer("* SEARCH 10\r\n" + t.last("OK searched\r\n"));
    cClient(t.mk(fetchCommand("10").constData()));
    cServer(helperCreateTrivialEnvelope(10, 10, QStringLiteral("ten")) + t.last("OK fetched\r\n"));
    cClient(t.mk(fetchCommand("8:9").constData()));
    QCOMPARE(finished.size(), 1);
    QCOMPARE(backfill->requestedCount(), 3);
    cServer(helperCreateTrivialEnvelope(8, 8, QStringLiteral("eight"))
        + helperCreateTrivialEnvelope(9, 9, QStringLiteral("nine")) + t.last("OK fetched\r\n"));
    cEmpty();
}

/** @short Without an answer to the search, the headers still load from the newest message down */
void ImapHeaderBackfillTest::testFailedSearchLoadsNewestFirst()
{
    QSignalSpy recent(backfill, &HeaderBackfill::recentLoaded);
    open(3);
    cClient(t.mk(searchRecent));
    cServer(t.last("NO search refused\r\n"));
    cClient(t.mk(fetchCommand("1:3").constData()));
    QCOMPARE(recent.size(), 1);
    QCOMPARE(backfill->recentCount(), 0);
}

/** @short With CONTEXT=SEARCH, the search still asks for one answer only, so no context stays open on the server */
void ImapHeaderBackfillTest::testSearchKeepsNoContext()
{
    open(3);
    // Before the search starts on the next pass through the event loop
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("ESEARCH"));
    injector.injectCapability(QStringLiteral("CONTEXT=SEARCH"));
    cClient(t.mk("UID SEARCH RETURN (ALL) CHARSET utf-8 SINCE 29-Oct-2026\r\n"));
    cServer("* ESEARCH (TAG \"" + t.last() + "\") UID ALL 3\r\n" + t.last("OK searched\r\n"));
    cClient(t.mk(fetchCommand("3").constData()));
    cServer(helperCreateTrivialEnvelope(3, 3, QStringLiteral("three")) + t.last("OK fetched\r\n"));
    // No CANCELUPDATE comes before the older messages
    cClient(t.mk(fetchCommand("1:2").constData()));
    QCOMPARE(backfill->recentCount(), 1);
}

/** @short An expensive network pauses the work until it is online again */
void ImapHeaderBackfillTest::testPausesOnExpensiveNetwork()
{
    otherWork = true;
    open(3);
    setModelNetworkPolicy(model, NETWORK_EXPENSIVE);
    otherWork = false;
    cEmpty();
    QVERIFY(backfill->isRunning());
    setModelNetworkPolicy(model, NETWORK_ONLINE);
    cClient(t.mk(searchRecent));
}

/** @short Going offline stops the work */
void ImapHeaderBackfillTest::testStopsOffline()
{
    open(3);
    cClient(t.mk(searchRecent));
    QVERIFY(backfill->isRunning());
    setModelNetworkPolicy(model, NETWORK_OFFLINE);
    QVERIFY(!backfill->isRunning());
}

/** @short Opening another folder moves the work there */
void ImapHeaderBackfillTest::testFollowsTheOpenFolder()
{
    QSignalSpy finished(backfill, &HeaderBackfill::finished);
    otherWork = true;
    open(3);
    QCOMPARE(backfill->mailbox(), QStringLiteral("a"));
    helperSyncBNoMessages();
    QCOMPARE(backfill->mailbox(), QStringLiteral("b"));
    otherWork = false;
    cEmpty();
    QCOMPARE(finished.size(), 1);
    QCOMPARE(finished.first().first().toString(), QStringLiteral("b"));
}

QTEST_GUILESS_MAIN(ImapHeaderBackfillTest)
