/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef IMAP_MODEL_HEADERBACKFILL_H
#define IMAP_MODEL_HEADERBACKFILL_H

#include <functional>

#include <QDate>
#include <QElapsedTimer>
#include <QObject>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QTimer>

#include "Imap/Model/MailboxTreeFwd.h"
#include "Imap/Parser/Uids.h"

namespace Imap {

namespace Mailbox {

class Model;
class SortTask;

/** @short Load the headers of the open folder in the background, the recent ones first

Once a folder is open, the server is asked which of its messages arrived during the last recentDays days, and their
headers are loaded newest first, a small batch at a time. The headers of older messages follow from the end of the list
downward. The list shows any message without waiting for this; the backfill only saves the wait when the user scrolls.

A batch is only asked for while the folder's connection has nothing else to do and the predicate of
setOtherWorkPending() says that nothing else is due, so that a click, a flag change, opening a message or sending one
never waits behind it. Headers which the offline cache has are taken from there without asking the server.

Only the folder which the user opened is filled, as the account shares one connection between its folders. The work
pauses while the network is expensive, stops when the account goes offline and starts again when the folder is
synchronized again. At most messageLimit() headers are asked for each time, so that a huge folder does not fill the
memory with headers nobody looks at.
*/
class HeaderBackfill : public QObject
{
    Q_OBJECT
public:
    explicit HeaderBackfill(Model *model, QObject *parent = nullptr);
    ~HeaderBackfill() override;

    void setEnabled(bool enabled);
    bool isEnabled() const;
    /** @short Messages which arrived during the last @p days come first; 30 by default */
    void setRecentDays(int days);
    /** @short Headers asked for at once: @p recent for the recent messages, @p older for the rest */
    void setBatchSizes(int recent, int older);
    /** @short Ask for at most @p limit headers each time a folder is filled; zero means no limit */
    void setMessageLimit(int limit);
    int messageLimit() const;
    /** @short How often to look whether the connection is free again; 50 ms by default */
    void setPollInterval(int milliseconds);
    /** @short The day from which recentDays count back, instead of today; for tests */
    void setReferenceDate(const QDate &date);
    /** @short Wait while @p pending says that other work of the account is due, such as a message to send */
    void setOtherWorkPending(const std::function<bool()> &pending);

    bool isRunning() const;
    /** @short The folder being filled, or empty */
    QString mailbox() const;
    /** @short Headers asked for since the folder was last synchronized */
    int requestedCount() const;
    /** @short Number of recent messages, as far as the server said */
    int recentCount() const;
    void stop();

signals:
    /** @short The recent headers of @p mailbox are loaded, @p elapsedMs after its synchronization started */
    void recentLoaded(const QString &mailbox, qint64 elapsedMs);
    void finished(const QString &mailbox);

private:
    enum class Phase {
        Idle,
        /** Waiting until the folder is open and idle, to ask for the recent messages */
        Waiting,
        Searching,
        Recent,
        Older,
    };

    void onSyncingProgress(const QModelIndex &mailbox, int state);
    void start(const QModelIndex &mailbox);
    void step();
    void search();
    void searchAnswered(quint64 generation, const Imap::Uids &uids);
    void loadRecent();
    void loadOlder();
    void finish();
    void stopTask();
    /** @short Ask for the headers of @p messages as far as the limit allows; returns how many were asked for */
    int request(const QList<TreeItemMessage *> &messages);
    bool limitReached() const;

    QPointer<Model> m_model;
    QTimer m_poll;
    std::function<bool()> m_otherWorkPending;
    bool m_enabled = true;
    int m_recentDays = 30;
    int m_recentBatch = 100;
    int m_olderBatch = 50;
    int m_limit = 5000;
    QDate m_referenceDate;

    quint64 m_generation = 0;
    Phase m_phase = Phase::Idle;
    QPersistentModelIndex m_mailbox;
    QString m_mailboxName;
    QPointer<SortTask> m_task;
    /** UIDs of the recent messages still to load, newest first */
    Imap::Uids m_recent;
    int m_recentCount = 0;
    /** Row of the message list to look at next, walking toward the oldest message */
    int m_cursor = -1;
    int m_requested = 0;
    QElapsedTimer m_clock;
};

}

}

#endif
