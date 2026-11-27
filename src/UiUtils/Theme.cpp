/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/Theme.h"

#include <QCoreApplication>
#include <QFont>
#include <QSet>
#include <algorithm>
#include <cmath>

#include "UiUtils/Color.h"
#include "UiUtils/FolderColors.h"
#include "UiUtils/StyleSheets.h"

namespace UiUtils {

namespace Theme {

namespace {

QString tr(const char *text, const char *comment = nullptr)
{
    return QCoreApplication::translate("UiUtils::Theme", text, comment);
}

constexpr qsizetype MaximumStringLength = 4096;
constexpr qsizetype MaximumListLength = 100;
constexpr qsizetype MaximumPaletteColors = 8;

QStringList defaultPalette()
{
    return {QStringLiteral("#3daee9"), QStringLiteral("#27ae60"), QStringLiteral("#f67400"),
            QStringLiteral("#da4453"), QStringLiteral("#9b59b6"), QStringLiteral("#fdbc4b")};
}

QList<Option> makeOptions()
{
    const auto choice = [](const char *entry, QStringList choices) {
        Option option{QLatin1String(entry), Type::Choice, choices.constFirst(), 0, 0, choices};
        return option;
    };
    const auto choiceWithDefault = [](const char *entry, QStringList choices, const char *defaultValue) {
        return Option{QLatin1String(entry), Type::Choice, QString::fromLatin1(defaultValue), 0, 0, choices};
    };
    const auto number = [](const char *entry, int defaultValue, int minimum, int maximum) {
        return Option{QLatin1String(entry), Type::Int, defaultValue, minimum, maximum, {}};
    };
    const auto flag = [](const char *entry, bool defaultValue) {
        return Option{QLatin1String(entry), Type::Bool, defaultValue, 0, 0, {}};
    };
    const auto text = [](const char *entry) { return Option{QLatin1String(entry), Type::String, QString(), 0, 0, {}}; };
    const auto list = [](const char *entry, QStringList defaultValue = {}) {
        return Option{QLatin1String(entry), Type::StringList, defaultValue, 0, 0, {}};
    };
    const auto color = [](const char *entry) {
        return Option{QLatin1String(entry), Type::Color, QColor(61, 174, 233), 0, 0, {}};
    };
    const QStringList followStyle{QStringLiteral("FollowStyle")};
    return {
        text("ColorScheme"),
        text("AccentColor"),
        list("CustomColors"),
        choice("PaneTint",
               {QStringLiteral("None"), QStringLiteral("AllPanes"), QStringLiteral("ActivePane"),
                QStringLiteral("InactivePane")}),
        color("PaneTintColor"),
        number("PaneTintStrength", 10, 1, 50),
        flag("BackgroundBlur", false),
        number("WindowOpacity", 100, 20, 100),
        number("PaneOpacity", 100, 20, 100),
        number("PanelOpacity", 100, 20, 100),
        choice("CornerRadius",
               followStyle +
                   QStringList{QStringLiteral("None"), QStringLiteral("Small"), QStringLiteral("Medium"),
                               QStringLiteral("Large"), QStringLiteral("Custom")}),
        number("CustomCornerRadius", 6, 0, 24),
        choice("Dividers", followStyle + QStringList{QStringLiteral("Thin"), QStringLiteral("Hidden")}),
        flag("RevealDividersOnHover", true),
        choice("TreeLines",
               followStyle + QStringList{QStringLiteral("Hidden"), QStringLiteral("Solid"), QStringLiteral("Dotted")}),
        choice(
            "TreeLineColors",
            {QStringLiteral("Neutral"), QStringLiteral("Single"), QStringLiteral("Depth"), QStringLiteral("Folder")}),
        color("TreeLineColor"),
        list("TreeLinePalette", defaultPalette()),
        choice("MessageListStyle", {QStringLiteral("Rows"), QStringLiteral("Cards")}),
        // The message list has always been striped; the folder tree has not
        flag("StripeRows", true),
        flag("StripeFolderTree", false),
        flag("HoverHighlight", true),
        choice("SelectionStyle",
               followStyle +
                   QStringList{QStringLiteral("Filled"), QStringLiteral("Outlined"), QStringLiteral("Rounded"),
                               QStringLiteral("AccentBar")}),
        choice("FolderColorStyle",
               {QStringLiteral("Icon"), QStringLiteral("Badge"), QStringLiteral("Name"), QStringLiteral("Hidden")}),
        list("FolderColorRules"),
        text("MessageListFont"),
        text("FolderTreeFont"),
        text("MessageHeaderFont"),
        text("PlainTextFont"),
        text("TabFont"),
        text("StatusBarFont"),
        number("SpacingScale", 100, 50, 200),
        choice("TabShape",
               followStyle + QStringList{QStringLiteral("Rounded"), QStringLiteral("Square"), QStringLiteral("Flat")}),
        choiceWithDefault("TabIndicator", {QStringLiteral("Line"), QStringLiteral("ThickLine"), QStringLiteral("None")},
                          "Line"),
        choice("TabColorStrength", {QStringLiteral("Text"), QStringLiteral("Tint"), QStringLiteral("Strong")}),
        choice("StatusBarStyle", followStyle + QStringList{QStringLiteral("Separated"), QStringLiteral("Shaded")}),
        choice("Animations", {QStringLiteral("FollowPlasma"), QStringLiteral("Reduced"), QStringLiteral("Off")}),
        flag("UseCustomStyleSheet", false),
        text("CustomStyleSheet"),
        flag("SuspendEffectsInHighContrast", true),
    };
}

bool hasControlCharacter(const QString &text)
{
    return std::any_of(text.cbegin(), text.cend(), [](QChar ch) {
        return ch.category() == QChar::Other_Control || ch == QChar::LineSeparator || ch == QChar::ParagraphSeparator;
    });
}

QStringList roleKeys()
{
    QStringList keys;
    for (const Role &role : roles())
        keys.append(role.key);
    return keys;
}

bool validCustomColor(const QString &entry)
{
    const qsizetype separator = entry.indexOf(QLatin1Char('='));
    if (separator <= 0)
        return false;
    return roleKeys().contains(entry.left(separator).trimmed()) &&
        QColor::fromString(entry.mid(separator + 1).trimmed()).isValid();
}

/** @short Rules beyond the option's type */
bool validValue(const QString &entry, const QVariant &value)
{
    if (entry == QLatin1String("ColorScheme")) {
        const QString scheme = value.toString();
        return scheme.size() <= 255 && !scheme.contains(QLatin1Char('/')) && !scheme.contains(QLatin1Char('\\')) &&
            !hasControlCharacter(scheme);
    }
    if (entry == QLatin1String("AccentColor")) {
        const QString accent = value.toString();
        return accent.isEmpty() || accent == QLatin1String(PlasmaAccent) || QColor::fromString(accent).isValid();
    }
    if (entry == QLatin1String("CustomColors")) {
        const QStringList colors = value.toStringList();
        return std::all_of(colors.cbegin(), colors.cend(), validCustomColor);
    }
    if (entry == QLatin1String("TreeLinePalette")) {
        const QStringList colors = value.toStringList();
        return !colors.isEmpty() && colors.size() <= MaximumPaletteColors &&
            std::all_of(colors.cbegin(), colors.cend(),
                        [](const QString &color) { return QColor::fromString(color.trimmed()).isValid(); });
    }
    if (fontEntries().contains(entry)) {
        const QString font = value.toString();
        QFont parsed;
        return font.isEmpty() || (!hasControlCharacter(font) && parsed.fromString(font));
    }
    if (entry == QLatin1String("CustomStyleSheet"))
        return StyleSheets::check(value.toString()).isEmpty();
    if (entry == QLatin1String("FolderColorRules")) {
        const QStringList rules = value.toStringList();
        return rules.size() <= FolderColors::MaximumRules &&
            std::all_of(rules.cbegin(), rules.cend(),
                        [](const QString &rule) { return FolderColors::parse(rule).has_value(); });
    }
    return true;
}

} // namespace

const QList<Option> &options()
{
    static const QList<Option> all = makeOptions();
    return all;
}

const Option *option(const QString &entry)
{
    for (const Option &candidate : options()) {
        if (candidate.entry == entry)
            return &candidate;
    }
    return nullptr;
}

QStringList entries()
{
    QStringList names;
    for (const Option &candidate : options())
        names.append(candidate.entry);
    return names;
}

Values defaults()
{
    Values values;
    for (const Option &candidate : options())
        values.insert(candidate.entry, candidate.defaultValue);
    return values;
}

QStringList fontEntries()
{
    return {QStringLiteral("MessageListFont"), QStringLiteral("FolderTreeFont"), QStringLiteral("MessageHeaderFont"),
            QStringLiteral("PlainTextFont"),   QStringLiteral("TabFont"),        QStringLiteral("StatusBarFont")};
}

QStringList profileEntries()
{
    QStringList names = entries();
    names.removeAll(QStringLiteral("FolderColorRules"));
    names.removeAll(QStringLiteral("SuspendEffectsInHighContrast"));
    return names;
}

QStringList keptOnReset()
{
    return {QStringLiteral("FolderColorRules")};
}

QStringList overrideEntries()
{
    QStringList names = profileEntries();
    for (const QString &entry : styleSheetEntries())
        names.removeAll(entry);
    names.append(QStringLiteral("FolderColorRules"));
    return names;
}

QStringList styleSheetEntries()
{
    return {QStringLiteral("UseCustomStyleSheet"), QStringLiteral("CustomStyleSheet")};
}

std::optional<QVariant> validate(const QString &entry, const QVariant &value)
{
    const Option *def = option(entry);
    if (!def || !value.isValid())
        return std::nullopt;
    std::optional<QVariant> result;
    switch (def->type) {
    case Type::Bool:
        if (value.typeId() == QMetaType::Bool) {
            result = value.toBool();
        } else if (value.typeId() == QMetaType::QString) {
            const QString text = value.toString().trimmed().toLower();
            if (text == QLatin1String("true") || text == QLatin1String("false"))
                result = text == QLatin1String("true");
        } else if (value.canConvert<int>()) {
            result = value.toInt() != 0;
        }
        break;
    case Type::Int: {
        bool ok = false;
        const double number = value.toDouble(&ok);
        if (ok && std::isfinite(number) && std::trunc(number) == number && number >= def->minimum &&
            number <= def->maximum)
            result = static_cast<int>(number);
        break;
    }
    case Type::String:
        if (value.canConvert<QString>()) {
            const QString text = value.toString();
            const qsizetype limit =
                entry == QLatin1String("CustomStyleSheet") ? StyleSheets::MaximumSize : MaximumStringLength;
            if (text.size() <= limit)
                result = text;
        }
        break;
    case Type::StringList:
        if (value.canConvert<QStringList>()) {
            const QStringList list = value.toStringList();
            if (list.size() <= MaximumListLength && std::all_of(list.cbegin(), list.cend(), [](const QString &item) {
                    return item.size() <= MaximumStringLength;
                }))
                result = list;
        }
        break;
    case Type::Color: {
        QColor color;
        if (value.typeId() == QMetaType::QColor)
            color = value.value<QColor>();
        else if (value.typeId() == QMetaType::QString && value.toString().size() <= 9)
            color = QColor::fromString(value.toString());
        if (color.isValid())
            result = QColor(color.rgb());
        break;
    }
    case Type::Choice: {
        const QString name = value.toString();
        if (def->choices.contains(name))
            result = name;
        break;
    }
    }
    if (result && !validValue(entry, *result))
        result.reset();
    return result;
}

Values sanitized(const Values &values, QStringList *problems)
{
    Values result;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (!option(it.key())) {
            if (problems)
                problems->append(tr("The appearance option %1 is not known and was skipped.").arg(it.key().left(100)));
            continue;
        }
        if (const auto checked = validate(it.key(), it.value()))
            result.insert(it.key(), *checked);
        else if (problems)
            problems->append(tr("The value of the appearance option %1 is not valid "
                                "and was skipped.")
                                 .arg(it.key()));
    }
    return result;
}

QVariant value(const Values &values, const QString &entry)
{
    const auto it = values.constFind(entry);
    if (it != values.constEnd()) {
        if (const auto checked = validate(entry, it.value()))
            return *checked;
    }
    const Option *def = option(entry);
    return def ? def->defaultValue : QVariant();
}

Values resolve(const Layers &layers)
{
    Values result = defaults();
    const auto apply = [&result](const Values &layer, const QStringList &allowed) {
        for (auto it = layer.constBegin(); it != layer.constEnd(); ++it) {
            if (!allowed.isEmpty() && !allowed.contains(it.key()))
                continue;
            if (const auto checked = validate(it.key(), it.value()))
                result.insert(it.key(), *checked);
        }
    };
    apply(layers.global, {});
    if (!layers.profile.isEmpty()) {
        // A profile holds every option it covers, so its missing entries are back
        // at their defaults
        const Values base = defaults();
        for (const QString &entry : profileEntries())
            result.insert(entry, base.value(entry));
        apply(layers.profile, profileEntries());
        // The stylesheet is the last layer: the profile's when it has one, else the
        // global one
        if (value(layers.profile, QStringLiteral("CustomStyleSheet")).toString().trimmed().isEmpty()) {
            for (const QString &entry : styleSheetEntries())
                result.insert(entry, value(layers.global, entry));
        }
    }
    apply(layers.overrides, overrideEntries());
    return result;
}

QStringList suspendedEntries()
{
    return {QStringLiteral("BackgroundBlur"),   QStringLiteral("WindowOpacity"),      QStringLiteral("PaneOpacity"),
            QStringLiteral("PanelOpacity"),     QStringLiteral("PaneTint"),           QStringLiteral("CornerRadius"),
            QStringLiteral("TabColorStrength"), QStringLiteral("UseCustomStyleSheet")};
}

bool effectsSuspended(const Values &values, bool highContrast)
{
    return highContrast && value(values, QStringLiteral("SuspendEffectsInHighContrast")).toBool();
}

Values withoutSuspendedEffects(const Values &values, bool highContrast)
{
    if (!effectsSuspended(values, highContrast))
        return values;
    Values result = values;
    const Values base = defaults();
    for (const QString &entry : suspendedEntries())
        result.insert(entry, base.value(entry));
    return result;
}

Values reset(const Values &global, QStringList *changed)
{
    Values result = defaults();
    for (const QString &entry : keptOnReset()) {
        if (global.contains(entry))
            result.insert(entry, value(global, entry));
    }
    if (changed) {
        for (const QString &entry : entries()) {
            if (!keptOnReset().contains(entry) && value(global, entry) != result.value(entry))
                changed->append(entry);
        }
    }
    return result;
}

QColor accentColor(const QString &accentEntry, const QColor &plasmaAccent)
{
    if (accentEntry.isEmpty() || accentEntry == QLatin1String(PlasmaAccent))
        return plasmaAccent;
    const QColor color = QColor::fromString(accentEntry);
    return color.isValid() ? QColor(color.rgb()) : plasmaAccent;
}

QList<Spacing> spacingPresets()
{
    return {Spacing::Compact, Spacing::Normal, Spacing::Comfortable, Spacing::Custom};
}

Spacing spacingPreset(int scale)
{
    for (Spacing preset : {Spacing::Compact, Spacing::Normal, Spacing::Comfortable}) {
        if (spacingScale(preset) == scale)
            return preset;
    }
    return Spacing::Custom;
}

int spacingScale(Spacing preset, int custom)
{
    switch (preset) {
    case Spacing::Compact:
        return 80;
    case Spacing::Normal:
        return 100;
    case Spacing::Comfortable:
        return 125;
    case Spacing::Custom:
        break;
    }
    return std::clamp(custom, 50, 200);
}

int scaled(int pixels, int scale)
{
    if (pixels <= 0)
        return 0;
    const int result = static_cast<int>(std::lround(pixels * std::clamp(scale, 50, 200) / 100.0));
    return std::max(1, result);
}

int cornerRadius(const QString &choice, int custom)
{
    if (choice == QLatin1String("None"))
        return 0;
    if (choice == QLatin1String("Small"))
        return 3;
    if (choice == QLatin1String("Medium"))
        return 6;
    if (choice == QLatin1String("Large"))
        return 10;
    if (choice == QLatin1String("Custom"))
        return std::clamp(custom, 0, 24);
    return -1;
}

int animationDuration(int baseMs, const QString &choice, qreal plasmaFactor)
{
    if (choice == QLatin1String("Off") || baseMs <= 0 || !(plasmaFactor > 0))
        return 0;
    const qreal factor = std::min<qreal>(plasmaFactor, 10);
    qreal duration = baseMs * factor;
    if (choice == QLatin1String("Reduced"))
        duration /= 2;
    return static_cast<int>(std::lround(duration));
}

QList<QColor> palette(const QStringList &entries)
{
    QList<QColor> colors;
    for (const QString &entry : entries) {
        const QColor color = QColor::fromString(entry.trimmed());
        if (color.isValid())
            colors.append(QColor(color.rgb()));
    }
    return colors;
}

QColor treeLineColor(const Values &values, int depth, const QColor &folderColor, const QColor &neutral)
{
    const QString mode = value(values, QStringLiteral("TreeLineColors")).toString();
    if (mode == QLatin1String("Single"))
        return value(values, QStringLiteral("TreeLineColor")).value<QColor>();
    if (mode == QLatin1String("Depth")) {
        const QList<QColor> colors = palette(value(values, QStringLiteral("TreeLinePalette")).toStringList());
        if (colors.isEmpty())
            return neutral;
        return colors.at(std::max(0, depth) % colors.size());
    }
    if (mode == QLatin1String("Folder") && folderColor.isValid())
        return folderColor;
    return neutral;
}

QList<Role> roles()
{
    return {
        {QStringLiteral("Window"), tr("Window background"), true},
        {QStringLiteral("WindowText"), tr("Window text"), true},
        {QStringLiteral("Base"), tr("View background"), true},
        {QStringLiteral("AlternateBase"), tr("Alternate view background"), true},
        {QStringLiteral("Text"), tr("View text"), true},
        {QStringLiteral("PlaceholderText"), tr("Placeholder text"), true},
        {QStringLiteral("Button"), tr("Button background"), true},
        {QStringLiteral("ButtonText"), tr("Button text"), true},
        {QStringLiteral("Highlight"), tr("Selection background"), true},
        {QStringLiteral("HighlightedText"), tr("Selection text"), true},
        {QStringLiteral("Accent"), tr("Accent and focus"), true},
        {QStringLiteral("Link"), tr("Link"), true},
        {QStringLiteral("LinkVisited"), tr("Visited link"), true},
        {QStringLiteral("ToolTipBase"), tr("Tooltip background"), true},
        {QStringLiteral("ToolTipText"), tr("Tooltip text"), true},
        {QStringLiteral("Unread"), tr("Unread messages"), false},
        {QStringLiteral("Flagged"), tr("Flagged messages"), false},
    };
}

Colors parseColors(const QStringList &entries)
{
    const QStringList known = roleKeys();
    Colors colors;
    for (const QString &entry : entries) {
        const qsizetype separator = entry.indexOf(QLatin1Char('='));
        if (separator <= 0)
            continue;
        const QString key = entry.left(separator).trimmed();
        const QColor color = QColor::fromString(entry.mid(separator + 1).trimmed());
        if (known.contains(key) && color.isValid())
            colors.insert(key, QColor(color.rgb()));
    }
    return colors;
}

QStringList formatColors(const Colors &colors)
{
    QStringList result;
    for (const Role &role : roles()) {
        const auto it = colors.constFind(role.key);
        if (it != colors.constEnd() && it->isValid())
            result.append(role.key + QLatin1Char('=') + it->name(QColor::HexRgb));
    }
    return result;
}

QList<ContrastIssue> contrastIssues(const Colors &colors, const QStringList &changed)
{
    struct Pair {
        const char *foreground;
        const char *background;
        qreal minimum;
    };
    // 4.5:1 for text, 3:1 for placeholder text and for indicators, as WCAG 2 asks
    static const Pair pairs[] = {
        {"Text", "Base", 4.5},
        {"Text", "AlternateBase", 4.5},
        {"WindowText", "Window", 4.5},
        {"ButtonText", "Button", 4.5},
        {"HighlightedText", "Highlight", 4.5},
        {"ToolTipText", "ToolTipBase", 4.5},
        {"Link", "Base", 4.5},
        {"LinkVisited", "Base", 4.5},
        {"Unread", "Base", 4.5},
        {"PlaceholderText", "Base", 3.0},
        {"Flagged", "Base", 3.0},
        {"Accent", "Window", 3.0},
        {"Accent", "Base", 3.0},
    };
    QList<ContrastIssue> issues;
    for (const Pair &pair : pairs) {
        const QString foreground = QLatin1String(pair.foreground);
        const QString background = QLatin1String(pair.background);
        if (!colors.contains(foreground) || !colors.contains(background))
            continue;
        if (!changed.isEmpty() && !changed.contains(foreground) && !changed.contains(background))
            continue;
        const qreal ratio = contrastRatio(colors.value(foreground), colors.value(background));
        if (ratio + 1e-9 < pair.minimum)
            issues.append({foreground, background, ratio, pair.minimum});
    }
    return issues;
}

QString describe(const ContrastIssue &issue)
{
    QString foreground = issue.foreground;
    QString background = issue.background;
    for (const Role &role : roles()) {
        if (role.key == issue.foreground)
            foreground = role.name;
        if (role.key == issue.background)
            background = role.name;
    }
    return tr("%1 on %2 has a contrast of %3:1; at least %4:1 keeps it readable.")
        .arg(foreground, background, QString::number(issue.ratio, 'f', 1), QString::number(issue.minimum, 'f', 1));
}

} // namespace Theme

} // namespace UiUtils
