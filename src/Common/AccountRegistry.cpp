/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "AccountRegistry.h"

#include <QFile>
#include <QSettings>

namespace Common {

AccountRegistry::AccountRegistry(QSettings *index, SettingsFactory factory, QObject *parent)
    : QObject(parent), m_index(index), m_factory(std::move(factory))
{
}

namespace {
const QString idsKey = QStringLiteral("accounts/ids");
const QString activeKey = QStringLiteral("accounts/active");
const QString nextKey = QStringLiteral("accounts/next");
const QString namePrefix = QStringLiteral("accounts/names/");
}

void AccountRegistry::copyAccountKeys(const QSettings &from, QSettings &to)
{
    static const QStringList prefixes = {QStringLiteral("imap."), QStringLiteral("msa."),
                                         QStringLiteral("offline."), QStringLiteral("composer/")};
    for (const QString &key : from.allKeys()) {
        for (const QString &prefix : prefixes) {
            if (key.startsWith(prefix)) {
                to.setValue(key, from.value(key));
                break;
            }
        }
    }
}

QStringList AccountRegistry::accountIds() const
{
    return m_index->value(idsKey).toStringList();
}

QString AccountRegistry::addAccount(const QString &displayName)
{
    const int serial = m_index->value(nextKey, 1).toInt();
    const QString accountId = QStringLiteral("account-%1").arg(serial);
    m_index->setValue(nextKey, serial + 1);
    m_index->setValue(idsKey, accountIds() << accountId);
    m_index->setValue(namePrefix + accountId, displayName);
    emit accountAdded(accountId);
    if (activeAccount().isEmpty())
        setActiveAccount(accountId);
    return accountId;
}

void AccountRegistry::removeAccount(const QString &accountId)
{
    QStringList ids = accountIds();
    if (!ids.removeOne(accountId))
        return;
    QSettings *stored = settings(accountId);
    stored->clear();
    stored->sync();
    const QString path = stored->fileName();
    m_settings.remove(accountId);
    QFile::remove(path);
    m_index->setValue(idsKey, ids);
    m_index->remove(namePrefix + accountId);
    emit accountRemoved(accountId);
    if (activeAccount() == accountId) {
        m_index->remove(activeKey);
        if (!ids.isEmpty())
            setActiveAccount(ids.first());
        else
            emit activeAccountChanged(QString());
    }
}

QString AccountRegistry::displayName(const QString &accountId) const
{
    return m_index->value(namePrefix + accountId).toString();
}

void AccountRegistry::setDisplayName(const QString &accountId, const QString &displayName)
{
    if (accountIds().contains(accountId))
        m_index->setValue(namePrefix + accountId, displayName);
}

QSettings *AccountRegistry::settings(const QString &accountId)
{
    if (!accountIds().contains(accountId))
        return nullptr;
    auto it = m_settings.find(accountId);
    if (it == m_settings.end())
        it = m_settings.insert(accountId, std::shared_ptr<QSettings>(m_factory(accountId)));
    return it->get();
}

QString AccountRegistry::activeAccount() const
{
    return m_index->value(activeKey).toString();
}

void AccountRegistry::setActiveAccount(const QString &accountId)
{
    if (!accountIds().contains(accountId) || accountId == activeAccount())
        return;
    m_index->setValue(activeKey, accountId);
    emit activeAccountChanged(accountId);
}

}
