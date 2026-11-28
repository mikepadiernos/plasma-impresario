/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <functional>

#include <KSharedConfig>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSslCertificate>
#include <QSslError>

#include "Nextcloud/Account.h"
#include "Nextcloud/Client.h"
#include "Nextcloud/CredentialStore.h"

class QSettings;
class QWidget;

namespace Gui {

/**
 * @short The Nextcloud accounts of the application, their app passwords and
 * their clients
 *
 * Owned by the main window. The accounts connected in the settings are kept in
 * KConfig and their app passwords in encrypted password storage; each account
 * gets one client, made when it is first needed. Certificates the user accepts
 * are remembered in the profile, as for mail servers.
 */
class NextcloudHub : public QObject
{
    Q_OBJECT
public:
    using Ready = std::function<void(Nextcloud::Client *client)>;
    using Failed = std::function<void(const QString &message)>;
    /** @short Asks a yes-or-no question; true for yes */
    using Confirm = std::function<bool(QWidget *parent, const QString &title, const QString &text)>;

    NextcloudHub(KSharedConfig::Ptr config, QSettings *profile, Nextcloud::CredentialStore::PluginGetter passwords,
        QObject *parent = nullptr);
    ~NextcloudHub() override;

    QList<Nextcloud::Account> accounts() const;
    /** @short The account @arg id; invalid when there is none */
    Nextcloud::Account account(const QString &id) const;
    /** @short Whether app passwords can be kept, which needs encrypted password
     * storage */
    bool canStoreCredentials() const;
    /** @short The ID of the account of the same user on the same server, or an
     * empty string */
    QString findAccount(const QUrl &server, const QString &userId) const;

    /** @short Keep @arg account and its app password; @arg done gets an empty
     * string, or why it failed */
    void addAccount(Nextcloud::Account account, const QString &appPassword, QObject *context,
        std::function<void(const QString &error)> done);
    /**
     * @short Forget the account: revoke its app password on the server, then
     * delete it here
     *
     * The account is removed here even when the server cannot be reached; @arg
     * done then gets a warning that the app password may still be valid.
     */
    void removeAccount(const QString &id, QObject *context, std::function<void(const QString &warning)> done);

    /** @short A client signed in to account @arg id; @arg ready or @arg failed
     * run only while @arg context exists */
    void withClient(const QString &id, QObject *context, Ready ready, Failed failed);
    /** @short Drop the client of @arg id, such as after its app password was
     * refused */
    void forgetClient(const QString &id);

    /** @short Make @arg client, which is not one of the hub's, check certificates
     * as the hub's do */
    void prepare(Nextcloud::Client *client);
    /**
     * @short After Failure::Certificate from @arg client, ask whether to trust
     * the certificate
     *
     * @return true when the user trusted it, so that the request may be tried
     * again
     */
    bool askTrust(Nextcloud::Client *client, QWidget *parent);
    /** @short How the hub asks; a message box unless replaced, as in tests */
    void setConfirm(Confirm confirm);

signals:
    void accountsChanged();

private:
    struct Rejected {
        QList<QSslCertificate> chain;
        QList<QSslError> errors;
    };

    Nextcloud::Client *client(const QString &id);
    void saveAccounts();

    KSharedConfig::Ptr m_config;
    QPointer<QSettings> m_profile;
    Nextcloud::CredentialStore m_credentials;
    QList<Nextcloud::Account> m_accounts;
    QHash<QString, Nextcloud::Client *> m_clients;
    /** Accounts whose client has its app password */
    QHash<QString, bool> m_signedIn;
    QHash<const Nextcloud::Client *, Rejected> m_rejected;
    Confirm m_confirm;
};

} // namespace Gui
