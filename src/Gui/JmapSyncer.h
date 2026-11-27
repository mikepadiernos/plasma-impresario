/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef GUI_JMAPSYNCER_H
#define GUI_JMAPSYNCER_H

#include <QPointer>
#include <QSslCertificate>
#include <QSslError>

#include "Jmap/SyncJob.h"

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
 * @short Synchronizes one JMAP account now and then, and reports the outcome as the account's status
 *
 * The account's mail is kept in its local store, which the mail views read like any other account. Changes made
 * there, such as reading, moving or deleting a message, are sent to the server shortly afterwards; the server's
 * changes come down at the account's interval. The password or token comes from the password store; a missing or
 * refused one, and a certificate that was not accepted before, are asked about only when the synchronization is
 * interactive, such as for the account shown in the window.
 */
class JmapSyncer : public QObject
{
    Q_OBJECT
public:
    JmapSyncer(const QString &accountId, Imap::ImapAccess *access, QSettings *account, QSettings *profile,
               Plugins::PluginManager *plugins, Common::AccountStatusTracker *status, QWidget *window);
    ~JmapSyncer() override;

    /** Synchronize now, unless a synchronization is running; @arg interactive may ask for a password or certificate */
    void sync(bool interactive);
    bool isSyncing() const;
    /** Forget the password of this session and wait for the next synchronization, after the settings changed */
    void settingsChanged();

    /** The password type under which the JMAP password or token is stored */
    static QString passwordType();
    /** Whether mail written in @arg account goes through its JMAP server rather than an SMTP server */
    static bool sendsThroughJmap(const QSettings &account);
    /** Milliseconds between a local change and the synchronization which sends it */
    static int changeDelay();

signals:
    void synced(int downloaded);
    /** The synchronization added, renamed or removed folders, which the folder list does not learn by itself */
    void foldersChanged();

private:
    void start(const QString &password);
    void askPassword(const QString &error);
    void onFailed(Jmap::Client::Failure failure, const QString &message);
    void scheduleNext();
    void onLocalChange();

    QString m_accountId;
    QPointer<Imap::ImapAccess> m_access;
    QPointer<QSettings> m_account;
    QPointer<QSettings> m_profile;
    QPointer<Plugins::PluginManager> m_plugins;
    QPointer<Common::AccountStatusTracker> m_status;
    QPointer<QWidget> m_window;
    QTimer *m_timer;
    QTimer *m_changeTimer;
    QPointer<Jmap::SyncJob> m_job;
    bool m_waitingForPassword = false;
    bool m_interactive = false;
    /** A local change arrived while synchronizing, so another round follows */
    bool m_changedMeanwhile = false;
    /** The password typed in this session, kept so that the next synchronization does not ask again */
    QString m_password;
    QStringList m_foldersBefore;
    QList<QSslCertificate> m_rejectedChain;
    QList<QSslError> m_rejectedErrors;
};

}

#endif
