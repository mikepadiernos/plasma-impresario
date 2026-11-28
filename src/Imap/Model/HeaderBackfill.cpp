/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "HeaderBackfill.h"

#include <algorithm>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/TaskFactory.h"
#include "Imap/Tasks/SortTask.h"

namespace Imap {

namespace Mailbox {

namespace {

/** The date as an IMAP SEARCH key, whose month names are English whatever the locale */
QString imapDate(const QDate &date)
{
    static const char *const months[]
        = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    return QStringLiteral("%1-%2-%3")
        .arg(date.day())
        .arg(QLatin1String(months[date.month() - 1]))
        .arg(date.year(), 4, 10, QLatin1Char('0'));
}

/** Rows looked at in one step while walking toward the oldest message, so that a long list does not block */
constexpr int maxRowsPerStep = 2000;

bool needsHeader(const TreeItemMessage *message)
{
    return message && message->uid() && !message->fetched() && !message->loading() && !message->isUnavailable();
}

}

HeaderBackfill::HeaderBackfill(Model *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
{
    m_poll.setInterval(50);
    connect(&m_poll, &QTimer::timeout, this, &HeaderBackfill::step);
    connect(model, &Model::mailboxSyncingProgress, this,
        [this](const QModelIndex &mailbox, const MailboxSyncingProgress state) { onSyncingProgress(mailbox, state); });
    connect(model, &Model::networkPolicyOffline, this, &HeaderBackfill::stop);
}

HeaderBackfill::~HeaderBackfill()
{
    stopTask();
}

void HeaderBackfill::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled)
        stop();
}

bool HeaderBackfill::isEnabled() const
{
    return m_enabled;
}

void HeaderBackfill::setRecentDays(int days)
{
    m_recentDays = std::max(days, 0);
}

void HeaderBackfill::setBatchSizes(int recent, int older)
{
    m_recentBatch = std::max(recent, 1);
    m_olderBatch = std::max(older, 1);
}

void HeaderBackfill::setMessageLimit(int limit)
{
    m_limit = std::max(limit, 0);
}

int HeaderBackfill::messageLimit() const
{
    return m_limit;
}

void HeaderBackfill::setPollInterval(int milliseconds)
{
    m_poll.setInterval(std::max(milliseconds, 0));
}

void HeaderBackfill::setReferenceDate(const QDate &date)
{
    m_referenceDate = date;
}

void HeaderBackfill::setOtherWorkPending(const std::function<bool()> &pending)
{
    m_otherWorkPending = pending;
}

bool HeaderBackfill::isRunning() const
{
    return m_phase != Phase::Idle;
}

QString HeaderBackfill::mailbox() const
{
    return isRunning() ? m_mailboxName : QString();
}

int HeaderBackfill::requestedCount() const
{
    return m_requested;
}

int HeaderBackfill::recentCount() const
{
    return m_recentCount;
}

void HeaderBackfill::stop()
{
    ++m_generation;
    m_poll.stop();
    stopTask();
    m_phase = Phase::Idle;
    m_recent.clear();
}

void HeaderBackfill::onSyncingProgress(const QModelIndex &mailbox, int state)
{
    if (!m_enabled || !mailbox.isValid())
        return;
    // A new synchronization of the folder may replace its messages, so the walk starts over
    const bool resync = state == STATE_WAIT_FOR_CONN || state == STATE_SELECTING;
    if (resync || QModelIndex(m_mailbox) != mailbox || (m_phase == Phase::Idle && state == STATE_DONE))
        start(mailbox);
}

void HeaderBackfill::start(const QModelIndex &mailbox)
{
    stop();
    m_mailbox = mailbox;
    m_mailboxName = mailbox.data(RoleMailboxName).toString();
    m_recentCount = 0;
    m_requested = 0;
    m_cursor = -1;
    m_phase = Phase::Waiting;
    m_clock.start();
    m_poll.start();
}

void HeaderBackfill::step()
{
    if (m_phase == Phase::Idle)
        return;
    if (!m_model || !m_mailbox.isValid() || m_model->networkPolicy() == NETWORK_OFFLINE
        || !m_model->isMailboxMaintained(m_mailbox)) {
        stop();
        return;
    }
    // A batch which is still loading, a request of the user or a search keeps the connection busy
    if (m_phase == Phase::Searching || m_model->networkPolicy() != NETWORK_ONLINE || !m_model->isMailboxIdle(m_mailbox)
        || (m_otherWorkPending && m_otherWorkPending()))
        return;
    switch (m_phase) {
    case Phase::Waiting: {
        // An empty folder has nothing to fill
        const QModelIndex list = m_model->index(0, 0, m_mailbox);
        if (!list.isValid() || m_model->rowCount(list) == 0)
            finish();
        else
            search();
        return;
    }
    case Phase::Recent:
        loadRecent();
        return;
    case Phase::Older:
        loadOlder();
        return;
    case Phase::Idle:
    case Phase::Searching:
        return;
    }
}

void HeaderBackfill::search()
{
    const QDate today = m_referenceDate.isValid() ? m_referenceDate : QDate::currentDate();
    const quint64 generation = m_generation;
    m_phase = Phase::Searching;
    // SINCE looks at the date when the server received a message, which is what the list is sorted by
    m_task = m_model->m_taskFactory->createSortTask(
        m_model, m_mailbox, {QStringLiteral("SINCE"), imapDate(today.addDays(-m_recentDays))}, QStringList());
    connect(m_task.data(), &SortTask::sortingAvailable, this,
        [this, generation](const Imap::Uids &uids) { searchAnswered(generation, uids); });
    // Without an answer, the recent messages simply come with the rest
    connect(m_task.data(), &ImapTask::failed, this, [this, generation]() { searchAnswered(generation, Imap::Uids()); });
    connect(m_task.data(), &QObject::destroyed, this, [this, generation]() {
        if (m_phase == Phase::Searching)
            searchAnswered(generation, Imap::Uids());
    });
}

void HeaderBackfill::searchAnswered(quint64 generation, const Imap::Uids &uids)
{
    if (generation != m_generation || m_phase != Phase::Searching)
        return;
    stopTask();
    m_recent = uids;
    std::sort(m_recent.begin(), m_recent.end(), std::greater<uint>());
    m_recent.erase(std::unique(m_recent.begin(), m_recent.end()), m_recent.end());
    m_recentCount = m_recent.size();
    m_phase = Phase::Recent;
}

void HeaderBackfill::loadRecent()
{
    auto *mailbox = static_cast<TreeItemMailbox *>(static_cast<TreeItem *>(m_mailbox.internalPointer()));
    while (!m_recent.isEmpty() && !limitReached()) {
        const int count = std::min<int>(m_recentBatch, m_recent.size());
        Imap::Uids batch = m_recent.mid(0, count);
        m_recent.remove(0, count);
        // The lookup walks the list once, in the order of the UIDs
        std::sort(batch.begin(), batch.end());
        QList<TreeItemMessage *> messages = m_model->findMessagesByUids(mailbox, batch);
        std::reverse(messages.begin(), messages.end());
        // One batch at a time, even when the offline cache has the headers, so that loading them never blocks
        if (request(messages) > 0)
            return;
    }
    m_recent.clear();
    emit recentLoaded(m_mailboxName, m_clock.elapsed());
    m_phase = Phase::Older;
    const QModelIndex list = m_model->index(0, 0, m_mailbox);
    m_cursor = list.isValid() ? m_model->rowCount(list) - 1 : -1;
    loadOlder();
}

void HeaderBackfill::loadOlder()
{
    const QModelIndex list = m_model->index(0, 0, m_mailbox);
    if (!list.isValid()) {
        finish();
        return;
    }
    // Messages may have been expunged since the last step
    m_cursor = std::min(m_cursor, m_model->rowCount(list) - 1);
    int looked = 0;
    QList<TreeItemMessage *> batch;
    while (m_cursor >= 0 && batch.size() < m_olderBatch && looked < maxRowsPerStep) {
        const QModelIndex index = m_model->index(m_cursor, 0, list);
        --m_cursor;
        ++looked;
        auto *message = dynamic_cast<TreeItemMessage *>(static_cast<TreeItem *>(index.internalPointer()));
        if (needsHeader(message))
            batch << message;
    }
    request(batch);
    if (m_cursor < 0 || limitReached())
        finish();
}

int HeaderBackfill::request(const QList<TreeItemMessage *> &messages)
{
    int asked = 0;
    for (TreeItemMessage *message : messages) {
        if (limitReached())
            break;
        if (!needsHeader(message))
            continue;
        m_model->askForMsgMetadata(message, Model::PRELOAD_DISABLED);
        ++m_requested;
        ++asked;
    }
    return asked;
}

bool HeaderBackfill::limitReached() const
{
    return m_limit > 0 && m_requested >= m_limit;
}

void HeaderBackfill::finish()
{
    const QString mailbox = m_mailboxName;
    stop();
    emit finished(mailbox);
}

void HeaderBackfill::stopTask()
{
    if (!m_task)
        return;
    disconnect(m_task.data(), nullptr, this, nullptr);
    if (m_model && m_task->isPersistent()) {
        // Stop the server's updates of a CONTEXT=SEARCH result; this happens outside of the task's own signals
        QPointer<SortTask> task = m_task;
        QMetaObject::invokeMethod(
            m_model.data(),
            [task]() {
                if (task && task->isPersistent())
                    task->cancelSortingUpdates();
            },
            Qt::QueuedConnection);
    }
    m_task.clear();
}

}

}
