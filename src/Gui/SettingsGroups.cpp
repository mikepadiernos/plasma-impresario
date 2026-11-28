/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/SettingsGroups.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <KConfigGroup>
#include <KLocalizedString>

#include "Common/SettingsNames.h"
#include "Gui/ThemeGroup.h"
#include "UiUtils/IconLoader.h"
#include "UiUtils/MailTabs.h"
#include "UiUtils/SettingsStore.h"

namespace Gui {

using namespace UiUtils;
using SettingsSchema::Group;

namespace {

/** @short A widget of the General page, which a group shows instead */
template <typename T> T *take(GeneralPage *general, const char *name)
{
    T *widget = general->findChild<T *>(QLatin1String(name));
    Q_ASSERT(widget);
    return widget;
}

QWidget *row(std::initializer_list<QWidget *> widgets)
{
    auto *container = new QWidget;
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    for (QWidget *widget : widgets)
        layout->addWidget(widget);
    layout->addStretch();
    return container;
}

void makeSmall(QLabel *label)
{
    label->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
}

QString commandItem(const QString &item)
{
    if (item == QLatin1String("vimMode"))
        return i18nc("@item keymap setting", "Vim mode");
    if (item == QLatin1String("hints"))
        return i18nc("@item keymap setting", "Key hints");
    if (item == QLatin1String("hintDelay"))
        return i18nc("@item keymap setting", "Hint delay");
    if (item == QLatin1String("timeout"))
        return i18nc("@item keymap setting", "Key timeout");
    return Keymap::description(item);
}

} // namespace

QString settingLabel(const QString &key)
{
    if (key == QLatin1String("Shell/StartFolder"))
        return i18nc("@option:check override", "Folder shown first");
    if (key == QLatin1String("Reading/AutoMarkRead"))
        return i18nc("@option:check override", "Mark messages as read when shown");
    if (key == QLatin1String("Reading/MarkReadDelay"))
        return i18nc("@option:check override", "Delay before marking as read");
    if (key == QLatin1String("Reading/PreferPlainText"))
        return i18nc("@option:check override", "Show plain text instead of HTML");
    return key;
}

SettingsGroupPage::SettingsGroupPage(Group group, const SettingsContext &context, QWidget *parent)
    : QScrollArea(parent)
    , m_context(context)
    , m_group(group)
{
    setObjectName(QStringLiteral("settingsGroup-") + SettingsSchema::groupId(group));
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    auto *inner = new QWidget(this);
    m_contents = new QVBoxLayout(inner);
    setWidget(inner);
}

Group SettingsGroupPage::group() const
{
    return m_group;
}

void SettingsGroupPage::save(QSettings &s)
{
    Q_UNUSED(s);
    saveSettings();
    config()->sync();
    emit saved();
}

QWidget *SettingsGroupPage::asWidget()
{
    return this;
}

bool SettingsGroupPage::checkValidity() const
{
    return true;
}

bool SettingsGroupPage::passwordFailures(QString &message) const
{
    Q_UNUSED(message);
    return false;
}

void SettingsGroupPage::updateWidgets()
{
    emit widgetsUpdated();
}

QVBoxLayout *SettingsGroupPage::contents() const
{
    return m_contents;
}

QFormLayout *SettingsGroupPage::addSection(const QString &title)
{
    if (m_contents->count() > 0)
        m_contents->addSpacing(m_contents->spacing() * 2);
    auto *heading = new QLabel(title);
    QFont font = heading->font();
    font.setBold(true);
    heading->setFont(font);
    m_contents->addWidget(heading);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_contents->addLayout(form);
    return form;
}

QLabel *SettingsGroupPage::addNote(QFormLayout *form, const QString &text)
{
    auto *note = new QLabel(text);
    makeSmall(note);
    form->addRow(note);
    return note;
}

KSharedConfig::Ptr SettingsGroupPage::config() const
{
    return KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
}

QVariant SettingsGroupPage::application(const QString &key) const
{
    return SettingsStore::value(*config(), m_context.legacy, key);
}

void SettingsGroupPage::setApplication(const QString &key, const QVariant &value)
{
    SettingsStore::setApplication(*config(), m_context.legacy, key, value);
}

GeneralGroup::GeneralGroup(GeneralPage *general, const SettingsContext &context, QWidget *parent)
    : SettingsGroupPage(Group::General, context, parent)
{
    QFormLayout *starting = addSection(i18nc("@title:group", "Starting and Closing"));
    starting->addRow(take<QCheckBox>(general, "guiStartMinimizedCheckbox"));
    starting->addRow(take<QCheckBox>(general, "guiRunInBackgroundCheckbox"));
    starting->addRow(take<QCheckBox>(general, "guiStartAfterLoginCheckbox"));
    QFormLayout *desktop = addSection(i18nc("@title:group", "Desktop"));
    desktop->addRow(take<QCheckBox>(general, "guiSystrayCheckbox"));
    contents()->addStretch();
}

void GeneralGroup::saveSettings()
{
    // The General page saves these
}

AccountsGroup::AccountsGroup(const SettingsContext &context, QWidget *parent)
    : SettingsGroupPage(Group::Accounts, context, parent)
    , m_account(new QComboBox)
    , m_scope(new QComboBox)
{
    m_account->setObjectName(QStringLiteral("overrideAccount"));
    m_scope->setObjectName(QStringLiteral("overrideScope"));
    for (const auto &account : context.accounts)
        m_account->addItem(account.name, account.id);
    QFormLayout *form = addSection(i18nc("@title:group", "Settings of an Account or Folder"));
    form->addRow(i18nc("@label:listbox", "Account:"), m_account);
    form->addRow(i18nc("@label:listbox", "Settings for:"), m_scope);
    addNote(form,
            i18nc("@info",
                  "A setting chosen here replaces the application's "
                  "setting for this account, or for one of its "
                  "folders. Settings which are not replaced are "
                  "inherited; each says where its value comes from."));
    m_rowsWidget = new QWidget;
    m_rows = new QGridLayout(m_rowsWidget);
    m_rows->setContentsMargins(0, 0, 0, 0);
    m_rows->setColumnStretch(1, 1);
    contents()->addWidget(m_rowsWidget);
    m_empty = new QLabel(i18nc("@info", "Add an account to give it settings of its own."));
    m_empty->setWordWrap(true);
    contents()->addWidget(m_empty);
    contents()->addStretch();
    connect(m_account, &QComboBox::currentIndexChanged, this, &AccountsGroup::fillScopes);
    connect(m_scope, &QComboBox::currentIndexChanged, this, &AccountsGroup::showOverrides);
    fillScopes();
}

void AccountsGroup::fillScopes()
{
    const QSignalBlocker blocker(m_scope);
    m_scope->clear();
    const QString accountId = m_account->currentData().toString();
    m_scope->addItem(i18nc("@item:inlistbox settings for", "The whole account"), QString());
    QStringList folders;
    if (!accountId.isEmpty()) {
        if (m_context.folders)
            folders = m_context.folders(accountId);
        folders += SettingsStore::folders(*config(), accountId);
        for (auto it = m_pending.constBegin(); it != m_pending.constEnd(); ++it) {
            if (it.key().first == accountId && !it.key().second.isEmpty())
                folders.append(it.key().second);
        }
    }
    folders.removeAll(QString());
    folders.removeDuplicates();
    folders.sort(Qt::CaseInsensitive);
    for (const QString &folder : std::as_const(folders))
        m_scope->addItem(i18nc("@item:inlistbox settings for %1 a folder", "Folder %1", folder), folder);
    showOverrides();
}

SettingsSchema::Values AccountsGroup::storedValues(const Target &target) const
{
    if (target.second.isEmpty())
        return SettingsStore::accountValues(*config(), target.first);
    return SettingsStore::folderValues(*config(), target.first, target.second);
}

SettingsSchema::Values AccountsGroup::values(const Target &target) const
{
    SettingsSchema::Values result = storedValues(target);
    const SettingsSchema::Values pending = m_pending.value(target);
    for (auto it = pending.constBegin(); it != pending.constEnd(); ++it) {
        if (it.value().isValid())
            result.insert(it.key(), it.value());
        else
            result.remove(it.key());
    }
    return result;
}

void AccountsGroup::setPending(const QString &key, const QVariant &value)
{
    const Target target {m_account->currentData().toString(), m_scope->currentData().toString()};
    if (target.first.isEmpty())
        return;
    m_pending[target].insert(key, value);
}

QWidget *AccountsGroup::editorFor(const SettingsSchema::Definition &definition, const QVariant &value)
{
    switch (definition.defaultValue.metaType().id()) {
    case QMetaType::Bool: {
        auto *combo = new QComboBox;
        combo->addItem(i18nc("@item:inlistbox setting value", "On"), true);
        combo->addItem(i18nc("@item:inlistbox setting value", "Off"), false);
        combo->setCurrentIndex(value.toBool() ? 0 : 1);
        return combo;
    }
    case QMetaType::Int: {
        auto *spin = new QSpinBox;
        spin->setRange(0, 60);
        spin->setSuffix(i18nc("@item:valuesuffix seconds", " s"));
        spin->setSpecialValueText(i18nc("@item:inlistbox no delay", "Immediately"));
        spin->setValue(value.toInt());
        return spin;
    }
    default: {
        auto *combo = new QComboBox;
        combo->addItem(i18nc("@item:inlistbox start folder", "The folder shown last"), QString());
        QStringList folders;
        if (m_context.folders)
            folders = m_context.folders(m_account->currentData().toString());
        const QString current = value.toString();
        if (!current.isEmpty() && !folders.contains(current))
            folders.append(current);
        folders.sort(Qt::CaseInsensitive);
        for (const QString &folder : std::as_const(folders))
            combo->addItem(folder, folder);
        combo->setCurrentIndex(std::max(0, combo->findData(current)));
        return combo;
    }
    }
}

QVariant AccountsGroup::editorValue(const SettingsSchema::Definition &definition, QWidget *editor) const
{
    if (auto *spin = qobject_cast<QSpinBox *>(editor))
        return spin->value();
    if (auto *combo = qobject_cast<QComboBox *>(editor)) {
        if (definition.defaultValue.metaType().id() == QMetaType::Bool)
            return combo->currentData().toBool();
        return combo->currentData().toString();
    }
    return {};
}

void AccountsGroup::showOverrides()
{
    while (QLayoutItem *item = m_rows->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    const bool hasAccount = m_account->count() > 0;
    m_empty->setVisible(!hasAccount);
    m_account->setEnabled(hasAccount);
    m_scope->setEnabled(hasAccount);
    if (!hasAccount)
        return;

    using SettingsSchema::Layer;
    const QString accountId = m_account->currentData().toString();
    const QString accountName = m_account->currentText();
    const QString folder = m_scope->currentData().toString();
    const Layer layer = folder.isEmpty() ? Layer::Account : Layer::Folder;
    SettingsSchema::Layers layers;
    layers.application = SettingsStore::applicationValues(*config(), m_context.legacy);
    layers.account = values({accountId, QString()});
    if (!folder.isEmpty())
        layers.folder = values({accountId, folder});
    const SettingsSchema::Values own = folder.isEmpty() ? layers.account : layers.folder;

    int line = 0;
    const auto keys
        = SettingsSchema::keys(folder.isEmpty() ? SettingsSchema::Scope::Account : SettingsSchema::Scope::Folder);
    for (const QString &key : keys) {
        const SettingsSchema::Definition *definition = SettingsSchema::definition(key);
        if (!definition)
            continue;
        const SettingsSchema::Resolved inherited = SettingsSchema::inherited(key, layers, layer);
        const bool overridden = own.contains(key);
        auto *check = new QCheckBox(settingLabel(key));
        check->setObjectName(QStringLiteral("override-") + key);
        check->setChecked(overridden);
        QWidget *editor = editorFor(*definition,
                                    overridden ? SettingsSchema::coerce(*definition, own.value(key)) : inherited.value);
        editor->setObjectName(QStringLiteral("overrideValue-") + key);
        editor->setAccessibleName(settingLabel(key));
        editor->setEnabled(overridden);
        const QString inheritedText = SettingsSchema::explanation(inherited.layer, accountName, folder);
        const QString ownText
            = folder.isEmpty() ? i18nc("@info", "Set for this account") : i18nc("@info", "Set for this folder");
        auto *note = new QLabel(overridden ? ownText : inheritedText);
        note->setObjectName(QStringLiteral("overrideNote-") + key);
        makeSmall(note);
        m_rows->addWidget(check, line, 0);
        m_rows->addWidget(editor, line, 1, Qt::AlignLeft);
        m_rows->addWidget(note, line + 1, 1);
        line += 2;

        const QVariant inheritedValue = inherited.value;
        const auto changed = [this, key, definition, editor, check]() {
            if (check->isChecked())
                setPending(key, editorValue(*definition, editor));
        };
        if (auto *combo = qobject_cast<QComboBox *>(editor))
            connect(combo, &QComboBox::currentIndexChanged, this, changed);
        if (auto *spin = qobject_cast<QSpinBox *>(editor))
            connect(spin, &QSpinBox::valueChanged, this, changed);
        connect(check, &QAbstractButton::toggled, this,
                [this, key, definition, editor, note, ownText, inheritedText, inheritedValue](bool on) {
                    editor->setEnabled(on);
                    note->setText(on ? ownText : inheritedText);
                    if (on) {
                        setPending(key, editorValue(*definition, editor));
                        return;
                    }
                    setPending(key, QVariant());
                    // Show what applies again without the override
                    const QSignalBlocker blocker(editor);
                    if (auto *combo = qobject_cast<QComboBox *>(editor))
                        combo->setCurrentIndex(std::max(0, combo->findData(inheritedValue)));
                    if (auto *spin = qobject_cast<QSpinBox *>(editor))
                        spin->setValue(inheritedValue.toInt());
                });
    }
    updateWidgets();
}

void AccountsGroup::saveSettings()
{
    for (auto it = m_pending.constBegin(); it != m_pending.constEnd(); ++it) {
        const auto &[accountId, folder] = it.key();
        for (auto value = it.value().constBegin(); value != it.value().constEnd(); ++value) {
            if (folder.isEmpty())
                SettingsStore::setAccountValue(*config(), accountId, value.key(), value.value());
            else
                SettingsStore::setFolderValue(*config(), accountId, folder, value.key(), value.value());
        }
    }
    m_pending.clear();
}

AppearanceGroup::AppearanceGroup(FavoriteTagsPage *favoriteTags, const SettingsContext &context, QWidget *parent)
    : SettingsGroupPage(Group::Appearance, context, parent)
    , m_preset(new QComboBox)
    , m_sidebar(new QComboBox)
    , m_header(new QCheckBox(i18nc("@option:check", "Show the header above the window's columns")))
    , m_symbolicTray(new QCheckBox(i18nc("@option:check", "Use a monochrome icon in the system tray")))
    , m_tabs(new QCheckBox(i18nc("@option:check", "Open folders and messages in tabs")))
    , m_tabRegion(new QComboBox)
    , m_tabOrientation(new QComboBox)
{
    m_preset->setObjectName(QStringLiteral("layoutPreset"));
    m_sidebar->setObjectName(QStringLiteral("sidebarPosition"));
    m_header->setObjectName(QStringLiteral("showHeader"));
    m_symbolicTray->setObjectName(QStringLiteral("symbolicTrayIcon"));
    m_tabs->setObjectName(QStringLiteral("tabsEnabled"));
    m_tabRegion->setObjectName(QStringLiteral("tabsRegion"));
    m_tabOrientation->setObjectName(QStringLiteral("tabsOrientation"));

    auto *tabs = new QTabWidget;
    tabs->setObjectName(QStringLiteral("appearanceTabs"));
    auto *layoutPage = new QWidget;
    auto *page = new QVBoxLayout(layoutPage);
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    page->addLayout(form);

    using SettingsSchema::LayoutPreset;
    m_preset->addItem(i18nc("@item:inlistbox layout preset", "Compact"),
                      SettingsSchema::configName(LayoutPreset::Compact));
    m_preset->addItem(i18nc("@item:inlistbox layout preset", "Comfortable"),
                      SettingsSchema::configName(LayoutPreset::Comfortable));
    m_preset->addItem(i18nc("@item:inlistbox layout preset", "Focus"), SettingsSchema::configName(LayoutPreset::Focus));
    const LayoutPreset preset = SettingsSchema::layoutPreset(
        application(QStringLiteral("Appearance/RowDensity")).toString() == QLatin1String("comfortable"),
        application(QStringLiteral("Appearance/FocusLayout")).toBool());
    m_preset->setCurrentIndex(m_preset->findData(SettingsSchema::configName(preset)));
    form->addRow(i18nc("@label:listbox", "Layout:"), m_preset);
    auto *presetNote = new QLabel(i18nc("@info",
                                        "Compact fits the most rows. Comfortable gives rows more room. Focus "
                                        "also "
                                        "hides the side columns and the footer to leave the reader more space."));
    makeSmall(presetNote);
    form->addRow(QString(), presetNote);

    KConfigGroup appearance(config(), QStringLiteral("Appearance"));
    m_sidebar->addItem(i18nc("@item:inlistbox folder sidebar position", "Left"), QStringLiteral("left"));
    m_sidebar->addItem(i18nc("@item:inlistbox folder sidebar position", "Right"), QStringLiteral("right"));
    m_sidebar->setCurrentIndex(
        std::max(0, m_sidebar->findData(appearance.readEntry("MailboxSidebarPosition", QStringLiteral("left")))));
    form->addRow(i18nc("@label:listbox", "Folder sidebar:"), m_sidebar);
    m_header->setChecked(appearance.readEntry("ShowHeader", false));
    form->addRow(QString(), m_header);
    m_symbolicTray->setChecked(application(QStringLiteral("Appearance/SymbolicTrayIcon")).toBool());
    m_symbolicTray->setToolTip(i18nc("@info:tooltip",
                                     "Show the symbolic icon of the active icon theme, coloured like the other "
                                     "icons in the panel; the number of unread messages is in its tooltip"));
    form->addRow(i18nc("@label", "System tray:"), m_symbolicTray);

    m_tabs->setChecked(application(QStringLiteral("Tabs/Enabled")).toBool());
    form->addRow(i18nc("@label", "Tabs:"), m_tabs);
    const QString region = application(QStringLiteral("Tabs/Region")).toString();
    for (MailTabs::Region value : MailTabs::regions()) {
        QString name;
        switch (value) {
        case MailTabs::Region::Header:
            name = i18nc("@item:inlistbox shell region", "Header");
            break;
        case MailTabs::Region::Left:
            name = i18nc("@item:inlistbox shell region", "Left column");
            break;
        case MailTabs::Region::Main:
            name = i18nc("@item:inlistbox shell region", "Above the message list");
            break;
        case MailTabs::Region::Right:
            name = i18nc("@item:inlistbox shell region", "Right column");
            break;
        case MailTabs::Region::Footer:
            name = i18nc("@item:inlistbox shell region", "Footer");
            break;
        }
        m_tabRegion->addItem(name, MailTabs::configName(value));
    }
    m_tabRegion->setCurrentIndex(std::max(0, m_tabRegion->findData(region)));
    form->addRow(i18nc("@label:listbox", "Tab bar in:"), m_tabRegion);
    m_tabOrientation->addItem(i18nc("@item:inlistbox tab bar orientation", "Horizontal"), QStringLiteral("horizontal"));
    m_tabOrientation->addItem(i18nc("@item:inlistbox tab bar orientation", "Vertical"), QStringLiteral("vertical"));
    m_tabOrientation->setCurrentIndex(
        std::max(0, m_tabOrientation->findData(application(QStringLiteral("Tabs/Orientation")).toString())));
    form->addRow(i18nc("@label:listbox", "Tab bar orientation:"), m_tabOrientation);
    auto *tabsNote = new QLabel(i18nc("@info",
                                      "A tab keeps a folder and the message open in it. Messages are written "
                                      "in "
                                      "their own windows, so closing tabs or turning them off loses nothing."));
    makeSmall(tabsNote);
    form->addRow(QString(), tabsNote);
    page->addStretch();
    connect(m_tabs, &QAbstractButton::toggled, this, &AppearanceGroup::updateTabs);
    updateTabs();

    tabs->addTab(layoutPage, i18nc("@title:tab", "Layout"));
    tabs->addTab(favoriteTags, i18nc("@title:tab", "Favorite Tags"));
    contents()->addWidget(tabs);
}

void AppearanceGroup::updateTabs()
{
    m_tabRegion->setEnabled(m_tabs->isChecked());
    m_tabOrientation->setEnabled(m_tabs->isChecked());
}

void AppearanceGroup::saveSettings()
{
    const auto preset = SettingsSchema::layoutPresetFromConfigName(m_preset->currentData().toString());
    setApplication(QStringLiteral("Appearance/RowDensity"),
                   SettingsSchema::comfortableRows(preset) ? QStringLiteral("comfortable") : QStringLiteral("compact"));
    setApplication(QStringLiteral("Appearance/FocusLayout"), SettingsSchema::focusLayout(preset));
    setApplication(QStringLiteral("Appearance/SymbolicTrayIcon"), m_symbolicTray->isChecked());
    KConfigGroup appearance(config(), QStringLiteral("Appearance"));
    appearance.writeEntry("MailboxSidebarPosition", m_sidebar->currentData().toString());
    appearance.writeEntry("ShowHeader", m_header->isChecked());
    setApplication(QStringLiteral("Tabs/Enabled"), m_tabs->isChecked());
    setApplication(QStringLiteral("Tabs/Region"), m_tabRegion->currentData().toString());
    setApplication(QStringLiteral("Tabs/Orientation"), m_tabOrientation->currentData().toString());
}

ReadingGroup::ReadingGroup(GeneralPage *general, const SettingsContext &context, QWidget *parent)
    : SettingsGroupPage(Group::Reading, context, parent)
{
    QFormLayout *reading = addSection(i18nc("@title:group", "Reading Messages"));
    auto *markReadSeconds = take<QSpinBox>(general, "markReadSeconds");
    markReadSeconds->setAccessibleName(i18nc("@label:spinbox accessible name", "Seconds before marking as read"));
    reading->addRow(row({take<QCheckBox>(general, "markReadCheckbox"), markReadSeconds}));
    reading->addRow(take<QCheckBox>(general, "preferPlaintextCheckbox"));
    addNote(reading,
            i18nc("@info",
                  "An account or a folder can replace these "
                  "settings on the Accounts page."));
    QFormLayout *folders = addSection(i18nc("@title:group", "Folder Sidebar"));
    folders->addRow(
        row({take<QCheckBox>(general, "mboxDropActionCheckbox"), take<QComboBox>(general, "mboxDropActionBox")}));
    contents()->addStretch();
}

void ReadingGroup::saveSettings()
{
    // The General page saves these
}

CompositionGroup::CompositionGroup(GeneralPage *general, const SettingsContext &context, QWidget *parent)
    : SettingsGroupPage(Group::Composition, context, parent)
{
    QFormLayout *form = addSection(i18nc("@title:group", "Writing Messages"));
    auto *spellLabel = take<QLabel>(general, "spellcheckerLabel");
    spellLabel->setText(i18nc("@label:listbox", "Spell checking:"));
    form->addRow(spellLabel, take<QComboBox>(general, "spellcheckerBox"));
    auto *addressLabel = take<QLabel>(general, "addressbookLabel");
    addressLabel->setText(i18nc("@label:listbox", "Address book:"));
    form->addRow(addressLabel, take<QComboBox>(general, "addressbookBox"));
    addNote(form,
            i18nc("@info",
                  "Identities and signatures belong to each "
                  "account; see Account Settings."));
    contents()->addStretch();
}

void CompositionGroup::saveSettings()
{
    // The General page saves these
}

PrivacyGroup::PrivacyGroup(GeneralPage *general, const SettingsContext &context, QWidget *parent)
    : SettingsGroupPage(Group::Privacy, context, parent)
{
    QFormLayout *sharing = addSection(i18nc("@title:group", "What Is Shared"));
    sharing->addRow(take<QCheckBox>(general, "revealImpresarioVersions"));
    sharing->addRow(take<QCheckBox>(general, "guiPlasmaSearchCheckbox"));
    sharing->addRow(take<QLabel>(general, "guiPlasmaWidgetsLabel"), take<QComboBox>(general, "guiPlasmaWidgetsBox"));
    QFormLayout *safety = addSection(i18nc("@title:group", "Safety"));
    safety->addRow(take<QCheckBox>(general, "guiScanAttachmentsCheckbox"));
    safety->addRow(take<QCheckBox>(general, "guiMoveJunkCheckbox"));
    QFormLayout *passwords = addSection(i18nc("@title:group", "Passwords"));
    auto *passwordLabel = take<QLabel>(general, "passwordLabel");
    passwordLabel->setText(i18nc("@label:listbox", "Keep passwords in:"));
    passwords->addRow(passwordLabel, take<QComboBox>(general, "passwordBox"));
    contents()->addStretch();
}

void PrivacyGroup::saveSettings()
{
    // The General page saves these
}

NotificationsGroup::NotificationsGroup(GeneralPage *general, const SettingsContext &context, QWidget *parent)
    : SettingsGroupPage(Group::Notifications, context, parent)
{
    QFormLayout *form = addSection(i18nc("@title:group", "New Mail"));
    form->addRow(take<QCheckBox>(general, "guiUnreadCountCheckbox"));
    auto *configure = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("preferences-desktop-notification")),
                                      i18nc("@action:button", "Configure Notifications…"));
    configure->setObjectName(QStringLiteral("configureNotifications"));
    configure->setEnabled(bool(context.configureNotifications));
    connect(configure, &QPushButton::clicked, this, [this]() {
        if (m_context.configureNotifications)
            m_context.configureNotifications();
    });
    form->addRow(row({configure}));
    addNote(form,
            i18nc("@info",
                  "Which accounts and folders announce new mail, "
                  "quiet hours and the sound are chosen in "
                  "Configure Notifications."));
    contents()->addStretch();
}

void NotificationsGroup::saveSettings()
{
    // The General page saves the unread count, and notifications are saved by
    // their own dialog
}

SearchGroup::SearchGroup(const SettingsContext &context, QWidget *parent)
    : SettingsGroupPage(Group::Search, context, parent)
    , m_places(new QComboBox)
    , m_useIndex(new QComboBox)
    , m_historyLimit(new QSpinBox)
    , m_remember(new QCheckBox(i18nc("@option:check", "Remember searches")))
    , m_suggest(new QCheckBox(i18nc("@option:check", "Suggest while typing")))
    , m_accounts(new QTableWidget)
{
    m_places->setObjectName(QStringLiteral("searchPlaces"));
    m_useIndex->setObjectName(QStringLiteral("searchUseIndex"));
    m_historyLimit->setObjectName(QStringLiteral("searchHistoryLimit"));
    m_remember->setObjectName(QStringLiteral("searchRemember"));
    m_suggest->setObjectName(QStringLiteral("searchSuggest"));
    m_accounts->setObjectName(QStringLiteral("searchIndexedAccounts"));
    m_accounts->setAccessibleName(i18nc("@label accessible name", "Offline index per account"));

    QFormLayout *searching = addSection(i18nc("@title:group", "Searching"));
    m_places->addItem(i18nc("@item:inlistbox search places", "The current folder"), QStringLiteral("current"));
    m_places->addItem(i18nc("@item:inlistbox search places", "The current account"), QStringLiteral("account"));
    m_places->addItem(i18nc("@item:inlistbox search places", "All accounts"), QStringLiteral("all"));
    searching->addRow(i18nc("@label:listbox", "Search in:"), m_places);
    m_useIndex->addItem(i18nc("@item:inlistbox search source", "The offline index when there is one"),
                        QStringLiteral("automatic"));
    m_useIndex->addItem(i18nc("@item:inlistbox search source", "The server while online"), QStringLiteral("server"));
    searching->addRow(i18nc("@label:listbox", "Search:"), m_useIndex);

    QFormLayout *history = addSection(i18nc("@title:group", "History"));
    m_historyLimit->setRange(1, 200);
    history->addRow(i18nc("@label:spinbox", "Searches kept:"), m_historyLimit);
    history->addRow(QString(), m_remember);
    history->addRow(QString(), m_suggest);
    auto *clear = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("edit-clear-history")),
                                  i18nc("@action:button", "Clear Search History"));
    clear->setObjectName(QStringLiteral("clearSearchHistory"));
    connect(clear, &QPushButton::clicked, this, [this]() {
        if (m_context.clearSearchHistory)
            m_context.clearSearchHistory();
    });
    history->addRow(QString(), row({clear}));

    QFormLayout *index = addSection(i18nc("@title:group", "Offline Index"));
    m_accounts->setColumnCount(3);
    m_accounts->setHorizontalHeaderLabels({i18nc("@title:column", "Account"), i18nc("@title:column", "Index Messages"),
                                           i18nc("@title:column", "Index Attachments")});
    m_accounts->verticalHeader()->hide();
    m_accounts->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_accounts->setSelectionMode(QAbstractItemView::NoSelection);
    m_accounts->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_accounts->setMinimumHeight(m_accounts->verticalHeader()->defaultSectionSize() * 4);
    index->addRow(m_accounts);
    connect(m_accounts, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (item->column() != 1)
            return;
        if (QTableWidgetItem *attachments = m_accounts->item(item->row(), 2)) {
            const Qt::ItemFlags flags = attachments->flags();
            attachments->setFlags(item->checkState() == Qt::Checked ? flags | Qt::ItemIsEnabled
                                                                    : flags & ~Qt::ItemIsEnabled);
        }
    });
    auto *manage = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("database-index")),
                                   i18nc("@action:button", "Manage Index…"));
    manage->setObjectName(QStringLiteral("manageSearchIndex"));
    connect(manage, &QPushButton::clicked, this, [this]() {
        if (m_context.manageSearchIndex)
            m_context.manageSearchIndex(this);
    });
    index->addRow(row({manage}));
    addNote(index,
            i18nc("@info",
                  "Manage Index pauses indexing, leaves folders "
                  "out and deletes the index of the current "
                  "account. Turning an index on or off takes "
                  "effect when the account connects again."));

    auto *reset = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("edit-reset")),
                                  i18nc("@action:button", "Reset Search Settings"));
    reset->setObjectName(QStringLiteral("resetSearch"));
    connect(reset, &QPushButton::clicked, this, [this]() {
        if (!m_context.confirm
            || !m_context.confirm(this, i18nc("@title:window", "Reset Search Settings"),
                                  i18nc("@info",
                                        "Go back to the default search "
                                        "settings? The search history and "
                                        "the offline indexes are kept.")))
            return;
        m_reset = true;
        const auto defaultOf
            = [](const char *key) { return SettingsSchema::definition(QLatin1String(key))->defaultValue; };
        m_places->setCurrentIndex(std::max(0, m_places->findData(defaultOf("Search/DefaultPlaces"))));
        m_useIndex->setCurrentIndex(std::max(0, m_useIndex->findData(defaultOf("Search/UseIndex"))));
        m_historyLimit->setValue(defaultOf("Search/HistoryLimit").toInt());
        m_remember->setChecked(true);
        m_suggest->setChecked(true);
    });
    contents()->addSpacing(contents()->spacing() * 2);
    contents()->addWidget(row({reset}));
    contents()->addStretch();
    load();
}

void SearchGroup::load()
{
    m_places->setCurrentIndex(std::max(0, m_places->findData(application(QStringLiteral("Search/DefaultPlaces")))));
    m_useIndex->setCurrentIndex(std::max(0, m_useIndex->findData(application(QStringLiteral("Search/UseIndex")))));
    m_historyLimit->setValue(application(QStringLiteral("Search/HistoryLimit")).toInt());
    const KConfigGroup quickSearch(config(), QStringLiteral("QuickSearch"));
    m_remember->setChecked(quickSearch.readEntry("RememberSearches", true));
    m_suggest->setChecked(quickSearch.readEntry("SuggestWhileTyping", true));

    const QSignalBlocker blocker(m_accounts);
    m_accounts->setRowCount(0);
    for (const auto &account : m_context.accounts) {
        QSettings *settings = m_context.accountSettings ? m_context.accountSettings(account.id) : nullptr;
        if (!settings)
            continue;
        const int line = m_accounts->rowCount();
        m_accounts->insertRow(line);
        auto *name = new QTableWidgetItem(account.name);
        name->setData(Qt::UserRole, account.id);
        name->setFlags(Qt::ItemIsEnabled);
        m_accounts->setItem(line, 0, name);
        const bool indexed = settings->value(Common::SettingsNames::searchIndexKey, false).toBool();
        auto *index = new QTableWidgetItem;
        index->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        index->setCheckState(indexed ? Qt::Checked : Qt::Unchecked);
        m_accounts->setItem(line, 1, index);
        auto *attachments = new QTableWidgetItem;
        attachments->setFlags(indexed ? Qt::ItemIsEnabled | Qt::ItemIsUserCheckable : Qt::ItemIsUserCheckable);
        attachments->setCheckState(settings->value(Common::SettingsNames::searchIndexAttachmentsKey, false).toBool()
                                       ? Qt::Checked
                                       : Qt::Unchecked);
        m_accounts->setItem(line, 2, attachments);
    }
}

void SearchGroup::saveSettings()
{
    if (m_reset)
        SettingsStore::resetSearch(*config());
    const auto write = [this](const QString &key, const QVariant &value) {
        // After a reset, defaults stay unset so that later defaults apply
        if (m_reset && SettingsSchema::definition(key)->defaultValue == value)
            return;
        setApplication(key, value);
    };
    write(QStringLiteral("Search/DefaultPlaces"), m_places->currentData().toString());
    write(QStringLiteral("Search/UseIndex"), m_useIndex->currentData().toString());
    write(QStringLiteral("Search/HistoryLimit"), m_historyLimit->value());
    KConfigGroup quickSearch(config(), QStringLiteral("QuickSearch"));
    if (!m_reset || !m_remember->isChecked())
        quickSearch.writeEntry("RememberSearches", m_remember->isChecked());
    if (!m_reset || !m_suggest->isChecked())
        quickSearch.writeEntry("SuggestWhileTyping", m_suggest->isChecked());
    m_reset = false;

    for (int line = 0; line < m_accounts->rowCount(); ++line) {
        const QString accountId = m_accounts->item(line, 0)->data(Qt::UserRole).toString();
        QSettings *settings = m_context.accountSettings ? m_context.accountSettings(accountId) : nullptr;
        if (!settings)
            continue;
        settings->setValue(Common::SettingsNames::searchIndexKey,
                           m_accounts->item(line, 1)->checkState() == Qt::Checked);
        settings->setValue(Common::SettingsNames::searchIndexAttachmentsKey,
                           m_accounts->item(line, 2)->checkState() == Qt::Checked);
        settings->sync();
    }
}

IntegrationsGroup::IntegrationsGroup(const SettingsContext &context, QWidget *parent)
    : SettingsGroupPage(Group::Integrations, context, parent)
    , m_nextcloud(new QListWidget)
    , m_connectNextcloud(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("list-add")),
                                         i18nc("@action:button", "Connect…")))
    , m_disconnectNextcloud(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("list-remove")),
                                            i18nc("@action:button", "Disconnect")))
    , m_nextcloudStatus(new QLabel)
    , m_calendar(new QCheckBox(i18nc("@option:check", "Offer to turn messages into calendar events")))
    , m_calendarReason(new QLabel)
    , m_notes(new QCheckBox(i18nc("@option:check", "Offer to save messages as notes")))
    , m_notesReason(new QLabel)
    , m_notesFolder(new QLineEdit)
    , m_workContext(new QCheckBox(i18nc("@option:check", "Save notes to the folder of the current workspace")))
{
    m_calendar->setObjectName(QStringLiteral("mailToCalendar"));
    m_calendarReason->setObjectName(QStringLiteral("mailToCalendarReason"));
    m_notes->setObjectName(QStringLiteral("mailToNotes"));
    m_notesReason->setObjectName(QStringLiteral("mailToNotesReason"));
    m_notesFolder->setObjectName(QStringLiteral("notesFolder"));
    m_notesFolder->setAccessibleName(i18nc("@label:textbox accessible name", "Notes folder"));
    m_workContext->setObjectName(QStringLiteral("workContext"));
    makeSmall(m_calendarReason);
    makeSmall(m_notesReason);

    QFormLayout *calendar = addSection(i18nc("@title:group", "Calendar"));
    calendar->addRow(m_calendar);
    calendar->addRow(m_calendarReason);
    addNote(calendar,
            i18nc("@info",
                  "The event opens in the calendar application, which "
                  "shows it before anything is saved."));

    QFormLayout *notes = addSection(i18nc("@title:group", "Notes"));
    notes->addRow(m_notes);
    notes->addRow(m_notesReason);
    m_notesFolder->setPlaceholderText(i18nc("@info:placeholder", "Choose the folder Marknote uses"));
    m_notesFolder->setClearButtonEnabled(true);
    auto *browse = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("document-open-folder")),
                                   i18nc("@action:button", "Choose…"));
    browse->setObjectName(QStringLiteral("chooseNotesFolder"));
    auto *folderRow = new QWidget;
    auto *folderLayout = new QHBoxLayout(folderRow);
    folderLayout->setContentsMargins(0, 0, 0, 0);
    folderLayout->addWidget(m_notesFolder, 1);
    folderLayout->addWidget(browse);
    notes->addRow(i18nc("@label:textbox", "Notes folder:"), folderRow);
    addNote(notes,
            i18nc("@info",
                  "A note is a Markdown file with the message's "
                  "sender, date, subject and text."));

    QFormLayout *workspaces = addSection(i18nc("@title:group", "Work Context"));
    workspaces->addRow(m_workContext);
    addNote(workspaces,
            i18nc("@info",
                  "Workspaces and their notes folders are "
                  "set up on the Workspaces page."));

    m_nextcloud->setObjectName(QStringLiteral("nextcloudAccountList"));
    m_nextcloud->setAccessibleName(i18nc("@label:listbox accessible name", "Nextcloud accounts"));
    m_nextcloud->setMaximumHeight(m_nextcloud->fontMetrics().height() * 6);
    m_connectNextcloud->setObjectName(QStringLiteral("connectNextcloud"));
    m_disconnectNextcloud->setObjectName(QStringLiteral("disconnectNextcloud"));
    m_nextcloudStatus->setObjectName(QStringLiteral("nextcloudStatus"));
    m_nextcloudStatus->setWordWrap(true);
    m_nextcloudStatus->setTextFormat(Qt::PlainText);
    m_nextcloudStatus->hide();
    QFormLayout *nextcloud = addSection(i18nc("@title:group", "Nextcloud"));
    nextcloud->addRow(m_nextcloud);
    auto *nextcloudButtons = new QWidget;
    auto *nextcloudButtonsLayout = new QHBoxLayout(nextcloudButtons);
    nextcloudButtonsLayout->setContentsMargins(0, 0, 0, 0);
    nextcloudButtonsLayout->addWidget(m_connectNextcloud);
    nextcloudButtonsLayout->addWidget(m_disconnectNextcloud);
    nextcloudButtonsLayout->addStretch();
    nextcloud->addRow(nextcloudButtons);
    nextcloud->addRow(m_nextcloudStatus);
    addNote(nextcloud,
            i18nc("@info",
                  "Connected accounts let you attach files from Nextcloud, save attachments there and "
                  "insert links to files you shared. Changes here take effect at once."));
    addNote(nextcloud, m_context.nextcloudOnlineAccounts
                           ? i18nc("@info",
                                   "Nextcloud accounts added in Online Accounts in System Settings are listed too; "
                                   "their passwords stay there.")
                           : i18nc("@info",
                                   "Online Accounts in System Settings is not available, so only the accounts "
                                   "connected here are listed."));
    contents()->addStretch();

    m_calendar->setChecked(application(QStringLiteral("Integrations/MailToCalendar")).toBool());
    m_notes->setChecked(application(QStringLiteral("Integrations/MailToNotes")).toBool());
    m_notesFolder->setText(application(QStringLiteral("Integrations/NotesFolder")).toString());
    m_workContext->setChecked(application(QStringLiteral("Integrations/WorkContext")).toBool());
    connect(m_notesFolder, &QLineEdit::textChanged, this, &IntegrationsGroup::updateAvailability);
    connect(browse, &QPushButton::clicked, this, [this]() {
        if (!m_context.chooseFile)
            return;
        const QString folder = m_context.chooseFile(this, i18nc("@title:window", "Notes Folder"),
                                                    SettingsContext::FileMode::Directory, m_notesFolder->text());
        if (!folder.isEmpty())
            m_notesFolder->setText(folder);
    });
    updateAvailability();

    connect(m_nextcloud, &QListWidget::currentRowChanged, this, &IntegrationsGroup::updateNextcloud);
    connect(m_connectNextcloud, &QPushButton::clicked, this, [this]() {
        if (!m_context.connectNextcloud)
            return;
        m_nextcloudStatus->hide();
        QPointer<IntegrationsGroup> self(this);
        m_context.connectNextcloud(this, [self]() {
            if (self)
                self->loadNextcloud();
        });
    });
    connect(m_disconnectNextcloud, &QPushButton::clicked, this, &IntegrationsGroup::disconnectNextcloud);
    loadNextcloud();
}

void IntegrationsGroup::loadNextcloud()
{
    const QString current = m_nextcloud->currentItem() ? m_nextcloud->currentItem()->data(Qt::UserRole).toString()
                                                       : QString();
    m_nextcloud->clear();
    const QList<SettingsContext::NextcloudAccount> accounts =
        m_context.nextcloudAccounts ? m_context.nextcloudAccounts() : QList<SettingsContext::NextcloudAccount>();
    for (const SettingsContext::NextcloudAccount &account : accounts) {
        auto *item = new QListWidgetItem(SymbolicIcon::fromTheme(QStringLiteral("folder-cloud")), account.title,
                                         m_nextcloud);
        item->setData(Qt::UserRole, account.id);
        item->setData(Qt::UserRole + 1, account.onlineAccount);
        if (account.onlineAccount) {
            item->setToolTip(i18nc("@info:tooltip", "This account comes from Online Accounts in System Settings, "
                                                    "where it can be removed."));
        }
        if (account.id == current)
            m_nextcloud->setCurrentItem(item);
    }
    if (!m_nextcloud->currentItem() && m_nextcloud->count() > 0)
        m_nextcloud->setCurrentRow(0);
    if (accounts.isEmpty()) {
        auto *item = new QListWidgetItem(i18nc("@info", "No account is connected."), m_nextcloud);
        item->setFlags(Qt::NoItemFlags);
    }
    updateNextcloud();
}

void IntegrationsGroup::updateNextcloud()
{
    const QListWidgetItem *item = m_nextcloud->currentItem();
    const bool removable = item && !item->data(Qt::UserRole).toString().isEmpty()
        && !item->data(Qt::UserRole + 1).toBool();
    m_connectNextcloud->setEnabled(bool(m_context.connectNextcloud));
    m_disconnectNextcloud->setEnabled(removable && m_context.disconnectNextcloud);
    m_disconnectNextcloud->setToolTip(item && item->data(Qt::UserRole + 1).toBool()
                                          ? i18nc("@info:tooltip", "Remove this account in Online Accounts in System Settings")
                                          : i18nc("@info:tooltip", "Forget the account and revoke its app password on the server"));
}

void IntegrationsGroup::disconnectNextcloud()
{
    const QListWidgetItem *item = m_nextcloud->currentItem();
    if (!item || !m_context.disconnectNextcloud)
        return;
    const QString id = item->data(Qt::UserRole).toString();
    if (id.isEmpty() || item->data(Qt::UserRole + 1).toBool())
        return;
    if (m_context.confirm
        && !m_context.confirm(this, i18nc("@title:window", "Disconnect Nextcloud Account"),
                              i18nc("@info",
                                    "Disconnect %1? Its app password is revoked on the server, and messages "
                                    "you already sent keep their links.",
                                    item->text()))) {
        return;
    }
    QPointer<IntegrationsGroup> self(this);
    m_disconnectNextcloud->setEnabled(false);
    m_nextcloudStatus->hide();
    m_context.disconnectNextcloud(this, id, [self](const QString &warning) {
        if (!self)
            return;
        self->loadNextcloud();
        self->m_nextcloudStatus->setText(warning);
        self->m_nextcloudStatus->setVisible(!warning.isEmpty());
    });
}

void IntegrationsGroup::updateAvailability()
{
    using SettingsSchema::Feature;
    const bool calendar = m_context.available.testFlag(Feature::Calendar);
    const bool notes = m_context.available.testFlag(Feature::Notes) || !m_notesFolder->text().trimmed().isEmpty();
    m_calendar->setEnabled(calendar);
    m_calendarReason->setText(calendar ? QString() : SettingsSchema::unavailableReason(Feature::Calendar));
    m_calendarReason->setVisible(!calendar);
    m_notes->setEnabled(notes);
    m_notesReason->setText(notes ? QString() : SettingsSchema::unavailableReason(Feature::Notes));
    m_notesReason->setVisible(!notes);
    m_workContext->setEnabled(notes);
}

void IntegrationsGroup::saveSettings()
{
    setApplication(QStringLiteral("Integrations/MailToCalendar"), m_calendar->isChecked());
    setApplication(QStringLiteral("Integrations/MailToNotes"), m_notes->isChecked());
    setApplication(QStringLiteral("Integrations/NotesFolder"), m_notesFolder->text().trimmed());
    setApplication(QStringLiteral("Integrations/WorkContext"), m_workContext->isChecked());
}

WorkspacesGroup::WorkspacesGroup(const SettingsContext &context, QWidget *parent)
    : SettingsGroupPage(Group::Workspaces, context, parent)
    , m_workspaces(SettingsStore::workspaces(*config()))
    , m_list(new QListWidget)
    , m_editor(new QWidget)
    , m_name(new QLineEdit)
    , m_accounts(new QListWidget)
    , m_activity(new QComboBox)
    , m_notesFolder(new QLineEdit)
    , m_appearance(new QComboBox)
    , m_profile(new QComboBox)
    , m_options(new QListWidget)
    , m_overrides(new QLabel)
    , m_clearOverrides(new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("edit-clear")),
                                       i18nc("@action:button", "Clear")))
    , m_problems(new QLabel)
    , m_follow(new QCheckBox(i18nc("@option:check", "Show a workspace's account when its activity becomes current")))
{
    m_profiles = SettingsStore::themeProfiles(*config());
    m_list->setObjectName(QStringLiteral("workspaceList"));
    m_list->setAccessibleName(i18nc("@label accessible name", "Workspaces"));
    m_name->setObjectName(QStringLiteral("workspaceName"));
    m_accounts->setObjectName(QStringLiteral("workspaceAccounts"));
    m_activity->setObjectName(QStringLiteral("workspaceActivity"));
    m_notesFolder->setObjectName(QStringLiteral("workspaceNotesFolder"));
    m_notesFolder->setAccessibleName(i18nc("@label:textbox accessible name", "Workspace notes folder"));
    m_problems->setObjectName(QStringLiteral("workspaceProblems"));
    m_appearance->setObjectName(QStringLiteral("workspaceAppearance"));
    m_profile->setObjectName(QStringLiteral("workspaceThemeProfile"));
    m_options->setObjectName(QStringLiteral("workspaceAppearanceOptions"));
    m_overrides->setObjectName(QStringLiteral("workspaceOverrides"));
    m_clearOverrides->setObjectName(QStringLiteral("clearWorkspaceOverrides"));
    m_follow->setObjectName(QStringLiteral("followActivity"));
    makeSmall(m_problems);

    QFormLayout *intro = addSection(i18nc("@title:group", "Workspaces"));
    addNote(intro,
            i18nc("@info",
                  "A workspace groups the accounts used for one "
                  "kind of work, such as Work or Home. Attached "
                  "to a Plasma Activity, it is chosen when that "
                  "activity becomes current."));

    auto *body = new QHBoxLayout;
    auto *listColumn = new QVBoxLayout;
    listColumn->addWidget(m_list, 1);
    auto *add = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("list-add")), i18nc("@action:button", "Add"));
    add->setObjectName(QStringLiteral("addWorkspace"));
    m_remove
        = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("list-remove")), i18nc("@action:button", "Remove"));
    m_remove->setObjectName(QStringLiteral("removeWorkspace"));
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(add);
    buttons->addWidget(m_remove);
    buttons->addStretch();
    listColumn->addLayout(buttons);
    body->addLayout(listColumn, 1);

    auto *form = new QFormLayout(m_editor);
    form->setContentsMargins(0, 0, 0, 0);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->addRow(i18nc("@label:textbox", "Name:"), m_name);
    for (const auto &account : context.accounts) {
        auto *item = new QListWidgetItem(account.name, m_accounts);
        item->setData(Qt::UserRole, account.id);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    m_accounts->setMaximumHeight(m_accounts->sizeHintForRow(0) * 5 + 2 * m_accounts->frameWidth());
    form->addRow(i18nc("@label", "Accounts:"), m_accounts);
    m_activity->addItem(i18nc("@item:inlistbox no activity", "None"), QString());
    for (const auto &activity : context.activities)
        m_activity->addItem(activity.name, activity.id);
    m_activity->setEnabled(context.activitiesAvailable);
    form->addRow(i18nc("@label:listbox", "Plasma Activity:"), m_activity);
    m_notesFolder->setPlaceholderText(i18nc("@info:placeholder", "The notes folder of the Integrations page"));
    m_notesFolder->setClearButtonEnabled(true);
    auto *browse = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("document-open-folder")),
                                   i18nc("@action:button", "Choose…"));
    auto *folderRow = new QWidget;
    auto *folderLayout = new QHBoxLayout(folderRow);
    folderLayout->setContentsMargins(0, 0, 0, 0);
    folderLayout->addWidget(m_notesFolder, 1);
    folderLayout->addWidget(browse);
    form->addRow(i18nc("@label:textbox", "Notes folder:"), folderRow);

    using Appearance = Workspaces::Workspace::Appearance;
    m_appearance->addItem(i18nc("@item:inlistbox workspace appearance", "The global appearance"),
                          static_cast<int>(Appearance::None));
    m_appearance->addItem(i18nc("@item:inlistbox workspace appearance", "All options of the profile and overrides"),
                          static_cast<int>(Appearance::All));
    m_appearance->addItem(i18nc("@item:inlistbox workspace appearance", "Only the selected options"),
                          static_cast<int>(Appearance::Selected));
    form->addRow(i18nc("@label:listbox", "Appearance:"), m_appearance);
    m_profile->addItem(i18nc("@item:inlistbox no theme profile", "None"), QString());
    for (const auto &profile : std::as_const(m_profiles))
        m_profile->addItem(profile.name, profile.id);
    form->addRow(i18nc("@label:listbox", "Theme profile:"), m_profile);
    for (const QString &entry : Theme::overrideEntries()) {
        auto *item = new QListWidgetItem(ThemeGroup::optionLabel(entry), m_options);
        item->setData(Qt::UserRole, entry);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    m_options->setMaximumHeight(m_options->sizeHintForRow(0) * 6 + 2 * m_options->frameWidth());
    m_options->setToolTip(i18nc("@info:tooltip",
                                "With only the selected options, these replace the global ones while the workspace "
                                "is active; the others keep their global values."));
    form->addRow(i18nc("@label", "Options:"), m_options);
    auto *capture = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("document-import")),
                                    i18nc("@action:button", "Use Saved Appearance"));
    capture->setObjectName(QStringLiteral("captureWorkspaceAppearance"));
    capture->setToolTip(i18nc("@info:tooltip",
                              "Keep the saved values of the checked options, from the Theme page, for this "
                              "workspace. They apply over the theme profile."));
    auto *overridesRow = new QWidget;
    auto *overridesLayout = new QHBoxLayout(overridesRow);
    overridesLayout->setContentsMargins(0, 0, 0, 0);
    overridesLayout->addWidget(m_overrides, 1);
    overridesLayout->addWidget(capture);
    overridesLayout->addWidget(m_clearOverrides);
    form->addRow(i18nc("@label", "Overrides:"), overridesRow);
    form->addRow(QString(), m_problems);
    body->addWidget(m_editor, 2);
    contents()->addLayout(body);

    QFormLayout *activities = addSection(i18nc("@title:group", "Plasma Activities"));
    activities->addRow(m_follow);
    m_follow->setChecked(application(QStringLiteral("Workspaces/FollowActivity")).toBool());
    m_follow->setEnabled(context.activitiesAvailable);
    if (!context.activitiesAvailable)
        addNote(activities, SettingsSchema::unavailableReason(SettingsSchema::Feature::Activities));
    contents()->addStretch();

    connect(m_list, &QListWidget::currentRowChanged, this, &WorkspacesGroup::showWorkspace);
    connect(add, &QPushButton::clicked, this, [this]() {
        Workspaces::Workspace workspace;
        workspace.id = Workspaces::newId(m_workspaces);
        workspace.name = i18nc("@item default name of a new workspace", "New Workspace");
        m_workspaces.append(workspace);
        fillList();
        m_list->setCurrentRow(m_list->count() - 1);
        m_name->setFocus();
        m_name->selectAll();
    });
    connect(m_remove, &QPushButton::clicked, this, [this]() {
        const int index = m_list->currentRow();
        if (index < 0 || index >= m_workspaces.size())
            return;
        m_workspaces.removeAt(index);
        fillList();
        m_list->setCurrentRow(std::min(index, int(m_list->count()) - 1));
    });
    connect(m_name, &QLineEdit::textEdited, this, &WorkspacesGroup::storeEditor);
    connect(m_accounts, &QListWidget::itemChanged, this, &WorkspacesGroup::storeEditor);
    connect(m_activity, &QComboBox::currentIndexChanged, this, &WorkspacesGroup::storeEditor);
    connect(m_notesFolder, &QLineEdit::textChanged, this, &WorkspacesGroup::storeEditor);
    connect(m_appearance, &QComboBox::currentIndexChanged, this, &WorkspacesGroup::storeEditor);
    connect(m_profile, &QComboBox::currentIndexChanged, this, &WorkspacesGroup::storeEditor);
    connect(m_options, &QListWidget::itemChanged, this, &WorkspacesGroup::storeEditor);
    connect(capture, &QPushButton::clicked, this, &WorkspacesGroup::captureAppearance);
    connect(m_clearOverrides, &QPushButton::clicked, this, [this]() {
        if (auto *workspace = current()) {
            workspace->appearanceOverrides.clear();
            storeEditor();
        }
    });
    connect(browse, &QPushButton::clicked, this, [this]() {
        if (!m_context.chooseFile)
            return;
        const QString folder = m_context.chooseFile(this, i18nc("@title:window", "Notes Folder of the Workspace"),
                                                    SettingsContext::FileMode::Directory, m_notesFolder->text());
        if (!folder.isEmpty())
            m_notesFolder->setText(folder);
    });
    fillList();
    m_list->setCurrentRow(m_workspaces.isEmpty() ? -1 : 0);
    showWorkspace();
}

void WorkspacesGroup::fillList()
{
    const QSignalBlocker blocker(m_list);
    m_list->clear();
    for (const auto &workspace : std::as_const(m_workspaces))
        m_list->addItem(workspace.name);
}

Workspaces::Workspace *WorkspacesGroup::current()
{
    const int index = m_list->currentRow();
    return index >= 0 && index < m_workspaces.size() ? &m_workspaces[index] : nullptr;
}

void WorkspacesGroup::showWorkspace()
{
    const Workspaces::Workspace *workspace = current();
    m_editor->setEnabled(workspace);
    m_remove->setEnabled(workspace);
    m_loading = true;
    m_name->setText(workspace ? workspace->name : QString());
    for (int i = 0; i < m_accounts->count(); ++i) {
        QListWidgetItem *item = m_accounts->item(i);
        item->setCheckState(workspace && workspace->accounts.contains(item->data(Qt::UserRole).toString())
                                ? Qt::Checked
                                : Qt::Unchecked);
    }
    const QString activity = workspace ? workspace->activityId : QString();
    int activityIndex = m_activity->findData(activity);
    if (activityIndex < 0) {
        // An activity which is gone, or not known while Plasma Activities is not
        // running
        m_activity->addItem(i18nc("@item:inlistbox %1 is an activity id", "Unknown activity (%1)", activity), activity);
        activityIndex = m_activity->count() - 1;
    }
    m_activity->setCurrentIndex(activityIndex);
    m_notesFolder->setText(workspace ? workspace->notesFolder : QString());
    m_appearance->setCurrentIndex(std::max(
        0,
        m_appearance->findData(static_cast<int>(workspace ? workspace->appearance
                                                          : Workspaces::Workspace::Appearance::None))));
    const QString profile = workspace ? workspace->themeProfile : QString();
    int profileIndex = m_profile->findData(profile);
    if (profileIndex < 0) {
        // A deleted profile stays referenced, so that it applies again if it comes back
        m_profile->addItem(i18nc("@item:inlistbox %1 is a theme profile id", "Missing profile (%1)", profile), profile);
        profileIndex = m_profile->count() - 1;
    }
    m_profile->setCurrentIndex(profileIndex);
    for (int i = 0; i < m_options->count(); ++i) {
        QListWidgetItem *item = m_options->item(i);
        item->setCheckState(workspace && workspace->selectedAppearance.contains(item->data(Qt::UserRole).toString())
                                ? Qt::Checked
                                : Qt::Unchecked);
    }
    m_loading = false;
    storeEditor();
}

void WorkspacesGroup::captureAppearance()
{
    Workspaces::Workspace *workspace = current();
    if (!workspace)
        return;
    const Theme::Values global = SettingsStore::theme(*config());
    Theme::Values overrides;
    for (int i = 0; i < m_options->count(); ++i) {
        const QListWidgetItem *item = m_options->item(i);
        if (item->checkState() != Qt::Checked)
            continue;
        const QString entry = item->data(Qt::UserRole).toString();
        overrides.insert(entry, Theme::value(global, entry));
    }
    workspace->appearanceOverrides = overrides;
    storeEditor();
}

void WorkspacesGroup::storeEditor()
{
    Workspaces::Workspace *workspace = current();
    if (!workspace) {
        m_problems->clear();
        return;
    }
    if (!m_loading) {
        workspace->name = m_name->text().trimmed();
        workspace->accounts.clear();
        for (int i = 0; i < m_accounts->count(); ++i) {
            if (m_accounts->item(i)->checkState() == Qt::Checked)
                workspace->accounts.append(m_accounts->item(i)->data(Qt::UserRole).toString());
        }
        workspace->activityId = m_activity->currentData().toString();
        workspace->notesFolder = m_notesFolder->text().trimmed();
        workspace->appearance = static_cast<Workspaces::Workspace::Appearance>(m_appearance->currentData().toInt());
        workspace->themeProfile = m_profile->currentData().toString();
        workspace->selectedAppearance.clear();
        for (int i = 0; i < m_options->count(); ++i) {
            if (m_options->item(i)->checkState() == Qt::Checked)
                workspace->selectedAppearance.append(m_options->item(i)->data(Qt::UserRole).toString());
        }
        if (QListWidgetItem *item = m_list->currentItem())
            item->setText(workspace->name);
    }
    QStringList accountIds;
    for (const auto &account : m_context.accounts)
        accountIds.append(account.id);
    QStringList activityIds;
    for (const auto &activity : m_context.activities)
        activityIds.append(activity.id);
    QStringList problems = Workspaces::problems(*workspace, accountIds, activityIds, m_context.activitiesAvailable);
    if (workspace->appearance != Workspaces::Workspace::Appearance::None)
        problems += Workspaces::appearance(workspace, m_profiles, SettingsStore::theme(*config())).problems;
    QStringList overridden;
    for (auto it = workspace->appearanceOverrides.constBegin(); it != workspace->appearanceOverrides.constEnd(); ++it)
        overridden.append(ThemeGroup::optionLabel(it.key()));
    overridden.sort(Qt::CaseInsensitive);
    m_overrides->setText(overridden.isEmpty() ? i18nc("@info no workspace overrides", "None")
                                              : overridden.join(i18nc("@info list separator", ", ")));
    m_overrides->setWordWrap(true);
    m_clearOverrides->setEnabled(!overridden.isEmpty());
    const bool applies = workspace->appearance != Workspaces::Workspace::Appearance::None;
    m_profile->setEnabled(applies);
    m_options->setEnabled(applies);
    m_problems->setText(problems.join(QLatin1Char('\n')));
    m_problems->setVisible(!problems.isEmpty());
}

void WorkspacesGroup::saveSettings()
{
    SettingsStore::setWorkspaces(*config(), m_workspaces);
    setApplication(QStringLiteral("Workspaces/FollowActivity"), m_follow->isChecked());
}

KeyboardGroup::KeyboardGroup(const SettingsContext &context, QWidget *parent)
    : SettingsGroupPage(Group::Keyboard, context, parent)
    , m_vimMode(new QCheckBox(i18nc("@option:check",
                                    "Use Vim-style keys in the folders, the "
                                    "message list and the reader")))
    , m_timeout(new QSpinBox)
    , m_hints(new QCheckBox(i18nc("@option:check", "Show which keys can follow")))
    , m_hintDelay(new QSpinBox)
    , m_bindings(new QTableWidget)
    , m_problems(new QLabel)
    , m_status(new QLabel)
    , m_follow(new QCheckBox(i18nc("@option:check", "Follow changes to a shared keymap file")))
    , m_followed(new QLineEdit)
    , m_undo(
          new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("edit-undo")), i18nc("@action:button", "Undo Import")))
{
    m_vimMode->setObjectName(QStringLiteral("vimMode"));
    m_timeout->setObjectName(QStringLiteral("vimTimeout"));
    m_hints->setObjectName(QStringLiteral("vimHints"));
    m_hintDelay->setObjectName(QStringLiteral("vimHintDelay"));
    m_bindings->setObjectName(QStringLiteral("vimBindings"));
    m_bindings->setAccessibleName(i18nc("@label accessible name", "Vim keys"));
    m_problems->setObjectName(QStringLiteral("vimProblems"));
    m_status->setObjectName(QStringLiteral("keymapStatus"));
    m_follow->setObjectName(QStringLiteral("followKeymap"));
    m_followed->setObjectName(QStringLiteral("followedKeymap"));
    m_followed->setAccessibleName(i18nc("@label:textbox accessible name", "Keymap file"));
    m_undo->setObjectName(QStringLiteral("undoKeymapImport"));
    makeSmall(m_problems);
    makeSmall(m_status);

    QFormLayout *vim = addSection(i18nc("@title:group", "Vim Mode"));
    vim->addRow(m_vimMode);
    m_timeout->setRange(200, 5000);
    m_timeout->setSingleStep(100);
    m_timeout->setSuffix(i18nc("@item:valuesuffix milliseconds", " ms"));
    vim->addRow(i18nc("@label:spinbox", "Wait for the next key:"), m_timeout);
    vim->addRow(QString(), m_hints);
    m_hintDelay->setRange(0, 5000);
    m_hintDelay->setSingleStep(100);
    m_hintDelay->setSuffix(i18nc("@item:valuesuffix milliseconds", " ms"));
    m_hintDelay->setSpecialValueText(i18nc("@item:inlistbox no delay", "Immediately"));
    vim->addRow(i18nc("@label:spinbox", "Show hints after:"), m_hintDelay);
    addNote(vim,
            i18nc("@info",
                  "Typing in the composer, the search field and "
                  "other text fields is never affected. Press "
                  "Escape to drop keys typed so far."));

    QFormLayout *keys = addSection(i18nc("@title:group", "Keys"));
    m_bindings->setColumnCount(3);
    m_bindings->setHorizontalHeaderLabels({i18nc("@title:column", "Action"), i18nc("@title:column", "Keys"),
                                           i18nc("@title:column", "Shared with Plasma Opus")});
    m_bindings->verticalHeader()->hide();
    m_bindings->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_bindings->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_bindings->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_bindings->setSelectionMode(QAbstractItemView::SingleSelection);
    m_bindings->setMinimumHeight(m_bindings->verticalHeader()->defaultSectionSize() * 8);
    keys->addRow(m_bindings);
    keys->addRow(m_problems);
    auto *defaults = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("edit-reset")),
                                     i18nc("@action:button", "Restore Default Keys"));
    defaults->setObjectName(QStringLiteral("restoreDefaultKeys"));
    keys->addRow(row({defaults}));
    addNote(keys,
            i18nc("@info",
                  "Keys use Vim notation: gg is g twice, G is "
                  "Shift+G and <C-d> is Ctrl+D. Leave the keys "
                  "empty to turn an action off."));

    QFormLayout *opus = addSection(i18nc("@title:group", "Plasma Opus"));
    auto *importButton = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("document-import")),
                                         i18nc("@action:button", "Import…"));
    importButton->setObjectName(QStringLiteral("importKeymap"));
    auto *exportButton = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("document-export")),
                                         i18nc("@action:button", "Export…"));
    exportButton->setObjectName(QStringLiteral("exportKeymap"));
    opus->addRow(row({importButton, exportButton, m_undo}));
    opus->addRow(m_follow);
    m_followed->setReadOnly(true);
    m_followed->setPlaceholderText(i18nc("@info:placeholder", "No keymap file chosen"));
    auto *choose
        = new QPushButton(SymbolicIcon::fromTheme(QStringLiteral("document-open")), i18nc("@action:button", "Choose…"));
    choose->setObjectName(QStringLiteral("chooseFollowedKeymap"));
    auto *followRow = new QWidget;
    auto *followLayout = new QHBoxLayout(followRow);
    followLayout->setContentsMargins(0, 0, 0, 0);
    followLayout->addWidget(m_followed, 1);
    followLayout->addWidget(choose);
    opus->addRow(i18nc("@label:textbox", "Keymap file:"), followRow);
    opus->addRow(m_status);
    addNote(opus,
            i18nc("@info",
                  "Only Vim mode's settings and the keys shared with Plasma Opus are "
                  "exchanged; mail actions "
                  "keep their keys. An import is saved at once and can be undone."));
    contents()->addStretch();

    connect(m_bindings, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (m_loading || item->column() != 1)
            return;
        const QString id = item->data(Qt::UserRole).toString();
        const QString text = item->text().trimmed();
        if (!text.isEmpty() && !Keymap::isValid(text)) {
            item->setForeground(palette().brush(QPalette::Disabled, QPalette::Text));
            item->setToolTip(i18nc("@info:tooltip", "These keys are not valid Vim notation, so they are not used."));
            updateProblems();
            return;
        }
        item->setData(Qt::ForegroundRole, QVariant());
        item->setToolTip(QString());
        m_keys.insert(id, text);
        updateProblems();
    });
    connect(defaults, &QPushButton::clicked, this, [this]() {
        m_keys = Keymap::defaultBindings();
        fillBindings();
    });
    connect(importButton, &QPushButton::clicked, this, &KeyboardGroup::importProfile);
    connect(exportButton, &QPushButton::clicked, this, &KeyboardGroup::exportProfile);
    connect(choose, &QPushButton::clicked, this, &KeyboardGroup::chooseFollowed);
    connect(m_undo, &QPushButton::clicked, this, [this]() {
        if (SettingsStore::restoreKeymap(*config())) {
            config()->sync();
            load();
            m_status->setText(i18nc("@info", "The settings from before the import are back."));
        }
    });
    connect(m_vimMode, &QAbstractButton::toggled, this, [this](bool on) {
        m_timeout->setEnabled(on);
        m_hints->setEnabled(on);
        m_hintDelay->setEnabled(on && m_hints->isChecked());
    });
    connect(m_hints, &QAbstractButton::toggled, this,
            [this](bool on) { m_hintDelay->setEnabled(on && m_vimMode->isChecked()); });
    load();
}

void KeyboardGroup::load()
{
    m_vimMode->setChecked(application(QStringLiteral("Keyboard/VimMode")).toBool());
    m_timeout->setValue(application(QStringLiteral("Keyboard/VimTimeout")).toInt());
    m_hints->setChecked(application(QStringLiteral("Keyboard/VimHints")).toBool());
    m_hintDelay->setValue(application(QStringLiteral("Keyboard/VimHintDelay")).toInt());
    m_timeout->setEnabled(m_vimMode->isChecked());
    m_hints->setEnabled(m_vimMode->isChecked());
    m_hintDelay->setEnabled(m_vimMode->isChecked() && m_hints->isChecked());
    m_keys = SettingsStore::bindings(*config());
    const QString followed = SettingsStore::followedProfile(*config());
    m_followed->setText(followed);
    m_follow->setChecked(application(QStringLiteral("Keyboard/FollowSharedKeymap")).toBool() && !followed.isEmpty());
    m_undo->setEnabled(SettingsStore::canRestoreKeymap(*config()));
    fillBindings();
}

void KeyboardGroup::fillBindings()
{
    m_loading = true;
    m_bindings->setRowCount(0);
    for (const Keymap::Command &command : Keymap::commands()) {
        const int line = m_bindings->rowCount();
        m_bindings->insertRow(line);
        auto *action = new QTableWidgetItem(Keymap::description(command.id));
        action->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        action->setToolTip(command.id);
        m_bindings->setItem(line, 0, action);
        auto *keys = new QTableWidgetItem(m_keys.value(command.id));
        keys->setData(Qt::UserRole, command.id);
        keys->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        m_bindings->setItem(line, 1, keys);
        auto *shared = new QTableWidgetItem(command.shared ? i18nc("@item shared with Plasma Opus", "Yes")
                                                           : i18nc("@item shared with Plasma Opus", "No"));
        shared->setFlags(Qt::ItemIsEnabled);
        m_bindings->setItem(line, 2, shared);
    }
    m_loading = false;
    updateProblems();
}

void KeyboardGroup::updateProblems()
{
    QStringList lines;
    for (const Keymap::Conflict &conflict : Keymap::conflicts(m_keys)) {
        QStringList names;
        for (const QString &id : conflict.commands)
            names.append(Keymap::description(id));
        lines.append(conflict.kind == Keymap::Conflict::Kind::Same
                         ? i18nc("@info %1 keys, %2 actions", "%1 is used by %2.", conflict.keys,
                                 names.join(i18nc("@item list separator", ", ")))
                         : i18nc("@info %1 keys, %2 actions", "%1 starts the keys of another action: %2.",
                                 conflict.keys, names.join(i18nc("@item list separator", ", "))));
    }
    const QStringList shadowed = Keymap::shadowedShortcuts(m_keys, m_context.shortcuts);
    if (!shadowed.isEmpty()) {
        QStringList names;
        for (const QString &id : shadowed)
            names.append(Keymap::description(id));
        lines.append(i18nc("@info %1 actions",
                           "While Vim mode is on, these take the first key of a "
                           "window shortcut: %1.",
                           names.join(i18nc("@item list separator", ", "))));
    }
    m_problems->setText(lines.join(QLatin1Char('\n')));
    m_problems->setVisible(!lines.isEmpty());
}

Keymap::Profile KeyboardGroup::profile() const
{
    Keymap::Profile result;
    result.vimMode = m_vimMode->isChecked();
    result.hints = m_hints->isChecked();
    result.hintDelay = m_hintDelay->value();
    result.timeout = m_timeout->value();
    result.bindings = Keymap::sharedOnly(Keymap::merged(m_keys));
    result.application = QStringLiteral("plasma-impresario");
    return result;
}

void KeyboardGroup::importProfile()
{
    if (!m_context.chooseFile)
        return;
    const QString path = m_context.chooseFile(this, i18nc("@title:window", "Import Keymap"),
                                              SettingsContext::FileMode::Open, QString());
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_status->setText(i18nc("@info %1 file, %2 reason", "%1 could not be read: %2", path, file.errorString()));
        return;
    }
    const Keymap::ParseResult parsed = Keymap::fromJson(file.readAll());
    if (!parsed.profile) {
        m_status->setText(i18nc("@info %1 file, %2 reason", "%1 is not a keymap: %2", path, parsed.error));
        return;
    }
    const QList<Keymap::Change> changes = Keymap::preview(profile(), *parsed.profile);
    if (changes.isEmpty()) {
        m_status->setText(i18nc("@info", "The keymap matches the current settings."));
        return;
    }
    QString text = i18nc("@info", "<p>Importing the keymap changes:</p>") + QStringLiteral("<ul>");
    for (const Keymap::Change &change : changes) {
        text += QStringLiteral("<li>")
            + i18nc("@item %1 setting or action, %2 old value, %3 new value", "%1: %2 to %3",
                    commandItem(change.item).toHtmlEscaped(),
                    change.from.isEmpty() ? i18nc("@item no keys", "none") : change.from.toHtmlEscaped(),
                    change.to.isEmpty() ? i18nc("@item no keys", "none") : change.to.toHtmlEscaped())
            + QStringLiteral("</li>");
    }
    text += QStringLiteral("</ul>");
    if (!parsed.ignored.isEmpty())
        text += i18ncp("@info", "<p>One action of the keymap is not known here and is left out.</p>",
                       "<p>%1 actions of the keymap are not known here and are left out.</p>", parsed.ignored.size());
    if (!m_context.confirm || !m_context.confirm(this, i18nc("@title:window", "Import Keymap"), text))
        return;
    // What is shown now is what the import replaces, and what Undo Import brings
    // back
    saveSettings();
    SettingsStore::importKeymap(*config(), *parsed.profile);
    config()->sync();
    load();
    m_status->setText(i18nc("@info %1 file", "Imported %1.", path));
}

void KeyboardGroup::exportProfile()
{
    if (!m_context.chooseFile)
        return;
    const QString path
        = m_context.chooseFile(this, i18nc("@title:window", "Export Keymap"), SettingsContext::FileMode::Save,
                               QStringLiteral("plasma-impresario.keymap.json"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(Keymap::toJson(profile())) < 0) {
        m_status->setText(i18nc("@info %1 file, %2 reason", "%1 could not be written: %2", path, file.errorString()));
        return;
    }
    m_status->setText(i18nc("@info %1 file", "Exported to %1. Plasma Opus can import it.", path));
}

void KeyboardGroup::chooseFollowed()
{
    if (!m_context.chooseFile)
        return;
    const QString path = m_context.chooseFile(this, i18nc("@title:window", "Shared Keymap File"),
                                              SettingsContext::FileMode::Open, m_followed->text());
    if (path.isEmpty())
        return;
    m_followed->setText(path);
    m_follow->setChecked(true);
}

void KeyboardGroup::saveSettings()
{
    setApplication(QStringLiteral("Keyboard/VimMode"), m_vimMode->isChecked());
    setApplication(QStringLiteral("Keyboard/VimTimeout"), m_timeout->value());
    setApplication(QStringLiteral("Keyboard/VimHints"), m_hints->isChecked());
    setApplication(QStringLiteral("Keyboard/VimHintDelay"), m_hintDelay->value());
    SettingsStore::setBindings(*config(), m_keys);
    const bool follow = m_follow->isChecked() && !m_followed->text().isEmpty();
    setApplication(QStringLiteral("Keyboard/FollowSharedKeymap"), follow);
    SettingsStore::setFollowedProfile(*config(), follow ? m_followed->text() : QString());
}

} // namespace Gui
