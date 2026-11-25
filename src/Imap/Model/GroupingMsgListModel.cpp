/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "GroupingMsgListModel.h"

#include <QItemSelection>
#include <QLocale>
#include <QMimeData>
#include <QSet>
#include <algorithm>

#include "Imap/Model/FavoriteTagsModel.h"
#include "Imap/Model/ItemRoles.h"
#include "UiUtils/QuickFilters.h"
#include "UiUtils/RelatedMessages.h"

namespace Imap
{

namespace Mailbox
{

struct GroupingMsgListModel::Group {
    GroupInfo info;
    /** Top-level rows of the source, in ascending order */
    std::vector<int> rows;
    int unread = 0;
    /** Row of the header */
    int position = 0;
};

namespace {

/** Changing more messages than this at once rebuilds the groups instead of moving each message */
const int maximumMoves = 20;

QString loadingKey()
{
    return QStringLiteral("\u0001loading");
}

/** @short Flags which the user does not see as tags */
bool isSystemFlag(const QString &flag)
{
    if (flag.isEmpty() || flag.startsWith(QLatin1Char('\\')))
        return true;
    static const QStringList special{
        QStringLiteral("$forwarded"), QStringLiteral("$junk"), QStringLiteral("$notjunk"), QStringLiteral("$mdnsent"),
        QStringLiteral("$submitted"), QStringLiteral("$submitpending"), QStringLiteral("junk"), QStringLiteral("nonjunk"),
    };
    return special.contains(flag.toLower());
}

/** @short "Name <address>" of the first address, or just the address */
QString senderLabel(const QVariant &addresses)
{
    const QVariantList list = addresses.toList();
    if (list.isEmpty())
        return QString();
    const QStringList parts = list.constFirst().toStringList();
    if (parts.size() < 4)
        return QString();
    const QString address = parts.at(2) + QLatin1Char('@') + parts.at(3);
    const QString name = parts.at(0).simplified();
    return name.isEmpty() ? address : QStringLiteral("%1 <%2>").arg(name, address);
}

}

GroupingMsgListModel::GroupingMsgListModel(QObject *parent)
    : QAbstractProxyModel(parent)
{
}

GroupingMsgListModel::~GroupingMsgListModel() = default;

QString GroupingMsgListModel::groupingName(Grouping grouping)
{
    switch (grouping) {
    case Grouping::None:
        return QStringLiteral("none");
    case Grouping::Conversation:
        return QStringLiteral("conversation");
    case Grouping::Date:
        return QStringLiteral("date");
    case Grouping::Sender:
        return QStringLiteral("sender");
    case Grouping::Subject:
        return QStringLiteral("subject");
    case Grouping::Tag:
        return QStringLiteral("tag");
    case Grouping::ReadState:
        return QStringLiteral("readState");
    case Grouping::MailingList:
        return QStringLiteral("mailingList");
    }
    return QString();
}

GroupingMsgListModel::Grouping GroupingMsgListModel::groupingFromName(const QString &name)
{
    for (Grouping grouping : {Grouping::Conversation, Grouping::Date, Grouping::Sender, Grouping::Subject, Grouping::Tag,
                              Grouping::ReadState, Grouping::MailingList}) {
        if (name == groupingName(grouping))
            return grouping;
    }
    return Grouping::None;
}

bool GroupingMsgListModel::isRealGrouping(Grouping grouping)
{
    return grouping != Grouping::None && grouping != Grouping::Conversation;
}

void GroupingMsgListModel::setSourceModel(QAbstractItemModel *model)
{
    beginResetModel();
    for (const QMetaObject::Connection &connection : std::as_const(m_sourceConnections))
        disconnect(connection);
    m_sourceConnections.clear();
    m_layoutPending = false;
    m_savedIndexes.clear();
    QAbstractProxyModel::setSourceModel(model);
    if (model) {
        m_sourceConnections << connect(model, &QAbstractItemModel::modelAboutToBeReset, this,
                                       &GroupingMsgListModel::sourceAboutToBeReset);
        m_sourceConnections << connect(model, &QAbstractItemModel::modelReset, this, &GroupingMsgListModel::sourceReset);
        m_sourceConnections << connect(model, &QAbstractItemModel::layoutAboutToBeChanged, this,
                                       &GroupingMsgListModel::sourceLayoutAboutToBeChanged);
        m_sourceConnections << connect(model, &QAbstractItemModel::layoutChanged, this,
                                       &GroupingMsgListModel::sourceLayoutChanged);
        m_sourceConnections << connect(model, &QAbstractItemModel::rowsAboutToBeMoved, this,
                                       &GroupingMsgListModel::sourceLayoutAboutToBeChanged);
        m_sourceConnections << connect(model, &QAbstractItemModel::rowsMoved, this, &GroupingMsgListModel::sourceLayoutChanged);
        m_sourceConnections << connect(model, &QAbstractItemModel::rowsInserted, this, &GroupingMsgListModel::sourceRowsInserted);
        m_sourceConnections << connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this,
                                       &GroupingMsgListModel::sourceRowsAboutToBeRemoved);
        m_sourceConnections << connect(model, &QAbstractItemModel::rowsRemoved, this, &GroupingMsgListModel::sourceRowsRemoved);
        m_sourceConnections << connect(model, &QAbstractItemModel::dataChanged, this,
                                       [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
            sourceDataChanged(topLeft, bottomRight);
        });
        m_sourceConnections << connect(model, &QAbstractItemModel::headerDataChanged, this,
                                       &GroupingMsgListModel::sourceHeaderDataChanged);
        // The columns are the same for all rows, so a change of them is a change of everything
        m_sourceConnections << connect(model, &QAbstractItemModel::columnsAboutToBeInserted, this,
                                       &GroupingMsgListModel::sourceAboutToBeReset);
        m_sourceConnections << connect(model, &QAbstractItemModel::columnsInserted, this, &GroupingMsgListModel::sourceReset);
        m_sourceConnections << connect(model, &QAbstractItemModel::columnsAboutToBeRemoved, this,
                                       &GroupingMsgListModel::sourceAboutToBeReset);
        m_sourceConnections << connect(model, &QAbstractItemModel::columnsRemoved, this, &GroupingMsgListModel::sourceReset);
        m_sourceConnections << connect(model, &QObject::destroyed, this, [this]() {
            beginResetModel();
            clearGroups();
            endResetModel();
        });
    }
    clearGroups();
    buildGroups();
    endResetModel();
}

GroupingMsgListModel::Grouping GroupingMsgListModel::grouping() const
{
    return m_grouping;
}

void GroupingMsgListModel::setGrouping(Grouping grouping)
{
    if (grouping == m_grouping)
        return;
    m_grouping = grouping;
    relayout();
}

void GroupingMsgListModel::setReferenceDate(const QDate &date)
{
    if (date == m_referenceDate)
        return;
    m_referenceDate = date;
    if (m_grouping == Grouping::Date)
        relayout();
}

void GroupingMsgListModel::setGroupOrder(Qt::SortOrder order)
{
    if (order == m_order)
        return;
    m_order = order;
    if (m_grouping == Grouping::Date)
        relayout();
}

Qt::SortOrder GroupingMsgListModel::groupOrder() const
{
    return m_order;
}

void GroupingMsgListModel::setFavoriteTags(FavoriteTagsModel *favoriteTags)
{
    if (m_favoriteTags)
        disconnect(m_favoriteTags, nullptr, this, nullptr);
    m_favoriteTags = favoriteTags;
    if (!favoriteTags)
        return;
    auto changed = [this]() {
        if (m_grouping == Grouping::Tag)
            relayout();
    };
    connect(favoriteTags, &QAbstractItemModel::modelReset, this, changed);
    connect(favoriteTags, &QAbstractItemModel::layoutChanged, this, changed);
    connect(favoriteTags, &QAbstractItemModel::rowsInserted, this, changed);
    connect(favoriteTags, &QAbstractItemModel::rowsRemoved, this, changed);
    connect(favoriteTags, &QAbstractItemModel::rowsMoved, this, changed);
    connect(favoriteTags, &QAbstractItemModel::dataChanged, this, changed);
}

bool GroupingMsgListModel::isGroupHeader(const QModelIndex &index) const
{
    return index.isValid() && index.model() == this && !index.internalPointer();
}

GroupingMsgListModel::Group *GroupingMsgListModel::groupAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_groups.size()))
        return nullptr;
    return m_groups[row].get();
}

QString GroupingMsgListModel::groupKey(const QModelIndex &header) const
{
    const Group *group = isGroupHeader(header) ? groupAt(header.row()) : nullptr;
    return group ? group->info.key : QString();
}

QString GroupingMsgListModel::groupLabel(const QModelIndex &header) const
{
    const Group *group = isGroupHeader(header) ? groupAt(header.row()) : nullptr;
    return group ? group->info.label : QString();
}

int GroupingMsgListModel::messageCount(const QModelIndex &header) const
{
    const Group *group = isGroupHeader(header) ? groupAt(header.row()) : nullptr;
    return group ? static_cast<int>(group->rows.size()) : 0;
}

int GroupingMsgListModel::unreadCount(const QModelIndex &header) const
{
    const Group *group = isGroupHeader(header) ? groupAt(header.row()) : nullptr;
    return group ? group->unread : 0;
}

QModelIndex GroupingMsgListModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || column < 0 || column >= columnCount(parent))
        return QModelIndex();
    if (!parent.isValid())
        return row < static_cast<int>(m_groups.size()) ? createIndex(row, column, nullptr) : QModelIndex();
    if (parent.model() != this || parent.internalPointer() || parent.column() != 0)
        return QModelIndex();
    Group *group = groupAt(parent.row());
    if (!group || row >= static_cast<int>(group->rows.size()))
        return QModelIndex();
    return createIndex(row, column, group);
}

QModelIndex GroupingMsgListModel::parent(const QModelIndex &child) const
{
    if (!child.isValid() || !child.internalPointer())
        return QModelIndex();
    const auto *group = static_cast<const Group *>(child.internalPointer());
    return createIndex(group->position, 0, nullptr);
}

QModelIndex GroupingMsgListModel::sibling(int row, int column, const QModelIndex &idx) const
{
    if (!idx.isValid())
        return QModelIndex();
    if (row == idx.row() && column == idx.column())
        return idx;
    return index(row, column, parent(idx));
}

int GroupingMsgListModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return static_cast<int>(m_groups.size());
    if (parent.model() != this || parent.internalPointer() || parent.column() != 0)
        return 0;
    const Group *group = groupAt(parent.row());
    return group ? static_cast<int>(group->rows.size()) : 0;
}

int GroupingMsgListModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return sourceModel() ? sourceModel()->columnCount() : 0;
}

bool GroupingMsgListModel::hasChildren(const QModelIndex &parent) const
{
    return rowCount(parent) > 0 && columnCount(parent) > 0;
}

QModelIndex GroupingMsgListModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!sourceModel() || !proxyIndex.isValid() || proxyIndex.model() != this || !proxyIndex.internalPointer())
        return QModelIndex();
    const auto *group = static_cast<const Group *>(proxyIndex.internalPointer());
    if (proxyIndex.row() >= static_cast<int>(group->rows.size()))
        return QModelIndex();
    return sourceModel()->index(group->rows[proxyIndex.row()], proxyIndex.column());
}

QModelIndex GroupingMsgListModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    if (!sourceModel() || !sourceIndex.isValid() || sourceIndex.model() != sourceModel() || sourceIndex.parent().isValid())
        return QModelIndex();
    const int row = sourceIndex.row();
    if (row >= static_cast<int>(m_rowGroup.size()) || !m_rowGroup[row])
        return QModelIndex();
    Group *group = m_rowGroup[row];
    const auto it = std::lower_bound(group->rows.cbegin(), group->rows.cend(), row);
    if (it == group->rows.cend() || *it != row)
        return QModelIndex();
    return createIndex(static_cast<int>(it - group->rows.cbegin()), sourceIndex.column(), group);
}

QItemSelection GroupingMsgListModel::mapSelectionToSource(const QItemSelection &selection) const
{
    // The messages of a group are not next to each other in the source
    QItemSelection result;
    for (const QItemSelectionRange &range : selection) {
        for (int row = range.top(); row <= range.bottom(); ++row) {
            const QModelIndex left = mapToSource(index(row, range.left(), range.parent()));
            const QModelIndex right = mapToSource(index(row, range.right(), range.parent()));
            if (left.isValid() && right.isValid())
                result.append(QItemSelectionRange(left, right));
        }
    }
    return result;
}

QItemSelection GroupingMsgListModel::mapSelectionFromSource(const QItemSelection &selection) const
{
    QItemSelection result;
    for (const QItemSelectionRange &range : selection) {
        if (range.parent().isValid())
            continue;
        for (int row = range.top(); row <= range.bottom(); ++row) {
            const QModelIndex left = mapFromSource(range.model()->index(row, range.left()));
            const QModelIndex right = mapFromSource(range.model()->index(row, range.right()));
            if (left.isValid() && right.isValid())
                result.append(QItemSelectionRange(left, right));
        }
    }
    return result;
}

QVariant GroupingMsgListModel::data(const QModelIndex &proxyIndex, int role) const
{
    if (!proxyIndex.isValid() || proxyIndex.model() != this)
        return QVariant();
    if (role == RoleIsGroupHeader)
        return !proxyIndex.internalPointer();
    if (proxyIndex.internalPointer())
        return sourceModel()->data(mapToSource(proxyIndex), role);

    // A header row; it is not a message, so all message roles stay empty
    const Group *group = groupAt(proxyIndex.row());
    if (!group || proxyIndex.column() != 0)
        return QVariant();
    const int count = static_cast<int>(group->rows.size());
    switch (role) {
    case Qt::DisplayRole:
    case Qt::AccessibleTextRole:
        return group->unread ? tr("%1 (%2, %3 unread)").arg(group->info.label, QString::number(count), QString::number(group->unread))
                             : tr("%1 (%2)").arg(group->info.label, QString::number(count));
    case Qt::ToolTipRole:
        return group->unread ? tr("%1: %n message(s), %2 unread", nullptr, count).arg(group->info.label, QString::number(group->unread))
                             : tr("%1: %n message(s)", nullptr, count).arg(group->info.label);
    default:
        return QVariant();
    }
}

QVariant GroupingMsgListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    // The default implementation would map the first row, which is a group header
    if (!sourceModel() || orientation != Qt::Horizontal)
        return QVariant();
    return sourceModel()->headerData(section, orientation, role);
}

QMap<int, QVariant> GroupingMsgListModel::itemData(const QModelIndex &index) const
{
    if (!index.isValid() || index.model() != this)
        return {};
    if (index.internalPointer())
        return sourceModel()->itemData(mapToSource(index));
    QMap<int, QVariant> result;
    for (int role : {int(Qt::DisplayRole), int(Qt::ToolTipRole)}) {
        const QVariant value = data(index, role);
        if (value.isValid())
            result.insert(role, value);
    }
    return result;
}

Qt::ItemFlags GroupingMsgListModel::flags(const QModelIndex &index) const
{
    if (!sourceModel())
        return Qt::NoItemFlags;
    if (!index.isValid())
        return sourceModel()->flags(QModelIndex()) & Qt::ItemIsDropEnabled;
    if (index.model() != this)
        return Qt::NoItemFlags;
    if (!index.internalPointer())
        return Qt::ItemIsEnabled;
    return sourceModel()->flags(mapToSource(index));
}

QMimeData *GroupingMsgListModel::mimeData(const QModelIndexList &indexes) const
{
    if (!sourceModel())
        return nullptr;
    QModelIndexList messages;
    for (const QModelIndex &index : indexes) {
        const QModelIndex source = mapToSource(index);
        if (source.isValid())
            messages << source;
    }
    return messages.isEmpty() ? nullptr : sourceModel()->mimeData(messages);
}

QModelIndex GroupingMsgListModel::buddy(const QModelIndex &index) const
{
    if (isGroupHeader(index))
        return index;
    return QAbstractProxyModel::buddy(index);
}

bool GroupingMsgListModel::canFetchMore(const QModelIndex &parent) const
{
    // A group is complete; only the whole list may have more to come
    return sourceModel() && !parent.isValid() && sourceModel()->canFetchMore(QModelIndex());
}

void GroupingMsgListModel::fetchMore(const QModelIndex &parent)
{
    if (sourceModel() && !parent.isValid())
        sourceModel()->fetchMore(QModelIndex());
}

void GroupingMsgListModel::sort(int column, Qt::SortOrder order)
{
    if (sourceModel())
        sourceModel()->sort(column, order);
    setGroupOrder(order);
}

bool GroupingMsgListModel::isUnread(const QModelIndex &sourceIndex) const
{
    return sourceIndex.data(RoleMessageUid).toUInt() != 0 && !sourceIndex.data(RoleMessageIsMarkedRead).toBool();
}

bool GroupingMsgListModel::needsEnvelope() const
{
    switch (m_grouping) {
    case Grouping::Date:
    case Grouping::Sender:
    case Grouping::Subject:
    case Grouping::MailingList:
        return true;
    default:
        return false;
    }
}

GroupingMsgListModel::GroupInfo GroupingMsgListModel::loadingGroup() const
{
    GroupInfo info;
    info.key = loadingKey();
    info.label = tr("Loading\u2026");
    info.tier = 2;
    return info;
}

GroupingMsgListModel::GroupInfo GroupingMsgListModel::groupInfo(int sourceRow, const Group *current) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0);
    // A message which is on its way out has lost its UID; it stays where it is until it is gone
    if (current && index.data(RoleMessageUid).toUInt() == 0)
        return current->info;

    GroupInfo info;
    switch (m_grouping) {
    case Grouping::None:
    case Grouping::Conversation:
        info.key = QStringLiteral("all");
        info.label = tr("All Messages");
        return info;
    case Grouping::ReadState: {
        const bool unread = isUnread(index);
        info.key = unread ? QStringLiteral("unread") : QStringLiteral("read");
        info.label = unread ? tr("Unread") : tr("Read");
        info.order = unread ? 0 : 1;
        return info;
    }
    case Grouping::Tag: {
        // Flags are known without downloading the message
        const QStringList flags = index.data(RoleMessageFlags).toStringList();
        QString tag;
        const int favorites = m_favoriteTags ? m_favoriteTags->rowCount() : 0;
        for (int i = 0; i < favorites && tag.isEmpty(); ++i) {
            const QString name = m_favoriteTags->tagNameByIndex(i);
            for (const QString &flag : flags) {
                if (!name.isEmpty() && flag.compare(name, Qt::CaseInsensitive) == 0) {
                    tag = flag;
                    info.order = i;
                    break;
                }
            }
        }
        if (tag.isEmpty()) {
            QStringList tags;
            for (const QString &flag : flags) {
                if (!isSystemFlag(flag))
                    tags << flag;
            }
            if (!tags.isEmpty()) {
                std::sort(tags.begin(), tags.end(), [](const QString &left, const QString &right) {
                    return QString::localeAwareCompare(left.toCaseFolded(), right.toCaseFolded()) < 0;
                });
                tag = tags.constFirst();
                info.order = favorites;
            }
        }
        if (tag.isEmpty()) {
            info.key = QStringLiteral("\u0002none");
            info.label = tr("No Tag");
            info.tier = 1;
        } else {
            info.key = QLatin1String("t:") + tag.toCaseFolded();
            info.label = tag;
        }
        return info;
    }
    default:
        break;
    }

    // Asking for the envelope of a message which has not been loaded would download it here
    if (!index.data(RoleIsFetched).toBool())
        return loadingGroup();

    switch (m_grouping) {
    case Grouping::Date: {
        const QDateTime dateTime = index.data(RoleMessageDate).toDateTime();
        if (!dateTime.isValid()) {
            info.key = QStringLiteral("\u0002none");
            info.label = tr("No Date");
            info.tier = 1;
            return info;
        }
        const QDate date = dateTime.toLocalTime().date();
        const QDate today = m_referenceDate.isValid() ? m_referenceDate : QDate::currentDate();
        const QDate weekStart = today.addDays(-((today.dayOfWeek() - QLocale().firstDayOfWeek() + 7) % 7));
        const QDate lastWeekStart = weekStart.addDays(-7);
        const QDate monthStart(today.year(), today.month(), 1);
        if (date > today) {
            info.key = QStringLiteral("future");
            info.label = tr("Future");
            info.order = today.toJulianDay() + 1;
        } else if (date == today) {
            info.key = QStringLiteral("today");
            info.label = tr("Today");
            info.order = today.toJulianDay();
        } else if (date == today.addDays(-1)) {
            info.key = QStringLiteral("yesterday");
            info.label = tr("Yesterday");
            info.order = date.toJulianDay();
        } else if (date >= weekStart) {
            info.key = QStringLiteral("thisWeek");
            info.label = tr("Earlier This Week");
            info.order = weekStart.toJulianDay();
        } else if (date >= lastWeekStart) {
            info.key = QStringLiteral("lastWeek");
            info.label = tr("Last Week");
            info.order = lastWeekStart.toJulianDay();
        } else if (date >= monthStart) {
            info.key = QStringLiteral("thisMonth");
            info.label = tr("Earlier This Month");
            info.order = monthStart.toJulianDay();
        } else {
            const QDate first(date.year(), date.month(), 1);
            info.key = QLatin1String("m:") + first.toString(QStringLiteral("yyyy-MM"));
            info.label = QLocale().toString(first, QStringLiteral("MMMM yyyy"));
            info.order = first.toJulianDay();
        }
        return info;
    }
    case Grouping::Sender: {
        const QVariant from = index.data(RoleMessageFrom);
        const QString address = UiUtils::firstAddress(from);
        if (address.isEmpty()) {
            info.key = QStringLiteral("\u0002none");
            info.label = tr("Unknown Sender");
            info.tier = 1;
        } else {
            info.key = QLatin1String("s:") + address;
            info.label = senderLabel(from);
        }
        return info;
    }
    case Grouping::Subject: {
        const QString subject = UiUtils::searchableSubject(index.data(RoleMessageSubject).toString());
        if (subject.isEmpty()) {
            info.key = QStringLiteral("\u0002none");
            info.label = tr("(no subject)");
            info.tier = 1;
        } else {
            info.key = QLatin1String("j:") + subject.toCaseFolded();
            info.label = subject;
        }
        return info;
    }
    case Grouping::MailingList: {
        const QString list = UiUtils::listPostAddress(index.data(RoleMessageHeaderListPost));
        if (list.isEmpty()) {
            info.key = QStringLiteral("\u0002none");
            info.label = tr("Not from a list");
            info.tier = 1;
        } else {
            info.key = QLatin1String("l:") + list;
            info.label = list;
        }
        return info;
    }
    default:
        return info;
    }
}

bool GroupingMsgListModel::groupLessThan(const GroupInfo &left, const GroupInfo &right) const
{
    if (left.tier != right.tier)
        return left.tier < right.tier;
    if (left.order != right.order) {
        if (m_grouping == Grouping::Date && m_order == Qt::DescendingOrder)
            return left.order > right.order;
        return left.order < right.order;
    }
    const int byLabel = QString::localeAwareCompare(left.label.toCaseFolded(), right.label.toCaseFolded());
    if (byLabel != 0)
        return byLabel < 0;
    return left.key < right.key;
}

int GroupingMsgListModel::insertionPosition(const GroupInfo &info) const
{
    const auto it = std::upper_bound(m_groups.cbegin(), m_groups.cend(), info,
                                     [this](const GroupInfo &value, const std::unique_ptr<Group> &group) {
        return groupLessThan(value, group->info);
    });
    return static_cast<int>(it - m_groups.cbegin());
}

void GroupingMsgListModel::renumberGroups(size_t from)
{
    for (size_t i = from; i < m_groups.size(); ++i)
        m_groups[i]->position = static_cast<int>(i);
}

GroupingMsgListModel::Group *GroupingMsgListModel::createGroup(const GroupInfo &info)
{
    const int position = insertionPosition(info);
    beginInsertRows(QModelIndex(), position, position);
    auto group = std::make_unique<Group>();
    group->info = info;
    Group *result = group.get();
    m_groups.insert(m_groups.begin() + position, std::move(group));
    m_groupsByKey.insert(info.key, result);
    renumberGroups(position);
    endInsertRows();
    return result;
}

void GroupingMsgListModel::removeGroupIfEmpty(Group *group)
{
    if (!group->rows.empty())
        return;
    const int position = group->position;
    beginRemoveRows(QModelIndex(), position, position);
    m_groupsByKey.remove(group->info.key);
    m_groups.erase(m_groups.begin() + position);
    renumberGroups(position);
    endRemoveRows();
}

void GroupingMsgListModel::emitHeaderChanged(Group *group)
{
    const QModelIndex header = createIndex(group->position, 0, nullptr);
    emit dataChanged(header, header, {Qt::DisplayRole, Qt::ToolTipRole, Qt::AccessibleTextRole});
}

void GroupingMsgListModel::clearGroups()
{
    m_groups.clear();
    m_groupsByKey.clear();
    m_rowGroup.clear();
    m_rowUnread.clear();
}

void GroupingMsgListModel::buildGroups()
{
    if (!sourceModel())
        return;
    const int rows = sourceModel()->rowCount();
    m_rowGroup.assign(rows, nullptr);
    m_rowUnread.assign(rows, 0);
    for (int row = 0; row < rows; ++row) {
        const GroupInfo info = groupInfo(row, nullptr);
        Group *group = m_groupsByKey.value(info.key);
        if (!group) {
            auto created = std::make_unique<Group>();
            created->info = info;
            group = created.get();
            m_groups.push_back(std::move(created));
            m_groupsByKey.insert(info.key, group);
        }
        group->rows.push_back(row);
        m_rowGroup[row] = group;
        const bool unread = isUnread(sourceModel()->index(row, 0));
        m_rowUnread[row] = unread;
        group->unread += unread;
    }
    std::stable_sort(m_groups.begin(), m_groups.end(), [this](const std::unique_ptr<Group> &left, const std::unique_ptr<Group> &right) {
        return groupLessThan(left->info, right->info);
    });
    renumberGroups();
    scheduleFetch();
}

void GroupingMsgListModel::relayout()
{
    // A pending change of the source's layout rebuilds the groups anyway
    if (m_layoutPending || !sourceModel())
        return;
    sourceLayoutAboutToBeChanged();
    sourceLayoutChanged();
}

void GroupingMsgListModel::sourceAboutToBeReset()
{
    beginResetModel();
}

void GroupingMsgListModel::sourceReset()
{
    m_layoutPending = false;
    m_savedIndexes.clear();
    clearGroups();
    buildGroups();
    endResetModel();
}

void GroupingMsgListModel::sourceLayoutAboutToBeChanged()
{
    if (m_layoutPending)
        return;
    m_layoutPending = true;
    // First, so that the views and selection models have their persistent indexes ready
    emit layoutAboutToBeChanged();
    m_savedIndexes.clear();
    const QModelIndexList persistent = persistentIndexList();
    m_savedIndexes.reserve(persistent.size());
    for (const QModelIndex &index : persistent) {
        SavedIndex saved;
        saved.proxy = index;
        saved.column = index.column();
        if (index.internalPointer()) {
            saved.source = mapToSource(index);
        } else if (const Group *group = groupAt(index.row())) {
            saved.headerKey = group->info.key;
        }
        m_savedIndexes.push_back(saved);
    }
}

void GroupingMsgListModel::sourceLayoutChanged()
{
    if (!m_layoutPending)
        return;
    clearGroups();
    buildGroups();
    QModelIndexList from;
    QModelIndexList to;
    from.reserve(m_savedIndexes.size());
    to.reserve(m_savedIndexes.size());
    for (const SavedIndex &saved : std::as_const(m_savedIndexes)) {
        QModelIndex target;
        if (saved.source.isValid()) {
            target = mapFromSource(saved.source);
        } else if (!saved.headerKey.isEmpty()) {
            if (const Group *group = m_groupsByKey.value(saved.headerKey))
                target = createIndex(group->position, saved.column, nullptr);
        }
        from << saved.proxy;
        to << target;
    }
    m_savedIndexes.clear();
    changePersistentIndexList(from, to);
    m_layoutPending = false;
    emit layoutChanged();
}

void GroupingMsgListModel::sourceRowsInserted(const QModelIndex &parent, int first, int last)
{
    if (parent.isValid() || m_layoutPending)
        return;
    const int count = last - first + 1;
    for (const auto &group : m_groups) {
        for (int &row : group->rows) {
            if (row >= first)
                row += count;
        }
    }
    m_rowGroup.insert(m_rowGroup.begin() + first, count, nullptr);
    m_rowUnread.insert(m_rowUnread.begin() + first, count, 0);

    // The new rows of each group, in the order in which the groups were first seen
    std::vector<std::pair<GroupInfo, std::vector<int>>> batches;
    QHash<QString, size_t> batchByKey;
    for (int row = first; row <= last; ++row) {
        const GroupInfo info = groupInfo(row, nullptr);
        auto it = batchByKey.constFind(info.key);
        if (it == batchByKey.cend()) {
            it = batchByKey.insert(info.key, batches.size());
            batches.emplace_back(info, std::vector<int>());
        }
        batches[*it].second.push_back(row);
    }

    for (const auto &[info, rows] : batches) {
        Group *group = m_groupsByKey.value(info.key);
        if (!group) {
            // The whole group is new, so it arrives with its messages
            const int position = insertionPosition(info);
            beginInsertRows(QModelIndex(), position, position);
            auto created = std::make_unique<Group>();
            created->info = info;
            group = created.get();
            group->rows = rows;
            for (int row : rows) {
                m_rowGroup[row] = group;
                const bool unread = isUnread(sourceModel()->index(row, 0));
                m_rowUnread[row] = unread;
                group->unread += unread;
            }
            m_groups.insert(m_groups.begin() + position, std::move(created));
            m_groupsByKey.insert(info.key, group);
            renumberGroups(position);
            endInsertRows();
            continue;
        }

        // Where the new rows end up; consecutive positions are inserted together
        std::vector<int> merged;
        merged.reserve(group->rows.size() + rows.size());
        std::merge(group->rows.cbegin(), group->rows.cend(), rows.cbegin(), rows.cend(), std::back_inserter(merged));
        std::vector<int> positions;
        positions.reserve(rows.size());
        for (int row : rows)
            positions.push_back(static_cast<int>(std::lower_bound(merged.cbegin(), merged.cend(), row) - merged.cbegin()));
        for (size_t start = 0; start < rows.size();) {
            size_t end = start;
            while (end + 1 < rows.size() && positions[end + 1] == positions[end] + 1)
                ++end;
            beginInsertRows(createIndex(group->position, 0, nullptr), positions[start], positions[end]);
            group->rows.insert(group->rows.begin() + positions[start], rows.begin() + start, rows.begin() + end + 1);
            for (size_t i = start; i <= end; ++i) {
                m_rowGroup[rows[i]] = group;
                const bool unread = isUnread(sourceModel()->index(rows[i], 0));
                m_rowUnread[rows[i]] = unread;
                group->unread += unread;
            }
            endInsertRows();
            start = end + 1;
        }
        emitHeaderChanged(group);
    }
    scheduleFetch();
}

void GroupingMsgListModel::sourceRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
    if (parent.isValid() || m_layoutPending)
        return;
    std::vector<std::pair<Group *, std::vector<int>>> affected;
    QHash<Group *, size_t> affectedIndex;
    for (int row = first; row <= last && row < static_cast<int>(m_rowGroup.size()); ++row) {
        Group *group = m_rowGroup[row];
        if (!group)
            continue;
        auto it = affectedIndex.constFind(group);
        if (it == affectedIndex.cend()) {
            it = affectedIndex.insert(group, affected.size());
            affected.emplace_back(group, std::vector<int>());
        }
        const auto position = std::lower_bound(group->rows.cbegin(), group->rows.cend(), row) - group->rows.cbegin();
        affected[*it].second.push_back(static_cast<int>(position));
    }

    for (auto &[group, positions] : affected) {
        // The positions are ascending; removing from the end keeps the others valid
        for (size_t end = positions.size(); end > 0;) {
            size_t start = end - 1;
            while (start > 0 && positions[start - 1] == positions[start] - 1)
                --start;
            const int from = positions[start];
            const int to = positions[end - 1];
            beginRemoveRows(createIndex(group->position, 0, nullptr), from, to);
            for (int position = from; position <= to; ++position) {
                const int row = group->rows[position];
                group->unread -= m_rowUnread[row];
                m_rowUnread[row] = 0;
                m_rowGroup[row] = nullptr;
            }
            group->rows.erase(group->rows.begin() + from, group->rows.begin() + to + 1);
            endRemoveRows();
            end = start;
        }
        if (group->rows.empty())
            removeGroupIfEmpty(group);
        else
            emitHeaderChanged(group);
    }
}

void GroupingMsgListModel::sourceRowsRemoved(const QModelIndex &parent, int first, int last)
{
    if (parent.isValid() || m_layoutPending)
        return;
    const int count = last - first + 1;
    const int end = std::min(last + 1, static_cast<int>(m_rowGroup.size()));
    if (first < end) {
        m_rowGroup.erase(m_rowGroup.begin() + first, m_rowGroup.begin() + end);
        m_rowUnread.erase(m_rowUnread.begin() + first, m_rowUnread.begin() + end);
    }
    for (const auto &group : m_groups) {
        for (int &row : group->rows) {
            if (row > last)
                row -= count;
        }
    }
}

void GroupingMsgListModel::sourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    if (!topLeft.isValid() || topLeft.parent().isValid() || m_layoutPending)
        return;
    QSet<QString> changedHeaders;
    std::vector<std::pair<int, GroupInfo>> moves;
    std::vector<std::pair<Group *, std::vector<int>>> unchanged;
    QHash<Group *, size_t> unchangedIndex;
    const int last = std::min(bottomRight.row(), static_cast<int>(m_rowGroup.size()) - 1);
    for (int row = topLeft.row(); row <= last; ++row) {
        Group *group = m_rowGroup[row];
        if (!group)
            continue;
        const bool unread = isUnread(sourceModel()->index(row, 0));
        if (unread != bool(m_rowUnread[row])) {
            m_rowUnread[row] = unread;
            group->unread += unread ? 1 : -1;
            changedHeaders.insert(group->info.key);
        }
        const GroupInfo info = groupInfo(row, group);
        if (info.key != group->info.key) {
            moves.emplace_back(row, info);
            continue;
        }
        auto it = unchangedIndex.constFind(group);
        if (it == unchangedIndex.cend()) {
            it = unchangedIndex.insert(group, unchanged.size());
            unchanged.emplace_back(group, std::vector<int>());
        }
        const auto position = std::lower_bound(group->rows.cbegin(), group->rows.cend(), row) - group->rows.cbegin();
        unchanged[*it].second.push_back(static_cast<int>(position));
    }

    for (const auto &[group, positions] : unchanged) {
        const QModelIndex header = createIndex(group->position, 0, nullptr);
        for (size_t start = 0; start < positions.size();) {
            size_t end = start;
            while (end + 1 < positions.size() && positions[end + 1] == positions[end] + 1)
                ++end;
            emit dataChanged(index(positions[start], topLeft.column(), header),
                             index(positions[end], bottomRight.column(), header));
            start = end + 1;
        }
    }

    if (static_cast<int>(moves.size()) > maximumMoves) {
        relayout();
        return;
    }
    for (const auto &[row, info] : moves) {
        Group *from = m_rowGroup[row];
        Group *to = m_groupsByKey.value(info.key);
        if (!to)
            to = createGroup(info);
        const int fromPosition = static_cast<int>(std::lower_bound(from->rows.cbegin(), from->rows.cend(), row) - from->rows.cbegin());
        const int toPosition = static_cast<int>(std::lower_bound(to->rows.cbegin(), to->rows.cend(), row) - to->rows.cbegin());
        const bool moving = beginMoveRows(createIndex(from->position, 0, nullptr), fromPosition, fromPosition,
                                          createIndex(to->position, 0, nullptr), toPosition);
        Q_ASSERT(moving);
        from->rows.erase(from->rows.begin() + fromPosition);
        from->unread -= m_rowUnread[row];
        to->rows.insert(to->rows.begin() + toPosition, row);
        to->unread += m_rowUnread[row];
        m_rowGroup[row] = to;
        if (moving)
            endMoveRows();
        changedHeaders.insert(from->info.key);
        changedHeaders.insert(to->info.key);
        removeGroupIfEmpty(from);
    }
    for (const QString &key : std::as_const(changedHeaders)) {
        if (Group *group = m_groupsByKey.value(key))
            emitHeaderChanged(group);
    }
}

void GroupingMsgListModel::sourceHeaderDataChanged(Qt::Orientation orientation, int first, int last)
{
    if (orientation == Qt::Horizontal)
        emit headerDataChanged(orientation, first, last);
}

void GroupingMsgListModel::scheduleFetch()
{
    if (m_fetchScheduled || !needsEnvelope() || !m_groupsByKey.contains(loadingKey()))
        return;
    m_fetchScheduled = true;
    QMetaObject::invokeMethod(this, &GroupingMsgListModel::fetchMissing, Qt::QueuedConnection);
}

void GroupingMsgListModel::fetchMissing()
{
    m_fetchScheduled = false;
    const Group *loading = needsEnvelope() && sourceModel() ? m_groupsByKey.value(loadingKey()) : nullptr;
    if (!loading)
        return;
    // Asking for the subject loads the envelope; the answer may arrive right away and move the messages
    std::vector<QPersistentModelIndex> pending;
    pending.reserve(loading->rows.size());
    for (int row : loading->rows)
        pending.emplace_back(sourceModel()->index(row, 0));
    for (const QPersistentModelIndex &index : pending) {
        if (index.isValid())
            index.data(RoleMessageSubject);
    }
}

}

}
