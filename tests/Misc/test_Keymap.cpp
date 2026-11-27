/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "UiUtils/Keymap.h"

using namespace UiUtils::Keymap;
using Result = Matcher::Result;

class KeymapTest : public QObject
{
    Q_OBJECT

private slots:
    void commandsAreComplete()
    {
        for (const Command &cmd : commands()) {
            QVERIFY2(isValid(cmd.defaultKeys), qPrintable(cmd.id));
            QVERIFY(cmd.contexts);
            QVERIFY2(description(cmd.id) != cmd.id, qPrintable(cmd.id));
            QCOMPARE(command(cmd.id), &cmd);
            // Mail-only commands are carried out by the window's actions, except going to the inbox
            if (!cmd.shared && cmd.id != QLatin1String("go.inbox"))
                QVERIFY2(cmd.action.startsWith(QLatin1String("action_")), qPrintable(cmd.id));
        }
        QCOMPARE(command(QStringLiteral("nope")), nullptr);
    }

    void defaultsDoNotConflict()
    {
        QCOMPARE(conflicts(defaultBindings()), QList<Conflict>());
    }

    void tokens_data()
    {
        QTest::addColumn<QString>("keys");
        QTest::addColumn<QStringList>("expected");
        QTest::newRow("single") << QStringLiteral("j") << QStringList{QStringLiteral("j")};
        QTest::newRow("double") << QStringLiteral("gg") << QStringList{QStringLiteral("g"), QStringLiteral("g")};
        QTest::newRow("ctrl") << QStringLiteral("<C-d>") << QStringList{QStringLiteral("<C-d>")};
        QTest::newRow("mixed") << QStringLiteral("g<C-u>") << QStringList{QStringLiteral("g"), QStringLiteral("<C-u>")};
        QTest::newRow("empty") << QString() << QStringList();
        QTest::newRow("space") << QStringLiteral("g g") << QStringList();
        QTest::newRow("unclosed") << QStringLiteral("<C-d") << QStringList();
        QTest::newRow("unknown") << QStringLiteral("<Tab>") << QStringList();
        QTest::newRow("upper ctrl") << QStringLiteral("<C-D>") << QStringList();
    }

    void tokens()
    {
        QFETCH(QString, keys);
        QFETCH(QStringList, expected);
        QCOMPARE(UiUtils::Keymap::tokens(keys), expected);
    }

    void tokenOfKeyPress()
    {
        QCOMPARE(token(Qt::Key_J, Qt::NoModifier, QStringLiteral("j")), QStringLiteral("j"));
        QCOMPARE(token(Qt::Key_G, Qt::ShiftModifier, QStringLiteral("G")), QStringLiteral("G"));
        QCOMPARE(token(Qt::Key_D, Qt::ControlModifier, QStringLiteral("\x04")), QStringLiteral("<C-d>"));
        QCOMPARE(token(Qt::Key_Escape, Qt::NoModifier, QString()), QStringLiteral("<Esc>"));
        // Keys which move the focus or open things keep their usual meaning
        QCOMPARE(token(Qt::Key_Tab, Qt::NoModifier, QStringLiteral("\t")), QString());
        QCOMPARE(token(Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r")), QString());
        QCOMPARE(token(Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ")), QString());
        QCOMPARE(token(Qt::Key_J, Qt::AltModifier, QStringLiteral("j")), QString());
        QCOMPARE(token(Qt::Key_1, Qt::ControlModifier, QStringLiteral("1")), QString());
    }

    void matcherFollowsSequences()
    {
        Matcher matcher;
        QCOMPARE(matcher.feed(QStringLiteral("j"), Context::MessageList), Result::Matched);
        QCOMPARE(matcher.matched(), QStringLiteral("move.down"));
        QCOMPARE(matcher.feed(QStringLiteral("g"), Context::MessageList), Result::Pending);
        QCOMPARE(matcher.pending(), QStringLiteral("g"));
        QCOMPARE(matcher.matched(), QString());
        QCOMPARE(matcher.feed(QStringLiteral("g"), Context::MessageList), Result::Matched);
        QCOMPARE(matcher.matched(), QStringLiteral("move.top"));
        QCOMPARE(matcher.pending(), QString());
        // An unknown continuation drops the sequence, and the key goes to the window
        QCOMPARE(matcher.feed(QStringLiteral("d"), Context::MessageList), Result::Pending);
        QCOMPARE(matcher.feed(QStringLiteral("z"), Context::MessageList), Result::None);
        QCOMPARE(matcher.pending(), QString());
        QCOMPARE(matcher.feed(QStringLiteral("Q"), Context::MessageList), Result::None);
    }

    void matcherRespectsContexts()
    {
        Matcher matcher;
        QCOMPARE(matcher.feed(QStringLiteral("r"), Context::FolderTree), Result::None);
        QCOMPARE(matcher.feed(QStringLiteral("r"), Context::Reader), Result::Matched);
        QCOMPARE(matcher.matched(), QStringLiteral("message.reply"));
        QCOMPARE(matcher.feed(QStringLiteral("x"), Context::Reader), Result::None);
        QCOMPARE(matcher.feed(QStringLiteral("x"), Context::MessageList), Result::Matched);
    }

    void escapeCancels()
    {
        Matcher matcher;
        QCOMPARE(matcher.feed(QStringLiteral("<Esc>"), Context::MessageList), Result::None);
        QCOMPARE(matcher.feed(QStringLiteral("g"), Context::MessageList), Result::Pending);
        QCOMPARE(matcher.feed(QStringLiteral("<Esc>"), Context::MessageList), Result::Cancelled);
        QCOMPARE(matcher.pending(), QString());
    }

    void continuations()
    {
        Matcher matcher;
        QVERIFY(matcher.continuations(Context::MessageList).size() > 10);
        matcher.feed(QStringLiteral("g"), Context::MessageList);
        const QList<Matcher::Continuation> next = matcher.continuations(Context::MessageList);
        QStringList keys;
        for (const auto &continuation : next)
            keys << continuation.keys;
        QCOMPARE(keys, (QStringList{QStringLiteral("T"), QStringLiteral("g"), QStringLiteral("i"), QStringLiteral("t")}));
        QCOMPARE(next.at(1).command, QStringLiteral("move.top"));
    }

    void overridesAndMerging()
    {
        Bindings changed;
        changed.insert(QStringLiteral("move.down"), QStringLiteral("J"));
        changed.insert(QStringLiteral("message.archive"), QString());
        changed.insert(QStringLiteral("unknown.command"), QStringLiteral("q"));
        changed.insert(QStringLiteral("move.up"), QStringLiteral("<Bad>"));
        const Bindings all = merged(changed);
        QCOMPARE(all.value(QStringLiteral("move.down")), QStringLiteral("J"));
        QCOMPARE(all.value(QStringLiteral("message.archive")), QString());
        QCOMPARE(all.value(QStringLiteral("move.up")), QStringLiteral("k"));
        QVERIFY(!all.contains(QStringLiteral("unknown.command")));
        Bindings expected;
        expected.insert(QStringLiteral("move.down"), QStringLiteral("J"));
        expected.insert(QStringLiteral("message.archive"), QString());
        QCOMPARE(overrides(all), expected);
        // A turned-off command is never matched
        Matcher matcher(all);
        QCOMPARE(matcher.feed(QStringLiteral("a"), Context::MessageList), Result::None);
    }

    void conflictsAreFound()
    {
        Bindings bindings = defaultBindings();
        bindings.insert(QStringLiteral("message.archive"), QStringLiteral("r"));
        bindings.insert(QStringLiteral("search"), QStringLiteral("g"));
        const QList<Conflict> found = conflicts(bindings);
        QVERIFY(found.contains(Conflict{Conflict::Kind::Same, QStringLiteral("r"),
                                        {QStringLiteral("message.archive"), QStringLiteral("message.reply")}}));
        bool prefix = false;
        for (const Conflict &conflict : found) {
            if (conflict.kind == Conflict::Kind::Prefix && conflict.keys == QLatin1String("gg"))
                prefix = conflict.commands.contains(QStringLiteral("search"));
        }
        QVERIFY(prefix);
        // Commands of different parts of the window may share keys
        bindings = defaultBindings();
        bindings.insert(QStringLiteral("select.toggle"), QStringLiteral("z"));
        bindings.insert(QStringLiteral("message.toggleFlag"), QStringLiteral("z"));
        QVERIFY(!conflicts(bindings).isEmpty());
    }

    void shadowedSingleKeyShortcuts()
    {
        const QList<QKeySequence> shortcuts = {QKeySequence(QStringLiteral("J")), QKeySequence(QStringLiteral("Ctrl+K")),
                                               QKeySequence(QStringLiteral("Shift+G")), QKeySequence(QStringLiteral("A, B"))};
        QStringList shadowed = shadowedShortcuts(defaultBindings(), shortcuts);
        shadowed.sort();
        QCOMPARE(shadowed, (QStringList{QStringLiteral("move.bottom"), QStringLiteral("move.down")}));
    }

    void profileRoundTrip()
    {
        Profile profile;
        profile.vimMode = true;
        profile.hintDelay = 250;
        profile.application = QStringLiteral("plasma-impresario");
        profile.bindings = defaultBindings();
        profile.bindings.insert(QStringLiteral("move.down"), QStringLiteral("J"));
        const QByteArray json = toJson(profile);
        const QJsonObject root = QJsonDocument::fromJson(json).object();
        QCOMPARE(root.value(QStringLiteral("format")).toString(), QStringLiteral("plasma-keymap"));
        QCOMPARE(root.value(QStringLiteral("version")).toInt(), ProfileVersion);
        // Mail-only commands stay in this application
        QVERIFY(!root.value(QStringLiteral("bindings")).toObject().contains(QStringLiteral("message.reply")));
        const ParseResult parsed = fromJson(json);
        QVERIFY(parsed.profile);
        QCOMPARE(parsed.ignored, QStringList());
        Profile expected = profile;
        expected.bindings = sharedOnly(profile.bindings);
        QCOMPARE(*parsed.profile, expected);
    }

    void invalidProfiles()
    {
        QVERIFY(!fromJson("not json").profile);
        QVERIFY(!fromJson(R"({"format":"other","version":1})").profile);
        const ParseResult newer = fromJson(R"({"format":"plasma-keymap","version":99})");
        QVERIFY(!newer.profile);
        QVERIFY(newer.error.contains(QLatin1String("99")));
        const ParseResult partial = fromJson(R"({"format":"plasma-keymap","version":1,"vimMode":"yes","timeout":5,
            "bindings":{"move.down":"J","message.reply":"q","files.rename":"cw","move.up":"<Bad>","search":3}})");
        QVERIFY(partial.profile);
        QCOMPARE(partial.profile->vimMode, false);
        QCOMPARE(partial.profile->timeout, 200);
        QCOMPARE(partial.profile->bindings.size(), 1);
        QStringList ignored = partial.ignored;
        ignored.sort();
        QCOMPARE(ignored, (QStringList{QStringLiteral("files.rename"), QStringLiteral("message.reply"),
                                       QStringLiteral("move.up"), QStringLiteral("search")}));
    }

    void previewAndApply()
    {
        Profile current;
        current.bindings = defaultBindings();
        current.bindings.insert(QStringLiteral("message.reply"), QStringLiteral("q"));
        Profile incoming;
        incoming.vimMode = true;
        incoming.timeout = 1500;
        incoming.bindings.insert(QStringLiteral("move.down"), QStringLiteral("J"));
        incoming.bindings.insert(QStringLiteral("move.up"), QStringLiteral("k"));
        incoming.bindings.insert(QStringLiteral("message.reply"), QStringLiteral("z"));
        const QList<Change> changes = preview(current, incoming);
        QCOMPARE(changes, (QList<Change>{{QStringLiteral("vimMode"), QStringLiteral("off"), QStringLiteral("on")},
                                         {QStringLiteral("timeout"), QStringLiteral("1000"), QStringLiteral("1500")},
                                         {QStringLiteral("move.down"), QStringLiteral("j"), QStringLiteral("J")}}));
        const Profile result = apply(current, incoming);
        QVERIFY(result.vimMode);
        QCOMPARE(result.timeout, 1500);
        QCOMPARE(result.bindings.value(QStringLiteral("move.down")), QStringLiteral("J"));
        QCOMPARE(result.bindings.value(QStringLiteral("message.reply")), QStringLiteral("q"));
    }
};

QTEST_GUILESS_MAIN(KeymapTest)
#include "test_Keymap.moc"
