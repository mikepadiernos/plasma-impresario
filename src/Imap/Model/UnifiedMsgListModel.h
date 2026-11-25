/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef IMAP_MODEL_UNIFIEDMSGLISTMODEL_H
#define IMAP_MODEL_UNIFIEDMSGLISTMODEL_H

#include <QAbstractProxyModel>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QStringList>
#include <utility>
#include <vector>

#include "Imap/Model/PrettyMsgListModel.h"

namespace Imap
{

namespace Mailbox
{

/** @short The messages of several accounts' flat message lists, one after another

Each participating account has one source, normally its MsgListModel showing its INBOX. The rows of the sources follow
each other in the order of the accounts, so mapToSource() returns the index in the account's own list and
Imap::deproxifiedIndex() reaches the message in that account's Model. Actions can therefore be sent to the account
which a message belongs to.

An account can take part without a source, for example while it is not configured; it then has no rows. The state
of each account is kept apart from its rows: the messages of an offline or failing account stay in the list.

A source list can be shown for one account only; using it again for another account is refused, so the same message
never appears twice. Destroying a source removes only that account's rows.

RoleMessageAccountId, RoleMessageAccountName and RoleMessageSourceLabel tell where a message comes from. Messages
cannot be dragged: dropped data names a folder and UIDs but not the account.
*/
class UnifiedMsgListModel : public QAbstractProxyModel
{
    Q_OBJECT
public:
    enum class AccountState {
        /** Connected, or working from its cache without known problems */
        Ready,
        /** Connecting or loading its messages */
        Syncing,
        /** Offline by choice or for lack of a network */
        Offline,
        /** A network or sign-in error; the details are in accountStateDetail() */
        Failed,
        /** Not configured or without messages to show */
        Unavailable,
    };
    Q_ENUM(AccountState)

    explicit UnifiedMsgListModel(QObject *parent = nullptr);
    ~UnifiedMsgListModel() override;

    /** @short Show @p messages as the messages of @p accountId, replacing its previous source

    Returns false, changing nothing, when another account already shows @p messages. A null @p messages lets the
    account take part without rows. New accounts are added after the existing ones.
    */
    bool setAccountSource(const QString &accountId, const QString &accountName, QAbstractItemModel *messages);
    void removeAccount(const QString &accountId);
    void setAccountName(const QString &accountId, const QString &accountName);
    QStringList accountIds() const;
    QString accountName(const QString &accountId) const;
    QAbstractItemModel *accountSource(const QString &accountId) const;
    /** @short The account of @p sourceModel, or an empty string */
    QString accountIdOfSource(const QAbstractItemModel *sourceModel) const;

    void setAccountState(const QString &accountId, AccountState state, const QString &detail = QString());
    AccountState accountState(const QString &accountId) const;
    QString accountStateDetail(const QString &accountId) const;

    /** @short Number of messages of @p accountId, or of all accounts for an empty ID */
    int messageCount(const QString &accountId = QString()) const;
    /** @short Number of unread messages of @p accountId, or of all accounts for an empty ID

    The folder's own count is used when the source is a MsgListModel which knows it; otherwise the messages whose
    flags are known to be unread are counted.
    */
    int unreadCount(const QString &accountId = QString()) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;
    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override;
    QVariant data(const QModelIndex &proxyIndex, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QModelIndex sibling(int row, int column, const QModelIndex &idx) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;
    QHash<int, QByteArray> roleNames() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    QStringList mimeTypes() const override;
    Qt::DropActions supportedDragActions() const override;

signals:
    /** @short Accounts were added, removed or renamed, or their state changed */
    void accountsChanged();

private:
    struct Source {
        QString accountId;
        QString accountName;
        QPointer<QAbstractItemModel> model;
        /** The model, for telling it apart while it is being destroyed */
        const QObject *raw = nullptr;
        int rows = 0;
        AccountState state = AccountState::Ready;
        QString detail;
        std::vector<QMetaObject::Connection> connections;
    };

    int positionOf(const QString &accountId) const;
    int positionOfModel(const QObject *model) const;
    int offsetOf(int position) const;
    /** @short Position of the source showing @p row, and the row within it */
    std::pair<int, int> locate(int row) const;
    void connectSource(Source &source);
    void disconnectSource(Source &source);
    void removeAt(int position);
    QString folderLabel(const QModelIndex &sourceIndex) const;

    std::vector<Source> m_sources;
    /** Persistent indexes in a source whose layout is changing, and where they point in the source */
    QModelIndexList m_layoutProxy;
    QList<QPersistentModelIndex> m_layoutSource;
};

/** @short The UnifiedMsgListModel as the message list shows it: sorted, filtered and presented like one folder

Every column is presented as PrettyMsgListModel presents it, and the quick filters work the same way. Sorting happens
here rather than on the server: by date and received date, size, subject or correspondents; other columns sort by
date. Equal messages keep the order of their accounts. The tooltips name the account and folder of each message.
*/
class PrettyUnifiedMsgListModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit PrettyUnifiedMsgListModel(QObject *parent = nullptr);

    void setFilters(PrettyMsgListModel::Filters filters);
    PrettyMsgListModel::Filters filters() const;
    /** @short Keep @p index shown even if it stops matching the filters; accepts an index of this or the source model */
    void setKeptVisibleIndex(const QModelIndex &index);
    /** @short Number of messages shown, not counting rows of messages which are not known yet */
    int visibleMessageCount() const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    void refilter(PrettyMsgListModel::Filters filters);

    PrettyMsgListModel::Filters m_filters = PrettyMsgListModel::NoFilter;
    QPersistentModelIndex m_keptVisible;
};

}

}

#endif
