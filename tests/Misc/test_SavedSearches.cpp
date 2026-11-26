/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>

#include <KConfig>
#include <KConfigGroup>

#include "test_SavedSearches.h"
#include "UiUtils/SavedSearches.h"

using UiUtils::SavedSearch;
using UiUtils::SearchScope::Dates;
using UiUtils::SearchScope::Places;
namespace SavedSearches = UiUtils::SavedSearches;

namespace {

SavedSearch named(const QString &owner, const QString &id, const QString &name, const QString &query = QStringLiteral("x"))
{
    SavedSearch search;
    search.owner = owner;
    search.id = id;
    search.name = name;
    search.query = query;
    return search;
}

}

void SavedSearchesTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
}

void SavedSearchesTest::roundTrip()
{
    KConfig config(m_dir.filePath(QStringLiteral("roundtrip")), KConfig::SimpleConfig);
    KConfigGroup root = config.group(QStringLiteral("SavedSearches"));

    SavedSearch search = named(QStringLiteral("account-1"), QStringLiteral("a"), QStringLiteral("Invoices"),
                               QStringLiteral("invoice"));
    search.subject = true;
    search.senders = true;
    search.fuzzy = true;
    search.dates = {Dates::Custom, QDate(2026, 3, 5), QDate(2026, 4, 1)};
    search.places = Places::SelectedFolders;
    search.folders = {QStringLiteral("account-1/INBOX"), QStringLiteral("account-2/Archive")};
    KConfigGroup group = root.group(search.owner).group(search.id);
    SavedSearches::write(group, search);
    QCOMPARE(SavedSearches::read(root.group(search.owner).group(search.id), search.owner), search);

    // A preset stays relative, and folders only matter for chosen folders
    SavedSearch week = named(QStringLiteral("account-1"), QStringLiteral("b"), QStringLiteral("Recent"));
    week.dates = {Dates::PastWeek, QDate(2026, 3, 5), QDate()};
    week.places = Places::Account;
    week.folders = {QStringLiteral("account-1/INBOX")};
    KConfigGroup weekGroup = root.group(week.owner).group(week.id);
    SavedSearches::write(weekGroup, week);
    const SavedSearch read = SavedSearches::read(root.group(week.owner).group(week.id), week.owner);
    QCOMPARE(read.dates.dates, Dates::PastWeek);
    QVERIFY(!read.dates.from.isValid());
    QCOMPARE(read.places, Places::Account);
    QVERIFY(read.folders.isEmpty());
}

void SavedSearchesTest::readsSearchesFromBeforeScopes()
{
    KConfig config(m_dir.filePath(QStringLiteral("old")), KConfig::SimpleConfig);
    KConfigGroup group = config.group(QStringLiteral("SavedSearches")).group(QStringLiteral("account-0")).group(QStringLiteral("id"));
    group.writeEntry("Name", QStringLiteral("Old"));
    group.writeEntry("Query", QStringLiteral("report"));
    group.writeEntry("Body", true);
    const SavedSearch search = SavedSearches::read(group, QStringLiteral("account-0"));
    QVERIFY(search.isValid());
    QCOMPARE(search.id, QStringLiteral("id"));
    QCOMPARE(search.owner, QStringLiteral("account-0"));
    QVERIFY(search.body);
    QVERIFY(!search.subject);
    QCOMPARE(search.dates.dates, Dates::AnyTime);
    QCOMPARE(search.places, Places::CurrentFolder);
}

void SavedSearchesTest::validity()
{
    SavedSearch search = named(QStringLiteral("o"), QStringLiteral("i"), QStringLiteral("Name"));
    QVERIFY(search.isValid());
    search.name = QStringLiteral("  ");
    QVERIFY(!search.isValid());
    search.name = QStringLiteral("Name");
    search.query.clear();
    QVERIFY(!search.isValid());
    // A date range alone is something to search for
    search.dates.dates = Dates::Today;
    QVERIFY(search.isValid());
}

void SavedSearchesTest::dependsOnOwner()
{
    SavedSearch search = named(QStringLiteral("o"), QStringLiteral("i"), QStringLiteral("Name"));
    search.places = Places::CurrentFolder;
    QVERIFY(search.dependsOnOwner());
    search.places = Places::Account;
    QVERIFY(search.dependsOnOwner());
    search.places = Places::SelectedFolders;
    QVERIFY(!search.dependsOnOwner());
    search.places = Places::AllAccounts;
    QVERIFY(!search.dependsOnOwner());
}

void SavedSearchesTest::loadSortsAcrossOwners()
{
    KConfig config(m_dir.filePath(QStringLiteral("load")), KConfig::SimpleConfig);
    KConfigGroup root = config.group(QStringLiteral("SavedSearches"));
    const QList<SavedSearch> stored = {
        named(QStringLiteral("account-2"), QStringLiteral("1"), QStringLiteral("travel")),
        named(QStringLiteral("account-1"), QStringLiteral("2"), QStringLiteral("Invoices")),
        named(QStringLiteral("account-2"), QStringLiteral("3"), QStringLiteral("Invoices")),
        named(QStringLiteral("account-1"), QStringLiteral("4"), QStringLiteral("Broken"), QString()),
        named(QStringLiteral("removed"), QStringLiteral("5"), QStringLiteral("Gone")),
        named(QStringLiteral("account-1"), QStringLiteral("6"), QStringLiteral("alerts")),
    };
    for (const SavedSearch &search : stored) {
        KConfigGroup group = root.group(search.owner).group(search.id);
        SavedSearches::write(group, search);
    }

    const QList<SavedSearch> loaded = SavedSearches::load(root, {QStringLiteral("account-1"), QStringLiteral("account-2")});
    QStringList order;
    for (const SavedSearch &search : loaded)
        order << search.owner + QLatin1Char(':') + search.name;
    // Invalid searches and those of other accounts are left out; equal names keep the order of the accounts
    QCOMPARE(order, (QStringList{QStringLiteral("account-1:alerts"), QStringLiteral("account-1:Invoices"),
                                 QStringLiteral("account-2:Invoices"), QStringLiteral("account-2:travel")}));
    QVERIFY(SavedSearches::load(root, {}).isEmpty());
}

void SavedSearchesTest::counts()
{
    KConfig config(m_dir.filePath(QStringLiteral("counts")), KConfig::SimpleConfig);
    KConfigGroup state = config.group(QStringLiteral("SavedSearchCounts"));
    QVERIFY(!SavedSearches::readCounts(state, QStringLiteral("o"), QStringLiteral("i")).isKnown());

    SavedSearches::writeCounts(state, QStringLiteral("o"), QStringLiteral("i"), {5, 2});
    QCOMPARE(SavedSearches::readCounts(state, QStringLiteral("o"), QStringLiteral("i")), (SavedSearches::Counts{5, 2}));
    QCOMPARE(state.keyList(), QStringList{SavedSearches::countsKey(QStringLiteral("o"), QStringLiteral("i"))});

    // Unknown counts forget the stored ones, and impossible ones are not trusted
    SavedSearches::writeCounts(state, QStringLiteral("o"), QStringLiteral("i"), {});
    QVERIFY(state.keyList().isEmpty());
    state.writeEntry(SavedSearches::countsKey(QStringLiteral("o"), QStringLiteral("i")), QList<int>{2, 5});
    QVERIFY(!SavedSearches::readCounts(state, QStringLiteral("o"), QStringLiteral("i")).isKnown());
    state.writeEntry(SavedSearches::countsKey(QStringLiteral("o"), QStringLiteral("i")), QList<int>{3});
    QVERIFY(!SavedSearches::readCounts(state, QStringLiteral("o"), QStringLiteral("i")).isKnown());
}

void SavedSearchesTest::pruneCounts()
{
    KConfig config(m_dir.filePath(QStringLiteral("prune")), KConfig::SimpleConfig);
    KConfigGroup state = config.group(QStringLiteral("SavedSearchCounts"));
    SavedSearches::writeCounts(state, QStringLiteral("account-1"), QStringLiteral("kept"), {1, 0});
    SavedSearches::writeCounts(state, QStringLiteral("account-1"), QStringLiteral("deleted"), {4, 4});
    SavedSearches::writeCounts(state, QStringLiteral("removed"), QStringLiteral("kept"), {2, 1});
    SavedSearches::pruneCounts(state, {named(QStringLiteral("account-1"), QStringLiteral("kept"), QStringLiteral("Kept"))});
    QCOMPARE(state.keyList(), QStringList{SavedSearches::countsKey(QStringLiteral("account-1"), QStringLiteral("kept"))});
}

QTEST_GUILESS_MAIN(SavedSearchesTest)
