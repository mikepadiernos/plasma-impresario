/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QtTest>

#include "test_SearchBuilder.h"
#include "UiUtils/SearchBuilder.h"

using namespace UiUtils::SearchBuilder;
using UiUtils::SearchQuery::Comparison;
using UiUtils::SearchQuery::Field;
using UiUtils::SearchQuery::Query;

Q_DECLARE_METATYPE(UiUtils::SearchBuilder::Condition)
Q_DECLARE_METATYPE(UiUtils::SearchBuilder::Match)

namespace {

const QDate today(2026, 3, 5);

Condition condition(Field field, Comparison comparison, bool negated, const QString &value)
{
    return Condition{field, comparison, negated, value};
}

Condition contains(Field field, const QString &value, bool negated = false)
{
    return condition(field, Comparison::Contains, negated, value);
}

}

void SearchBuilderTest::offersOperatorsPerField()
{
    const QList<Operator> text = operators(Field::Subject);
    QCOMPARE(text.size(), 2);
    QCOMPARE(text[0].comparison, Comparison::Contains);
    QVERIFY(!text[0].negated);
    QVERIFY(text[1].negated);

    const QList<Operator> dates = operators(Field::Date);
    QCOMPARE(dates.size(), 6);
    QVERIFY(std::none_of(dates.begin(), dates.end(), [](const Operator &op) {
        return op.negated && op.comparison != Comparison::Contains;
    }));
    const QList<Operator> sizes = operators(Field::Size);
    QCOMPARE(sizes.size(), 4);
    QVERIFY(std::none_of(sizes.begin(), sizes.end(), [](const Operator &op) {
        return op.negated || op.comparison == Comparison::Contains;
    }));

    QVERIFY(!needsValue(Field::Has));
    QVERIFY(needsValue(Field::Text));
    QVERIFY(needsValue(Field::Is));
}

void SearchBuilderTest::writesConditions_data()
{
    QTest::addColumn<Condition>("condition");
    QTest::addColumn<QString>("expected");
    QTest::newRow("word") << contains(Field::Text, QStringLiteral("lunch")) << QStringLiteral("lunch");
    QTest::newRow("phrase") << contains(Field::Text, QStringLiteral("release plan")) << QStringLiteral("\"release plan\"");
    QTest::newRow("trimmed") << contains(Field::From, QStringLiteral("  ann ")) << QStringLiteral("from:ann");
    QTest::newRow("negated") << contains(Field::Subject, QStringLiteral("report"), true) << QStringLiteral("-subject:report");
    QTest::newRow("quoted value") << contains(Field::Subject, QStringLiteral("Re: plan")) << QStringLiteral("subject:\"Re: plan\"");
    QTest::newRow("operator word") << contains(Field::Text, QStringLiteral("OR")) << QStringLiteral("\"OR\"");
    QTest::newRow("leading minus") << contains(Field::Text, QStringLiteral("-1")) << QStringLiteral("\"-1\"");
    QTest::newRow("list") << contains(Field::List, QStringLiteral("dev@lists.example.org")) << QStringLiteral("list:dev@lists.example.org");
    QTest::newRow("status") << contains(Field::Is, QStringLiteral("unread")) << QStringLiteral("is:unread");
    QTest::newRow("tag") << contains(Field::Tag, QStringLiteral("$label1"), true) << QStringLiteral("-tag:$label1");
    QTest::newRow("attachment") << contains(Field::Has, QString()) << QStringLiteral("has:attachment");
    QTest::newRow("no attachment") << contains(Field::Has, QString(), true) << QStringLiteral("-has:attachment");
    QTest::newRow("date in") << contains(Field::Date, QStringLiteral("2025-06")) << QStringLiteral("date:2025-06");
    QTest::newRow("date not in") << contains(Field::Date, QStringLiteral("today"), true) << QStringLiteral("-date:today");
    QTest::newRow("date before") << condition(Field::Date, Comparison::Less, false, QStringLiteral("2026"))
                                 << QStringLiteral("date<2026");
    QTest::newRow("date from") << condition(Field::Date, Comparison::GreaterOrEqual, false, QStringLiteral("2026-11-21"))
                               << QStringLiteral("date>=2026-11-21");
    QTest::newRow("size") << condition(Field::Size, Comparison::Greater, false, QStringLiteral("2M")) << QStringLiteral("size>2M");
    QTest::newRow("size at most") << condition(Field::Size, Comparison::LessOrEqual, false, QStringLiteral("500K"))
                                  << QStringLiteral("size<=500K");
    QTest::newRow("no value yet") << contains(Field::From, QStringLiteral("  ")) << QString();
}

void SearchBuilderTest::writesConditions()
{
    QFETCH(Condition, condition);
    QFETCH(QString, expected);
    QCOMPARE(conditionText(condition), expected);
    if (!expected.isEmpty())
        QVERIFY2(Query::parse(expected, today).isValid(), qPrintable(Query::parse(expected, today).error().message));
}

void SearchBuilderTest::joinsConditions()
{
    Conditions conditions;
    QCOMPARE(queryText(conditions), QString());
    conditions.conditions = {contains(Field::From, QStringLiteral("ann")), contains(Field::Subject, QString()),
                             contains(Field::Is, QStringLiteral("read"), true)};
    QCOMPARE(queryText(conditions), QStringLiteral("from:ann -is:read"));
    conditions.match = Match::Any;
    QCOMPARE(queryText(conditions), QStringLiteral("from:ann OR -is:read"));
    QCOMPARE(Query::parse(queryText(conditions), today).toString(), QStringLiteral("from:ann OR NOT is:read"));
}

void SearchBuilderTest::opensQueries_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<Match>("match");
    QTest::addColumn<QList<Condition>>("expected");
    QTest::newRow("empty") << QStringLiteral("  ") << Match::All << QList<Condition>{};
    QTest::newRow("one term") << QStringLiteral("from:ann") << Match::All << QList<Condition>{contains(Field::From, QStringLiteral("ann"))};
    QTest::newRow("negated term") << QStringLiteral("NOT has:attachment") << Match::All
                                  << QList<Condition>{contains(Field::Has, QStringLiteral("attachment"), true)};
    QTest::newRow("all") << QStringLiteral("from:ann -subject:\"release plan\" lunch") << Match::All
                         << QList<Condition>{contains(Field::From, QStringLiteral("ann")),
                                             contains(Field::Subject, QStringLiteral("release plan"), true),
                                             contains(Field::Text, QStringLiteral("lunch"))};
    QTest::newRow("redundant parentheses") << QStringLiteral("(a b) c") << Match::All
                                           << QList<Condition>{contains(Field::Text, QStringLiteral("a")),
                                                               contains(Field::Text, QStringLiteral("b")),
                                                               contains(Field::Text, QStringLiteral("c"))};
    QTest::newRow("any") << QStringLiteral("is:Starred OR -tag:$label1") << Match::Any
                         << QList<Condition>{contains(Field::Is, QStringLiteral("flagged")),
                                             contains(Field::Tag, QStringLiteral("$label1"), true)};
    QTest::newRow("date equal reads as in") << QStringLiteral("date=2025") << Match::All
                                            << QList<Condition>{contains(Field::Date, QStringLiteral("2025"))};
    QTest::newRow("negated date comparison turns around")
        << QStringLiteral("NOT date<2025-06 -size>2M") << Match::All
        << QList<Condition>{condition(Field::Date, Comparison::GreaterOrEqual, false, QStringLiteral("2025-06")),
                            condition(Field::Size, Comparison::LessOrEqual, false, QStringLiteral("2M"))};
    QTest::newRow("double negation") << QStringLiteral("NOT NOT from:ann") << Match::All
                                     << QList<Condition>{contains(Field::From, QStringLiteral("ann"))};
}

void SearchBuilderTest::opensQueries()
{
    QFETCH(QString, text);
    QFETCH(Match, match);
    QFETCH(QList<Condition>, expected);
    const Result result = fromQuery(text, today);
    QVERIFY2(result.isValid(), qPrintable(result.problem));
    QCOMPARE(result.conditions.match, match);
    QCOMPARE(result.conditions.conditions.size(), expected.size());
    for (int i = 0; i < expected.size(); ++i) {
        const Condition &actual = result.conditions.conditions[i];
        QVERIFY2(actual == expected[i], qPrintable(QStringLiteral("condition %1: %2").arg(i).arg(conditionText(actual))));
    }
}

void SearchBuilderTest::explainsQueriesWithoutBuilderForm_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("mentions");
    QTest::newRow("invalid") << QStringLiteral("from:") << QStringLiteral("not valid");
    QTest::newRow("unknown field") << QStringLiteral("foo:bar") << QStringLiteral("not valid");
    QTest::newRow("mixed AND and OR") << QStringLiteral("a OR b c") << QStringLiteral("a OR b AND c");
    QTest::newRow("group inside") << QStringLiteral("from:ann (a OR b)") << QStringLiteral("from:ann AND (a OR b)");
    QTest::newRow("negated group") << QStringLiteral("-(a OR b)") << QStringLiteral("NOT (a OR b)");
}

void SearchBuilderTest::explainsQueriesWithoutBuilderForm()
{
    QFETCH(QString, text);
    QFETCH(QString, mentions);
    const Result result = fromQuery(text, today);
    QVERIFY(!result.isValid());
    QVERIFY(result.conditions.conditions.isEmpty());
    QVERIFY2(result.problem.contains(mentions), qPrintable(result.problem));
}

void SearchBuilderTest::keepsTheMeaningOfOpenedQueries_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("expected");
    QTest::newRow("words") << QStringLiteral("release notes") << QStringLiteral("release notes");
    QTest::newRow("fields") << QStringLiteral("FROM:ann subject:\"release plan\"") << QStringLiteral("from:ann subject:\"release plan\"");
    QTest::newRow("any") << QStringLiteral("from:bob OR NOT from:carol") << QStringLiteral("from:bob OR -from:carol");
    QTest::newRow("dates and sizes") << QStringLiteral("date>=2026-11-21 size<=2K") << QStringLiteral("date>=2026-11-21 size<=2K");
    QTest::newRow("subject prefix") << QStringLiteral("Re: plan") << QStringLiteral("\"Re:\" plan");
    QTest::newRow("escapes") << QStringLiteral("\"say \\\"hi\\\"\"") << QStringLiteral("\"say \\\"hi\\\"\"");
}

void SearchBuilderTest::keepsTheMeaningOfOpenedQueries()
{
    QFETCH(QString, text);
    QFETCH(QString, expected);
    const Result result = fromQuery(text, today);
    QVERIFY2(result.isValid(), qPrintable(result.problem));
    const QString written = queryText(result.conditions);
    QCOMPARE(written, expected);
    const UiUtils::SearchScope::Fields fields(UiUtils::SearchScope::Field::Subject);
    QCOMPARE(Query::parse(written, today).imapConditions(fields, false), Query::parse(text, today).imapConditions(fields, false));
}

QTEST_GUILESS_MAIN(SearchBuilderTest)
