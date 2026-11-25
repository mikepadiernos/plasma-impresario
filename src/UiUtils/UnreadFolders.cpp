/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "UnreadFolders.h"

#include <QAbstractItemModel>
#include <QList>

#include "Imap/Model/ItemRoles.h"

namespace UiUtils {

namespace {

void collectListed(const QAbstractItemModel *model, const QModelIndex &parent, QList<QModelIndex> &out)
{
    for (int row = 0; row < model->rowCount(parent); ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        out.append(index);
        // Do not descend into branches that have not been listed yet; that would send LIST commands
        const QVariant fetched = index.data(Imap::Mailbox::RoleIsFetched);
        if ((!fetched.isValid() || fetched.toBool()) && model->hasChildren(index))
            collectListed(model, index, out);
    }
}

bool hasUnread(const QModelIndex &index)
{
    if (!index.data(Imap::Mailbox::RoleMailboxIsSelectable).toBool())
        return false;
    const QVariant unread = index.data(Imap::Mailbox::RoleUnreadMessageCount);
    return unread.isValid() && unread.toInt() > 0;
}

}

QModelIndex nextUnreadMailbox(const QAbstractItemModel *model, const QString &current)
{
    if (!model)
        return QModelIndex();
    QList<QModelIndex> folders;
    collectListed(model, QModelIndex(), folders);
    int start = 0;
    int candidates = folders.size();
    if (!current.isEmpty()) {
        for (int i = 0; i < folders.size(); ++i) {
            if (folders.at(i).data(Imap::Mailbox::RoleMailboxName).toString() == current) {
                start = i + 1;
                --candidates;
                break;
            }
        }
    }
    for (int step = 0; step < candidates; ++step) {
        const QModelIndex &index = folders.at((start + step) % folders.size());
        if (hasUnread(index))
            return index;
    }
    return QModelIndex();
}

}
