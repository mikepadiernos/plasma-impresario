/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef IMAP_MODEL_SEARCHINDEXER_H
#define IMAP_MODEL_SEARCHINDEXER_H

#include <functional>
#include <memory>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QTimer>

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

    /** @short Pause before a scheduled pass, pause between batches and messages per batch; for tests */
    void setTimings(int scheduleMs, int batchMs, int batchSize);

    /** @short What the index keeps of a message in the cache; false when its headers are not in the cache */
    static bool document(const AbstractCache &cache, const QString &mailbox, uint uid, const QStringList &flags,
                         SearchIndex::Document *document);

signals:
    void progressChanged();
    void finished();

private:
    void step();
    void beginPass(const AbstractCache &cache);
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
    QTimer m_scheduled;
    QTimer m_batch;
    int m_batchSize = 200;

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
