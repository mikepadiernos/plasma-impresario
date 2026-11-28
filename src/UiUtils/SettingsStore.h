/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "Nextcloud/Account.h"
#include "UiUtils/FolderColors.h"
#include "UiUtils/Keymap.h"
#include "UiUtils/MailTabs.h"
#include "UiUtils/SettingsSchema.h"
#include "UiUtils/Theme.h"
#include "UiUtils/ThemeProfiles.h"
#include "UiUtils/Workspaces.h"

class KConfigBase;
class QSettings;

namespace UiUtils {

/** @short Where the settings of UiUtils::SettingsSchema are kept

Application values are in the application's KConfig file, an entry per setting in the group named by the key, such
as AutoMarkRead in group Reading. Settings which predate KConfig keep their application value in the profile's
QSettings instead, under the definition's legacyKey, so that older versions keep reading them.

Account and folder values are in group Overrides, a subgroup per account and in it a numbered subgroup per folder
which names its folder in entry Name; folder names are not used as group names, as they may contain any character.
*/
namespace SettingsStore {

using SettingsSchema::Values;

/** @short The application value of @arg key, or an invalid QVariant when it is not set */
QVariant application(const KConfigBase &config, const QSettings *legacy, const QString &key);
/** @short Store @arg value as the application value of @arg key; an invalid value removes it */
void setApplication(KConfigBase &config, QSettings *legacy, const QString &key, const QVariant &value);
Values applicationValues(const KConfigBase &config, const QSettings *legacy);

Values accountValues(const KConfigBase &config, const QString &accountId);
/** @short Store @arg value for @arg accountId; an invalid value removes the override */
void setAccountValue(KConfigBase &config, const QString &accountId, const QString &key, const QVariant &value);

Values folderValues(const KConfigBase &config, const QString &accountId, const QString &folder);
void setFolderValue(
    KConfigBase &config, const QString &accountId, const QString &folder, const QString &key, const QVariant &value);
/** @short Folders of @arg accountId with settings of their own, sorted */
QStringList folders(const KConfigBase &config, const QString &accountId);

/** @short Forget the overrides and colours of @arg accountId, and of its folders */
void removeAccount(KConfigBase &config, const QString &accountId);
/** @short Move the overrides and colours of @arg from, and of its subfolders, to @arg to */
void renameFolder(
    KConfigBase &config, const QString &accountId, const QString &from, const QString &to, QChar separator);

/** @short Every layer of the settings for @arg folder of @arg accountId; empty strings leave layers out */
SettingsSchema::Layers layers(
    const KConfigBase &config, const QSettings *legacy, const QString &accountId, const QString &folder);
/** @short The value of @arg key for @arg folder of @arg accountId */
QVariant value(const KConfigBase &config, const QSettings *legacy, const QString &key, const QString &accountId = {},
    const QString &folder = {});

QList<Workspaces::Workspace> workspaces(const KConfigBase &config);
void setWorkspaces(KConfigBase &config, const QList<Workspaces::Workspace> &workspaces);

/** @short The Nextcloud accounts connected in the settings, in group Nextcloud, without their app passwords

Accounts from KDE Online Accounts are not stored; they are found again at each start.
*/
QList<Nextcloud::Account> nextcloudAccounts(const KConfigBase &config);
void setNextcloudAccounts(KConfigBase &config, const QList<Nextcloud::Account> &accounts);

/** @short The bindings of every Vim command, defaults included */
Keymap::Bindings bindings(const KConfigBase &config);
void setBindings(KConfigBase &config, const Keymap::Bindings &bindings);
/** @short Vim mode's settings and bindings, as exported to Plasma Opus */
Keymap::Profile keymapProfile(const KConfigBase &config);
/** @short Use the settings and shared bindings of @arg profile, keeping the current ones to be restored */
void importKeymap(KConfigBase &config, const Keymap::Profile &profile);
/** @short Whether settings replaced by the last import can be restored */
bool canRestoreKeymap(const KConfigBase &config);
/** @short Go back to the settings from before the last import; false when there are none */
bool restoreKeymap(KConfigBase &config);
/** @short The profile file whose changes are followed, or empty when none is */
QString followedProfile(const KConfigBase &config);
void setFollowedProfile(KConfigBase &config, const QString &path);

/** @short The stored global appearance options of group Theme; invalid entries are left out */
Theme::Values theme(const KConfigBase &config);
/** @short Store @arg values; options at their defaults are removed, so that they keep following Plasma */
void setTheme(KConfigBase &config, const Theme::Values &values);
/** @short The saved theme profiles; unreadable ones are skipped and described in @arg problems */
QList<ThemeProfiles::Profile> themeProfiles(const KConfigBase &config, QStringList *problems = nullptr);
void setThemeProfiles(KConfigBase &config, const QList<ThemeProfiles::Profile> &profiles);
/** @short The theme profile file whose changes are followed, such as one exported by Plasma Opus, or empty */
QString followedTheme(const KConfigBase &config);
void setFollowedTheme(KConfigBase &config, const QString &path);

/** @short Account colours, folder colours and the folder colour rules of the Theme group */
FolderColors::Colors folderColors(const KConfigBase &config);
/** @short Set the colour of @arg accountId; an invalid colour removes it */
void setAccountColor(KConfigBase &config, const QString &accountId, const QColor &color);
/** @short Set the colour of @arg folder of @arg accountId; an invalid colour removes it */
void setFolderColor(KConfigBase &config, const QString &accountId, const QString &folder, const QColor &color);

/** @short The tabs saved in @arg state, the application's state file */
MailTabs::Session tabs(const KConfigBase &state);
void setTabs(KConfigBase &state, const MailTabs::Session &session);

/** @short Forget the search settings, so that their defaults apply again; the history and the index are kept */
void resetSearch(KConfigBase &config);

}

}
