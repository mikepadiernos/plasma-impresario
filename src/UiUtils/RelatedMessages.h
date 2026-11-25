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

/** @short Subject without reply and forward prefixes, folded for comparison

"Re: Fwd: AW: Hello  world" and "hello world" give the same result. An empty
result means that the subject cannot be used to find a conversation.
*/
QString normalizedSubject(const QString &subject);

/** @short Lower-case "mailbox@host" of the first address in a RoleMessageFrom-style list, or an empty string */
QString firstAddress(const QVariant &addresses);

struct RelatedMessages {
    /** Messages sharing the normalized subject or linked through Message-ID and In-Reply-To */
    QList<QPersistentModelIndex> conversation;
    /** Other messages from the same sender */
    QList<QPersistentModelIndex> sameSender;
    /** Whether a group was cut at its limit or not every row was scanned */
    bool truncated = false;
};

/** @short Find messages related to @arg current among the rows of a message list

@arg messages is a flat message list such as Imap::Mailbox::MsgListModel. Only rows
whose data was already fetched are considered, because asking for the envelope of
any other row would make the IMAP model download it. Rows are scanned from the last
(newest arrival) backwards, so each group starts with the most recent message.
@arg current must be an index whose envelope is fetched; rows that deproxify to it
are skipped.
*/
RelatedMessages collectRelatedMessages(const QAbstractItemModel *messages, const QModelIndex &current, int maximumPerGroup = 25,
                                       int maximumScannedRows = 20000);

/** @short The whole conversation of @arg current among the loaded rows of a message list

Unlike collectRelatedMessages(), which lists the messages directly related to @arg current, this follows the links
from message to message: a row joins when it shares the normalized subject of a message already in the conversation,
replies to one, or is replied to by one. Every message of the result therefore gives the same conversation. The result
does not contain @arg current and holds at most @arg maximum rows; the other arguments are those of
collectRelatedMessages().
*/
QList<QPersistentModelIndex> collectConversation(const QAbstractItemModel *messages, const QModelIndex &current, int maximum = 1000,
                                                 int maximumScannedRows = 20000);

/** @short The message of the conversation of @arg current which comes after (or before) it

@arg view is the shown message list and @arg current one of its rows. With @arg threaded, the conversation is the
thread of @arg current: its top-level message and every reply below it, whether expanded or not. Otherwise it is
@arg current and the rows of @arg view which are the same messages as @arg related, for example the conversation
found by collectRelatedMessages(). Either way the messages go from the oldest to the newest, whatever the sorting of
the list, and messages of the same date keep their list order. Group headers and rows without a UID, such as a
thread's missing parent, are skipped. There is no wrap-around.
*/
QModelIndex adjacentInConversation(const QAbstractItemModel *view, const QModelIndex &current, bool forward, bool threaded,
                                   const QList<QPersistentModelIndex> &related);

}
