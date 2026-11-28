/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TESTS_NEXTCLOUD_FAKEPASSWORDPLUGIN_H
#define TESTS_NEXTCLOUD_FAKEPASSWORDPLUGIN_H

#include <QMap>
#include <QString>

#include "Plugins/PasswordPlugin.h"

/** @short Password storage kept in memory, encrypted unless asked otherwise */
class FakePasswordPlugin;

class FakePasswordJob : public Plugins::PasswordJob
{
public:
    enum class Operation { Read, Store, Delete };
    FakePasswordJob(FakePasswordPlugin *plugin, Operation operation, const QString &key, const QString &value)
        : PasswordJob(nullptr)
        , m_plugin(plugin)
        , m_operation(operation)
        , m_key(key)
        , m_value(value)
    {
    }

protected:
    void doStart() override;
    void doStop() override
    {
        emit error(Stopped, QString());
    }

private:
    FakePasswordPlugin *m_plugin;
    Operation m_operation;
    QString m_key;
    QString m_value;
};

class FakePasswordPlugin : public Plugins::PasswordPlugin
{
public:
    explicit FakePasswordPlugin(bool encrypted = true)
        : PasswordPlugin(nullptr)
        , m_encrypted(encrypted)
    {
    }

    Features features() const override
    {
        return m_encrypted ? FeatureEncryptedStorage : Features();
    }
    Plugins::PasswordJob *requestPassword(const QString &accountId, const QString &accountType) override
    {
        return new FakePasswordJob(
            this, FakePasswordJob::Operation::Read, accountId + QLatin1Char('/') + accountType, {});
    }
    Plugins::PasswordJob *storePassword(
        const QString &accountId, const QString &accountType, const QString &password) override
    {
        return new FakePasswordJob(
            this, FakePasswordJob::Operation::Store, accountId + QLatin1Char('/') + accountType, password);
    }
    Plugins::PasswordJob *deletePassword(const QString &accountId, const QString &accountType) override
    {
        return new FakePasswordJob(
            this, FakePasswordJob::Operation::Delete, accountId + QLatin1Char('/') + accountType, {});
    }

    QMap<QString, QString> entries;
    bool failWrites = false;

private:
    bool m_encrypted;
};

inline void FakePasswordJob::doStart()
{
    switch (m_operation) {
    case Operation::Read:
        if (!m_plugin->entries.contains(m_key))
            emit error(NoSuchPassword, QString());
        else
            emit passwordAvailable(m_plugin->entries.value(m_key));
        break;
    case Operation::Store:
        if (m_plugin->failWrites) {
            emit error(UnknownError, QStringLiteral("The wallet is read-only"));
        } else {
            m_plugin->entries.insert(m_key, m_value);
            emit passwordStored();
        }
        break;
    case Operation::Delete:
        if (m_plugin->entries.remove(m_key) == 0)
            emit error(NoSuchPassword, QString());
        else
            emit passwordDeleted();
        break;
    }
}

#endif
