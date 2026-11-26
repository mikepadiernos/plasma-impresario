/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UnifiedMsgListModel.h"

#include <QDateTime>
#include <QTextDocument>

#include "ItemRoles.h"
#include "MsgListModel.h"
#include "UiUtils/Formatting.h"
#include "UiUtils/Relevance.h"

namespace Imap
{

namespace Mailbox
{

UnifiedMsgListModel::UnifiedMsgListModel(QObject *parent) : QAbstractProxyModel(parent) { }

UnifiedMsgListModel::~UnifiedMsgListModel()
{
    for (Source &source : m_sources)
        disconnectSource(source);
}

int UnifiedMsgListModel::positionOf(const QString &accountId) const
{
    for (int i = 0, count = int(m_sources.size()); i < count; ++i) {
        if (m_sources[i].accountId == accountId)
            return i;
    }
    return -1;
}

int UnifiedMsgListModel::positionOfModel(const QObject *model) const
{
    if (!model)
        return -1;
    for (int i = 0, count = int(m_sources.size()); i < count; ++i) {
        if (m_sources[i].raw == model)
            return i;
    }
    return -1;
}

int UnifiedMsgListModel::offsetOf(const int position) const
{
    int offset = 0;
    for (int i = 0; i < position; ++i)
        offset += m_sources[i].rows;
    return offset;
}

std::pair<int, int> UnifiedMsgListModel::locate(int row) const
{
    for (int i = 0, count = int(m_sources.size()); i < count; ++i) {
        if (row < m_sources[i].rows)
            return {i, row};
        row -= m_sources[i].rows;
    }
    return {-1, -1};
}

bool UnifiedMsgListModel::setAccountSource(const QString &accountId, const QString &accountName,
                                           QAbstractItemModel *messages)
{
    const int owner = positionOfModel(messages);
    if (owner >= 0)
        return m_sources[owner].accountId == accountId;

    int position = positionOf(accountId);
    if (position < 0) {
        Source source;
        source.accountId = accountId;
        source.accountName = accountName;
        m_sources.push_back(std::move(source));
        position = int(m_sources.size()) - 1;
    } else {
        m_sources[position].accountName = accountName;
        removeAt(position);
    }

    Source &source = m_sources[position];
    source.model = messages;
    source.raw = messages;
    const int rows = messages ? messages->rowCount() : 0;
    if (rows > 0) {
        const int offset = offsetOf(position);
        beginInsertRows(QModelIndex(), offset, offset + rows - 1);
        source.rows = rows;
        endInsertRows();
    }
    if (messages)
        connectSource(source);
    emit accountsChanged();
    return true;
}

void UnifiedMsgListModel::removeAt(const int position)
{
    Source &source = m_sources[position];
    disconnectSource(source);
    if (source.rows > 0) {
        const int offset = offsetOf(position);
        beginRemoveRows(QModelIndex(), offset, offset + source.rows - 1);
        source.rows = 0;
        endRemoveRows();
    }
    source.model = nullptr;
    source.raw = nullptr;
}

void UnifiedMsgListModel::removeAccount(const QString &accountId)
{
    const int position = positionOf(accountId);
    if (position < 0)
        return;
    removeAt(position);
    m_sources.erase(m_sources.begin() + position);
    emit accountsChanged();
}

void UnifiedMsgListModel::setAccountName(const QString &accountId, const QString &accountName)
{
    const int position = positionOf(accountId);
    if (position < 0 || m_sources[position].accountName == accountName)
        return;
    m_sources[position].accountName = accountName;
    const int offset = offsetOf(position);
    if (m_sources[position].rows > 0) {
        emit dataChanged(index(offset, 0), index(offset + m_sources[position].rows - 1, columnCount() - 1),
                         {RoleMessageAccountName, RoleMessageSourceLabel, RoleMessageRowLabel, Qt::ToolTipRole});
    }
    emit accountsChanged();
}

void UnifiedMsgListModel::setShowsFolders(const bool showsFolders)
{
    if (m_showsFolders == showsFolders)
        return;
    m_showsFolders = showsFolders;
    if (rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {RoleMessageRowLabel});
}

bool UnifiedMsgListModel::showsFolders() const
{
    return m_showsFolders;
}

QStringList UnifiedMsgListModel::accountIds() const
{
    QStringList ids;
    for (const Source &source : m_sources)
        ids << source.accountId;
    return ids;
}

QString UnifiedMsgListModel::accountName(const QString &accountId) const
{
    const int position = positionOf(accountId);
    return position < 0 ? QString() : m_sources[position].accountName;
}

QAbstractItemModel *UnifiedMsgListModel::accountSource(const QString &accountId) const
{
    const int position = positionOf(accountId);
    return position < 0 ? nullptr : m_sources[position].model.data();
}

QString UnifiedMsgListModel::accountIdOfSource(const QAbstractItemModel *sourceModel) const
{
    const int position = positionOfModel(sourceModel);
    return position < 0 ? QString() : m_sources[position].accountId;
}

void UnifiedMsgListModel::setAccountState(const QString &accountId, const AccountState state, const QString &detail)
{
    const int position = positionOf(accountId);
    if (position < 0)
        return;
    Source &source = m_sources[position];
    if (source.state == state && source.detail == detail)
        return;
    source.state = state;
    source.detail = detail;
    emit accountsChanged();
}

UnifiedMsgListModel::AccountState UnifiedMsgListModel::accountState(const QString &accountId) const
{
    const int position = positionOf(accountId);
    return position < 0 ? AccountState::Unavailable : m_sources[position].state;
}

QString UnifiedMsgListModel::accountStateDetail(const QString &accountId) const
{
    const int position = positionOf(accountId);
    return position < 0 ? QString() : m_sources[position].detail;
}

int UnifiedMsgListModel::messageCount(const QString &accountId) const
{
    int count = 0;
    for (const Source &source : m_sources) {
        if (accountId.isEmpty() || source.accountId == accountId)
            count += source.rows;
    }
    return count;
}

int UnifiedMsgListModel::unreadCount(const QString &accountId) const
{
    int count = 0;
    for (const Source &source : m_sources) {
        if (!source.model || (!accountId.isEmpty() && source.accountId != accountId))
            continue;
        if (const auto *msgList = qobject_cast<const MsgListModel *>(source.model.data())) {
            const QVariant unread = msgList->currentMailbox().data(RoleUnreadMessageCount);
            if (unread.isValid()) {
                count += unread.toInt();
                continue;
            }
        }
        for (int row = 0; row < source.rows; ++row) {
            const QVariant read = source.model->index(row, 0).data(RoleMessageIsMarkedRead);
            if (read.isValid() && !read.toBool())
                ++count;
        }
    }
    return count;
}

void UnifiedMsgListModel::connectSource(Source &source)
{
    QAbstractItemModel *model = source.model.data();
    const QObject *raw = model;
    auto &c = source.connections;
    c.push_back(connect(model, &QAbstractItemModel::rowsAboutToBeInserted, this,
                        [this, raw](const QModelIndex &parent, int first, int last) {
                            const int position = positionOfModel(raw);
                            if (parent.isValid() || position < 0)
                                return;
                            const int offset = offsetOf(position);
                            beginInsertRows(QModelIndex(), offset + first, offset + last);
                        }));
    c.push_back(connect(model, &QAbstractItemModel::rowsInserted, this,
                        [this, raw](const QModelIndex &parent, int first, int last) {
                            const int position = positionOfModel(raw);
                            if (parent.isValid() || position < 0)
                                return;
                            m_sources[position].rows += last - first + 1;
                            endInsertRows();
                        }));
    c.push_back(connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this,
                        [this, raw](const QModelIndex &parent, int first, int last) {
                            const int position = positionOfModel(raw);
                            if (parent.isValid() || position < 0)
                                return;
                            const int offset = offsetOf(position);
                            beginRemoveRows(QModelIndex(), offset + first, offset + last);
                        }));
    c.push_back(connect(model, &QAbstractItemModel::rowsRemoved, this,
                        [this, raw](const QModelIndex &parent, int first, int last) {
                            const int position = positionOfModel(raw);
                            if (parent.isValid() || position < 0)
                                return;
                            m_sources[position].rows -= last - first + 1;
                            endRemoveRows();
                        }));
    c.push_back(
        connect(model, &QAbstractItemModel::dataChanged, this,
                [this, raw](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles) {
                    const int position = positionOfModel(raw);
                    if (topLeft.parent().isValid() || position < 0)
                        return;
                    const int offset = offsetOf(position);
                    emit dataChanged(index(offset + topLeft.row(), topLeft.column()),
                                     index(offset + bottomRight.row(), bottomRight.column()), roles);
                }));
    c.push_back(connect(model, &QAbstractItemModel::modelAboutToBeReset, this, [this]() { beginResetModel(); }));
    c.push_back(connect(model, &QAbstractItemModel::modelReset, this, [this, raw]() {
        const int position = positionOfModel(raw);
        if (position >= 0)
            m_sources[position].rows = m_sources[position].model ? m_sources[position].model->rowCount() : 0;
        endResetModel();
    }));
    c.push_back(connect(model, &QAbstractItemModel::layoutAboutToBeChanged, this, [this, raw]() {
        emit layoutAboutToBeChanged();
        const int position = positionOfModel(raw);
        if (position < 0)
            return;
        const int offset = offsetOf(position);
        const int end = offset + m_sources[position].rows;
        const QModelIndexList persistent = persistentIndexList();
        for (const QModelIndex &proxy : persistent) {
            if (proxy.row() >= offset && proxy.row() < end) {
                m_layoutProxy << proxy;
                m_layoutSource << QPersistentModelIndex(mapToSource(proxy));
            }
        }
    }));
    c.push_back(connect(model, &QAbstractItemModel::layoutChanged, this, [this, raw]() {
        const int position = positionOfModel(raw);
        if (position >= 0)
            m_sources[position].rows = m_sources[position].model ? m_sources[position].model->rowCount() : 0;
        for (int i = 0, count = int(m_layoutProxy.size()); i < count; ++i)
            changePersistentIndex(m_layoutProxy.at(i), mapFromSource(m_layoutSource.at(i)));
        m_layoutProxy.clear();
        m_layoutSource.clear();
        emit layoutChanged();
    }));
    c.push_back(connect(model, &QObject::destroyed, this, [this, raw]() {
        // Too late to ask the model anything; its rows go by the last known count
        const int position = positionOfModel(raw);
        if (position < 0)
            return;
        Source &gone = m_sources[position];
        gone.connections.clear();
        if (gone.rows > 0) {
            const int offset = offsetOf(position);
            beginRemoveRows(QModelIndex(), offset, offset + gone.rows - 1);
            gone.rows = 0;
            gone.raw = nullptr;
            endRemoveRows();
        }
        gone.raw = nullptr;
        emit accountsChanged();
    }));
}

void UnifiedMsgListModel::disconnectSource(Source &source)
{
    for (const QMetaObject::Connection &connection : source.connections)
        disconnect(connection);
    source.connections.clear();
}

QModelIndex UnifiedMsgListModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() || row < 0 || column < 0 || row >= rowCount() || column >= columnCount())
        return QModelIndex();
    return createIndex(row, column);
}

QModelIndex UnifiedMsgListModel::parent(const QModelIndex &) const
{
    return QModelIndex();
}

int UnifiedMsgListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : messageCount();
}

int UnifiedMsgListModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : MsgListModel::COLUMN_COUNT;
}

bool UnifiedMsgListModel::hasChildren(const QModelIndex &parent) const
{
    return !parent.isValid() && rowCount() > 0;
}

QModelIndex UnifiedMsgListModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid() || proxyIndex.model() != this)
        return QModelIndex();
    const auto [position, row] = locate(proxyIndex.row());
    if (position < 0 || !m_sources[position].model)
        return QModelIndex();
    return m_sources[position].model->index(row, proxyIndex.column());
}

QModelIndex UnifiedMsgListModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    if (!sourceIndex.isValid() || sourceIndex.parent().isValid())
        return QModelIndex();
    const int position = positionOfModel(sourceIndex.model());
    if (position < 0 || sourceIndex.row() >= m_sources[position].rows)
        return QModelIndex();
    return index(offsetOf(position) + sourceIndex.row(), sourceIndex.column());
}

QString UnifiedMsgListModel::folderLabel(const QModelIndex &sourceIndex) const
{
    const QString mailbox = sourceIndex.data(RoleMailboxName).toString();
    if (mailbox.compare(QLatin1String("INBOX"), Qt::CaseInsensitive) == 0)
        return tr("Inbox");
    return mailbox;
}

QVariant UnifiedMsgListModel::data(const QModelIndex &proxyIndex, int role) const
{
    if (!proxyIndex.isValid() || proxyIndex.model() != this)
        return QVariant();
    const int position = locate(proxyIndex.row()).first;
    if (position < 0)
        return QVariant();
    const Source &source = m_sources[position];
    switch (role) {
    case RoleMessageAccountId:
        return source.accountId;
    case RoleMessageAccountName:
        return source.accountName;
    case RoleMessageRowLabel:
        if (!m_showsFolders)
            return source.accountName;
        [[fallthrough]];
    case RoleMessageSourceLabel: {
        const QString folder = folderLabel(mapToSource(proxyIndex));
        if (folder.isEmpty())
            return source.accountName;
        return tr("%1 \u00B7 %2", "account name, folder name of a message in the unified inbox")
            .arg(source.accountName, folder);
    }
    default:
        return mapToSource(proxyIndex).data(role);
    }
}

QVariant UnifiedMsgListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    for (const Source &source : m_sources) {
        if (source.model)
            return source.model->headerData(section, orientation, role);
    }
    return QAbstractItemModel::headerData(section, orientation, role);
}

Qt::ItemFlags UnifiedMsgListModel::flags(const QModelIndex &index) const
{
    const QModelIndex source = mapToSource(index);
    if (!source.isValid())
        return Qt::NoItemFlags;
    // A dropped message names its folder and UID but not its account, so another account could act on it
    return source.flags() & ~Qt::ItemIsDragEnabled;
}

QModelIndex UnifiedMsgListModel::sibling(int row, int column, const QModelIndex &idx) const
{
    return idx.isValid() ? index(row, column) : QModelIndex();
}

bool UnifiedMsgListModel::canFetchMore(const QModelIndex &) const
{
    return false;
}

void UnifiedMsgListModel::fetchMore(const QModelIndex &) { }

QHash<int, QByteArray> UnifiedMsgListModel::roleNames() const
{
    QHash<int, QByteArray> names;
    for (const Source &source : m_sources) {
        if (source.model) {
            names = source.model->roleNames();
            break;
        }
    }
    if (names.isEmpty())
        names = QAbstractItemModel::roleNames();
    names.insert(RoleMessageAccountId, QByteArrayLiteral("messageAccountId"));
    names.insert(RoleMessageAccountName, QByteArrayLiteral("messageAccountName"));
    names.insert(RoleMessageSourceLabel, QByteArrayLiteral("messageSourceLabel"));
    names.insert(RoleMessageRowLabel, QByteArrayLiteral("messageRowLabel"));
    return names;
}

QMimeData *UnifiedMsgListModel::mimeData(const QModelIndexList &) const
{
    return nullptr;
}

QStringList UnifiedMsgListModel::mimeTypes() const
{
    return QStringList();
}

Qt::DropActions UnifiedMsgListModel::supportedDragActions() const
{
    return Qt::IgnoreAction;
}

PrettyUnifiedMsgListModel::PrettyUnifiedMsgListModel(QObject *parent) : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void PrettyUnifiedMsgListModel::setFilters(const PrettyMsgListModel::Filters filters)
{
    if (filters != m_filters)
        refilter(filters);
}

void PrettyUnifiedMsgListModel::refilter(const PrettyMsgListModel::Filters filters)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    m_filters = filters;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    m_filters = filters;
    invalidateFilter();
#endif
}

PrettyMsgListModel::Filters PrettyUnifiedMsgListModel::filters() const
{
    return m_filters;
}

void PrettyUnifiedMsgListModel::setKeptVisibleIndex(const QModelIndex &index)
{
    QModelIndex source = index.model() == this ? mapToSource(index) : index;
    if (source.isValid() && source.model() != sourceModel())
        source = QModelIndex();
    if (source.isValid())
        source = source.sibling(source.row(), 0);
    if (source == m_keptVisible)
        return;
    const QPersistentModelIndex previous = m_keptVisible;
    m_keptVisible = source;
    // The message shown before may stop matching once it is no longer kept
    if (previous.isValid() && m_filters != PrettyMsgListModel::NoFilter
        && !PrettyMsgListModel::matchesFilters(previous, m_filters))
        refilter(m_filters);
}

int PrettyUnifiedMsgListModel::visibleMessageCount() const
{
    int count = 0;
    for (int row = 0, rows = rowCount(); row < rows; ++row) {
        if (index(row, 0).data(RoleIsFetched).isValid())
            ++count;
    }
    return count;
}

void PrettyUnifiedMsgListModel::setRelevanceTerms(const QStringList &terms, Qt::SortOrder order)
{
    m_relevanceTerms = terms;
    // Sorting again by the same column in the same order would be a no-op, so start from the source's order
    QSortFilterProxyModel::sort(-1, order);
    if (!terms.isEmpty())
        QSortFilterProxyModel::sort(0, order);
}

QStringList PrettyUnifiedMsgListModel::relevanceTerms() const
{
    return m_relevanceTerms;
}

bool PrettyUnifiedMsgListModel::isSortingByRelevance() const
{
    return !m_relevanceTerms.isEmpty();
}

QVariant PrettyUnifiedMsgListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this)
        return QVariant();
    const QModelIndex source = mapToSource(index);
    std::optional<QVariant> pretty = PrettyMsgListModel::presentationData(source, index.column(), role);
    if (role == Qt::ToolTipRole) {
        // Name the account and the folder in every column, since no column shows them
        const QString label = source.data(RoleMessageSourceLabel).toString();
        // The presented tooltips are escaped already
        QString text = pretty ? pretty->toString() : source.data(role).toString();
        if (label.isEmpty())
            return text;
        const QString where = UiUtils::Formatting::htmlEscaped(label);
        if (!pretty && !Qt::mightBeRichText(text))
            text = UiUtils::Formatting::htmlEscaped(text);
        return text.isEmpty() ? where : QStringLiteral("%1<br/><i>%2</i>").arg(text, where);
    }
    if (pretty)
        return *pretty;
    return QSortFilterProxyModel::data(index, role);
}

bool PrettyUnifiedMsgListModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_filters == PrettyMsgListModel::NoFilter)
        return true;
    const QModelIndex message = sourceModel()->index(sourceRow, 0, sourceParent);
    if (m_keptVisible.isValid() && m_keptVisible.row() == message.row() && m_keptVisible.model() == message.model())
        return true;
    return PrettyMsgListModel::matchesFilters(message, m_filters);
}

bool PrettyUnifiedMsgListModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    if (!m_relevanceTerms.isEmpty()) {
        const int leftScore = UiUtils::relevanceScore(left, m_relevanceTerms);
        const int rightScore = UiUtils::relevanceScore(right, m_relevanceTerms);
        if (leftScore != rightScore)
            return leftScore > rightScore;
        if (leftScore >= 0) {
            const QDateTime leftDate = left.data(RoleMessageDate).toDateTime();
            const QDateTime rightDate = right.data(RoleMessageDate).toDateTime();
            if (leftDate != rightDate)
                return leftDate > rightDate;
        }
        return left.row() < right.row();
    }
    auto byDate = [&left, &right](const int role) -> std::optional<bool> {
        const QDateTime a = left.data(role).toDateTime();
        const QDateTime b = right.data(role).toDateTime();
        if (a == b)
            return std::nullopt;
        return a < b;
    };
    std::optional<bool> result;
    switch (left.column()) {
    case MsgListModel::RECEIVED_DATE:
        result = byDate(RoleMessageInternalDate);
        break;
    case MsgListModel::SIZE: {
        const qulonglong a = left.data(RoleMessageSize).toULongLong();
        const qulonglong b = right.data(RoleMessageSize).toULongLong();
        if (a != b)
            result = a < b;
        break;
    }
    case MsgListModel::SUBJECT:
    case MsgListModel::FROM:
    case MsgListModel::TO:
    case MsgListModel::CC:
    case MsgListModel::BCC: {
        const auto text = [](const QModelIndex &index) {
            const auto pretty = PrettyMsgListModel::presentationData(index, index.column(), Qt::DisplayRole);
            return (pretty ? *pretty : index.data(Qt::DisplayRole)).toString();
        };
        const int compared = QString::localeAwareCompare(text(left), text(right));
        if (compared != 0)
            result = compared < 0;
        break;
    }
    default:
        break;
    }
    if (!result)
        result = byDate(RoleMessageDate);
    // Equal messages keep the order of their accounts and of their folders
    return result ? *result : left.row() < right.row();
}

} // namespace Mailbox

} // namespace Imap
