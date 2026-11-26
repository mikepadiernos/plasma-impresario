/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QPersistentModelIndex>
#include <QString>

class QAbstractItemModel;
class QVariant;

namespace UiUtils {

/** @short Subject without reply and forward prefixes or list tags, folded for comparison

"Re: [team] AW: Hello  world", "Re[2]: hello world" and "hello world" give the same result (see strippedSubject()). A
subject which is only a list tag keeps it. An empty result means that the subject cannot be used to find a
conversation.
*/
QString normalizedSubject(const QString &subject);

/** @short Lower-case "mailbox@host" of the first address in a RoleMessageFrom-style list, or an empty string */
QString firstAddress(const QVariant &addresses);

struct RelatedMessages {
    /** Messages of the same conversation, as grouped by groupConversations() */
    QList<QPersistentModelIndex> conversation;
    /** Other messages from the same sender */
    QList<QPersistentModelIndex> sameSender;
    /** Whether a group was cut at its limit or not every row was scanned */
    bool truncated = false;
};

/** @short Find messages related to @arg current among the rows of a message list

@arg messages is a flat message list such as Imap::Mailbox::MsgListModel. Only rows
whose data was already fetched are considered, because asking for the envelope of
any other row would make the IMAP model download it. Each group starts with the last row
(newest arrival). @arg current must be an index whose envelope is fetched; rows that
deproxify to it are skipped. Only the last @arg maximumScannedRows rows are scanned.
*/
RelatedMessages collectRelatedMessages(const QAbstractItemModel *messages, const QModelIndex &current, int maximumPerGroup = 25,
                                       int maximumScannedRows = 20000);

/** @short The whole conversation of @arg current among the loaded rows of a message list

The conversation follows groupConversations(), so every message of the result gives the same conversation. The result
starts with the last row, does not contain @arg current and holds at most @arg maximum rows; the other arguments are
those of collectRelatedMessages().
*/
QList<QPersistentModelIndex> collectConversation(const QAbstractItemModel *messages, const QModelIndex &current, int maximum = 1000,
                                                 int maximumScannedRows = 20000);

/** @short The message of the thread of @arg current which comes after (or before) it

@arg view is the shown, threaded message list and @arg current one of its rows. The thread is its top-level message
and every reply below it, whether expanded or not; a group header above it is not part of it. The messages go from
the oldest to the newest, whatever the sorting of the list, and messages of the same date keep their list order. Group
headers and rows without a UID, such as a thread's missing first message, are skipped. There is no wrap-around.
*/
QModelIndex adjacentInThread(const QAbstractItemModel *view, const QModelIndex &current, bool forward);

/** @short The message of @arg conversation which comes after (or before) @arg current by date

@arg conversation holds the other messages of the conversation, for example from collectConversation(), as indexes of
any model above the IMAP model. The messages go from the oldest to the newest; messages of the same date keep the order
of @arg conversation, with @arg current first. Messages without a UID are skipped and there is no wrap-around. The
result is an index of the IMAP model (see Imap::deproxifiedIndex()), so it can be a message which a filtered list
does not show.
*/
QModelIndex adjacentByDate(const QModelIndex &current, bool forward, const QList<QPersistentModelIndex> &conversation);

}
