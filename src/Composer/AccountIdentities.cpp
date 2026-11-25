/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "AccountIdentities.h"

#include "Common/SettingsNames.h"
#include "SenderIdentitiesModel.h"
#include <QSettings>

namespace Composer {
namespace AccountIdentities {

bool usesSharedIdentities(const QSettings &account)
{
    return account.value(Common::SettingsNames::composerUseSharedIdentitiesKey, false).toBool();
}

void setUsesSharedIdentities(QSettings &account, bool shared)
{
    account.setValue(Common::SettingsNames::composerUseSharedIdentitiesKey, shared);
}

void initializeNewAccount(QSettings &account)
{
    setUsesSharedIdentities(account, false);
}

bool migrateAccount(QSettings &shared, QSettings &account)
{
    if (account.contains(Common::SettingsNames::composerUseSharedIdentitiesKey))
        return false;
    const int own = account.beginReadArray(Common::SettingsNames::identitiesKey);
    account.endArray();
    if (own == 0) {
        SenderIdentitiesModel identities;
        identities.loadFromSettings(shared);
        identities.saveToSettings(account);
    }
    setUsesSharedIdentities(account, false);
    return true;
}

} // namespace AccountIdentities
} // namespace Composer
