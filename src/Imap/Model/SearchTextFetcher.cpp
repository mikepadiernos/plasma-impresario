/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "SearchTextFetcher.h"

#include <algorithm>

#include "Imap/Model/FindInterestingPart.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"

namespace Imap
{
namespace Mailbox
{

SearchTextFetcher::SearchTextFetcher(Model *model, const QString &mailbox, const Imap::Uids &uids, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_mailbox(mailbox)
    , m_uids(uids)
{
    m_poll.setInterval(200);
    connect(&m_poll, &QTimer::timeout, this, &SearchTextFetcher::poll);
    m_deadline.setSingleShot(true);
    m_deadline.setInterval(60 * 1000);
    connect(&m_deadline, &QTimer::timeout, this, [this]() {
        if (m_phase == Phase::Opening) {
            m_failed = int(m_uids.size());
            finish();
            return;
        }
        // Whatever did not arrive in time stays without text; the rest of the folder goes on
        m_failed += int(m_pending.size());
        m_pending.clear();
        nextBatch();
    });
}

void SearchTextFetcher::setTimings(int batchSize, int batchMs, int pollMs)
{
    m_batchSize = std::max(1, batchSize);
    m_deadline.setInterval(batchMs);
    m_poll.setInterval(pollMs);
}

bool SearchTextFetcher::isRunning() const
{
    return m_phase != Phase::Idle;
}

QString SearchTextFetcher::mailbox() const
{
    return m_mailbox;
}

int SearchTextFetcher::total() const
{
    return int(m_uids.size());
}

int SearchTextFetcher::downloaded() const
{
    return m_downloaded;
}

int SearchTextFetcher::failed() const
{
    return m_failed;
}

void SearchTextFetcher::start()
{
    cancel();
    m_position = 0;
    m_downloaded = 0;
    m_failed = 0;
    const QModelIndex mailbox = m_model ? m_model->mailboxIndexByName(m_mailbox) : QModelIndex();
    const QModelIndex list = mailbox.isValid() ? m_model->index(0, 0, mailbox) : QModelIndex();
    if (!list.isValid() || m_uids.isEmpty()) {
        m_failed = int(m_uids.size());
        m_phase = Phase::Opening;
        QMetaObject::invokeMethod(this, &SearchTextFetcher::finish, Qt::QueuedConnection);
        return;
    }
    m_list = list;
    // Asks for the messages of the folder, from the cache and the server
    m_model->rowCount(list);
    m_phase = Phase::Opening;
    m_deadline.start();
    m_poll.start();
    emit progressChanged();
}

void SearchTextFetcher::cancel()
{
    m_poll.stop();
    m_deadline.stop();
    m_pending.clear();
    m_list = QPersistentModelIndex();
    m_phase = Phase::Idle;
}

void SearchTextFetcher::poll()
{
    if (!m_model) {
        m_failed = int(m_uids.size()) - m_downloaded;
        finish();
        return;
    }
    if (m_phase == Phase::Opening) {
        if (m_list.isValid() && !m_list.data(RoleIsFetched).toBool() && !m_list.data(RoleIsUnavailable).toBool())
            return;
        if (!m_list.isValid() || !m_list.data(RoleIsFetched).toBool()) {
            m_failed = int(m_uids.size());
            finish();
            return;
        }
        m_phase = Phase::Fetching;
        nextBatch();
        return;
    }
    if (m_phase != Phase::Fetching)
        return;
    for (int i = int(m_pending.size()) - 1; i >= 0; --i) {
        const QPersistentModelIndex message = m_pending.at(i);
        if (!message.isValid() || message.data(RoleIsUnavailable).toBool()) {
            ++m_failed;
            m_pending.removeAt(i);
            continue;
        }
        if (!message.data(RoleIsFetched).toBool())
            continue;
        QModelIndex part;
        QString partMessage;
        QString data;
        // Asks for the structure first and then for the main part, as the reader does
        switch (FindInterestingPart::findMainPartOfMessage(message, part, partMessage, &data)) {
        case FindInterestingPart::MAINPART_FOUND:
            ++m_downloaded;
            m_pending.removeAt(i);
            break;
        case FindInterestingPart::MAINPART_PART_CANNOT_DETERMINE:
            ++m_failed;
            m_pending.removeAt(i);
            break;
        case FindInterestingPart::MAINPART_PART_LOADING:
            if (part.isValid() && part.data(RoleIsUnavailable).toBool()) {
                ++m_failed;
                m_pending.removeAt(i);
            }
            break;
        case FindInterestingPart::MAINPART_MESSAGE_NOT_LOADED:
            break;
        }
    }
    emit progressChanged();
    if (m_pending.isEmpty())
        nextBatch();
}

void SearchTextFetcher::nextBatch()
{
    if (m_phase != Phase::Fetching)
        return;
    if (!m_model || !m_model->mailboxIndexByName(m_mailbox).isValid()) {
        m_failed = int(m_uids.size()) - m_downloaded;
        finish();
        return;
    }
    while (m_pending.isEmpty() && m_position < int(m_uids.size())) {
        const int end = std::min(int(m_uids.size()), m_position + m_batchSize);
        for (; m_position < end; ++m_position) {
            const QModelIndex message = m_model->messageIndexByUid(m_mailbox, m_uids.at(m_position));
            if (!message.isValid()) {
                // Deleted since the index saw it
                ++m_failed;
                continue;
            }
            // Asks for the envelope and the structure, which come from the cache when they are there
            message.data(RoleMessageSubject);
            m_pending << QPersistentModelIndex(message);
        }
    }
    if (m_pending.isEmpty()) {
        finish();
        return;
    }
    m_deadline.start();
    emit progressChanged();
}

void SearchTextFetcher::finish()
{
    if (m_phase == Phase::Idle)
        return;
    cancel();
    emit progressChanged();
    emit finished();
}

}
}
