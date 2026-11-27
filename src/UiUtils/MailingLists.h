/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

class QAbstractItemModel;
class QModelIndex;
class QVariant;

namespace UiUtils {

/** @short The mailing list which a message came through

Lists are recognized by their List-Id header (RFC 2919). Lists which only send List-Post are recognized by its posting
address, so that older lists keep working.
*/
struct MailingList {
    /** The lower-case identifier of List-Id, such as "dev.lists.example.org" */
    QString id;
    /** The description of List-Id, such as "Developers"; may be empty */
    QString name;
    /** The lower-case address of the first mailto: URL of List-Post; may be empty */
    QString address;

    bool isValid() const { return !id.isEmpty() || !address.isEmpty(); }
    /** @short What a list: search looks for: the ID, or the posting address of lists without one */
    QString searchValue() const { return id.isEmpty() ? address : id; }
    /** @short The description, or the ID or address of lists without one */
    QString displayName() const;
    bool operator==(const MailingList &other) const = default;
};

namespace MailingLists {

/** @short The list named by a List-Id value, such as "Developers <dev.lists.example.org>"

The description may be quoted or encoded as in RFC 2047. A value without angle brackets is taken as the ID. The
address is left empty.
*/
MailingList parseListId(const QString &value);

/** @short The list named by a RoleMessageHeaderList value @arg listHeaders and a RoleMessageHeaderListPost value
@arg listPost */
MailingList fromHeaders(const QVariant &listHeaders, const QVariant &listPost);

/** @short The list of @arg message, or an invalid one when it has none or its headers are not loaded yet

Only rows whose RoleIsFetched is set are read, because asking an IMAP model for the headers would download the
message.
*/
MailingList fromMessage(const QModelIndex &message);

/** @short IMAP SEARCH keys finding the messages of the list with the ID or posting address @arg value

Both List-Id and List-Post are searched, as substrings, so that lists without a List-Id are found too.
*/
QStringList searchKeys(const QString &value);

/** @short An action which a list offers through its List-* headers */
enum class Action {
    Post,
    Subscribe,
    Unsubscribe,
    Help,
    Archive,
    Owner,
};

struct ActionTarget {
    Action action = Action::Help;
    QUrl url;
    bool operator==(const ActionTarget &other) const = default;
};

/** @short The URLs of an RFC 2369 header value, such as "<mailto:a@example.org> (comment), <https://example.org/b>"

Whitespace inside the angle brackets is ignored, as the RFC asks. Invalid URLs are left out.
*/
QList<QUrl> headerUrls(const QString &value);

/** @short The first URL of @arg urls which can be followed: mailto: with an address, http: or https:

RFC 2369 lists them in the list's order of preference.
*/
QUrl usableUrl(const QList<QUrl> &urls);

/** @short The actions of @arg message which have a usable URL, in the order of Action

Like fromMessage(), only rows whose RoleIsFetched is set are read. A List-Post of "NO" offers no Post action.
*/
QList<ActionTarget> actions(const QModelIndex &message);

/** @short How many loaded messages of a list a model holds */
struct Found {
    MailingList list;
    int messages = 0;
    bool operator==(const Found &other) const = default;
};

/** @short The lists of the loaded messages of @arg model, sorted by displayName(), ignoring case

Rows below other rows, such as replies in threads, are included. Messages of the same list are counted once per
searchValue().
*/
QList<Found> listsIn(const QAbstractItemModel *model);

}

}
