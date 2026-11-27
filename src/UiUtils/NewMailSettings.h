/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QHash>

#include "UiUtils/NewMail.h"
#include "UiUtils/NewMailWatcher.h"

class KConfigBase;

namespace UiUtils {

/** @short The new-mail announcement settings as stored in the configuration, and what was seen in each folder

The settings are kept in the Notifications group: the general switches, and one subgroup per account with its mode
and the folders whose announcement differs from the default. Folder names are stored as entry values, so that any
character in them is kept. What was seen in each watched folder belongs to the state configuration, in the
NewMailSeen group, one entry per account and folder.
*/
namespace NewMailSettings {

NewMail::Settings load(const KConfigBase &config);
void save(KConfigBase &config, const NewMail::Settings &settings);

/** Forget the settings of @arg accountId, as when the account is removed */
void removeAccount(KConfigBase &config, const QString &accountId);
/** Keep the setting of a renamed folder; returns whether there was one */
bool renameFolder(KConfigBase &config, const QString &accountId, const QString &oldName, const QString &newName);

NewMailWatcher::Baseline loadBaseline(const KConfigBase &state, const QString &accountId, const QString &mailbox);
void saveBaseline(KConfigBase &state, const QString &accountId, const QString &mailbox,
                  const NewMailWatcher::Baseline &baseline);
void removeBaselines(KConfigBase &state, const QString &accountId);

}

}
