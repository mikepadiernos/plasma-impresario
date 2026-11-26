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
#ifndef PRETTYMSGLISTMODEL_H
#define PRETTYMSGLISTMODEL_H

#include <QHash>
#include <QPersistentModelIndex>
#include <QSortFilterProxyModel>
#include <QStringList>
#include <optional>
#include "Imap/Model/MailboxModel.h"
#include "Imap/Model/FavoriteTagsModel.h"

namespace Imap
{

namespace Mailbox
{

/** @short A pretty proxy model which increases sexiness of the (Threaded)MsgListModel

The model also provides the quick filters of the message list. Active filters are combined, so that a message has to
match all of them. A message whose state is not known yet is treated so that it changes its visibility at most once
when the data arrive:

- Unread: a message without known flags counts as unread, so it is shown. A message which was unread when the
  mailbox was opened stays shown after it has been read, until another mailbox is opened.
- Flagged: a message without known flags is not flagged, so it is hidden until its flags say otherwise.
- With attachments: a message whose structure has not been fetched yet is shown.

When the messages are shown in threads, a thread is shown as a whole when any of its messages matches.

The message set by setKeptVisibleIndex() stays shown even when it stops matching, for example when reading it clears
its unread state, until another message takes its place. A message which the filters hide is shown once it is set,
so that going through a conversation can reach its other messages.

While relevance terms are set, the model sorts the messages itself by UiUtils::relevanceScore(), the best matches first
in ascending order; equally relevant messages are sorted newest first. Otherwise the order of the source is kept.
*/
class PrettyMsgListModel: public QSortFilterProxyModel
{
    Q_OBJECT
public:
    enum Filter {
        NoFilter = 0,
        FilterUnread = 1 << 0,
        FilterFlagged = 1 << 1,
        FilterAttachments = 1 << 2,
    };
    Q_DECLARE_FLAGS(Filters, Filter)

    explicit PrettyMsgListModel(QObject *parent);
    QVariant data(const QModelIndex &index, int role) const override;
    /** @short How the list shows @p column of the message @p message for @p role; nothing when the message's own data applies */
    static std::optional<QVariant> presentationData(const QModelIndex &message, int column, int role);
    /** @short Whether the message @p message passes the quick @p filters */
    static bool matchesFilters(const QModelIndex &message, Filters filters);
    /** @short Show only unread messages; the same as the FilterUnread quick filter */
    void setHideRead(bool value);
    bool hideRead() const;
    Filters filters() const;
    void setFilters(Filters filters);
    void setFilterEnabled(Filter filter, bool enabled);
    /** @short Keep @p index shown even if it stops matching the filters; accepts an index of this or the source model */
    void setKeptVisibleIndex(const QModelIndex &index);
    /** @short The message kept visible, as an index of the source model */
    QModelIndex keptVisibleIndex() const;
    /** @short Number of messages shown, including those in collapsed threads but not placeholders of missing ones */
    int visibleMessageCount() const;
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    void sort(int column, Qt::SortOrder order) override;
    /** @short Sort by relevance for @p terms (see UiUtils::relevanceTerms()); no terms keep the order of the source */
    void setRelevanceTerms(const QStringList &terms, Qt::SortOrder order);
    QStringList relevanceTerms() const;
    bool isSortingByRelevance() const;

signals:
    void sortingPreferenceChanged(int column, Qt::SortOrder order);
    void filtersChanged(Imap::Mailbox::PrettyMsgListModel::Filters filters);

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    int relevance(const QModelIndex &sourceIndex) const;
    bool messageMatches(const QModelIndex &sourceIndex) const;
    bool subtreeMatches(const QModelIndex &sourceIndex) const;
    void refilter(Filters filters);

    Filters m_filters;
    QPersistentModelIndex m_keptVisible;
    QStringList m_relevanceTerms;
    /** Scores of loaded messages by UID; their envelope does not change */
    mutable QHash<uint, int> m_relevanceCache;
};

}

}

Q_DECLARE_OPERATORS_FOR_FLAGS(Imap::Mailbox::PrettyMsgListModel::Filters)

#endif // PRETTYMSGLISTMODEL_H
