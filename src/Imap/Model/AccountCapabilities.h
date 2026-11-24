/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef IMAP_ACCOUNTCAPABILITIES_H
#define IMAP_ACCOUNTCAPABILITIES_H

#include <QString>

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

private:
    AccountProtocol m_protocol;
};

}

#endif
