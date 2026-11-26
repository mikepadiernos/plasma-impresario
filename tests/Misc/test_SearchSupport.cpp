/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QtTest>

#include "test_SearchSupport.h"
#include "Imap/Model/SearchSupport.h"

using Imap::Mailbox::SearchSupport;
namespace Responses = Imap::Responses;

void SearchSupportTest::findsUnsupportedKeys_data()
{
    QTest::addColumn<QStringList>("conditions");
    QTest::addColumn<QStringList>("capabilities");
    QTest::addColumn<QStringList>("missing");

    const QStringList base{QStringLiteral("IMAP4rev1"), QStringLiteral("ESEARCH")};
    const auto keys = [](const QString &text) { return text.split(QLatin1Char(' '), Qt::SkipEmptyParts); };

    QTest::newRow("baseline keys") << keys(QStringLiteral("OR SUBJECT foo FROM foo SENTSINCE 1-Jan-2026")) << base << QStringList();
    QTest::newRow("fuzzy without the extension")
        << keys(QStringLiteral("OR FUZZY SUBJECT x FUZZY BODY x")) << base << QStringList{QStringLiteral("FUZZY:SEARCH=FUZZY")};
    QTest::newRow("fuzzy with the extension")
        << keys(QStringLiteral("FUZZY SUBJECT x")) << (base + QStringList{QStringLiteral("SEARCH=FUZZY")}) << QStringList();
    QTest::newRow("a value which reads like a key") << keys(QStringLiteral("OR SUBJECT older FROM fuzzy")) << base << QStringList();
    QTest::newRow("header takes two values") << keys(QStringLiteral("HEADER X-Older younger")) << base << QStringList();

    QTest::newRow("raw: within") << QStringList{QStringLiteral("older 3600 NOT (YOUNGER 60)")} << base
                                 << QStringList{QStringLiteral("OLDER:WITHIN"), QStringLiteral("YOUNGER:WITHIN")};
    QTest::newRow("raw: quoted value") << QStringList{QStringLiteral("SUBJECT \"OLDER \\\"FUZZY\\\"\" UNSEEN")} << base << QStringList();
    QTest::newRow("raw: literal value") << QStringList{QStringLiteral("SUBJECT {5}\r\nOLDER SEEN")} << base << QStringList();
    QTest::newRow("raw: sequence set and flags") << QStringList{QStringLiteral("1:5,7 UID 9:* FLAGGED")} << base << QStringList();
    QTest::newRow("raw: modseq") << QStringList{QStringLiteral("MODSEQ 33")} << base << QStringList{QStringLiteral("MODSEQ:CONDSTORE")};
    QTest::newRow("raw: modseq with an entry")
        << QStringList{QStringLiteral("MODSEQ \"/flags/\\\\draft\" all 620162338 OLDER 5")} << base
        << QStringList{QStringLiteral("MODSEQ:CONDSTORE"), QStringLiteral("OLDER:WITHIN")};
    QTest::newRow("raw: QRESYNC implies CONDSTORE")
        << QStringList{QStringLiteral("MODSEQ 33")} << (base + QStringList{QStringLiteral("QRESYNC")}) << QStringList();
    QTest::newRow("raw: Gmail") << QStringList{QStringLiteral("X-GM-RAW \"has:attachment\"")} << base
                                << QStringList{QStringLiteral("X-GM-RAW:X-GM-EXT-1")};
    QTest::newRow("raw: saved date and object ids")
        << QStringList{QStringLiteral("SAVEDSINCE 1-Jan-2026 EMAILID M123")} << base
        << QStringList{QStringLiteral("SAVEDSINCE:SAVEDATE"), QStringLiteral("EMAILID:OBJECTID")};
    QTest::newRow("raw: an unknown key stops the scan")
        << QStringList{QStringLiteral("XFOO OLDER 5")} << base << QStringList();
    QTest::newRow("raw: case does not matter")
        << QStringList{QStringLiteral("fuzzy subject x")} << QStringList{QStringLiteral("search=fuzzy")} << QStringList();
    QTest::newRow("all") << QStringList{QStringLiteral("ALL")} << base << QStringList();
}

void SearchSupportTest::findsUnsupportedKeys()
{
    QFETCH(QStringList, conditions);
    QFETCH(QStringList, capabilities);
    QFETCH(QStringList, missing);
    QStringList found;
    for (const auto &entry : SearchSupport::unsupportedKeys(conditions, capabilities))
        found << entry.key + QLatin1Char(':') + entry.capability;
    QCOMPARE(found, missing);
}

void SearchSupportTest::detectsTextBeyondAscii()
{
    QVERIFY(!SearchSupport::needsUnicode({QStringLiteral("SUBJECT"), QStringLiteral("cafe")}));
    QVERIFY(SearchSupport::needsUnicode({QStringLiteral("SUBJECT"), QStringLiteral("caf\u00E9")}));
    QVERIFY(SearchSupport::needsUnicode({QStringLiteral("FROM \"\u5F20\u4F1F\"")}));
    QVERIFY(!SearchSupport::needsUnicode({}));
}

void SearchSupportTest::explainsRejections_data()
{
    QTest::addColumn<int>("kind");
    QTest::addColumn<int>("code");
    QTest::addColumn<QStringList>("charsets");
    QTest::addColumn<QString>("serverText");
    QTest::addColumn<bool>("raw");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("bad raw query") << int(Responses::BAD) << int(Responses::NONE) << QStringList()
                                   << QStringLiteral("Unknown argument XFOO") << true
                                   << QStringList{QStringLiteral("IMAP search query"), QStringLiteral("The server said: Unknown argument XFOO")};
    QTest::newRow("bad generated search") << int(Responses::BAD) << int(Responses::NONE) << QStringList() << QString() << false
                                          << QStringList{QStringLiteral("did not understand the search.")};
    QTest::newRow("badcharset with charsets")
        << int(Responses::NO) << int(Responses::BADCHARSET) << QStringList{QStringLiteral("US-ASCII")} << QStringLiteral("nope") << false
        << QStringList{QStringLiteral("UTF-8"), QStringLiteral("accepts US-ASCII"), QStringLiteral("plain ASCII")};
    QTest::newRow("badcharset without charsets") << int(Responses::NO) << int(Responses::BADCHARSET) << QStringList() << QString()
                                                 << false << QStringList{QStringLiteral("cannot search for text in UTF-8.")};
    QTest::newRow("cannot") << int(Responses::NO) << int(Responses::CANNOT) << QStringList() << QStringLiteral("Too complex") << false
                            << QStringList{QStringLiteral("cannot run this search"), QStringLiteral("Too complex")};
    QTest::newRow("limit") << int(Responses::NO) << int(Responses::LIMIT) << QStringList() << QString() << false
                           << QStringList{QStringLiteral("limit of the server")};
    QTest::newRow("unavailable") << int(Responses::NO) << int(Responses::UNAVAILABLE) << QStringList() << QString() << false
                                 << QStringList{QStringLiteral("try again later")};
    QTest::newRow("other refusal") << int(Responses::NO) << int(Responses::NONE) << QStringList() << QStringLiteral("  Backend down ")
                                   << false << QStringList{QStringLiteral("refused the search. The server said: Backend down")};
}

void SearchSupportTest::explainsRejections()
{
    QFETCH(int, kind);
    QFETCH(int, code);
    QFETCH(QStringList, charsets);
    QFETCH(QString, serverText);
    QFETCH(bool, raw);
    QFETCH(QStringList, expected);
    const QString text = SearchSupport::explainRejection(Responses::Kind(kind), Responses::Code(code), charsets, serverText, raw);
    for (const QString &part : std::as_const(expected))
        QVERIFY2(text.contains(part), qPrintable(text));
}

void SearchSupportTest::explainsUnsupportedKeys()
{
    const QString text = SearchSupport::explainUnsupported(
        {{QStringLiteral("FUZZY"), QStringLiteral("SEARCH=FUZZY")}, {QStringLiteral("OLDER"), QStringLiteral("WITHIN")}});
    QVERIFY2(text.contains(QLatin1String("fuzzy matching (FUZZY needs SEARCH=FUZZY), OLDER (needs WITHIN)")), qPrintable(text));
    QVERIFY2(!SearchSupport::explainAsciiOnly().isEmpty(), "an explanation");
}

QTEST_GUILESS_MAIN(SearchSupportTest)
