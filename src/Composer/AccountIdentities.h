/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COMPOSER_ACCOUNTIDENTITIES_H
#define COMPOSER_ACCOUNTIDENTITIES_H

#include <QList>

class QSettings;

namespace Imap {
namespace Message {
class MailAddress;
}
}

namespace Composer {

/**
 * Rules for choosing the sender identities of an account.
 *
 * Each account has its own identities, stored in its own settings, by default.
 * It can instead use the identities shared by all accounts, which are stored in
 * the application-wide settings, or send without an identity, in which case the
 * composer starts with an empty From address and adds no signature.
 */
namespace AccountIdentities {

/** Where an account takes its sender identities from. */
enum class Source {
    Own, /**< The account's own identities; the default */
    Shared, /**< The identities shared by all accounts */
    None, /**< No identity; the From address is typed in the composer */
};

/** Return where the account takes its sender identities from. */
Source source(const QSettings &account);

/**
 * Record where the account takes its sender identities from.
 *
 * The account's own identities are kept whichever source is chosen, so they are
 * available again when the account switches back to them.
 */
void setSource(QSettings &account, Source source);

/** Return true if the account uses the shared identities instead of its own. */
bool usesSharedIdentities(const QSettings &account);

/** Record whether the account uses the shared identities or its own. */
void setUsesSharedIdentities(QSettings &account, bool shared);

/**
 * Return the account's own addresses that come from its login rather than from
 * its identities.
 *
 * This is the IMAP user name when it is a plain email address. Replies treat it
 * as the user's own address, so *Reply to All but Me* still excludes the user
 * when the account sends without an identity.
 */
QList<Imap::Message::MailAddress> loginAddresses(const QSettings &account);

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
