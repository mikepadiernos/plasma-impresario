/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "JmapSyncer.h"

#include <QMessageBox>
#include <QSettings>
#include <QTimer>

#include <KLocalizedString>

#include "Common/AccountStatus.h"
#include "Common/SettingsNames.h"
#include "Common/TrustedCertificates.h"
#include "Imap/Model/AccountCapabilities.h"
#include "Imap/Model/ImapAccess.h"
#include "LocalMail/Store.h"
#include "PasswordDialog.h"
#include "Plugins/PasswordPlugin.h"
#include "Plugins/PluginManager.h"
#include "UiUtils/Formatting.h"

namespace Gui {

JmapSyncer::JmapSyncer(const QString &accountId, Imap::ImapAccess *access, QSettings *account, QSettings *profile,
                       Plugins::PluginManager *plugins, Common::AccountStatusTracker *status, QWidget *window)
    : QObject(access)
    , m_accountId(accountId)
    , m_access(access)
    , m_account(account)
    , m_profile(profile)
    , m_plugins(plugins)
    , m_status(status)
    , m_window(window)
    , m_timer(new QTimer(this))
    , m_changeTimer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, [this]() { sync(false); });
    m_changeTimer->setSingleShot(true);
    m_changeTimer->setInterval(changeDelay());
    connect(m_changeTimer, &QTimer::timeout, this, [this]() { sync(false); });
    if (const auto store = access->localStore())
        connect(store.get(), &LocalMail::Store::changed, this, &JmapSyncer::onLocalChange);
}

JmapSyncer::~JmapSyncer() = default;

QString JmapSyncer::passwordType()
{
    return QStringLiteral("jmap");
}

bool JmapSyncer::sendsThroughJmap(const QSettings &account)
{
    return Imap::AccountCapabilities::forAccount(account).protocol() == Imap::AccountProtocol::Jmap
        && Jmap::Settings::fromSettings(account).sending == Jmap::Sending::Jmap;
}

int JmapSyncer::changeDelay()
{
    return 5000;
}

bool JmapSyncer::isSyncing() const
{
    return m_job || m_waitingForPassword;
}

void JmapSyncer::settingsChanged()
{
    m_password.clear();
    m_timer->stop();
    m_changeTimer->stop();
}

void JmapSyncer::onLocalChange()
{
    // The synchronization writes the server's changes into the same store; those need not go back
    if (m_job) {
        return;
    }
    if (m_waitingForPassword) {
        m_changedMeanwhile = true;
        return;
    }
    m_changeTimer->start();
}

void JmapSyncer::sync(bool interactive)
{
    if (!m_account || !m_access)
        return;
    if (isSyncing()) {
        // A synchronization that runs anyway may now ask, if it needs to
        m_interactive = m_interactive || interactive;
        return;
    }
    m_timer->stop();
    m_changeTimer->stop();
    m_changedMeanwhile = false;
    if (Jmap::Settings::fromSettings(*m_account).url.isEmpty())
        return;
    m_interactive = interactive;
    if (m_status && !Common::AccountStatusTracker::needsAttention(m_status->status(m_accountId).kind))
        m_status->setStatus(m_accountId, Common::AccountStatusKind::Connecting);

    if (!m_password.isEmpty()) {
        start(m_password);
        return;
    }
    Plugins::PasswordPlugin *store = m_plugins ? m_plugins->password() : nullptr;
    Plugins::PasswordJob *job = store ? store->requestPassword(m_access->credentialAccountId(), passwordType()) : nullptr;
    if (!job) {
        askPassword(QString());
        return;
    }
    m_waitingForPassword = true;
    connect(job, &Plugins::PasswordJob::passwordAvailable, this, [this](const QString &password) {
        m_waitingForPassword = false;
        start(password);
    });
    connect(job, &Plugins::PasswordJob::error, this, [this](Plugins::PasswordJob::Error error, const QString &message) {
        m_waitingForPassword = false;
        askPassword(error == Plugins::PasswordJob::Error::NoSuchPassword
                        ? QString()
                        : i18nc("@info", "The stored password could not be read: %1", message));
    });
    job->setAutoDelete(true);
    job->start();
}

void JmapSyncer::start(const QString &password)
{
    if (!m_access || !m_account)
        return;
    Jmap::Settings settings = Jmap::Settings::fromSettings(*m_account);
    settings.password = password;
    const auto store = m_access->localStore();
    m_foldersBefore = store ? store->mailboxNames() : QStringList();
    auto *job = new Jmap::SyncJob(settings, store, this);
    m_job = job;
    QPointer<QSettings> profile = m_profile;
    job->setTrustCheck([profile](const QString &host, const QSslCertificate &certificate) {
        return profile && Common::TrustedCertificates::isTrusted(*profile, host, certificate);
    });
    connect(job, &Jmap::SyncJob::certificateRejected, this,
            [this](const QList<QSslCertificate> &chain, const QList<QSslError> &errors) {
                m_rejectedChain = chain;
                m_rejectedErrors = errors;
            });
    connect(job, &Jmap::SyncJob::succeeded, this, [this, job, password](int downloaded) {
        job->deleteLater();
        m_password = password;
        if (m_account && m_account->value(Common::SettingsNames::jmapCanSubmitKey, true).toBool() != job->canSubmit())
            m_account->setValue(Common::SettingsNames::jmapCanSubmitKey, job->canSubmit());
        const QStringList warnings = job->warnings();
        if (m_status) {
            m_status->setStatus(m_accountId, Common::AccountStatusKind::Connected,
                                warnings.isEmpty() ? QString() : warnings.join(QLatin1Char('\n')));
        }
        scheduleNext();
        const auto store = m_access ? m_access->localStore() : nullptr;
        if (store && store->mailboxNames() != m_foldersBefore)
            emit foldersChanged();
        emit synced(downloaded);
    });
    connect(job, &Jmap::SyncJob::failed, this, [this, job](Jmap::Client::Failure failure, const QString &message) {
        job->deleteLater();
        onFailed(failure, message);
    });
    job->start();
}

void JmapSyncer::onFailed(Jmap::Client::Failure failure, const QString &message)
{
    using Common::AccountStatusKind;
    using Failure = Jmap::Client::Failure;
    switch (failure) {
    case Failure::Authentication:
        m_password.clear();
        if (m_status)
            m_status->setStatus(m_accountId, AccountStatusKind::AuthFailed, message);
        if (m_interactive)
            askPassword(message);
        return;
    case Failure::Certificate: {
        const QString host = m_account ? Jmap::Settings::fromSettings(*m_account).host() : QString();
        if (m_interactive && m_window && m_profile && !m_rejectedChain.isEmpty()) {
            QString title, text;
            UiUtils::Formatting::IconType icon;
            UiUtils::Formatting::formatSslState(m_rejectedChain, QByteArray(), m_rejectedErrors, &title, &text, &icon,
                                                UiUtils::Formatting::ServerKind::Jmap);
            QPointer<JmapSyncer> guard(this);
            const bool accepted = QMessageBox(static_cast<QMessageBox::Icon>(icon), title, text,
                                              QMessageBox::Yes | QMessageBox::No, m_window)
                                      .exec()
                == QMessageBox::Yes;
            if (!guard)
                return;
            if (accepted && m_profile) {
                Common::TrustedCertificates::trust(*m_profile, host, m_rejectedChain.first());
                sync(true);
                return;
            }
        }
        if (m_status)
            m_status->setStatus(m_accountId, AccountStatusKind::NetworkError, message);
        return;
    }
    case Failure::Network:
    case Failure::Security:
    case Failure::Protocol:
    case Failure::Unsupported:
    case Failure::Storage:
        if (m_status)
            m_status->setStatus(m_accountId, AccountStatusKind::NetworkError, message);
        scheduleNext();
        return;
    }
}

void JmapSyncer::askPassword(const QString &error)
{
    if (!m_interactive || !m_window || !m_account) {
        if (m_status && m_status->status(m_accountId).kind != Common::AccountStatusKind::AuthFailed)
            m_status->setStatus(m_accountId, Common::AccountStatusKind::AuthRequired, error);
        return;
    }
    const Jmap::Settings settings = Jmap::Settings::fromSettings(*m_account);
    const QString prompt = settings.authentication == Jmap::Authentication::Bearer
        ? i18nc("@info %1 is a user name, %2 a server",
                "<p>Please provide the JMAP access token for <b>%1</b> on <b>%2</b>:</p>",
                settings.user.toHtmlEscaped(), settings.host().toHtmlEscaped())
        : i18nc("@info %1 is a user name, %2 a server",
                "<p>Please provide the JMAP password for <b>%1</b> on <b>%2</b>:</p>", settings.user.toHtmlEscaped(),
                settings.host().toHtmlEscaped());
    m_waitingForPassword = true;
    auto *dialog = PasswordDialog::getPassword(m_window, i18nc("@title:window", "Authentication Required"), prompt, error);
    connect(dialog, &PasswordDialog::gotPassword, this, [this](const QString &password) {
        m_waitingForPassword = false;
        start(password);
    });
    connect(dialog, &PasswordDialog::rejected, this, [this]() {
        m_waitingForPassword = false;
        if (m_status && m_status->status(m_accountId).kind != Common::AccountStatusKind::AuthFailed)
            m_status->setStatus(m_accountId, Common::AccountStatusKind::AuthRequired);
    });
}

void JmapSyncer::scheduleNext()
{
    if (m_changedMeanwhile) {
        m_changedMeanwhile = false;
        m_changeTimer->start();
    }
    if (!m_account)
        return;
    const int minutes = Jmap::Settings::fromSettings(*m_account).interval;
    if (minutes > 0)
        m_timer->start(minutes * 60 * 1000);
}

}
