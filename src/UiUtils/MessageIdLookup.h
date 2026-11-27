/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>

namespace Imap {
namespace Mailbox {
class FolderSearch;
class Model;
class SearchIndexer;
}
}

namespace UiUtils {

/** @short Find a message by its Message-ID in the accounts, first on this computer and then, if asked, on the servers

start() looks at the messages which the accounts have loaded and at their local search indexes, and returns at once.
When nothing is found there and an account is online, the state is NeedsServer: searchServers(), which the user has to
agree to, then searches every folder of each online account on its own connection. The first account which has the
message ends the search.

Searching a server may change which folder the account has selected; selectionChanged() asks whoever shows the
account's messages to select its folder again.
*/
class MessageIdLookup : public QObject
{
    Q_OBJECT
public:
    struct Account {
        QString id;
        QPointer<Imap::Mailbox::Model> model;
        QPointer<Imap::Mailbox::SearchIndexer> indexer;
    };

    struct Location {
        QString accountId;
        QString mailbox;
        /** 0 when unknown */
        uint uidValidity = 0;
        uint uid = 0;
    };

    enum class State {
        Idle,
        /** Found on this computer or on a server; see found() */
        Found,
        /** Not found on this computer, and an account could be searched on its server */
        NeedsServer,
        SearchingServers,
        NotFound,
    };

    explicit MessageIdLookup(QObject *parent = nullptr);
    ~MessageIdLookup() override;

    /** @short Look for @arg messageId, without angle brackets, in the loaded messages and indexes of @arg accounts */
    State start(const QByteArray &messageId, const QList<Account> &accounts);
    /** @short Search the servers of the online accounts; false when there is nothing to search */
    bool searchServers();
    /** @short Stop searching the servers; the state becomes NotFound unless the message was found */
    void cancel();

    State state() const;
    QByteArray messageId() const;
    /** @short Where the message is, loaded messages first, then indexed ones, then those found on a server */
    QList<Location> found() const;
    /** @short Places which could not be searched, as "account: folder: reason" or "account: reason" */
    QStringList failures() const;
    /** @short Accounts whose servers searchServers() would search, or searches */
    QStringList serverAccounts() const;
    int folderCount() const;
    int searchedCount() const;

    /** @short Limits for listing the folders, for an answer to a search and for loading envelopes; for tests */
    void setTimeouts(int listingMs, int searchMs, int loadingMs);

signals:
    void progressChanged();
    /** @short The state changed to Found or NotFound */
    void finished();
    /** @short A server search used the connection of @arg model, and finished or was stopped */
    void selectionChanged(Imap::Mailbox::Model *model);

private:
    struct ServerSearch {
        QString accountId;
        QPointer<Imap::Mailbox::FolderSearch> search;
        bool done = false;
    };

    void add(const Location &location);
    void searchFinished(Imap::Mailbox::FolderSearch *search);
    void stopSearches();
    void finish(State state);

    State m_state = State::Idle;
    QByteArray m_messageId;
    QList<Account> m_accounts;
    QList<Location> m_found;
    QStringList m_failures;
    QList<ServerSearch> m_searches;
    int m_listingMs = 30000;
    int m_searchMs = 60000;
    int m_loadingMs = 30000;
};

}
