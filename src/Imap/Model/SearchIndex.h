/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef IMAP_MODEL_SEARCHINDEX_H
#define IMAP_MODEL_SEARCHINDEX_H

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include "Imap/Parser/Uids.h"

namespace Imap
{
namespace Mailbox
{

/** @short A full-text index of the mail in an account's offline cache

The index lives in the account's SQLite cache, next to the data it is built from, and uses the cache's own database
connection. Its text is kept in an SQLite FTS5 table with the trigram tokenizer, which finds any part of a word
regardless of case, as IMAP SEARCH does; the other conditions of a search, such as flags, dates, sizes and the
mailing-list headers, are columns of an ordinary table.

Only what the cache already holds is indexed: the headers of the messages and the text of the parts which were
downloaded. Building the index never fetches anything; see SearchIndexer.

The index answers the IMAP SEARCH keys which the search field produces, so that a search gives the same matches
offline as on the server where it can; keys it cannot answer are reported instead of being ignored.
*/
class SearchIndex
{
    Q_DECLARE_TR_FUNCTIONS(Imap::Mailbox::SearchIndex)
public:
    /** @short What the index keeps of one message */
    struct Document {
        QString mailbox;
        uint uid = 0;
        /** Day of the Date header and of the INTERNALDATE */
        QDate sent;
        QDate received;
        quint64 size = 0;
        QStringList flags;
        /** Top-level media type in lower case, such as multipart/mixed */
        QString contentType;
        bool hasAttachment = false;
        QString messageId;
        QString inReplyTo;
        QString references;
        QString listId;
        QString listPost;
        /** Whether the text of the message was downloaded, or it has no text to download */
        bool hasText = false;
        /** Ids of the text parts, which are checked again while the text is missing */
        QStringList textParts;
        QString subject;
        QString from;
        QString to;
        QString cc;
        QString bcc;
        QString body;
        /** Names of the attached files and messages */
        QString attachments;
        /** Text read from the attached files */
        QString attachmentText;
        /** Ids of attached files whose text could be read once they are downloaded */
        QStringList attachmentParts;
        /** One line per attached file which was read or tried: its state (read, none, large or failed), a space and
            its MIME type */
        QStringList attachmentReport;
    };

    /** @short What is known about an indexed message without reading its text */
    struct Entry {
        bool hasText = false;
        QStringList flags;
        QStringList textParts;
        QStringList attachmentParts;
    };

    /** @short What became of the attached files of the indexed messages */
    struct AttachmentSummary {
        /** Files whose text is in the index */
        int read = 0;
        /** Files waiting to be downloaded, whose text is not in the index */
        int notDownloaded = 0;
        /** Files larger than the limit, and files the extractor could not read */
        int tooLarge = 0;
        int failed = 0;
        /** Files of each MIME type for which no text extractor is installed */
        QHash<QString, int> withoutExtractor;
    };

    /** @short State of one folder in the index */
    struct Folder {
        QString mailbox;
        uint uidValidity = 0;
        /** Messages in the cached folder, messages indexed and those of them with their text */
        int messages = 0;
        int indexed = 0;
        int withText = 0;
        /** When the folder was last indexed; invalid when never */
        QDateTime updated;
    };

    /** @short Where an indexed message is */
    struct Location {
        QString mailbox;
        uint uid = 0;
        /** UIDVALIDITY of the folder when it was indexed; 0 when unknown */
        uint uidValidity = 0;
    };

    /** @short The headers of an indexed message which a quick search shows */
    struct Summary {
        Location location;
        QString subject;
        /** The From addresses, as indexed */
        QString sender;
        /** Day of the Date header; invalid when the message has none */
        QDate sent;
        /** The Message-ID header as indexed; filled in by followUps() only */
        QString messageId;
        /** Whether the message carries the $TODO keyword; filled in by followUps() only */
        bool toDo = false;
    };

    struct Result {
        /** Why the index could not answer, or empty */
        QString problem;
        /** Matching UIDs of each folder, in ascending order */
        QHash<QString, Imap::Uids> matches;
        /** Whether the search looks at the text of the messages */
        bool usesText = false;
        /** Messages in the searched folders whose text was not downloaded, so the text could not be searched */
        int withoutText = 0;
    };

    /** @short Use the SQLite database connection called @arg connectionName; open() creates the tables */
    explicit SearchIndex(const QString &connectionName);

    /** @short Check that SQLite offers FTS5 with the trigram tokenizer and create the tables if needed */
    bool open();
    bool isOpen() const;
    /** @short The last error, for the user */
    QString errorString() const;

    /** @short Whether @arg db supports the index; @arg problem says why not */
    static bool supportsFullText(const QSqlDatabase &db, QString *problem = nullptr);

    /** @short Remove the index from the database connection called @arg connectionName, if there is one */
    static bool drop(const QString &connectionName, QString *problem = nullptr);

    /** @short Forget every folder and message, keeping the tables, so that the index can be built again */
    bool clear();

    QList<Folder> folders() const;
    /** @short The state of @arg mailbox; its uidValidity is 0 when the folder is not in the index */
    Folder folder(const QString &mailbox) const;
    bool setFolder(const Folder &folder);
    /** @short Forget a folder and its messages */
    bool removeFolder(const QString &mailbox);

    QHash<uint, Entry> entries(const QString &mailbox) const;
    /** @short Add or replace the messages; a replaced message loses any decrypted text added to it */
    bool store(const QList<Document> &documents);
    /** @short Whether the message is in the index */
    bool hasMessage(const QString &mailbox, uint uid) const;
    /** @short Add @arg text, the decrypted text of an encrypted message which is in the index, to its body

    Text which the message already has is not added again. The message is marked, so that removeDecrypted() can take
    the decrypted text out of the index again.
    */
    bool addDecryptedText(const QString &mailbox, uint uid, const QString &text);
    /** @short Number of messages holding decrypted text */
    int decryptedMessages() const;
    /** @short Forget the messages holding decrypted text, so that they are indexed again without it */
    bool removeDecrypted();
    /** @short Counts of the attached files which were read, tried or are waiting */
    AttachmentSummary attachmentSummary() const;
    /** @short Forget the messages with attached files which were read or tried, so that they are indexed again */
    bool forgetAttachmentText();
    /** @short A value which the index keeps about how it was built, or an empty string */
    QString setting(const QString &key) const;
    bool setSetting(const QString &key, const QString &value);
    bool setFlags(const QString &mailbox, const QHash<uint, QStringList> &flags);
    bool remove(const QString &mailbox, const QList<uint> &uids);

    /** @short Indexed messages whose Message-ID is exactly @arg messageId, given without angle brackets, newest first

    Unlike a HEADER Message-ID search, which matches any part of the ID, only the same ID matches, stored with or
    without angle brackets.
    */
    QList<Location> messagesWithId(const QString &messageId) const;

    /** @short Messages matching the IMAP SEARCH keys @arg conditions in @arg mailboxes, or in all folders if empty */
    Result search(const QStringList &conditions, const QStringList &mailboxes) const;

    /** @short The newest messages whose subject or sender contains every one of @arg words, at most @arg limit

    Only the subject and the sender are searched, never the text of the messages. Each word matches any part of the
    subject or the sender regardless of case, as SUBJECT and FROM do.
    */
    QList<Summary> quickMatches(const QStringList &words, int limit) const;

    /** @short The newest flagged messages and those with the $TODO keyword, at most @arg limit

    $TODO is the keyword which KMail sets on messages marked as action items. Deleted messages are left out.
    */
    QList<Summary> followUps(int limit) const;

    /** @short Translate IMAP SEARCH keys to an SQL condition on the messages, binding @arg values in order

    Returns false and sets @arg problem for keys the index cannot answer. @arg usesText tells whether a BODY or TEXT
    key looks at the text of the messages.
    */
    static bool translate(const QStringList &conditions, QString *sql, QVariantList *values, QString *problem,
                          bool *usesText = nullptr);

    /** @short Readable text of an HTML part, for the index */
    static QString plainTextOfHtml(const QString &html);

private:
    QSqlDatabase database() const;
    /** @short The newest messages matching the SQL @arg condition, with @arg values bound in order */
    QList<Summary> summaries(const QString &condition, const QVariantList &values, int limit, bool details) const;
    bool fail(const QString &what, const QString &detail) const;

    QString m_connection;
    mutable QString m_error;
    bool m_open = false;
};

}
}

#endif
