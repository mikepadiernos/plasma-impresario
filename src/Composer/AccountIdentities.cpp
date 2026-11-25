/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "AccountIdentities.h"

#include "Common/SettingsNames.h"
#include "SenderIdentitiesModel.h"
#include <QSettings>

namespace Composer {
namespace AccountIdentities {

Source source(const QSettings &account)
{
    if (account.value(Common::SettingsNames::composerNoIdentityKey, false).toBool())
        return Source::None;
    if (account.value(Common::SettingsNames::composerUseSharedIdentitiesKey, false).toBool())
        return Source::Shared;
    return Source::Own;
}

void setSource(QSettings &account, Source source)
{
    account.setValue(Common::SettingsNames::composerUseSharedIdentitiesKey, source == Source::Shared);
    account.setValue(Common::SettingsNames::composerNoIdentityKey, source == Source::None);
}

bool usesSharedIdentities(const QSettings &account)
{
    return source(account) == Source::Shared;
}

void setUsesSharedIdentities(QSettings &account, bool shared)
{
    setSource(account, shared ? Source::Shared : Source::Own);
}

void initializeNewAccount(QSettings &account)
{
    setSource(account, Source::Own);
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
    setSource(account, Source::Own);
    return true;
}

} // namespace AccountIdentities
} // namespace Composer
