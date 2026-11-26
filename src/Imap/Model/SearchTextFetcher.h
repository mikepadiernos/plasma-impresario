/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef IMAP_MODEL_SEARCHTEXTFETCHER_H
#define IMAP_MODEL_SEARCHTEXTFETCHER_H

#include <QList>
#include <QObject>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QTimer>

#include "Imap/Parser/Uids.h"

namespace Imap
{
namespace Mailbox
{

class Model;

/** @short Downloads the text of messages for the local search index, after the user asked for it

The index never downloads anything itself. When the user chooses to make the text of a folder searchable offline, this
asks the server for the main text part of each listed message, a batch at a time, the same way the reader does; the
offline cache keeps the parts and the indexer picks them up on its next pass.

Opening the folder changes which folder of the account is selected; whoever starts it selects the shown folder again
afterwards.
*/
class SearchTextFetcher : public QObject
{
    Q_OBJECT
public:
    SearchTextFetcher(Model *model, const QString &mailbox, const Imap::Uids &uids, QObject *parent = nullptr);

    void start();
    void cancel();
    bool isRunning() const;

    QString mailbox() const;
    int total() const;
    /** @short Messages whose text arrived */
    int downloaded() const;
    /** @short Messages whose text could not be downloaded in time, or which have no text to download */
    int failed() const;

    /** @short Messages asked for at once, and how long to wait for a batch, for opening the folder and between checks */
    void setTimings(int batchSize, int batchMs, int pollMs);

signals:
    void progressChanged();
    void finished();

private:
    enum class Phase {
        Idle,
        Opening,
        Fetching,
    };

    void poll();
    void nextBatch();
    void finish();

    QPointer<Model> m_model;
    QString m_mailbox;
    Imap::Uids m_uids;
    Phase m_phase = Phase::Idle;
    int m_position = 0;
    int m_downloaded = 0;
    int m_failed = 0;
    int m_batchSize = 50;
    QPersistentModelIndex m_list;
    QList<QPersistentModelIndex> m_pending;
    QTimer m_poll;
    QTimer m_deadline;
};

}
}

#endif
