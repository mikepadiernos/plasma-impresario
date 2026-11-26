/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "SearchResultsModel.h"

#include <algorithm>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MsgListModel.h"

namespace Imap
{

namespace Mailbox
{

SearchResultsModel::SearchResultsModel(QObject *parent)
    : QAbstractProxyModel(parent)
{
}

void SearchResultsModel::setSourceModel(QAbstractItemModel *model)
{
    beginResetModel();
    for (const QMetaObject::Connection &connection : std::as_const(m_connections))
        disconnect(connection);
    m_connections.clear();
    m_rows.clear();
    QAbstractProxyModel::setSourceModel(model);
    if (model) {
        m_connections << connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this, &SearchResultsModel::removeRowsOf);
        m_connections << connect(model, &QAbstractItemModel::modelAboutToBeReset, this, [this]() {
            beginResetModel();
            m_rows.clear();
        });
        m_connections << connect(model, &QAbstractItemModel::modelReset, this, [this]() { endResetModel(); });
        // Moved rows keep their persistent indexes; rows which a new layout dropped have to go
        m_connections << connect(model, &QAbstractItemModel::layoutChanged, this, &SearchResultsModel::removeInvalidRows);
        m_connections << connect(model, &QAbstractItemModel::rowsMoved, this, &SearchResultsModel::removeInvalidRows);
        m_connections << connect(model, &QAbstractItemModel::dataChanged, this,
                                 [this](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles) {
            const QModelIndex parent = topLeft.parent();
            for (int row = 0; row < m_rows.size(); ++row) {
                const QPersistentModelIndex &message = m_rows.at(row);
                if (message.row() >= topLeft.row() && message.row() <= bottomRight.row() && message.parent() == parent)
                    emit dataChanged(index(row, 0), index(row, MsgListModel::COLUMN_COUNT - 1), roles);
            }
        });
        m_connections << connect(model, &QObject::destroyed, this, [this]() {
            beginResetModel();
            m_rows.clear();
            m_connections.clear();
            endResetModel();
        });
    }
    endResetModel();
}

void SearchResultsModel::addMessages(const QModelIndexList &messages)
{
    QList<QPersistentModelIndex> added;
    for (const QModelIndex &message : messages) {
        if (!message.isValid() || message.model() != sourceModel())
            continue;
        const QPersistentModelIndex row(message.siblingAtColumn(0));
        if (!m_rows.contains(row) && !added.contains(row))
            added << row;
    }
    if (added.isEmpty())
        return;
    beginInsertRows(QModelIndex(), int(m_rows.size()), int(m_rows.size() + added.size() - 1));
    m_rows << added;
    endInsertRows();
}

void SearchResultsModel::clear()
{
    if (m_rows.isEmpty())
        return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

QModelIndexList SearchResultsModel::messages() const
{
    QModelIndexList list;
    list.reserve(m_rows.size());
    for (const QPersistentModelIndex &message : m_rows)
        list << message;
    return list;
}

void SearchResultsModel::removeRowsOf(const QModelIndex &parent, int first, int last)
{
    QList<int> rows;
    for (int row = 0; row < m_rows.size(); ++row) {
        const QPersistentModelIndex &message = m_rows.at(row);
        // Also the messages below a removed folder or message list
        QModelIndex ancestor = message;
        while (ancestor.isValid()) {
            if (ancestor.parent() == parent && ancestor.row() >= first && ancestor.row() <= last) {
                rows << row;
                break;
            }
            ancestor = ancestor.parent();
        }
    }
    removeListedRows(rows);
}

void SearchResultsModel::removeInvalidRows()
{
    QList<int> rows;
    for (int row = 0; row < m_rows.size(); ++row) {
        if (!m_rows.at(row).isValid())
            rows << row;
    }
    removeListedRows(rows);
}

void SearchResultsModel::removeListedRows(const QList<int> &rows)
{
    // From the last one, so that the rows still to remove keep their numbers; neighbours go together
    for (qsizetype i = rows.size() - 1; i >= 0;) {
        const int last = rows.at(i);
        int first = last;
        while (i > 0 && rows.at(i - 1) == first - 1)
            first = rows.at(--i);
        --i;
        beginRemoveRows(QModelIndex(), first, last);
        m_rows.remove(first, last - first + 1);
        endRemoveRows();
    }
}

QModelIndex SearchResultsModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() || row < 0 || row >= m_rows.size() || column < 0 || column >= MsgListModel::COLUMN_COUNT)
        return QModelIndex();
    return createIndex(row, column);
}

QModelIndex SearchResultsModel::parent(const QModelIndex &) const
{
    return QModelIndex();
}

int SearchResultsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

int SearchResultsModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : MsgListModel::COLUMN_COUNT;
}

bool SearchResultsModel::hasChildren(const QModelIndex &parent) const
{
    return !parent.isValid() && !m_rows.isEmpty();
}

QModelIndex SearchResultsModel::sibling(int row, int column, const QModelIndex &idx) const
{
    return idx.isValid() ? index(row, column) : QModelIndex();
}

QModelIndex SearchResultsModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid() || proxyIndex.model() != this || proxyIndex.row() >= m_rows.size())
        return QModelIndex();
    return m_rows.at(proxyIndex.row());
}

QModelIndex SearchResultsModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    if (!sourceIndex.isValid() || sourceIndex.model() != sourceModel())
        return QModelIndex();
    const QModelIndex message = sourceIndex.siblingAtColumn(0);
    for (int row = 0; row < m_rows.size(); ++row) {
        if (m_rows.at(row) == message)
            return index(row, 0);
    }
    return QModelIndex();
}

QVariant SearchResultsModel::data(const QModelIndex &proxyIndex, int role) const
{
    const QModelIndex message = mapToSource(proxyIndex);
    if (!message.isValid())
        return QVariant();
    if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
        return message.data(role);
    // Each column shows what MsgListModel shows in it
    switch (proxyIndex.column()) {
    case MsgListModel::SUBJECT:
        return message.data(Qt::DisplayRole);
    case MsgListModel::FROM:
        return QStringLiteral("[from]");
    case MsgListModel::TO:
        return QStringLiteral("[to]");
    case MsgListModel::CC:
        return QStringLiteral("[cc]");
    case MsgListModel::BCC:
        return QStringLiteral("[bcc]");
    case MsgListModel::DATE:
        return message.data(RoleMessageDate);
    case MsgListModel::RECEIVED_DATE:
        return message.data(RoleMessageInternalDate);
    case MsgListModel::SIZE:
        return message.data(RoleMessageSize);
    default:
        return QVariant();
    }
}

QVariant SearchResultsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && (role == Qt::DecorationRole || role == Qt::DisplayRole))
        return MsgListModel::columnHeader(section, role);
    return QAbstractProxyModel::headerData(section, orientation, role);
}

Qt::ItemFlags SearchResultsModel::flags(const QModelIndex &index) const
{
    const QModelIndex message = mapToSource(index);
    if (!message.isValid() || !message.data(RoleMessageUid).toUInt())
        return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren;
}

}

}
