/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <functional>

#include <QList>
#include <QObject>
#include <QVariantMap>

#include "Nextcloud/Account.h"

namespace Nextcloud {

/**
 * @short Nextcloud accounts kept by the desktop, such as KDE Online Accounts
 *
 * The desktop keeps the credentials of these accounts; they are asked for each
 * time a client signs in and are never stored by the application.
 */
class OnlineAccounts : public QObject
{
    Q_OBJECT
public:
    /** @short Why the credentials of an account could not be had */
    enum class Problem {
        /** The account was removed or turned off in the desktop's settings */
        Removed,
        /** The desktop asks the user to sign in to the account again */
        Expired,
        /** The desktop's account service does not answer */
        Unavailable,
    };
    using Credentials = std::function<void(const QString &loginName, const QString &appPassword)>;
    using Failed = std::function<void(Problem problem, const QString &detail)>;

    explicit OnlineAccounts(QObject *parent = nullptr);
    ~OnlineAccounts() override;

    /** @short Whether the desktop's accounts can be listed at all */
    virtual bool isAvailable() const = 0;
    /** @short The enabled Nextcloud accounts, with
     * Account::Source::OnlineAccounts */
    virtual QList<Account> accounts() const = 0;
    /** @short Ask for the credentials of @arg onlineAccountId; @arg done or @arg
     * failed run later, and only while @arg context exists */
    virtual void credentials(quint32 onlineAccountId, QObject *context, Credentials done, Failed failed) = 0;

signals:
    void accountsChanged();
};

/** @short The ID under which the application knows the online account @arg
 * onlineAccountId */
QString onlineAccountKey(quint32 onlineAccountId);

/**
 * @short The account described by the settings of a KDE Online Accounts
 * Nextcloud account; invalid when they do not describe one which may be used
 *
 * The Nextcloud provider of KDE Online Accounts stores "server", and
 * "dav/storagePath" as /remote.php/dav/files/USER; the user name is the
 * account's display name. An address without TLS is refused, as when connecting
 * in the settings.
 */
Account onlineAccount(quint32 onlineAccountId, const QString &displayName, const QVariantMap &settings);

} // namespace Nextcloud
