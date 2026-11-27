/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "MessageSelectionRestorer.h"

#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/Utils.h"
#include <QMouseEvent>
#include <QTimer>
#include <QTreeView>

namespace Gui {

MessageSelectionRestorer::MessageSelectionRestorer(QTreeView *view, QObject *parent) :
    QObject(parent), m_view(view), m_model(view->model())
{
    Q_ASSERT(m_model);
    view->installEventFilter(this);
    view->viewport()->installEventFilter(this);
    connect(m_model, &QAbstractItemModel::rowsInserted, this, &MessageSelectionRestorer::scheduleCheck);
    connect(m_model, &QAbstractItemModel::dataChanged, this, &MessageSelectionRestorer::scheduleCheck);
    connect(m_model, &QAbstractItemModel::layoutChanged, this, &MessageSelectionRestorer::scheduleCheck);
    connect(m_model, &QAbstractItemModel::modelReset, this, &MessageSelectionRestorer::scheduleCheck);
}

void MessageSelectionRestorer::restore(const QString &mailbox, uint uidValidity, uint uid)
{
    restorePosition(mailbox, uidValidity, uid, 0);
    m_reportMissing = false;
}

void MessageSelectionRestorer::restorePosition(const QString &mailbox, uint uidValidity, uint uid, uint anchorUid,
                                               const QList<uint> &selectedUids)
{
    m_mailbox = mailbox;
    m_uidValidity = uidValidity;
    m_uid = uid;
    m_anchorUid = anchorUid;
    m_selectedUids = selectedUids;
    m_reportMissing = true;
    m_pending = !mailbox.isEmpty() && uidValidity && (uid || anchorUid);
    scheduleCheck();
}

bool MessageSelectionRestorer::isPending() const
{
    return m_pending;
}

void MessageSelectionRestorer::cancel()
{
    m_pending = false;
}

bool MessageSelectionRestorer::eventFilter(QObject *watched, QEvent *event)
{
    if (m_view && ((watched == m_view && event->type() == QEvent::KeyPress) ||
                   (watched == m_view->viewport() && event->type() == QEvent::MouseButtonPress &&
                    m_view->indexAt(static_cast<QMouseEvent *>(event)->position().toPoint()).isValid()))) {
        m_pending = false;
    }
    return QObject::eventFilter(watched, event);
}

void MessageSelectionRestorer::scheduleCheck()
{
    if (!m_pending || m_checkScheduled)
        return;
    m_checkScheduled = true;
    QTimer::singleShot(0, this, &MessageSelectionRestorer::check);
}

void MessageSelectionRestorer::check()
{
    m_checkScheduled = false;
    if (!m_pending || !m_model || !m_view || !m_model->rowCount())
        return;
    // The view may show a proxy on top of the model, such as the message list's groups
    QAbstractItemModel *viewModel = m_view->model();
    if (!viewModel || !viewModel->rowCount())
        return;
    // The first row may be the header of a group, so look for any message of the mailbox
    const QModelIndex first = viewModel->index(0, 0);
    auto findMessage = [viewModel, &first, this](uint uid) -> QModelIndex {
        if (!uid)
            return {};
        const auto matches = viewModel->match(first, Imap::Mailbox::RoleMessageUid, uid, -1,
                                              Qt::MatchExactly | Qt::MatchRecursive);
        for (const QModelIndex &index : matches) {
            if (index.data(Imap::Mailbox::RoleMailboxName).toString() == m_mailbox)
                return index;
        }
        return {};
    };
    QModelIndex current = findMessage(m_uid);
    QModelIndex anchor = findMessage(m_anchorUid);
    QModelIndex known = current.isValid() ? current : anchor;
    if (!known.isValid()) {
        const auto messages = viewModel->match(first, Imap::Mailbox::RoleMailboxName, m_mailbox, 1,
                                               Qt::MatchExactly | Qt::MatchRecursive);
        if (messages.isEmpty())
            return;
        known = messages.constFirst();
    }
    const uint validity = known.data(Imap::Mailbox::RoleMailboxUidValidity).toUInt();
    if (!validity) {
        // The folder is being resynced, which may end without changing any row, so wait for the sync itself
        const QModelIndex message = Imap::deproxifiedIndex(known);
        if (const auto *model = qobject_cast<const Imap::Mailbox::Model *>(message.model())) {
            connect(model, &Imap::Mailbox::Model::mailboxSyncingProgress, this, &MessageSelectionRestorer::scheduleCheck,
                    Qt::UniqueConnection);
        }
        return;
    }
    if (validity != m_uidValidity) {
        m_pending = false;
        emit restoreFailed();
        return;
    }
    if (!current.isValid() && m_uid) {
        // Messages whose UID is still being synchronized might turn out to be the saved one
        if (!m_reportMissing || !viewModel->match(first, Imap::Mailbox::RoleMessageUid, 0u, 1,
                                                  Qt::MatchExactly | Qt::MatchRecursive).isEmpty())
            return;
    }
    m_pending = false;
    if (anchor.isValid())
        m_view->scrollTo(anchor, QAbstractItemView::PositionAtTop);
    if (current.isValid()) {
        m_view->setCurrentIndex(current);
        if (QItemSelectionModel *selection = m_view->selectionModel(); selection && !m_selectedUids.isEmpty()) {
            QItemSelection selected;
            const int lastColumn = viewModel->columnCount(current.parent()) - 1;
            for (const uint uid : std::as_const(m_selectedUids)) {
                const QModelIndex index = uid == m_uid ? current : findMessage(uid);
                if (index.isValid())
                    selected.select(index.siblingAtColumn(0), index.siblingAtColumn(qMax(0, lastColumn)));
            }
            selected.select(current.siblingAtColumn(0), current.siblingAtColumn(qMax(0, lastColumn)));
            selection->select(selected, QItemSelectionModel::ClearAndSelect);
        }
        m_view->scrollTo(current);
        emit messageRestored(current);
    } else if (m_uid) {
        emit restoreFailed();
    }
}

}
