/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <functional>

#include <QString>

class QObject;

namespace Plugins {
class PasswordPlugin;
}

namespace Nextcloud {

/** @short Keeps the app password of each manually connected account in the
password plugin

App passwords are only stored in encrypted storage, under the account ID and the
type "nextcloud". The callbacks run asynchronously and only while the context
exists. Accounts from KDE Online Accounts keep their credentials there.
*/
class CredentialStore
{
public:
    enum class Error {
        NoError,
        /** No app password is stored for the account */
        NotFound,
        /** No encrypted password storage is available, or it failed */
        StorageUnavailable,
    };

    using PluginGetter = std::function<Plugins::PasswordPlugin *()>;

    explicit CredentialStore(PluginGetter plugin);

    static QString credentialType();

    /** @short NoError when an encrypted password plugin is available,
     * StorageUnavailable otherwise */
    Error availability() const;

    void load(const QString &accountId, QObject *context, std::function<void(Error, const QString &)> done) const;
    void save(
        const QString &accountId, const QString &appPassword, QObject *context, std::function<void(Error)> done) const;
    /** @short Delete the app password; succeeds when none is stored */
    void remove(const QString &accountId, QObject *context, std::function<void(Error)> done) const;

private:
    PluginGetter m_plugin;
};

} // namespace Nextcloud
