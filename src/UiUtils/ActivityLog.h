/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>

namespace UiUtils {

/** @short What the application did in the background: syncing, rules, scans and announcements, newest first

Work which is reported again under the same key, such as the sync of one account, updates its entry and moves it to
the top instead of adding another one, so that a long session does not bury failures under progress. The log keeps
the newest entries up to capacity; failures are dropped last. An entry can name what fixes it, such as signing in or
reviewing a rule, for the window to offer.

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

    QList<Entry> m_entries;
};

}
