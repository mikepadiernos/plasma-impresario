/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef JMAP_SYNCJOB_H
#define JMAP_SYNCJOB_H

#include <memory>

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>

#include "Jmap/Client.h"

namespace LocalMail {
class Store;
}

namespace Jmap {

/**
 * @short One synchronization of a JMAP account with the mail kept on this computer
 *
 * The server's mailboxes become local folders, its special mailboxes under the usual names (INBOX, Sent, Drafts,
 * Trash, Junk, Archive). Each local copy of a server message carries the origin "jmap:<email id>"; a message in
 * several mailboxes has a copy in each folder.
 *
 * Changes made on this computer since the last visit go first: new, renamed and removed folders; read, flagged and
 * other keywords; messages moved, copied or removed; and messages stored locally, such as drafts and sent copies,
 * which are uploaded. Then the server's changes come down, through Email/changes when the server can tell them, or
 * by comparing everything otherwise. What was last agreed with the server is kept in @ref stateFileName() in the
 * store's folder.
 *
 * Folders the server does not have when the account is first synchronized, such as a local Trash, stay on this
 * computer only; moving a message into one of them removes it from the server.
 */
class SyncJob : public QObject
{
    Q_OBJECT
public:
    using Failure = Client::Failure;

    SyncJob(const Settings &settings, std::shared_ptr<LocalMail::Store> store, QObject *parent = nullptr);
    ~SyncJob() override;

    void setTrustCheck(Client::TrustCheck check);
    void setTimeout(int seconds);

    void start();
    void abort();
    bool isRunning() const;

    /** Messages downloaded during this synchronization */
    int downloaded() const;
    /** Messages stored on this computer and uploaded during this synchronization */
    int uploaded() const;
    /** Whether the server offers sending, known once the session was read */
    bool canSubmit() const;
    /** Changes the server refused, which are tried again next time */
    QStringList warnings() const;

    /** The file in the store's folder which keeps the synchronization state */
    static QString stateFileName();
    /** IMAP flags for JMAP keywords, such as \Seen for $seen */
    static QStringList keywordsToFlags(const QStringList &keywords);
    /** JMAP keywords for IMAP flags, in lower case; \Deleted, \Recent and unknown system flags have none */
    static QStringList flagsToKeywords(const QStringList &flags);

signals:
    void certificateRejected(const QList<QSslCertificate> &chain, const QList<QSslError> &errors);
    void failed(Jmap::Client::Failure failure, const QString &message);
    void succeeded(int downloaded);

private:
    struct MailboxState {
        QString name;
        uint uidValidity = 0;
        QString serverName;
        QString parentId;
        QString role;
    };
    struct EmailState {
        QStringList keywords;
        QStringList mailboxIds;
    };
    struct Copy {
        QString folder;
        uint uid = 0;
        QStringList flags;
    };
    struct Import {
        QString folder;
        uint uid = 0;
        QString mailboxId;
        QStringList keywords;
        QDateTime date;
        QString blobId;
    };

    void loadState();
    bool saveState();
    QHash<uint, QString> localFolders() const;
    QHash<uint, QString> mailboxIdsByValidity() const;

    void onSession();
    void pushMailboxes();
    void pushEmails();
    void runEmailSet();
    void uploadNext();
    void runImports();
    void destroyMailboxes();
    void pullMailboxes();
    void pullEmails();
    void requestChanges();
    void fullResync();
    void queryPage(int position);
    void removeDestroyed();
    void fetchNextChunk();
    void applyNext();
    bool applyEmail(const QJsonObject &email, const QByteArray &content);
    void indexCopies();
    void finish();
    void fail(Failure failure, const QString &message);
    void storageFailure(const QString &error);
    void warn(const QString &message);

    Settings m_settings;
    std::shared_ptr<LocalMail::Store> m_store;
    QPointer<Client> m_client;
    bool m_running = false;

    QString m_accountId;
    QString m_emailState;
    QHash<QString, MailboxState> m_mailboxes;
    QSet<uint> m_localOnly;
    QHash<QString, EmailState> m_emails;
    bool m_hadState = false;

    QStringList m_pendingDestroy;
    QJsonObject m_emailUpdates;
    QStringList m_emailDestroys;
    QHash<QString, EmailState> m_afterUpdate;
    QList<Import> m_imports;
    int m_importCursor = 0;

    QString m_newEmailState;
    bool m_full = false;
    QStringList m_toFetch;
    QSet<QString> m_toFetchSet;
    QStringList m_destroyed;
    QSet<QString> m_serverIds;
    int m_fetchCursor = 0;
    QList<QJsonObject> m_applyQueue;
    QHash<QString, QList<Copy>> m_copies;
    QHash<QString, QString> m_folderByMailboxId;

    int m_downloaded = 0;
    int m_uploaded = 0;
    bool m_canSubmit = false;
    QStringList m_warnings;
};

}

#endif
