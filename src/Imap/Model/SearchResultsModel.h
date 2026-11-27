/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef IMAP_MODEL_SEARCHRESULTSMODEL_H
#define IMAP_MODEL_SEARCHRESULTSMODEL_H

#include <QAbstractProxyModel>
#include <QList>
#include <QPersistentModelIndex>

namespace Imap
{

namespace Mailbox
{

class SearchSnippets;

/** @short A flat list of chosen messages of one account, from any of its folders

The rows point at message items of the source model, normally the account's Model, in the order in which they were
added; each message is listed once. The columns and the display data are those of MsgListModel, so the list can be a
source of UnifiedMsgListModel, and mapToSource() leads to the message itself, so that Imap::deproxifiedIndex() and the
actions on messages work.

A message which the source removes, as after an expunge, leaves the list; resetting or destroying the source empties
it. Data changes of listed messages are forwarded.

The RoleMessageSearchSnippet of each message comes from snippets(), which has no terms and no text until it is told.
*/
class SearchResultsModel : public QAbstractProxyModel
{
    Q_OBJECT
public:
    explicit SearchResultsModel(QObject *parent = nullptr);

    void setSourceModel(QAbstractItemModel *sourceModel) override;

    /** @short Append those of @p messages which are not listed yet; they must belong to the source model */
    void addMessages(const QModelIndexList &messages);
    void clear();
    /** @short The listed messages in the source model */
    QModelIndexList messages() const;
    /** @short The snippets of the listed messages for the terms of the search */
    SearchSnippets *snippets() const;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex sibling(int row, int column, const QModelIndex &idx) const override;
    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;
    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override;
    QVariant data(const QModelIndex &proxyIndex, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    void removeRowsOf(const QModelIndex &parent, int first, int last);
    void removeInvalidRows();
    void removeListedRows(const QList<int> &rows);

    QList<QPersistentModelIndex> m_rows;
    QList<QMetaObject::Connection> m_connections;
    SearchSnippets *m_snippets;
};

}

}

#endif
