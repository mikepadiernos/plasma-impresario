/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDate>
#include <QFlags>
#include <QString>
#include <QStringList>

/** @short What the quick search looks at: message fields and a date range

The quick search turns its text, the chosen fields and the date range into IMAP
SEARCH keys. All keys used here are part of IMAP4rev1 (RFC 3501), so every
server supports them; FUZZY (RFC 6203) is added only where the server
advertises it. The date range compares the date in the Date header, as the
message list shows it, through SENTSINCE and SENTBEFORE.
*/
namespace UiUtils {
namespace SearchScope {

enum class Field {
    Subject = 0x1,
    Body = 0x2,
    Senders = 0x4,
    Recipients = 0x8,
};
Q_DECLARE_FLAGS(Fields, Field)

/** @short Preset date ranges; each ends today, except AnyTime and Custom */
enum class Dates {
    AnyTime,
    Today,
    PastWeek,
    PastMonth,
    PastYear,
    Custom,
};

struct DateRange {
    Dates dates = Dates::AnyTime;
    /** @short First and last day of a Custom range, inclusive; a null date leaves that end open */
    QDate from;
    QDate to;

    bool operator==(const DateRange &other) const = default;
};

/** @short The first and last day of @arg range, inclusive, as of @arg today; a null date is an open end */
struct Days {
    QDate first;
    QDate last;
};
Days resolve(const DateRange &range, const QDate &today);

/** @short Whether @arg range limits the search at all */
bool isLimited(const DateRange &range);

/** @short A date in IMAP's date format, such as "5-Mar-2026", with English month names in any locale */
QString imapDate(const QDate &date);

/** @short SENTSINCE and SENTBEFORE keys for @arg range, or an empty list when it does not limit the search */
QStringList dateConditions(const DateRange &range, const QDate &today);

/** @short Keys which match @arg text in any of @arg fields, joined by OR; empty without text or fields */
QStringList textConditions(const QString &text, Fields fields, bool fuzzy);

/** @short The text and date keys together; the date range alone also makes a search */
QStringList conditions(const QString &text, Fields fields, bool fuzzy, const DateRange &range, const QDate &today);

/** @short Add the date keys to a raw IMAP query typed by the user, keeping it a single query string */
QString withDateConditions(const QString &rawQuery, const DateRange &range, const QDate &today);

/** @short Stable names for storing a range in a configuration file */
QString configName(Dates dates);
Dates datesFromConfigName(const QString &name);

}
}

Q_DECLARE_OPERATORS_FOR_FLAGS(UiUtils::SearchScope::Fields)
