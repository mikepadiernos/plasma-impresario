/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "test_ViewStateStore.h"
#include "UiUtils/ViewStateStore.h"

using UiUtils::FolderPosition;
using UiUtils::FolderPositionStore;
using UiUtils::MessageListView;
using UiUtils::RecentFolders;
using UiUtils::ViewStateStore;
using Scope = ViewStateStore::Scope;

namespace {

MessageListView view(int column, Qt::SortOrder order, bool threading = false, const QString &groupBy = QString())
{
    MessageListView res;
    res.sortColumn = column;
    res.sortOrder = order;
    res.threading = threading;
    res.groupBy = groupBy;
    return res;
}

const MessageListView global = view(5, Qt::DescendingOrder);

QString folder(int i)
{
    return QStringLiteral("F%1").arg(i);
}

}

void ViewStateStoreTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void ViewStateStoreTest::fallsBackFromFolderToAccountToGlobal()
{
    ViewStateStore store;
    store.setGlobalDefault(global);
    QCOMPARE(store.effectiveView(QStringLiteral("INBOX")), global);
    QCOMPARE(store.scope(QStringLiteral("INBOX")), Scope::Global);

    const MessageListView account = view(2, Qt::AscendingOrder, true);
    store.setAccountDefault(account);
    QCOMPARE(store.effectiveView(QStringLiteral("INBOX")), account);
    QCOMPARE(store.scope(QStringLiteral("INBOX")), Scope::Account);

    const MessageListView inbox = view(3, Qt::DescendingOrder, false, QStringLiteral("Date"));
    store.setFolderView(QStringLiteral("INBOX"), inbox);
    QCOMPARE(store.effectiveView(QStringLiteral("INBOX")), inbox);
    QCOMPARE(store.scope(QStringLiteral("INBOX")), Scope::Folder);
    QCOMPARE(store.effectiveView(QStringLiteral("Archive")), account);
    // Without a folder, the account default applies
    QCOMPARE(store.effectiveView(QString()), account);
    QVERIFY(!store.folderView(QString()));
}

void ViewStateStoreTest::legacyGroupingIsTheAccountDefault()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath(QStringLiteral("account.ini")), QSettings::IniFormat);
    settings.setValue(ViewStateStore::legacyGroupByKey, QStringLiteral("Sender"));

    ViewStateStore store;
    store.setGlobalDefault(view(5, Qt::DescendingOrder, true));
    store.load(settings);
    QCOMPARE(store.scope(QStringLiteral("INBOX")), Scope::Account);
    QVERIFY(!store.accountDefault());
    // Groups show a flat list sorted as the global default
    QCOMPARE(store.effectiveView(QStringLiteral("INBOX")), view(5, Qt::DescendingOrder, false, QStringLiteral("Sender")));
}

void ViewStateStoreTest::folderChangesStayInTheFolder()
{
    ViewStateStore store;
    store.setGlobalDefault(global);
    store.setFolderView(QStringLiteral("INBOX"), view(4, Qt::AscendingOrder));
    store.setFolderView(QStringLiteral("INBOX"), view(1, Qt::AscendingOrder, false, QStringLiteral("Date")));
    QCOMPARE(store.folders(), QStringList{QStringLiteral("INBOX")});
    QCOMPARE(store.effectiveView(QStringLiteral("INBOX")), view(1, Qt::AscendingOrder, false, QStringLiteral("Date")));
    QCOMPARE(store.effectiveView(QStringLiteral("Archive")), global);
    QVERIFY(!store.accountDefault());
}

void ViewStateStoreTest::keepsAtMostTheLimitDroppingLeastRecentlyUsed()
{
    ViewStateStore store;
    QCOMPARE(store.limit(), 200);
    for (int i = 0; i < 205; ++i)
        store.setFolderView(folder(i), view(i % 7, Qt::AscendingOrder));
    QCOMPARE(store.folders().size(), 200);
    QCOMPARE(store.folders().first(), folder(204));
    QCOMPARE(store.folders().last(), folder(5));
    QVERIFY(!store.folderView(folder(4)));

    // Using a folder again makes it the most recent one, so that it survives
    store.setFolderView(folder(5), view(1, Qt::DescendingOrder));
    store.setFolderView(folder(300), view(1, Qt::DescendingOrder));
    QVERIFY(store.folderView(folder(5)));
    QVERIFY(!store.folderView(folder(6)));
    QCOMPARE(store.folders().size(), 200);

    ViewStateStore small(2);
    small.setFolderView(QStringLiteral("a"), global);
    small.setFolderView(QStringLiteral("b"), global);
    small.setFolderView(QStringLiteral("c"), global);
    QCOMPARE(small.folders(), (QStringList{QStringLiteral("c"), QStringLiteral("b")}));
}

void ViewStateStoreTest::resetFolderFollowsTheAccountAgain()
{
    ViewStateStore store;
    store.setGlobalDefault(global);
    const MessageListView account = view(2, Qt::AscendingOrder);
    store.setAccountDefault(account);
    store.setFolderView(QStringLiteral("INBOX"), view(3, Qt::DescendingOrder));
    store.setFolderView(QStringLiteral("Archive"), view(4, Qt::DescendingOrder));
    QVERIFY(store.resetFolder(QStringLiteral("INBOX")));
    QVERIFY(!store.resetFolder(QStringLiteral("INBOX")));
    QCOMPARE(store.effectiveView(QStringLiteral("INBOX")), account);
    QCOMPARE(store.scope(QStringLiteral("INBOX")), Scope::Account);
    QCOMPARE(store.effectiveView(QStringLiteral("Archive")), view(4, Qt::DescendingOrder));
}

void ViewStateStoreTest::applyToAllFoldersClearsOverrides()
{
    ViewStateStore store;
    store.setGlobalDefault(global);
    store.setFolderView(QStringLiteral("INBOX"), view(3, Qt::DescendingOrder));
    store.setFolderView(QStringLiteral("Archive"), view(4, Qt::AscendingOrder, true));
    const MessageListView chosen = view(1, Qt::AscendingOrder, false, QStringLiteral("Date"));
    store.applyToAllFolders(chosen);
    QVERIFY(store.folders().isEmpty());
    QCOMPARE(store.accountDefault(), std::optional<MessageListView>(chosen));
    QCOMPARE(store.effectiveView(QStringLiteral("INBOX")), chosen);
    QCOMPARE(store.effectiveView(QStringLiteral("Archive")), chosen);
    QCOMPARE(store.effectiveView(QStringLiteral("Never seen")), chosen);
    QCOMPARE(store.globalDefault(), global);
}

void ViewStateStoreTest::renameMovesFolderAndChildren()
{
    ViewStateStore store;
    store.setFolderView(QStringLiteral("Work/Sub"), view(1, Qt::AscendingOrder));
    store.setFolderView(QStringLiteral("Workshop"), view(2, Qt::AscendingOrder));
    store.setFolderView(QStringLiteral("Work"), view(3, Qt::AscendingOrder));
    QVERIFY(store.renameFolder(QStringLiteral("Work"), QStringLiteral("Jobs"), QStringLiteral("/")));
    QCOMPARE(store.folders(), (QStringList{QStringLiteral("Jobs"), QStringLiteral("Workshop"), QStringLiteral("Jobs/Sub")}));
    QCOMPARE(store.folderView(QStringLiteral("Jobs/Sub")), std::optional<MessageListView>(view(1, Qt::AscendingOrder)));
    QVERIFY(!store.renameFolder(QStringLiteral("Missing"), QStringLiteral("Other"), QStringLiteral("/")));

    FolderPositionStore positions;
    positions.setPosition(QStringLiteral("Jobs/Sub"), FolderPosition{7, 3, 2});
    QVERIFY(positions.renameFolder(QStringLiteral("Jobs"), QStringLiteral("Work"), QStringLiteral("/")));
    QCOMPARE(positions.position(QStringLiteral("Work/Sub")), std::optional<FolderPosition>(FolderPosition{7, 3, 2}));
}

void ViewStateStoreTest::pruneForgetsGoneFolders()
{
    ViewStateStore store;
    store.setFolderView(QStringLiteral("Gone"), global);
    store.setFolderView(QStringLiteral("Unknown"), global);
    store.setFolderView(QStringLiteral("INBOX"), global);
    const RecentFolders::StateLookup state = [](const QString &mailbox) {
        if (mailbox == QLatin1String("Gone"))
            return RecentFolders::State::Gone;
        if (mailbox == QLatin1String("INBOX"))
            return RecentFolders::State::Available;
        return RecentFolders::State::Unknown;
    };
    QVERIFY(store.prune(state));
    QVERIFY(!store.prune(state));
    QCOMPARE(store.folders(), (QStringList{QStringLiteral("INBOX"), QStringLiteral("Unknown")}));

    FolderPositionStore positions;
    positions.setPosition(QStringLiteral("Gone"), FolderPosition{1, 2, 0});
    positions.setPosition(QStringLiteral("INBOX"), FolderPosition{1, 2, 0});
    QVERIFY(positions.prune(state));
    QCOMPARE(positions.folders(), QStringList{QStringLiteral("INBOX")});
}

void ViewStateStoreTest::saveAndLoadRoundTrip()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("account.ini"));
    {
        QSettings settings(path, QSettings::IniFormat);
        ViewStateStore store;
        store.setAccountDefault(view(2, Qt::DescendingOrder, true));
        store.setFolderView(QStringLiteral("Archive"), view(-1, Qt::AscendingOrder, true));
        store.setFolderView(QStringLiteral("INBOX"), view(4, Qt::DescendingOrder, false, QStringLiteral("Sender")));
        store.save(settings);
    }
    QSettings settings(path, QSettings::IniFormat);
    ViewStateStore store;
    store.setGlobalDefault(global);
    store.load(settings);
    QCOMPARE(store.folders(), (QStringList{QStringLiteral("INBOX"), QStringLiteral("Archive")}));
    QCOMPARE(store.effectiveView(QStringLiteral("INBOX")), view(4, Qt::DescendingOrder, false, QStringLiteral("Sender")));
    QCOMPARE(store.effectiveView(QStringLiteral("Archive")), view(-1, Qt::AscendingOrder, true));
    QCOMPARE(store.effectiveView(QStringLiteral("Other")), view(2, Qt::DescendingOrder, true));
    // The account default has no groups, so the older key is gone
    QVERIFY(!settings.contains(ViewStateStore::legacyGroupByKey));

    // Nothing left to remember removes the keys
    store.resetFolder(QStringLiteral("INBOX"));
    store.resetFolder(QStringLiteral("Archive"));
    store.save(settings);
    QVERIFY(!settings.contains(ViewStateStore::folderViewsKey));
}

void ViewStateStoreTest::saveKeepsLegacyGrouping()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath(QStringLiteral("account.ini")), QSettings::IniFormat);
    settings.setValue(ViewStateStore::legacyGroupByKey, QStringLiteral("Date"));
    ViewStateStore store;
    store.load(settings);
    store.setFolderView(QStringLiteral("INBOX"), global);
    store.save(settings);
    QCOMPARE(settings.value(ViewStateStore::legacyGroupByKey).toString(), QStringLiteral("Date"));
    QVERIFY(!settings.contains(ViewStateStore::defaultViewKey));

    store.applyToAllFolders(view(1, Qt::AscendingOrder, false, QStringLiteral("Sender")));
    store.save(settings);
    QCOMPARE(settings.value(ViewStateStore::legacyGroupByKey).toString(), QStringLiteral("Sender"));
    QVERIFY(settings.contains(ViewStateStore::defaultViewKey));
    QVERIFY(!settings.contains(ViewStateStore::folderViewsKey));

    store.applyToAllFolders(view(1, Qt::AscendingOrder));
    store.save(settings);
    QVERIFY(!settings.contains(ViewStateStore::legacyGroupByKey));
}

void ViewStateStoreTest::loadIgnoresMalformedEntries()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath(QStringLiteral("account.ini")), QSettings::IniFormat);
    settings.setValue(ViewStateStore::defaultViewKey, QStringLiteral("not json"));
    settings.setValue(ViewStateStore::folderViewsKey,
                      QStringLiteral(R"([{"folder":"INBOX","sort":3},{"folder":"","sort":1},{"sort":2},)"
                                     R"({"folder":"Bad","sort":"x"},{"folder":"INBOX","sort":4},7])"));
    ViewStateStore store;
    store.setGlobalDefault(global);
    store.load(settings);
    QVERIFY(!store.accountDefault());
    QCOMPARE(store.folders(), QStringList{QStringLiteral("INBOX")});
    QCOMPARE(store.effectiveView(QStringLiteral("INBOX")), view(3, Qt::AscendingOrder));
    QVERIFY(!ViewStateStore::fromJson(QString()));
    QCOMPARE(ViewStateStore::fromJson(ViewStateStore::toJson(global)), std::optional<MessageListView>(global));
}

void ViewStateStoreTest::positionsRoundTripAndBound()
{
    FolderPositionStore positions(3);
    positions.setPosition(QStringLiteral("a"), FolderPosition{10, 1, 0});
    positions.setPosition(QStringLiteral("b"), FolderPosition{10, 0, 5});
    positions.setPosition(QStringLiteral("c"), FolderPosition{4000000000u, 4000000001u, 3});
    positions.setPosition(QStringLiteral("d"), FolderPosition{10, 2, 2});
    QCOMPARE(positions.folders(), (QStringList{QStringLiteral("d"), QStringLiteral("c"), QStringLiteral("b")}));

    FolderPositionStore reread(3);
    reread.loadJson(positions.toJson());
    QCOMPARE(reread.folders(), positions.folders());
    QCOMPARE(reread.position(QStringLiteral("c")), std::optional<FolderPosition>(FolderPosition{4000000000u, 4000000001u, 3}));
    QCOMPARE(reread.position(QStringLiteral("b")), std::optional<FolderPosition>(FolderPosition{10, 0, 5}));

    // The selection besides the current message comes back, unless it was too large to be worth it
    positions.setPosition(QStringLiteral("e"), FolderPosition{10, 7, 3, {5, 6}});
    QList<uint> many;
    for (uint uid = 1; uid <= FolderPosition::MaxSelectedUids + 1; ++uid)
        many << uid;
    positions.setPosition(QStringLiteral("f"), FolderPosition{10, 7, 3, many});
    reread.loadJson(positions.toJson());
    QCOMPARE(reread.position(QStringLiteral("e")), std::optional<FolderPosition>(FolderPosition{10, 7, 3, {5, 6}}));
    QCOMPARE(reread.position(QStringLiteral("f")), std::optional<FolderPosition>(FolderPosition{10, 7, 3}));

    FolderPositionStore defaults;
    for (int i = 0; i < 250; ++i)
        defaults.setPosition(folder(i), FolderPosition{1, uint(i + 1), 0});
    QCOMPARE(defaults.folders().size(), FolderPositionStore::DefaultLimit);
    reread.loadJson(QStringLiteral("garbage"));
    QVERIFY(reread.folders().isEmpty());
}

void ViewStateStoreTest::invalidPositionForgetsFolder()
{
    FolderPositionStore positions;
    positions.setPosition(QStringLiteral("INBOX"), FolderPosition{10, 1, 0});
    positions.setPosition(QStringLiteral("INBOX"), FolderPosition{10, 0, 0});
    QVERIFY(!positions.position(QStringLiteral("INBOX")));
    positions.setPosition(QStringLiteral("INBOX"), FolderPosition{0, 1, 1});
    QVERIFY(!positions.position(QStringLiteral("INBOX")));
    QVERIFY(!positions.remove(QStringLiteral("INBOX")));
}

QTEST_GUILESS_MAIN(ViewStateStoreTest)
