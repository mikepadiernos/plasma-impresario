/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef IMAP_MODEL_GROUPINGMSGLISTMODEL_H
#define IMAP_MODEL_GROUPINGMSGLISTMODEL_H

#include <QAbstractProxyModel>
#include <QDate>
#include <QHash>
#include <QPersistentModelIndex>
#include <QPointer>
#include <memory>
#include <vector>

namespace Imap
{

namespace Mailbox
{

class FavoriteTagsModel;

/** @short Show the messages of a flat message list in collapsible groups

The model sits on top of a flat message list, usually the PrettyMsgListModel while threading is turned off. Only the
top-level rows of the source are used. Each group is a top-level header row whose children are the messages of the
group, in the order of the source, so the active sorting applies within each group.

Header rows are not messages: they are enabled but not selectable, have no UID and return invalid data for all message
roles. RoleIsGroupHeader tells them apart. Column 0 of a header shows the group's name with the number of its messages
and of its unread ones.

Groups:
- Date: Today, Yesterday, Earlier This Week, Last Week, Earlier This Month, then one group per month. Dates after
  today are in a Future group. The groups follow the direction of the sorting, newest first when sorting descending.
- Sender: the address of the first sender.
- Subject: the subject without reply and forward prefixes or list tags, ignoring case.
- Tag: the first favourite tag which the message has, otherwise its alphabetically first tag which is not a system
  flag. A message with several tags is shown only once, in that group. Favourite tags come first, in their order.
- Read state: Unread before Read.
- Mailing list: the List-Post address.

Groups without a value (no date, no sender, no subject, no tag, not from a list) come after the others, and a
Loading group for messages whose envelope is not known yet comes last. The envelope of such messages is requested
asynchronously, so that every message ends up in its group. Except for dates and read states, the groups are in
alphabetical order.
*/
class GroupingMsgListModel : public QAbstractProxyModel
{
    Q_OBJECT
public:
    enum class Grouping {
        None,
        Conversation,
        Date,
        Sender,
        Subject,
        Tag,
        ReadState,
        MailingList,
    };
    Q_ENUM(Grouping)

    explicit GroupingMsgListModel(QObject *parent = nullptr);
    ~GroupingMsgListModel() override;

    /** @short Name of @p grouping for the settings, empty for None and Conversation */
    static QString groupingName(Grouping grouping);
    /** @short The grouping stored as @p name, or None */
    static Grouping groupingFromName(const QString &name);
    /** @short Whether @p grouping is shown by this model, as opposed to a flat or threaded list */
    static bool isRealGrouping(Grouping grouping);

    void setSourceModel(QAbstractItemModel *sourceModel) override;

    Grouping grouping() const;
    void setGrouping(Grouping grouping);
    /** @short The day which the date groups are relative to; an invalid date means today */
    void setReferenceDate(const QDate &date);
    /** @short Order of the date groups; also set by sort() */
    void setGroupOrder(Qt::SortOrder order);
    Qt::SortOrder groupOrder() const;
    void setFavoriteTags(FavoriteTagsModel *favoriteTags);

    bool isGroupHeader(const QModelIndex &index) const;
    /** @short The key identifying the group of a header row, stable while the group exists */
    QString groupKey(const QModelIndex &header) const;
    /** @short The group's name without the counts */
    QString groupLabel(const QModelIndex &header) const;
    int messageCount(const QModelIndex &header) const;
    int unreadCount(const QModelIndex &header) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    QModelIndex sibling(int row, int column, const QModelIndex &idx) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;
    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override;
    QItemSelection mapSelectionToSource(const QItemSelection &selection) const override;
    QItemSelection mapSelectionFromSource(const QItemSelection &selection) const override;
    QVariant data(const QModelIndex &proxyIndex, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QMap<int, QVariant> itemData(const QModelIndex &index) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    QModelIndex buddy(const QModelIndex &index) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

private:
    struct Group;
    struct GroupInfo {
        QString key;
        QString label;
        /** 0 for groups with a value, 1 for the group without one, 2 for messages which are still loading */
        int tier = 0;
        qint64 order = 0;
    };

    GroupInfo groupInfo(int sourceRow, const Group *current) const;
    GroupInfo loadingGroup() const;
    bool groupLessThan(const GroupInfo &left, const GroupInfo &right) const;
    bool isUnread(const QModelIndex &sourceIndex) const;
    Group *groupAt(int row) const;
    int insertionPosition(const GroupInfo &info) const;
    /** @short Add an empty group at its position, announcing it to the views */
    Group *createGroup(const GroupInfo &info);
    void removeGroupIfEmpty(Group *group);
    void renumberGroups(size_t from = 0);
    void emitHeaderChanged(Group *group);
    void buildGroups();
    void clearGroups();
    void relayout();
    void scheduleFetch();
    void fetchMissing();
    bool needsEnvelope() const;

    void sourceAboutToBeReset();
    void sourceReset();
    void sourceLayoutAboutToBeChanged();
    void sourceLayoutChanged();
    void sourceRowsInserted(const QModelIndex &parent, int first, int last);
    void sourceRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last);
    void sourceRowsRemoved(const QModelIndex &parent, int first, int last);
    void sourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void sourceHeaderDataChanged(Qt::Orientation orientation, int first, int last);

    /** @short A persistent index saved across a layout change */
    struct SavedIndex {
        QModelIndex proxy;
        QString headerKey;
        QPersistentModelIndex source;
        int column = 0;
    };

    Grouping m_grouping = Grouping::None;
    QDate m_referenceDate;
    Qt::SortOrder m_order = Qt::AscendingOrder;
    QPointer<FavoriteTagsModel> m_favoriteTags;
    /** The groups in their display order */
    std::vector<std::unique_ptr<Group>> m_groups;
    QHash<QString, Group *> m_groupsByKey;
    /** The group of each top-level source row */
    std::vector<Group *> m_rowGroup;
    std::vector<char> m_rowUnread;
    std::vector<SavedIndex> m_savedIndexes;
    bool m_layoutPending = false;
    bool m_fetchScheduled = false;
    QList<QMetaObject::Connection> m_sourceConnections;
};

}

}

#endif
