/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "test_SearchIndex.h"

#include <QDataStream>
#include <QDebug>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTest>
#include <QTimeZone>

#include "Imap/Model/CombinedCache.h"
#include "Imap/Model/DecryptedTextIndexer.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxMetadata.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/SearchIndex.h"
#include "Imap/Model/SearchIndexer.h"
#include "Imap/Parser/LowLevelParser.h"
#include "Imap/Parser/Message.h"

using namespace Imap::Mailbox;

namespace
{

const QString inbox = QStringLiteral("INBOX");
const QString archive = QStringLiteral("Archive");

QByteArray structure(const QByteArray &bodyStructure)
{
    int start = 0;
    const QVariantList list = Imap::LowLevelParser::parseList('(', ')', bodyStructure, start);
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_4_6);
    stream << list;
    return buffer;
}

const QByteArray plainText =
    "(\"text\" \"plain\" (\"charset\" \"utf-8\") NIL NIL \"8bit\" 40 1 NIL NIL NIL NIL)";
const QByteArray mixedWithPdf =
    "((\"text\" \"plain\" (\"charset\" \"utf-8\") NIL NIL \"quoted-printable\" 30 2 NIL NIL NIL NIL)"
    "(\"application\" \"pdf\" (\"name\" \"report.pdf\") NIL NIL \"base64\" 1000 NIL "
    "(\"attachment\" (\"filename\" \"report.pdf\")) NIL NIL) \"mixed\" (\"boundary\" \"b1\") NIL NIL NIL)";
const QByteArray alternative =
    "((\"text\" \"plain\" (\"charset\" \"iso-8859-1\") NIL NIL \"7bit\" 30 2 NIL NIL NIL NIL)"
    "(\"text\" \"html\" (\"charset\" \"utf-8\") NIL NIL \"7bit\" 50 2 NIL NIL NIL NIL) \"alternative\" "
    "(\"boundary\" \"b2\") NIL NIL NIL)";

AbstractCache::MessageDataBundle message(uint uid, const QString &subject, const QString &from, const QDate &sent,
                                         const QDate &received, quint64 size, const QByteArray &bodyStructure)
{
    AbstractCache::MessageDataBundle data;
    data.uid = uid;
    const QList<Imap::Message::MailAddress> sender{
        Imap::Message::MailAddress(from, QString(), from.toLower(), QStringLiteral("example.org"))};
    const QList<Imap::Message::MailAddress> to{
        Imap::Message::MailAddress(QStringLiteral("Dana"), QString(), QStringLiteral("dana"), QStringLiteral("example.org"))};
    data.envelope = Imap::Message::Envelope(QDateTime(sent, QTime(9, 0), QTimeZone::utc()), subject, sender, sender, sender,
                                            to, {}, {}, {}, QByteArray("<") + QByteArray::number(uid) + "@example.org>");
    data.internalDate = QDateTime(received, QTime(10, 0), QTimeZone::utc());
    data.size = size;
    data.serializedBodyStructure = structure(bodyStructure);
    data.hdrListPostNo = false;
    return data;
}

const QByteArray encrypted =
    "((\"application\" \"pgp-encrypted\" NIL NIL NIL \"7bit\" 11 NIL NIL NIL NIL)"
    "(\"application\" \"octet-stream\" NIL NIL NIL \"7bit\" 500 NIL NIL NIL NIL) \"encrypted\" "
    "(\"protocol\" \"application/pgp-encrypted\" \"boundary\" \"b3\") NIL NIL NIL)";

QStandardItem *part(const QByteArray &mimeType, const QByteArray &data = QByteArray(),
                    const QByteArray &disposition = QByteArray())
{
    auto *item = new QStandardItem(QString::fromLatin1(mimeType));
    item->setData(mimeType, RolePartMimeType);
    item->setData(data, RolePartData);
    item->setData(QByteArray("utf-8"), RolePartCharset);
    item->setData(disposition, RolePartBodyDisposition);
    return item;
}

QStringList logged;

void keepMessage(QtMsgType, const QMessageLogContext &, const QString &message)
{
    logged << message;
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

void SearchIndexTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void SearchIndexTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_cache = std::make_shared<CombinedCache>(QStringLiteral("search-index-test-%1").arg(++m_counter), m_dir->path());
    m_cache->setErrorHandler([](const QString &error) { QFAIL(qPrintable(error)); });
    QVERIFY(m_cache->open());
    std::shared_ptr<CombinedCache> cache = m_cache;
    m_indexer = std::make_unique<SearchIndexer>([cache]() { return std::static_pointer_cast<AbstractCache>(cache); });
    m_indexer->setTimings(0, 0, 2);
    QVERIFY2(m_indexer->open(), qPrintable(m_indexer->unavailableReason()));
    seed();
    runIndexer();
}

void SearchIndexTest::cleanup()
{
    m_indexer.reset();
    m_cache.reset();
    m_dir.reset();
}

void SearchIndexTest::seed()
{
    m_cache->setChildMailboxes(QString(), {
        MailboxMetadata(inbox, QStringLiteral("/"), {QStringLiteral("\\HasNoChildren")}),
        MailboxMetadata(archive, QStringLiteral("/"), {QStringLiteral("\\HasNoChildren")}),
        MailboxMetadata(QStringLiteral("Shared"), QStringLiteral("/"), {QStringLiteral("\\Noselect"), QStringLiteral("\\HasNoChildren")}),
    });

    m_cache->setMailboxSyncState(inbox, syncState(100, 3, 4));
    m_cache->setUidMapping(inbox, Imap::Uids{1, 2, 3});

    auto first = message(1, QStringLiteral("Quarterly Report"), QStringLiteral("Alice"), QDate(2026, 1, 10),
                         QDate(2026, 1, 10), 2000, plainText);
    first.hdrList.insert("list-id", "Developers <dev.lists.example.org>");
    m_cache->setMessageMetadata(inbox, 1, first);
    m_cache->setMsgFlags(inbox, 1, {QStringLiteral("\\Seen"), QStringLiteral("\\Recent")});
    m_cache->setMsgPart(inbox, 1, "1", QStringLiteral("Meet at the Café in Zürich").toUtf8());

    m_cache->setMessageMetadata(inbox, 2, message(2, QStringLiteral("Lunch"), QStringLiteral("Bob"), QDate(2026, 2, 5),
                                                  QDate(2026, 2, 6), 50000, mixedWithPdf));
    m_cache->setMsgFlags(inbox, 2, {QStringLiteral("\\Flagged"), QStringLiteral("$Important")});
    // Stored as received, with its transfer encoding, as the reader stores raw parts
    m_cache->setMsgPart(inbox, 2, "1.X-RAW", "Noon at the caf=C3=A9, bring the numbers");

    // Its text was never downloaded
    m_cache->setMessageMetadata(inbox, 3, message(3, QStringLiteral("Holiday plans"), QStringLiteral("Carol"),
                                                  QDate(2026, 3, 1), QDate(2026, 3, 1), 3000, plainText));
    m_cache->setMsgFlags(inbox, 3, {});

    m_cache->setMailboxSyncState(archive, syncState(7, 1, 6));
    m_cache->setUidMapping(archive, Imap::Uids{5});
    m_cache->setMessageMetadata(archive, 5, message(5, QStringLiteral("Old report"), QStringLiteral("Alice"),
                                                    QDate(2025, 6, 1), QDate(2025, 6, 1), 4000, alternative));
    m_cache->setMsgFlags(archive, 5, {QStringLiteral("\\Seen")});
    m_cache->setMsgPart(archive, 5, "2", "<html><head><style>p{}</style></head><body><p>Budget&nbsp;&amp; plans</p>"
                                         "<p>&#x263A; done</p></body></html>");
}

void SearchIndexTest::runIndexer()
{
    QSignalSpy finished(m_indexer.get(), &SearchIndexer::finished);
    m_indexer->update();
    QVERIFY(m_indexer->isRunning());
    QVERIFY(finished.wait(5000));
    QVERIFY(!m_indexer->isRunning());
}

QList<uint> SearchIndexTest::matches(const QStringList &conditions, const QString &mailbox)
{
    const SearchIndex::Result result = m_indexer->index()->search(conditions, mailbox.isEmpty() ? QStringList() : QStringList{mailbox});
    if (!result.problem.isEmpty())
        qWarning() << "search problem" << result.problem;
    const Imap::Uids uids = result.matches.value(mailbox);
    return QList<uint>(uids.cbegin(), uids.cend());
}

void SearchIndexTest::testTranslate_data()
{
    QTest::addColumn<QStringList>("conditions");
    QTest::addColumn<bool>("ok");
    QTest::addColumn<bool>("usesText");

    QTest::newRow("subject") << QStringList{QStringLiteral("SUBJECT"), QStringLiteral("report")} << true << false;
    QTest::newRow("body") << QStringList{QStringLiteral("BODY"), QStringLiteral("report")} << true << true;
    QTest::newRow("text in raw IMAP") << QStringList{QStringLiteral("OR TEXT \"a b\" (FROM bob UNSEEN)")} << true << true;
    QTest::newRow("fuzzy") << QStringList{QStringLiteral("FUZZY"), QStringLiteral("SUBJECT"), QStringLiteral("x")} << true << false;
    QTest::newRow("list header") << QStringList{QStringLiteral("HEADER"), QStringLiteral("List-Id"), QString()} << true << false;
    QTest::newRow("keyword") << QStringList{QStringLiteral("KEYWORD"), QStringLiteral("$Important")} << true << false;
    QTest::newRow("dates") << QStringList{QStringLiteral("SENTSINCE 1-Jan-2026 BEFORE 2-Feb-2026")} << true << false;
    QTest::newRow("unknown header") << QStringList{QStringLiteral("HEADER"), QStringLiteral("X-Spam"), QStringLiteral("yes")} << false << false;
    QTest::newRow("server only") << QStringList{QStringLiteral("X-GM-RAW"), QStringLiteral("has:drive")} << false << false;
    QTest::newRow("not a date") << QStringList{QStringLiteral("SINCE"), QStringLiteral("yesterday")} << false << false;
    QTest::newRow("not a size") << QStringList{QStringLiteral("LARGER"), QStringLiteral("big")} << false << false;
    QTest::newRow("missing value") << QStringList{QStringLiteral("SUBJECT")} << false << false;
    QTest::newRow("open parenthesis") << QStringList{QStringLiteral("(SEEN")} << false << false;
}

void SearchIndexTest::testTranslate()
{
    QFETCH(QStringList, conditions);
    QFETCH(bool, ok);
    QFETCH(bool, usesText);
    QString sql;
    QVariantList values;
    QString problem;
    bool text = false;
    QCOMPARE(SearchIndex::translate(conditions, &sql, &values, &problem, &text), ok);
    QCOMPARE(problem.isEmpty(), ok);
    if (ok) {
        QVERIFY(!sql.isEmpty());
        QCOMPARE(text, usesText);
        // Searched text is always bound, never part of the statement
        for (const QString &condition : conditions)
            QVERIFY(condition.size() < 4 || !sql.contains(condition));
    }
}

void SearchIndexTest::testUnavailableWithoutPersistentCache()
{
    auto memory = std::make_shared<MemoryCache>();
    SearchIndexer indexer([memory]() { return std::static_pointer_cast<AbstractCache>(memory); });
    QVERIFY(!indexer.open());
    QVERIFY(!indexer.isAvailable());
    QVERIFY(!indexer.unavailableReason().isEmpty());
    QVERIFY(!indexer.index());
    indexer.update();
    QVERIFY(!indexer.isRunning());

    // A cache which failed and was replaced makes the index unavailable rather than wrong
    std::shared_ptr<AbstractCache> current = m_cache;
    SearchIndexer replaced([&current]() { return current; });
    QVERIFY(replaced.open());
    QVERIFY(replaced.isAvailable());
    current = memory;
    QVERIFY(!replaced.isAvailable());
    QVERIFY(!replaced.unavailableReason().isEmpty());
}

void SearchIndexTest::testSearches_data()
{
    QTest::addColumn<QStringList>("conditions");
    QTest::addColumn<QList<uint>>("expected");

    const auto keys = [](const char *text) { return QStringList{QString::fromUtf8(text)}; };
    QTest::newRow("subject, any case") << keys("SUBJECT QUARTERLY") << QList<uint>{1};
    QTest::newRow("part of a word") << keys("SUBJECT arterl") << QList<uint>{1};
    QTest::newRow("non-ASCII case") << keys("BODY ZÜRICH") << QList<uint>{1};
    QTest::newRow("non-ASCII phrase, any case") << keys("BODY \"the CAFÉ\"") << QList<uint>{1, 2};
    QTest::newRow("quoted-printable text") << keys("BODY \"café, bring\"") << QList<uint>{2};
    QTest::newRow("short term") << keys("SUBJECT lu") << QList<uint>{2};
    QTest::newRow("short term, any case") << keys("SUBJECT LU") << QList<uint>{2};
    QTest::newRow("sender") << keys("FROM alice") << QList<uint>{1};
    QTest::newRow("recipient") << keys("TO dana@example") << QList<uint>{1, 2, 3};
    QTest::newRow("attachment name") << keys("TEXT report.pdf") << QList<uint>{2};
    QTest::newRow("text covers headers") << keys("TEXT holiday") << QList<uint>{3};
    QTest::newRow("or") << keys("OR FROM alice FROM bob") << QList<uint>{1, 2};
    QTest::newRow("not") << keys("NOT SUBJECT lunch") << QList<uint>{1, 3};
    QTest::newRow("seen") << keys("SEEN") << QList<uint>{1};
    QTest::newRow("unseen") << keys("UNSEEN") << QList<uint>{2, 3};
    QTest::newRow("recent is not kept") << keys("KEYWORD \\Recent") << QList<uint>{};
    QTest::newRow("flagged") << keys("FLAGGED") << QList<uint>{2};
    QTest::newRow("keyword, any case") << keys("KEYWORD $important") << QList<uint>{2};
    QTest::newRow("unkeyword") << keys("UNKEYWORD $Important") << QList<uint>{1, 3};
    QTest::newRow("since") << keys("SINCE 6-Feb-2026") << QList<uint>{2, 3};
    QTest::newRow("sent before") << keys("SENTBEFORE 6-Feb-2026") << QList<uint>{1, 2};
    QTest::newRow("sent on") << keys("SENTON 5-Feb-2026") << QList<uint>{2};
    QTest::newRow("on") << keys("ON 5-Feb-2026") << QList<uint>{};
    QTest::newRow("larger") << keys("LARGER 2500") << QList<uint>{2, 3};
    QTest::newRow("smaller") << keys("SMALLER 2500") << QList<uint>{1};
    QTest::newRow("list header present") << keys("HEADER List-Id \"\"") << QList<uint>{1};
    QTest::newRow("list header value") << keys("HEADER List-Id dev.lists") << QList<uint>{1};
    QTest::newRow("message id") << keys("HEADER Message-ID <2@example.org>") << QList<uint>{2};
    QTest::newRow("content type") << keys("HEADER Content-Type multipart/mixed") << QList<uint>{2};
    QTest::newRow("all") << keys("ALL") << QList<uint>{1, 2, 3};
    QTest::newRow("parenthesized") << keys("(UNSEEN SMALLER 10000)") << QList<uint>{3};
    QTest::newRow("quote in a term") << keys("SUBJECT \"a \\\"b\\\"\"") << QList<uint>{};
    QTest::newRow("percent is literal") << keys("SUBJECT %") << QList<uint>{};
}

void SearchIndexTest::testSearches()
{
    QFETCH(QStringList, conditions);
    QFETCH(QList<uint>, expected);
    const SearchIndex::Result result = m_indexer->index()->search(conditions, {inbox});
    QCOMPARE(result.problem, QString());
    const Imap::Uids uids = result.matches.value(inbox);
    QCOMPARE(QList<uint>(uids.cbegin(), uids.cend()), expected);
}

void SearchIndexTest::testFolderFilterAndText()
{
    SearchIndex *index = m_indexer->index();
    // All folders at once; the folder which cannot hold messages is not indexed
    SearchIndex::Result result = index->search({QStringLiteral("FROM"), QStringLiteral("alice")}, {});
    QCOMPARE(result.problem, QString());
    QCOMPARE(result.matches.size(), 2);
    QCOMPARE(result.matches.value(inbox), Imap::Uids{1});
    QCOMPARE(result.matches.value(archive), Imap::Uids{5});
    QVERIFY(!result.usesText);
    QCOMPARE(result.withoutText, 0);

    // Text of an HTML part, with its markup and styles left out
    QCOMPARE(matches({QStringLiteral("BODY"), QStringLiteral("budget & plans")}, archive), QList<uint>{5});
    QCOMPARE(matches({QStringLiteral("BODY"), QStringLiteral("p{}")}, archive), QList<uint>{});

    // A search of the text tells how many messages it could not look into
    result = index->search({QStringLiteral("BODY"), QStringLiteral("holiday")}, {inbox});
    QVERIFY(result.usesText);
    QVERIFY(result.matches.isEmpty());
    QCOMPARE(result.withoutText, 1);
    result = index->search({QStringLiteral("BODY"), QStringLiteral("holiday")}, {archive});
    QCOMPARE(result.withoutText, 0);

    const QList<SearchIndex::Folder> folders = index->folders();
    QCOMPARE(folders.size(), 2);
    const SearchIndex::Folder folder = index->folder(inbox);
    QCOMPARE(folder.uidValidity, 100u);
    QCOMPARE(folder.messages, 3);
    QCOMPARE(folder.indexed, 3);
    QCOMPARE(folder.withText, 2);
    QVERIFY(folder.updated.isValid());

    // Keys the index cannot answer are reported, not ignored
    result = index->search({QStringLiteral("X-GM-RAW"), QStringLiteral("x")}, {});
    QVERIFY(!result.problem.isEmpty());
    QVERIFY(result.matches.isEmpty());
}

void SearchIndexTest::testTextArrivingLater()
{
    QCOMPARE(matches({QStringLiteral("BODY"), QStringLiteral("beach")}), QList<uint>{});
    const QHash<uint, SearchIndex::Entry> before = m_indexer->index()->entries(inbox);
    QVERIFY(!before.value(3).hasText);
    QCOMPARE(before.value(3).textParts, QStringList{QStringLiteral("1")});
    QVERIFY(before.value(1).hasText);
    QVERIFY(before.value(2).hasText);

    m_cache->setMsgPart(inbox, 3, "1", "A sunny BEACH all week");
    runIndexer();
    QCOMPARE(matches({QStringLiteral("BODY"), QStringLiteral("beach")}), QList<uint>{3});
    QVERIFY(m_indexer->index()->entries(inbox).value(3).hasText);
    QCOMPARE(m_indexer->index()->search({QStringLiteral("BODY"), QStringLiteral("x")}, {inbox}).withoutText, 0);
    QCOMPARE(m_indexer->index()->folder(inbox).withText, 3);
}

void SearchIndexTest::testFlagsAndRemovedMessages()
{
    m_cache->setMsgFlags(inbox, 3, {QStringLiteral("\\Flagged"), QStringLiteral("\\Seen")});
    m_cache->setMailboxSyncState(inbox, syncState(100, 2, 4));
    m_cache->setUidMapping(inbox, Imap::Uids{2, 3});
    runIndexer();
    QCOMPARE(matches({QStringLiteral("FLAGGED")}), (QList<uint>{2, 3}));
    QCOMPARE(matches({QStringLiteral("SEEN")}), QList<uint>{3});
    QCOMPARE(matches({QStringLiteral("SUBJECT"), QStringLiteral("quarterly")}), QList<uint>{});
    QCOMPARE(m_indexer->index()->entries(inbox).size(), 2);
    QCOMPARE(m_indexer->index()->folder(inbox).messages, 2);
}

void SearchIndexTest::testUidValidityChange()
{
    // The server numbered the folder anew; old UIDs must not point at new messages
    m_cache->clearAllMessages(inbox);
    m_cache->setMailboxSyncState(inbox, syncState(200, 1, 11));
    m_cache->setUidMapping(inbox, Imap::Uids{10});
    m_cache->setMessageMetadata(inbox, 10, message(10, QStringLiteral("Renumbered"), QStringLiteral("Eve"),
                                                   QDate(2026, 4, 1), QDate(2026, 4, 1), 100, plainText));
    m_cache->setMsgFlags(inbox, 10, {});
    runIndexer();
    QCOMPARE(matches({QStringLiteral("ALL")}), QList<uint>{10});
    QCOMPARE(m_indexer->index()->folder(inbox).uidValidity, 200u);
    QCOMPARE(matches({QStringLiteral("FROM"), QStringLiteral("alice")}, archive), QList<uint>{5});
}

void SearchIndexTest::testRemovedFolder()
{
    m_cache->setChildMailboxes(QString(), {MailboxMetadata(inbox, QStringLiteral("/"), {QStringLiteral("\\HasNoChildren")})});
    runIndexer();
    QCOMPARE(m_indexer->index()->folders().size(), 1);
    const SearchIndex::Result result = m_indexer->index()->search({QStringLiteral("ALL")}, {});
    QCOMPARE(result.matches.keys(), QStringList{inbox});
}

void SearchIndexTest::testDropAndReopen()
{
    const QString connection = m_cache->databaseConnectionName();
    QString problem;
    QVERIFY2(SearchIndex::drop(connection, &problem), qPrintable(problem));
    QSqlDatabase db = QSqlDatabase::database(connection, false);
    // The tables go, unless the cache was reading at the time; their content goes in any case
    for (const QString &table : {QStringLiteral("search_docs"), QStringLiteral("search_text")}) {
        if (db.tables().contains(table)) {
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("SELECT count(*) FROM %1").arg(table)));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 0);
        }
    }
    // The cache itself is untouched
    QCOMPARE(m_cache->uidMapping(inbox), (Imap::Uids{1, 2, 3}));
    // Dropping what is not there is fine
    QVERIFY(SearchIndex::drop(connection));

    QVERIFY(m_indexer->open());
    QCOMPARE(m_indexer->index()->folders().size(), 0);
    runIndexer();
    QCOMPARE(matches({QStringLiteral("SUBJECT"), QStringLiteral("report")}), QList<uint>{1});
}

void SearchIndexTest::testPlainTextOfHtml()
{
    QCOMPARE(SearchIndex::plainTextOfHtml(QStringLiteral(
                 "<html><head><title>T</title></head><body><script>var x;</script><!-- note -->"
                 "<p>One&nbsp;&lt;two&gt;</p><div>Caf&#233; &#x263a;<br>next</div></body></html>")),
             QString::fromUtf8("One <two>\nCafé \u263a\nnext"));
}

void SearchIndexTest::testClear()
{
    QVERIFY(!m_indexer->index()->folders().isEmpty());
    QVERIFY(m_indexer->index()->clear());
    QVERIFY(m_indexer->index()->isOpen());
    QCOMPARE(m_indexer->index()->folders().size(), 0);
    QCOMPARE(m_indexer->index()->search({QStringLiteral("ALL")}, {}).matches.size(), 0);
    QVERIFY(!m_indexer->index()->folder(inbox).updated.isValid());
    // The tables stay, so the index fills again without being opened anew
    runIndexer();
    QCOMPARE(matches({QStringLiteral("SUBJECT"), QStringLiteral("report")}), QList<uint>{1});
}

void SearchIndexTest::testCoverage()
{
    using State = SearchIndexer::FolderCoverage::State;
    const auto stateOf = [this](const QString &mailbox) {
        const QList<SearchIndexer::FolderCoverage> coverage = m_indexer->coverage();
        for (const SearchIndexer::FolderCoverage &folder : coverage) {
            if (folder.mailbox == mailbox)
                return folder;
        }
        return SearchIndexer::FolderCoverage{};
    };

    QList<SearchIndexer::FolderCoverage> coverage = m_indexer->coverage();
    // Folders which cannot be selected hold no messages, so they are not listed
    QCOMPARE(coverage.size(), 2);
    QCOMPARE(coverage.at(0).mailbox, inbox);
    QCOMPARE(coverage.at(0).state, State::Indexed);
    QCOMPARE(coverage.at(0).cached, 3);
    QCOMPARE(coverage.at(0).indexed, 3);
    QCOMPARE(coverage.at(0).withText, 2);
    QVERIFY(coverage.at(0).updated.isValid());
    QCOMPARE(coverage.at(1).mailbox, archive);
    QCOMPARE(coverage.at(1).state, State::Indexed);

    // A folder which is listed but was never synchronized is configured for indexing, yet not confirmed as indexed
    m_cache->setChildMailboxes(QString(), {
        MailboxMetadata(inbox, QStringLiteral("/"), {QStringLiteral("\\HasNoChildren")}),
        MailboxMetadata(archive, QStringLiteral("/"), {QStringLiteral("\\HasNoChildren")}),
        MailboxMetadata(QStringLiteral("Drafts"), QStringLiteral("/"), {QStringLiteral("\\HasNoChildren")}),
    });
    QCOMPARE(stateOf(QStringLiteral("Drafts")).state, State::NotCached);
    QVERIFY(!stateOf(QStringLiteral("Drafts")).updated.isValid());

    // Synchronized, but not indexed yet
    m_cache->setMailboxSyncState(QStringLiteral("Drafts"), syncState(3, 0, 1));
    m_cache->setUidMapping(QStringLiteral("Drafts"), Imap::Uids{});
    QCOMPARE(stateOf(QStringLiteral("Drafts")).state, State::NotIndexed);

    // A new message arrived since the last pass
    m_cache->setMailboxSyncState(inbox, syncState(100, 4, 5));
    m_cache->setUidMapping(inbox, Imap::Uids{1, 2, 3, 4});
    QCOMPARE(stateOf(inbox).state, State::OutOfDate);
    QCOMPARE(stateOf(inbox).cached, 4);

    // Its headers are not in the cache, so the pass cannot index it
    runIndexer();
    QCOMPARE(stateOf(inbox).state, State::Partial);
    QCOMPARE(stateOf(inbox).indexed, 3);
    QCOMPARE(stateOf(QStringLiteral("Drafts")).state, State::Indexed);

    m_cache->setMessageMetadata(inbox, 4, message(4, QStringLiteral("Late"), QStringLiteral("Dan"), QDate(2026, 5, 1),
                                                  QDate(2026, 5, 1), 100, plainText));
    m_cache->setMsgFlags(inbox, 4, {});
    runIndexer();
    QCOMPARE(stateOf(inbox).state, State::Indexed);
    QCOMPARE(stateOf(inbox).indexed, 4);

    // The server numbered the folder anew
    m_cache->setMailboxSyncState(archive, syncState(8, 1, 6));
    QCOMPARE(stateOf(archive).state, State::OutOfDate);
}

void SearchIndexTest::testProgress()
{
    QCOMPARE(m_indexer->progress().folders, 0);
    QVERIFY(m_indexer->progress().mailbox.isEmpty());
    m_indexer->setTimings(0, 50, 1);
    QSignalSpy finished(m_indexer.get(), &SearchIndexer::finished);
    m_indexer->update();
    QCOMPARE(m_indexer->progress().folders, 2);
    QCOMPARE(m_indexer->progress().folder, 1);
    QTRY_COMPARE_WITH_TIMEOUT(m_indexer->progress().mailbox, inbox, 2000);
    QCOMPARE(m_indexer->progress().messages, 3);
    QTRY_COMPARE_WITH_TIMEOUT(m_indexer->progress().position, 2, 2000);
    // The archive holds one message, so it is done within one batch
    QTRY_COMPARE_WITH_TIMEOUT(m_indexer->progress().folder, 2, 2000);
    QVERIFY(finished.wait(5000));
    QCOMPARE(m_indexer->progress().folders, 0);
}

void SearchIndexTest::testPauseAndResume()
{
    QSignalSpy progress(m_indexer.get(), &SearchIndexer::progressChanged);
    m_indexer->setPaused(true);
    QVERIFY(m_indexer->isPaused());
    QVERIFY(!progress.isEmpty());
    QVERIFY(!m_indexer->hasPendingChanges());

    m_cache->setMsgFlags(inbox, 3, {QStringLiteral("\\Flagged")});
    m_indexer->scheduleUpdate();
    m_indexer->update();
    QVERIFY(!m_indexer->isRunning());
    QVERIFY(m_indexer->hasPendingChanges());
    QTest::qWait(50);
    QCOMPARE(matches({QStringLiteral("FLAGGED")}), QList<uint>{2});

    QSignalSpy finished(m_indexer.get(), &SearchIndexer::finished);
    m_indexer->setPaused(false);
    QVERIFY(!m_indexer->isPaused());
    QVERIFY(m_indexer->isRunning());
    QVERIFY(finished.wait(5000));
    QVERIFY(!m_indexer->hasPendingChanges());
    QCOMPARE(matches({QStringLiteral("FLAGGED")}), (QList<uint>{2, 3}));

    // Resuming without changes does not start a pass
    m_indexer->setPaused(true);
    m_indexer->setPaused(false);
    QVERIFY(!m_indexer->isRunning());
}

void SearchIndexTest::testPauseDuringPass()
{
    QVERIFY(m_indexer->index()->clear());
    m_indexer->setTimings(0, 50, 1);
    QSignalSpy finished(m_indexer.get(), &SearchIndexer::finished);
    m_indexer->update();
    QTRY_COMPARE_WITH_TIMEOUT(m_indexer->progress().mailbox, inbox, 2000);
    m_indexer->setPaused(true);
    QVERIFY(!m_indexer->isRunning());
    QVERIFY(m_indexer->hasPendingChanges());
    QCOMPARE(m_indexer->progress().folders, 0);
    QTest::qWait(200);
    QVERIFY(finished.isEmpty());
    // The folder under way was not finished, so it is not confirmed as indexed
    QCOMPARE(m_indexer->index()->folder(inbox).uidValidity, 0u);

    m_indexer->setTimings(0, 0, 2);
    m_indexer->setPaused(false);
    QVERIFY(finished.wait(5000));
    QCOMPARE(m_indexer->index()->folder(inbox).indexed, 3);
    QCOMPARE(matches({QStringLiteral("SUBJECT"), QStringLiteral("report")}), QList<uint>{1});
}

void SearchIndexTest::testRebuild()
{
    QSignalSpy finished(m_indexer.get(), &SearchIndexer::finished);
    QVERIFY(m_indexer->rebuild());
    QVERIFY(m_indexer->isRunning());
    QCOMPARE(m_indexer->index()->folders().size(), 0);
    QCOMPARE(m_indexer->coverage().at(0).state, SearchIndexer::FolderCoverage::State::NotIndexed);
    QVERIFY(finished.wait(5000));
    QCOMPARE(m_indexer->index()->folders().size(), 2);
    QCOMPARE(matches({QStringLiteral("BODY"), QStringLiteral("zürich")}), QList<uint>{1});

    // Rebuilding during a pass starts over
    m_indexer->setTimings(0, 50, 1);
    m_indexer->update();
    QTRY_COMPARE_WITH_TIMEOUT(m_indexer->progress().position, 2, 2000);
    finished.clear();
    QVERIFY(m_indexer->rebuild());
    QCOMPARE(m_indexer->progress().folder, 1);
    QVERIFY(finished.wait(5000));
    QCOMPARE(finished.size(), 1);
    QCOMPARE(m_indexer->index()->folders().size(), 2);
}

void SearchIndexTest::testRebuildWhilePaused()
{
    m_indexer->setPaused(true);
    QVERIFY(m_indexer->rebuild());
    QVERIFY(!m_indexer->isRunning());
    QVERIFY(m_indexer->hasPendingChanges());
    QCOMPARE(m_indexer->index()->folders().size(), 0);
    QSignalSpy finished(m_indexer.get(), &SearchIndexer::finished);
    m_indexer->setPaused(false);
    QVERIFY(finished.wait(5000));
    QCOMPARE(m_indexer->index()->folders().size(), 2);
}

void SearchIndexTest::testExcludedFolders()
{
    using State = SearchIndexer::FolderCoverage::State;
    QSignalSpy finished(m_indexer.get(), &SearchIndexer::finished);
    m_indexer->setExcludedFolders({archive});
    // What the index holds of the folder goes right away
    QCOMPARE(m_indexer->index()->folder(archive).uidValidity, 0u);
    QVERIFY(matches({QStringLiteral("SUBJECT"), QStringLiteral("report")}, archive).isEmpty());
    QCOMPARE(m_indexer->excludedFolders(), QStringList{archive});
    QVERIFY(finished.wait(5000));
    QCOMPARE(m_indexer->index()->folder(archive).uidValidity, 0u);
    QVERIFY(matches({QStringLiteral("SUBJECT"), QStringLiteral("report")}, archive).isEmpty());
    QCOMPARE(matches({QStringLiteral("SUBJECT"), QStringLiteral("report")}), QList<uint>{1});
    const QList<SearchIndexer::FolderCoverage> coverage = m_indexer->coverage();
    QCOMPARE(coverage.size(), 2);
    QCOMPARE(coverage.at(0).state, State::Indexed);
    QCOMPARE(coverage.at(1).mailbox, archive);
    QCOMPARE(coverage.at(1).state, State::Excluded);
    QCOMPARE(coverage.at(1).cached, 1);

    // The same choice again changes nothing
    m_indexer->setExcludedFolders({archive, QString()});
    QVERIFY(!m_indexer->isRunning());

    // Included again, the folder is indexed by the next pass
    finished.clear();
    m_indexer->setExcludedFolders({});
    QVERIFY(m_indexer->excludedFolders().isEmpty());
    QVERIFY(finished.wait(5000));
    QCOMPARE(matches({QStringLiteral("SUBJECT"), QStringLiteral("report")}, archive), QList<uint>{5});
    QCOMPARE(m_indexer->coverage().at(1).state, State::Indexed);
}

void SearchIndexTest::testExcludedWhilePaused()
{
    m_indexer->setPaused(true);
    m_indexer->setExcludedFolders({inbox});
    QVERIFY(!m_indexer->isRunning());
    QVERIFY(m_indexer->hasPendingChanges());
    QVERIFY(matches({QStringLiteral("SUBJECT"), QStringLiteral("report")}).isEmpty());
    QSignalSpy finished(m_indexer.get(), &SearchIndexer::finished);
    m_indexer->setPaused(false);
    QVERIFY(finished.wait(5000));
    QVERIFY(matches({QStringLiteral("SUBJECT"), QStringLiteral("report")}).isEmpty());
    QCOMPARE(m_indexer->index()->folders().size(), 1);
}

void SearchIndexTest::testDecryptedText()
{
    const QStringList secret{QStringLiteral("BODY"), QStringLiteral("rendezvous")};
    // Off by default
    QVERIFY(!m_indexer->indexesDecrypted());
    QVERIFY(!m_indexer->addDecryptedText(inbox, 3, QStringLiteral("Secret rendezvous at the harbour")));
    QVERIFY(matches(secret).isEmpty());

    m_indexer->setIndexDecrypted(true);
    QVERIFY(m_indexer->indexesDecrypted());
    QVERIFY(m_indexer->addDecryptedText(inbox, 3, QStringLiteral("Secret rendezvous at the harbour")));
    QVERIFY(m_indexer->addDecryptedText(inbox, 3, QStringLiteral("Secret rendezvous at the harbour")));
    QVERIFY(!m_indexer->addDecryptedText(inbox, 3, QStringLiteral("  ")));
    QCOMPARE(matches(secret), QList<uint>{3});
    QCOMPARE(matches({QStringLiteral("SUBJECT"), QStringLiteral("holiday")}), QList<uint>{3});
    QCOMPARE(m_indexer->index()->decryptedMessages(), 1);

    // A message which the index has not reached yet is added from the cache first
    m_cache->setMailboxSyncState(inbox, syncState(100, 4, 5));
    m_cache->setUidMapping(inbox, Imap::Uids{1, 2, 3, 4});
    m_cache->setMessageMetadata(inbox, 4, message(4, QStringLiteral("Sealed"), QStringLiteral("Dan"), QDate(2026, 5, 1),
                                                  QDate(2026, 5, 1), 900, encrypted));
    m_cache->setMsgFlags(inbox, 4, {QStringLiteral("\\Seen")});
    QVERIFY(m_indexer->addDecryptedText(inbox, 4, QStringLiteral("Minutes of the board")));
    QCOMPARE(matches({QStringLiteral("BODY"), QStringLiteral("minutes")}), QList<uint>{4});
    QCOMPARE(matches({QStringLiteral("SEEN")}), (QList<uint>{1, 4}));
    // A pass keeps the decrypted text
    runIndexer();
    QCOMPARE(matches({QStringLiteral("BODY"), QStringLiteral("minutes")}), QList<uint>{4});
    QCOMPARE(m_indexer->index()->decryptedMessages(), 2);
    // A message missing from the cache cannot be added
    QVERIFY(!m_indexer->addDecryptedText(inbox, 42, QStringLiteral("Nowhere")));

    // Nothing is added to excluded folders
    QSignalSpy finished(m_indexer.get(), &SearchIndexer::finished);
    m_indexer->setExcludedFolders({archive});
    QVERIFY(finished.wait(5000));
    QVERIFY(!m_indexer->addDecryptedText(archive, 5, QStringLiteral("Archived rendezvous")));

    // Turning it off removes the decrypted text, and the next pass indexes the messages without it
    finished.clear();
    m_indexer->setIndexDecrypted(false);
    QCOMPARE(m_indexer->index()->decryptedMessages(), 0);
    QVERIFY(matches(secret).isEmpty());
    QVERIFY(matches({QStringLiteral("BODY"), QStringLiteral("minutes")}).isEmpty());
    QVERIFY(finished.wait(5000));
    QCOMPARE(matches({QStringLiteral("SUBJECT"), QStringLiteral("holiday")}), QList<uint>{3});
    QCOMPARE(matches({QStringLiteral("SUBJECT"), QStringLiteral("sealed")}), QList<uint>{4});
    QVERIFY(matches(secret).isEmpty());
    QVERIFY(matches({QStringLiteral("BODY"), QStringLiteral("minutes")}).isEmpty());
    // Off again does nothing
    m_indexer->setIndexDecrypted(false);
    QVERIFY(!m_indexer->isRunning());
}

void SearchIndexTest::testDecryptedTextIndexer()
{
    m_indexer->setIndexDecrypted(true);
    QStandardItemModel model;
    auto *root = part("multipart/mixed");
    auto *sealed = part("multipart/encrypted");
    sealed->setData(true, RolePartDecryptionSupported);
    root->appendRow(sealed);
    model.appendRow(root);
    new DecryptedTextIndexer(&model, m_indexer.get(), inbox, 3);

    // Text which is not below an encrypted part is indexed as usual, not as decrypted text
    root->appendRow(part("text/plain", "Visible lighthouse note"));
    QVERIFY(matches({QStringLiteral("BODY"), QStringLiteral("lighthouse")}).isEmpty());

    auto *decrypted = part("multipart/mixed");
    auto *alternative = part("multipart/alternative");
    alternative->appendRow(part("text/plain", "Decrypted harbour plan"));
    alternative->appendRow(part("text/html", "<p>Gate <b>seven</b> at dawn</p>"));
    decrypted->appendRow(alternative);
    decrypted->appendRow(part("text/plain", "Attachment ledger", "attachment"));
    decrypted->appendRow(part("message/rfc822", "Forwarded payroll"));
    decrypted->appendRow(part("application/pdf", "%PDF quarterly"));
    sealed->appendRow(decrypted);

    QCOMPARE(matches({QStringLiteral("BODY"), QStringLiteral("harbour plan")}), QList<uint>{3});
    QCOMPARE(matches({QStringLiteral("BODY"), QStringLiteral("gate seven")}), QList<uint>{3});
    QVERIFY(matches({QStringLiteral("BODY"), QStringLiteral("ledger")}).isEmpty());
    QVERIFY(matches({QStringLiteral("BODY"), QStringLiteral("payroll")}).isEmpty());
    QVERIFY(matches({QStringLiteral("BODY"), QStringLiteral("%PDF")}).isEmpty());

    const QStringList texts = DecryptedTextIndexer::texts(model.indexFromItem(decrypted));
    QCOMPARE(texts.size(), 2);
    QCOMPARE(texts.at(0), QStringLiteral("Decrypted harbour plan"));
    QVERIFY(texts.at(1).contains(QStringLiteral("Gate seven at dawn")));

    // A failed decryption inserts only a notice, which is not text to index
    sealed->appendRow(part("application/octet-stream", "Failure blob"));
    QVERIFY(matches({QStringLiteral("BODY"), QStringLiteral("blob")}).isEmpty());
}

void SearchIndexTest::testDeleteIndex()
{
    QSignalSpy progress(m_indexer.get(), &SearchIndexer::progressChanged);
    QString problem;
    QVERIFY2(m_indexer->deleteIndex(&problem), qPrintable(problem));
    QVERIFY(!progress.isEmpty());
    QVERIFY(!m_indexer->isAvailable());
    QVERIFY(!m_indexer->index());
    QVERIFY(m_indexer->unavailableReason().contains(QStringLiteral("deleted")));
    QVERIFY(m_indexer->coverage().isEmpty());
    const QStringList tables = QSqlDatabase::database(m_cache->databaseConnectionName(), false).tables();
    for (const QString &table : {QStringLiteral("search_text"), QStringLiteral("search_docs"),
                                 QStringLiteral("search_folders"), QStringLiteral("search_meta")})
        QVERIFY2(!tables.contains(table), qPrintable(table));
    // The cached mail stays
    QCOMPARE(m_cache->uidMapping(inbox), (Imap::Uids{1, 2, 3}));
    // Nothing happens afterwards
    m_indexer->update();
    QVERIFY(!m_indexer->isRunning());
    QVERIFY(!m_indexer->addDecryptedText(inbox, 1, QStringLiteral("text")));
}

void SearchIndexTest::testNothingIndexedIsLogged()
{
    logged.clear();
    const QtMessageHandler previous = qInstallMessageHandler(keepMessage);
    QSignalSpy finished(m_indexer.get(), &SearchIndexer::finished);
    m_indexer->rebuild();
    finished.wait(5000);
    m_indexer->index()->search({QStringLiteral("HEADER"), QStringLiteral("X-Spam"), QStringLiteral("Quarterly")}, {});
    m_indexer->index()->search({QStringLiteral("BODY Zürich")}, {inbox});
    m_indexer->setIndexDecrypted(true);
    m_indexer->addDecryptedText(inbox, 3, QStringLiteral("Confidential rendezvous"));
    m_indexer->setExcludedFolders({inbox});
    finished.wait(5000);
    m_indexer->setIndexDecrypted(false);
    m_indexer->deleteIndex();
    qInstallMessageHandler(previous);
    for (const QString &message : std::as_const(logged)) {
        for (const QString &word : {QStringLiteral("Zürich"), QStringLiteral("Café"), QStringLiteral("Quarterly"),
                                    QStringLiteral("rendezvous"), QStringLiteral("numbers"), QStringLiteral("Budget")})
            QVERIFY2(!message.contains(word, Qt::CaseInsensitive), qPrintable(message));
    }
}

QTEST_GUILESS_MAIN(SearchIndexTest)
