/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QColor>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <optional>

namespace UiUtils {

/** @short The appearance options of the Theme settings group: what they are,
their defaults and how they combine

Every option follows Plasma by default. The entry names, choices and ranges are
the ones Plasma Opus uses in its Themes group, so that theme profiles can be
exchanged; options only a mail client has, such as the message list style and
the fonts of message headers and plain-text messages, are added.

Values are kept by entry name in the types the options use: bool, int, QString,
QStringList and QColor. Choices are stored by name, such as "AllPanes", rather
than by index, so that stored values stay readable and survive new choices.

The appearance in effect is layered, each layer replacing the options it holds:
 1. Plasma's own appearance, the defaults of the options;
 2. the global settings;
 3. the theme profile of the active workspace, which holds every option it
covers;
 4. the workspace's own overrides;
 5. the custom stylesheet in effect, from the global settings or the profile, on
top of all of them.
*/
namespace Theme {

/** @short The KConfig group of the global appearance options */
inline constexpr auto Group = "Theme";

enum class Type {
    Bool,
    Int,
    String,
    StringList,
    Color,
    Choice,
};

struct Option {
    QString entry;
    Type type = Type::Bool;
    QVariant defaultValue;
    int minimum = 0;
    int maximum = 0;
    /** The names of a Choice option, in the order Plasma Opus uses */
    QStringList choices;
};

using Values = QHash<QString, QVariant>;

/** @short Every appearance option, in the order the settings page shows them */
const QList<Option> &options();
/** @short The option stored as @arg entry, or nullptr */
const Option *option(const QString &entry);
QStringList entries();
/** @short The defaults of every option, which follow Plasma */
Values defaults();

/** @short The entries holding font descriptions; an empty description follows
 * Plasma's font */
QStringList fontEntries();
/** @short The entries a theme profile holds: all but folder colour rules and
 * the high-contrast safeguard */
QStringList profileEntries();
/** @short The entries a reset keeps, as they are user-made lists rather than
 * styling */
QStringList keptOnReset();
/** @short The entries a workspace can override: profile entries but the
 * stylesheet, plus folder colour rules */
QStringList overrideEntries();
/** @short The entries which hold the custom stylesheet */
QStringList styleSheetEntries();

/** @short @arg value in the type of @arg entry, or std::nullopt when it does
not fit the option

Ranges, choices and entry-specific rules are checked: colour schemes must be
plain names, accents a colour or "plasma", custom colours known roles, the tree
line palette one to eight colours, fonts parseable, the stylesheet must pass
StyleSheets::check() and folder colour rules must parse.
*/
std::optional<QVariant> validate(const QString &entry, const QVariant &value);
/** @short @arg values without unknown entries and invalid values, which are
 * described in @arg problems */
Values sanitized(const Values &values, QStringList *problems = nullptr);
/** @short The value of @arg entry in @arg values, or its default */
QVariant value(const Values &values, const QString &entry);

struct Layers {
    Values global;
    /** The theme profile of the active workspace; empty for none */
    Values profile;
    /** The workspace's overrides; entries outside overrideEntries() are ignored
     */
    Values overrides;
};

/** @short Every option's value after layering defaults, global settings, the
 * profile and the overrides */
Values resolve(const Layers &layers);

/** @short The entries suspended while a high-contrast colour scheme is in use
 */
QStringList suspendedEntries();
/** @short Whether @arg values ask for effects to be suspended now that @arg
 * highContrast is known */
bool effectsSuspended(const Values &values, bool highContrast);
/** @short @arg values with blur, translucency, tints, custom radii, tinted tabs
and the stylesheet turned off when effectsSuspended() */
Values withoutSuspendedEffects(const Values &values, bool highContrast);

/** @short @arg global reset to the defaults, keeping keptOnReset(); @arg
 * changed receives the reset entries */
Values reset(const Values &global, QStringList *changed = nullptr);

/** @short The colour scheme presets, besides installed KDE colour schemes */
inline constexpr auto LightPreset = "preset:light";
inline constexpr auto DarkPreset = "preset:dark";
/** @short The AccentColor value which follows Plasma's accent, as does an empty
 * value */
inline constexpr auto PlasmaAccent = "plasma";

/** @short The accent for @arg accentEntry: @arg plasmaAccent when it follows
 * Plasma or is not a colour */
QColor accentColor(const QString &accentEntry, const QColor &plasmaAccent);

/** @short The named spacing presets of SpacingScale */
enum class Spacing {
    Compact,
    Normal,
    Comfortable,
    Custom,
};
QList<Spacing> spacingPresets();
/** @short The preset a scale belongs to; any other scale is Custom */
Spacing spacingPreset(int scale);
/** @short The scale of @arg preset, in percent; Custom keeps @arg custom */
int spacingScale(Spacing preset, int custom = 100);
/** @short @arg pixels scaled by @arg scale percent, never below zero and never
 * turning a non-zero value into zero */
int scaled(int pixels, int scale);

/** @short The corner radius in device-independent pixels, or -1 to keep the
 * widget style's own */
int cornerRadius(const QString &choice, int custom);

/** @short How long a transition of @arg baseMs lasts with the Animations choice
and Plasma's animation speed

@arg plasmaFactor is Plasma's AnimationDurationFactor: 1 for normal speed, 0 for
instant. Reduced halves the duration; Off and a zero factor return 0.
*/
int animationDuration(int baseMs, const QString &choice, qreal plasmaFactor);

/** @short The colours of TreeLinePalette, skipping invalid entries */
QList<QColor> palette(const QStringList &entries);
/** @short The colour of a tree line at @arg depth, counted from 0 for top-level
items

Neutral uses @arg neutral, Single the TreeLineColor option, Depth cycles through
the palette and Folder uses
@arg folderColor when there is one.
*/
QColor treeLineColor(const Values &values, int depth, const QColor &folderColor, const QColor &neutral);

/** @short A named colour role of the colour editor */
struct Role {
    /** Stable key, such as "Base" or "Unread", as stored in CustomColors */
    QString key;
    QString name;
    /** Whether the role belongs to QPalette, rather than to the message list */
    bool palette = true;
};
QList<Role> roles();
/** @short Colours by role key */
using Colors = QHash<QString, QColor>;
/** @short The colours of CustomColors entries such as "Base=#202020"; unknown
 * roles and invalid colours are skipped */
Colors parseColors(const QStringList &entries);
QStringList formatColors(const Colors &colors);

struct ContrastIssue {
    QString foreground;
    QString background;
    qreal ratio = 1;
    qreal minimum = 1;

    bool operator==(const ContrastIssue &other) const = default;
};
/** @short Role pairs in @arg colors whose contrast is below the WCAG minimum
for text, or for indicators

Only pairs where both roles are in @arg colors are checked, and only pairs
touching @arg changed when it is not empty, so that the colours of the scheme
itself are not reported as the user's mistake.
*/
QList<ContrastIssue> contrastIssues(const Colors &colors, const QStringList &changed = {});
/** @short A sentence describing @arg issue for the colour editor */
QString describe(const ContrastIssue &issue);

} // namespace Theme

} // namespace UiUtils
