/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "PrettyMsgListModel.h"
#include "ItemRoles.h"
#include "MsgListModel.h"
#include "ThreadingMsgListModel.h"
#include "UiUtils/Formatting.h"
#include "UiUtils/IconLoader.h"
#include "UiUtils/Relevance.h"


namespace Imap
{

namespace Mailbox
{

PrettyMsgListModel::PrettyMsgListModel(QObject *parent):
    QSortFilterProxyModel(parent), m_filters(NoFilter)
{
    setDynamicSortFilter(true);
    // Another mailbox reuses the UIDs
    connect(this, &QAbstractItemModel::modelAboutToBeReset, this, [this]() { m_relevanceCache.clear(); });
}

QVariant PrettyMsgListModel::data(const QModelIndex &index, int role) const
{
    if (! index.isValid() || index.model() != this)
        return QVariant();

    if (index.column() < 0 || index.column() >= columnCount(index.parent()))
        return QVariant();

    if (index.row() < 0 || index.row() >= rowCount(index.parent()))
        return QVariant();

    QModelIndex translated = mapToSource(index);

    switch (role) {

    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        switch (index.column()) {
        case MsgListModel::TO:
        case MsgListModel::FROM:
        case MsgListModel::CC:
        case MsgListModel::BCC:
        {
            int backendRole = 0;
            switch (index.column()) {
            case MsgListModel::FROM:
                backendRole = RoleMessageFrom;
                break;
            case MsgListModel::TO:
                backendRole = RoleMessageTo;
                break;
            case MsgListModel::CC:
                backendRole = RoleMessageCc;
                break;
            case MsgListModel::BCC:
                backendRole = RoleMessageBcc;
                break;
            }
            QVariantList items = translated.data(backendRole).toList();
            if (role == Qt::DisplayRole) {
                return Imap::Message::MailAddress::prettyList(items, Imap::Message::MailAddress::FORMAT_JUST_NAME);
            } else {
                return UiUtils::Formatting::htmlEscaped(Imap::Message::MailAddress::prettyList(items, Imap::Message::MailAddress::FORMAT_READABLE));
            }
        }
        case MsgListModel::DATE:
        case MsgListModel::RECEIVED_DATE:
        {
            QDateTime res = translated.data(RoleMessageDate).toDateTime();
            if (role == Qt::ToolTipRole) {
                // tooltips shall always show the full and complete data
                return QLocale().toString(res.toLocalTime(), QLocale::LongFormat);
            }
            return UiUtils::Formatting::prettyDate(res.toLocalTime());
        }
        case MsgListModel::SIZE:
        {
            QVariant size = translated.data(RoleMessageSize);
            if (!size.isValid()) {
                return QVariant();
            }
            return UiUtils::Formatting::prettySize(size.toULongLong());
        }
        case MsgListModel::SUBJECT:
        {
            if (!translated.data(RoleIsFetched).toBool())
                return tr("Loading...");
            QString subject = translated.data(RoleMessageSubject).toString();
            if (role == Qt::ToolTipRole) {
                subject = UiUtils::Formatting::htmlEscaped(subject);
            }
            return subject.isEmpty() ? tr("(no subject)") : subject;
        }
        }
        break;

    case Qt::AccessibleTextRole:
        // The indicator columns show only icons, so give screen readers their meaning
        switch (index.column()) {
        case MsgListModel::SEEN:
            if (!translated.data(RoleIsFetched).toBool())
                return QVariant();
            return translated.data(RoleMessageIsMarkedRead).toBool() ? tr("Read") : tr("Unread");
        case MsgListModel::FLAGGED:
            if (!translated.data(RoleIsFetched).toBool())
                return QVariant();
            return translated.data(RoleMessageIsMarkedFlagged).toBool() ? tr("Flagged") : tr("Not flagged");
        case MsgListModel::ATTACHMENT:
            return translated.data(RoleMessageHasAttachments).toBool() ? tr("Has attachments") : QVariant();
        }
        break;


    case Qt::TextAlignmentRole:
        switch (index.column()) {
        case MsgListModel::DATE:
        case MsgListModel::RECEIVED_DATE:
        case MsgListModel::SIZE:
            return int(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        } // we return int because QVariant cannot hold QFlag: https://bugreports.qt.io/browse/QTBUG-54210

    case Qt::DecorationRole:
        // We will need the data, but asking for Flags or IsMarkedXYZ doesn't cause a fetch
        translated.data(RoleMessageSubject);

        switch (index.column()) {
        case MsgListModel::SUBJECT:
        {
            if (! translated.data(RoleIsFetched).toBool())
                return QVariant();

            bool isForwarded = translated.data(RoleMessageIsMarkedForwarded).toBool();
            bool isReplied = translated.data(RoleMessageIsMarkedReplied).toBool();

            if (isForwarded && isReplied)
                return UiUtils::loadIcon(QStringLiteral("mail-forwarded-replied"));
            else if (isReplied)
                return UiUtils::loadIcon(QStringLiteral("mail-replied"));
            else if (isForwarded)
                return UiUtils::loadIcon(QStringLiteral("mail-forwarded"));
            else if (translated.data(RoleMessageIsMarkedRecent).toBool())
                return UiUtils::loadIcon(QStringLiteral("mail-mark-unread-new"));
            else if (!translated.data(RoleMessageIsMarkedRead).toBool())
                return UiUtils::loadIcon(QStringLiteral("mail-mark-unread"));
            else
                return UiUtils::loadIcon(QStringLiteral("mail-mark-read"));
        }
        case MsgListModel::SEEN:
            if (! translated.data(RoleIsFetched).toBool())
                return QVariant();
            break;
        case MsgListModel::FLAGGED:
            if (! translated.data(RoleIsFetched).toBool())
                return QVariant();
            if (translated.data(RoleMessageIsMarkedFlagged).toBool())
                return UiUtils::loadIcon(QStringLiteral("mail-flagged"));
            else
                return UiUtils::loadIcon(QStringLiteral("mail-unflagged"));
        case MsgListModel::ATTACHMENT:
            if (translated.data(RoleMessageHasAttachments).toBool())
                return UiUtils::loadIcon(QStringLiteral("mail-attachment"));
            else
                return QVariant();
        default:
            return QVariant();
        }

    }

    return QSortFilterProxyModel::data(index, role);
}

void PrettyMsgListModel::setHideRead(bool value)
{
    setFilterEnabled(FilterUnread, value);
}

bool PrettyMsgListModel::hideRead() const
{
    return m_filters.testFlag(FilterUnread);
}

PrettyMsgListModel::Filters PrettyMsgListModel::filters() const
{
    return m_filters;
}

void PrettyMsgListModel::setFilters(Filters filters)
{
    if (filters == m_filters)
        return;
    refilter(filters);
    emit filtersChanged(m_filters);
}

void PrettyMsgListModel::setFilterEnabled(Filter filter, bool enabled)
{
    setFilters(enabled ? m_filters | filter : m_filters & ~Filters(filter));
}

void PrettyMsgListModel::refilter(Filters filters)
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

void PrettyMsgListModel::setKeptVisibleIndex(const QModelIndex &index)
{
    QModelIndex source = index.model() == this ? mapToSource(index) : index;
    if (source.isValid() && source.model() != sourceModel())
        source = QModelIndex();
    if (source.isValid())
        source = source.sibling(source.row(), 0);
    if (m_keptVisible == source)
        return;

    const QModelIndex previous = m_keptVisible;
    m_keptVisible = source;
    if (m_filters == NoFilter || !previous.isValid())
        return;

    // The previous message might have been the only reason for showing its thread
    QModelIndex top = previous;
    while (top.parent().isValid())
        top = top.parent();
    if (!subtreeMatches(top))
        refilter(m_filters);
}

QModelIndex PrettyMsgListModel::keptVisibleIndex() const
{
    return m_keptVisible;
}

bool PrettyMsgListModel::messageMatches(const QModelIndex &sourceIndex) const
{
    const QVariant fetched = sourceIndex.data(RoleIsFetched);
    if (!fetched.isValid()) {
        // A placeholder for a message which is missing from a thread
        return false;
    }
    // Neither of these roles fetches anything, and messages with unknown flags count as unread and not flagged
    if (m_filters.testFlag(FilterUnread) && !sourceIndex.data(RoleMessageWasUnread).toBool()
            && sourceIndex.data(RoleMessageIsMarkedRead).toBool())
        return false;
    if (m_filters.testFlag(FilterFlagged) && !sourceIndex.data(RoleMessageIsMarkedFlagged).toBool())
        return false;
    // Asking about attachments would fetch the message, and before that it says "no attachments"
    if (m_filters.testFlag(FilterAttachments) && fetched.toBool() && !sourceIndex.data(RoleMessageHasAttachments).toBool())
        return false;
    return true;
}

bool PrettyMsgListModel::subtreeMatches(const QModelIndex &sourceIndex) const
{
    QModelIndexList pending{sourceIndex};
    while (!pending.isEmpty()) {
        const QModelIndex item = pending.takeLast();
        if (m_keptVisible == item || messageMatches(item))
            return true;
        for (int row = 0, rows = sourceModel()->rowCount(item); row < rows; ++row)
            pending.append(sourceModel()->index(row, 0, item));
    }
    return false;
}

int PrettyMsgListModel::visibleMessageCount() const
{
    if (!sourceModel())
        return 0;
    int count = 0;
    QModelIndexList pending;
    for (int row = 0, rows = rowCount(); row < rows; ++row)
        pending.append(mapToSource(index(row, 0)));
    // Within a shown thread, all messages are shown
    while (!pending.isEmpty()) {
        const QModelIndex item = pending.takeLast();
        if (item.data(RoleIsFetched).isValid())
            ++count;
        for (int row = 0, rows = sourceModel()->rowCount(item); row < rows; ++row)
            pending.append(sourceModel()->index(row, 0, item));
    }
    return count;
}

bool PrettyMsgListModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (m_filters == NoFilter)
        return true;

    // A thread is shown as a whole once its root has been accepted
    if (source_parent.isValid())
        return true;

    return subtreeMatches(sourceModel()->index(source_row, 0, source_parent));
}


void PrettyMsgListModel::sort(int column, Qt::SortOrder order)
{
    ThreadingMsgListModel *threadingModel = qobject_cast<ThreadingMsgListModel*>(sourceModel());
    Q_ASSERT(threadingModel);

    ThreadingMsgListModel::SortCriterium criterium = ThreadingMsgListModel::SortCriterium::SORT_NONE;
    switch (column) {
    case MsgListModel::SEEN:
    case MsgListModel::FLAGGED:
    case MsgListModel::ATTACHMENT:
    case MsgListModel::COLUMN_COUNT:
    case MsgListModel::BCC:
    case -1:
        criterium = ThreadingMsgListModel::SortCriterium::SORT_NONE;
        break;
    case MsgListModel::SUBJECT:
        criterium = ThreadingMsgListModel::SortCriterium::SORT_SUBJECT;
        break;
    case MsgListModel::FROM:
        criterium = ThreadingMsgListModel::SortCriterium::SORT_FROM;
        break;
    case MsgListModel::TO:
        criterium = ThreadingMsgListModel::SortCriterium::SORT_TO;
        break;
    case MsgListModel::CC:
        criterium = ThreadingMsgListModel::SortCriterium::SORT_CC;
        break;
    case MsgListModel::DATE:
        criterium = ThreadingMsgListModel::SortCriterium::SORT_DATE;
        break;
    case MsgListModel::RECEIVED_DATE:
        criterium = ThreadingMsgListModel::SortCriterium::SORT_ARRIVAL;
        break;
    case MsgListModel::SIZE:
        criterium = ThreadingMsgListModel::SortCriterium::SORT_SIZE;
        break;
    }

    bool willSort = threadingModel->setUserSearchingSortingPreference(threadingModel->currentSearchCondition(), criterium, order);

    // Now let the view know about whether we accept such a sorting criteria.
    // This is needed because the QHeaderView doesn't offer a way to say "hey, you cannot sort in columns XYZ, only on ABC".
    if (criterium != ThreadingMsgListModel::SortCriterium::SORT_NONE && willSort)
        emit sortingPreferenceChanged(column, order);
    else
        emit sortingPreferenceChanged(-1, order);
}

void PrettyMsgListModel::setRelevanceTerms(const QStringList &terms, Qt::SortOrder order)
{
    m_relevanceTerms = terms;
    m_relevanceCache.clear();
    // Sorting again by the same column in the same order would be a no-op, so start from the source's order
    QSortFilterProxyModel::sort(-1, order);
    if (!terms.isEmpty())
        QSortFilterProxyModel::sort(0, order);
}

QStringList PrettyMsgListModel::relevanceTerms() const
{
    return m_relevanceTerms;
}

bool PrettyMsgListModel::isSortingByRelevance() const
{
    return !m_relevanceTerms.isEmpty();
}

int PrettyMsgListModel::relevance(const QModelIndex &sourceIndex) const
{
    const uint uid = sourceIndex.data(RoleMessageUid).toUInt();
    if (uid) {
        const auto it = m_relevanceCache.constFind(uid);
        if (it != m_relevanceCache.cend())
            return *it;
    }
    const int score = UiUtils::relevanceScore(sourceIndex, m_relevanceTerms);
    // A message which is not loaded yet gets its score once it is
    if (uid && score >= 0)
        m_relevanceCache.insert(uid, score);
    return score;
}

bool PrettyMsgListModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    if (m_relevanceTerms.isEmpty())
        return QSortFilterProxyModel::lessThan(left, right);
    const int leftScore = relevance(left);
    const int rightScore = relevance(right);
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

}

}
