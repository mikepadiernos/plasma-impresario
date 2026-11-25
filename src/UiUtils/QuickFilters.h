/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QModelIndexList>
#include <QString>
#include <QStringList>
#include <functional>

class QAbstractItemModel;
class QVariant;

/** @short Helpers for The Bat!-style fast processing of the message list

These work on the rows of a message list model through the roles of
Imap::Mailbox, so they can be tested with any QAbstractItemModel. Envelope data
is only read from rows whose RoleIsFetched is set, because asking an IMAP model
for it would download the message.
*/
namespace UiUtils {

/** @short Subject for searching related messages, without reply and forward
prefixes or list tags

"Re: [dev] AW: Release plan" gives "Release plan". The case is kept, since the
result is shown to the user; IMAP SUBJECT searches ignore it anyway. A subject
which is only a tag keeps it.
*/
QString searchableSubject(const QString &subject);

/** @short A single field which the quick search can be limited to */
enum class FieldFilter {
  None,
  From,
  To,
  Subject,
  MailingList,
};

struct ParsedFieldFilter {
  FieldFilter field = FieldFilter::None;
  QString value;
};

/** @short Quick search text for a field filter, such as "from:ann@example.org"
or "subject:Release plan"

The value is quoted only when it would otherwise lose leading or trailing spaces
or quotes.
*/
QString fieldFilterText(FieldFilter field, const QString &value);

/** @short Recognize a "from:", "to:", "subject:" or "list:" filter typed into
the quick search

The keyword is case-insensitive and not translated. Anything else, including a
keyword without a value, gives FieldFilter::None.
*/
ParsedFieldFilter parseFieldFilter(const QString &text);

/** @short IMAP SEARCH keys for a field filter, or an empty list for
 * FieldFilter::None */
QStringList fieldFilterConditions(const ParsedFieldFilter &filter);

/** @short Lower-case address of the first mailto: URL in a
 * RoleMessageHeaderListPost value, or an empty string */
QString listPostAddress(const QVariant &listPost);

/** @short Value which a field filter for @arg message would search for, or an
 * empty string if it is not known yet */
QString fieldFilterValue(const QModelIndex &message, FieldFilter field);

/** @short Ways in which Select Similar picks messages */
enum class SimilarMessages {
  FromSender,
  WithSubject,
  Unread,
  Flagged,
};

/** @short Rows with a message UID for which @arg matches returns true, depth
 * first in display order */
QModelIndexList
findMatchingMessages(const QAbstractItemModel *model,
                     const std::function<bool(const QModelIndex &)> &matches);

/** @short Rows of @arg model which are similar to @arg reference according to
@arg criterion

FromSender and WithSubject need a fetched @arg reference and only consider
fetched rows; Unread and Flagged ignore @arg reference and use the flags of
every row.
*/
QModelIndexList findSimilarMessages(const QAbstractItemModel *model,
                                    const QModelIndex &reference,
                                    SimilarMessages criterion);

/** @short The message row after (or before) @arg current in display order,
skipping rows without a UID

Children are only visited when @arg isExpanded returns true for their parent,
like the rows of a tree view. An invalid @arg current starts at the first (or
last) row. There is no wrap-around.
*/
QModelIndex
adjacentMessage(const QAbstractItemModel *model, const QModelIndex &current,
                bool forward,
                const std::function<bool(const QModelIndex &)> &isExpanded);

} // namespace UiUtils
