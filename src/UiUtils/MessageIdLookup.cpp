/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "MessageIdLookup.h"

#include "Imap/Model/FolderSearch.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/SearchIndex.h"
#include "Imap/Model/SearchIndexer.h"
#include "Imap/Model/SearchResultsModel.h"
#include "MessageId.h"

namespace UiUtils {

namespace {

MessageIdLookup::Location locationOf(const QString &accountId, const QModelIndex &message)
{
    MessageIdLookup::Location location;
    location.accountId = accountId;
    location.mailbox = message.data(Imap::Mailbox::RoleMailboxName).toString();
    // The role reads 0 while the list is being resynced, which happens right after a server search reselects it
    if (const auto *model = qobject_cast<const Imap::Mailbox::Model *>(message.model()))
        location.uidValidity = model->knownUidValidity(message);
    if (!location.uidValidity)
        location.uidValidity = message.data(Imap::Mailbox::RoleMailboxUidValidity).toUInt();
    location.uid = message.data(Imap::Mailbox::RoleMessageUid).toUInt();
    return location;
}

}

MessageIdLookup::MessageIdLookup(QObject *parent)
    : QObject(parent)
{
}

MessageIdLookup::~MessageIdLookup()
{
    stopSearches();
}

MessageIdLookup::State MessageIdLookup::start(const QByteArray &messageId, const QList<Account> &accounts)
{
    stopSearches();
    m_messageId = messageId;
    m_accounts = accounts;
    m_found.clear();
    m_failures.clear();
    m_state = State::Idle;
    if (MessageId::validate(messageId) != MessageId::Problem::None) {
        finish(State::NotFound);
        return m_state;
    }
    for (const Account &account : accounts) {
        if (!account.model)
            continue;
        const QModelIndexList loaded = account.model->findLoadedMessagesById(messageId);
        for (const QModelIndex &message : loaded)
            add(locationOf(account.id, message));
    }
    for (const Account &account : accounts) {
        if (!account.indexer || !account.indexer->isAvailable() || !account.indexer->index())
            continue;
        const auto indexed = account.indexer->index()->messagesWithId(QString::fromUtf8(messageId));
        for (const auto &entry : indexed) {
            Location location;
            location.accountId = account.id;
            location.mailbox = entry.mailbox;
            location.uidValidity = entry.uidValidity;
            location.uid = entry.uid;
            add(location);
        }
    }
    if (!m_found.isEmpty()) {
        finish(State::Found);
    } else if (!serverAccounts().isEmpty()) {
        m_state = State::NeedsServer;
        emit progressChanged();
    } else {
        finish(State::NotFound);
    }
    return m_state;
}

void MessageIdLookup::add(const Location &location)
{
    // Without a UIDVALIDITY the message could not be told apart from another one with the same UID
    if (location.mailbox.isEmpty() || !location.uidValidity || !location.uid)
        return;
    for (const Location &known : std::as_const(m_found)) {
        if (known.accountId == location.accountId && known.mailbox == location.mailbox && known.uid == location.uid)
            return;
    }
    m_found << location;
}

QStringList MessageIdLookup::serverAccounts() const
{
    QStringList ids;
    for (const Account &account : m_accounts) {
        if (account.model && account.model->isNetworkAvailable())
            ids << account.id;
    }
    return ids;
}

bool MessageIdLookup::searchServers()
{
    if (m_state != State::NeedsServer)
        return false;
    const QStringList conditions = MessageId::searchConditions(m_messageId);
    for (const Account &account : std::as_const(m_accounts)) {
        if (!account.model || !account.model->isNetworkAvailable())
            continue;
        auto *search = new Imap::Mailbox::FolderSearch(account.model, this);
        search->setSearchIndex(nullptr);
        search->setTimeouts(m_listingMs, m_searchMs, m_loadingMs);
        connect(search, &Imap::Mailbox::FolderSearch::progressChanged, this, &MessageIdLookup::progressChanged);
        connect(search, &Imap::Mailbox::FolderSearch::finished, this, [this, search]() {
            searchFinished(search);
        });
        m_searches << ServerSearch{account.id, search, false};
    }
    if (m_searches.isEmpty())
        return false;
    m_state = State::SearchingServers;
    // Started after all of them exist, so that one finishing at once cannot leave the others out
    const auto searches = m_searches;
    for (const ServerSearch &entry : searches) {
        if (entry.search)
            entry.search->start({}, true, conditions);
    }
    emit progressChanged();
    return true;
}

void MessageIdLookup::searchFinished(Imap::Mailbox::FolderSearch *search)
{
    if (m_state != State::SearchingServers)
        return;
    QString accountId;
    for (ServerSearch &entry : m_searches) {
        if (entry.search == search) {
            entry.done = true;
            accountId = entry.accountId;
        }
    }
    const auto failures = search->failures();
    for (const auto &failure : failures) {
        m_failures << (failure.mailbox.isEmpty() ? tr("%1: %2").arg(accountId, failure.reason)
                                                 : tr("%1: %2: %3").arg(accountId, failure.mailbox, failure.reason));
    }
    // The server matches any Message-ID which contains the one searched for
    const QModelIndexList messages = search->results()->messages();
    for (const QModelIndex &message : messages) {
        if (MessageId::sameId(message.data(Imap::Mailbox::RoleMessageMessageId).toByteArray(), m_messageId))
            add(locationOf(accountId, message));
    }
    if (search->model())
        emit selectionChanged(search->model());
    if (!m_found.isEmpty()) {
        stopSearches();
        finish(State::Found);
        return;
    }
    for (const ServerSearch &entry : std::as_const(m_searches)) {
        if (!entry.done && entry.search)
            return;
    }
    stopSearches();
    finish(State::NotFound);
}

void MessageIdLookup::cancel()
{
    if (m_state == State::NeedsServer || m_state == State::SearchingServers) {
        stopSearches();
        finish(m_found.isEmpty() ? State::NotFound : State::Found);
    }
}

void MessageIdLookup::stopSearches()
{
    const auto searches = m_searches;
    m_searches.clear();
    for (const ServerSearch &entry : searches) {
        if (!entry.search)
            continue;
        const bool running = entry.search->isRunning();
        QPointer<Imap::Mailbox::Model> model = entry.search->model();
        entry.search->cancel();
        entry.search->deleteLater();
        if (running && model)
            emit selectionChanged(model);
    }
}

void MessageIdLookup::finish(State state)
{
    m_state = state;
    emit progressChanged();
    emit finished();
}

MessageIdLookup::State MessageIdLookup::state() const
{
    return m_state;
}

QByteArray MessageIdLookup::messageId() const
{
    return m_messageId;
}

QList<MessageIdLookup::Location> MessageIdLookup::found() const
{
    return m_found;
}

QStringList MessageIdLookup::failures() const
{
    return m_failures;
}

int MessageIdLookup::folderCount() const
{
    int count = 0;
    for (const ServerSearch &entry : m_searches) {
        if (entry.search)
            count += entry.search->folderCount();
    }
    return count;
}

int MessageIdLookup::searchedCount() const
{
    int count = 0;
    for (const ServerSearch &entry : m_searches) {
        if (entry.search)
            count += entry.search->searchedCount();
    }
    return count;
}

void MessageIdLookup::setTimeouts(int listingMs, int searchMs, int loadingMs)
{
    m_listingMs = listingMs;
    m_searchMs = searchMs;
    m_loadingMs = loadingMs;
}

}
