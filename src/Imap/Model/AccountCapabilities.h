/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef IMAP_ACCOUNTCAPABILITIES_H
#define IMAP_ACCOUNTCAPABILITIES_H

#include <QString>

class QSettings;

namespace Imap {

/** Mail protocols an account can use. */
enum class AccountProtocol { Imap, Pop3, Jmap };

/** Operations whose availability depends on the account protocol. */
enum class AccountOperation {
    ServerFolders,
    ServerSideFlags,
    ServerSideSearch,
    ServerSideMove,
    SaveDraftsOnServer,
    LeaveMessagesOnServer,
    PushNotifications,
};

/** Describes which operations an account protocol supports. */
class AccountCapabilities
{
public:
    explicit AccountCapabilities(AccountProtocol protocol);

    /** Return the protocol these capabilities describe. */
    AccountProtocol protocol() const;

    /**
     * Return whether the protocol supports an operation.
     * @param operation the operation to check
     */
    bool supports(AccountOperation operation) const;

    /**
     * Return a translated explanation of why an operation is unavailable.
     * @param operation the operation to check
     * @returns the explanation, or an empty string if the operation is supported
     */
    QString limitation(AccountOperation operation) const;

    /**
     * Return the protocol for a stored account type name.
     * @param name "imap", "pop3" or "jmap", case-insensitive
     * @param ok set to false when the name is not recognized
     */
    static AccountProtocol protocolFromName(const QString &name, bool *ok);

    /** Return the name under which a protocol is stored: "imap", "pop3" or "jmap". */
    static QString protocolName(AccountProtocol protocol);

    /** Return the key that holds the protocol in an account's own settings. */
    static QString protocolSettingsKey();

    /**
     * Return the capabilities of the account whose settings are given.
     * Accounts without a stored protocol use IMAP, which every account used before protocols were stored.
     * @param account settings of one account
     * @param recognized set to false when the stored protocol is unknown; the account then reports IMAP
     */
    static AccountCapabilities forAccount(const QSettings &account, bool *recognized = nullptr);

    /** Store the protocol in an account's own settings. */
    static void setProtocol(QSettings &account, AccountProtocol protocol);

private:
    AccountProtocol m_protocol;
};

}

#endif
