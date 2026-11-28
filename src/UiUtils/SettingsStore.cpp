/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/SettingsStore.h"

#include <KConfigGroup>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <algorithm>

namespace UiUtils {

namespace SettingsStore {

namespace {

const char overridesGroup[] = "Overrides";
const char folderPrefix[] = "Folder ";
const char nameEntry[] = "Name";
const char workspacesGroup[] = "Workspaces";
const char nextcloudGroup[] = "Nextcloud";
const char keymapGroup[] = "Keymap";
const char keymapBackupGroup[] = "KeymapBackup";
const char tabsGroup[] = "MailTabs";
const char themeProfilesGroup[] = "ThemeProfiles";
const char accountColorsGroup[] = "AccountColors";
const char folderColorsGroup[] = "FolderColors";

/** The folder colours of one account, a JSON object per folder, as folder names may contain any character */
QHash<QString, QColor> storedFolderColors(const KConfigGroup &group)
{
    QHash<QString, QColor> colors;
    const QStringList entries = group.readEntry("Colors", QStringList());
    for (const QString &entry : entries) {
        const QJsonObject object = QJsonDocument::fromJson(entry.toUtf8()).object();
        const QString folder = object.value(QStringLiteral("folder")).toString();
        const QColor color = QColor::fromString(object.value(QStringLiteral("color")).toString());
        if (!folder.isEmpty() && color.isValid())
            colors.insert(folder, QColor(color.rgb()));
    }
    return colors;
}

void storeFolderColors(KConfigGroup group, const QHash<QString, QColor> &colors)
{
    if (colors.isEmpty()) {
        group.deleteGroup();
        return;
    }
    QStringList folders = colors.keys();
    folders.sort();
    QStringList entries;
    for (const QString &folder : std::as_const(folders)) {
        const QJsonObject object{
            {QStringLiteral("folder"), folder}, {QStringLiteral("color"), colors.value(folder).name(QColor::HexRgb)}};
        entries << QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    }
    group.writeEntry("Colors", entries);
}

KConfigGroup accountGroup(const KConfigBase &config, const QString &accountId)
{
    return config.group(QLatin1String(overridesGroup)).group(accountId);
}

KConfigGroup accountGroup(KConfigBase &config, const QString &accountId)
{
    return config.group(QLatin1String(overridesGroup)).group(accountId);
}

/** The subgroup which holds @arg folder, or an empty name when the folder has none */
QString folderGroupName(const KConfigGroup &account, const QString &folder)
{
    const QStringList names = account.groupList();
    for (const QString &name : names) {
        if (name.startsWith(QLatin1String(folderPrefix))
            && account.group(name).readEntry(nameEntry, QString()) == folder)
            return name;
    }
    return QString();
}

QString unusedFolderGroupName(const KConfigGroup &account)
{
    const QStringList names = account.groupList();
    for (int n = 1;; ++n) {
        const QString name = QLatin1String(folderPrefix) + QString::number(n);
        if (!names.contains(name))
            return name;
    }
}

Values readValues(const KConfigGroup &group, SettingsSchema::Scope scope)
{
    Values values;
    for (const QString &key : SettingsSchema::keys(scope)) {
        if (!group.hasKey(key))
            continue;
        const QVariant value
            = SettingsSchema::coerce(*SettingsSchema::definition(key), group.readEntry(key, QString()));
        if (value.isValid())
            values.insert(key, value);
    }
    return values;
}

void writeValue(KConfigGroup group, const QString &key, const QVariant &value, SettingsSchema::Scope scope)
{
    const SettingsSchema::Definition *def = SettingsSchema::definition(key);
    if (!def || !def->scopes.testFlag(scope))
        return;
    const QVariant coerced = SettingsSchema::coerce(*def, value);
    if (coerced.isValid())
        group.writeEntry(key, coerced);
    else
        group.deleteEntry(key);
    group.sync();
}

void writeProfile(KConfigBase &config, const Keymap::Profile &profile)
{
    setApplication(config, nullptr, QStringLiteral("Keyboard/VimMode"), profile.vimMode);
    setApplication(config, nullptr, QStringLiteral("Keyboard/VimHints"), profile.hints);
    setApplication(config, nullptr, QStringLiteral("Keyboard/VimHintDelay"), profile.hintDelay);
    setApplication(config, nullptr, QStringLiteral("Keyboard/VimTimeout"), profile.timeout);
    setBindings(config, profile.bindings);
}

}

QVariant application(const KConfigBase &config, const QSettings *legacy, const QString &key)
{
    const SettingsSchema::Definition *def = SettingsSchema::definition(key);
    if (!def)
        return {};
    if (!def->legacyKey.isEmpty() && legacy) {
        if (!legacy->contains(def->legacyKey))
            return {};
        return SettingsSchema::coerce(*def, legacy->value(def->legacyKey));
    }
    const KConfigGroup group = config.group(SettingsSchema::configGroup(key));
    const QString entry = SettingsSchema::configEntry(key);
    if (!group.hasKey(entry))
        return {};
    return SettingsSchema::coerce(*def, group.readEntry(entry, QString()));
}

void setApplication(KConfigBase &config, QSettings *legacy, const QString &key, const QVariant &value)
{
    const SettingsSchema::Definition *def = SettingsSchema::definition(key);
    if (!def)
        return;
    const QVariant coerced = SettingsSchema::coerce(*def, value);
    if (!def->legacyKey.isEmpty() && legacy) {
        if (coerced.isValid())
            legacy->setValue(def->legacyKey, coerced);
        else
            legacy->remove(def->legacyKey);
        return;
    }
    KConfigGroup group = config.group(SettingsSchema::configGroup(key));
    const QString entry = SettingsSchema::configEntry(key);
    if (coerced.isValid())
        group.writeEntry(entry, coerced);
    else
        group.deleteEntry(entry);
    group.sync();
}

Values applicationValues(const KConfigBase &config, const QSettings *legacy)
{
    Values values;
    for (const SettingsSchema::Definition &def : SettingsSchema::definitions()) {
        const QVariant value = application(config, legacy, def.key);
        if (value.isValid())
            values.insert(def.key, value);
    }
    return values;
}

Values accountValues(const KConfigBase &config, const QString &accountId)
{
    if (accountId.isEmpty())
        return {};
    return readValues(accountGroup(config, accountId), SettingsSchema::Scope::Account);
}

void setAccountValue(KConfigBase &config, const QString &accountId, const QString &key, const QVariant &value)
{
    if (accountId.isEmpty())
        return;
    writeValue(accountGroup(config, accountId), key, value, SettingsSchema::Scope::Account);
}

Values folderValues(const KConfigBase &config, const QString &accountId, const QString &folder)
{
    if (accountId.isEmpty() || folder.isEmpty())
        return {};
    const KConfigGroup account = accountGroup(config, accountId);
    const QString name = folderGroupName(account, folder);
    if (name.isEmpty())
        return {};
    return readValues(account.group(name), SettingsSchema::Scope::Folder);
}

void setFolderValue(
    KConfigBase &config, const QString &accountId, const QString &folder, const QString &key, const QVariant &value)
{
    if (accountId.isEmpty() || folder.isEmpty())
        return;
    KConfigGroup account = accountGroup(config, accountId);
    QString name = folderGroupName(account, folder);
    if (name.isEmpty()) {
        if (!value.isValid())
            return;
        name = unusedFolderGroupName(account);
        account.group(name).writeEntry(nameEntry, folder);
    }
    KConfigGroup group = account.group(name);
    writeValue(group, key, value, SettingsSchema::Scope::Folder);
    if (group.keyList() == QStringList{QLatin1String(nameEntry)}) {
        // Nothing is left to override
        group.deleteGroup();
        account.sync();
    }
}

QStringList folders(const KConfigBase &config, const QString &accountId)
{
    QStringList result;
    if (accountId.isEmpty())
        return result;
    const KConfigGroup account = accountGroup(config, accountId);
    const QStringList names = account.groupList();
    for (const QString &name : names) {
        if (name.startsWith(QLatin1String(folderPrefix))) {
            const QString folder = account.group(name).readEntry(nameEntry, QString());
            if (!folder.isEmpty())
                result << folder;
        }
    }
    result.sort();
    return result;
}

void removeAccount(KConfigBase &config, const QString &accountId)
{
    if (accountId.isEmpty())
        return;
    KConfigGroup overrides = config.group(QLatin1String(overridesGroup));
    overrides.group(accountId).deleteGroup();
    overrides.sync();
    config.group(QLatin1String(accountColorsGroup)).deleteEntry(accountId);
    config.group(QLatin1String(folderColorsGroup)).group(accountId).deleteGroup();
}

void renameFolder(
    KConfigBase &config, const QString &accountId, const QString &from, const QString &to, QChar separator)
{
    if (accountId.isEmpty() || from.isEmpty() || to.isEmpty() || from == to)
        return;
    KConfigGroup account = accountGroup(config, accountId);
    const QStringList names = account.groupList();
    for (const QString &name : names) {
        if (!name.startsWith(QLatin1String(folderPrefix)))
            continue;
        KConfigGroup group = account.group(name);
        const QString folder = group.readEntry(nameEntry, QString());
        if (folder == from)
            group.writeEntry(nameEntry, to);
        else if (folder.startsWith(from + separator))
            group.writeEntry(nameEntry, QString(to + folder.mid(from.size())));
    }
    account.sync();

    KConfigGroup colorsGroup = config.group(QLatin1String(folderColorsGroup)).group(accountId);
    const QHash<QString, QColor> stored = storedFolderColors(colorsGroup);
    QHash<QString, QColor> moved;
    for (auto it = stored.constBegin(); it != stored.constEnd(); ++it) {
        if (it.key() == from)
            moved.insert(to, it.value());
        else if (it.key().startsWith(from + separator))
            moved.insert(to + it.key().mid(from.size()), it.value());
        else
            moved.insert(it.key(), it.value());
    }
    if (moved != stored)
        storeFolderColors(colorsGroup, moved);
}

SettingsSchema::Layers layers(
    const KConfigBase &config, const QSettings *legacy, const QString &accountId, const QString &folder)
{
    SettingsSchema::Layers result;
    result.application = applicationValues(config, legacy);
    result.account = accountValues(config, accountId);
    result.folder = folderValues(config, accountId, folder);
    return result;
}

QVariant value(const KConfigBase &config, const QSettings *legacy, const QString &key, const QString &accountId,
    const QString &folder)
{
    return SettingsSchema::resolve(key, layers(config, legacy, accountId, folder)).value;
}

QList<Workspaces::Workspace> workspaces(const KConfigBase &config)
{
    QList<Workspaces::Workspace> result;
    const KConfigGroup group = config.group(QLatin1String(workspacesGroup));
    QStringList order = group.readEntry("Order", QStringList());
    const QStringList stored = group.groupList();
    for (const QString &id : stored) {
        if (!order.contains(id))
            order << id;
    }
    for (const QString &id : std::as_const(order)) {
        if (!stored.contains(id))
            continue;
        const KConfigGroup entry = group.group(id);
        Workspaces::Workspace workspace;
        workspace.id = id;
        workspace.name = entry.readEntry("Name", QString());
        workspace.accounts = entry.readEntry("Accounts", QStringList());
        workspace.activityId = entry.readEntry("Activity", QString());
        workspace.notesFolder = entry.readEntry("NotesFolder", QString());
        workspace.appearance = Workspaces::appearanceFromConfigName(entry.readEntry("Appearance", QString()));
        workspace.themeProfile = entry.readEntry("ThemeProfile", QString());
        workspace.selectedAppearance = entry.readEntry("SelectedAppearance", QStringList());
        const QJsonObject overrides
            = QJsonDocument::fromJson(entry.readEntry("AppearanceOverrides", QString()).toUtf8()).object();
        for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it) {
            // Kept as stored, valid or not, so that a value which cannot apply now is not lost
            workspace.appearanceOverrides.insert(it.key(), it.value().toVariant());
        }
        result << workspace;
    }
    return result;
}

QList<Nextcloud::Account> nextcloudAccounts(const KConfigBase &config)
{
    QList<Nextcloud::Account> result;
    const KConfigGroup group = config.group(QLatin1String(nextcloudGroup));
    QStringList order = group.readEntry("Order", QStringList());
    const QStringList stored = group.groupList();
    for (const QString &id : stored) {
        if (!order.contains(id))
            order << id;
    }
    for (const QString &id : std::as_const(order)) {
        if (!stored.contains(id))
            continue;
        const KConfigGroup entry = group.group(id);
        Nextcloud::Account account;
        account.id = id;
        account.server = QUrl(entry.readEntry("Server", QString()));
        account.loginName = entry.readEntry("LoginName", QString());
        account.userId = entry.readEntry("UserId", QString());
        account.displayName = entry.readEntry("DisplayName", QString());
        // An address edited by hand to plain HTTP is not used, so that the app password never leaves unprotected
        if (account.isValid() && Nextcloud::isAcceptable(account.server))
            result << account;
    }
    return result;
}

void setNextcloudAccounts(KConfigBase &config, const QList<Nextcloud::Account> &accounts)
{
    KConfigGroup group = config.group(QLatin1String(nextcloudGroup));
    const QStringList stored = group.groupList();
    for (const QString &id : stored)
        group.group(id).deleteGroup();
    QStringList order;
    for (const Nextcloud::Account &account : accounts) {
        if (account.id.isEmpty() || account.source != Nextcloud::Account::Source::Manual)
            continue;
        KConfigGroup entry = group.group(account.id);
        entry.writeEntry("Server", account.server.toString());
        entry.writeEntry("LoginName", account.loginName);
        entry.writeEntry("UserId", account.userId);
        entry.writeEntry("DisplayName", account.displayName);
        order << account.id;
    }
    if (order.isEmpty())
        group.deleteEntry("Order");
    else
        group.writeEntry("Order", order);
    group.sync();
}

void setWorkspaces(KConfigBase &config, const QList<Workspaces::Workspace> &list)
{
    KConfigGroup group = config.group(QLatin1String(workspacesGroup));
    const QStringList stored = group.groupList();
    for (const QString &id : stored)
        group.group(id).deleteGroup();
    QStringList order;
    for (const Workspaces::Workspace &workspace : list) {
        if (workspace.id.isEmpty())
            continue;
        KConfigGroup entry = group.group(workspace.id);
        entry.writeEntry("Name", workspace.name);
        entry.writeEntry("Accounts", workspace.accounts);
        entry.writeEntry("Activity", workspace.activityId);
        entry.writeEntry("NotesFolder", workspace.notesFolder);
        if (workspace.appearance != Workspaces::Workspace::Appearance::None)
            entry.writeEntry("Appearance", Workspaces::configName(workspace.appearance));
        if (!workspace.themeProfile.isEmpty())
            entry.writeEntry("ThemeProfile", workspace.themeProfile);
        if (!workspace.selectedAppearance.isEmpty())
            entry.writeEntry("SelectedAppearance", workspace.selectedAppearance);
        if (!workspace.appearanceOverrides.isEmpty()) {
            QJsonObject overrides;
            for (auto it = workspace.appearanceOverrides.constBegin(); it != workspace.appearanceOverrides.constEnd();
                ++it) {
                const QVariant &value = it.value();
                overrides.insert(it.key(),
                    value.typeId() == QMetaType::QColor ? QJsonValue(value.value<QColor>().name(QColor::HexRgb))
                                                        : QJsonValue::fromVariant(value));
            }
            entry.writeEntry(
                "AppearanceOverrides", QString::fromUtf8(QJsonDocument(overrides).toJson(QJsonDocument::Compact)));
        }
        order << workspace.id;
    }
    group.writeEntry("Order", order);
    group.sync();
}

Theme::Values theme(const KConfigBase &config)
{
    Theme::Values values;
    const KConfigGroup group = config.group(QLatin1String(Theme::Group));
    for (const Theme::Option &option : Theme::options()) {
        if (!group.hasKey(option.entry))
            continue;
        QVariant stored;
        switch (option.type) {
        case Theme::Type::Bool:
            stored = group.readEntry(option.entry, option.defaultValue.toBool());
            break;
        case Theme::Type::Int:
            stored = group.readEntry(option.entry, option.defaultValue.toInt());
            break;
        case Theme::Type::StringList:
            stored = group.readEntry(option.entry, option.defaultValue.toStringList());
            break;
        case Theme::Type::Color: {
            // As a name such as #3daee9, or as KConfig writes colours: red, green and blue
            const QString text = group.readEntry(option.entry, QString()).trimmed();
            const QStringList parts = text.split(QLatin1Char(','));
            stored = text;
            if (parts.size() == 3) {
                bool ok[3] = {false, false, false};
                const QColor color(parts.at(0).trimmed().toInt(&ok[0]), parts.at(1).trimmed().toInt(&ok[1]),
                    parts.at(2).trimmed().toInt(&ok[2]));
                if (ok[0] && ok[1] && ok[2] && color.isValid())
                    stored = color.name(QColor::HexRgb);
            }
            break;
        }
        case Theme::Type::String:
        case Theme::Type::Choice:
            stored = group.readEntry(option.entry, option.defaultValue.toString());
            break;
        }
        if (const auto checked = Theme::validate(option.entry, stored))
            values.insert(option.entry, *checked);
    }
    return values;
}

void setTheme(KConfigBase &config, const Theme::Values &values)
{
    KConfigGroup group = config.group(QLatin1String(Theme::Group));
    for (const Theme::Option &option : Theme::options()) {
        const auto it = values.constFind(option.entry);
        const std::optional<QVariant> checked
            = it == values.constEnd() ? std::nullopt : Theme::validate(option.entry, it.value());
        if (!checked || *checked == option.defaultValue) {
            group.deleteEntry(option.entry);
            continue;
        }
        switch (option.type) {
        case Theme::Type::Bool:
            group.writeEntry(option.entry, checked->toBool());
            break;
        case Theme::Type::Int:
            group.writeEntry(option.entry, checked->toInt());
            break;
        case Theme::Type::StringList:
            group.writeEntry(option.entry, checked->toStringList());
            break;
        case Theme::Type::Color:
            group.writeEntry(option.entry, checked->value<QColor>().name(QColor::HexRgb));
            break;
        case Theme::Type::String:
        case Theme::Type::Choice:
            group.writeEntry(option.entry, checked->toString());
            break;
        }
    }
}

QList<ThemeProfiles::Profile> themeProfiles(const KConfigBase &config, QStringList *problems)
{
    const KConfigGroup group = config.group(QLatin1String(themeProfilesGroup));
    return ThemeProfiles::restore(group.readEntry("Profiles", QStringList()), problems);
}

void setThemeProfiles(KConfigBase &config, const QList<ThemeProfiles::Profile> &profiles)
{
    KConfigGroup group = config.group(QLatin1String(themeProfilesGroup));
    if (profiles.isEmpty())
        group.deleteEntry("Profiles");
    else
        group.writeEntry("Profiles", ThemeProfiles::store(profiles));
}

QString followedTheme(const KConfigBase &config)
{
    return config.group(QLatin1String(themeProfilesGroup)).readEntry("FollowedFile", QString());
}

void setFollowedTheme(KConfigBase &config, const QString &path)
{
    KConfigGroup group = config.group(QLatin1String(themeProfilesGroup));
    if (path.isEmpty())
        group.deleteEntry("FollowedFile");
    else
        group.writeEntry("FollowedFile", path);
}

FolderColors::Colors folderColors(const KConfigBase &config)
{
    FolderColors::Colors colors;
    const KConfigGroup accounts = config.group(QLatin1String(accountColorsGroup));
    const QStringList accountIds = accounts.keyList();
    for (const QString &accountId : accountIds) {
        const QColor color = QColor::fromString(accounts.readEntry(accountId, QString()));
        if (color.isValid())
            colors.accounts.insert(accountId, QColor(color.rgb()));
    }
    const KConfigGroup folders = config.group(QLatin1String(folderColorsGroup));
    const QStringList folderAccounts = folders.groupList();
    for (const QString &accountId : folderAccounts) {
        const QHash<QString, QColor> stored = storedFolderColors(folders.group(accountId));
        if (!stored.isEmpty())
            colors.folders.insert(accountId, stored);
    }
    colors.rules = FolderColors::rules(Theme::value(theme(config), QStringLiteral("FolderColorRules")).toStringList());
    return colors;
}

void setAccountColor(KConfigBase &config, const QString &accountId, const QColor &color)
{
    if (accountId.isEmpty())
        return;
    KConfigGroup group = config.group(QLatin1String(accountColorsGroup));
    if (color.isValid())
        group.writeEntry(accountId, color.name(QColor::HexRgb));
    else
        group.deleteEntry(accountId);
}

void setFolderColor(KConfigBase &config, const QString &accountId, const QString &folder, const QColor &color)
{
    if (accountId.isEmpty() || folder.isEmpty())
        return;
    KConfigGroup group = config.group(QLatin1String(folderColorsGroup)).group(accountId);
    QHash<QString, QColor> stored = storedFolderColors(group);
    if (color.isValid())
        stored.insert(folder, QColor(color.rgb()));
    else
        stored.remove(folder);
    storeFolderColors(group, stored);
}

Keymap::Bindings bindings(const KConfigBase &config)
{
    Keymap::Bindings stored;
    const KConfigGroup group = config.group(QLatin1String(keymapGroup));
    const QStringList keys = group.keyList();
    for (const QString &id : keys)
        stored.insert(id, group.readEntry(id, QString()));
    return Keymap::merged(stored);
}

void setBindings(KConfigBase &config, const Keymap::Bindings &bindings)
{
    KConfigGroup group = config.group(QLatin1String(keymapGroup));
    // Entries only: the subgroup Sync says which profile is followed
    const QStringList stored = group.keyList();
    for (const QString &id : stored)
        group.deleteEntry(id);
    const Keymap::Bindings changed = Keymap::overrides(Keymap::merged(bindings));
    for (auto it = changed.cbegin(); it != changed.cend(); ++it)
        group.writeEntry(it.key(), it.value());
    config.sync();
}

Keymap::Profile keymapProfile(const KConfigBase &config)
{
    Keymap::Profile profile;
    profile.application = QStringLiteral("plasma-impresario");
    auto read = [&config](const char *key) { return value(config, nullptr, QLatin1String(key)); };
    profile.vimMode = read("Keyboard/VimMode").toBool();
    profile.hints = read("Keyboard/VimHints").toBool();
    profile.hintDelay = read("Keyboard/VimHintDelay").toInt();
    profile.timeout = read("Keyboard/VimTimeout").toInt();
    profile.bindings = bindings(config);
    return profile;
}

void importKeymap(KConfigBase &config, const Keymap::Profile &profile)
{
    const Keymap::Profile current = keymapProfile(config);
    KConfigGroup backup = config.group(QLatin1String(keymapBackupGroup));
    backup.writeEntry("Profile", QString::fromUtf8(Keymap::toJson(current)));
    backup.sync();
    writeProfile(config, Keymap::apply(current, profile));
}

bool canRestoreKeymap(const KConfigBase &config)
{
    return config.group(QLatin1String(keymapBackupGroup)).hasKey("Profile");
}

bool restoreKeymap(KConfigBase &config)
{
    KConfigGroup backup = config.group(QLatin1String(keymapBackupGroup));
    const Keymap::ParseResult parsed = Keymap::fromJson(backup.readEntry("Profile", QString()).toUtf8());
    if (!parsed.profile)
        return false;
    writeProfile(config, Keymap::apply(keymapProfile(config), *parsed.profile));
    backup.deleteGroup();
    config.sync();
    return true;
}

QString followedProfile(const KConfigBase &config)
{
    return config.group(QLatin1String(keymapGroup)).group(QStringLiteral("Sync")).readEntry("Follow", QString());
}

void setFollowedProfile(KConfigBase &config, const QString &path)
{
    KConfigGroup group = config.group(QLatin1String(keymapGroup)).group(QStringLiteral("Sync"));
    if (path.isEmpty())
        group.deleteEntry("Follow");
    else
        group.writeEntry("Follow", path);
    config.sync();
}

MailTabs::Session tabs(const KConfigBase &state)
{
    const QString data = state.group(QLatin1String(tabsGroup)).readEntry("Session", QString());
    return MailTabs::deserialize(data.toUtf8()).value_or(MailTabs::Session());
}

void setTabs(KConfigBase &state, const MailTabs::Session &session)
{
    KConfigGroup group = state.group(QLatin1String(tabsGroup));
    group.writeEntry("Session", QString::fromUtf8(MailTabs::serialize(session)));
    group.sync();
}

void resetSearch(KConfigBase &config)
{
    for (const char *name : {"Search", "QuickSearch"})
        config.group(QLatin1String(name)).deleteGroup();
    config.sync();
}

}

}
