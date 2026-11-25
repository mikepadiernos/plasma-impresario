/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QSettings>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "test_RecentFolders.h"
#include "Imap/Model/ItemRoles.h"
#include "UiUtils/RecentFolders.h"

using UiUtils::RecentFolders;
using State = RecentFolders::State;

namespace {

const QString key = QStringLiteral("imap.recentDestinationFolders");

RecentFolders::StateLookup states(const QMap<QString, State> &known)
{
    return [known](const QString &mailbox) {
        return known.value(mailbox, State::Unknown);
    };
}

QStandardItem *mailbox(const QString &name, const bool selectable = true, const bool childrenKnown = true)
{
    using namespace Imap::Mailbox;
    auto *item = new QStandardItem(name);
    item->setData(name, RoleMailboxName);
    item->setData(QStringLiteral("/"), RoleMailboxSeparator);
    item->setData(selectable, RoleMailboxIsSelectable);
    item->setData(childrenKnown, RoleIsFetched);
    return item;
}

QStringList names(std::initializer_list<const char *> list)
{
    QStringList res;
    for (const char *name : list)
        res << QString::fromUtf8(name);
    return res;
}

}

void RecentFoldersTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void RecentFoldersTest::constructorNormalizes()
{
    const RecentFolders recent({QStringLiteral("a"), QString(), QStringLiteral("b"), QStringLiteral("a"),
                                QStringLiteral("c"), QStringLiteral("d"), QStringLiteral("e"), QStringLiteral("f")});
    QCOMPARE(recent.limit(), RecentFolders::DefaultLimit);
    QCOMPARE(recent.folders(), names({"a", "b", "c", "d", "e"}));
    QCOMPARE(RecentFolders(QStringList(), 0).limit(), 1);
}

void RecentFoldersTest::addMovesToFrontAndCaps()
{
    RecentFolders recent(QStringList(), 3);
    recent.add(QStringLiteral("a"));
    recent.add(QStringLiteral("b"));
    recent.add(QStringLiteral("c"));
    QCOMPARE(recent.folders(), names({"c", "b", "a"}));
    // Using a remembered folder again makes it the most recent one without repeating it
    recent.add(QStringLiteral("a"));
    QCOMPARE(recent.folders(), names({"a", "c", "b"}));
    // The oldest one falls off
    recent.add(QStringLiteral("d"));
    QCOMPARE(recent.folders(), names({"d", "a", "c"}));
}

void RecentFoldersTest::addIgnoresEmptyNames()
{
    RecentFolders recent({QStringLiteral("a")});
    recent.add(QString());
    QCOMPARE(recent.folders(), names({"a"}));
}

void RecentFoldersTest::pruneForgetsOnlyGoneFolders()
{
    RecentFolders recent(names({"gone", "here", "unlisted"}));
    const auto lookup = states({{QStringLiteral("gone"), State::Gone}, {QStringLiteral("here"), State::Available}});
    QVERIFY(recent.prune(lookup));
    QCOMPARE(recent.folders(), names({"here", "unlisted"}));
    QVERIFY(!recent.prune(lookup));
    QVERIFY(!recent.prune(RecentFolders::StateLookup()));
}

void RecentFoldersTest::offeredSkipsGoneAndExcluded()
{
    const RecentFolders recent(names({"current", "gone", "here", "unlisted"}));
    const auto lookup = states({{QStringLiteral("gone"), State::Gone},
                                {QStringLiteral("here"), State::Available},
                                {QStringLiteral("current"), State::Available}});
    QCOMPARE(recent.offered(lookup, {QStringLiteral("current")}), names({"here", "unlisted"}));
    // Offering never changes what is remembered
    QCOMPARE(recent.folders().size(), 4);
}

void RecentFoldersTest::saveAndLoadRoundTrip()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("account.ini")), QSettings::IniFormat);
    RecentFolders recent;
    // Names which need quoting in an INI list
    recent.add(QStringLiteral("Lists, Archive"));
    recent.add(QStringLiteral("INBOX.Single"));
    recent.add(QString::fromUtf8("Projekte/\xc3\x9c" "bersicht"));
    recent.save(settings, key);
    settings.sync();

    QSettings reread(directory.filePath(QStringLiteral("account.ini")), QSettings::IniFormat);
    QCOMPARE(RecentFolders::load(reread, key).folders(), recent.folders());

    // A single entry must not turn into a plain string that is split differently
    RecentFolders single;
    single.add(QStringLiteral("Lists, Archive"));
    single.save(settings, key);
    settings.sync();
    QSettings rereadSingle(directory.filePath(QStringLiteral("account.ini")), QSettings::IniFormat);
    QCOMPARE(RecentFolders::load(rereadSingle, key).folders(), names({"Lists, Archive"}));
}

void RecentFoldersTest::saveRemovesEmptyList()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("account.ini")), QSettings::IniFormat);
    RecentFolders({QStringLiteral("a")}).save(settings, key);
    QVERIFY(settings.contains(key));
    RecentFolders().save(settings, key);
    QVERIFY(!settings.contains(key));
    QVERIFY(RecentFolders::load(settings, key).folders().isEmpty());
}

void RecentFoldersTest::settingsAreIndependent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings first(directory.filePath(QStringLiteral("account-1.ini")), QSettings::IniFormat);
    QSettings second(directory.filePath(QStringLiteral("account-2.ini")), QSettings::IniFormat);
    RecentFolders one = RecentFolders::load(first, key);
    one.add(QStringLiteral("Work"));
    one.save(first, key);
    QVERIFY(RecentFolders::load(second, key).folders().isEmpty());
    RecentFolders two = RecentFolders::load(second, key);
    two.add(QStringLiteral("Family"));
    two.save(second, key);
    QCOMPARE(RecentFolders::load(first, key).folders(), names({"Work"}));
    QCOMPARE(RecentFolders::load(second, key).folders(), names({"Family"}));
}

void RecentFoldersTest::stateFromModel()
{
    QStandardItemModel model;
    model.appendRow(mailbox(QStringLiteral("INBOX")));
    QStandardItem *lists = mailbox(QStringLiteral("Lists"), false);
    lists->appendRow(mailbox(QStringLiteral("Lists/KDE")));
    model.appendRow(lists);
    // Children of a branch which has not been listed yet must not be judged
    QStandardItem *projects = mailbox(QStringLiteral("Projects"), true, false);
    projects->appendRow(mailbox(QStringLiteral("Projects/Stale")));
    model.appendRow(projects);

    const auto state = RecentFolders::stateFromModel(&model);
    QCOMPARE(state(QStringLiteral("INBOX")), State::Available);
    QCOMPARE(state(QStringLiteral("Lists/KDE")), State::Available);
    QCOMPARE(state(QStringLiteral("Projects")), State::Available);
    // Listed, but cannot hold messages
    QCOMPARE(state(QStringLiteral("Lists")), State::Gone);
    // Missing from completely listed levels
    QCOMPARE(state(QStringLiteral("Deleted")), State::Gone);
    QCOMPARE(state(QStringLiteral("Lists/Qt")), State::Gone);
    QCOMPARE(state(QStringLiteral("Lists/KDE/Plasma")), State::Gone);
    // Below a folder whose children are not known yet
    QCOMPARE(state(QStringLiteral("Projects/Stale")), State::Unknown);
    QCOMPARE(state(QStringLiteral("Projects/2026/Q1")), State::Unknown);

    // The lookup is a snapshot taken when it was made
    model.removeRow(0);
    QCOMPARE(state(QStringLiteral("INBOX")), State::Available);
    QCOMPARE(RecentFolders::stateFromModel(&model)(QStringLiteral("INBOX")), State::Gone);
}

void RecentFoldersTest::stateFromEmptyModelIsUnknown()
{
    QStandardItemModel model;
    const auto state = RecentFolders::stateFromModel(&model);
    QCOMPARE(state(QStringLiteral("Work")), State::Unknown);
    QCOMPARE(RecentFolders::stateFromModel(nullptr)(QStringLiteral("Work")), State::Unknown);

    RecentFolders recent({QStringLiteral("Work")});
    QVERIFY(!recent.prune(state));
    QCOMPARE(recent.offered(state), names({"Work"}));
}

QTEST_GUILESS_MAIN(RecentFoldersTest)
