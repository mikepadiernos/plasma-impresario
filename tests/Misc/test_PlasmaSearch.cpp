/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "test_PlasmaSearch.h"

#include <QDateTime>
#include <QDir>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTest>
#include <QTimeZone>

#include <KConfig>

#include "Imap/Model/MailboxMetadata.h"
#include "Imap/Model/SearchIndex.h"
#include "Imap/Parser/Message.h"
#include "UiUtils/PlasmaSearch.h"

using namespace Imap::Mailbox;
namespace PlasmaSearch = UiUtils::PlasmaSearch;

namespace
{

AbstractCache::MessageDataBundle message(uint uid, const QString &subject, const QString &from, const QDate &sent)
{
    AbstractCache::MessageDataBundle data;
    data.uid = uid;
    const QList<Imap::Message::MailAddress> sender{
        Imap::Message::MailAddress(from, QString(), from.toLower(), QStringLiteral("example.org"))};
    const QDateTime date = sent.isValid() ? QDateTime(sent, QTime(9, 0), QTimeZone::utc()) : QDateTime();
    data.envelope = Imap::Message::Envelope(date, subject, sender, sender, sender, sender, {}, {}, {},
                                            QByteArray("<") + QByteArray::number(uid) + "@example.org>");
    data.internalDate = QDateTime(QDate(2026, 1, 1), QTime(10, 0), QTimeZone::utc());
    data.size = 100;
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

void PlasmaSearchTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(m_dir.isValid());
}

void PlasmaSearchTest::cleanup()
{
    for (Account *account : {&m_home, &m_work}) {
        account->indexer.reset();
        account->cache.reset();
    }
}

void PlasmaSearchTest::openAccount(Account &account, const QString &name)
{
    QVERIFY(QDir(m_dir.path()).mkpath(name));
    account.cache = std::make_shared<CombinedCache>(QStringLiteral("plasma-search-test-") + name, m_dir.filePath(name));
    account.cache->setErrorHandler([](const QString &error) { QFAIL(qPrintable(error)); });
    QVERIFY(account.cache->open());
    std::shared_ptr<CombinedCache> cache = account.cache;
    account.indexer = std::make_unique<SearchIndexer>([cache]() { return std::static_pointer_cast<AbstractCache>(cache); });
    account.indexer->setTimings(0, 0, 2);
    QVERIFY2(account.indexer->open(), qPrintable(account.indexer->unavailableReason()));
}

void PlasmaSearchTest::offByDefault()
{
    const QString path = m_dir.filePath(QStringLiteral("settingsrc"));
    {
        KConfig config(path, KConfig::SimpleConfig);
        QVERIFY(!PlasmaSearch::isEnabled(config));
        PlasmaSearch::setEnabled(config, true);
        QVERIFY(PlasmaSearch::isEnabled(config));
    }
    KConfig reread(path, KConfig::SimpleConfig);
    QVERIFY(PlasmaSearch::isEnabled(reread));
    PlasmaSearch::setEnabled(reread, false);
    KConfig again(path, KConfig::SimpleConfig);
    QVERIFY(!PlasmaSearch::isEnabled(again));
}

void PlasmaSearchTest::words_data()
{
    QTest::addColumn<QString>("query");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("empty") << QString() << QStringList();
    QTest::newRow("blank") << QStringLiteral(" \t ") << QStringList();
    QTest::newRow("short words dropped") << QStringLiteral("re: an invoice")
                                         << QStringList{QStringLiteral("re:"), QStringLiteral("invoice")};
    QTest::newRow("too short") << QStringLiteral("to re") << QStringList();
    QTest::newRow("repeats dropped regardless of case")
        << QStringLiteral("Report report REPORT plan") << QStringList{QStringLiteral("Report"), QStringLiteral("plan")};
    QTest::newRow("counted in characters") << QStringLiteral("Zü 日本語")
                                           << QStringList{QStringLiteral("日本語")};
    QTest::newRow("at most eight")
        << QStringLiteral("one two six ten aaa bbb ccc ddd eee fff")
        << QStringList{QStringLiteral("one"), QStringLiteral("two"), QStringLiteral("six"), QStringLiteral("ten"),
                       QStringLiteral("aaa"), QStringLiteral("bbb"), QStringLiteral("ccc"), QStringLiteral("ddd")};
}

void PlasmaSearchTest::words()
{
    QFETCH(QString, query);
    QFETCH(QStringList, expected);
    QCOMPARE(PlasmaSearch::words(query), expected);
}

void PlasmaSearchTest::matchIdRoundTrip_data()
{
    QTest::addColumn<QString>("account");
    QTest::addColumn<QString>("mailbox");

    QTest::newRow("plain") << QStringLiteral("work") << QStringLiteral("INBOX");
    QTest::newRow("hierarchy") << QStringLiteral("home") << QStringLiteral("Lists/dev.example");
    QTest::newRow("spaces and percent") << QStringLiteral("my account") << QStringLiteral("Sent 100%");
    QTest::newRow("non-ASCII") << QStringLiteral("Zürich") << QStringLiteral("Entwürfe");
}

void PlasmaSearchTest::matchIdRoundTrip()
{
    QFETCH(QString, account);
    QFETCH(QString, mailbox);
    const PlasmaSearch::Target target{account, mailbox, 4294967295u, 17};
    const QString id = PlasmaSearch::matchId(target);
    QVERIFY(id.startsWith(QLatin1String("message/")));
    QCOMPARE(id.count(QLatin1Char('/')), 4);
    PlasmaSearch::Target parsed;
    QVERIFY(PlasmaSearch::parseMatchId(id, &parsed));
    QCOMPARE(parsed.accountId, account);
    QCOMPARE(parsed.mailbox, mailbox);
    QCOMPARE(parsed.uidValidity, 4294967295u);
    QCOMPARE(parsed.uid, 17u);
    QVERIFY(PlasmaSearch::parseMatchId(id, nullptr));
}

void PlasmaSearchTest::rejectedMatchIds_data()
{
    QTest::addColumn<QString>("id");

    QTest::newRow("empty") << QString();
    QTest::newRow("other prefix") << QStringLiteral("file/work/INBOX/1/2");
    QTest::newRow("too few parts") << QStringLiteral("message/work/INBOX/1");
    QTest::newRow("too many parts") << QStringLiteral("message/work/Lists/dev/1/2");
    QTest::newRow("empty account") << QStringLiteral("message//INBOX/1/2");
    QTest::newRow("empty mailbox") << QStringLiteral("message/work//1/2");
    QTest::newRow("not canonical") << QStringLiteral("message/my account/INBOX/1/2");
    QTest::newRow("lowercase escape") << QStringLiteral("message/my%2faccount/INBOX/1/2");
    QTest::newRow("zero uidvalidity") << QStringLiteral("message/work/INBOX/0/2");
    QTest::newRow("zero uid") << QStringLiteral("message/work/INBOX/1/0");
    QTest::newRow("leading zero") << QStringLiteral("message/work/INBOX/01/2");
    QTest::newRow("sign") << QStringLiteral("message/work/INBOX/+1/2");
    QTest::newRow("negative") << QStringLiteral("message/work/INBOX/1/-2");
    QTest::newRow("too large") << QStringLiteral("message/work/INBOX/4294967296/2");
    QTest::newRow("not a number") << QStringLiteral("message/work/INBOX/1/two");
}

void PlasmaSearchTest::rejectedMatchIds()
{
    QFETCH(QString, id);
    PlasmaSearch::Target target{QStringLiteral("kept"), QString(), 0, 0};
    QVERIFY(!PlasmaSearch::parseMatchId(id, &target));
    QCOMPARE(target.accountId, QStringLiteral("kept"));
}

void PlasmaSearchTest::matchesAcrossAccounts()
{
    openAccount(m_home, QStringLiteral("home"));
    QVERIFY(!QTest::currentTestFailed());
    openAccount(m_work, QStringLiteral("work"));
    QVERIFY(!QTest::currentTestFailed());

    const QString inbox = QStringLiteral("INBOX");
    const QString lists = QStringLiteral("Lists/dev");
    m_home.cache->setChildMailboxes(QString(), {MailboxMetadata(inbox, QStringLiteral("/"), {})});
    m_home.cache->setMailboxSyncState(inbox, syncState(11, 3, 4));
    m_home.cache->setUidMapping(inbox, Imap::Uids{1, 2, 3});
    m_home.cache->setMessageMetadata(inbox, 1, message(1, QStringLiteral("Release plan"), QStringLiteral("Ann"), QDate(2026, 3, 1)));
    m_home.cache->setMessageMetadata(inbox, 2, message(2, QStringLiteral("Lunch"), QStringLiteral("Bob"), QDate(2026, 5, 1)));
    m_home.cache->setMessageMetadata(inbox, 3, message(3, QStringLiteral("Undated release"), QStringLiteral("Ann"), QDate()));

    m_work.cache->setChildMailboxes(QString(), {MailboxMetadata(inbox, QStringLiteral("/"), {}),
                                                MailboxMetadata(lists, QStringLiteral("/"), {})});
    m_work.cache->setMailboxSyncState(inbox, syncState(22, 1, 8));
    m_work.cache->setUidMapping(inbox, Imap::Uids{7});
    m_work.cache->setMessageMetadata(inbox, 7, message(7, QStringLiteral("Release notes"), QStringLiteral("Carol"), QDate(2026, 4, 1)));
    m_work.cache->setMailboxSyncState(lists, syncState(33, 1, 3));
    m_work.cache->setUidMapping(lists, Imap::Uids{2});
    m_work.cache->setMessageMetadata(lists, 2, message(2, QStringLiteral("Old release"), QStringLiteral("Dan"), QDate(2025, 1, 1)));

    for (Account *account : {&m_home, &m_work}) {
        QSignalSpy finished(account->indexer.get(), &SearchIndexer::finished);
        account->indexer->update();
        QVERIFY(finished.wait(5000));
    }

    const QList<PlasmaSearch::Source> sources{{QStringLiteral("home"), m_home.indexer->index()},
                                              {QStringLiteral("work"), m_work.indexer->index()},
                                              {QStringLiteral("closed"), nullptr}};
    const auto ids = [&sources](const QString &query, int limit = PlasmaSearch::maximumMatches) {
        QStringList result;
        const auto found = PlasmaSearch::matches(query, sources, limit);
        for (const auto &match : found)
            result << match.id;
        return result;
    };

    // Newest first across both accounts, the undated last
    const QStringList releases{QStringLiteral("message/work/INBOX/22/7"), QStringLiteral("message/home/INBOX/11/1"),
                               QStringLiteral("message/work/Lists%2Fdev/33/2"), QStringLiteral("message/home/INBOX/11/3")};
    QCOMPARE(ids(QStringLiteral("release")), releases);
    QCOMPARE(ids(QStringLiteral("release"), 2), releases.mid(0, 2));
    QVERIFY(ids(QStringLiteral("release"), 0).isEmpty());

    // Short words are left out; with nothing left there is no search
    QCOMPARE(ids(QStringLiteral("an release")), releases);
    QVERIFY(ids(QStringLiteral("an")).isEmpty());
    QVERIFY(ids(QString()).isEmpty());

    // Every word must match, in the subject or the sender
    QCOMPARE(ids(QStringLiteral("ann release")),
             (QStringList{QStringLiteral("message/home/INBOX/11/1"), QStringLiteral("message/home/INBOX/11/3")}));
    QVERIFY(ids(QStringLiteral("ann lunch")).isEmpty());

    const auto found = PlasmaSearch::matches(QStringLiteral("release"), sources);
    QCOMPARE(found.size(), 4);
    QCOMPARE(found.at(0).subject, QStringLiteral("Release notes"));
    QVERIFY(found.at(0).sender.contains(QLatin1String("Carol")));
    QCOMPARE(found.at(0).sent, QDate(2026, 4, 1));
    QVERIFY(!found.at(3).sent.isValid());
    // The text is left to the GUI
    QVERIFY(found.at(0).title.isEmpty());
    QVERIFY(found.at(0).subtext.isEmpty());
    for (int i = 0; i < found.size(); ++i) {
        QVERIFY(found.at(i).relevance > 0);
        QVERIFY(found.at(i).relevance < 1);
        if (i)
            QVERIFY(found.at(i).relevance < found.at(i - 1).relevance);
        PlasmaSearch::Target target;
        QVERIFY(PlasmaSearch::parseMatchId(found.at(i).id, &target));
    }

    // A folder whose UIDVALIDITY is unknown gives no matches, as they could not be opened safely. The indexer never
    // records one, so an older or damaged index is stood in for.
    QSqlQuery q(QSqlDatabase::database(m_home.cache->databaseConnectionName(), false));
    QVERIFY(q.exec(QStringLiteral("UPDATE search_folders SET uidvalidity = 0 WHERE mailbox = 'INBOX'")));
    QCOMPARE(ids(QStringLiteral("release")),
             (QStringList{QStringLiteral("message/work/INBOX/22/7"), QStringLiteral("message/work/Lists%2Fdev/33/2")}));
}

QTEST_GUILESS_MAIN(PlasmaSearchTest)
