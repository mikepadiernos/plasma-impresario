/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QFile>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <memory>

class KJob;
class QWidget;

namespace KIO {
class Job;
class TransferJob;
}

namespace Composer {
class MessageComposer;
}

namespace Gui {

class InlineNotificationBar;

/** @short Fetch the files on other computers attached to a message being composed, one after another, through KIO

Each file is read from its source and written into the private copy which the composer created for it; the source is
never changed. @arg notice shows which file is being fetched and from where, its progress and a Cancel button, and
afterwards what was fetched, with sizes, and what could not be, with the reason. Fetching that fails or is cancelled
removes the attachment, so that a message is never sent without a file it seemed to have.
*/
class RemoteAttachmentFetcher : public QObject
{
    Q_OBJECT
public:
    RemoteAttachmentFetcher(Composer::MessageComposer *composer, InlineNotificationBar *notice, QWidget *window);
    ~RemoteAttachmentFetcher() override;

    /** @short Files waiting or being fetched */
    int pendingCount() const;

    /** @short Report the files which could not be attached, each as "file: reason", along with what is being fetched */
    void reportRefused(const QStringList &lines);

public slots:
    /** @short Stop fetching and remove the attachments not yet fetched */
    void cancel();

private:
    struct Pending {
        QUrl source;
        QString localCopy;
    };

    void enqueue(const QUrl &source, const QString &localCopy);
    void forget(const QString &localCopy);
    void startNext();
    void slotData(KIO::Job *job, const QByteArray &data);
    void slotResult(KJob *job);
    void slotProgress(KJob *job);
    void stopCurrent();
    void showSummary();

    QPointer<Composer::MessageComposer> m_composer;
    QPointer<InlineNotificationBar> m_notice;
    QPointer<QWidget> m_window;
    QList<Pending> m_queue;
    Pending m_current;
    QPointer<KIO::TransferJob> m_job;
    QFile m_file;
    QString m_writeError;
    /** Files of the current batch, for "2 of 3" */
    int m_batchSize = 0;
    int m_batchDone = 0;
    QStringList m_fetched;
    QStringList m_failed;
    QStringList m_refused;
    QStringList m_cancelled;
};

}
