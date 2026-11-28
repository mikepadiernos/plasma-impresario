/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <functional>

#include <QAbstractListModel>
#include <QDateTime>
#include <QHash>
#include <QList>

namespace UiUtils {

/** @short What the application did in the background: syncing, rules, scans and announcements, newest first

Work which is reported again under the same key, such as the sync of one account, updates its entry and moves it to
the top instead of adding another one, so that a long session does not bury failures under progress. The log keeps
the newest entries up to capacity; failures are dropped last. An entry can name what fixes it, such as signing in or
reviewing a rule, for the window to offer.

A failed entry whose work can run again carries Action::Retry and a handler set through setRetry(); retry() runs it.
The handler belongs to that failure: recording the entry again without Action::Retry, as when the work succeeds or
fails in a way which retrying does not fix, forgets it.

The log is kept for the session only.
*/
class ActivityLog : public QAbstractListModel
{
    Q_OBJECT
public:
    enum class Kind {
        Sync,
        Rule,
        Scan,
        Notification,
        Job,
    };
    Q_ENUM(Kind)

    enum class State {
        Running,
        /** Waiting for the user, a connection or a preview */
        Waiting,
        Done,
        Failed,
    };
    Q_ENUM(State)

    /** @short What the user can do about an entry */
    enum class Action {
        None,
        Retry,
        SignIn,
        ReviewRule,
        OpenSettings,
        OpenAccount,
        /** Open the saved draft or message file in Entry::target */
        OpenDraft,
        /** Show the messages waiting to be sent */
        OpenOutbox,
    };
    Q_ENUM(Action)

    struct Entry {
        /** Entries with the same non-empty key are one entry */
        QString key;
        Kind kind = Kind::Job;
        State state = State::Done;
        QString accountId;
        QString title;
        QString detail;
        QString error;
        Action action = Action::None;
        /** What the action applies to, such as a rule ID */
        QString target;
        QDateTime time;
        /** From 0 to 100, or -1 when not known */
        int progress = -1;
        bool operator==(const Entry &other) const = default;
    };

    enum Role {
        KindRole = Qt::UserRole + 1,
        StateRole,
        AccountRole,
        DetailRole,
        ErrorRole,
        ActionRole,
        TargetRole,
        TimeRole,
        ProgressRole,
        KeyRole,
    };

    static constexpr int capacity = 200;

    explicit ActivityLog(QObject *parent = nullptr);
    ~ActivityLog() override;

    /** @short Add @arg entry, or update the entry with its key; an invalid time is now */
    void record(Entry entry);
    /** @short Set the state of the entry with @arg key; returns whether there was one */
    bool finish(const QString &key, State state, const QString &error = QString());
    /** @short Let the failed entry @arg key run its work again through @arg handler

    The entry offers Action::Retry from now on. Returns false when there is no such entry.
    */
    bool setRetry(const QString &key, const std::function<void()> &handler);
    /** Whether the entry @arg key has a handler which retry() would run */
    bool canRetry(const QString &key) const;
    /** @short Run the handler of the entry @arg key, showing the entry as running again

    The handler is used once; the work records the entry again, and sets a new handler if it fails again.
    */
    bool retry(const QString &key);
    /** @short Retry every failed entry which can be retried; returns how many were */
    int retryAll();
    /** The number of failed entries which can be retried */
    int retryableCount() const;
    /** Forget the entries which are done, keeping running, waiting and failed ones */
    void clearFinished();
    /** Forget the entries of @arg accountId, as when the account is removed */
    void removeAccount(const QString &accountId);
    void clear();

    int count() const;
    Entry entry(int row) const;
    /** The row of the entry with @arg key, or -1 */
    int find(const QString &key) const;
    int failedCount() const;
    int runningCount() const;

    static QString stateName(State state);
    static QString actionName(Action action);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    /** The number of failed or running entries changed */
    void countsChanged();

private:
    void trim();
    void notifyCounts(int failedBefore, int runningBefore);

    void forgetRetry(const QString &key);

    QList<Entry> m_entries;
    QHash<QString, std::function<void()>> m_retries;
};

}
