/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "UiUtils/FolderColors.h"
#include "UiUtils/StyleSheets.h"
#include "UiUtils/Theme.h"
#include "UiUtils/ThemeProfiles.h"
#include "UiUtils/Workspaces.h"

using namespace UiUtils;

class ThemeTest : public QObject {
    Q_OBJECT

private slots:
    void optionsMatchPlasmaOpus()
    {
        // Entries, choices and ranges shared with Plasma Opus's Themes group
        const Theme::Option *tint = Theme::option(QStringLiteral("PaneTint"));
        QVERIFY(tint);
        QCOMPARE(tint->choices,
                 (QStringList{QStringLiteral("None"), QStringLiteral("AllPanes"), QStringLiteral("ActivePane"),
                              QStringLiteral("InactivePane")}));
        QCOMPARE(tint->defaultValue.toString(), QStringLiteral("None"));
        const Theme::Option *strength = Theme::option(QStringLiteral("PaneTintStrength"));
        QCOMPARE(strength->minimum, 1);
        QCOMPARE(strength->maximum, 50);
        QCOMPARE(strength->defaultValue.toInt(), 10);
        QCOMPARE(Theme::option(QStringLiteral("TabIndicator"))->defaultValue.toString(), QStringLiteral("Line"));
        QCOMPARE(Theme::option(QStringLiteral("CustomCornerRadius"))->defaultValue.toInt(), 6);
        QCOMPARE(Theme::option(QStringLiteral("TreeLineColor"))->defaultValue.value<QColor>(), QColor(61, 174, 233));
        QCOMPARE(Theme::option(QStringLiteral("TreeLinePalette"))->defaultValue.toStringList().size(), 6);
        QCOMPARE(Theme::option(QStringLiteral("nope")), nullptr);
        for (const Theme::Option &option : Theme::options())
            QVERIFY2(Theme::validate(option.entry, option.defaultValue), qPrintable(option.entry));
    }

    void entrySets()
    {
        QVERIFY(!Theme::profileEntries().contains(QStringLiteral("FolderColorRules")));
        QVERIFY(!Theme::profileEntries().contains(QStringLiteral("SuspendEffectsInHighContrast")));
        QVERIFY(Theme::profileEntries().contains(QStringLiteral("CustomStyleSheet")));
        QVERIFY(Theme::overrideEntries().contains(QStringLiteral("FolderColorRules")));
        QVERIFY(Theme::overrideEntries().contains(QStringLiteral("MessageListStyle")));
        QVERIFY(!Theme::overrideEntries().contains(QStringLiteral("CustomStyleSheet")));
        QCOMPARE(Theme::keptOnReset(), QStringList{QStringLiteral("FolderColorRules")});
        for (const QString &entry : Theme::fontEntries())
            QVERIFY(Theme::option(entry));
    }

    void validation_data()
    {
        QTest::addColumn<QString>("entry");
        QTest::addColumn<QVariant>("value");
        QTest::addColumn<QVariant>("expected");

        QTest::newRow("choice") << "PaneTint" << QVariant(QStringLiteral("ActivePane"))
                                << QVariant(QStringLiteral("ActivePane"));
        QTest::newRow("unknown choice") << "PaneTint" << QVariant(QStringLiteral("Everything")) << QVariant();
        QTest::newRow("choice index") << "PaneTint" << QVariant(2) << QVariant();
        QTest::newRow("int in range") << "PaneTintStrength" << QVariant(50) << QVariant(50);
        QTest::newRow("int as text") << "SpacingScale" << QVariant(QStringLiteral("125")) << QVariant(125);
        QTest::newRow("int below") << "WindowOpacity" << QVariant(19) << QVariant();
        QTest::newRow("int above") << "CustomCornerRadius" << QVariant(25) << QVariant();
        QTest::newRow("fraction") << "SpacingScale" << QVariant(100.5) << QVariant();
        QTest::newRow("bool") << "BackgroundBlur" << QVariant(true) << QVariant(true);
        QTest::newRow("bool text") << "BackgroundBlur" << QVariant(QStringLiteral("false")) << QVariant(false);
        QTest::newRow("color text") << "PaneTintColor" << QVariant(QStringLiteral("#ff0000"))
                                    << QVariant(QColor(255, 0, 0));
        QTest::newRow("color alpha dropped")
            << "PaneTintColor" << QVariant(QColor(1, 2, 3, 4)) << QVariant(QColor(1, 2, 3));
        QTest::newRow("bad color") << "TreeLineColor" << QVariant(QStringLiteral("nocolor")) << QVariant();
        QTest::newRow("scheme name") << "ColorScheme" << QVariant(QStringLiteral("BreezeDark"))
                                     << QVariant(QStringLiteral("BreezeDark"));
        QTest::newRow("scheme path") << "ColorScheme" << QVariant(QStringLiteral("../evil")) << QVariant();
        QTest::newRow("accent plasma") << "AccentColor" << QVariant(QStringLiteral("plasma"))
                                       << QVariant(QStringLiteral("plasma"));
        QTest::newRow("accent color") << "AccentColor" << QVariant(QStringLiteral("#123456"))
                                      << QVariant(QStringLiteral("#123456"));
        QTest::newRow("accent junk") << "AccentColor" << QVariant(QStringLiteral("red-ish")) << QVariant();
        QTest::newRow("custom colors")
            << "CustomColors" << QVariant(QStringList{QStringLiteral("Base=#000000"), QStringLiteral("Unread=#ff0000")})
            << QVariant(QStringList{QStringLiteral("Base=#000000"), QStringLiteral("Unread=#ff0000")});
        QTest::newRow("custom colors unknown role")
            << "CustomColors" << QVariant(QStringList{QStringLiteral("Nope=#000000")}) << QVariant();
        QTest::newRow("empty palette") << "TreeLinePalette" << QVariant(QStringList()) << QVariant();
        QTest::newRow("long palette") << "TreeLinePalette" << QVariant(QStringList(9, QStringLiteral("#000000")))
                                      << QVariant();
        QTest::newRow("font") << "TabFont" << QVariant(QStringLiteral("Noto Sans,11,-1,5,400,0,0,0,0,0,0,0,0,0,0,1"))
                              << QVariant(QStringLiteral("Noto Sans,11,-1,5,400,0,0,0,0,0,0,0,0,0,0,1"));
        QTest::newRow("empty font") << "TabFont" << QVariant(QString()) << QVariant(QString());
        QTest::newRow("bad stylesheet") << "CustomStyleSheet" << QVariant(QStringLiteral("@import x;")) << QVariant();
        QTest::newRow("bad rule") << "FolderColorRules" << QVariant(QStringList{QStringLiteral("{}")}) << QVariant();
        QTest::newRow("unknown entry") << "Nope" << QVariant(true) << QVariant();
    }

    void validation()
    {
        QFETCH(QString, entry);
        QFETCH(QVariant, value);
        QFETCH(QVariant, expected);
        const std::optional<QVariant> result = Theme::validate(entry, value);
        QCOMPARE(result.has_value(), expected.isValid());
        if (result)
            QCOMPARE(*result, expected);
    }

    void sanitizedReportsProblems()
    {
        QStringList problems;
        const Theme::Values values = Theme::sanitized({{QStringLiteral("PaneTint"), QStringLiteral("AllPanes")},
                                                       {QStringLiteral("WindowOpacity"), 5},
                                                       {QStringLiteral("Unknown"), 1}},
                                                      &problems);
        QCOMPARE(values, (Theme::Values{{QStringLiteral("PaneTint"), QStringLiteral("AllPanes")}}));
        QCOMPARE(problems.size(), 2);
        QCOMPARE(Theme::value(values, QStringLiteral("WindowOpacity")).toInt(), 100);
    }

    void layering()
    {
        Theme::Layers layers;
        layers.global = {{QStringLiteral("PaneTint"), QStringLiteral("AllPanes")},
                         {QStringLiteral("BackgroundBlur"), true},
                         {QStringLiteral("UseCustomStyleSheet"), true},
                         {QStringLiteral("CustomStyleSheet"), QStringLiteral("QLabel { color: red; }")},
                         {QStringLiteral("FolderColorRules"), QStringList()}};
        // Global settings over the defaults
        Theme::Values values = Theme::resolve(layers);
        QCOMPARE(values.value(QStringLiteral("PaneTint")).toString(), QStringLiteral("AllPanes"));
        QCOMPARE(values.value(QStringLiteral("Dividers")).toString(), QStringLiteral("FollowStyle"));

        // A profile holds every option it covers: what it leaves out is back at the
        // default
        layers.profile = {{QStringLiteral("Dividers"), QStringLiteral("Hidden")}};
        values = Theme::resolve(layers);
        QCOMPARE(values.value(QStringLiteral("Dividers")).toString(), QStringLiteral("Hidden"));
        QCOMPARE(values.value(QStringLiteral("PaneTint")).toString(), QStringLiteral("None"));
        QCOMPARE(values.value(QStringLiteral("BackgroundBlur")).toBool(), false);
        // ... but the global stylesheet stays on top when the profile has none
        QCOMPARE(values.value(QStringLiteral("CustomStyleSheet")).toString(), QStringLiteral("QLabel { color: red; }"));
        QVERIFY(values.value(QStringLiteral("UseCustomStyleSheet")).toBool());

        // The profile's stylesheet wins when it has one
        layers.profile.insert(QStringLiteral("CustomStyleSheet"), QStringLiteral("QLabel { color: blue; }"));
        layers.profile.insert(QStringLiteral("UseCustomStyleSheet"), true);
        values = Theme::resolve(layers);
        QCOMPARE(values.value(QStringLiteral("CustomStyleSheet")).toString(),
                 QStringLiteral("QLabel { color: blue; }"));

        // Overrides on top; the stylesheet cannot be overridden by a workspace
        layers.overrides = {{QStringLiteral("Dividers"), QStringLiteral("Thin")},
                            {QStringLiteral("CustomStyleSheet"), QStringLiteral("QLabel { color: green; }")},
                            {QStringLiteral("MessageListStyle"), QStringLiteral("Cards")}};
        values = Theme::resolve(layers);
        QCOMPARE(values.value(QStringLiteral("Dividers")).toString(), QStringLiteral("Thin"));
        QCOMPARE(values.value(QStringLiteral("MessageListStyle")).toString(), QStringLiteral("Cards"));
        QCOMPARE(values.value(QStringLiteral("CustomStyleSheet")).toString(),
                 QStringLiteral("QLabel { color: blue; }"));
        QCOMPARE(values.size(), Theme::options().size());
    }

    void highContrastSuspendsEffects()
    {
        const Theme::Values values{{QStringLiteral("BackgroundBlur"), true},
                                   {QStringLiteral("WindowOpacity"), 60},
                                   {QStringLiteral("PaneTint"), QStringLiteral("AllPanes")},
                                   {QStringLiteral("CornerRadius"), QStringLiteral("Large")},
                                   {QStringLiteral("UseCustomStyleSheet"), true},
                                   {QStringLiteral("Dividers"), QStringLiteral("Hidden")}};
        QCOMPARE(Theme::withoutSuspendedEffects(values, false), values);
        const Theme::Values suspended = Theme::withoutSuspendedEffects(values, true);
        QCOMPARE(suspended.value(QStringLiteral("BackgroundBlur")).toBool(), false);
        QCOMPARE(suspended.value(QStringLiteral("WindowOpacity")).toInt(), 100);
        QCOMPARE(suspended.value(QStringLiteral("PaneTint")).toString(), QStringLiteral("None"));
        QCOMPARE(suspended.value(QStringLiteral("CornerRadius")).toString(), QStringLiteral("FollowStyle"));
        QCOMPARE(suspended.value(QStringLiteral("UseCustomStyleSheet")).toBool(), false);
        QCOMPARE(suspended.value(QStringLiteral("Dividers")).toString(), QStringLiteral("Hidden"));

        Theme::Values optOut = values;
        optOut.insert(QStringLiteral("SuspendEffectsInHighContrast"), false);
        QVERIFY(!Theme::effectsSuspended(optOut, true));
        QCOMPARE(Theme::withoutSuspendedEffects(optOut, true), optOut);
    }

    void resetKeepsFolderRules()
    {
        const QString rule = FolderColors::entry(
            {QString(), FolderColors::Match::Name, QStringLiteral("Archive*"), QColor(Qt::red), false});
        const Theme::Values global{{QStringLiteral("PaneTint"), QStringLiteral("AllPanes")},
                                   {QStringLiteral("FolderColorRules"), QStringList{rule}},
                                   {QStringLiteral("SpacingScale"), 100}};
        QStringList changed;
        const Theme::Values reset = Theme::reset(global, &changed);
        QCOMPARE(reset.value(QStringLiteral("PaneTint")).toString(), QStringLiteral("None"));
        QCOMPARE(reset.value(QStringLiteral("FolderColorRules")).toStringList(), QStringList{rule});
        QCOMPARE(changed, QStringList{QStringLiteral("PaneTint")});
    }

    void helpers()
    {
        QCOMPARE(Theme::accentColor(QString(), Qt::blue), QColor(Qt::blue));
        QCOMPARE(Theme::accentColor(QStringLiteral("plasma"), Qt::blue), QColor(Qt::blue));
        QCOMPARE(Theme::accentColor(QStringLiteral("#ff0000"), Qt::blue), QColor(Qt::red));

        QCOMPARE(Theme::spacingPreset(100), Theme::Spacing::Normal);
        QCOMPARE(Theme::spacingPreset(80), Theme::Spacing::Compact);
        QCOMPARE(Theme::spacingPreset(125), Theme::Spacing::Comfortable);
        QCOMPARE(Theme::spacingPreset(140), Theme::Spacing::Custom);
        QCOMPARE(Theme::spacingScale(Theme::Spacing::Custom, 140), 140);
        QCOMPARE(Theme::spacingScale(Theme::Spacing::Custom, 900), 200);
        QCOMPARE(Theme::scaled(4, 50), 2);
        QCOMPARE(Theme::scaled(1, 50), 1);
        QCOMPARE(Theme::scaled(0, 200), 0);
        QCOMPARE(Theme::scaled(6, 125), 8);

        QCOMPARE(Theme::cornerRadius(QStringLiteral("FollowStyle"), 6), -1);
        QCOMPARE(Theme::cornerRadius(QStringLiteral("None"), 6), 0);
        QCOMPARE(Theme::cornerRadius(QStringLiteral("Large"), 6), 10);
        QCOMPARE(Theme::cornerRadius(QStringLiteral("Custom"), 30), 24);

        QCOMPARE(Theme::animationDuration(200, QStringLiteral("FollowPlasma"), 1), 200);
        QCOMPARE(Theme::animationDuration(200, QStringLiteral("FollowPlasma"), 0.5), 100);
        QCOMPARE(Theme::animationDuration(200, QStringLiteral("Reduced"), 1), 100);
        QCOMPARE(Theme::animationDuration(200, QStringLiteral("Off"), 1), 0);
        QCOMPARE(Theme::animationDuration(200, QStringLiteral("FollowPlasma"), 0), 0);
    }

    void treeLineColors()
    {
        const QColor neutral(Qt::gray);
        Theme::Values values;
        QCOMPARE(Theme::treeLineColor(values, 2, Qt::red, neutral), neutral);
        values.insert(QStringLiteral("TreeLineColors"), QStringLiteral("Single"));
        QCOMPARE(Theme::treeLineColor(values, 2, Qt::red, neutral), QColor(61, 174, 233));
        values.insert(QStringLiteral("TreeLineColors"), QStringLiteral("Depth"));
        QCOMPARE(Theme::treeLineColor(values, 0, Qt::red, neutral), QColor(QStringLiteral("#3daee9")));
        QCOMPARE(Theme::treeLineColor(values, 7, Qt::red, neutral), QColor(QStringLiteral("#27ae60")));
        values.insert(QStringLiteral("TreeLineColors"), QStringLiteral("Folder"));
        QCOMPARE(Theme::treeLineColor(values, 1, Qt::red, neutral), QColor(Qt::red));
        QCOMPARE(Theme::treeLineColor(values, 1, QColor(), neutral), neutral);
    }

    void colorRoles()
    {
        const Theme::Colors colors = Theme::parseColors(
            {QStringLiteral("Base=#ffffff"), QStringLiteral("Text = #777777"), QStringLiteral("Nope=#000000"),
             QStringLiteral("Unread=#bad"), QStringLiteral("garbage")});
        QCOMPARE(colors.size(), 3);
        QCOMPARE(Theme::formatColors(colors),
                 (QStringList{QStringLiteral("Base=#ffffff"), QStringLiteral("Text=#777777"),
                              QStringLiteral("Unread=#bbaadd")}));

        // #777777 on white is 4.48:1, just short of 4.5:1; #bbaadd is far below
        const QList<Theme::ContrastIssue> issues = Theme::contrastIssues(colors);
        QCOMPARE(issues.size(), 2);
        QCOMPARE(issues.at(1).foreground, QStringLiteral("Unread"));
        QCOMPARE(issues.constFirst().foreground, QStringLiteral("Text"));
        QCOMPARE(issues.constFirst().minimum, 4.5);
        QVERIFY(Theme::describe(issues.constFirst()).contains(QLatin1String("4.5")));
        // Unchanged pairs are not the user's mistake
        QVERIFY(Theme::contrastIssues(colors, {QStringLiteral("Highlight")}).isEmpty());
        QCOMPARE(Theme::contrastIssues(colors, {QStringLiteral("Base")}).size(), 2);
        QCOMPARE(Theme::contrastIssues(colors, {QStringLiteral("Text")}).size(), 1);
    }

    void styleSheets_data()
    {
        QTest::addColumn<QString>("sheet");
        QTest::addColumn<bool>("valid");
        QTest::newRow("empty") << QString() << true;
        QTest::newRow("simple") << QStringLiteral("QTreeView { background: #202020; }") << true;
        QTest::newRow("resource url") << QStringLiteral("QLabel { image: url(:/icons/x.png); }") << true;
        QTest::newRow("qrc url") << QStringLiteral("QLabel { image: url(\"qrc:/x.png\"); }") << true;
        QTest::newRow("file url") << QStringLiteral("QLabel { image: url(/etc/passwd); }") << false;
        QTest::newRow("http url") << QStringLiteral("QLabel { image: url(https://x.test/a.png); }") << false;
        QTest::newRow("import") << QStringLiteral("@import \"x.css\";") << false;
        QTest::newRow("qproperty") << QStringLiteral("QLabel { qproperty-text: \"x\"; }") << false;
        QTest::newRow("unclosed rule") << QStringLiteral("QLabel { color: red;") << false;
        QTest::newRow("stray brace") << QStringLiteral("}") << false;
        QTest::newRow("nested") << QStringLiteral("A { B { } }") << false;
        QTest::newRow("unclosed comment") << QStringLiteral("/* x") << false;
        QTest::newRow("comment hides at") << QStringLiteral("/* @import */ QLabel {}") << true;
        QTest::newRow("string hides brace") << QStringLiteral("QLabel[text=\"{\"] {}") << true;
        QTest::newRow("unclosed string") << QStringLiteral("QLabel[text=\"x] {}") << false;
        QTest::newRow("parentheses") << QStringLiteral("QLabel { color: rgb(1, 2, 3; }") << false;
        QTest::newRow("control") << QStringLiteral("QLabel { color: red; }\x01") << false;
        QTest::newRow("too long") << QString(StyleSheets::MaximumSize + 1, QLatin1Char(' ')) << false;
        QTest::newRow("too many rules") << QStringLiteral("A {}").repeated(StyleSheets::MaximumRules + 1) << false;
    }

    void styleSheets()
    {
        QFETCH(QString, sheet);
        QFETCH(bool, valid);
        const QString error = StyleSheets::check(sheet);
        QVERIFY2(error.isEmpty() == valid, qPrintable(error));
    }

    void styleSheetErrorsNameTheLine()
    {
        QVERIFY(StyleSheets::check(QStringLiteral("A {}\n/* two\nlines */\nB { image: url(x.png); }"))
                    .contains(QLatin1String("4")));
    }

    void styleSheetFiles()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("a.qss"));
        QCOMPARE(StyleSheets::writeFile(path, QStringLiteral("QLabel { color: red; }")), QString());
        QString error;
        QCOMPARE(StyleSheets::readFile(path, &error), std::optional<QString>(QStringLiteral("QLabel { color: red; }")));
        QFile big(dir.filePath(QStringLiteral("big.qss")));
        QVERIFY(big.open(QIODevice::WriteOnly));
        big.write(QByteArray(StyleSheets::MaximumSize + 1, ' '));
        big.close();
        QVERIFY(!StyleSheets::readFile(big.fileName(), &error));
        QVERIFY(!error.isEmpty());
    }

    void folderRules()
    {
        using FolderColors::Match;
        using FolderColors::Rule;
        const Rule lists{QString(), Match::Path, QStringLiteral("Lists"), QColor(Qt::red), true};
        const Rule archive{QStringLiteral("work"), Match::Name, QStringLiteral("archive*"), QColor(Qt::blue), false};
        QCOMPARE(FolderColors::parse(FolderColors::entry(lists)), std::optional<Rule>(lists));
        QCOMPARE(FolderColors::parse(FolderColors::entry(archive)), std::optional<Rule>(archive));
        QVERIFY(!FolderColors::parse(QStringLiteral("{\"match\":\"name\",\"pattern\":\"a/b\",\"color\":\"#fff\"}")));
        QVERIFY(!FolderColors::parse(QStringLiteral("{\"match\":\"tag\",\"pattern\":\"a\",\"color\":\"#fff\"}")));
        QVERIFY(!FolderColors::parse(QStringLiteral("{\"match\":\"name\",\"pattern\":\"a\",\"color\":\"x\"}")));
        QVERIFY(!FolderColors::parse(QStringLiteral("not json")));

        const QList<Rule> rules = FolderColors::rules(FolderColors::entries({lists, archive, lists}));
        QCOMPARE(rules.size(), 2);
        QCOMPARE(FolderColors::ruleColor(rules, QStringLiteral("home"), QStringLiteral("Lists"), QLatin1Char('.')),
                 QColor(Qt::red));
        // Subfolders inherit, with the account's own separator
        QCOMPARE(
            FolderColors::ruleColor(rules, QStringLiteral("home"), QStringLiteral("Lists.kde.devel"), QLatin1Char('.')),
            QColor(Qt::red));
        // Names match case-insensitively, only for their account
        QCOMPARE(FolderColors::ruleColor(rules, QStringLiteral("work"), QStringLiteral("INBOX/Archive 2025"),
                                         QLatin1Char('/')),
                 QColor(Qt::blue));
        QCOMPARE(FolderColors::ruleColor(rules, QStringLiteral("home"), QStringLiteral("INBOX/Archive 2025"),
                                         QLatin1Char('/')),
                 QColor());
        // Without inheritance a subfolder does not match
        QCOMPARE(
            FolderColors::ruleColor(rules, QStringLiteral("work"), QStringLiteral("Archive/2024"), QLatin1Char('/')),
            QColor());

        FolderColors::Colors colors;
        colors.rules = rules;
        colors.accounts.insert(QStringLiteral("work"), QColor(Qt::green));
        colors.folders[QStringLiteral("home")].insert(QStringLiteral("Lists.kde"), QColor(Qt::yellow));
        QCOMPARE(FolderColors::accountColor(colors, QStringLiteral("work")), QColor(Qt::green));
        QCOMPARE(FolderColors::accountColor(colors, QStringLiteral("home")), QColor());
        // An explicit colour wins over the rules
        QCOMPARE(
            FolderColors::folderColor(colors, QStringLiteral("home"), QStringLiteral("Lists.kde"), QLatin1Char('.')),
            QColor(Qt::yellow));
        QCOMPARE(
            FolderColors::folderColor(colors, QStringLiteral("home"), QStringLiteral("Lists.qt"), QLatin1Char('.')),
            QColor(Qt::red));
    }

    void folderRuleLimit()
    {
        QStringList entries;
        for (int i = 0; i < FolderColors::MaximumRules + 5; ++i)
            entries << FolderColors::entry(
                {QString(), FolderColors::Match::Name, QStringLiteral("f%1").arg(i), QColor(Qt::red), false});
        QCOMPARE(FolderColors::rules(entries).size(), FolderColors::MaximumRules);
    }

    void folderIcons()
    {
        QCOMPARE(FolderColors::iconName(QColor(220, 60, 80)), QStringLiteral("folder-red"));
        QCOMPARE(FolderColors::iconName(QColor(60, 170, 230)), QStringLiteral("folder-blue"));
        QCOMPARE(FolderColors::iconName(QColor(40, 170, 90)), QStringLiteral("folder-green"));
        QCOMPARE(FolderColors::iconName(QColor()), QString());
        QCOMPARE(FolderColors::iconColors().size(), 12);
        for (const QString &name : FolderColors::iconColors())
            QCOMPARE(FolderColors::iconName(FolderColors::iconColor(name)), QStringLiteral("folder-") + name);
        QVERIFY(!FolderColors::iconColor(QStringLiteral("plaid")).isValid());
    }

    void profileAppliedToGlobal()
    {
        Theme::Values global = Theme::defaults();
        global.insert(QStringLiteral("FolderColorRules"), QStringList{QStringLiteral("rule")});
        global.insert(QStringLiteral("StripeRows"), false);
        global.insert(QStringLiteral("SuspendEffectsInHighContrast"), false);
        Theme::Values chosen = Theme::defaults();
        chosen.insert(QStringLiteral("CornerRadius"), QStringLiteral("Large"));
        const ThemeProfiles::Profile profile = ThemeProfiles::fromValues(QStringLiteral("  Large  "), chosen);
        QCOMPARE(profile.name, QStringLiteral("Large"));
        QVERIFY(!profile.values.contains(QStringLiteral("FolderColorRules")));
        const Theme::Values applied = ThemeProfiles::appliedTo(global, profile);
        QCOMPARE(applied.value(QStringLiteral("CornerRadius")).toString(), QStringLiteral("Large"));
        // The profile holds every profile option, so the global stripe setting is replaced
        QCOMPARE(applied.value(QStringLiteral("StripeRows")).toBool(), true);
        // Options a profile never holds stay
        QCOMPARE(applied.value(QStringLiteral("FolderColorRules")).toStringList(), QStringList{QStringLiteral("rule")});
        QCOMPARE(applied.value(QStringLiteral("SuspendEffectsInHighContrast")).toBool(), false);
        // A file leaving options out keeps the global ones
        ThemeProfiles::Profile partial;
        partial.values.insert(QStringLiteral("Dividers"), QStringLiteral("Hidden"));
        const Theme::Values merged = ThemeProfiles::appliedTo(global, partial);
        QCOMPARE(merged.value(QStringLiteral("Dividers")).toString(), QStringLiteral("Hidden"));
        QCOMPARE(merged.value(QStringLiteral("StripeRows")).toBool(), false);
    }

    void profileRoundTrip()
    {
        ThemeProfiles::Profile profile;
        profile.name = QStringLiteral("  Night  ");
        profile.values = Theme::defaults();
        profile.values.insert(QStringLiteral("PaneTint"), QStringLiteral("ActivePane"));
        profile.values.insert(QStringLiteral("PaneTintColor"), QColor(Qt::red));
        profile.values.insert(QStringLiteral("MessageListStyle"), QStringLiteral("Cards"));
        profile.values.insert(QStringLiteral("MessageListFont"), QStringLiteral("Noto Sans,10,-1,5,400,0,0,0,0,0"));
        profile.values.insert(QStringLiteral("StripeRows"), false);
        profile.values.insert(QStringLiteral("StripeFolderTree"), true);
        profile.values.insert(QStringLiteral("CustomStyleSheet"), QStringLiteral("QLabel { color: red; }"));
        for (const QString &entry : Theme::entries()) {
            if (!Theme::profileEntries().contains(entry))
                profile.values.remove(entry);
        }

        const QByteArray json = ThemeProfiles::toJson(profile);
        const QJsonObject object = QJsonDocument::fromJson(json).object();
        QCOMPARE(object.value(QStringLiteral("format")).toString(), QStringLiteral("plasma-impresario-theme"));
        QCOMPARE(object.value(QStringLiteral("version")).toInt(), 1);
        QCOMPARE(object.value(QStringLiteral("name")).toString(), QStringLiteral("Night"));
        const QJsonObject appearance = object.value(QStringLiteral("appearance")).toObject();
        QCOMPARE(appearance.value(QStringLiteral("PaneTintColor")).toString(), QStringLiteral("#ff0000"));
        QVERIFY(!appearance.contains(QStringLiteral("FolderColorRules")));
        QVERIFY(!appearance.contains(QStringLiteral("SuspendEffectsInHighContrast")));

        const ThemeProfiles::ParseResult parsed = ThemeProfiles::fromJson(json);
        QVERIFY2(parsed.profile, qPrintable(parsed.error));
        QVERIFY(!parsed.fromOpus);
        QCOMPARE(parsed.profile->name, QStringLiteral("Night"));
        QCOMPARE(parsed.profile->values, profile.values);

        // Without the stylesheet, which then has its default
        const ThemeProfiles::ParseResult bare =
            ThemeProfiles::fromJson(ThemeProfiles::toJson(profile, ThemeProfiles::Target::Impresario, false));
        QCOMPARE(bare.profile->values.value(QStringLiteral("CustomStyleSheet")).toString(), QString());
    }

    void opusExchange()
    {
        ThemeProfiles::Profile profile;
        profile.name = QStringLiteral("Shared");
        profile.values = {{QStringLiteral("MessageListFont"), QStringLiteral("Noto Sans,10,-1,5,400,0,0,0,0,0")},
                          {QStringLiteral("StripeRows"), true},
                          {QStringLiteral("MessageListStyle"), QStringLiteral("Cards")},
                          {QStringLiteral("StripeFolderTree"), true},
                          {QStringLiteral("Dividers"), QStringLiteral("Thin")}};
        const QJsonObject object =
            QJsonDocument::fromJson(ThemeProfiles::toJson(profile, ThemeProfiles::Target::Opus)).object();
        QCOMPARE(object.value(QStringLiteral("format")).toString(), QStringLiteral("plasma-opus-theme"));
        const QJsonObject appearance = object.value(QStringLiteral("appearance")).toObject();
        QCOMPARE(appearance.value(QStringLiteral("FileListFont")).toString(),
                 QStringLiteral("Noto Sans,10,-1,5,400,0,0,0,0,0"));
        QCOMPARE(appearance.value(QStringLiteral("StripeDetailsRows")).toBool(), true);
        QVERIFY(!appearance.contains(QStringLiteral("MessageListFont")));
        QVERIFY(!appearance.contains(QStringLiteral("MessageListStyle")));
        QVERIFY(!appearance.contains(QStringLiteral("PlainTextFont")));
        QVERIFY(!appearance.contains(QStringLiteral("StripeFolderTree")));
        QCOMPARE(appearance.value(QStringLiteral("Dividers")).toString(), QStringLiteral("Thin"));

        // A Plasma Opus profile, with its own names and an option only Plasma Opus
        // has
        QJsonObject opusAppearance = appearance;
        opusAppearance.insert(QStringLiteral("DefaultViewMode"), QStringLiteral("Icons"));
        QJsonObject opus = object;
        opus.insert(QStringLiteral("appearance"), opusAppearance);
        const ThemeProfiles::ParseResult parsed = ThemeProfiles::fromJson(QJsonDocument(opus).toJson());
        QVERIFY2(parsed.profile, qPrintable(parsed.error));
        QVERIFY(parsed.fromOpus);
        QCOMPARE(parsed.warnings.size(), 1);
        QCOMPARE(parsed.profile->values.value(QStringLiteral("MessageListFont")).toString(),
                 QStringLiteral("Noto Sans,10,-1,5,400,0,0,0,0,0"));
        QCOMPARE(parsed.profile->values.value(QStringLiteral("StripeRows")).toBool(), true);
        QCOMPARE(parsed.profile->values.value(QStringLiteral("MessageListStyle")).toString(), QStringLiteral("Rows"));
    }

    void invalidProfiles_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("not json") << QByteArray("nope");
        QTest::newRow("other format") << QByteArray(R"({"format":"x","version":1,"appearance":{}})");
        QTest::newRow("no version") << QByteArray(R"({"format":"plasma-impresario-theme","appearance":{}})");
        QTest::newRow("newer version") << QByteArray(
            R"({"format":"plasma-impresario-theme","version":2,"appearance":{}})");
        QTest::newRow("no appearance") << QByteArray(R"({"format":"plasma-impresario-theme","version":1})");
        QTest::newRow("bad value") << QByteArray(
            R"({"format":"plasma-impresario-theme","version":1,"appearance":{"WindowOpacity":5}})");
        QTest::newRow("wrong type") << QByteArray(
            R"({"format":"plasma-impresario-theme","version":1,"appearance":{"BackgroundBlur":"yes"}})");
        QTest::newRow("bad stylesheet") << QByteArray(
            R"({"format":"plasma-impresario-theme","version":1,"appearance":{"CustomStyleSheet":"@import x;"}})");
        QTest::newRow("control name") << QByteArray("{\"format\":\"plasma-impresario-theme\",\"version\":1,"
                                                    "\"name\":\"a\\nb\",\"appearance\":{}}");
        QTest::newRow("too large") << QByteArray(ThemeProfiles::MaximumFileSize + 1, ' ');
    }

    void invalidProfiles()
    {
        QFETCH(QByteArray, json);
        const ThemeProfiles::ParseResult result = ThemeProfiles::fromJson(json);
        QVERIFY(!result.profile);
        QVERIFY(!result.error.isEmpty());
    }

    void profileFiles()
    {
        QTemporaryDir dir;
        ThemeProfiles::Profile profile{
            QString(), QStringLiteral("File"), {{QStringLiteral("Dividers"), QStringLiteral("Hidden")}}};
        const QString path = dir.filePath(QStringLiteral("theme.json"));
        QCOMPARE(ThemeProfiles::exportFile(profile, path), QString());
        const ThemeProfiles::ParseResult parsed = ThemeProfiles::importFile(path);
        QVERIFY(parsed.profile);
        QCOMPARE(parsed.profile->values.value(QStringLiteral("Dividers")).toString(), QStringLiteral("Hidden"));
        QVERIFY(!ThemeProfiles::importFile(dir.filePath(QStringLiteral("missing.json"))).profile);
    }

    void profileLists()
    {
        QList<ThemeProfiles::Profile> profiles{
            {QStringLiteral("a"), QStringLiteral("Day"), {{QStringLiteral("Dividers"), QStringLiteral("Thin")}}},
            {QStringLiteral("b"), QStringLiteral("Night"), {}}};
        QCOMPARE(ThemeProfiles::listError(profiles), QString());
        QCOMPARE(ThemeProfiles::indexOfName(profiles, QStringLiteral(" night ")), 1);
        QCOMPARE(ThemeProfiles::indexOfName(profiles, QStringLiteral("Dusk")), -1);
        QVERIFY(ThemeProfiles::find(profiles, QStringLiteral("a")));
        QVERIFY(!ThemeProfiles::find(profiles, QStringLiteral("c")));
        QVERIFY(!profiles.contains(ThemeProfiles::Profile{ThemeProfiles::newId(profiles), {}, {}}));

        QStringList problems;
        const QList<ThemeProfiles::Profile> restored =
            ThemeProfiles::restore(ThemeProfiles::store(profiles) << QStringLiteral("junk"), &problems);
        QCOMPARE(restored.size(), 2);
        QCOMPARE(restored.at(0).id, QStringLiteral("a"));
        QCOMPARE(restored.at(0).values.value(QStringLiteral("Dividers")).toString(), QStringLiteral("Thin"));
        QCOMPARE(problems.size(), 1);

        profiles.append({QStringLiteral("c"), QStringLiteral("DAY"), {}});
        QVERIFY(!ThemeProfiles::listError(profiles).isEmpty());
        QVERIFY(!ThemeProfiles::nameError(QString(ThemeProfiles::MaximumNameLength + 1, QLatin1Char('x'))).isEmpty());
        QVERIFY(!ThemeProfiles::nameError(QStringLiteral("  ")).isEmpty());
    }

    void workspaceAppearance()
    {
        const QList<ThemeProfiles::Profile> profiles{{QStringLiteral("p"),
                                                      QStringLiteral("Cards"),
                                                      {{QStringLiteral("MessageListStyle"), QStringLiteral("Cards")},
                                                       {QStringLiteral("Dividers"), QStringLiteral("Hidden")}}}};
        const Theme::Values global{{QStringLiteral("PaneTint"), QStringLiteral("AllPanes")}};
        Workspaces::Workspace work;
        work.name = QStringLiteral("Work");
        work.themeProfile = QStringLiteral("p");
        work.appearanceOverrides = {{QStringLiteral("CornerRadius"), QStringLiteral("Large")},
                                    {QStringLiteral("CustomStyleSheet"), QStringLiteral("QLabel {}")}};

        // None applies nothing
        Workspaces::ResolvedAppearance resolved = Workspaces::appearance(&work, profiles, global);
        QVERIFY(resolved.layers.profile.isEmpty());
        QVERIFY(resolved.layers.overrides.isEmpty());

        work.appearance = Workspaces::Workspace::Appearance::All;
        resolved = Workspaces::appearance(&work, profiles, global);
        QCOMPARE(resolved.problems.size(),
                 1); // the stylesheet cannot be overridden
        Theme::Values values = Theme::resolve(resolved.layers);
        QCOMPARE(values.value(QStringLiteral("MessageListStyle")).toString(), QStringLiteral("Cards"));
        QCOMPARE(values.value(QStringLiteral("CornerRadius")).toString(), QStringLiteral("Large"));
        QCOMPARE(values.value(QStringLiteral("PaneTint")).toString(), QStringLiteral("None"));

        // Selected options only; the others keep their global values
        work.appearance = Workspaces::Workspace::Appearance::Selected;
        work.selectedAppearance = {QStringLiteral("MessageListStyle")};
        values = Theme::resolve(Workspaces::appearance(&work, profiles, global).layers);
        QCOMPARE(values.value(QStringLiteral("MessageListStyle")).toString(), QStringLiteral("Cards"));
        QCOMPARE(values.value(QStringLiteral("Dividers")).toString(), QStringLiteral("FollowStyle"));
        QCOMPARE(values.value(QStringLiteral("CornerRadius")).toString(), QStringLiteral("FollowStyle"));
        QCOMPARE(values.value(QStringLiteral("PaneTint")).toString(), QStringLiteral("AllPanes"));

        // Started with --no-workspace-appearance
        work.appearance = Workspaces::Workspace::Appearance::All;
        resolved = Workspaces::appearance(&work, profiles, global, true);
        QVERIFY(resolved.layers.overrides.isEmpty());

        // A missing profile is reported and its reference kept
        work.themeProfile = QStringLiteral("gone");
        resolved = Workspaces::appearance(&work, profiles, global);
        QVERIFY(resolved.problems.constFirst().contains(QLatin1String("Work")));
        QCOMPARE(work.themeProfile, QStringLiteral("gone"));
    }

    void activeWorkspace()
    {
        Workspaces::Workspace home{QStringLiteral("h"), QStringLiteral("Home"), {QStringLiteral("me")}, {}, {}};
        Workspaces::Workspace work{
            QStringLiteral("w"), QStringLiteral("Work"), {QStringLiteral("job")}, QStringLiteral("act"), {}};
        const QList<Workspaces::Workspace> list{home, work};
        QCOMPARE(Workspaces::active(list, QStringLiteral("act"), QStringLiteral("me"))->id, QStringLiteral("w"));
        QCOMPARE(Workspaces::active(list, QString(), QStringLiteral("me"))->id, QStringLiteral("h"));
        QCOMPARE(Workspaces::active(list, QString(), QStringLiteral("other")), nullptr);
        QCOMPARE(
            Workspaces::appearanceFromConfigName(Workspaces::configName(Workspaces::Workspace::Appearance::Selected)),
            Workspaces::Workspace::Appearance::Selected);
        QCOMPARE(Workspaces::appearanceFromConfigName(QStringLiteral("junk")), Workspaces::Workspace::Appearance::None);
    }
};

QTEST_GUILESS_MAIN(ThemeTest)
#include "test_Theme.moc"
