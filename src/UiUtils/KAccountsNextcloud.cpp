/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "UiUtils/KAccountsNextcloud.h"

#include <Accounts/Account>
#include <Accounts/Manager>
#include <KAccounts/Core>
#include <KAccounts/GetCredentialsJob>
#include <QPointer>

namespace UiUtils {

namespace {

const QLatin1String provider("nextcloud");

} // namespace

KAccountsNextcloud::KAccountsNextcloud(QObject *parent)
    : Nextcloud::OnlineAccounts(parent)
    , m_manager(KAccounts::accountsManager())
{
    if (!m_manager)
        return;
    connect(m_manager, &Accounts::Manager::accountCreated, this, &Nextcloud::OnlineAccounts::accountsChanged);
    connect(m_manager, &Accounts::Manager::accountRemoved, this, &Nextcloud::OnlineAccounts::accountsChanged);
    connect(m_manager, &Accounts::Manager::accountUpdated, this, &Nextcloud::OnlineAccounts::accountsChanged);
    connect(m_manager, &Accounts::Manager::enabledEvent, this, &Nextcloud::OnlineAccounts::accountsChanged);
}

KAccountsNextcloud::~KAccountsNextcloud() = default;

bool KAccountsNextcloud::isAvailable() const
{
    return m_manager;
}

QList<Nextcloud::Account> KAccountsNextcloud::accounts() const
{
    QList<Nextcloud::Account> found;
    if (!m_manager)
        return found;
    const Accounts::AccountIdList ids = m_manager->accountList();
    for (const Accounts::AccountId id : ids) {
        // Kept by the manager, which hands out the same object each time
        Accounts::Account *account = m_manager->account(id);
        if (!account || account->providerName() != provider || !account->enabled())
            continue;
        account->selectService();
        QVariantMap settings;
        for (const QString &key :
            {QStringLiteral("server"), QStringLiteral("dav/host"), QStringLiteral("dav/storagePath")}) {
            settings.insert(key, account->value(key));
        }
        const Nextcloud::Account nextcloud = Nextcloud::onlineAccount(id, account->displayName(), settings);
        if (nextcloud.isValid())
            found.append(nextcloud);
    }
    return found;
}

void KAccountsNextcloud::credentials(quint32 onlineAccountId, QObject *context, Credentials done, Failed failed)
{
    QPointer<QObject> guard(context);
    auto later = [this, guard, failed](Problem problem) {
        QMetaObject::invokeMethod(
            this,
            [guard, failed, problem] {
                if (guard)
                    failed(problem, QString());
            },
            Qt::QueuedConnection);
    };
    if (!m_manager) {
        later(Problem::Unavailable);
        return;
    }
    Accounts::Account *account = m_manager->account(onlineAccountId);
    if (!account || account->providerName() != provider || !account->enabled()) {
        later(Problem::Removed);
        return;
    }
    auto *job = new KAccounts::GetCredentialsJob(onlineAccountId, this);
    connect(job, &KJob::result, this, [guard, done, failed](KJob *finished) {
        if (!guard)
            return;
        if (finished->error()) {
            failed(Problem::Unavailable, finished->errorString());
            return;
        }
        const QVariantMap data = static_cast<KAccounts::GetCredentialsJob *>(finished)->credentialsData();
        const QString secret = data.value(QStringLiteral("Secret")).toString();
        if (secret.isEmpty()) {
            // The sign-on service has nothing until the user signs in again
            failed(Problem::Expired, QString());
            return;
        }
        done(data.value(QStringLiteral("UserName")).toString(), secret);
    });
    job->start();
}

} // namespace UiUtils
