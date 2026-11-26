/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef IMAP_MODEL_SEARCHINDEXER_H
#define IMAP_MODEL_SEARCHINDEXER_H

#include <functional>
#include <memory>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include "Imap/Model/AttachmentTextExtractor.h"
#include "Imap/Model/SearchIndex.h"

namespace Imap
{
namespace Mailbox
{

class AbstractCache;

/** @short Keeps an account's SearchIndex up to date with its offline cache

The indexer reads the folders, headers, flags and downloaded text parts which the cache already holds, a few messages
at a time so that the window stays responsive, and never asks the server for anything. Messages whose text was not
downloaded are indexed by their headers and looked at again in later passes, once their text may have arrived.

The cache is asked for through a function, because the account replaces its cache when the cache fails. The index
works only with a persistent SQLite cache, see CombinedCache.
*/
class SearchIndexer : public QObject
{
    Q_OBJECT
public:
    using CacheSource = std::function<std::shared_ptr<AbstractCache>()>;

    /** @short How far the index covers one folder of the offline cache */
    struct FolderCoverage {
        enum class State {
            /** The folder was never synchronized with the cache on, so its messages are not on this computer */
            NotCached,
            /** The folder is in the cache but not in the index yet */
            NotIndexed,
            /** The cached folder changed since it was indexed */
            OutOfDate,
            /** Some cached messages have no headers in the cache yet, so the index cannot hold them */
            Partial,
            Indexed,
            /** The user keeps the folder out of the index */
            Excluded,
        };
        QString mailbox;
        State state = State::NotCached;
        /** Messages of the folder in the cache */
        int cached = 0;
        /** Messages indexed and those of them with their text, as of the last update */
        int indexed = 0;
        int withText = 0;
        /** When the folder was last indexed; invalid when never */
        QDateTime updated;
    };

    /** @short Where the running pass is */
    struct Progress {
        /** The folder being indexed, or empty */
        QString mailbox;
        /** Number of the folder in the pass, from one, and the folders of the pass */
        int folder = 0;
        int folders = 0;
        /** Messages of the folder done, and all of them */
        int position = 0;
        int messages = 0;
    };

    explicit SearchIndexer(const CacheSource &cache, QObject *parent = nullptr);
    ~SearchIndexer() override;

    /** @short Open the index in the cache's database; false when the index cannot be used, see unavailableReason() */
    bool open();
    bool isAvailable() const;
    QString unavailableReason() const;
    SearchIndex *index() const;

    /** @short Index what the cache holds now; when a pass is under way, another one follows it */
    void update();
    /** @short Update after a pause, so that a burst of changes in the cache leads to one pass */
    void scheduleUpdate();
    bool isRunning() const;
    Progress progress() const;

    /** @short Stop indexing until resumed; changes to the cache in the meantime are indexed on resume */
    void setPaused(bool paused);
    bool isPaused() const;
    /** @short Whether a pass is waiting: scheduled, asked for while paused, or due after the running one */
    bool hasPendingChanges() const;
    /** @short Empty the index and build it again from the cache; while paused, it is built on resume */
    bool rebuild();

    /** @short The selectable folders of the cache, in the order of the folder tree, and how far each is indexed

    This is what the index is meant to cover; folders which only the server knows of are not listed. Empty when the
    index is not available.
    */
    QList<FolderCoverage> coverage() const;

    /** @short Keep @arg mailboxes out of the index; what the index holds of them is removed right away */
    void setExcludedFolders(const QStringList &mailboxes);
    /** @short The folders kept out of the index, sorted */
    QStringList excludedFolders() const;

    /** @short Whether addDecryptedText() adds the decrypted text of encrypted messages; off by default

    Turning it off removes the decrypted text which the index holds; the messages are then indexed again without it.
    */
    void setIndexDecrypted(bool enabled);
    bool indexesDecrypted() const;
    /** @short Add @arg text, decrypted from the message @arg uid of @arg mailbox, to the index

    Does nothing unless decrypted text is to be indexed and the folder is not excluded. A message which is not in the
    index yet is added from the cache first.
    */
    bool addDecryptedText(const QString &mailbox, uint uid, const QString &text);

    /** @short Whether the text of downloaded attachments up to @arg limit bytes is indexed; off by default

    Attachments are never downloaded for the index: one is read once the user has downloaded it, such as by opening
    or saving it. Turning this off, or changing the limit, indexes the messages whose attachments were read again.
    */
    void setAttachmentIndexing(bool enabled, quint64 limit);
    bool indexesAttachments() const;
    quint64 attachmentLimit() const;
    /** @short Read attachments with @arg extractor instead of the default one; for tests */
    void setAttachmentExtractor(const std::shared_ptr<AttachmentTextExtractor> &extractor);
    /** @short The extractor which reads attachments, created when first needed */
    const AttachmentTextExtractor *attachmentExtractor() const;

    /** @short Delete the index from the cache, leaving the indexer unavailable; @arg problem says what failed */
    bool deleteIndex(QString *problem = nullptr);

    /** @short The indexer of the IMAP model @arg model, or nullptr when the account has none */
    static SearchIndexer *forModel(const QObject *model);

    /** @short Pause before a scheduled pass, pause between batches and messages per batch; for tests */
    void setTimings(int scheduleMs, int batchMs, int batchSize);

    /** @short The downloaded text parts of a message in the cache, without its attachments

    Empty when its headers or text are not in the cache. Nothing is fetched.
    */
    static QString downloadedText(const AbstractCache &cache, const QString &mailbox, uint uid);
    /** @short What the index keeps of a message in the cache; false when its headers are not in the cache */
    bool document(const AbstractCache &cache, const QString &mailbox, uint uid, const QStringList &flags,
                  SearchIndex::Document *document) const;

signals:
    void progressChanged();
    void finished();

private:
    void step();
    void beginPass(const AbstractCache &cache);
    /** @short Selectable folders of @arg cache in tree order; @arg listed gets every folder, selectable or not */
    static QStringList cachedFolders(const AbstractCache &cache, QSet<QString> *listed = nullptr);
    /** @short Forget the pass under way without finishing it */
    void abandonPass();
    bool beginFolder(const AbstractCache &cache, const QString &mailbox);
    void indexBatch(const AbstractCache &cache);
    void endFolder();
    void endPass();
    std::shared_ptr<AbstractCache> usableCache() const;

    CacheSource m_cache;
    std::unique_ptr<SearchIndex> m_index;
    QString m_connection;
    QString m_unavailable;
    bool m_running = false;
    bool m_again = false;
    bool m_paused = false;
    /** Changes which arrived while paused */
    bool m_pending = false;
    int m_passFolders = 0;
    QTimer m_scheduled;
    QTimer m_batch;
    int m_batchSize = 200;
    QSet<QString> m_excluded;
    bool m_indexDecrypted = false;
    bool m_indexAttachments = false;
    quint64 m_attachmentLimit = 10 * 1024 * 1024;
    mutable std::shared_ptr<AttachmentTextExtractor> m_extractor;

    QStringList m_queue;
    QString m_mailbox;
    SearchIndex::Folder m_folder;
    Imap::Uids m_uids;
    int m_position = 0;
    QHash<uint, SearchIndex::Entry> m_entries;
};

}
}

#endif
