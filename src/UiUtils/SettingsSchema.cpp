/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/SettingsSchema.h"

#include <QCoreApplication>

#include "Common/SettingsNames.h"

namespace UiUtils {

namespace SettingsSchema {

namespace {

QString tr(const char *text, const char *comment = nullptr)
{
    return QCoreApplication::translate("UiUtils::SettingsSchema", text, comment);
}

constexpr std::pair<Group, const char *> groupNames[] = {
    {Group::General, "general"},
    {Group::Accounts, "accounts"},
    {Group::Appearance, "appearance"},
    {Group::Reading, "reading"},
    {Group::Composition, "composition"},
    {Group::Privacy, "privacy"},
    {Group::Notifications, "notifications"},
    {Group::Search, "search"},
    {Group::Integrations, "integrations"},
    {Group::Workspaces, "workspaces"},
    {Group::Keyboard, "keyboard"},
};

constexpr std::pair<LayoutPreset, const char *> presetNames[] = {
    {LayoutPreset::Compact, "compact"},
    {LayoutPreset::Comfortable, "comfortable"},
    {LayoutPreset::Focus, "focus"},
};

QList<Definition> makeDefinitions()
{
    const Scopes app = Scope::Application;
    const Scopes account = Scope::Application | Scope::Account;
    const Scopes folder = Scope::Application | Scope::Account | Scope::Folder;
    return {
        {QStringLiteral("Shell/StartFolder"), Group::Accounts, account, QString(), Feature::None, {}},
        {QStringLiteral("Appearance/RowDensity"), Group::Appearance, app, QStringLiteral("compact"), Feature::None, {}},
        {QStringLiteral("Appearance/FocusLayout"), Group::Appearance, app, false, Feature::None, {}},
        {QStringLiteral("Tabs/Enabled"), Group::Appearance, app, false, Feature::None, {}},
        {QStringLiteral("Tabs/Region"), Group::Appearance, app, QStringLiteral("header"), Feature::None, {}},
        {QStringLiteral("Tabs/Orientation"), Group::Appearance, app, QStringLiteral("horizontal"), Feature::None, {}},
        {QStringLiteral("Reading/AutoMarkRead"), Group::Reading, folder, true, Feature::None,
         Common::SettingsNames::autoMarkReadEnabled},
        {QStringLiteral("Reading/MarkReadDelay"), Group::Reading, folder, 0, Feature::None,
         Common::SettingsNames::autoMarkReadSeconds},
        {QStringLiteral("Reading/PreferPlainText"), Group::Reading, folder, true, Feature::None,
         Common::SettingsNames::guiPreferPlaintextRendering},
        {QStringLiteral("Search/DefaultPlaces"), Group::Search, app, QStringLiteral("current"), Feature::None, {}},
        {QStringLiteral("Search/UseIndex"), Group::Search, app, QStringLiteral("automatic"), Feature::None, {}},
        {QStringLiteral("Search/HistoryLimit"), Group::Search, app, 20, Feature::None, {}},
        {QStringLiteral("Integrations/MailToCalendar"), Group::Integrations, app, true, Feature::Calendar, {}},
        {QStringLiteral("Integrations/MailToNotes"), Group::Integrations, app, true, Feature::Notes, {}},
        {QStringLiteral("Integrations/NotesFolder"), Group::Integrations, app, QString(), Feature::None, {}},
        {QStringLiteral("Integrations/WorkContext"), Group::Integrations, app, true, Feature::None, {}},
        {QStringLiteral("Workspaces/FollowActivity"), Group::Workspaces, app, true, Feature::Activities, {}},
        {QStringLiteral("Keyboard/VimMode"), Group::Keyboard, app, false, Feature::None, {}},
        {QStringLiteral("Keyboard/VimTimeout"), Group::Keyboard, app, 1000, Feature::None, {}},
        {QStringLiteral("Keyboard/VimHints"), Group::Keyboard, app, true, Feature::None, {}},
        {QStringLiteral("Keyboard/VimHintDelay"), Group::Keyboard, app, 400, Feature::None, {}},
        {QStringLiteral("Keyboard/FollowSharedKeymap"), Group::Keyboard, app, false, Feature::None, {}},
    };
}

Scope scopeOf(Layer layer)
{
    switch (layer) {
    case Layer::Folder:
        return Scope::Folder;
    case Layer::Account:
        return Scope::Account;
    case Layer::Application:
    case Layer::Default:
        break;
    }
    return Scope::Application;
}

const Values *valuesOf(const Layers &layers, Layer layer)
{
    switch (layer) {
    case Layer::Folder:
        return &layers.folder;
    case Layer::Account:
        return &layers.account;
    case Layer::Application:
        return &layers.application;
    case Layer::Default:
        break;
    }
    return nullptr;
}

Resolved resolveBelow(const QString &key, const Layers &layers, Layer top)
{
    const Definition *def = definition(key);
    if (!def)
        return {};
    for (Layer layer : {Layer::Folder, Layer::Account, Layer::Application}) {
        if (layer > top || !def->scopes.testFlag(scopeOf(layer)))
            continue;
        const Values *values = valuesOf(layers, layer);
        const auto it = values->constFind(key);
        if (it == values->constEnd())
            continue;
        const QVariant value = coerce(*def, it.value());
        if (value.isValid())
            return {value, layer};
    }
    return {def->defaultValue, Layer::Default};
}

}

QList<Group> groups()
{
    QList<Group> result;
    for (const auto &[group, name] : groupNames)
        result.append(group);
    return result;
}

QString groupId(Group group)
{
    for (const auto &[value, name] : groupNames) {
        if (value == group)
            return QLatin1String(name);
    }
    return QStringLiteral("general");
}

std::optional<Group> groupFromId(const QString &id)
{
    for (const auto &[value, name] : groupNames) {
        if (id == QLatin1String(name))
            return value;
    }
    return std::nullopt;
}

const QList<Definition> &definitions()
{
    static const QList<Definition> all = makeDefinitions();
    return all;
}

const Definition *definition(const QString &key)
{
    for (const Definition &def : definitions()) {
        if (def.key == key)
            return &def;
    }
    return nullptr;
}

QList<Definition> definitions(Group group)
{
    QList<Definition> result;
    for (const Definition &def : definitions()) {
        if (def.group == group)
            result.append(def);
    }
    return result;
}

QStringList keys(Scope scope)
{
    QStringList result;
    for (const Definition &def : definitions()) {
        if (def.scopes.testFlag(scope))
            result.append(def.key);
    }
    return result;
}

QString configGroup(const QString &key)
{
    return key.section(QLatin1Char('/'), 0, 0);
}

QString configEntry(const QString &key)
{
    return key.section(QLatin1Char('/'), 1);
}

bool isAvailable(const Definition &definition, Features available)
{
    return definition.requirement == Feature::None || available.testFlag(definition.requirement);
}

QString unavailableReason(Feature feature)
{
    switch (feature) {
    case Feature::Calendar:
        return tr("No application is set up to open calendar events, so messages cannot be turned into events.");
    case Feature::Notes:
        return tr("Choose a folder for notes, such as the one Marknote uses, to save messages as notes.");
    case Feature::Activities:
        return tr("Plasma Activities is not running, so workspaces cannot follow the current activity.");
    case Feature::None:
        break;
    }
    return QString();
}

QVariant coerce(const Definition &definition, const QVariant &value)
{
    if (!value.isValid())
        return {};
    const QMetaType type = definition.defaultValue.metaType();
    if (value.metaType() == type)
        return value;
    if (type.id() == QMetaType::Bool && value.metaType().id() == QMetaType::QString) {
        // Stored as text, as KConfig and QSettings do
        const QString text = value.toString().trimmed().toLower();
        if (text == QLatin1String("true") || text == QLatin1String("1"))
            return true;
        if (text == QLatin1String("false") || text == QLatin1String("0"))
            return false;
        return {};
    }
    if (type.id() == QMetaType::Int) {
        bool ok = false;
        const int number = value.toInt(&ok);
        return ok ? QVariant(number) : QVariant();
    }
    QVariant converted = value;
    if (!converted.convert(type))
        return {};
    return converted;
}

Resolved resolve(const QString &key, const Layers &layers)
{
    return resolveBelow(key, layers, Layer::Folder);
}

Resolved inherited(const QString &key, const Layers &layers, Layer below)
{
    if (below == Layer::Default)
        return {};
    return resolveBelow(key, layers, static_cast<Layer>(static_cast<int>(below) - 1));
}

QString explanation(Layer layer, const QString &accountName, const QString &folderName)
{
    switch (layer) {
    case Layer::Default:
        return tr("Uses the default setting");
    case Layer::Application:
        return tr("Inherited from the application settings");
    case Layer::Account:
        return tr("Inherited from account %1").arg(accountName);
    case Layer::Folder:
        return tr("Set for folder %1").arg(folderName);
    }
    return QString();
}

QString configName(LayoutPreset preset)
{
    for (const auto &[value, name] : presetNames) {
        if (value == preset)
            return QLatin1String(name);
    }
    return QStringLiteral("compact");
}

LayoutPreset layoutPresetFromConfigName(const QString &name)
{
    for (const auto &[value, known] : presetNames) {
        if (name == QLatin1String(known))
            return value;
    }
    return LayoutPreset::Compact;
}

LayoutPreset layoutPreset(bool comfortableRows, bool focus)
{
    if (focus)
        return LayoutPreset::Focus;
    return comfortableRows ? LayoutPreset::Comfortable : LayoutPreset::Compact;
}

bool comfortableRows(LayoutPreset preset)
{
    return preset != LayoutPreset::Compact;
}

bool focusLayout(LayoutPreset preset)
{
    return preset == LayoutPreset::Focus;
}

}

}
