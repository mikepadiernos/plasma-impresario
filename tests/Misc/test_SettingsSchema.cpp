/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QSet>
#include <QTest>

#include "Common/SettingsNames.h"
#include "UiUtils/SettingsSchema.h"

using namespace UiUtils::SettingsSchema;

class SettingsSchemaTest : public QObject
{
    Q_OBJECT

private slots:
    void groupsAreOrderedAndNamed()
    {
        const QList<Group> all = groups();
        QCOMPARE(all.size(), 12);
        QCOMPARE(all.at(3), Group::Theme);
        QCOMPARE(all.first(), Group::General);
        QCOMPARE(all.last(), Group::Keyboard);
        QSet<QString> ids;
        for (Group group : all) {
            const QString id = groupId(group);
            QVERIFY(!ids.contains(id));
            ids.insert(id);
            QCOMPARE(groupFromId(id), std::optional<Group>(group));
        }
        QCOMPARE(groupFromId(QStringLiteral("nope")), std::nullopt);
    }

    void definitionsAreConsistent()
    {
        QSet<QString> keys;
        for (const Definition &def : definitions()) {
            QVERIFY2(!keys.contains(def.key), qPrintable(def.key));
            keys.insert(def.key);
            QVERIFY(def.defaultValue.isValid());
            QVERIFY(def.scopes.testFlag(Scope::Application));
            QVERIFY(!configGroup(def.key).isEmpty());
            QVERIFY(!configEntry(def.key).isEmpty());
            QCOMPARE(definition(def.key), &def);
        }
        QCOMPARE(definition(QStringLiteral("Unknown/Key")), nullptr);
        QCOMPARE(configGroup(QStringLiteral("Reading/AutoMarkRead")), QStringLiteral("Reading"));
        QCOMPARE(configEntry(QStringLiteral("Reading/AutoMarkRead")), QStringLiteral("AutoMarkRead"));
        // The dialog shows the Reading group's settings together
        QCOMPARE(definitions(Group::Reading).size(), 3);
    }

    void defaultsKeepOptionalFeaturesOff()
    {
        QCOMPARE(definition(QStringLiteral("Tabs/Enabled"))->defaultValue, QVariant(false));
        QCOMPARE(definition(QStringLiteral("Tabs/Region"))->defaultValue, QVariant(QStringLiteral("header")));
        QCOMPARE(definition(QStringLiteral("Keyboard/VimMode"))->defaultValue, QVariant(false));
        QCOMPARE(definition(QStringLiteral("Keyboard/FollowSharedKeymap"))->defaultValue, QVariant(false));
        QCOMPARE(definition(QStringLiteral("Appearance/RowDensity"))->defaultValue, QVariant(QStringLiteral("compact")));
    }

    void legacyKeysAreKept()
    {
        QCOMPARE(definition(QStringLiteral("Reading/AutoMarkRead"))->legacyKey, Common::SettingsNames::autoMarkReadEnabled);
        QCOMPARE(definition(QStringLiteral("Reading/PreferPlainText"))->legacyKey,
                 Common::SettingsNames::guiPreferPlaintextRendering);
    }

    void scopes()
    {
        const QStringList folder = keys(Scope::Folder);
        QVERIFY(folder.contains(QStringLiteral("Reading/AutoMarkRead")));
        QVERIFY(!folder.contains(QStringLiteral("Shell/StartFolder")));
        const QStringList account = keys(Scope::Account);
        QVERIFY(account.contains(QStringLiteral("Shell/StartFolder")));
        QVERIFY(account.contains(QStringLiteral("Reading/MarkReadDelay")));
        QVERIFY(!account.contains(QStringLiteral("Keyboard/VimMode")));
    }

    void availability()
    {
        const Definition *calendar = definition(QStringLiteral("Integrations/MailToCalendar"));
        QVERIFY(!isAvailable(*calendar, {}));
        QVERIFY(!isAvailable(*calendar, Feature::Notes));
        QVERIFY(isAvailable(*calendar, Feature::Calendar | Feature::Notes));
        QVERIFY(isAvailable(*definition(QStringLiteral("Reading/AutoMarkRead")), {}));
        for (Feature feature : {Feature::Calendar, Feature::Notes, Feature::Activities})
            QVERIFY(!unavailableReason(feature).isEmpty());
        QVERIFY(unavailableReason(Feature::None).isEmpty());
    }

    void coercion()
    {
        const Definition *flag = definition(QStringLiteral("Reading/AutoMarkRead"));
        QCOMPARE(coerce(*flag, QStringLiteral("false")), QVariant(false));
        QCOMPARE(coerce(*flag, QStringLiteral("TRUE")), QVariant(true));
        QCOMPARE(coerce(*flag, QStringLiteral("perhaps")), QVariant());
        const Definition *delay = definition(QStringLiteral("Reading/MarkReadDelay"));
        QCOMPARE(coerce(*delay, QStringLiteral("5")), QVariant(5));
        QCOMPARE(coerce(*delay, QStringLiteral("five")), QVariant());
        QCOMPARE(coerce(*delay, QVariant()), QVariant());
        const Definition *folder = definition(QStringLiteral("Shell/StartFolder"));
        QCOMPARE(coerce(*folder, QStringLiteral("INBOX")), QVariant(QStringLiteral("INBOX")));
    }

    void resolveTakesTheMostSpecificLayer()
    {
        const QString key = QStringLiteral("Reading/AutoMarkRead");
        Layers layers;
        QCOMPARE(resolve(key, layers), (Resolved{true, Layer::Default}));
        layers.application.insert(key, false);
        QCOMPARE(resolve(key, layers), (Resolved{false, Layer::Application}));
        layers.account.insert(key, true);
        QCOMPARE(resolve(key, layers), (Resolved{true, Layer::Account}));
        layers.folder.insert(key, QStringLiteral("false"));
        QCOMPARE(resolve(key, layers), (Resolved{false, Layer::Folder}));
        // What a folder override replaces, and what an account override replaces
        QCOMPARE(inherited(key, layers, Layer::Folder), (Resolved{true, Layer::Account}));
        QCOMPARE(inherited(key, layers, Layer::Account), (Resolved{false, Layer::Application}));
        QCOMPARE(inherited(key, layers, Layer::Application), (Resolved{true, Layer::Default}));
    }

    void resolveSkipsLayersTheSettingDoesNotAllow()
    {
        const QString key = QStringLiteral("Shell/StartFolder");
        Layers layers;
        layers.application.insert(key, QStringLiteral("INBOX"));
        layers.folder.insert(key, QStringLiteral("Ignored"));
        QCOMPARE(resolve(key, layers), (Resolved{QStringLiteral("INBOX"), Layer::Application}));
        const QString vim = QStringLiteral("Keyboard/VimMode");
        layers.account.insert(vim, true);
        QCOMPARE(resolve(vim, layers), (Resolved{false, Layer::Default}));
    }

    void resolveSkipsValuesOfTheWrongType()
    {
        const QString key = QStringLiteral("Reading/MarkReadDelay");
        Layers layers;
        layers.application.insert(key, 3);
        layers.account.insert(key, QStringLiteral("soon"));
        QCOMPARE(resolve(key, layers), (Resolved{3, Layer::Application}));
        QCOMPARE(resolve(QStringLiteral("Unknown/Key"), layers), Resolved());
    }

    void explanations()
    {
        QVERIFY(explanation(Layer::Account, QStringLiteral("Work"), QString()).contains(QLatin1String("Work")));
        QVERIFY(explanation(Layer::Folder, QString(), QStringLiteral("Lists")).contains(QLatin1String("Lists")));
        QVERIFY(!explanation(Layer::Default, {}, {}).isEmpty());
        QVERIFY(!explanation(Layer::Application, {}, {}).isEmpty());
    }

    void layoutPresets()
    {
        for (LayoutPreset preset : {LayoutPreset::Compact, LayoutPreset::Comfortable, LayoutPreset::Focus}) {
            QCOMPARE(layoutPresetFromConfigName(configName(preset)), preset);
            QCOMPARE(layoutPreset(comfortableRows(preset), focusLayout(preset)), preset);
        }
        QCOMPARE(layoutPresetFromConfigName(QStringLiteral("huge")), LayoutPreset::Compact);
        QVERIFY(!comfortableRows(LayoutPreset::Compact));
        QVERIFY(comfortableRows(LayoutPreset::Focus));
        QVERIFY(focusLayout(LayoutPreset::Focus));
        QCOMPARE(layoutPreset(false, true), LayoutPreset::Focus);
    }
};

QTEST_GUILESS_MAIN(SettingsSchemaTest)
#include "test_SettingsSchema.moc"
