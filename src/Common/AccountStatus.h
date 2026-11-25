/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COMMON_ACCOUNTSTATUS_H
#define COMMON_ACCOUNTSTATUS_H

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

namespace Common {

/** Connection and authentication state of one account. */
enum class AccountStatusKind { Idle, Connecting, Connected, Offline, NetworkError, AuthRequired, AuthFailed };

/** Current status of an account with an optional explanation, such as a server error message. */
struct AccountStatus {
    AccountStatusKind kind = AccountStatusKind::Idle;
    QString detail;
};

/** Keeps the latest status of every account so the interface can show accounts that need attention. */
class AccountStatusTracker : public QObject
{
    Q_OBJECT
public:
    explicit AccountStatusTracker(QObject *parent = nullptr);

    /** Return the status of an account; Idle for an account without recorded status. */
    AccountStatus status(const QString &accountId) const;

    /**
     * Record the status of an account and emit statusChanged() when kind or detail differ from the current status.
     * @param accountId account identifier
     * @param kind new status
     * @param detail explanation shown to the user; may be empty
     */
    void setStatus(const QString &accountId, AccountStatusKind kind, const QString &detail = QString());

    /** Discard the status of a removed account. */
    void forget(const QString &accountId);

    /** Return the sorted identifiers of accounts whose status needs user action. */
    QStringList accountsNeedingAttention() const;

    /** Return whether a status needs user action: NetworkError, AuthRequired or AuthFailed. */
    static bool needsAttention(AccountStatusKind kind);

    /** Return a translated, human-readable name of a status. */
    static QString label(AccountStatusKind kind);

signals:
    void statusChanged(const QString &accountId);

    /**
     * An account started to need attention, after statusChanged().
     *
     * This is emitted once until the account connects again, so that retries which fail again do not repeat it.
     */
    void attentionRaised(const QString &accountId);

private:
    QHash<QString, AccountStatus> m_statuses;
    QSet<QString> m_raised;
};

}

#endif
