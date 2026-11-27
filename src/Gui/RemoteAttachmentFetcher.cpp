/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/RemoteAttachmentFetcher.h"

#include <KIO/Global>
#include <KIO/TransferJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <QFileInfo>
#include <QLocale>
#include <QProgressBar>

#include "Composer/AttachmentRequests.h"
#include "Composer/MessageComposer.h"
#include "Gui/InlineNotificationBar.h"

namespace Gui {

namespace {

/** The file name of @arg source, for messages */
QString shownName(const QUrl &source)
{
    const QString name = source.adjusted(QUrl::StripTrailingSlash).fileName();
    return name.isEmpty() ? Composer::AttachmentRequests::displayUrl(source) : name;
}

/** Where @arg source is, for messages: its host, or the whole URL when it names none */
QString shownPlace(const QUrl &source)
{
    return source.host().isEmpty() ? Composer::AttachmentRequests::displayUrl(source) : source.host();
}

}

RemoteAttachmentFetcher::RemoteAttachmentFetcher(Composer::MessageComposer *composer, InlineNotificationBar *notice,
                                                 QWidget *window)
    : QObject(window)
    , m_composer(composer)
    , m_notice(notice)
    , m_window(window)
{
    connect(composer, &Composer::MessageComposer::remoteFileAttachmentAdded, this, &RemoteAttachmentFetcher::enqueue);
    connect(composer, &Composer::MessageComposer::remoteFileAttachmentRemoved, this, &RemoteAttachmentFetcher::forget);
}

RemoteAttachmentFetcher::~RemoteAttachmentFetcher()
{
    stopCurrent();
}

int RemoteAttachmentFetcher::pendingCount() const
{
    return m_queue.size() + (m_job ? 1 : 0);
}

void RemoteAttachmentFetcher::reportRefused(const QStringList &lines)
{
    m_refused << lines;
    if (!m_job)
        showSummary();
}

void RemoteAttachmentFetcher::enqueue(const QUrl &source, const QString &localCopy)
{
    m_queue << Pending{source, localCopy};
    ++m_batchSize;
    if (!m_job)
        startNext();
}

void RemoteAttachmentFetcher::forget(const QString &localCopy)
{
    // The user removed the attachment from the list
    if (m_job && m_current.localCopy == localCopy) {
        stopCurrent();
        ++m_batchDone;
        startNext();
        return;
    }
    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue[i].localCopy == localCopy) {
            m_queue.removeAt(i);
            --m_batchSize;
            break;
        }
    }
}

void RemoteAttachmentFetcher::startNext()
{
    if (!m_composer)
        return;
    if (m_queue.isEmpty()) {
        showSummary();
        return;
    }
    m_current = m_queue.takeFirst();
    m_writeError.clear();
    m_file.setFileName(m_current.localCopy);
    // The composer created the copy, readable only by the user; it is filled in place so that it stays private
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::ExistingOnly)) {
        m_failed << i18nc("@item %1 is a file on another computer, %2 why it was not fetched", "%1: %2",
                          Composer::AttachmentRequests::displayUrl(m_current.source), m_file.errorString());
        ++m_batchDone;
        m_composer->removeRemoteFileAttachment(m_current.localCopy);
        startNext();
        return;
    }

    m_job = KIO::get(m_current.source, KIO::NoReload, KIO::HideProgressInfo);
    if (m_window)
        KJobWidgets::setWindow(m_job, m_window->window());
    connect(m_job, &KIO::TransferJob::data, this, &RemoteAttachmentFetcher::slotData);
    connect(m_job, &KJob::result, this, &RemoteAttachmentFetcher::slotResult);
    connect(m_job, &KJob::processedAmountChanged, this, [this](KJob *job) { slotProgress(job); });
    connect(m_job, &KJob::totalAmountChanged, this, [this](KJob *job) { slotProgress(job); });

    if (m_notice) {
        const QString text = m_batchSize > 1
                ? i18nc("@info:progress %1 is a file name, %2 a computer, %3 and %4 count files",
                        "Fetching %1 from %2 (%3 of %4)…", shownName(m_current.source), shownPlace(m_current.source),
                        m_batchDone + 1, m_batchSize)
                : i18nc("@info:progress %1 is a file name, %2 a computer", "Fetching %1 from %2…",
                        shownName(m_current.source), shownPlace(m_current.source));
        m_notice->showMessage(InlineNotificationBar::Kind::Busy, text,
                              {{i18nc("@action:button", "Cancel"), QStringLiteral("dialog-cancel"),
                                QStringLiteral("cancelRemoteAttachments"), [this]() { cancel(); }}});
        m_notice->progressBar()->setRange(0, 0);
    }
}

void RemoteAttachmentFetcher::slotData(KIO::Job *job, const QByteArray &data)
{
    if (job != m_job || data.isEmpty() || !m_writeError.isEmpty())
        return;
    if (m_file.write(data) != data.size()) {
        m_writeError = m_file.errorString();
        m_job->kill(KJob::EmitResult);
    }
}

void RemoteAttachmentFetcher::slotProgress(KJob *job)
{
    if (job != m_job || !m_notice || !m_notice->isBusy())
        return;
    const qulonglong total = job->totalAmount(KJob::Bytes);
    if (total > 0) {
        m_notice->progressBar()->setRange(0, 1000);
        m_notice->progressBar()->setValue(int(std::min<qulonglong>(1000, job->processedAmount(KJob::Bytes) * 1000 / total)));
    }
}

void RemoteAttachmentFetcher::slotResult(KJob *job)
{
    if (job != m_job || !m_composer)
        return;
    m_job = nullptr;
    const qint64 size = m_file.size();
    m_file.close();
    ++m_batchDone;
    const QString source = Composer::AttachmentRequests::displayUrl(m_current.source);
    if (job->error() || !m_writeError.isEmpty()) {
        QString reason = m_writeError.isEmpty() ? job->errorString() : m_writeError;
        // KIO's messages name the URL with its user name, which may be a credential of its own
        if (!m_current.source.userName().isEmpty())
            reason.replace(m_current.source.userName() + QLatin1Char('@'), QString());
        m_failed << i18nc("@item %1 is a file on another computer, %2 why it was not fetched", "%1: %2", source, reason);
        m_composer->removeRemoteFileAttachment(m_current.localCopy);
    } else if (m_composer->setRemoteFileFetched(m_current.localCopy)) {
        m_fetched << i18nc("@item %1 is a file name, %2 its size, %3 a computer", "%1 (%2) from %3",
                           shownName(m_current.source), KIO::convertSize(size), shownPlace(m_current.source));
    }
    m_current = {};
    startNext();
}

void RemoteAttachmentFetcher::stopCurrent()
{
    if (m_job) {
        auto *job = m_job.data();
        m_job = nullptr;
        job->kill(KJob::Quietly);
    }
    if (m_file.isOpen())
        m_file.close();
}

void RemoteAttachmentFetcher::cancel()
{
    QList<Pending> stopped = m_queue;
    m_queue.clear();
    if (m_job) {
        stopped.prepend(m_current);
        stopCurrent();
    }
    m_current = {};
    for (const Pending &pending : std::as_const(stopped)) {
        m_cancelled << Composer::AttachmentRequests::displayUrl(pending.source);
        if (m_composer)
            m_composer->removeRemoteFileAttachment(pending.localCopy);
    }
    showSummary();
}

void RemoteAttachmentFetcher::showSummary()
{
    QStringList lines;
    if (!m_refused.isEmpty()) {
        lines << i18ncp("@info", "A file could not be attached:", "%1 files could not be attached:", m_refused.size());
        lines << m_refused;
    }
    if (!m_failed.isEmpty()) {
        lines << i18ncp("@info", "A file could not be fetched and is not attached:",
                        "%1 files could not be fetched and are not attached:", m_failed.size());
        lines << m_failed;
    }
    if (!m_cancelled.isEmpty()) {
        lines << i18ncp("@info", "Fetching was cancelled, so this file is not attached:",
                        "Fetching was cancelled, so these %1 files are not attached:", m_cancelled.size());
        lines << m_cancelled;
    }
    if (!m_fetched.isEmpty()) {
        lines << i18ncp("@info %2 is a list of files with their sizes", "Fetched %2; check it before sending.",
                        "Fetched %1 files: %2. Check them before sending.", m_fetched.size(),
                        QLocale().createSeparatedList(m_fetched));
    }
    if (m_notice) {
        if (lines.isEmpty())
            m_notice->dismiss();
        else if (m_failed.isEmpty() && m_refused.isEmpty())
            m_notice->showInfo(lines.join(QLatin1Char('\n')));
        else
            m_notice->showError(lines.join(QLatin1Char('\n')));
    }
    m_fetched.clear();
    m_failed.clear();
    m_refused.clear();
    m_cancelled.clear();
    m_batchSize = 0;
    m_batchDone = 0;
}

}
