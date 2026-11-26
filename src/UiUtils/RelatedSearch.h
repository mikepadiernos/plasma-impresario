/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QObject>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QStringList>

#include "UiUtils/RelatedMessages.h"

namespace Imap {
namespace Mailbox {
class FolderSearch;
class Model;
}
}

namespace UiUtils {

/** @short IMAP SEARCH keys for the messages related to @arg message

@arg message is a fetched message of the IMAP model or of a model above it. The keys match the messages which its
Message-ID, In-Reply-To and References name, the replies to it, the other messages which refer to the first message of
its thread, and the messages from its sender. Only the last @arg maximumReferences references are used, so a long
thread keeps the command short. The result is empty when the message has nothing to search for.
*/
QStringList relatedSearchConditions(const QModelIndex &message, int maximumReferences = 10);

/** @short Sort the @arg found messages into those of the conversation of @arg current and those from its sender

@arg found are messages which matched relatedSearchConditions() for @arg current. The conversation follows
groupConversations(), as the local search does; the server matches header keys as substrings, so a message which
neither belongs to the conversation nor comes from exactly the same address is left out, as are @arg current itself and
messages which are not fetched. Each group goes from the newest to the oldest message by date and holds at most
@arg maximumPerGroup messages.
*/
RelatedMessages classifyRelated(const QModelIndex &current, const QList<QPersistentModelIndex> &found, int maximumPerGroup = 25);

/** @short Search the server for the messages related to one message, in its folder or in other folders of its account

The search runs in the background on the account's own connection and is bounded: at most folderLimit() folders and
the newest resultLimit() matches of each folder. Starting another search, or cancel(), stops the previous one, and its
results are never reported for the new message.

Searching may change which folder the account has selected; selectionChanged() asks whoever shows the account's
messages to select its folder again.
*/
class RelatedSearch : public QObject
{
    Q_OBJECT
public:
    enum class Scope {
        MessageFolder, /**< The folder of the message */
        AllFolders, /**< Every folder of the account, up to folderLimit() */
        ChosenFolders, /**< The folders given to start() */
    };

    explicit RelatedSearch(QObject *parent = nullptr);
    ~RelatedSearch() override;

    static constexpr int folderLimit() { return 20; }
    static constexpr int resultLimit() { return 50; }

    /** @short Search for the messages related to @arg message, which must be a fetched message of an IMAP model

    @arg folders are full mailbox names for Scope::ChosenFolders. Returns false, with problem() telling why, when no
    search could be started.
    */
    bool start(const QModelIndex &message, Scope scope, const QStringList &folders = QStringList());
    void cancel();

    bool isRunning() const;
    /** @short The message of the current or last search, as an index of the IMAP model */
    QModelIndex message() const;
    /** @short Why the last search could not start, or empty */
    QString problem() const;
    /** @short The related messages found so far, see classifyRelated() */
    RelatedMessages results(int maximumPerGroup = 25) const;
    /** @short Folders which could not be searched, as "folder: reason" or the reason for the whole account */
    QStringList failures() const;
    int folderCount() const;
    int searchedCount() const;
    /** @short Whether folders or matches were left out because of the limits */
    bool isTruncated() const;

    /** @short Limits for listing the folders, for an answer to a search and for loading envelopes; for tests */
    void setTimeouts(int listingMs, int searchMs, int loadingMs);

signals:
    /** @short Results or progress changed */
    void changed();
    void finished();
    /** @short The search used the connection of @arg model, and finished or was stopped; select the shown folder again */
    void selectionChanged(Imap::Mailbox::Model *model);

private:
    void searchFinished();
    void stop(bool notify);

    QPointer<Imap::Mailbox::FolderSearch> m_search;
    QPersistentModelIndex m_message;
    QString m_mailbox;
    QString m_problem;
    int m_listingMs = 30000;
    int m_searchMs = 60000;
    int m_loadingMs = 30000;
};

}
