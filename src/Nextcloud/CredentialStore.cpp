/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Nextcloud/CredentialStore.h"

#include <QTimer>

#include "Plugins/PasswordPlugin.h"

namespace Nextcloud {

namespace {

bool isEncrypted(const Plugins::PasswordPlugin *plugin)
{
    return plugin && (plugin->features() & Plugins::PasswordPlugin::FeatureEncryptedStorage);
}

void later(QObject *context, std::function<void()> callback)
{
    QTimer::singleShot(0, context, std::move(callback));
}

void startJob(Plugins::PasswordJob *job)
{
    job->setAutoDelete(true);
    job->start();
}

} // namespace

CredentialStore::CredentialStore(PluginGetter plugin)
    : m_plugin(std::move(plugin))
{
}

QString CredentialStore::credentialType()
{
    return QStringLiteral("nextcloud");
}

CredentialStore::Error CredentialStore::availability() const
{
    return isEncrypted(m_plugin ? m_plugin() : nullptr) ? Error::NoError : Error::StorageUnavailable;
}

void CredentialStore::load(
    const QString &accountId, QObject *context, std::function<void(Error, const QString &)> done) const
{
    Plugins::PasswordPlugin *plugin = m_plugin ? m_plugin() : nullptr;
    Plugins::PasswordJob *job = isEncrypted(plugin) ? plugin->requestPassword(accountId, credentialType()) : nullptr;
    if (!job) {
        later(context, [done] { done(Error::StorageUnavailable, QString()); });
        return;
    }
    QObject::connect(job, &Plugins::PasswordJob::passwordAvailable, context,
        [done](const QString &password) { done(password.isEmpty() ? Error::NotFound : Error::NoError, password); });
    QObject::connect(
        job, &Plugins::PasswordJob::error, context, [done](Plugins::PasswordJob::Error error, const QString &) {
            done(
                error == Plugins::PasswordJob::NoSuchPassword ? Error::NotFound : Error::StorageUnavailable, QString());
        });
    startJob(job);
}

void CredentialStore::save(
    const QString &accountId, const QString &appPassword, QObject *context, std::function<void(Error)> done) const
{
    Plugins::PasswordPlugin *plugin = m_plugin ? m_plugin() : nullptr;
    Plugins::PasswordJob *job
        = isEncrypted(plugin) ? plugin->storePassword(accountId, credentialType(), appPassword) : nullptr;
    if (!job) {
        later(context, [done] { done(Error::StorageUnavailable); });
        return;
    }
    QObject::connect(job, &Plugins::PasswordJob::passwordStored, context, [done] { done(Error::NoError); });
    QObject::connect(job, &Plugins::PasswordJob::error, context,
        [done](Plugins::PasswordJob::Error, const QString &) { done(Error::StorageUnavailable); });
    startJob(job);
}

void CredentialStore::remove(const QString &accountId, QObject *context, std::function<void(Error)> done) const
{
    Plugins::PasswordPlugin *plugin = m_plugin ? m_plugin() : nullptr;
    if (plugin && !isEncrypted(plugin)) {
        // App passwords are never written to unencrypted storage, so there is
        // nothing to delete
        later(context, [done] { done(Error::NoError); });
        return;
    }
    Plugins::PasswordJob *job = plugin ? plugin->deletePassword(accountId, credentialType()) : nullptr;
    if (!job) {
        later(context, [done] { done(Error::StorageUnavailable); });
        return;
    }
    QObject::connect(job, &Plugins::PasswordJob::passwordDeleted, context, [done] { done(Error::NoError); });
    QObject::connect(
        job, &Plugins::PasswordJob::error, context, [done](Plugins::PasswordJob::Error error, const QString &) {
            done(error == Plugins::PasswordJob::NoSuchPassword ? Error::NoError : Error::StorageUnavailable);
        });
    startJob(job);
}

} // namespace Nextcloud
