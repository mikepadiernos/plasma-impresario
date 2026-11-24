/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COMMON_ACCOUNTREGISTRY_H
#define COMMON_ACCOUNTREGISTRY_H

#include <QHash>
#include <QObject>
#include <QStringList>
#include <functional>
#include <memory>

class QSettings;

namespace Common {

/**
 * Tracks the mail accounts of one window. Each account keeps its configuration
 * in its own QSettings instance, so accounts cannot read or overwrite each
 * other's server, credential or cache settings.
 */
class AccountRegistry : public QObject
{
    Q_OBJECT
public:
    /** Creates the QSettings that stores the configuration of one account. */
    using SettingsFactory = std::function<std::unique_ptr<QSettings>(const QString &accountId)>;

    /**
     * @param index settings that hold the account list and the active account; must outlive the registry
     * @param factory creates the per-account settings
     * @param parent owning object
     */
    AccountRegistry(QSettings *index, SettingsFactory factory, QObject *parent = nullptr);

    /** Return account identifiers in the order the accounts were added. */
    QStringList accountIds() const;

    /**
     * Create an account.
     * @param displayName name shown in the interface; must not be empty
     * @returns the new account identifier, which is never reused
     */
    QString addAccount(const QString &displayName);

    /**
     * Remove an account and delete its stored configuration. If it was the active account,
     * the first remaining account becomes active.
     * @param accountId identifier of an existing account
     */
    void removeAccount(const QString &accountId);

    /** Return the display name of an account, or an empty string for an unknown identifier. */
    QString displayName(const QString &accountId) const;

    /** Change an account's display name. */
    void setDisplayName(const QString &accountId, const QString &displayName);

    /**
     * Return the configuration of an account, owned by the registry.
     * @returns nullptr for an unknown identifier
     */
    QSettings *settings(const QString &accountId);

    /** Return the identifier of the active account, or an empty string when there are no accounts. */
    QString activeAccount() const;

    /** Make an existing account the active one. */
    void setActiveAccount(const QString &accountId);

signals:
    void accountAdded(const QString &accountId);
    void accountRemoved(const QString &accountId);
    void activeAccountChanged(const QString &accountId);

private:
    QSettings *m_index;
    SettingsFactory m_factory;
    QHash<QString, std::shared_ptr<QSettings>> m_settings;
};

}

#endif
