/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QStandardPaths>
#include <QtTest>

#include "test_ShortcutOverview.h"
#include "UiUtils/ShortcutOverview.h"

using namespace UiUtils::ShortcutOverview;

namespace {

Entry entry(const QString &id, const QString &area, const QString &text, const QStringList &shortcuts = {})
{
    Entry result{id, area, text, {}};
    for (const QString &shortcut : shortcuts)
        result.shortcuts << QKeySequence(shortcut);
    return result;
}

QStringList ids(const Group &group)
{
    QStringList result;
    for (const Entry &entry : group.entries)
        result << entry.id;
    return result;
}

QStringList areas(const QList<Group> &groups)
{
    QStringList result;
    for (const Group &group : groups)
        result << group.area;
    return result;
}

const QList<Entry> sample{
    entry(QStringLiteral("action_archive"), QStringLiteral("Message"), QStringLiteral("&Archive"), {QStringLiteral("A")}),
    entry(QStringLiteral("action_move_to_folder"), QStringLiteral("Message"), QStringLiteral("&Move to Folder…"),
          {QStringLiteral("V")}),
    entry(QStringLiteral("action_expunge"), QStringLiteral("Mailbox"), QStringLiteral("Exp&unge"), {QStringLiteral("Ctrl+E")}),
    entry(QStringLiteral("action_focus_next_region"), QStringLiteral("General"), QStringLiteral("Move Focus to Next Pane"),
          {QStringLiteral("F6")}),
    entry(QStringLiteral("action_shortcut_overview"), QStringLiteral("Help"), QStringLiteral("&Keyboard Shortcuts"),
          {QStringLiteral("?")}),
    entry(QStringLiteral("action_zoom_original"), QStringLiteral("Reader"), QStringLiteral("Original Size")),
};

}

void ShortcutOverviewTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void ShortcutOverviewTest::plainText_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("expected");

    QTest::newRow("mnemonic") << QStringLiteral("Mark as &Read") << QStringLiteral("Mark as Read");
    QTest::newRow("ellipsis") << QStringLiteral("&Move to Folder…") << QStringLiteral("Move to Folder");
    QTest::newRow("dots") << QStringLiteral("&New Message...") << QStringLiteral("New Message");
    QTest::newRow("literal ampersand") << QStringLiteral("R&&D") << QStringLiteral("R&D");
    QTest::newRow("trailing ampersand") << QStringLiteral("Odd&") << QStringLiteral("Odd");
    QTest::newRow("plain") << QStringLiteral("Zoom In") << QStringLiteral("Zoom In");
    QTest::newRow("empty") << QString() << QString();
}

void ShortcutOverviewTest::plainText()
{
    QFETCH(QString, text);
    QFETCH(QString, expected);
    QCOMPARE(UiUtils::ShortcutOverview::plainText(text), expected);
}

void ShortcutOverviewTest::shortcutText()
{
    QCOMPARE(UiUtils::ShortcutOverview::shortcutText(sample.at(0)), QKeySequence(QStringLiteral("A")).toString(QKeySequence::NativeText));
    QVERIFY(UiUtils::ShortcutOverview::shortcutText(sample.at(5)).isEmpty());
    const Entry two = entry(QStringLiteral("x"), QString(), QStringLiteral("X"), {QStringLiteral("F5"), QStringLiteral("Ctrl+R")});
    QCOMPARE(UiUtils::ShortcutOverview::shortcutText(two),
             QKeySequence(QStringLiteral("F5")).toString(QKeySequence::NativeText) + QLatin1String(", ")
                 + QKeySequence(QStringLiteral("Ctrl+R")).toString(QKeySequence::NativeText));
}

void ShortcutOverviewTest::groupsFollowTheAreaOrder()
{
    const QStringList order{QStringLiteral("Mailbox"), QStringLiteral("Message"), QStringLiteral("Help"),
                            QStringLiteral("Unused")};
    const QList<Group> groups = group(sample, order);
    // Listed areas first, then the others alphabetically; an area without entries is not shown
    QCOMPARE(areas(groups), (QStringList{QStringLiteral("Mailbox"), QStringLiteral("Message"), QStringLiteral("Help"),
                                         QStringLiteral("General"), QStringLiteral("Reader")}));
    QCOMPARE(ids(groups.at(1)), (QStringList{QStringLiteral("action_archive"), QStringLiteral("action_move_to_folder")}));

    // Entries are sorted by their text without mnemonics
    const QList<Entry> unsorted{
        entry(QStringLiteral("b"), QStringLiteral("Tags"), QStringLiteral("Tag with &3rd tag")),
        entry(QStringLiteral("a"), QStringLiteral("Tags"), QStringLiteral("Tag with &2nd tag")),
        entry(QStringLiteral("c"), QStringLiteral("Tags"), QStringLiteral("&Archive")),
    };
    const QList<Group> tags = group(unsorted);
    QCOMPARE(tags.size(), 1);
    QCOMPARE(ids(tags.constFirst()), (QStringList{QStringLiteral("c"), QStringLiteral("a"), QStringLiteral("b")}));
    QVERIFY(group({}).isEmpty());
}

void ShortcutOverviewTest::filterMatchesTextShortcutAndArea()
{
    const QList<Group> groups = group(sample);
    QCOMPARE(filter(groups, QString()).size(), groups.size());
    QCOMPARE(filter(groups, QStringLiteral("  ")).size(), groups.size());

    // Text, ignoring case and mnemonics
    QList<Group> found = filter(groups, QStringLiteral("ARCH"));
    QCOMPARE(areas(found), QStringList{QStringLiteral("Message")});
    QCOMPARE(ids(found.constFirst()), QStringList{QStringLiteral("action_archive")});
    found = filter(groups, QStringLiteral("expunge"));
    QCOMPARE(ids(found.constFirst()), QStringList{QStringLiteral("action_expunge")});

    // Shortcut
    found = filter(groups, QStringLiteral("ctrl+e"));
    QCOMPARE(found.size(), 1);
    QCOMPARE(ids(found.constFirst()), QStringList{QStringLiteral("action_expunge")});
    found = filter(groups, QStringLiteral("F6"));
    QCOMPARE(ids(found.constFirst()), QStringList{QStringLiteral("action_focus_next_region")});

    // Area keeps all of its entries
    found = filter(groups, QStringLiteral("message"));
    QCOMPARE(areas(found), QStringList{QStringLiteral("Message")});
    QCOMPARE(found.constFirst().entries.size(), 2);

    QVERIFY(filter(groups, QStringLiteral("nothing like this")).isEmpty());
}

void ShortcutOverviewTest::duplicateShortcuts()
{
    QVERIFY(UiUtils::ShortcutOverview::duplicateShortcuts(sample).isEmpty());
    QList<Entry> clashing = sample;
    clashing << entry(QStringLiteral("action_other"), QStringLiteral("General"), QStringLiteral("Other"),
                      {QStringLiteral("Ctrl+E"), QStringLiteral("V")});
    // The same action listing a shortcut twice is not a clash
    clashing << entry(QStringLiteral("action_twice"), QStringLiteral("General"), QStringLiteral("Twice"),
                      {QStringLiteral("F9"), QStringLiteral("F9")});
    QCOMPARE(UiUtils::ShortcutOverview::duplicateShortcuts(clashing), (QStringList{QStringLiteral("Ctrl+E"), QStringLiteral("V")}));
}

QTEST_GUILESS_MAIN(ShortcutOverviewTest)
