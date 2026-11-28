/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "Nextcloud/OnlineAccounts.h"

namespace Accounts {
class Manager;
}

namespace UiUtils {

/**
 * @short The Nextcloud accounts in KDE Online Accounts
 *
 * Lists the enabled accounts of its Nextcloud provider and asks the sign-on
 * service for their credentials each time; nothing is copied into the
 * application's settings.
 */
class KAccountsNextcloud : public Nextcloud::OnlineAccounts
{
    Q_OBJECT
public:
    explicit KAccountsNextcloud(QObject *parent = nullptr);
    ~KAccountsNextcloud() override;

    bool isAvailable() const override;
    QList<Nextcloud::Account> accounts() const override;
    void credentials(quint32 onlineAccountId, QObject *context, Credentials done, Failed failed) override;

private:
    Accounts::Manager *m_manager = nullptr;
};

} // namespace UiUtils
