/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/AppearanceManager.h"

#include <KColorScheme>
#include <KConfigGroup>
#include <KSharedConfig>
#include <KWindowEffects>
#include <QApplication>
#include <QLayout>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QStandardPaths>
#include <QStyleHints>
#include <QWidget>
#include <QWindow>
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
#include <QAccessibilityHints>
#endif

#include "Gui/AppearanceStyle.h"
#include "UiUtils/Color.h"
#include "UiUtils/SettingsStore.h"
#include "UiUtils/StyleSheets.h"
#include "UiUtils/ThemeProfiles.h"
#include "UiUtils/Workspaces.h"

namespace Gui {

using UiUtils::Theme::Values;
namespace Theme = UiUtils::Theme;

namespace {

AppearanceManager *s_instance = nullptr;

QColor mix(const QColor &a, const QColor &b, qreal ratio)
{
    return QColor::fromRgbF(a.redF() * (1 - ratio) + b.redF() * ratio, a.greenF() * (1 - ratio) + b.greenF() * ratio,
                            a.blueF() * (1 - ratio) + b.blueF() * ratio, a.alphaF());
}

/** @short A plain palette for the presets when Breeze's schemes are not
 * installed */
QPalette fallbackPalette(bool dark)
{
    const QColor window = dark ? QColor(0x20, 0x23, 0x26) : QColor(0xef, 0xf0, 0xf1);
    const QColor base = dark ? QColor(0x14, 0x16, 0x18) : QColor(0xff, 0xff, 0xff);
    const QColor text = dark ? QColor(0xfc, 0xfc, 0xfc) : QColor(0x23, 0x26, 0x29);
    const QColor highlight(0x3d, 0xae, 0xe9);
    QPalette palette(text, window, window.lighter(130), window.darker(130), window.darker(115), text, base);
    palette.setColor(QPalette::AlternateBase, mix(base, window, 0.5));
    palette.setColor(QPalette::Button, dark ? QColor(0x29, 0x2c, 0x30) : QColor(0xfc, 0xfc, 0xfc));
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::Accent, highlight);
    palette.setColor(QPalette::Link, dark ? QColor(0x1d, 0x99, 0xf3) : QColor(0x29, 0x80, 0xb9));
    palette.setColor(QPalette::LinkVisited, dark ? QColor(0x9b, 0x59, 0xb6) : QColor(0x9b, 0x59, 0xb6));
    palette.setColor(QPalette::PlaceholderText, mix(text, base, 0.4));
    palette.setColor(QPalette::ToolTipBase, window);
    palette.setColor(QPalette::ToolTipText, text);
    for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, mix(text, window, 0.6));
    return palette;
}

QPalette::ColorRole paletteRole(const QString &key)
{
    static const QHash<QString, QPalette::ColorRole> roles{
        {QStringLiteral("Window"), QPalette::Window},
        {QStringLiteral("WindowText"), QPalette::WindowText},
        {QStringLiteral("Base"), QPalette::Base},
        {QStringLiteral("AlternateBase"), QPalette::AlternateBase},
        {QStringLiteral("Text"), QPalette::Text},
        {QStringLiteral("PlaceholderText"), QPalette::PlaceholderText},
        {QStringLiteral("Button"), QPalette::Button},
        {QStringLiteral("ButtonText"), QPalette::ButtonText},
        {QStringLiteral("Highlight"), QPalette::Highlight},
        {QStringLiteral("HighlightedText"), QPalette::HighlightedText},
        {QStringLiteral("Accent"), QPalette::Accent},
        {QStringLiteral("Link"), QPalette::Link},
        {QStringLiteral("LinkVisited"), QPalette::LinkVisited},
        {QStringLiteral("ToolTipBase"), QPalette::ToolTipBase},
        {QStringLiteral("ToolTipText"), QPalette::ToolTipText},
    };
    return roles.value(key, QPalette::NColorRoles);
}

bool atDefaults(const Values &values)
{
    const Values defaults = Theme::defaults();
    for (auto it = defaults.constBegin(); it != defaults.constEnd(); ++it) {
        if (Theme::value(values, it.key()) != it.value())
            return false;
    }
    return true;
}

bool paletteOptionsAtDefaults(const Values &values)
{
    for (const char *entry : {"ColorScheme", "AccentColor", "CustomColors"}) {
        const QString name = QLatin1String(entry);
        if (Theme::value(values, name) != Theme::option(name)->defaultValue)
            return false;
    }
    return true;
}

QString plasmaSchemeName()
{
    return KConfigGroup(KSharedConfig::openConfig(QStringLiteral("kdeglobals")), QStringLiteral("General"))
        .readEntry("ColorScheme", QString());
}

} // namespace

AppearanceManager *AppearanceManager::instance()
{
    if (!s_instance)
        s_instance = new AppearanceManager(qApp);
    return s_instance;
}

AppearanceManager::AppearanceManager(QObject *parent)
    : QObject(parent)
    , m_values(Theme::defaults())
    , m_systemPalette(QGuiApplication::palette())
{
    qApp->installEventFilter(this);
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        QWidget *pane = nullptr;
        for (QWidget *candidate = now; candidate && !pane; candidate = candidate->parentWidget()) {
            for (const QPointer<QWidget> &registered : std::as_const(m_panes)) {
                if (registered == candidate)
                    pane = candidate;
            }
        }
        if (pane && pane != m_activePane) {
            m_activePane = pane;
            updatePaneTint();
        }
    });
}

void AppearanceManager::load(const KConfigBase &config, const QString &activityId, const QString &accountId)
{
    QStringList problems;
    const Values global = UiUtils::SettingsStore::theme(config);
    const QList<UiUtils::ThemeProfiles::Profile> profiles = UiUtils::SettingsStore::themeProfiles(config, &problems);
    const QList<UiUtils::Workspaces::Workspace> workspaces = UiUtils::SettingsStore::workspaces(config);
    const UiUtils::Workspaces::Workspace *workspace = UiUtils::Workspaces::active(workspaces, activityId, accountId);
    const UiUtils::Workspaces::ResolvedAppearance resolved =
        UiUtils::Workspaces::appearance(workspace, profiles, global, m_workspaceAppearanceDisabled);
    problems += resolved.problems;

    UiUtils::FolderColors::Colors colors = UiUtils::SettingsStore::folderColors(config);
    const Values values = Theme::resolve(resolved.layers);
    // Workspace rules replace the global ones, as any other option does
    colors.rules =
        UiUtils::FolderColors::rules(Theme::value(values, QStringLiteral("FolderColorRules")).toStringList());
    m_problems = problems;
    apply(values, colors);
}

void AppearanceManager::apply(const Values &values, const UiUtils::FolderColors::Colors &colors)
{
    const QString scheme = Theme::value(values, QStringLiteral("ColorScheme")).toString();
    const QString schemeName = scheme.isEmpty() ? plasmaSchemeName() : scheme;
    m_highContrast = schemeName.contains(QLatin1String("contrast"), Qt::CaseInsensitive);
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    if (scheme.isEmpty() &&
        QGuiApplication::styleHints()->accessibility()->contrastPreference() == Qt::ContrastPreference::HighContrast)
        m_highContrast = true;
#endif
    m_suspended = Theme::effectsSuspended(values, m_highContrast);
    const bool wasDefault = atDefaults(m_values);
    m_values = Theme::withoutSuspendedEffects(values, m_highContrast);
    m_colors = colors;
    m_customColors = Theme::parseColors(Theme::value(m_values, QStringLiteral("CustomColors")).toStringList());

    m_applying = true;
    AppearanceStyle::ensureInstalled();
    applyPalette();
    applyFonts();
    applyStyleSheet();
    for (const QPointer<QWidget> &window : std::as_const(m_windows)) {
        if (window)
            applyWindow(window);
    }
    updatePaneTint();
    // With Plasma's look before and after there is nothing to lay out again
    if (!wasDefault || !atDefaults(m_values))
        repolish();
    m_applying = false;
    emit changed();
}

void AppearanceManager::setWorkspaceAppearanceDisabled(bool disabled)
{
    m_workspaceAppearanceDisabled = disabled;
}

bool AppearanceManager::workspaceAppearanceDisabled() const
{
    return m_workspaceAppearanceDisabled;
}

const Values &AppearanceManager::values() const
{
    return m_values;
}

QVariant AppearanceManager::value(const QString &entry) const
{
    return Theme::value(m_values, entry);
}

QStringList AppearanceManager::problems() const
{
    return m_problems;
}

bool AppearanceManager::highContrast() const
{
    return m_highContrast;
}

bool AppearanceManager::effectsSuspended() const
{
    return m_suspended;
}

bool AppearanceManager::blurAvailable()
{
    return KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind);
}

QColor AppearanceManager::accent() const
{
    return Theme::accentColor(value(QStringLiteral("AccentColor")).toString(),
                              m_systemPalette.color(QPalette::Highlight));
}

QColor AppearanceManager::unreadColor() const
{
    return m_customColors.value(QStringLiteral("Unread"));
}

QColor AppearanceManager::flaggedColor() const
{
    return m_customColors.value(QStringLiteral("Flagged"));
}

const UiUtils::FolderColors::Colors &AppearanceManager::folderColors() const
{
    return m_colors;
}

QColor AppearanceManager::accountColor(const QString &accountId) const
{
    return UiUtils::FolderColors::accountColor(m_colors, accountId);
}

QColor AppearanceManager::folderColor(const QString &accountId, const QString &folder, QChar separator) const
{
    return UiUtils::FolderColors::folderColor(m_colors, accountId, folder, separator);
}

int AppearanceManager::cornerRadius() const
{
    return Theme::cornerRadius(value(QStringLiteral("CornerRadius")).toString(),
                               value(QStringLiteral("CustomCornerRadius")).toInt());
}

int AppearanceManager::spacingScale() const
{
    return value(QStringLiteral("SpacingScale")).toInt();
}

int AppearanceManager::animationDuration(int baseMs) const
{
    return Theme::animationDuration(baseMs, value(QStringLiteral("Animations")).toString(), plasmaAnimationFactor());
}

QFont AppearanceManager::plainTextFont() const
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    const QString chosen = value(QStringLiteral("PlainTextFont")).toString();
    if (!chosen.isEmpty())
        font.fromString(chosen);
    return font;
}

qreal AppearanceManager::plasmaAnimationFactor()
{
    return KConfigGroup(KSharedConfig::openConfig(QStringLiteral("kdeglobals")), QStringLiteral("KDE"))
        .readEntry("AnimationDurationFactor", 1.0);
}

QList<std::pair<QString, QString>> AppearanceManager::installedColorSchemes()
{
    QList<std::pair<QString, QString>> schemes;
    QStringList seen;
    const QStringList directories = QStandardPaths::locateAll(
        QStandardPaths::GenericDataLocation, QStringLiteral("color-schemes"), QStandardPaths::LocateDirectory);
    for (const QString &directory : directories) {
        const QFileInfoList files =
            QDir(directory).entryInfoList({QStringLiteral("*.colors")}, QDir::Files, QDir::Name);
        for (const QFileInfo &file : files) {
            const QString name = file.completeBaseName();
            if (seen.contains(name))
                continue;
            seen << name;
            const auto config = KSharedConfig::openConfig(file.absoluteFilePath(), KConfig::SimpleConfig);
            const QString display = KConfigGroup(config, QStringLiteral("General")).readEntry("Name", name);
            schemes.append({name, display});
        }
    }
    return schemes;
}

QString AppearanceManager::colorSchemePath(const QString &name)
{
    if (name.isEmpty() || name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\')))
        return QString();
    return QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                  QStringLiteral("color-schemes/%1.colors").arg(name));
}

QPalette AppearanceManager::schemePalette(const QString &scheme, bool *found)
{
    if (found)
        *found = true;
    if (scheme.isEmpty())
        return KColorScheme::createApplicationPalette(KSharedConfig::openConfig(QStringLiteral("kdeglobals")));
    const bool light = scheme == QLatin1String(Theme::LightPreset);
    const bool dark = scheme == QLatin1String(Theme::DarkPreset);
    const QString file = colorSchemePath(light      ? QStringLiteral("BreezeLight")
                                             : dark ? QStringLiteral("BreezeDark")
                                                    : scheme);
    if (!file.isEmpty())
        return KColorScheme::createApplicationPalette(KSharedConfig::openConfig(file, KConfig::SimpleConfig));
    if (light || dark)
        return fallbackPalette(dark);
    if (found)
        *found = false;
    return KColorScheme::createApplicationPalette(KSharedConfig::openConfig(QStringLiteral("kdeglobals")));
}

void AppearanceManager::applyPalette()
{
    if (paletteOptionsAtDefaults(m_values)) {
        if (m_paletteChanged) {
            QApplication::setPalette(m_systemPalette);
            m_paletteChanged = false;
        }
        return;
    }
    if (!m_paletteChanged)
        m_systemPalette = QGuiApplication::palette();

    const QString scheme = value(QStringLiteral("ColorScheme")).toString();
    bool found = true;
    QPalette palette = scheme.isEmpty() ? m_systemPalette : schemePalette(scheme, &found);
    if (!found) {
        m_problems.append(tr("The colour scheme %1 is not installed; Plasma's "
                             "colours are used until it is back.")
                              .arg(scheme));
    }
    const QString accentEntry = value(QStringLiteral("AccentColor")).toString();
    if (!accentEntry.isEmpty() && accentEntry != QLatin1String(Theme::PlasmaAccent)) {
        const QColor accent = Theme::accentColor(accentEntry, palette.color(QPalette::Highlight));
        for (QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive}) {
            palette.setColor(group, QPalette::Highlight, accent);
            palette.setColor(group, QPalette::Accent, accent);
            palette.setColor(group, QPalette::HighlightedText, UiUtils::readableTextColor(accent));
        }
    }
    for (auto it = m_customColors.constBegin(); it != m_customColors.constEnd(); ++it) {
        const QPalette::ColorRole role = paletteRole(it.key());
        if (role == QPalette::NColorRoles)
            continue;
        palette.setColor(QPalette::Active, role, it.value());
        palette.setColor(QPalette::Inactive, role, it.value());
    }
    QApplication::setPalette(palette);
    m_paletteChanged = true;
}

void AppearanceManager::registerFont(QWidget *widget, FontTarget target)
{
    if (!widget || m_fontTargets.contains(widget))
        return;
    m_fontTargets.insert(widget, target);
    connect(widget, &QObject::destroyed, this, [this, widget]() { m_fontTargets.remove(widget); });
    applyFonts();
}

void AppearanceManager::applyFonts()
{
    static const QHash<FontTarget, QString> entries{
        {FontTarget::MessageList, QStringLiteral("MessageListFont")},
        {FontTarget::FolderTree, QStringLiteral("FolderTreeFont")},
        {FontTarget::MessageHeaders, QStringLiteral("MessageHeaderFont")},
        {FontTarget::Tabs, QStringLiteral("TabFont")},
        {FontTarget::StatusBar, QStringLiteral("StatusBarFont")},
    };
    for (auto it = m_fontTargets.constBegin(); it != m_fontTargets.constEnd(); ++it) {
        const QString description = value(entries.value(it.value())).toString();
        QFont font;
        // A default font resolves nothing, so the widget follows its parent and
        // Plasma again
        if (!description.isEmpty())
            font.fromString(description);
        if (it.key()->font().resolveMask() != font.resolveMask() ||
            (!description.isEmpty() && it.key()->font() != font))
            it.key()->setFont(font);
    }
}

void AppearanceManager::applyStyleSheet()
{
    const bool use = value(QStringLiteral("UseCustomStyleSheet")).toBool();
    const QString sheet = use ? value(QStringLiteral("CustomStyleSheet")).toString() : QString();
    if (sheet.isEmpty() && !m_styleSheetChanged)
        return;
    if (qApp->styleSheet() != sheet)
        qApp->setStyleSheet(sheet);
    m_styleSheetChanged = !sheet.isEmpty();
}

void AppearanceManager::registerWindow(QWidget *window)
{
    if (!window)
        return;
    for (const QPointer<QWidget> &known : std::as_const(m_windows)) {
        if (known == window)
            return;
    }
    m_windows.append(window);
    // Translucency must be asked for before the window is created; later changes
    // apply when it next opens
    const bool translucent =
        value(QStringLiteral("WindowOpacity")).toInt() < 100 || value(QStringLiteral("BackgroundBlur")).toBool();
    if (translucent && !window->testAttribute(Qt::WA_WState_Created))
        window->setAttribute(Qt::WA_TranslucentBackground);
    window->installEventFilter(this);
    applyWindow(window);
}

void AppearanceManager::applyWindow(QWidget *window)
{
    const int opacity = value(QStringLiteral("WindowOpacity")).toInt();
    const bool blur = value(QStringLiteral("BackgroundBlur")).toBool() && blurAvailable();
    const bool translucent = window->testAttribute(Qt::WA_TranslucentBackground);
    QPalette palette = window->palette();
    QColor background = QGuiApplication::palette().color(QPalette::Window);
    background.setAlpha(translucent ? qRound(255 * opacity / 100.0) : 255);
    if (palette.color(QPalette::Window) != background) {
        palette.setColor(QPalette::Window, background);
        window->setPalette(palette);
    }
    if (QWindow *handle = window->windowHandle())
        KWindowEffects::enableBlurBehind(handle, translucent && blur);
}

void AppearanceManager::registerPane(QWidget *pane)
{
    if (!pane)
        return;
    m_panes.append(pane);
    updatePaneTint();
}

QPalette AppearanceManager::paneBasePalette(QWidget *pane) const
{
    QPalette palette = pane->parentWidget() ? pane->parentWidget()->palette() : QGuiApplication::palette();
    const int opacity = value(QStringLiteral("PaneOpacity")).toInt();
    if (opacity < 100 && pane->window()->testAttribute(Qt::WA_TranslucentBackground)) {
        QColor base = palette.color(QPalette::Base);
        base.setAlpha(qRound(255 * opacity / 100.0));
        palette.setColor(QPalette::Base, base);
    }
    return palette;
}

void AppearanceManager::updatePaneTint()
{
    const QString tint = value(QStringLiteral("PaneTint")).toString();
    const QColor color = value(QStringLiteral("PaneTintColor")).value<QColor>();
    const qreal strength = value(QStringLiteral("PaneTintStrength")).toInt() / 100.0;
    m_panes.removeAll(nullptr);
    for (const QPointer<QWidget> &pane : std::as_const(m_panes)) {
        const bool active = pane == m_activePane;
        const bool tinted = tint == QLatin1String("AllPanes") || (tint == QLatin1String("ActivePane") && active) ||
            (tint == QLatin1String("InactivePane") && !active);
        QPalette palette = paneBasePalette(pane);
        if (tinted) {
            for (QPalette::ColorRole role : {QPalette::Base, QPalette::AlternateBase}) {
                for (QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive}) {
                    const QColor base = palette.color(group, role);
                    palette.setColor(group, role, mix(base, color, strength));
                }
            }
        }
        const bool custom = tinted || palette.color(QPalette::Base).alpha() < 255;
        if (custom)
            pane->setPalette(palette);
        else if (pane->testAttribute(Qt::WA_SetPalette))
            pane->setPalette(QPalette());
    }
}

void AppearanceManager::repolish()
{
    // The widget style reads the options while drawing and measuring; widgets lay
    // out again for the new spacing
    const QList<QWidget *> widgets = QApplication::allWidgets();
    for (QWidget *widget : widgets) {
        QEvent change(QEvent::StyleChange);
        QApplication::sendEvent(widget, &change);
        if (QLayout *layout = widget->layout())
            layout->invalidate();
        widget->updateGeometry();
        widget->update();
    }
}

bool AppearanceManager::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == qApp && event->type() == QEvent::ApplicationPaletteChange && !m_applying && !m_paletteChanged) {
        // Plasma changed its colours; that is the palette to return to
        m_systemPalette = QGuiApplication::palette();
    } else if (event->type() == QEvent::Show && watched->isWidgetType() &&
               static_cast<QWidget *>(watched)->isWindow()) {
        for (const QPointer<QWidget> &window : std::as_const(m_windows)) {
            if (window == watched)
                applyWindow(window);
        }
    }
    return QObject::eventFilter(watched, event);
}

} // namespace Gui
