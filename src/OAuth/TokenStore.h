/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef OAUTH_TOKENSTORE_H
#define OAUTH_TOKENSTORE_H

#include <functional>

#include "OAuth/OAuth.h"

class QObject;

namespace Plugins {
class PasswordPlugin;
}

namespace OAuth {

/** @short Keeps each account's grant in the password plugin, under the account ID and credentialType()

Grants are only stored in encrypted storage. The callbacks run asynchronously and only while the context exists.
*/
class TokenStore
{
public:
    using PluginGetter = std::function<Plugins::PasswordPlugin *()>;

    explicit TokenStore(PluginGetter plugin);

    /** @short NoError when an encrypted password plugin is available, StorageUnavailable otherwise */
    Error availability() const;

    /** @short Read the grant; NotSignedIn when none is stored */
    void load(const QString &accountId, QObject *context, std::function<void(Error, const Grant &)> done) const;
    void save(const QString &accountId, const Grant &grant, QObject *context, std::function<void(Error)> done) const;
    /** @short Delete the grant; succeeds when none is stored */
    void remove(const QString &accountId, QObject *context, std::function<void(Error)> done) const;

private:
    PluginGetter m_plugin;
};

}

#endif
