/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "UiUtils/Keymap.h"
#include "UiUtils/MailTabs.h"
#include "UiUtils/SettingsSchema.h"
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
void setFolderValue(KConfigBase &config, const QString &accountId, const QString &folder, const QString &key,
                    const QVariant &value);
/** @short Folders of @arg accountId with settings of their own, sorted */
QStringList folders(const KConfigBase &config, const QString &accountId);

/** @short Forget the overrides of @arg accountId, and its folders */
void removeAccount(KConfigBase &config, const QString &accountId);
/** @short Move the overrides of @arg from, and of its subfolders, to @arg to */
void renameFolder(KConfigBase &config, const QString &accountId, const QString &from, const QString &to,
                  QChar separator);

/** @short Every layer of the settings for @arg folder of @arg accountId; empty strings leave layers out */
SettingsSchema::Layers layers(const KConfigBase &config, const QSettings *legacy, const QString &accountId,
                              const QString &folder);
/** @short The value of @arg key for @arg folder of @arg accountId */
QVariant value(const KConfigBase &config, const QSettings *legacy, const QString &key, const QString &accountId = {},
               const QString &folder = {});

QList<Workspaces::Workspace> workspaces(const KConfigBase &config);
void setWorkspaces(KConfigBase &config, const QList<Workspaces::Workspace> &workspaces);

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

/** @short The tabs saved in @arg state, the application's state file */
MailTabs::Session tabs(const KConfigBase &state);
void setTabs(KConfigBase &state, const MailTabs::Session &session);

/** @short Forget the search settings, so that their defaults apply again; the history and the index are kept */
void resetSearch(KConfigBase &config);

}

}
