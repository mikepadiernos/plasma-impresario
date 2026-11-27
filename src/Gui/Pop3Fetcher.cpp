/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Pop3Fetcher.h"

#include <QMessageBox>
#include <QSettings>
#include <QTimer>

#include <KLocalizedString>

#include "Common/AccountStatus.h"
#include "Common/SettingsNames.h"
#include "Common/TrustedCertificates.h"
#include "PasswordDialog.h"
#include "Imap/Model/ImapAccess.h"
#include "Plugins/PasswordPlugin.h"
#include "Plugins/PluginManager.h"
#include "UiUtils/Formatting.h"

namespace Gui {

Pop3Fetcher::Pop3Fetcher(const QString &accountId, Imap::ImapAccess *access, QSettings *account, QSettings *profile,
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
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, [this]() { check(false); });
}

Pop3Fetcher::~Pop3Fetcher() = default;

QString Pop3Fetcher::passwordType()
{
    return QStringLiteral("pop3");
}

bool Pop3Fetcher::isChecking() const
{
    return m_job || m_waitingForPassword;
}

void Pop3Fetcher::settingsChanged()
{
    m_password.clear();
    m_timer->stop();
}

void Pop3Fetcher::check(bool interactive)
{
    if (!m_account || !m_access)
        return;
    if (isChecking()) {
        // A check that runs anyway may now ask, if it needs to
        m_interactive = m_interactive || interactive;
        return;
    }
    m_timer->stop();
    if (Pop3::Settings::fromSettings(*m_account).host.isEmpty())
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

void Pop3Fetcher::start(const QString &password)
{
    if (!m_access || !m_account)
        return;
    Pop3::Settings settings = Pop3::Settings::fromSettings(*m_account);
    settings.password = password;
    auto *job = new Pop3::FetchJob(settings, m_access->localStore(), this);
    m_job = job;
    QPointer<QSettings> profile = m_profile;
    job->setTrustCheck([profile](const QString &host, const QSslCertificate &certificate) {
        return profile && Common::TrustedCertificates::isTrusted(*profile, host, certificate);
    });
    connect(job, &Pop3::FetchJob::certificateRejected, this,
            [this](const QList<QSslCertificate> &chain, const QList<QSslError> &errors) {
                m_rejectedChain = chain;
                m_rejectedErrors = errors;
            });
    connect(job, &Pop3::FetchJob::succeeded, this, [this, job, password](int downloaded) {
        job->deleteLater();
        m_password = password;
        if (m_status)
            m_status->setStatus(m_accountId, Common::AccountStatusKind::Connected);
        scheduleNext();
        emit checked(downloaded);
    });
    connect(job, &Pop3::FetchJob::failed, this, [this, job](Pop3::FetchJob::Failure failure, const QString &message) {
        job->deleteLater();
        onFailed(failure, message);
    });
    job->start();
}

void Pop3Fetcher::onFailed(Pop3::FetchJob::Failure failure, const QString &message)
{
    using Common::AccountStatusKind;
    switch (failure) {
    case Pop3::FetchJob::Failure::Authentication:
        m_password.clear();
        if (m_status)
            m_status->setStatus(m_accountId, AccountStatusKind::AuthFailed, message);
        if (m_interactive)
            askPassword(message);
        return;
    case Pop3::FetchJob::Failure::Certificate: {
        const QString host = m_account ? Pop3::Settings::fromSettings(*m_account).host : QString();
        if (m_interactive && m_window && m_profile && !m_rejectedChain.isEmpty()) {
            QString title, text;
            UiUtils::Formatting::IconType icon;
            UiUtils::Formatting::formatSslState(m_rejectedChain, QByteArray(), m_rejectedErrors, &title, &text, &icon,
                                                UiUtils::Formatting::ServerKind::Pop3);
            QPointer<Pop3Fetcher> guard(this);
            const bool accepted = QMessageBox(static_cast<QMessageBox::Icon>(icon), title, text,
                                              QMessageBox::Yes | QMessageBox::No, m_window)
                                      .exec()
                == QMessageBox::Yes;
            if (!guard)
                return;
            if (accepted && m_profile) {
                Common::TrustedCertificates::trust(*m_profile, host, m_rejectedChain.first());
                check(true);
                return;
            }
        }
        if (m_status)
            m_status->setStatus(m_accountId, AccountStatusKind::NetworkError, message);
        return;
    }
    case Pop3::FetchJob::Failure::Network:
    case Pop3::FetchJob::Failure::Security:
    case Pop3::FetchJob::Failure::Protocol:
    case Pop3::FetchJob::Failure::Storage:
        if (m_status)
            m_status->setStatus(m_accountId, AccountStatusKind::NetworkError, message);
        scheduleNext();
        return;
    }
}

void Pop3Fetcher::askPassword(const QString &error)
{
    if (!m_interactive || !m_window || !m_account) {
        if (m_status && m_status->status(m_accountId).kind != Common::AccountStatusKind::AuthFailed)
            m_status->setStatus(m_accountId, Common::AccountStatusKind::AuthRequired, error);
        return;
    }
    const Pop3::Settings settings = Pop3::Settings::fromSettings(*m_account);
    m_waitingForPassword = true;
    auto *dialog = PasswordDialog::getPassword(
        m_window, i18nc("@title:window", "Authentication Required"),
        i18nc("@info %1 is a user name, %2 a server", "<p>Please provide the POP3 password for <b>%1</b> on <b>%2</b>:</p>",
              settings.user.toHtmlEscaped(), settings.host.toHtmlEscaped()),
        error);
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

void Pop3Fetcher::scheduleNext()
{
    if (!m_account)
        return;
    const int minutes = Pop3::Settings::fromSettings(*m_account).interval;
    if (minutes > 0)
        m_timer->start(minutes * 60 * 1000);
}

}
