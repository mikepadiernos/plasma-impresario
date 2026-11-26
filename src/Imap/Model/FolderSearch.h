/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef IMAP_MODEL_FOLDERSEARCH_H
#define IMAP_MODEL_FOLDERSEARCH_H

#include <QList>
#include <QObject>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include "Imap/Parser/Uids.h"

namespace Imap
{

namespace Mailbox
{

class Model;
class SearchResultsModel;
class SortTask;
class TreeItemMailbox;

/** @short Search several folders of one account, one folder after another

The folders are selected in turn on the account's own connection and searched with the given IMAP SEARCH keys; the
matching messages are collected in results(), which point at the messages of the account's Model. The envelopes of the
matches are loaded before the next folder is selected, so that the list can show them.

With allFolders, the folders are listed first, as far as the server lets them be listed. Folders which cannot be
searched, such as those which are missing, cannot be selected or fail on the server, are reported through
folderFailed(); an empty mailbox name means that the whole account could not be searched.

The search changes which folder of the account is selected; whoever starts it selects the shown folder again
afterwards.
*/
class FolderSearch : public QObject
{
    Q_OBJECT
public:
    struct Failure {
        /** Full mailbox name, or empty for the whole account */
        QString mailbox;
        QString reason;
    };

    explicit FolderSearch(Model *model, QObject *parent = nullptr);
    ~FolderSearch() override;

    /** @short Search @p mailboxes, or every folder for @p allFolders, for messages matching @p conditions

    A running search is cancelled first. The folders are searched in the order of @p mailboxes; listed folders start
    with INBOX.
    */
    void start(const QStringList &mailboxes, bool allFolders, const QStringList &conditions);
    /** @short Search @p mailboxes, or every folder for @p allFolders, again with the conditions of start()

    The results found so far stay, and those found again are not repeated; the failures and the progress cover only
    this search. Meant for the places which could not be searched before.
    */
    void retry(const QStringList &mailboxes, bool allFolders);
    /** @short Stop without emitting finished(); the results found so far stay */
    void cancel();
    bool isRunning() const;

    Model *model() const;
    SearchResultsModel *results() const;
    /** @short Number of folders to search, as far as they are known yet */
    int folderCount() const;
    /** @short Number of folders done, searched or failed */
    int searchedCount() const;
    /** @short The folder being searched, or empty */
    QString currentFolder() const;
    QList<Failure> failures() const;

    /** @short Limits for listing the folders, for an answer to a search and for loading envelopes; for tests */
    void setTimeouts(int listingMs, int searchMs, int loadingMs);

signals:
    void progressChanged();
    void folderFailed(const QString &mailbox, const QString &reason);
    void finished();

private:
    enum class Phase {
        Idle,
        Listing,
        Searching,
        Loading,
    };

    void begin(const QStringList &mailboxes, bool allFolders, bool keepResults);
    void list();
    /** @short Walk the folders below @p mailbox, asking the server for those not listed yet; true when all are known */
    bool walk(TreeItemMailbox *mailbox, QStringList &found, bool everything);
    void listingTimedOut();
    void searchNext();
    void searchAnswered(quint64 generation, const Imap::Uids &uids);
    void searchFailed(quint64 generation, const QString &reason);
    void checkLoaded();
    void fail(const QString &mailbox, const QString &reason);
    void finish();
    void stopTask();
    void disconnectModel();

    QPointer<Model> m_model;
    SearchResultsModel *m_results;
    quint64 m_generation = 0;
    Phase m_phase = Phase::Idle;
    QStringList m_requested;
    bool m_allFolders = false;
    QStringList m_conditions;
    QStringList m_queue;
    QString m_current;
    /** The folder being searched; invalid once a new listing of the folders replaced it */
    QPersistentModelIndex m_currentMailbox;
    /** Folders searched once more because a new listing replaced them during their search */
    QSet<QString> m_searchedAgain;
    int m_folderCount = 0;
    int m_searchedCount = 0;
    QList<Failure> m_failures;
    QPointer<SortTask> m_task;
    QList<QPersistentModelIndex> m_loading;
    QList<QMetaObject::Connection> m_modelConnections;
    QTimer m_walkTimer;
    QTimer m_deadline;
    QTimer m_loadPoll;
    int m_listingMs = 30000;
    int m_searchMs = 60000;
    int m_loadingMs = 30000;
};

}

}

#endif
