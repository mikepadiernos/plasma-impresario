/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Gui/NextcloudHub.h"

#include <KLocalizedString>
#include <QCryptographicHash>
#include <QMessageBox>
#include <QSettings>

#include "Common/TrustedCertificates.h"
#include "UiUtils/SettingsStore.h"

namespace Gui {

namespace {

QString fingerprint(const QSslCertificate &certificate)
{
    return QString::fromLatin1(certificate.digest(QCryptographicHash::Sha256).toHex(':').toUpper());
}

} // namespace

NextcloudHub::NextcloudHub(
    KSharedConfig::Ptr config, QSettings *profile, Nextcloud::CredentialStore::PluginGetter passwords, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
    , m_profile(profile)
    , m_credentials(std::move(passwords))
    , m_accounts(UiUtils::SettingsStore::nextcloudAccounts(*m_config))
    , m_confirm([](QWidget *parent, const QString &title, const QString &text) {
        return QMessageBox::warning(parent, title, text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            == QMessageBox::Yes;
    })
{
}

NextcloudHub::~NextcloudHub() = default;

QList<Nextcloud::Account> NextcloudHub::accounts() const
{
    return m_accounts;
}

Nextcloud::Account NextcloudHub::account(const QString &id) const
{
    for (const Nextcloud::Account &account : m_accounts) {
        if (account.id == id)
            return account;
    }
    return {};
}

bool NextcloudHub::canStoreCredentials() const
{
    return m_credentials.availability() == Nextcloud::CredentialStore::Error::NoError;
}

QString NextcloudHub::findAccount(const QUrl &server, const QString &userId) const
{
    for (const Nextcloud::Account &account : m_accounts) {
        if (account.userId == userId && account.server.matches(server, QUrl::StripTrailingSlash))
            return account.id;
    }
    return {};
}

void NextcloudHub::addAccount(
    Nextcloud::Account account, const QString &appPassword, QObject *context, std::function<void(const QString &)> done)
{
    QPointer<NextcloudHub> self(this);
    m_credentials.save(
        account.id, appPassword, context, [self, account, done](Nextcloud::CredentialStore::Error error) {
            if (!self)
                return;
            if (error != Nextcloud::CredentialStore::Error::NoError) {
                done(i18nc("@info",
                    "The app password could not be kept in encrypted "
                    "password storage, so the account was "
                    "not connected."));
                return;
            }
            bool replaced = false;
            for (Nextcloud::Account &existing : self->m_accounts) {
                if (existing.id == account.id) {
                    existing = account;
                    replaced = true;
                }
            }
            if (!replaced)
                self->m_accounts.append(account);
            self->forgetClient(account.id);
            self->saveAccounts();
            done(QString());
        });
}

void NextcloudHub::removeAccount(const QString &id, QObject *context, std::function<void(const QString &)> done)
{
    QPointer<NextcloudHub> self(this);
    QPointer<QObject> guard(context);
    auto forget = [self, guard, id, done](const QString &warning) {
        if (!self)
            return;
        self->forgetClient(id);
        self->m_accounts.removeIf([&id](const Nextcloud::Account &account) { return account.id == id; });
        self->saveAccounts();
        self->m_credentials.remove(id, self, [guard, warning, done](Nextcloud::CredentialStore::Error error) {
            if (!guard)
                return;
            if (error != Nextcloud::CredentialStore::Error::NoError && warning.isEmpty()) {
                done(i18nc("@info",
                    "The app password could not be deleted from "
                    "the password storage."));
                return;
            }
            done(warning);
        });
    };
    const QString notRevoked = i18nc("@info",
        "The app password could not be revoked on the server, so it may "
        "still sign in. "
        "Revoke it under Security in your Nextcloud personal settings.");
    withClient(
        id, this,
        [forget, notRevoked](Nextcloud::Client *client) {
            client->revokeAppPassword([forget] { forget(QString()); },
                [forget, notRevoked](Nextcloud::Client::Failure, const QString &) { forget(notRevoked); });
        },
        [forget, notRevoked](const QString &) { forget(notRevoked); });
}

Nextcloud::Client *NextcloudHub::client(const QString &id)
{
    if (Nextcloud::Client *existing = m_clients.value(id))
        return existing;
    auto *client = new Nextcloud::Client(this);
    client->setAccount(account(id));
    prepare(client);
    m_clients.insert(id, client);
    return client;
}

void NextcloudHub::withClient(const QString &id, QObject *context, Ready ready, Failed failed)
{
    QPointer<QObject> guard(context);
    const Nextcloud::Account found = account(id);
    if (!found.isValid()) {
        QMetaObject::invokeMethod(
            this,
            [guard, failed] {
                if (guard)
                    failed(i18nc("@info", "The Nextcloud account is no longer connected."));
            },
            Qt::QueuedConnection);
        return;
    }
    if (m_signedIn.value(id)) {
        QPointer<Nextcloud::Client> client = this->client(id);
        QMetaObject::invokeMethod(
            this,
            [guard, client, ready] {
                if (guard && client)
                    ready(client);
            },
            Qt::QueuedConnection);
        return;
    }
    QPointer<NextcloudHub> self(this);
    m_credentials.load(
        id, this, [self, guard, id, ready, failed](Nextcloud::CredentialStore::Error error, const QString &password) {
            if (!self || !guard)
                return;
            switch (error) {
            case Nextcloud::CredentialStore::Error::NoError:
                break;
            case Nextcloud::CredentialStore::Error::NotFound:
                failed(i18nc("@info",
                    "No app password is stored for this Nextcloud "
                    "account. Connect it again on the "
                    "Integrations page of the settings."));
                return;
            case Nextcloud::CredentialStore::Error::StorageUnavailable:
                failed(i18nc("@info",
                    "The encrypted password storage is not "
                    "available, so the app password of this "
                    "Nextcloud account cannot be read."));
                return;
            }
            if (self->account(id).isValid()) {
                Nextcloud::Client *client = self->client(id);
                client->setAppPassword(password);
                self->m_signedIn.insert(id, true);
                ready(client);
            } else {
                failed(i18nc("@info", "The Nextcloud account is no longer connected."));
            }
        });
}

void NextcloudHub::forgetClient(const QString &id)
{
    m_signedIn.remove(id);
    if (Nextcloud::Client *client = m_clients.take(id)) {
        m_rejected.remove(client);
        client->abortAll();
        client->deleteLater();
    }
}

void NextcloudHub::prepare(Nextcloud::Client *client)
{
    QPointer<QSettings> profile = m_profile;
    client->setTrustCheck([profile](const QString &host, const QSslCertificate &certificate) {
        return profile && Common::TrustedCertificates::isTrusted(*profile, host, certificate);
    });
    connect(client, &Nextcloud::Client::certificateRejected, this,
        [this, client](const QList<QSslCertificate> &chain, const QList<QSslError> &errors) {
            m_rejected.insert(client, Rejected{chain, errors});
        });
    connect(client, &QObject::destroyed, this, [this, client] { m_rejected.remove(client); });
}

bool NextcloudHub::askTrust(Nextcloud::Client *client, QWidget *parent)
{
    const Rejected rejected = m_rejected.take(client);
    if (rejected.chain.isEmpty() || !m_profile)
        return false;
    const QString host = client->account().server.host();
    QStringList problems;
    for (const QSslError &error : rejected.errors)
        problems << error.errorString();
    const QSslCertificate &certificate = rejected.chain.constFirst();
    const QString text = xi18nc("@info",
        "<para>The certificate of the Nextcloud server "
        "<emphasis>%1</emphasis> is not "
        "trusted: %2</para><para>Issued to %3 by %4. SHA-256 fingerprint: "
        "%5</para><para>Trust "
        "this certificate only when you know that your server uses it, "
        "for example because you "
        "set up the server yourself. Trust it from now on?</para>",
        host, problems.join(QLatin1Char(' ')),
        certificate.subjectInfo(QSslCertificate::CommonName).join(QLatin1String(", ")),
        certificate.issuerInfo(QSslCertificate::CommonName).join(QLatin1String(", ")), fingerprint(certificate));
    if (!m_confirm(parent, i18nc("@title:window", "Untrusted Certificate"), text))
        return false;
    if (m_profile)
        Common::TrustedCertificates::trust(*m_profile, host, certificate);
    return true;
}

void NextcloudHub::setConfirm(Confirm confirm)
{
    m_confirm = std::move(confirm);
}

void NextcloudHub::saveAccounts()
{
    UiUtils::SettingsStore::setNextcloudAccounts(*m_config, m_accounts);
    emit accountsChanged();
}

} // namespace Gui
