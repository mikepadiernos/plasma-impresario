/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "MessageSelectionRestorer.h"

#include "Imap/Model/ItemRoles.h"
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
    m_mailbox = mailbox;
    m_uidValidity = uidValidity;
    m_uid = uid;
    m_pending = !mailbox.isEmpty() && uidValidity && uid;
    scheduleCheck();
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
    const auto matches = m_model->match(m_model->index(0, 0), Imap::Mailbox::RoleMessageUid, m_uid, -1,
                                        Qt::MatchExactly | Qt::MatchRecursive);
    for (const QModelIndex &index : matches) {
        if (index.data(Imap::Mailbox::RoleMailboxName).toString() != m_mailbox)
            continue;
        const uint validity = index.data(Imap::Mailbox::RoleMailboxUidValidity).toUInt();
        if (!validity)
            continue;
        m_pending = false;
        if (validity == m_uidValidity) {
            m_view->setCurrentIndex(index);
            m_view->scrollTo(index);
            emit messageRestored(index);
        }
        return;
    }
}

}
