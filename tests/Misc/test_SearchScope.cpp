/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QtTest>

#include "test_SearchScope.h"
#include "UiUtils/SearchScope.h"

using namespace UiUtils::SearchScope;

namespace {

const QDate today(2026, 3, 5);

QStringList keys(const char *text)
{
    return QString::fromLatin1(text).split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

}

void SearchScopeTest::formatsImapDates_data()
{
    QTest::addColumn<QDate>("date");
    QTest::addColumn<QString>("expected");
    QTest::newRow("single-digit day") << QDate(2026, 3, 5) << QStringLiteral("5-Mar-2026");
    QTest::newRow("two-digit day") << QDate(2025, 12, 31) << QStringLiteral("31-Dec-2025");
    QTest::newRow("january") << QDate(2026, 1, 1) << QStringLiteral("1-Jan-2026");
    QTest::newRow("invalid") << QDate() << QString();
}

void SearchScopeTest::formatsImapDates()
{
    QFETCH(QDate, date);
    QFETCH(QString, expected);
    // Month names are IMAP's, not the user's language
    const QLocale previous;
    QLocale::setDefault(QLocale(QLocale::German));
    QCOMPARE(imapDate(date), expected);
    QLocale::setDefault(previous);
}

void SearchScopeTest::resolvesPresets_data()
{
    QTest::addColumn<int>("dates");
    QTest::addColumn<QDate>("first");
    QTest::addColumn<QDate>("last");
    QTest::newRow("any time") << int(Dates::AnyTime) << QDate() << QDate();
    QTest::newRow("today") << int(Dates::Today) << today << today;
    QTest::newRow("past week") << int(Dates::PastWeek) << QDate(2026, 2, 26) << today;
    QTest::newRow("past month") << int(Dates::PastMonth) << QDate(2026, 2, 5) << today;
    QTest::newRow("past year") << int(Dates::PastYear) << QDate(2025, 3, 5) << today;
}

void SearchScopeTest::resolvesPresets()
{
    QFETCH(int, dates);
    QFETCH(QDate, first);
    QFETCH(QDate, last);
    DateRange range;
    range.dates = Dates(dates);
    // Presets ignore the custom days
    range.from = QDate(1999, 1, 1);
    range.to = QDate(1999, 1, 2);
    const Days days = resolve(range, today);
    QCOMPARE(days.first, first);
    QCOMPARE(days.last, last);
    QCOMPARE(isLimited(range), first.isValid());
}

void SearchScopeTest::resolvesCustomRanges()
{
    DateRange range{Dates::Custom, QDate(2026, 1, 10), QDate(2026, 1, 20)};
    QCOMPARE(resolve(range, today).first, QDate(2026, 1, 10));
    QCOMPARE(resolve(range, today).last, QDate(2026, 1, 20));

    // Days given the wrong way round still make a range
    range = {Dates::Custom, QDate(2026, 1, 20), QDate(2026, 1, 10)};
    QCOMPARE(resolve(range, today).first, QDate(2026, 1, 10));
    QCOMPARE(resolve(range, today).last, QDate(2026, 1, 20));

    // Open ends
    range = {Dates::Custom, QDate(2026, 1, 10), QDate()};
    QCOMPARE(resolve(range, today).first, QDate(2026, 1, 10));
    QVERIFY(!resolve(range, today).last.isValid());
    QVERIFY(isLimited(range));
    range = {Dates::Custom, QDate(), QDate()};
    QVERIFY(!isLimited(range));
    QVERIFY(dateConditions(range, today).isEmpty());
}

void SearchScopeTest::buildsDateConditions()
{
    QCOMPARE(dateConditions({}, today), QStringList());
    QCOMPARE(dateConditions({Dates::Today, {}, {}}, today), keys("SENTSINCE 5-Mar-2026 SENTBEFORE 6-Mar-2026"));
    QCOMPARE(dateConditions({Dates::PastWeek, {}, {}}, today), keys("SENTSINCE 26-Feb-2026 SENTBEFORE 6-Mar-2026"));
    QCOMPARE(dateConditions({Dates::Custom, QDate(2026, 1, 10), QDate()}, today), keys("SENTSINCE 10-Jan-2026"));
    // The last day is included, across the end of a month
    QCOMPARE(dateConditions({Dates::Custom, QDate(), QDate(2026, 1, 31)}, today), keys("SENTBEFORE 1-Feb-2026"));
}

void SearchScopeTest::buildsTextConditions()
{
    QCOMPARE(textConditions(QString(), Field::Subject, false), QStringList());
    QCOMPARE(textConditions(QStringLiteral("plan"), Fields(), false), QStringList());
    QCOMPARE(textConditions(QStringLiteral("plan"), Field::Subject, false), keys("SUBJECT plan"));
    QCOMPARE(textConditions(QStringLiteral("plan"), Field::Subject | Field::Senders, false), keys("OR SUBJECT plan FROM plan"));
    QCOMPARE(textConditions(QStringLiteral("plan"), Field::Body | Field::Recipients, false),
             keys("OR OR OR BODY plan TO plan CC plan BCC plan"));
    QCOMPARE(textConditions(QStringLiteral("plan"), Field::Subject | Field::Senders, true),
             keys("OR FUZZY SUBJECT plan FUZZY FROM plan"));
    // The text stays one value, spaces and all
    const QStringList phrase = textConditions(QStringLiteral("quarterly plan"), Field::Subject, false);
    QCOMPARE(phrase, (QStringList{QStringLiteral("SUBJECT"), QStringLiteral("quarterly plan")}));
}

void SearchScopeTest::combinesTextAndDates()
{
    const DateRange week{Dates::PastWeek, {}, {}};
    QCOMPARE(conditions(QStringLiteral("plan"), Field::Subject | Field::Senders, false, week, today),
             keys("OR SUBJECT plan FROM plan SENTSINCE 26-Feb-2026 SENTBEFORE 6-Mar-2026"));
    // A date range alone is a search
    QCOMPARE(conditions(QString(), Field::Subject, false, week, today), keys("SENTSINCE 26-Feb-2026 SENTBEFORE 6-Mar-2026"));
    QCOMPARE(conditions(QString(), Field::Subject, false, {}, today), QStringList());
    // Text without fields matches nothing, so the dates do not turn it into a search
    QCOMPARE(conditions(QStringLiteral("plan"), Fields(), false, week, today), QStringList());
}

void SearchScopeTest::extendsRawQueries()
{
    const DateRange today_{Dates::Today, {}, {}};
    QCOMPARE(withDateConditions(QStringLiteral("OR FROM ann TO ann "), today_, today),
             QStringLiteral("OR FROM ann TO ann SENTSINCE 5-Mar-2026 SENTBEFORE 6-Mar-2026"));
    QCOMPARE(withDateConditions(QStringLiteral("SEEN"), {}, today), QStringLiteral("SEEN"));
    QCOMPARE(withDateConditions(QStringLiteral("  "), today_, today), QStringLiteral("  "));
}

void SearchScopeTest::configNamesRoundTrip()
{
    for (const Dates dates : {Dates::AnyTime, Dates::Today, Dates::PastWeek, Dates::PastMonth, Dates::PastYear, Dates::Custom})
        QCOMPARE(datesFromConfigName(configName(dates)), dates);
    QCOMPARE(datesFromConfigName(QStringLiteral("fortnight")), Dates::AnyTime);
    QCOMPARE(datesFromConfigName(QString()), Dates::AnyTime);
}

QTEST_GUILESS_MAIN(SearchScopeTest)
