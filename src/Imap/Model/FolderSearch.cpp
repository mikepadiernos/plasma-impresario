/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "FolderSearch.h"

#include <algorithm>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/SearchIndexer.h"
#include "Imap/Model/SearchResultsModel.h"
#include "Imap/Model/TaskFactory.h"
#include "Imap/Tasks/SortTask.h"
#include "UiUtils/SearchScope.h"

namespace Imap
{

namespace Mailbox
{

FolderSearch::FolderSearch(Model *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_results(new SearchResultsModel(this))
{
    m_results->setSourceModel(model);
    m_walkTimer.setSingleShot(true);
    m_walkTimer.setInterval(100);
    connect(&m_walkTimer, &QTimer::timeout, this, &FolderSearch::list);
    m_deadline.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this, [this]() {
        switch (m_phase) {
        case Phase::Listing:
            listingTimedOut();
            break;
        case Phase::Searching:
            searchFailed(m_generation, tr("The server did not answer in time"));
            break;
        case Phase::Opening:
            m_loadPoll.stop();
            m_opening = QPersistentModelIndex();
            fail(m_current, tr("The folder could not be opened from the offline cache in time"));
            searchNext();
            break;
        case Phase::Loading:
            // Show what has arrived; the rest loads when the list asks for it
            m_loadPoll.stop();
            m_loading.clear();
            searchNext();
            break;
        case Phase::Idle:
            break;
        }
    });
    m_loadPoll.setInterval(200);
    connect(&m_loadPoll, &QTimer::timeout, this, [this]() {
        if (m_phase == Phase::Opening)
            checkOpened();
        else
            checkLoaded();
    });
}

FolderSearch::~FolderSearch()
{
    cancel();
}

void FolderSearch::setTimeouts(int listingMs, int searchMs, int loadingMs)
{
    m_listingMs = listingMs;
    m_searchMs = searchMs;
    m_loadingMs = loadingMs;
}

void FolderSearch::setLimits(int folders, int perFolder)
{
    m_folderLimit = std::max(0, folders);
    m_resultLimit = std::max(0, perFolder);
}

bool FolderSearch::isTruncated() const
{
    return m_truncated;
}

void FolderSearch::setSearchIndex(SearchIndexer *indexer)
{
    m_indexer = indexer;
}

bool FolderSearch::usedLocalIndex() const
{
    return m_local;
}

int FolderSearch::messagesWithoutText() const
{
    return m_local && m_localResult.usesText ? m_localResult.withoutText : 0;
}

QStringList FolderSearch::incompleteFolders() const
{
    return m_local ? m_incompleteFolders : QStringList();
}

bool FolderSearch::indexPending() const
{
    return m_local && m_indexPending;
}

bool FolderSearch::indexPaused() const
{
    return m_local && m_indexPaused;
}

Model *FolderSearch::model() const
{
    return m_model;
}

SearchResultsModel *FolderSearch::results() const
{
    return m_results;
}

int FolderSearch::folderCount() const
{
    return m_folderCount;
}

int FolderSearch::searchedCount() const
{
    return m_searchedCount;
}

QString FolderSearch::currentFolder() const
{
    return m_phase == Phase::Opening || m_phase == Phase::Searching || m_phase == Phase::Loading ? m_current : QString();
}

QList<FolderSearch::Failure> FolderSearch::failures() const
{
    return m_failures;
}

bool FolderSearch::isRunning() const
{
    return m_phase != Phase::Idle;
}

void FolderSearch::start(const QStringList &mailboxes, bool allFolders, const QStringList &conditions)
{
    m_conditions = conditions;
    begin(mailboxes, allFolders, false);
}

void FolderSearch::retry(const QStringList &mailboxes, bool allFolders)
{
    begin(mailboxes, allFolders, true);
}

void FolderSearch::begin(const QStringList &mailboxes, bool allFolders, bool keepResults)
{
    cancel();
    if (!keepResults)
        m_results->clear();
    m_failures.clear();
    m_searchedAgain.clear();
    m_truncated = false;
    m_requested = UiUtils::SearchScope::searchOrder(mailboxes);
    m_allFolders = allFolders;
    m_queue.clear();
    m_current.clear();
    m_folderCount = allFolders ? 0 : int(m_requested.size());
    m_searchedCount = 0;
    m_phase = Phase::Listing;
    const quint64 generation = m_generation;
    m_local = m_model && !m_model->isNetworkAvailable() && m_indexer && m_indexer->isAvailable();
    m_localResult = SearchIndex::Result();
    m_incompleteFolders.clear();
    m_indexPending = false;
    m_indexPaused = false;
    QString problem;
    if (m_local) {
        // One query answers every folder; the folders are still listed, from the offline cache, to show the matches
        m_localResult = m_indexer->index()->search(m_conditions, QStringList());
        problem = m_localResult.problem;
        const QList<SearchIndexer::FolderCoverage> coverage = m_indexer->coverage();
        QSet<QString> known;
        for (const SearchIndexer::FolderCoverage &folder : coverage) {
            known.insert(folder.mailbox);
            if (folder.state != SearchIndexer::FolderCoverage::State::Indexed
                && (allFolders || m_requested.contains(folder.mailbox)))
                m_incompleteFolders << folder.mailbox;
        }
        if (!allFolders) {
            // Chosen folders which the cache does not even list
            for (const QString &mailbox : std::as_const(m_requested)) {
                if (!known.contains(mailbox))
                    m_incompleteFolders << mailbox;
            }
        }
        // A pass merely scheduled after browsing rarely changes anything; one under way may still add messages
        m_indexPending = m_indexer->isRunning();
        m_indexPaused = m_indexer->isPaused();
    } else if (!m_model) {
        problem = tr("The account is not available");
    } else if (!m_model->isNetworkAvailable()) {
        problem = tr("The account is offline");
    }

    if (!problem.isEmpty()) {
        // Report it once the caller is done starting searches
        QMetaObject::invokeMethod(this, [this, generation, problem]() {
            if (generation != m_generation)
                return;
            fail(QString(), problem);
            finish();
        }, Qt::QueuedConnection);
        return;
    }

    // New folders arrive as rows of the account's model
    const auto relist = [this]() {
        if (m_phase == Phase::Listing)
            m_walkTimer.start();
    };
    m_modelConnections << connect(m_model, &QAbstractItemModel::rowsInserted, this, relist);
    m_modelConnections << connect(m_model, &QAbstractItemModel::dataChanged, this, relist);
    m_modelConnections << connect(m_model, &QAbstractItemModel::layoutChanged, this, relist);
    m_modelConnections << connect(m_model, &QAbstractItemModel::modelReset, this, relist);
    m_modelConnections << connect(m_model, &QObject::destroyed, this, [this]() {
        m_phase = Phase::Idle;
        m_opening = QPersistentModelIndex();
        m_deadline.stop();
        m_walkTimer.stop();
        m_loadPoll.stop();
    });
    m_deadline.start(m_listingMs);
    emit progressChanged();
    QMetaObject::invokeMethod(this, [this, generation]() {
        if (generation == m_generation)
            list();
    }, Qt::QueuedConnection);
}

void FolderSearch::cancel()
{
    ++m_generation;
    m_deadline.stop();
    m_walkTimer.stop();
    m_loadPoll.stop();
    m_loading.clear();
    m_opening = QPersistentModelIndex();
    stopTask();
    disconnectModel();
    m_phase = Phase::Idle;
}

void FolderSearch::stopTask()
{
    if (!m_task)
        return;
    disconnect(m_task.data(), nullptr, this, nullptr);
    if (m_task->isPersistent()) {
        // Stop the server's updates of a CONTEXT=SEARCH result; this happens outside of the task's own signals
        QPointer<SortTask> task = m_task;
        QMetaObject::invokeMethod(this, [task]() {
            if (task && task->isPersistent())
                task->cancelSortingUpdates();
        }, Qt::QueuedConnection);
    }
    m_task.clear();
}

void FolderSearch::disconnectModel()
{
    for (const QMetaObject::Connection &connection : std::as_const(m_modelConnections))
        disconnect(connection);
    m_modelConnections.clear();
}

bool FolderSearch::walk(TreeItemMailbox *mailbox, QStringList &found, bool everything)
{
    if (!mailbox->fetched()) {
        if (mailbox->isUnavailable())
            return true;
        // Asks the server for the child folders, unless that is already under way
        mailbox->fetch(m_model);
        if (!mailbox->fetched())
            return false;
    }
    bool complete = true;
    const int count = int(mailbox->childrenCount(m_model));
    for (int i = 0; i < count; ++i) {
        auto *child = dynamic_cast<TreeItemMailbox *>(mailbox->child(i, m_model));
        if (!child)
            continue;
        if (everything) {
            if (child->isSelectable())
                found << child->mailbox();
        } else {
            // Only the branches which lead to a requested folder which is not known yet
            const QString prefix = child->mailbox() + child->separator();
            const bool leads = std::any_of(m_requested.cbegin(), m_requested.cend(), [&](const QString &name) {
                return name.startsWith(prefix) && !m_model->findMailboxByName(name);
            });
            if (!leads)
                continue;
        }
        if (!walk(child, found, everything))
            complete = false;
    }
    return complete;
}

void FolderSearch::list()
{
    if (m_phase != Phase::Listing || !m_model)
        return;
    QStringList found;
    bool complete = true;
    if (m_allFolders) {
        complete = walk(m_model->m_mailboxes, found, true);
        m_folderCount = int(found.size());
        emit progressChanged();
    } else {
        const bool allKnown = std::all_of(m_requested.cbegin(), m_requested.cend(), [this](const QString &name) {
            return m_model->findMailboxByName(name) != nullptr;
        });
        if (!allKnown)
            complete = walk(m_model->m_mailboxes, found, false);
    }
    if (!complete)
        return;
    m_deadline.stop();
    m_walkTimer.stop();
    disconnectModel();
    m_queue = m_allFolders ? UiUtils::SearchScope::searchOrder(found) : m_requested;
    limitQueue();
    if (m_allFolders && m_queue.isEmpty())
        fail(QString(), tr("The account has no folders which can be searched"));
    m_phase = Phase::Searching;
    emit progressChanged();
    searchNext();
}

void FolderSearch::listingTimedOut()
{
    if (m_phase != Phase::Listing || !m_model)
        return;
    // Search what is known; report the rest
    QStringList found;
    if (m_allFolders)
        walk(m_model->m_mailboxes, found, true);
    m_walkTimer.stop();
    disconnectModel();
    m_queue = m_allFolders ? UiUtils::SearchScope::searchOrder(found) : m_requested;
    limitQueue();
    if (m_allFolders)
        fail(QString(), tr("Not all folders could be listed in time; only the listed ones were searched"));
    m_phase = Phase::Searching;
    emit progressChanged();
    searchNext();
}

void FolderSearch::limitQueue()
{
    if (m_folderLimit > 0 && m_queue.size() > m_folderLimit) {
        m_queue = m_queue.mid(0, m_folderLimit);
        m_truncated = true;
    }
    m_folderCount = int(m_queue.size());
}

void FolderSearch::searchNext()
{
    if (m_phase == Phase::Idle)
        return;
    m_phase = Phase::Searching;
    while (!m_queue.isEmpty()) {
        m_current = m_queue.takeFirst();
        if (!m_model) {
            fail(m_current, tr("The account is not available"));
            continue;
        }
        if (!m_local && !m_model->isNetworkAvailable()) {
            fail(m_current, tr("The account went offline"));
            continue;
        }
        TreeItemMailbox *mailbox = m_model->findMailboxByName(m_current);
        if (!mailbox) {
            fail(m_current, m_local ? tr("The folder is not in the offline cache") : tr("The folder was not found on the server"));
            continue;
        }
        if (!mailbox->isSelectable()) {
            fail(m_current, tr("The folder cannot contain messages"));
            continue;
        }
        if (m_local) {
            openLocally(mailbox);
            return;
        }
        const quint64 generation = m_generation;
        m_currentMailbox = mailbox->toIndex(m_model);
        m_task = m_model->m_taskFactory->createSortTask(m_model, mailbox->toIndex(m_model), m_conditions, QStringList());
        connect(m_task.data(), &SortTask::sortingAvailable, this, [this, generation](const Imap::Uids &uids) {
            searchAnswered(generation, uids);
        });
        connect(m_task.data(), &ImapTask::failed, this, [this, generation](const QString &reason) {
            searchFailed(generation, reason);
        });
        connect(m_task.data(), &QObject::destroyed, this, [this, generation]() {
            if (m_phase == Phase::Searching)
                searchFailed(generation, tr("The connection closed before the server answered"));
        });
        m_deadline.start(m_searchMs);
        emit progressChanged();
        return;
    }
    finish();
}

void FolderSearch::searchAnswered(quint64 generation, const Imap::Uids &uids)
{
    if (generation != m_generation || m_phase != Phase::Searching)
        return;
    m_deadline.stop();
    stopTask();
    TreeItemMailbox *mailbox = m_model ? m_model->findMailboxByName(m_current) : nullptr;
    if (!mailbox) {
        fail(m_current, tr("The folder disappeared during the search"));
        searchNext();
        return;
    }
    ++m_searchedCount;
    Imap::Uids sorted = uids;
    std::sort(sorted.begin(), sorted.end());
    if (m_resultLimit > 0 && sorted.size() > m_resultLimit) {
        sorted = sorted.mid(sorted.size() - m_resultLimit);
        m_truncated = true;
    }
    QModelIndexList messages;
    const QList<TreeItemMessage *> items = m_model->findMessagesByUids(mailbox, sorted);
    for (TreeItemMessage *item : items)
        messages << item->toIndex(m_model);
    m_results->addMessages(messages);
    m_loading.clear();
    for (const QModelIndex &message : std::as_const(messages)) {
        // Asks for the envelope, which the list shows
        message.data(RoleMessageSubject);
        m_loading << QPersistentModelIndex(message);
    }
    m_phase = Phase::Loading;
    emit progressChanged();
    m_deadline.start(m_loadingMs);
    m_loadPoll.start();
    checkLoaded();
}

void FolderSearch::checkLoaded()
{
    if (m_phase != Phase::Loading)
        return;
    const bool done = std::all_of(m_loading.cbegin(), m_loading.cend(), [](const QPersistentModelIndex &message) {
        // Offline, messages whose envelope is not in the cache become unavailable instead of arriving
        return !message.isValid() || message.data(RoleIsFetched).toBool() || message.data(RoleIsUnavailable).toBool();
    });
    if (!done)
        return;
    m_loadPoll.stop();
    m_deadline.stop();
    m_loading.clear();
    const quint64 generation = m_generation;
    QMetaObject::invokeMethod(this, [this, generation]() {
        if (generation == m_generation)
            searchNext();
    }, Qt::QueuedConnection);
}

void FolderSearch::openLocally(TreeItemMailbox *mailbox)
{
    auto *list = dynamic_cast<TreeItemMsgList *>(mailbox->child(0, m_model));
    if (!list) {
        fail(m_current, tr("The folder cannot contain messages"));
        const quint64 generation = m_generation;
        QMetaObject::invokeMethod(this, [this, generation]() {
            if (generation == m_generation)
                searchNext();
        }, Qt::QueuedConnection);
        return;
    }
    m_currentMailbox = mailbox->toIndex(m_model);
    m_opening = list->toIndex(m_model);
    // Offline, the message list is filled from the cache at the end of the event loop
    list->fetch(m_model);
    m_phase = Phase::Opening;
    emit progressChanged();
    m_deadline.start(m_loadingMs);
    m_loadPoll.start();
    checkOpened();
}

void FolderSearch::checkOpened()
{
    if (m_phase != Phase::Opening)
        return;
    if (m_opening.isValid() && !m_opening.data(RoleIsFetched).toBool() && !m_opening.data(RoleIsUnavailable).toBool())
        return;
    m_loadPoll.stop();
    m_deadline.stop();
    const bool usable = m_opening.isValid() && m_opening.data(RoleIsFetched).toBool();
    m_opening = QPersistentModelIndex();
    const quint64 generation = m_generation;
    if (!usable) {
        fail(m_current, tr("The folder's messages are not in the offline cache"));
        QMetaObject::invokeMethod(this, [this, generation]() {
            if (generation == m_generation)
                searchNext();
        }, Qt::QueuedConnection);
        return;
    }
    m_phase = Phase::Searching;
    searchAnswered(generation, m_localResult.matches.value(m_current));
}

void FolderSearch::searchFailed(quint64 generation, const QString &reason)
{
    if (generation != m_generation || m_phase != Phase::Searching)
        return;
    m_deadline.stop();
    stopTask();
    if (!m_currentMailbox.isValid() && !m_searchedAgain.contains(m_current)) {
        // The folders were listed again while this one was opened, for example after reconnecting, which replaced it
        m_searchedAgain.insert(m_current);
        m_queue.prepend(m_current);
    } else {
        fail(m_current, reason.isEmpty() ? tr("The server refused the search") : reason);
    }
    const quint64 current = m_generation;
    QMetaObject::invokeMethod(this, [this, current]() {
        if (current == m_generation)
            searchNext();
    }, Qt::QueuedConnection);
}

void FolderSearch::fail(const QString &mailbox, const QString &reason)
{
    if (!mailbox.isEmpty())
        ++m_searchedCount;
    m_failures.append({mailbox, reason});
    emit folderFailed(mailbox, reason);
    emit progressChanged();
}

void FolderSearch::finish()
{
    m_deadline.stop();
    m_walkTimer.stop();
    m_loadPoll.stop();
    stopTask();
    disconnectModel();
    m_current.clear();
    m_phase = Phase::Idle;
    emit progressChanged();
    emit finished();
}

}

}
