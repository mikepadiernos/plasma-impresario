/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/ThemeGroup.h"

#include <KColorButton>
#include <KConfigGroup>
#include <KFontRequester>
#include <KLazyLocalizedString>
#include <KLocalizedString>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include "Gui/AppearanceManager.h"
#include "UiUtils/Color.h"
#include "UiUtils/SettingsStore.h"
#include "UiUtils/StyleSheets.h"

namespace Gui {

namespace Theme = UiUtils::Theme;
namespace ThemeProfiles = UiUtils::ThemeProfiles;
namespace FolderColors = UiUtils::FolderColors;
namespace SettingsStore = UiUtils::SettingsStore;
using UiUtils::SettingsSchema::Group;

namespace {

QString choiceLabel(const QString &entry, const QString &choice)
{
    if (choice == QLatin1String("FollowStyle"))
        return i18nc("@item:inlistbox appearance choice", "As the widget style draws it");
    if (choice == QLatin1String("None")) {
        if (entry == QLatin1String("PaneTint"))
            return i18nc("@item:inlistbox pane tint", "No tint");
        if (entry == QLatin1String("TabIndicator"))
            return i18nc("@item:inlistbox active tab indicator", "No indicator");
        return i18nc("@item:inlistbox corner radius", "Square corners");
    }
    static const QHash<QString, KLazyLocalizedString> labels{
        {QStringLiteral("AllPanes"), kli18nc("@item:inlistbox pane tint", "All panes")},
        {QStringLiteral("ActivePane"), kli18nc("@item:inlistbox pane tint", "The pane with the focus")},
        {QStringLiteral("InactivePane"), kli18nc("@item:inlistbox pane tint", "The panes without the focus")},
        {QStringLiteral("Small"), kli18nc("@item:inlistbox corner radius", "Small")},
        {QStringLiteral("Medium"), kli18nc("@item:inlistbox corner radius", "Medium")},
        {QStringLiteral("Large"), kli18nc("@item:inlistbox corner radius", "Large")},
        {QStringLiteral("Custom"), kli18nc("@item:inlistbox corner radius", "Custom")},
        {QStringLiteral("Thin"), kli18nc("@item:inlistbox dividers", "Thin")},
        {QStringLiteral("Hidden"), kli18nc("@item:inlistbox lines or colours", "Hidden")},
        {QStringLiteral("Solid"), kli18nc("@item:inlistbox tree lines", "Solid")},
        {QStringLiteral("Dotted"), kli18nc("@item:inlistbox tree lines", "Dotted")},
        {QStringLiteral("Neutral"), kli18nc("@item:inlistbox tree line colours", "Neutral")},
        {QStringLiteral("Single"), kli18nc("@item:inlistbox tree line colours", "One colour")},
        {QStringLiteral("Depth"), kli18nc("@item:inlistbox tree line colours", "A colour for each level")},
        {QStringLiteral("Folder"), kli18nc("@item:inlistbox tree line colours", "The folder's or account's colour")},
        {QStringLiteral("Rows"), kli18nc("@item:inlistbox message list style", "Rows")},
        {QStringLiteral("Cards"), kli18nc("@item:inlistbox message list style", "Cards")},
        {QStringLiteral("Filled"), kli18nc("@item:inlistbox selection style", "Filled")},
        {QStringLiteral("Outlined"), kli18nc("@item:inlistbox selection style", "Outlined")},
        {QStringLiteral("Rounded"), kli18nc("@item:inlistbox selection or tab style", "Rounded")},
        {QStringLiteral("AccentBar"), kli18nc("@item:inlistbox selection style", "Accent bar")},
        {QStringLiteral("Icon"), kli18nc("@item:inlistbox folder colour style", "Coloured folder icon")},
        {QStringLiteral("Badge"), kli18nc("@item:inlistbox folder colour style", "Colour badge")},
        {QStringLiteral("Name"), kli18nc("@item:inlistbox folder colour style", "Coloured name")},
        {QStringLiteral("Square"), kli18nc("@item:inlistbox tab shape", "Square")},
        {QStringLiteral("Flat"), kli18nc("@item:inlistbox tab shape", "Flat")},
        {QStringLiteral("Line"), kli18nc("@item:inlistbox active tab indicator", "Line")},
        {QStringLiteral("ThickLine"), kli18nc("@item:inlistbox active tab indicator", "Thick line")},
        {QStringLiteral("Text"), kli18nc("@item:inlistbox tab colour strength", "Coloured text")},
        {QStringLiteral("Tint"), kli18nc("@item:inlistbox tab colour strength", "Tinted tab")},
        {QStringLiteral("Strong"), kli18nc("@item:inlistbox tab colour strength", "Filled tab")},
        {QStringLiteral("Separated"), kli18nc("@item:inlistbox status bar style", "Separated items")},
        {QStringLiteral("Shaded"), kli18nc("@item:inlistbox status bar style", "Shaded")},
        {QStringLiteral("FollowPlasma"), kli18nc("@item:inlistbox animations", "At Plasma's speed")},
        {QStringLiteral("Reduced"), kli18nc("@item:inlistbox animations", "Reduced")},
        {QStringLiteral("Off"), kli18nc("@item:inlistbox animations", "Off")},
    };
    const auto it = labels.constFind(choice);
    return it == labels.constEnd() ? choice : it->toString();
}

QFormLayout *addSectionTo(QVBoxLayout *page, const QString &title)
{
    if (page->count() > 0)
        page->addSpacing(page->spacing() * 2);
    auto *heading = new QLabel(title);
    QFont font = heading->font();
    font.setBold(true);
    heading->setFont(font);
    page->addWidget(heading);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    page->addLayout(form);
    return form;
}

QLabel *noteLabel(const QString &text)
{
    auto *note = new QLabel(text);
    note->setWordWrap(true);
    note->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    return note;
}

QPushButton *button(const QString &objectName, const QString &icon, const QString &text)
{
    auto *result = new QPushButton(QIcon::fromTheme(icon), text);
    result->setObjectName(objectName);
    return result;
}

QString ruleText(const FolderColors::Rule &rule, const SettingsContext &context)
{
    QString account = i18nc("@item folder colour rule for", "Every account");
    for (const SettingsContext::Account &known : context.accounts) {
        if (known.id == rule.account)
            account = known.name;
    }
    if (!rule.account.isEmpty() && account == i18nc("@item folder colour rule for", "Every account"))
        account = rule.account;
    const QString match = rule.match == FolderColors::Match::Path
        ? i18nc("@item folder colour rule, %1 is a pattern", "path %1", rule.pattern)
        : i18nc("@item folder colour rule, %1 is a pattern", "name %1", rule.pattern);
    const QString text = i18nc("@item folder colour rule: %1 account, %2 match, %3 colour", "%1: %2, %3", account,
                               match, rule.color.name());
    return rule.subfolders ? i18nc("@item folder colour rule", "%1, with subfolders", text) : text;
}

} // namespace

ThemeGroup::ThemeGroup(const SettingsContext &context, QWidget *parent)
    : SettingsGroupPage(Group::Theme, context, parent)
{
    m_values = Theme::defaults();
    const Theme::Values stored = SettingsStore::theme(*config());
    for (auto it = stored.constBegin(); it != stored.constEnd(); ++it)
        m_values.insert(it.key(), it.value());
    m_profiles = SettingsStore::themeProfiles(*config());
    m_followed = SettingsStore::followedTheme(*config());

    auto *intro = noteLabel(i18nc("@info",
                                  "Every option follows Plasma until it is changed, and changes "
                                  "apply to Plasma "
                                  "Impresario only: the desktop, other applications and window "
                                  "decorations keep "
                                  "their appearance. HTML messages keep their own styling."));
    contents()->addWidget(intro);

    auto *top = new QHBoxLayout;
    m_livePreview = new QCheckBox(i18nc("@option:check", "Preview changes in the window"));
    m_livePreview->setObjectName(QStringLiteral("livePreview"));
    top->addWidget(m_livePreview);
    top->addStretch();
    auto *reset = button(QStringLiteral("resetAppearance"), QStringLiteral("edit-undo"),
                         i18nc("@action:button", "Reset to Plasma Defaults"));
    reset->setToolTip(i18nc("@info:tooltip",
                            "Return every option to Plasma's appearance; theme "
                            "profiles, folder colour rules and "
                            "account colours are kept"));
    top->addWidget(reset);
    contents()->addLayout(top);

    auto *tabs = new QTabWidget;
    tabs->setObjectName(QStringLiteral("themeTabs"));
    tabs->addTab(colorsPage(), i18nc("@title:tab", "Colours"));
    tabs->addTab(effectsPage(), i18nc("@title:tab", "Effects"));
    tabs->addTab(listsPage(), i18nc("@title:tab", "Lines and Lists"));
    tabs->addTab(textPage(), i18nc("@title:tab", "Text and Tabs"));
    tabs->addTab(profilesPage(), i18nc("@title:tab", "Profiles"));
    tabs->addTab(styleSheetPage(), i18nc("@title:tab", "Stylesheet"));
    contents()->addWidget(tabs);

    m_problems = noteLabel(QString());
    m_problems->setObjectName(QStringLiteral("themeProblems"));
    m_problems->hide();
    contents()->addWidget(m_problems);

    connect(reset, &QPushButton::clicked, this, &ThemeGroup::resetToPlasma);
    connect(m_livePreview, &QCheckBox::toggled, this, &ThemeGroup::preview);
    showValues();
    fillProfiles();
}

QString ThemeGroup::optionLabel(const QString &entry)
{
    static const QHash<QString, KLazyLocalizedString> labels{
        {QStringLiteral("FollowStyle"), kli18nc("@item appearance option", "Follow the widget style")},
        {QStringLiteral("ColorScheme"), kli18nc("@item appearance option", "Colour scheme")},
        {QStringLiteral("AccentColor"), kli18nc("@item appearance option", "Accent colour")},
        {QStringLiteral("CustomColors"), kli18nc("@item appearance option", "Custom colours")},
        {QStringLiteral("PaneTint"), kli18nc("@item appearance option", "Pane tint")},
        {QStringLiteral("PaneTintColor"), kli18nc("@item appearance option", "Pane tint colour")},
        {QStringLiteral("PaneTintStrength"), kli18nc("@item appearance option", "Pane tint strength")},
        {QStringLiteral("BackgroundBlur"), kli18nc("@item appearance option", "Background blur")},
        {QStringLiteral("WindowOpacity"), kli18nc("@item appearance option", "Window opacity")},
        {QStringLiteral("PaneOpacity"), kli18nc("@item appearance option", "Pane opacity")},
        {QStringLiteral("PanelOpacity"), kli18nc("@item appearance option", "Panel opacity")},
        {QStringLiteral("CornerRadius"), kli18nc("@item appearance option", "Corner radius")},
        {QStringLiteral("CustomCornerRadius"), kli18nc("@item appearance option", "Custom corner radius")},
        {QStringLiteral("Dividers"), kli18nc("@item appearance option", "Divider visibility")},
        {QStringLiteral("RevealDividersOnHover"), kli18nc("@item appearance option", "Reveal dividers on hover")},
        {QStringLiteral("TreeLines"), kli18nc("@item appearance option", "Tree lines")},
        {QStringLiteral("TreeLineColors"), kli18nc("@item appearance option", "Tree line colouring")},
        {QStringLiteral("TreeLineColor"), kli18nc("@item appearance option", "Tree line colour")},
        {QStringLiteral("TreeLinePalette"), kli18nc("@item appearance option", "Tree line colours by level")},
        {QStringLiteral("MessageListStyle"), kli18nc("@item appearance option", "Message rows or cards")},
        {QStringLiteral("StripeRows"), kli18nc("@item appearance option", "Striped message list")},
        {QStringLiteral("StripeFolderTree"), kli18nc("@item appearance option", "Striped folder tree")},
        {QStringLiteral("HoverHighlight"), kli18nc("@item appearance option", "Hover highlight")},
        {QStringLiteral("SelectionStyle"), kli18nc("@item appearance option", "Selection style")},
        {QStringLiteral("FolderColorStyle"), kli18nc("@item appearance option", "Folder colour style")},
        {QStringLiteral("FolderColorRules"), kli18nc("@item appearance option", "Folder colour rules")},
        {QStringLiteral("MessageListFont"), kli18nc("@item appearance option", "Message list font")},
        {QStringLiteral("FolderTreeFont"), kli18nc("@item appearance option", "Folder tree font")},
        {QStringLiteral("MessageHeaderFont"), kli18nc("@item appearance option", "Message header font")},
        {QStringLiteral("PlainTextFont"), kli18nc("@item appearance option", "Plain-text message font")},
        {QStringLiteral("TabFont"), kli18nc("@item appearance option", "Tab font")},
        {QStringLiteral("StatusBarFont"), kli18nc("@item appearance option", "Status bar font")},
        {QStringLiteral("SpacingScale"), kli18nc("@item appearance option", "Spacing")},
        {QStringLiteral("TabShape"), kli18nc("@item appearance option", "Tab shape")},
        {QStringLiteral("TabIndicator"), kli18nc("@item appearance option", "Active tab indicator")},
        {QStringLiteral("TabColorStrength"), kli18nc("@item appearance option", "Colour strength on tabs")},
        {QStringLiteral("StatusBarStyle"), kli18nc("@item appearance option", "Status bar style")},
        {QStringLiteral("Animations"), kli18nc("@item appearance option", "Animation speed")},
        {QStringLiteral("UseCustomStyleSheet"), kli18nc("@item appearance option", "Custom stylesheet in use")},
        {QStringLiteral("CustomStyleSheet"), kli18nc("@item appearance option", "Custom stylesheet")},
        {QStringLiteral("SuspendEffectsInHighContrast"),
         kli18nc("@item appearance option", "Suspend effects with high contrast")},
    };
    const auto it = labels.constFind(entry);
    return it == labels.constEnd() ? entry : it->toString();
}

ThemeGroup::~ThemeGroup()
{
    // Back to the saved appearance; the window applies its workspace's appearance
    // on top again
    if (m_previewed && !m_saved)
        AppearanceManager::instance()->load(*config());
}

QComboBox *ThemeGroup::addChoice(QFormLayout *form, const QString &entry, const QString &label)
{
    auto *combo = new QComboBox;
    combo->setObjectName(QLatin1String("theme-") + entry);
    for (const QString &choice : Theme::option(entry)->choices)
        combo->addItem(choiceLabel(entry, choice), choice);
    form->addRow(label, combo);
    m_editors.insert(entry, combo);
    watch(combo);
    return combo;
}

QCheckBox *ThemeGroup::addFlag(QFormLayout *form, const QString &entry, const QString &text)
{
    auto *check = new QCheckBox(text);
    check->setObjectName(QLatin1String("theme-") + entry);
    form->addRow(QString(), check);
    m_editors.insert(entry, check);
    watch(check);
    return check;
}

QSpinBox *ThemeGroup::addNumber(QFormLayout *form, const QString &entry, const QString &label, const QString &suffix)
{
    const Theme::Option *option = Theme::option(entry);
    auto *spin = new QSpinBox;
    spin->setObjectName(QLatin1String("theme-") + entry);
    spin->setRange(option->minimum, option->maximum);
    spin->setSuffix(suffix);
    form->addRow(label, spin);
    m_editors.insert(entry, spin);
    watch(spin);
    return spin;
}

KColorButton *ThemeGroup::addColor(QFormLayout *form, const QString &entry, const QString &label)
{
    auto *color = new KColorButton;
    color->setObjectName(QLatin1String("theme-") + entry);
    color->setAlphaChannelEnabled(false);
    form->addRow(label, color);
    m_editors.insert(entry, color);
    watch(color);
    return color;
}

void ThemeGroup::addFont(QFormLayout *form, const QString &entry, const QString &label)
{
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *custom = new QCheckBox(i18nc("@option:check use a font of one's own", "Own font:"));
    custom->setObjectName(QLatin1String("themeFontCustom-") + entry);
    auto *requester = new KFontRequester;
    requester->setObjectName(QLatin1String("theme-") + entry);
    layout->addWidget(custom);
    layout->addWidget(requester, 1);
    form->addRow(label, row);
    m_editors.insert(entry, requester);
    m_fontCustom.insert(entry, custom);
    watch(custom);
    watch(requester);
}

void ThemeGroup::watch(QWidget *editor)
{
    if (auto *check = qobject_cast<QCheckBox *>(editor))
        connect(check, &QCheckBox::toggled, this, &ThemeGroup::onChanged);
    else if (auto *combo = qobject_cast<QComboBox *>(editor))
        connect(combo, &QComboBox::currentIndexChanged, this, &ThemeGroup::onChanged);
    else if (auto *spin = qobject_cast<QSpinBox *>(editor))
        connect(spin, &QSpinBox::valueChanged, this, &ThemeGroup::onChanged);
    else if (auto *color = qobject_cast<KColorButton *>(editor))
        connect(color, &KColorButton::changed, this, &ThemeGroup::onChanged);
    else if (auto *font = qobject_cast<KFontRequester *>(editor))
        connect(font, &KFontRequester::fontSelected, this, &ThemeGroup::onChanged);
    else if (auto *line = qobject_cast<QLineEdit *>(editor))
        connect(line, &QLineEdit::editingFinished, this, &ThemeGroup::onChanged);
    else if (auto *text = qobject_cast<QPlainTextEdit *>(editor))
        connect(text, &QPlainTextEdit::textChanged, this, &ThemeGroup::onChanged);
}

QWidget *ThemeGroup::colorsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    QFormLayout *scheme = addSectionTo(layout, i18nc("@title:group", "Colour Scheme"));
    auto *schemes = new QComboBox;
    schemes->setObjectName(QStringLiteral("theme-ColorScheme"));
    schemes->addItem(i18nc("@item:inlistbox colour scheme", "Plasma's colours"), QString());
    schemes->addItem(i18nc("@item:inlistbox colour scheme", "Light"), QLatin1String(Theme::LightPreset));
    schemes->addItem(i18nc("@item:inlistbox colour scheme", "Dark"), QLatin1String(Theme::DarkPreset));
    for (const auto &[name, display] : AppearanceManager::installedColorSchemes())
        schemes->addItem(display, name);
    scheme->addRow(i18nc("@label:listbox", "Colours:"), schemes);
    m_editors.insert(QStringLiteral("ColorScheme"), schemes);
    watch(schemes);

    auto *accentRow = new QWidget;
    auto *accentLayout = new QHBoxLayout(accentRow);
    accentLayout->setContentsMargins(0, 0, 0, 0);
    m_accentSource = new QComboBox;
    m_accentSource->setObjectName(QStringLiteral("accentSource"));
    m_accentSource->addItem(i18nc("@item:inlistbox accent colour", "Plasma's accent"), false);
    m_accentSource->addItem(i18nc("@item:inlistbox accent colour", "Own colour"), true);
    m_accent = new KColorButton;
    m_accent->setObjectName(QStringLiteral("accentColor"));
    m_accent->setAlphaChannelEnabled(false);
    accentLayout->addWidget(m_accentSource);
    accentLayout->addWidget(m_accent);
    accentLayout->addStretch();
    scheme->addRow(i18nc("@label", "Accent:"), accentRow);
    watch(m_accentSource);
    watch(m_accent);
    scheme->addRow(noteLabel(i18nc("@info",
                                   "The accent colours the selection, the focus, "
                                   "the active pane and tab, progress "
                                   "and unread indicators.")));

    QFormLayout *roles = addSectionTo(layout, i18nc("@title:group", "Colour Roles"));
    for (const Theme::Role &role : Theme::roles()) {
        auto *row = new QWidget;
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto *check = new QCheckBox(i18nc("@option:check use a colour of one's own", "Own colour"));
        check->setObjectName(QLatin1String("role-") + role.key);
        auto *color = new KColorButton;
        color->setObjectName(QLatin1String("roleColor-") + role.key);
        color->setAlphaChannelEnabled(false);
        rowLayout->addWidget(check);
        rowLayout->addWidget(color);
        rowLayout->addStretch();
        roles->addRow(role.name + QLatin1Char(':'), row);
        m_roles.insert(role.key, {check, color});
        watch(check);
        watch(color);
    }
    m_contrast = noteLabel(QString());
    m_contrast->setObjectName(QStringLiteral("contrastIssues"));
    roles->addRow(m_contrast);

    QFormLayout *tint = addSectionTo(layout, i18nc("@title:group", "Pane Tint"));
    addChoice(tint, QStringLiteral("PaneTint"), i18nc("@label:listbox", "Tint:"));
    addColor(tint, QStringLiteral("PaneTintColor"), i18nc("@label", "Tint colour:"));
    addNumber(tint, QStringLiteral("PaneTintStrength"), i18nc("@label:spinbox", "Strength:"),
              i18nc("@item:valuesuffix percent", " %"));

    if (!m_context.accounts.isEmpty()) {
        QFormLayout *accounts = addSectionTo(layout, i18nc("@title:group", "Account Colours"));
        const FolderColors::Colors colors = SettingsStore::folderColors(*config());
        for (const SettingsContext::Account &account : m_context.accounts) {
            auto *row = new QWidget;
            auto *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            auto *check = new QCheckBox(i18nc("@option:check give the account a colour", "Colour"));
            check->setObjectName(QLatin1String("accountColorUsed-") + account.id);
            auto *color = new KColorButton;
            color->setObjectName(QLatin1String("accountColor-") + account.id);
            color->setAlphaChannelEnabled(false);
            const QColor stored = colors.accounts.value(account.id);
            check->setChecked(stored.isValid());
            color->setColor(stored.isValid() ? stored : QColor(0x3d, 0xae, 0xe9));
            color->setEnabled(stored.isValid());
            connect(check, &QCheckBox::toggled, color, &QWidget::setEnabled);
            rowLayout->addWidget(check);
            rowLayout->addWidget(color);
            rowLayout->addStretch();
            accounts->addRow(account.name + QLatin1Char(':'), row);
            m_accountColors.insert(account.id, {check, color});
        }
        accounts->addRow(noteLabel(i18nc("@info",
                                         "Accounts are always named as well, so that a colour is never the only "
                                         "way to tell them apart. Folder colours are chosen from a folder's "
                                         "context menu.")));
    }

    QFormLayout *access = addSectionTo(layout, i18nc("@title:group", "Accessibility"));
    addFlag(access, QStringLiteral("SuspendEffectsInHighContrast"),
            i18nc("@option:check",
                  "With a high-contrast colour scheme, turn off blur, "
                  "translucency, tints, corner radii and the "
                  "stylesheet"));
    layout->addStretch();
    return page;
}

QWidget *ThemeGroup::effectsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    QFormLayout *window = addSectionTo(layout, i18nc("@title:group", "Translucency"));
    addFlag(window, QStringLiteral("BackgroundBlur"), i18nc("@option:check", "Blur behind translucent windows"));
    if (!AppearanceManager::blurAvailable()) {
        auto *note = noteLabel(i18nc("@info",
                                     "KWin is not blurring windows now, so the "
                                     "background stays opaque until its "
                                     "blur effect is available."));
        note->setObjectName(QStringLiteral("blurUnavailable"));
        window->addRow(note);
    }
    const QString percent = i18nc("@item:valuesuffix percent", " %");
    addNumber(window, QStringLiteral("WindowOpacity"), i18nc("@label:spinbox", "Window opacity:"), percent);
    addNumber(window, QStringLiteral("PaneOpacity"), i18nc("@label:spinbox", "Pane opacity:"), percent);
    addNumber(window, QStringLiteral("PanelOpacity"), i18nc("@label:spinbox", "Panel opacity:"), percent);
    window->addRow(noteLabel(i18nc("@info",
                                   "Text, icons, the selection and message content "
                                   "stay opaque. A window becomes "
                                   "translucent when it next opens.")));

    QFormLayout *shape = addSectionTo(layout, i18nc("@title:group", "Shape"));
    addChoice(shape, QStringLiteral("CornerRadius"), i18nc("@label:listbox", "Corner radius:"));
    addNumber(shape, QStringLiteral("CustomCornerRadius"), i18nc("@label:spinbox", "Custom radius:"),
              i18nc("@item:valuesuffix pixels", " px"));
    shape->addRow(noteLabel(i18nc("@info",
                                  "The radius rounds item highlights, message cards, "
                                  "panels, input fields, tabs and "
                                  "tooltips. Window corners stay with the window "
                                  "decoration, and buttons, combo "
                                  "boxes and scroll bars keep the widget style's shape.")));

    QFormLayout *motion = addSectionTo(layout, i18nc("@title:group", "Motion"));
    addChoice(motion, QStringLiteral("Animations"), i18nc("@label:listbox", "Animations:"));
    layout->addStretch();
    return page;
}

QWidget *ThemeGroup::listsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    QFormLayout *lines = addSectionTo(layout, i18nc("@title:group", "Lines and Dividers"));
    addChoice(lines, QStringLiteral("Dividers"), i18nc("@label:listbox", "Dividers:"));
    addFlag(lines, QStringLiteral("RevealDividersOnHover"),
            i18nc("@option:check", "Show hidden dividers under the mouse pointer"));
    addChoice(lines, QStringLiteral("TreeLines"), i18nc("@label:listbox", "Tree lines:"));
    addChoice(lines, QStringLiteral("TreeLineColors"), i18nc("@label:listbox", "Tree line colours:"));
    addColor(lines, QStringLiteral("TreeLineColor"), i18nc("@label", "Line colour:"));
    auto *palette = new QLineEdit;
    palette->setObjectName(QStringLiteral("theme-TreeLinePalette"));
    palette->setPlaceholderText(i18nc("@info:placeholder", "Colours such as #3daee9, #27ae60"));
    lines->addRow(i18nc("@label:textbox", "Colours by level:"), palette);
    m_editors.insert(QStringLiteral("TreeLinePalette"), palette);
    watch(palette);
    lines->addRow(noteLabel(i18nc("@info",
                                  "Hidden dividers keep their resize area and keyboard "
                                  "resizing. Nesting stays "
                                  "visible by indentation, whatever the colours.")));

    QFormLayout *lists = addSectionTo(layout, i18nc("@title:group", "Lists"));
    addChoice(lists, QStringLiteral("MessageListStyle"), i18nc("@label:listbox", "Message list:"));
    addFlag(lists, QStringLiteral("StripeRows"), i18nc("@option:check", "Stripe the rows of the message list"));
    addFlag(lists, QStringLiteral("StripeFolderTree"), i18nc("@option:check", "Stripe the rows of the folder tree"));
    addFlag(lists, QStringLiteral("HoverHighlight"), i18nc("@option:check", "Highlight the row under the pointer"));
    addChoice(lists, QStringLiteral("SelectionStyle"), i18nc("@label:listbox", "Selection:"));

    QFormLayout *folders = addSectionTo(layout, i18nc("@title:group", "Folder Colours"));
    addChoice(folders, QStringLiteral("FolderColorStyle"), i18nc("@label:listbox", "Show colours as:"));
    m_ruleList = new QListWidget;
    m_ruleList->setObjectName(QStringLiteral("folderColorRules"));
    m_ruleList->setMaximumHeight(m_ruleList->fontMetrics().height() * 7);
    folders->addRow(i18nc("@label", "Rules:"), m_ruleList);
    auto *editor = new QWidget;
    auto *editorLayout = new QHBoxLayout(editor);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    m_ruleAccount = new QComboBox;
    m_ruleAccount->setObjectName(QStringLiteral("ruleAccount"));
    m_ruleAccount->addItem(i18nc("@item:inlistbox folder colour rule for", "Every account"), QString());
    for (const SettingsContext::Account &account : m_context.accounts)
        m_ruleAccount->addItem(account.name, account.id);
    m_ruleMatch = new QComboBox;
    m_ruleMatch->setObjectName(QStringLiteral("ruleMatch"));
    m_ruleMatch->addItem(i18nc("@item:inlistbox folder colour rule matches", "Name"),
                         static_cast<int>(FolderColors::Match::Name));
    m_ruleMatch->addItem(i18nc("@item:inlistbox folder colour rule matches", "Path"),
                         static_cast<int>(FolderColors::Match::Path));
    m_rulePattern = new QLineEdit;
    m_rulePattern->setObjectName(QStringLiteral("rulePattern"));
    m_rulePattern->setPlaceholderText(i18nc("@info:placeholder", "Archive* or Lists/*"));
    m_ruleColor = new KColorButton(QColor(0x27, 0xae, 0x60));
    m_ruleColor->setObjectName(QStringLiteral("ruleColor"));
    m_ruleColor->setAlphaChannelEnabled(false);
    m_ruleSubfolders = new QCheckBox(i18nc("@option:check", "Subfolders too"));
    m_ruleSubfolders->setObjectName(QStringLiteral("ruleSubfolders"));
    editorLayout->addWidget(m_ruleAccount);
    editorLayout->addWidget(m_ruleMatch);
    editorLayout->addWidget(m_rulePattern, 1);
    editorLayout->addWidget(m_ruleColor);
    editorLayout->addWidget(m_ruleSubfolders);
    folders->addRow(QString(), editor);
    auto *ruleButtons = new QHBoxLayout;
    auto *add =
        button(QStringLiteral("addFolderRule"), QStringLiteral("list-add"), i18nc("@action:button", "Add Rule"));
    auto *remove = button(QStringLiteral("removeFolderRule"), QStringLiteral("list-remove"),
                          i18nc("@action:button", "Remove Rule"));
    ruleButtons->addWidget(add);
    ruleButtons->addWidget(remove);
    ruleButtons->addStretch();
    folders->addRow(QString(), ruleButtons);
    folders->addRow(noteLabel(i18nc("@info",
                                    "The first matching rule colours a folder; a colour chosen for the "
                                    "folder itself "
                                    "comes first. Rules never change folders or messages on the server.")));
    connect(add, &QPushButton::clicked, this, &ThemeGroup::addRule);
    connect(remove, &QPushButton::clicked, this, &ThemeGroup::removeRule);
    layout->addStretch();
    return page;
}

QWidget *ThemeGroup::textPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    QFormLayout *fonts = addSectionTo(layout, i18nc("@title:group", "Fonts"));
    addFont(fonts, QStringLiteral("MessageListFont"), i18nc("@label", "Message list:"));
    addFont(fonts, QStringLiteral("FolderTreeFont"), i18nc("@label", "Folder tree:"));
    addFont(fonts, QStringLiteral("MessageHeaderFont"), i18nc("@label", "Message headers:"));
    addFont(fonts, QStringLiteral("PlainTextFont"), i18nc("@label", "Plain-text messages:"));
    addFont(fonts, QStringLiteral("TabFont"), i18nc("@label", "Tabs:"));
    addFont(fonts, QStringLiteral("StatusBarFont"), i18nc("@label", "Status bar:"));
    fonts->addRow(noteLabel(i18nc("@info", "Without a font of their own, these follow Plasma's fonts.")));

    QFormLayout *spacing = addSectionTo(layout, i18nc("@title:group", "Spacing"));
    m_spacingPreset = new QComboBox;
    m_spacingPreset->setObjectName(QStringLiteral("spacingPreset"));
    m_spacingPreset->addItem(i18nc("@item:inlistbox spacing", "Compact"), static_cast<int>(Theme::Spacing::Compact));
    m_spacingPreset->addItem(i18nc("@item:inlistbox spacing", "Normal"), static_cast<int>(Theme::Spacing::Normal));
    m_spacingPreset->addItem(i18nc("@item:inlistbox spacing", "Comfortable"),
                             static_cast<int>(Theme::Spacing::Comfortable));
    m_spacingPreset->addItem(i18nc("@item:inlistbox spacing", "Custom"), static_cast<int>(Theme::Spacing::Custom));
    spacing->addRow(i18nc("@label:listbox", "Spacing:"), m_spacingPreset);
    addNumber(spacing, QStringLiteral("SpacingScale"), i18nc("@label:spinbox", "Scale:"),
              i18nc("@item:valuesuffix percent", " %"));
    spacing->addRow(noteLabel(i18nc("@info",
                                    "Spacing scales the margins of toolbars, tabs, panels and the "
                                    "status bar; the "
                                    "layout preset still chooses the height of message rows.")));
    connect(m_spacingPreset, &QComboBox::currentIndexChanged, this, [this]() {
        if (m_loading)
            return;
        const auto preset = static_cast<Theme::Spacing>(m_spacingPreset->currentData().toInt());
        auto *scale = static_cast<QSpinBox *>(m_editors.value(QStringLiteral("SpacingScale")));
        if (preset != Theme::Spacing::Custom)
            scale->setValue(Theme::spacingScale(preset, scale->value()));
        updateDependent();
    });

    QFormLayout *tabs = addSectionTo(layout, i18nc("@title:group", "Tabs and Status Bar"));
    addChoice(tabs, QStringLiteral("TabShape"), i18nc("@label:listbox", "Tab shape:"));
    addChoice(tabs, QStringLiteral("TabIndicator"), i18nc("@label:listbox", "Active tab:"));
    addChoice(tabs, QStringLiteral("TabColorStrength"), i18nc("@label:listbox", "Folder colours on tabs:"));
    addChoice(tabs, QStringLiteral("StatusBarStyle"), i18nc("@label:listbox", "Status bar:"));
    layout->addStretch();
    return page;
}

QWidget *ThemeGroup::profilesPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    QFormLayout *profiles = addSectionTo(layout, i18nc("@title:group", "Theme Profiles"));
    m_profileList = new QListWidget;
    m_profileList->setObjectName(QStringLiteral("themeProfiles"));
    m_profileList->setMaximumHeight(m_profileList->fontMetrics().height() * 8);
    profiles->addRow(m_profileList);
    m_profileName = new QLineEdit;
    m_profileName->setObjectName(QStringLiteral("profileName"));
    m_profileName->setPlaceholderText(i18nc("@info:placeholder", "Name of the profile"));
    profiles->addRow(i18nc("@label:textbox", "Name:"), m_profileName);
    auto *buttons = new QHBoxLayout;
    auto *save = button(QStringLiteral("saveProfile"), QStringLiteral("document-save"),
                        i18nc("@action:button", "Save Options as Profile"));
    auto *use =
        button(QStringLiteral("useProfile"), QStringLiteral("dialog-ok-apply"), i18nc("@action:button", "Use Profile"));
    auto *remove =
        button(QStringLiteral("deleteProfile"), QStringLiteral("edit-delete"), i18nc("@action:button", "Delete"));
    buttons->addWidget(save);
    buttons->addWidget(use);
    buttons->addWidget(remove);
    buttons->addStretch();
    profiles->addRow(buttons);
    profiles->addRow(noteLabel(i18nc("@info",
                                     "A profile holds the options of this page, "
                                     "except the folder colour rules, and "
                                     "can be attached to a workspace.")));

    QFormLayout *files = addSectionTo(layout, i18nc("@title:group", "Files"));
    auto *fileButtons = new QHBoxLayout;
    auto *import =
        button(QStringLiteral("importProfile"), QStringLiteral("document-import"), i18nc("@action:button", "Import…"));
    auto *exportButton =
        button(QStringLiteral("exportProfile"), QStringLiteral("document-export"), i18nc("@action:button", "Export…"));
    fileButtons->addWidget(import);
    fileButtons->addWidget(exportButton);
    fileButtons->addStretch();
    files->addRow(fileButtons);
    m_exportForOpus = new QCheckBox(i18nc("@option:check", "Export for Plasma Opus"));
    m_exportForOpus->setObjectName(QStringLiteral("exportForOpus"));
    m_exportStyleSheet = new QCheckBox(i18nc("@option:check", "Include the stylesheet"));
    m_exportStyleSheet->setObjectName(QStringLiteral("exportStyleSheet"));
    m_exportStyleSheet->setChecked(true);
    files->addRow(QString(), m_exportForOpus);
    files->addRow(QString(), m_exportStyleSheet);

    auto *follow = new QHBoxLayout;
    m_followedFile = new QLineEdit;
    m_followedFile->setObjectName(QStringLiteral("followedTheme"));
    m_followedFile->setReadOnly(true);
    m_followedFile->setPlaceholderText(i18nc("@info:placeholder", "No file is followed"));
    m_followedFile->setText(m_followed);
    auto *choose =
        button(QStringLiteral("followTheme"), QStringLiteral("document-open"), i18nc("@action:button", "Follow File…"));
    auto *stop = button(QStringLiteral("stopFollowingTheme"), QStringLiteral("edit-clear"),
                        i18nc("@action:button", "Stop Following"));
    follow->addWidget(m_followedFile, 1);
    follow->addWidget(choose);
    follow->addWidget(stop);
    files->addRow(i18nc("@label", "Follow:"), follow);
    files->addRow(noteLabel(i18nc("@info",
                                  "A followed file, such as a theme exported by "
                                  "Plasma Opus, becomes the appearance "
                                  "whenever it changes. Theme files only hold "
                                  "option values; nothing in them is "
                                  "run, and they cannot load other files.")));
    m_profileStatus = noteLabel(QString());
    m_profileStatus->setObjectName(QStringLiteral("profileStatus"));
    files->addRow(m_profileStatus);
    layout->addStretch();

    connect(save, &QPushButton::clicked, this, &ThemeGroup::saveProfile);
    connect(use, &QPushButton::clicked, this, &ThemeGroup::useProfile);
    connect(remove, &QPushButton::clicked, this, &ThemeGroup::deleteProfile);
    connect(import, &QPushButton::clicked, this, &ThemeGroup::importProfile);
    connect(exportButton, &QPushButton::clicked, this, &ThemeGroup::exportProfile);
    connect(choose, &QPushButton::clicked, this, &ThemeGroup::followTheme);
    connect(stop, &QPushButton::clicked, this, [this]() {
        m_followed.clear();
        m_followedFile->clear();
    });
    connect(m_profileList, &QListWidget::currentRowChanged, this, [this]() {
        if (const ThemeProfiles::Profile *profile = selectedProfile())
            m_profileName->setText(profile->name);
    });
    return page;
}

QWidget *ThemeGroup::styleSheetPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    QFormLayout *sheet = addSectionTo(layout, i18nc("@title:group", "Custom Stylesheet"));
    addFlag(sheet, QStringLiteral("UseCustomStyleSheet"), i18nc("@option:check", "Apply the stylesheet"));
    auto *text = new QPlainTextEdit;
    text->setObjectName(QStringLiteral("theme-CustomStyleSheet"));
    text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    text->setPlaceholderText(QStringLiteral("QTreeView { padding: 2px; }"));
    sheet->addRow(text);
    m_editors.insert(QStringLiteral("CustomStyleSheet"), text);
    watch(text);
    auto *buttons = new QHBoxLayout;
    auto *check = button(QStringLiteral("checkStyleSheet"), QStringLiteral("tools-check-spelling"),
                         i18nc("@action:button", "Check"));
    auto *import = button(QStringLiteral("importStyleSheet"), QStringLiteral("document-import"),
                          i18nc("@action:button", "Import…"));
    auto *exportButton = button(QStringLiteral("exportStyleSheet"), QStringLiteral("document-export"),
                                i18nc("@action:button", "Export…"));
    buttons->addWidget(check);
    buttons->addWidget(import);
    buttons->addWidget(exportButton);
    buttons->addStretch();
    sheet->addRow(buttons);
    m_styleSheetStatus = noteLabel(QString());
    m_styleSheetStatus->setObjectName(QStringLiteral("styleSheetStatus"));
    sheet->addRow(m_styleSheetStatus);
    sheet->addRow(noteLabel(i18nc("@info",
                                  "A Qt stylesheet, a subset of CSS, styles the "
                                  "application on top of the other "
                                  "options; it does not style HTML messages. "
                                  "Imports, external files and web "
                                  "addresses are refused.")));
    connect(check, &QPushButton::clicked, this, &ThemeGroup::checkStyleSheet);
    connect(import, &QPushButton::clicked, this, &ThemeGroup::importStyleSheet);
    connect(exportButton, &QPushButton::clicked, this, &ThemeGroup::exportStyleSheet);
    return page;
}

void ThemeGroup::showValues()
{
    m_loading = true;
    for (auto it = m_editors.constBegin(); it != m_editors.constEnd(); ++it) {
        const QVariant value = Theme::value(m_values, it.key());
        QWidget *editor = it.value();
        if (auto *check = qobject_cast<QCheckBox *>(editor)) {
            check->setChecked(value.toBool());
        } else if (auto *combo = qobject_cast<QComboBox *>(editor)) {
            int index = combo->findData(value.toString());
            if (index < 0 && it.key() == QLatin1String("ColorScheme")) {
                // An uninstalled scheme stays chosen, to apply again once it is back
                combo->addItem(i18nc("@item:inlistbox %1 is a colour scheme", "%1 (not installed)", value.toString()),
                               value.toString());
                index = combo->count() - 1;
            }
            combo->setCurrentIndex(std::max(0, index));
        } else if (auto *spin = qobject_cast<QSpinBox *>(editor)) {
            spin->setValue(value.toInt());
        } else if (auto *color = qobject_cast<KColorButton *>(editor)) {
            color->setColor(value.value<QColor>());
        } else if (auto *font = qobject_cast<KFontRequester *>(editor)) {
            const QString description = value.toString();
            QFont chosen = QApplication::font();
            if (it.key() == QLatin1String("PlainTextFont"))
                chosen = QFontDatabase::systemFont(QFontDatabase::FixedFont);
            if (!description.isEmpty())
                chosen.fromString(description);
            font->setFont(chosen);
            m_fontCustom.value(it.key())->setChecked(!description.isEmpty());
        } else if (auto *line = qobject_cast<QLineEdit *>(editor)) {
            line->setText(value.toStringList().join(QLatin1String(", ")));
        } else if (auto *text = qobject_cast<QPlainTextEdit *>(editor)) {
            if (text->toPlainText() != value.toString())
                text->setPlainText(value.toString());
        }
    }

    const QString accent = Theme::value(m_values, QStringLiteral("AccentColor")).toString();
    const bool ownAccent = !accent.isEmpty() && accent != QLatin1String(Theme::PlasmaAccent);
    m_accentSource->setCurrentIndex(ownAccent ? 1 : 0);
    m_accent->setColor(Theme::accentColor(accent, QApplication::palette().color(QPalette::Highlight)));

    const QPalette palette =
        AppearanceManager::schemePalette(Theme::value(m_values, QStringLiteral("ColorScheme")).toString());
    const Theme::Colors custom =
        Theme::parseColors(Theme::value(m_values, QStringLiteral("CustomColors")).toStringList());
    for (auto it = m_roles.constBegin(); it != m_roles.constEnd(); ++it) {
        QColor shown = custom.value(it.key());
        if (!shown.isValid()) {
            if (it.key() == QLatin1String("Unread"))
                shown = palette.color(QPalette::Highlight);
            else if (it.key() == QLatin1String("Flagged"))
                shown = QColor(0xda, 0x44, 0x53);
            else {
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
                shown = palette.color(roles.value(it.key(), QPalette::Window));
            }
        }
        it->first->setChecked(custom.contains(it.key()));
        it->second->setColor(shown);
    }

    const auto scale = Theme::value(m_values, QStringLiteral("SpacingScale")).toInt();
    m_spacingPreset->setCurrentIndex(m_spacingPreset->findData(static_cast<int>(Theme::spacingPreset(scale))));
    m_rules = FolderColors::rules(Theme::value(m_values, QStringLiteral("FolderColorRules")).toStringList());
    fillRules();
    m_loading = false;
    updateDependent();
    updateContrast();
}

void ThemeGroup::readWidgets()
{
    for (auto it = m_editors.constBegin(); it != m_editors.constEnd(); ++it) {
        QWidget *editor = it.value();
        QVariant value;
        if (auto *check = qobject_cast<QCheckBox *>(editor))
            value = check->isChecked();
        else if (auto *combo = qobject_cast<QComboBox *>(editor))
            value = combo->currentData().toString();
        else if (auto *spin = qobject_cast<QSpinBox *>(editor))
            value = spin->value();
        else if (auto *color = qobject_cast<KColorButton *>(editor))
            value = color->color();
        else if (auto *font = qobject_cast<KFontRequester *>(editor))
            value = m_fontCustom.value(it.key())->isChecked() ? font->font().toString() : QString();
        else if (auto *line = qobject_cast<QLineEdit *>(editor)) {
            QStringList colors;
            for (const QString &part : line->text().split(QLatin1Char(','), Qt::SkipEmptyParts)) {
                if (!part.trimmed().isEmpty())
                    colors.append(part.trimmed());
            }
            value = colors;
        } else if (auto *text = qobject_cast<QPlainTextEdit *>(editor))
            value = text->toPlainText();
        m_values.insert(it.key(), value);
    }
    m_values.insert(QStringLiteral("AccentColor"),
                    m_accentSource->currentData().toBool() ? m_accent->color().name() : QString());
    Theme::Colors custom;
    for (auto it = m_roles.constBegin(); it != m_roles.constEnd(); ++it) {
        if (it->first->isChecked())
            custom.insert(it.key(), it->second->color());
    }
    m_values.insert(QStringLiteral("CustomColors"), Theme::formatColors(custom));
    m_values.insert(QStringLiteral("FolderColorRules"), FolderColors::entries(m_rules));
}

void ThemeGroup::onChanged()
{
    if (m_loading)
        return;
    readWidgets();
    if (sender() == m_editors.value(QStringLiteral("SpacingScale"))) {
        m_loading = true;
        const int scale = Theme::value(m_values, QStringLiteral("SpacingScale")).toInt();
        m_spacingPreset->setCurrentIndex(m_spacingPreset->findData(static_cast<int>(Theme::spacingPreset(scale))));
        m_loading = false;
    }
    updateDependent();
    updateContrast();
    if (m_livePreview->isChecked())
        preview();
}

void ThemeGroup::updateDependent()
{
    const auto choice = [this](const char *entry) { return Theme::value(m_values, QLatin1String(entry)).toString(); };
    const auto enable = [this](const char *entry, bool enabled) {
        if (QWidget *editor = m_editors.value(QLatin1String(entry)))
            editor->setEnabled(enabled);
    };
    const bool tint = choice("PaneTint") != QLatin1String("None");
    enable("PaneTintColor", tint);
    enable("PaneTintStrength", tint);
    enable("CustomCornerRadius", choice("CornerRadius") == QLatin1String("Custom"));
    enable("RevealDividersOnHover", choice("Dividers") == QLatin1String("Hidden"));
    const bool lines = choice("TreeLines") != QLatin1String("Hidden");
    enable("TreeLineColors", lines);
    enable("TreeLineColor", lines && choice("TreeLineColors") == QLatin1String("Single"));
    enable("TreeLinePalette", lines && choice("TreeLineColors") == QLatin1String("Depth"));
    enable("CustomStyleSheet", true);
    enable("SpacingScale",
           static_cast<Theme::Spacing>(m_spacingPreset->currentData().toInt()) == Theme::Spacing::Custom);
    m_accent->setEnabled(m_accentSource->currentData().toBool());
    for (auto it = m_fontCustom.constBegin(); it != m_fontCustom.constEnd(); ++it)
        m_editors.value(it.key())->setEnabled(it.value()->isChecked());
    for (auto it = m_roles.constBegin(); it != m_roles.constEnd(); ++it)
        it->second->setEnabled(it->first->isChecked());
}

void ThemeGroup::updateContrast()
{
    Theme::Colors colors;
    for (auto it = m_roles.constBegin(); it != m_roles.constEnd(); ++it)
        colors.insert(it.key(), it->second->color());
    QStringList changed;
    for (auto it = m_roles.constBegin(); it != m_roles.constEnd(); ++it) {
        if (it->first->isChecked())
            changed.append(it.key());
        else if (it.key() == QLatin1String("Unread") || it.key() == QLatin1String("Flagged"))
            colors.remove(it.key());
    }
    if (m_accentSource->currentData().toBool()) {
        const QColor accent = m_accent->color();
        colors.insert(QStringLiteral("Highlight"), accent);
        colors.insert(QStringLiteral("HighlightedText"), UiUtils::readableTextColor(accent));
        colors.insert(QStringLiteral("Accent"), accent);
        changed << QStringLiteral("Highlight") << QStringLiteral("Accent");
    }
    QStringList lines;
    for (const Theme::ContrastIssue &issue : Theme::contrastIssues(colors, changed))
        lines.append(Theme::describe(issue));
    m_contrast->setText(lines.isEmpty() ? i18nc("@info", "The chosen colours keep text and indicators readable.")
                                        : lines.join(QLatin1Char('\n')));
}

void ThemeGroup::preview()
{
    readWidgets();
    QStringList problems;
    const Theme::Values values = Theme::sanitized(m_values, &problems);
    FolderColors::Colors colors = SettingsStore::folderColors(*config());
    colors.rules = m_rules;
    AppearanceManager::instance()->apply(values, colors);
    m_previewed = true;
}

void ThemeGroup::resetToPlasma()
{
    if (m_context.confirm &&
        !m_context.confirm(this, i18nc("@title:window", "Reset Appearance"),
                           i18nc("@info",
                                 "Return every appearance option to Plasma's? "
                                 "Theme profiles, folder colour rules "
                                 "and account colours are kept.")))
        return;
    readWidgets();
    m_values = Theme::defaults();
    const Theme::Values kept = Theme::reset(m_values);
    for (auto it = kept.constBegin(); it != kept.constEnd(); ++it)
        m_values.insert(it.key(), it.value());
    m_values.insert(QStringLiteral("FolderColorRules"), FolderColors::entries(m_rules));
    showValues();
    if (m_livePreview->isChecked())
        preview();
}

Theme::Values ThemeGroup::values() const
{
    auto *self = const_cast<ThemeGroup *>(this);
    self->readWidgets();
    return Theme::sanitized(m_values);
}

void ThemeGroup::saveSettings()
{
    readWidgets();
    QStringList problems;
    const Theme::Values values = Theme::sanitized(m_values, &problems);
    SettingsStore::setTheme(*config(), values);
    SettingsStore::setThemeProfiles(*config(), m_profiles);
    SettingsStore::setFollowedTheme(*config(), m_followed);
    for (auto it = m_accountColors.constBegin(); it != m_accountColors.constEnd(); ++it)
        SettingsStore::setAccountColor(*config(), it.key(), it->first->isChecked() ? it->second->color() : QColor());
    m_problems->setText(problems.join(QLatin1Char('\n')));
    m_problems->setVisible(!problems.isEmpty());
    m_saved = true;
}

const ThemeProfiles::Profile *ThemeGroup::selectedProfile() const
{
    const QListWidgetItem *item = m_profileList->currentItem();
    return item ? ThemeProfiles::find(m_profiles, item->data(Qt::UserRole).toString()) : nullptr;
}

void ThemeGroup::fillProfiles()
{
    const QString current =
        m_profileList->currentItem() ? m_profileList->currentItem()->data(Qt::UserRole).toString() : QString();
    m_profileList->clear();
    for (const ThemeProfiles::Profile &profile : std::as_const(m_profiles)) {
        auto *item = new QListWidgetItem(profile.name, m_profileList);
        item->setData(Qt::UserRole, profile.id);
        if (profile.id == current)
            m_profileList->setCurrentItem(item);
    }
}

void ThemeGroup::addProfile(ThemeProfiles::Profile profile)
{
    const qsizetype existing = ThemeProfiles::indexOfName(m_profiles, profile.name);
    if (existing >= 0) {
        if (m_context.confirm &&
            !m_context.confirm(this, i18nc("@title:window", "Replace Profile"),
                               i18nc("@info", "A profile called %1 exists. Replace it?", profile.name)))
            return;
        profile.id = m_profiles.at(existing).id;
        m_profiles[existing] = profile;
    } else {
        profile.id = ThemeProfiles::newId(m_profiles);
        QList<ThemeProfiles::Profile> candidate = m_profiles;
        candidate.append(profile);
        const QString error = ThemeProfiles::listError(candidate);
        if (!error.isEmpty()) {
            m_profileStatus->setText(error);
            return;
        }
        m_profiles = candidate;
    }
    fillProfiles();
    for (int row = 0; row < m_profileList->count(); ++row) {
        if (m_profileList->item(row)->data(Qt::UserRole).toString() == profile.id)
            m_profileList->setCurrentRow(row);
    }
}

void ThemeGroup::saveProfile()
{
    const QString name = m_profileName->text().trimmed();
    const QString error = ThemeProfiles::nameError(name);
    if (!error.isEmpty()) {
        m_profileStatus->setText(error);
        return;
    }
    readWidgets();
    addProfile(ThemeProfiles::fromValues(name, Theme::sanitized(m_values)));
    m_profileStatus->setText(i18nc("@info", "The profile %1 holds the options shown now.", name));
}

void ThemeGroup::useProfile()
{
    const ThemeProfiles::Profile *profile = selectedProfile();
    if (!profile)
        return;
    readWidgets();
    m_values = ThemeProfiles::appliedTo(m_values, *profile);
    showValues();
    m_profileStatus->setText(i18nc("@info", "The options of %1 are shown; save to use them.", profile->name));
    if (m_livePreview->isChecked())
        preview();
}

void ThemeGroup::deleteProfile()
{
    const ThemeProfiles::Profile *profile = selectedProfile();
    if (!profile)
        return;
    const QString id = profile->id;
    m_profiles.removeIf([&id](const ThemeProfiles::Profile &known) { return known.id == id; });
    fillProfiles();
    m_profileStatus->setText(i18nc("@info",
                                   "Workspaces using the deleted profile report it as "
                                   "missing and keep their own options."));
}

void ThemeGroup::importProfile()
{
    if (!m_context.chooseFile)
        return;
    const QString path = m_context.chooseFile(this, i18nc("@title:window", "Import Theme Profile"),
                                              SettingsContext::FileMode::Open, QString());
    if (path.isEmpty())
        return;
    const ThemeProfiles::ParseResult result = ThemeProfiles::importFile(path);
    if (!result.profile) {
        m_profileStatus->setText(result.error);
        return;
    }
    addProfile(*result.profile);
    QStringList lines{result.fromOpus ? i18nc("@info", "Imported %1 from Plasma Opus.", result.profile->name)
                                      : i18nc("@info", "Imported %1.", result.profile->name)};
    lines += result.warnings;
    m_profileStatus->setText(lines.join(QLatin1Char('\n')));
}

void ThemeGroup::exportProfile()
{
    if (!m_context.chooseFile)
        return;
    readWidgets();
    ThemeProfiles::Profile profile;
    if (const ThemeProfiles::Profile *selected = selectedProfile())
        profile = *selected;
    else
        profile = ThemeProfiles::fromValues(QStringLiteral("Plasma Impresario"), Theme::sanitized(m_values));
    const QString path =
        m_context.chooseFile(this, i18nc("@title:window", "Export Theme Profile"), SettingsContext::FileMode::Save,
                             profile.name + QLatin1String(".theme.json"));
    if (path.isEmpty())
        return;
    const auto target = m_exportForOpus->isChecked() ? ThemeProfiles::Target::Opus : ThemeProfiles::Target::Impresario;
    const QString error = ThemeProfiles::exportFile(profile, path, target, m_exportStyleSheet->isChecked());
    m_profileStatus->setText(error.isEmpty() ? i18nc("@info", "Exported %1.", profile.name) : error);
}

void ThemeGroup::followTheme()
{
    if (!m_context.chooseFile)
        return;
    const QString path = m_context.chooseFile(this, i18nc("@title:window", "Follow Theme File"),
                                              SettingsContext::FileMode::Open, m_followed);
    if (path.isEmpty())
        return;
    const ThemeProfiles::ParseResult result = ThemeProfiles::importFile(path);
    if (!result.profile) {
        m_profileStatus->setText(result.error);
        return;
    }
    m_followed = path;
    m_followedFile->setText(path);
    readWidgets();
    m_values = ThemeProfiles::appliedTo(m_values, *result.profile);
    showValues();
    m_profileStatus->setText(i18nc("@info", "The options of the file are shown and follow its changes once saved."));
    if (m_livePreview->isChecked())
        preview();
}

void ThemeGroup::fillRules()
{
    m_ruleList->clear();
    for (const FolderColors::Rule &rule : std::as_const(m_rules)) {
        auto *item = new QListWidgetItem(ruleText(rule, m_context), m_ruleList);
        QPixmap swatch(16, 16);
        swatch.fill(rule.color);
        item->setIcon(QIcon(swatch));
    }
}

void ThemeGroup::addRule()
{
    FolderColors::Rule rule;
    rule.account = m_ruleAccount->currentData().toString();
    rule.match = static_cast<FolderColors::Match>(m_ruleMatch->currentData().toInt());
    rule.pattern = m_rulePattern->text().trimmed();
    rule.color = m_ruleColor->color();
    rule.subfolders = m_ruleSubfolders->isChecked();
    if (rule.pattern.isEmpty() || m_rules.contains(rule) || m_rules.size() >= FolderColors::MaximumRules ||
        !FolderColors::parse(FolderColors::entry(rule)))
        return;
    m_rules.append(rule);
    m_rulePattern->clear();
    fillRules();
    onChanged();
}

void ThemeGroup::removeRule()
{
    const int row = m_ruleList->currentRow();
    if (row < 0 || row >= m_rules.size())
        return;
    m_rules.removeAt(row);
    fillRules();
    onChanged();
}

void ThemeGroup::checkStyleSheet()
{
    readWidgets();
    const QString error = UiUtils::StyleSheets::check(m_values.value(QStringLiteral("CustomStyleSheet")).toString());
    m_styleSheetStatus->setText(error.isEmpty() ? i18nc("@info", "The stylesheet can be applied.") : error);
}

void ThemeGroup::importStyleSheet()
{
    if (!m_context.chooseFile)
        return;
    const QString path = m_context.chooseFile(this, i18nc("@title:window", "Import Stylesheet"),
                                              SettingsContext::FileMode::Open, QString());
    if (path.isEmpty())
        return;
    QString error;
    const std::optional<QString> sheet = UiUtils::StyleSheets::readFile(path, &error);
    if (!sheet) {
        m_styleSheetStatus->setText(error);
        return;
    }
    static_cast<QPlainTextEdit *>(m_editors.value(QStringLiteral("CustomStyleSheet")))->setPlainText(*sheet);
    checkStyleSheet();
}

void ThemeGroup::exportStyleSheet()
{
    if (!m_context.chooseFile)
        return;
    const QString path = m_context.chooseFile(this, i18nc("@title:window", "Export Stylesheet"),
                                              SettingsContext::FileMode::Save, QStringLiteral("plasma-impresario.qss"));
    if (path.isEmpty())
        return;
    readWidgets();
    const QString error =
        UiUtils::StyleSheets::writeFile(path, m_values.value(QStringLiteral("CustomStyleSheet")).toString());
    m_styleSheetStatus->setText(error.isEmpty() ? i18nc("@info", "Exported the stylesheet.") : error);
}

} // namespace Gui
