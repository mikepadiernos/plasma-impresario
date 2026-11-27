/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "SavedSearches.h"

#include <KConfigGroup>
#include <QSet>

#include <algorithm>

namespace UiUtils {

using SearchScope::Dates;
using SearchScope::Places;

bool SavedSearch::isValid() const
{
    return !name.trimmed().isEmpty() && (!query.isEmpty() || SearchScope::isLimited(dates));
}

bool SavedSearch::dependsOnOwner() const
{
    return places == Places::CurrentFolder || places == Places::Account;
}

namespace SavedSearches {

SavedSearch read(const KConfigGroup &group, const QString &owner)
{
    SavedSearch search;
    search.owner = owner;
    search.id = group.name();
    search.name = group.readEntry("Name", QString());
    search.query = group.readEntry("Query", QString());
    search.subject = group.readEntry("Subject", false);
    search.body = group.readEntry("Body", false);
    search.senders = group.readEntry("Senders", false);
    search.recipients = group.readEntry("Recipients", false);
    search.fuzzy = group.readEntry("Fuzzy", false);
    search.raw = group.readEntry("Raw", false);
    search.dates.dates = SearchScope::datesFromConfigName(group.readEntry("Dates", QString()));
    if (search.dates.dates == Dates::Custom) {
        search.dates.from = QDate::fromString(group.readEntry("DateFrom", QString()), Qt::ISODate);
        search.dates.to = QDate::fromString(group.readEntry("DateTo", QString()), Qt::ISODate);
    }
    search.places = SearchScope::placesFromConfigName(group.readEntry("Places", QString()));
    if (search.places == Places::SelectedFolders)
        search.folders = group.readEntry("Folders", QStringList());
    return search;
}

void write(KConfigGroup &group, const SavedSearch &search)
{
    group.writeEntry("Name", search.name);
    group.writeEntry("Query", search.query);
    group.writeEntry("Subject", search.subject);
    group.writeEntry("Body", search.body);
    group.writeEntry("Senders", search.senders);
    group.writeEntry("Recipients", search.recipients);
    group.writeEntry("Fuzzy", search.fuzzy);
    group.writeEntry("Raw", search.raw);
    // Presets stay relative, so a search for the past week keeps finding the latest week
    group.writeEntry("Dates", SearchScope::configName(search.dates.dates));
    const bool custom = search.dates.dates == Dates::Custom;
    group.writeEntry("DateFrom", custom ? search.dates.from.toString(Qt::ISODate) : QString());
    group.writeEntry("DateTo", custom ? search.dates.to.toString(Qt::ISODate) : QString());
    group.writeEntry("Places", SearchScope::configName(search.places));
    group.writeEntry("Folders", search.places == Places::SelectedFolders ? search.folders : QStringList());
}

QList<SavedSearch> load(const KConfigGroup &root, const QStringList &owners, Orphans orphans)
{
    QStringList keys = owners;
    if (orphans == Orphans::Include) {
        QStringList removed;
        for (const QString &owner : root.groupList()) {
            if (!owners.contains(owner))
                removed << owner;
        }
        removed.sort();
        keys << removed;
    }
    QList<SavedSearch> searches;
    for (const QString &owner : std::as_const(keys)) {
        const KConfigGroup account = root.group(owner);
        for (const QString &id : account.groupList()) {
            SavedSearch search = read(account.group(id), owner);
            if (search.isValid())
                searches.append(search);
        }
    }
    // Equal names keep the order of the owners
    std::stable_sort(searches.begin(), searches.end(), [](const SavedSearch &a, const SavedSearch &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    const QStringList order = readOrder(root);
    const auto position = [&order](const SavedSearch &search) {
        const qsizetype index = order.indexOf(countsKey(search.owner, search.id));
        return index < 0 ? order.size() : index;
    };
    std::stable_sort(searches.begin(), searches.end(),
                     [&position](const SavedSearch &a, const SavedSearch &b) { return position(a) < position(b); });
    return searches;
}

QStringList readOrder(const KConfigGroup &root)
{
    return root.readEntry("Order", QStringList());
}

void writeOrder(KConfigGroup &root, const QList<SavedSearch> &searches)
{
    QStringList order;
    for (const SavedSearch &search : searches)
        order << countsKey(search.owner, search.id);
    root.writeEntry("Order", order);
}

bool isNameTaken(const KConfigGroup &root, const QString &owner, const QString &name, const QString &exceptId)
{
    const KConfigGroup account = root.group(owner);
    const QStringList ids = account.groupList();
    return std::any_of(ids.cbegin(), ids.cend(), [&](const QString &id) {
        return id != exceptId && account.group(id).readEntry("Name", QString()).compare(name, Qt::CaseInsensitive) == 0;
    });
}

bool changeOwner(KConfigGroup &root, const QString &from, const QString &id, const QString &to)
{
    if (from == to || !root.group(from).hasGroup(id) || root.group(to).hasGroup(id))
        return false;
    KConfigGroup source = root.group(from).group(id);
    KConfigGroup target = root.group(to).group(id);
    source.copyTo(&target);
    source.deleteGroup();
    QStringList order = readOrder(root);
    const qsizetype index = order.indexOf(countsKey(from, id));
    if (index >= 0) {
        order[index] = countsKey(to, id);
        root.writeEntry("Order", order);
    }
    return true;
}

Problems problems(const SavedSearch &search, const QStringList &owners, const QStringList &accountIds,
                  const std::function<FolderPresence(const SearchScope::Folder &)> &presence)
{
    Problems found;
    found.missingOwner = !owners.contains(search.owner);
    if (search.places != Places::SelectedFolders)
        return found;
    for (const QString &key : search.folders) {
        const SearchScope::Folder folder = SearchScope::folderFromKey(key);
        if (folder.accountId.isEmpty() || !accountIds.contains(folder.accountId)
            || (presence && presence(folder) == FolderPresence::Missing))
            found.missingFolders << key;
    }
    return found;
}

QString countsKey(const QString &owner, const QString &id)
{
    return owner + QLatin1Char('/') + id;
}

Counts readCounts(const KConfigGroup &state, const QString &owner, const QString &id)
{
    const QList<int> values = state.readEntry(countsKey(owner, id), QList<int>());
    if (values.size() != 2 || values.at(0) < 0 || values.at(1) < 0 || values.at(1) > values.at(0))
        return {};
    return {values.at(0), values.at(1)};
}

void writeCounts(KConfigGroup &state, const QString &owner, const QString &id, const Counts &counts)
{
    if (counts.isKnown())
        state.writeEntry(countsKey(owner, id), QList<int>{counts.total, counts.unread});
    else
        state.deleteEntry(countsKey(owner, id));
}

void pruneCounts(KConfigGroup &state, const QList<SavedSearch> &searches)
{
    QSet<QString> known;
    for (const SavedSearch &search : searches)
        known.insert(countsKey(search.owner, search.id));
    for (const QString &key : state.keyList()) {
        if (!known.contains(key))
            state.deleteEntry(key);
    }
}

}

}
