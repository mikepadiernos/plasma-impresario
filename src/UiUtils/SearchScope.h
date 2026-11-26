/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDate>
#include <QFlags>
#include <QString>
#include <QStringList>
#include <QList>

/** @short What the quick search looks at: message fields, a date range and places

The quick search turns its text, the chosen fields and the date range into IMAP
SEARCH keys. All keys used here are part of IMAP4rev1 (RFC 3501), so every
server supports them; FUZZY (RFC 6203) is added only where the server
advertises it. The date range compares the date in the Date header, as the
message list shows it, through SENTSINCE and SENTBEFORE.

The places say which folders are searched: the shown folder, folders chosen by
the user, every folder of the shown account or every folder of all accounts.
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

/** @short Where the quick search looks for messages */
enum class Places {
    /** The shown list: one folder, or the INBOX of each account in the Unified Inbox */
    CurrentFolder,
    /** Folders which the user picked, of any account */
    SelectedFolders,
    /** Every folder of the shown account */
    Account,
    /** Every folder of every account */
    AllAccounts,
};

QString configName(Places places);
Places placesFromConfigName(const QString &name);

/** @short One folder of one account */
struct Folder {
    QString accountId;
    QString mailbox;

    bool operator==(const Folder &other) const = default;
};

/** @short A folder as one string for storing it, "accountId/mailbox"; account IDs contain no slash */
QString folderKey(const Folder &folder);
/** @short The folder of a key made by folderKey(); an invalid key gives empty fields */
Folder folderFromKey(const QString &key);

/** @short The folders of one account which a search looks at */
struct AccountFolders {
    QString accountId;
    /** Full mailbox names, without duplicates; empty when allFolders is set */
    QStringList mailboxes;
    /** Search every folder of the account, as listed on the server */
    bool allFolders = false;

    bool operator==(const AccountFolders &other) const = default;
};

/** @short What a search in @arg places looks at, account by account in the order of @arg accounts

@arg activeAccount is the shown account, @arg shown the folders of the shown list (several in the Unified Inbox) and
@arg selected the folders picked for SelectedFolders. Folders of accounts which are not in @arg accounts are left out,
as are accounts with nothing to search.
*/
QList<AccountFolders> foldersToSearch(Places places, const QString &activeAccount, const QList<Folder> &shown,
                                      const QList<Folder> &selected, const QStringList &accounts);

/** @short @arg mailboxes in the order they are searched: INBOX first, then the others as given */
QStringList searchOrder(const QStringList &mailboxes);

}
}

Q_DECLARE_OPERATORS_FOR_FLAGS(UiUtils::SearchScope::Fields)
