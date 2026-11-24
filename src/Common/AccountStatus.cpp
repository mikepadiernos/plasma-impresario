/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "AccountStatus.h"

namespace Common {

AccountStatusTracker::AccountStatusTracker(QObject *parent)
    : QObject(parent)
{
}

AccountStatus AccountStatusTracker::status(const QString &accountId) const
{
    return m_statuses.value(accountId);
}

void AccountStatusTracker::setStatus(const QString &accountId, AccountStatusKind kind, const QString &detail)
{
    const AccountStatus current = m_statuses.value(accountId);
    if (current.kind == kind && current.detail == detail)
        return;
    m_statuses.insert(accountId, {kind, detail});
    emit statusChanged(accountId);
}

void AccountStatusTracker::forget(const QString &accountId)
{
    if (m_statuses.remove(accountId))
        emit statusChanged(accountId);
}

QStringList AccountStatusTracker::accountsNeedingAttention() const
{
    QStringList accounts;
    for (auto it = m_statuses.constBegin(); it != m_statuses.constEnd(); ++it) {
        if (needsAttention(it->kind))
            accounts << it.key();
    }
    accounts.sort();
    return accounts;
}

bool AccountStatusTracker::needsAttention(AccountStatusKind kind)
{
    return kind == AccountStatusKind::NetworkError || kind == AccountStatusKind::AuthRequired
        || kind == AccountStatusKind::AuthFailed;
}

QString AccountStatusTracker::label(AccountStatusKind kind)
{
    switch (kind) {
    case AccountStatusKind::Idle:
        return QObject::tr("Idle");
    case AccountStatusKind::Connecting:
        return QObject::tr("Connecting");
    case AccountStatusKind::Connected:
        return QObject::tr("Connected");
    case AccountStatusKind::Offline:
        return QObject::tr("Offline");
    case AccountStatusKind::NetworkError:
        return QObject::tr("Network error");
    case AccountStatusKind::AuthRequired:
        return QObject::tr("Password required");
    case AccountStatusKind::AuthFailed:
        return QObject::tr("Authentication failed");
    }
    Q_UNREACHABLE();
}

}
