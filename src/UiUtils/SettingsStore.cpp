/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/SettingsStore.h"

#include <KConfigGroup>
#include <QSettings>
#include <algorithm>

namespace UiUtils {

namespace SettingsStore {

namespace {

const char overridesGroup[] = "Overrides";
const char folderPrefix[] = "Folder ";
const char nameEntry[] = "Name";
const char workspacesGroup[] = "Workspaces";
const char keymapGroup[] = "Keymap";
const char keymapBackupGroup[] = "KeymapBackup";
const char tabsGroup[] = "MailTabs";

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
        if (name.startsWith(QLatin1String(folderPrefix)) && account.group(name).readEntry(nameEntry, QString()) == folder)
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
        const QVariant value = SettingsSchema::coerce(*SettingsSchema::definition(key), group.readEntry(key, QString()));
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

void setFolderValue(KConfigBase &config, const QString &accountId, const QString &folder, const QString &key,
                    const QVariant &value)
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
}

void renameFolder(KConfigBase &config, const QString &accountId, const QString &from, const QString &to,
                  QChar separator)
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
}

SettingsSchema::Layers layers(const KConfigBase &config, const QSettings *legacy, const QString &accountId,
                              const QString &folder)
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
        result << workspace;
    }
    return result;
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
        order << workspace.id;
    }
    group.writeEntry("Order", order);
    group.sync();
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
