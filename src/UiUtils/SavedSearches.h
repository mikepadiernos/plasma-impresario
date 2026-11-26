/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "UiUtils/SearchScope.h"

class KConfigGroup;

namespace UiUtils {

/** @short A search saved by the user, which the sidebar shows as a virtual folder

Saved searches are stored in the SavedSearches group of plasma-impresariorc: one subgroup per account, named after the
account's key (ImapAccess::credentialAccountId()), with one group per search, named by a UUID. The account a search
belongs to is its owner. The places decide where it looks: CurrentFolder and Account are relative to the owner, while
SelectedFolders and AllAccounts name their folders and accounts themselves.

A virtual folder only lists what its search finds; messages are never moved, copied or changed to fill it.
*/
struct SavedSearch {
    QString owner;
    QString id;
    QString name;
    /** The text of the quick search; empty when only the date range limits the search */
    QString query;
    bool subject = false;
    bool body = false;
    bool senders = false;
    bool recipients = false;
    bool fuzzy = false;
    bool raw = false;
    SearchScope::DateRange dates;
    SearchScope::Places places = SearchScope::Places::CurrentFolder;
    /** For SelectedFolders, keys made by SearchScope::folderKey() */
    QStringList folders;

    /** @short Whether the search has a name and something to search for: a query, a date range or both */
    bool isValid() const;
    /** @short Whether the search looks at its owner's folders, so that the owner has to be shown to run it */
    bool dependsOnOwner() const;
    bool operator==(const SavedSearch &other) const = default;
};

namespace SavedSearches {

/** @short The search stored in @arg group, whose name is the ID, for the account @arg owner

Searches saved before a date range or places could be chosen use any time and the shown folder.
*/
SavedSearch read(const KConfigGroup &group, const QString &owner);
/** @short Store @arg search in @arg group, which should be named after its ID */
void write(KConfigGroup &group, const SavedSearch &search);
/** @short The valid searches of @arg owners, read from the SavedSearches group @arg root

Searches of other keys, such as those of removed accounts, are left out. The result is sorted by name, ignoring
case, then by owner in the order of @arg owners.
*/
QList<SavedSearch> load(const KConfigGroup &root, const QStringList &owners);

/** @short How many messages a virtual folder showed when its search last ran */
struct Counts {
    int total = -1;
    int unread = -1;

    bool isKnown() const { return total >= 0 && unread >= 0; }
    bool operator==(const Counts &other) const = default;
};

/** @short The key of the counts of a search in the state config */
QString countsKey(const QString &owner, const QString &id);
Counts readCounts(const KConfigGroup &state, const QString &owner, const QString &id);
void writeCounts(KConfigGroup &state, const QString &owner, const QString &id, const Counts &counts);
/** @short Forget the counts of searches which are not in @arg searches any more */
void pruneCounts(KConfigGroup &state, const QList<SavedSearch> &searches);

}

}
