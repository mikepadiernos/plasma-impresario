/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "SearchScope.h"

#include <array>
#include <utility>

namespace UiUtils {
namespace SearchScope {

namespace {

constexpr std::array<std::pair<Dates, const char *>, 6> names{{
    {Dates::AnyTime, "any"},
    {Dates::Today, "today"},
    {Dates::PastWeek, "week"},
    {Dates::PastMonth, "month"},
    {Dates::PastYear, "year"},
    {Dates::Custom, "custom"},
}};

}

Days resolve(const DateRange &range, const QDate &today)
{
    switch (range.dates) {
    case Dates::AnyTime:
        return {};
    case Dates::Today:
        return {today, today};
    case Dates::PastWeek:
        return {today.addDays(-7), today};
    case Dates::PastMonth:
        return {today.addMonths(-1), today};
    case Dates::PastYear:
        return {today.addYears(-1), today};
    case Dates::Custom:
        break;
    }
    QDate first = range.from.isValid() ? range.from : QDate();
    QDate last = range.to.isValid() ? range.to : QDate();
    if (first.isValid() && last.isValid() && first > last)
        std::swap(first, last);
    return {first, last};
}

bool isLimited(const DateRange &range)
{
    const Days days = resolve(range, QDate(2000, 1, 1));
    return days.first.isValid() || days.last.isValid();
}

QString imapDate(const QDate &date)
{
    static const char *const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (!date.isValid() || date.year() < 1)
        return QString();
    return QStringLiteral("%1-%2-%3").arg(date.day()).arg(QLatin1String(months[date.month() - 1])).arg(date.year(), 4, 10, QLatin1Char('0'));
}

QStringList dateConditions(const DateRange &range, const QDate &today)
{
    const Days days = resolve(range, today);
    QStringList keys;
    if (days.first.isValid())
        keys << QStringLiteral("SENTSINCE") << imapDate(days.first);
    // SENTBEFORE excludes its day, so the last day needs the day after it
    if (days.last.isValid())
        keys << QStringLiteral("SENTBEFORE") << imapDate(days.last.addDays(1));
    return keys;
}

QStringList textConditions(const QString &text, const Fields fields, const bool fuzzy)
{
    if (text.isEmpty())
        return {};
    QStringList keys;
    if (fields & Field::Subject)
        keys << QStringLiteral("SUBJECT");
    if (fields & Field::Body)
        keys << QStringLiteral("BODY");
    if (fields & Field::Recipients)
        keys << QStringLiteral("TO") << QStringLiteral("CC") << QStringLiteral("BCC");
    if (fields & Field::Senders)
        keys << QStringLiteral("FROM");

    QStringList result;
    for (const QString &key : std::as_const(keys)) {
        if (fuzzy)
            result << QStringLiteral("FUZZY");
        result << key << text;
    }
    // OR takes two operands in prefix notation, so n alternatives need n - 1 of them in front
    for (qsizetype i = 1; i < keys.size(); ++i)
        result.prepend(QStringLiteral("OR"));
    return result;
}

QStringList conditions(const QString &text, const Fields fields, const bool fuzzy, const DateRange &range, const QDate &today)
{
    QStringList result = textConditions(text, fields, fuzzy);
    // Text which no field is chosen for finds nothing, whatever the dates say
    if (!text.isEmpty() && result.isEmpty())
        return result;
    return result + dateConditions(range, today);
}

QString withDateConditions(const QString &rawQuery, const DateRange &range, const QDate &today)
{
    const QStringList dates = dateConditions(range, today);
    if (dates.isEmpty() || rawQuery.trimmed().isEmpty())
        return rawQuery;
    // Keys in a row must all match, so the dates narrow whatever the query matches
    return rawQuery.trimmed() + QLatin1Char(' ') + dates.join(QLatin1Char(' '));
}

QString configName(const Dates dates)
{
    for (const auto &[value, name] : names) {
        if (value == dates)
            return QLatin1String(name);
    }
    return QStringLiteral("any");
}

Dates datesFromConfigName(const QString &name)
{
    for (const auto &[value, known] : names) {
        if (name == QLatin1String(known))
            return value;
    }
    return Dates::AnyTime;
}

}
}
