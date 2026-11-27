/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QFlags>
#include <QHash>
#include <QList>
#include <QString>
#include <QVariant>
#include <optional>

namespace UiUtils {

/** @short What the settings are, where they are shown, what they default to and how they are inherited

The schema is the settings model of the application, kept apart from the features which read the settings and from
the dialog which edits them. Each setting has a key of the form "Group/Name", a group of the settings dialog, a
default, the layers at which it may be set and an optional integration it needs.

A setting is looked up from the most specific layer it allows: a folder, then its account, then the application, and
finally the default. resolve() says which layer the value came from, so that the dialog can explain inherited values.
The schema reads and writes nothing; UiUtils::SettingsOverrides keeps the account and folder layers in KConfig.
*/
namespace SettingsSchema {

/** @short The groups of the settings dialog, in the order in which they are shown */
enum class Group {
    General,
    Accounts,
    Appearance,
    Reading,
    Composition,
    Privacy,
    Notifications,
    Search,
    Integrations,
    Workspaces,
    Keyboard,
};

QList<Group> groups();
/** @short A stable name of @arg group, such as "reading", for storing which group was shown */
QString groupId(Group group);
std::optional<Group> groupFromId(const QString &id);

/** @short Where a value comes from, from the least to the most specific */
enum class Layer {
    Default,
    Application,
    Account,
    Folder,
};

enum class Scope {
    Application = 0x1,
    Account = 0x2,
    Folder = 0x4,
};
Q_DECLARE_FLAGS(Scopes, Scope)

/** @short Optional integrations which a setting needs before it does anything */
enum class Feature {
    None = 0,
    /** An application which opens calendar events */
    Calendar = 0x1,
    /** A folder for notes, as used by Marknote */
    Notes = 0x2,
    /** The Plasma Activities service */
    Activities = 0x4,
};
Q_DECLARE_FLAGS(Features, Feature)

struct Definition {
    QString key;
    Group group = Group::General;
    Scopes scopes = Scope::Application;
    QVariant defaultValue;
    Feature requirement = Feature::None;
    /** @short Key of the application value in the profile's QSettings, for settings which predate KConfig */
    QString legacyKey;
};

/** @short Every known setting, grouped and in the order in which the dialog shows them */
const QList<Definition> &definitions();
/** @short The setting called @arg key, or nullptr for an unknown key */
const Definition *definition(const QString &key);
QList<Definition> definitions(Group group);
/** @short Keys of the settings which may be set for an account or a folder */
QStringList keys(Scope scope);

/** @short The KConfig group and entry of @arg key: "Reading/AutoMarkRead" is entry AutoMarkRead of group Reading */
QString configGroup(const QString &key);
QString configEntry(const QString &key);

/** @short Whether @arg definition does something with the integrations in @arg available */
bool isAvailable(const Definition &definition, Features available);
/** @short Why @arg feature is not available, in a sentence for the settings dialog */
QString unavailableReason(Feature feature);

/** @short @arg value converted to the type of the setting's default, or an invalid QVariant when it does not fit */
QVariant coerce(const Definition &definition, const QVariant &value);

using Values = QHash<QString, QVariant>;

/** @short Values set at each layer; a key missing from a layer is inherited from the layer below */
struct Layers {
    Values application;
    Values account;
    Values folder;
};

struct Resolved {
    QVariant value;
    Layer layer = Layer::Default;

    bool operator==(const Resolved &other) const = default;
};

/** @short The value of @arg key and the layer it comes from

Layers which the setting does not allow, and values which do not fit its type, are skipped. An unknown key resolves
to an invalid value from the default layer.
*/
Resolved resolve(const QString &key, const Layers &layers);
/** @short The value of @arg key without the folder and account layers above @arg below; what an override replaces */
Resolved inherited(const QString &key, const Layers &layers, Layer below);

/** @short Where an inherited value comes from, such as "Inherited from account Work" */
QString explanation(Layer layer, const QString &accountName, const QString &folderName);

/** @short The layout presets: row density, and whether the side regions are hidden to focus on reading */
enum class LayoutPreset {
    Compact,
    Comfortable,
    Focus,
};
QString configName(LayoutPreset preset);
LayoutPreset layoutPresetFromConfigName(const QString &name);
/** @short The preset which matches a row density and the focus layout */
LayoutPreset layoutPreset(bool comfortableRows, bool focus);
bool comfortableRows(LayoutPreset preset);
bool focusLayout(LayoutPreset preset);

}

}

Q_DECLARE_OPERATORS_FOR_FLAGS(UiUtils::SettingsSchema::Scopes)
Q_DECLARE_OPERATORS_FOR_FLAGS(UiUtils::SettingsSchema::Features)
