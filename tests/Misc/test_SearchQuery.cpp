/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QtTest>

#include "test_SearchQuery.h"
#include "UiUtils/SearchQuery.h"

using namespace UiUtils::SearchQuery;
using UiUtils::SearchScope::Fields;
using SearchField = UiUtils::SearchScope::Field;

namespace {

const QDate today(2026, 3, 5);

/** Items separated by "|", so that a value may contain spaces */
QStringList items(const QString &text)
{
    return text.split(QLatin1Char('|'), Qt::SkipEmptyParts);
}

}

void SearchQueryTest::treatsBlankTextAsEmpty()
{
    for (const QString &text : {QString(), QStringLiteral("   ")}) {
        const Query query = Query::parse(text, today);
        QVERIFY(query.isEmpty());
        QVERIFY(query.isValid());
        QVERIFY(query.toString().isEmpty());
        QVERIFY(query.imapConditions(Fields(SearchField::Subject), false).isEmpty());
    }
}

void SearchQueryTest::normalizes_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("expected");
    QTest::newRow("word") << QStringLiteral("lunch") << QStringLiteral("lunch");
    QTest::newRow("words are ANDed") << QStringLiteral("release notes") << QStringLiteral("release AND notes");
    QTest::newRow("AND flattens") << QStringLiteral("a AND b AND c") << QStringLiteral("a AND b AND c");
    QTest::newRow("redundant parentheses") << QStringLiteral("a (b c)") << QStringLiteral("a AND b AND c");
    QTest::newRow("AND binds tighter") << QStringLiteral("a OR b c") << QStringLiteral("a OR b AND c");
    QTest::newRow("grouped OR") << QStringLiteral("from:ann (a OR b)") << QStringLiteral("from:ann AND (a OR b)");
    QTest::newRow("phrase and negation") << QStringLiteral("from:ann subject:\"release plan\" OR -is:read")
                                         << QStringLiteral("from:ann AND subject:\"release plan\" OR NOT is:read");
    QTest::newRow("negated group") << QStringLiteral("-(a OR b)") << QStringLiteral("NOT (a OR b)");
    QTest::newRow("double negation") << QStringLiteral("NOT NOT from:ann") << QStringLiteral("from:ann");
    QTest::newRow("field names ignore case") << QStringLiteral("FROM:ann Subject:x") << QStringLiteral("from:ann AND subject:x");
    QTest::newRow("is aliases") << QStringLiteral("is:Starred is:seen") << QStringLiteral("is:flagged AND is:read");
    QTest::newRow("has plural") << QStringLiteral("has:attachments") << QStringLiteral("has:attachment");
    QTest::newRow("Plasma Opus field names") << QStringLiteral("content:invoice Contents:\"due date\" tags:work")
                                            << QStringLiteral("body:invoice AND body:\"due date\" AND tag:work");
    QTest::newRow("dates") << QStringLiteral("date>=2025-06 date:today") << QStringLiteral("date>=2025-06 AND date:today");
    QTest::newRow("sizes") << QStringLiteral("size>2M") << QStringLiteral("size>2M");
    QTest::newRow("subject prefix is text") << QStringLiteral("Re: lunch") << QStringLiteral("\"Re:\" AND lunch");
    QTest::newRow("escapes") << QStringLiteral("\"say \\\"hi\\\" \\\\o/\"") << QStringLiteral("\"say \\\"hi\\\" \\\\o/\"");
    QTest::newRow("quoted operator") << QStringLiteral("\"OR\" lunch") << QStringLiteral("\"OR\" AND lunch");
    QTest::newRow("lower-case operators are words") << QStringLiteral("salt and pepper") << QStringLiteral("salt AND and AND pepper");
    QTest::newRow("lone minus is a word") << QStringLiteral("a - b") << QStringLiteral("a AND \"-\" AND b");
    QTest::newRow("hyphenated word") << QStringLiteral("e-mail") << QStringLiteral("e-mail");
}

void SearchQueryTest::normalizes()
{
    QFETCH(QString, text);
    QFETCH(QString, expected);
    const Query query = Query::parse(text, today);
    QVERIFY2(query.isValid(), qPrintable(query.error().message));
    QVERIFY(!query.isEmpty());
    QCOMPARE(query.toString(), expected);
    // The normal form reads back as the same query
    QCOMPARE(Query::parse(expected, today).toString(), expected);
}

void SearchQueryTest::translatesToImap_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("expected");
    QTest::newRow("word") << QStringLiteral("release") << QStringLiteral("SUBJECT|release");
    QTest::newRow("words") << QStringLiteral("release notes") << QStringLiteral("SUBJECT|release|SUBJECT|notes");
    QTest::newRow("phrase") << QStringLiteral("\"release plan\"") << QStringLiteral("SUBJECT|release plan");
    QTest::newRow("from") << QStringLiteral("from:ann") << QStringLiteral("FROM|ann");
    QTest::newRow("recipients") << QStringLiteral("to:a cc:b bcc:c") << QStringLiteral("TO|a|CC|b|BCC|c");
    QTest::newRow("body and text") << QStringLiteral("body:a text:b") << QStringLiteral("BODY|a|TEXT|b");
    QTest::newRow("negation") << QStringLiteral("from:ann -subject:report") << QStringLiteral("FROM|ann|NOT|SUBJECT|report");
    QTest::newRow("OR") << QStringLiteral("from:ann OR from:bob") << QStringLiteral("OR|FROM|ann|FROM|bob");
    QTest::newRow("three alternatives") << QStringLiteral("from:ann OR from:bob OR from:carol")
                                        << QStringLiteral("OR|OR|FROM|ann|FROM|bob|FROM|carol");
    QTest::newRow("OR and a flag") << QStringLiteral("(from:ann OR from:bob) is:unread")
                                   << QStringLiteral("OR|FROM|ann|FROM|bob|UNSEEN");
    QTest::newRow("AND inside OR") << QStringLiteral("(from:ann subject:release) OR is:flagged")
                                   << QStringLiteral("OR|NOT|OR|NOT|FROM|ann|NOT|SUBJECT|release|FLAGGED");
    QTest::newRow("precedence") << QStringLiteral("a OR b c")
                                << QStringLiteral("OR|SUBJECT|a|NOT|OR|NOT|SUBJECT|b|NOT|SUBJECT|c");
    QTest::newRow("negated AND") << QStringLiteral("NOT (from:ann subject:x)")
                                 << QStringLiteral("OR|NOT|FROM|ann|NOT|SUBJECT|x");
    QTest::newRow("negated OR") << QStringLiteral("-(from:ann OR from:bob)") << QStringLiteral("NOT|OR|FROM|ann|FROM|bob");
    QTest::newRow("negated negation inside OR") << QStringLiteral("-is:read OR from:ann") << QStringLiteral("OR|NOT|SEEN|FROM|ann");
    QTest::newRow("flags") << QStringLiteral("is:read is:flagged is:unflagged is:answered is:unanswered is:draft is:deleted")
                           << QStringLiteral("SEEN|FLAGGED|UNFLAGGED|ANSWERED|UNANSWERED|DRAFT|DELETED");
    QTest::newRow("forwarded") << QStringLiteral("is:forwarded") << QStringLiteral("KEYWORD|$Forwarded");
    QTest::newRow("tag") << QStringLiteral("tag:$label1") << QStringLiteral("KEYWORD|$label1");
    QTest::newRow("attachment") << QStringLiteral("has:attachment") << QStringLiteral("HEADER|Content-Type|multipart/mixed");
    QTest::newRow("list") << QStringLiteral("list:dev@lists.example.test")
                          << QStringLiteral("OR|HEADER|List-Id|dev@lists.example.test|HEADER|List-Post|dev@lists.example.test");
    QTest::newRow("day") << QStringLiteral("date:2025-06-10") << QStringLiteral("SENTSINCE|10-Jun-2025|SENTBEFORE|11-Jun-2025");
    QTest::newRow("month") << QStringLiteral("date=2025-06") << QStringLiteral("SENTSINCE|1-Jun-2025|SENTBEFORE|1-Jul-2025");
    QTest::newRow("year") << QStringLiteral("date:2025") << QStringLiteral("SENTSINCE|1-Jan-2025|SENTBEFORE|1-Jan-2026");
    QTest::newRow("before") << QStringLiteral("date<2025") << QStringLiteral("SENTBEFORE|1-Jan-2025");
    QTest::newRow("until") << QStringLiteral("date<=2025-06-10") << QStringLiteral("SENTBEFORE|11-Jun-2025");
    QTest::newRow("after") << QStringLiteral("date>2025-06-10") << QStringLiteral("SENTSINCE|11-Jun-2025");
    QTest::newRow("since") << QStringLiteral("date>=2025-06-10") << QStringLiteral("SENTSINCE|10-Jun-2025");
    QTest::newRow("today") << QStringLiteral("date:today") << QStringLiteral("SENTSINCE|5-Mar-2026|SENTBEFORE|6-Mar-2026");
    QTest::newRow("yesterday") << QStringLiteral("date:Yesterday") << QStringLiteral("SENTSINCE|4-Mar-2026|SENTBEFORE|5-Mar-2026");
    QTest::newRow("negated day") << QStringLiteral("-date:2025-06-10")
                                 << QStringLiteral("OR|NOT|SENTSINCE|10-Jun-2025|NOT|SENTBEFORE|11-Jun-2025");
    QTest::newRow("larger") << QStringLiteral("size>2K") << QStringLiteral("LARGER|2048");
    QTest::newRow("at least") << QStringLiteral("size>=2K") << QStringLiteral("LARGER|2047");
    QTest::newRow("smaller") << QStringLiteral("size<1M") << QStringLiteral("SMALLER|1048576");
    QTest::newRow("at most") << QStringLiteral("size<=1MB") << QStringLiteral("SMALLER|1048577");
    QTest::newRow("fractions and bytes") << QStringLiteral("size>1.5k size<900") << QStringLiteral("LARGER|1536|SMALLER|900");
    QTest::newRow("nothing is smaller than nothing") << QStringLiteral("size<0") << QStringLiteral("NOT|ALL");
    QTest::newRow("everything is at least nothing") << QStringLiteral("size>=0") << QStringLiteral("ALL");
    QTest::newRow("subject prefix") << QStringLiteral("Re: lunch") << QStringLiteral("SUBJECT|Re:|SUBJECT|lunch");
    QTest::newRow("escaped quote") << QStringLiteral("subject:\"say \\\"hi\\\"\"") << QStringLiteral("SUBJECT|say \"hi\"");
    QTest::newRow("quoted operator") << QStringLiteral("\"OR\"") << QStringLiteral("SUBJECT|OR");
}

void SearchQueryTest::translatesToImap()
{
    QFETCH(QString, text);
    QFETCH(QString, expected);
    const Query query = Query::parse(text, today);
    QVERIFY2(query.isValid(), qPrintable(query.error().message));
    Error problem{QStringLiteral("stale"), 3, 1};
    QCOMPARE(query.imapConditions(Fields(SearchField::Subject), false, &problem), items(expected));
    QVERIFY(problem.isEmpty());
}

void SearchQueryTest::searchesTheChosenFields()
{
    const Fields fields = Fields(SearchField::Subject) | SearchField::Senders;
    QCOMPARE(Query::parse(QStringLiteral("ann"), today).imapConditions(fields, false),
             items(QStringLiteral("OR|SUBJECT|ann|FROM|ann")));
    QCOMPARE(Query::parse(QStringLiteral("-ann"), today).imapConditions(fields, false),
             items(QStringLiteral("NOT|OR|SUBJECT|ann|FROM|ann")));
    QCOMPARE(Query::parse(QStringLiteral("ann"), today).imapConditions(Fields(SearchField::Subject), true),
             items(QStringLiteral("FUZZY|SUBJECT|ann")));
    // A named field is searched exactly, even when the plain words are fuzzy
    QCOMPARE(Query::parse(QStringLiteral("from:ann"), today).imapConditions(Fields(SearchField::Subject), true),
             items(QStringLiteral("FROM|ann")));
}

void SearchQueryTest::needsAFieldForPlainWords()
{
    const Query query = Query::parse(QStringLiteral("from:ann lunch"), today);
    QVERIFY(query.isValid());
    Error problem;
    QVERIFY(query.imapConditions(Fields(), false, &problem).isEmpty());
    QVERIFY(problem.message.contains(QLatin1String("subject:lunch")));
    QCOMPARE(problem.position, 9);
    QCOMPARE(problem.length, 5);
    // Fields alone need no choice
    QCOMPARE(Query::parse(QStringLiteral("from:ann"), today).imapConditions(Fields(), false, &problem),
             items(QStringLiteral("FROM|ann")));
    QVERIFY(problem.isEmpty());
}

void SearchQueryTest::rejectsInvalidQueries_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<int>("position");
    QTest::addColumn<int>("length");
    QTest::addColumn<QString>("message");
    QTest::newRow("missing value") << QStringLiteral("from:") << 0 << 5 << QStringLiteral("Enter a value after from:");
    QTest::newRow("unclosed parenthesis") << QStringLiteral("(from:ann") << 0 << 1 << QStringLiteral("not closed");
    QTest::newRow("unopened parenthesis") << QStringLiteral("from:ann)") << 8 << 1 << QStringLiteral("no matching opening");
    QTest::newRow("empty parentheses") << QStringLiteral("a ()") << 2 << 2 << QStringLiteral("must contain");
    QTest::newRow("leading AND") << QStringLiteral("AND lunch") << 0 << 3 << QStringLiteral("between two search terms");
    QTest::newRow("trailing AND") << QStringLiteral("lunch AND") << 6 << 3 << QStringLiteral("after AND");
    QTest::newRow("trailing OR") << QStringLiteral("lunch OR") << 6 << 2 << QStringLiteral("after OR");
    QTest::newRow("OR before a parenthesis") << QStringLiteral("(a OR)") << 3 << 2 << QStringLiteral("after OR");
    QTest::newRow("double OR") << QStringLiteral("a OR OR b") << 5 << 2 << QStringLiteral("between two search terms");
    QTest::newRow("lone NOT") << QStringLiteral("NOT") << 0 << 3 << QStringLiteral("after NOT");
    QTest::newRow("unknown field") << QStringLiteral("a foo:bar") << 2 << 7 << QStringLiteral("Unknown field");
    QTest::newRow("unclosed quote") << QStringLiteral("a \"open") << 2 << 5 << QStringLiteral("not closed");
    QTest::newRow("empty quotes") << QStringLiteral("\"\"") << 0 << 2 << QStringLiteral("inside the quotation marks");
    QTest::newRow("quote inside a word") << QStringLiteral("a\"b\"") << 0 << 2 << QStringLiteral("whole value");
    QTest::newRow("text after a quote") << QStringLiteral("\"a\"b") << 0 << 4 << QStringLiteral("Separate");
    QTest::newRow("size without comparison") << QStringLiteral("size:2M") << 0 << 7 << QStringLiteral("compare with");
    QTest::newRow("bad size") << QStringLiteral("size>lots") << 0 << 9 << QStringLiteral("not a valid size");
    QTest::newRow("size over 32 bits") << QStringLiteral("size>5G") << 0 << 7 << QStringLiteral("not a valid size");
    QTest::newRow("bad date") << QStringLiteral("date:2025-13-01") << 0 << 15 << QStringLiteral("not a valid date");
    QTest::newRow("vague date") << QStringLiteral("date:soon") << 0 << 9 << QStringLiteral("not a valid date");
    QTest::newRow("bad comparison") << QStringLiteral("date=>2025") << 0 << 10 << QStringLiteral("Unknown comparison");
    QTest::newRow("ordered text") << QStringLiteral("from>ann") << 0 << 8 << QStringLiteral("Only date and size");
    QTest::newRow("bad is") << QStringLiteral("is:important") << 0 << 12 << QStringLiteral("not a valid is:");
    QTest::newRow("bad has") << QStringLiteral("has:pictures") << 0 << 12 << QStringLiteral("has:attachment");
    QTest::newRow("bad tag") << QStringLiteral("tag:\"two words\"") << 0 << 15 << QStringLiteral("not a valid tag");
}

void SearchQueryTest::rejectsInvalidQueries()
{
    QFETCH(QString, text);
    QFETCH(int, position);
    QFETCH(int, length);
    QFETCH(QString, message);
    const Query query = Query::parse(text, today);
    QVERIFY(!query.isEmpty());
    QVERIFY(!query.isValid());
    QVERIFY2(query.error().message.contains(message), qPrintable(query.error().message));
    QCOMPARE(query.error().position, position);
    QCOMPARE(query.error().length, length);
    QVERIFY(query.toString().isEmpty());
    Error problem;
    QVERIFY(query.imapConditions(Fields(SearchField::Subject), false, &problem).isEmpty());
    QCOMPARE(problem.message, query.error().message);
}

void SearchQueryTest::tracksRelativeDates()
{
    QVERIFY(Query::parse(QStringLiteral("from:ann date:today"), today).usesRelativeDates());
    QVERIFY(Query::parse(QStringLiteral("date>=yesterday"), today).usesRelativeDates());
    QVERIFY(!Query::parse(QStringLiteral("date:2025"), today).usesRelativeDates());
    QVERIFY(!Query::parse(QStringLiteral("today"), today).usesRelativeDates());
}

void SearchQueryTest::quotesValues_data()
{
    QTest::addColumn<QString>("value");
    QTest::addColumn<QString>("expected");
    QTest::newRow("plain") << QStringLiteral("ann@example.org") << QStringLiteral("ann@example.org");
    QTest::newRow("space") << QStringLiteral("release plan") << QStringLiteral("\"release plan\"");
    QTest::newRow("minus") << QStringLiteral("-x") << QStringLiteral("\"-x\"");
    QTest::newRow("operator") << QStringLiteral("NOT") << QStringLiteral("\"NOT\"");
    QTest::newRow("lower-case operator") << QStringLiteral("not") << QStringLiteral("not");
    QTest::newRow("colon") << QStringLiteral("Re:") << QStringLiteral("\"Re:\"");
    QTest::newRow("comparison") << QStringLiteral("a<b") << QStringLiteral("\"a<b\"");
    QTest::newRow("parenthesis") << QStringLiteral("(draft)") << QStringLiteral("\"(draft)\"");
    QTest::newRow("escapes") << QStringLiteral("a\"b\\c") << QStringLiteral("\"a\\\"b\\\\c\"");
    QTest::newRow("empty") << QString() << QStringLiteral("\"\"");
}

void SearchQueryTest::quotesValues()
{
    QFETCH(QString, value);
    QFETCH(QString, expected);
    QCOMPARE(quotedValue(value), expected);
    if (!value.isEmpty()) {
        const Query query = Query::parse(QStringLiteral("subject:") + expected, today);
        QVERIFY(query.isValid());
        QCOMPARE(query.root().term.value, value);
    }
}

void SearchQueryTest::namesFields()
{
    QCOMPARE(fieldName(Field::From), QStringLiteral("from"));
    QCOMPARE(fieldName(Field::Message), QStringLiteral("text"));
    QCOMPARE(fieldName(Field::List), QStringLiteral("list"));
    QVERIFY(fieldName(Field::Text).isEmpty());
}

QTEST_GUILESS_MAIN(SearchQueryTest)
