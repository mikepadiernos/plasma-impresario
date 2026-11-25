/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COMPOSER_ACCOUNTIDENTITIES_H
#define COMPOSER_ACCOUNTIDENTITIES_H

class QSettings;

namespace Composer {

/**
 * Rules for choosing the sender identities of an account.
 *
 * Each account has its own identities, stored in its own settings, unless it
 * opts to use the identities shared by all accounts, which are stored in the
 * application-wide settings.
 */
namespace AccountIdentities {

/** Return true if the account uses the shared identities instead of its own. */
bool usesSharedIdentities(const QSettings &account);

/** Record whether the account uses the shared identities instead of its own. */
void setUsesSharedIdentities(QSettings &account, bool shared);

/** Prepare a new account to use its own identities; it starts with none. */
void initializeNewAccount(QSettings &account);

/**
 * Move an account that predates per-account identities to its own identities.
 *
 * Accounts used to share one list of identities. An account with no recorded
 * choice receives a copy of the shared identities, unless it already has its
 * own, so it keeps the identities it was using. The shared identities are left
 * unchanged.
 * @returns true if the account was migrated, false if it already had a recorded
 * choice
 */
bool migrateAccount(QSettings &shared, QSettings &account);

} // namespace AccountIdentities

} // namespace Composer

#endif
