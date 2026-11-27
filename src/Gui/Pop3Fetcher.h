/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef GUI_POP3FETCHER_H
#define GUI_POP3FETCHER_H

#include <QPointer>
#include <QSslCertificate>
#include <QSslError>

#include "Pop3/FetchJob.h"

class QSettings;
class QTimer;
class QWidget;

namespace Common {
class AccountStatusTracker;
}

namespace Imap {
class ImapAccess;
}

namespace Plugins {
class PluginManager;
}

namespace Gui {

/**
 * @short Checks the server of one POP3 account now and then, and reports the outcome as the account's status
 *
 * New mail goes into the account's local store, which the mail views read like any other account. The password
 * comes from the password store; a missing or refused password, and a certificate that was not accepted before,
 * are asked about only when the check is interactive, such as for the account shown in the window.
 */
class Pop3Fetcher : public QObject
{
    Q_OBJECT
public:
    Pop3Fetcher(const QString &accountId, Imap::ImapAccess *access, QSettings *account, QSettings *profile,
                Plugins::PluginManager *plugins, Common::AccountStatusTracker *status, QWidget *window);
    ~Pop3Fetcher() override;

    /** Check the server now, unless a check is running; @arg interactive may ask for a password or a certificate */
    void check(bool interactive);
    bool isChecking() const;
    /** Forget the password of this session and wait for the next check, after the account's settings changed */
    void settingsChanged();

    /** The password type under which the POP3 password is stored */
    static QString passwordType();

signals:
    void checked(int downloaded);

private:
    void start(const QString &password);
    void askPassword(const QString &error);
    void onFailed(Pop3::FetchJob::Failure failure, const QString &message);
    void scheduleNext();

    QString m_accountId;
    QPointer<Imap::ImapAccess> m_access;
    QPointer<QSettings> m_account;
    QPointer<QSettings> m_profile;
    QPointer<Plugins::PluginManager> m_plugins;
    QPointer<Common::AccountStatusTracker> m_status;
    QPointer<QWidget> m_window;
    QTimer *m_timer;
    QPointer<Pop3::FetchJob> m_job;
    bool m_waitingForPassword = false;
    bool m_interactive = false;
    /** The password typed in this session, kept so that the next check does not ask again */
    QString m_password;
    QList<QSslCertificate> m_rejectedChain;
    QList<QSslError> m_rejectedErrors;
};

}

#endif
