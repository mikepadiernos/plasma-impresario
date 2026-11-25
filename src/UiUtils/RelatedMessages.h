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

}
