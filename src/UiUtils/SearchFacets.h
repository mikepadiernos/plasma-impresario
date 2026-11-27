/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDate>
#include <QList>
#include <QModelIndexList>
#include <QString>

/** @short Facets which narrow the results of a search: account, folder, sender, date, attachments and tags

Each message of a list has values for each kind of facet, read through the roles of Imap::Mailbox from a row of any
message list model. Facets are chosen among the values of the messages shown, and a message is kept when it has one
of the chosen values of every kind which has any: values of one kind are alternatives, kinds are combined.

The sender, the date and the attachments are known only once a message is loaded, since asking for them would
download it. Until then a message has no value of these kinds, is not counted for them, and is not kept by a facet
of these kinds. The flags of a message are used for its tags as far as they are known.
*/
namespace UiUtils {

class SearchFacets
{
public:
    enum class Kind {
        Account,
        Folder,
        Sender,
        Date,
        Attachments,
        Tag,
    };

    struct Value {
        Kind kind = Kind::Account;
        /** Tells values of one kind apart; not shown */
        QString key;
        /** The value as the user sees it */
        QString label;

        bool operator==(const Value &other) const { return kind == other.kind && key == other.key; }
        bool operator!=(const Value &other) const { return !(*this == other); }
    };

    struct Count {
        Value value;
        int count = 0;
    };

    /** @short What the values of a message depend on beyond the message itself */
    struct Context {
        /** The account of messages which do not name their own, as in the list of one folder */
        QString accountId;
        QString accountName;
        /** The day which the dates are relative to; an invalid date means today */
        QDate today;
    };

    /** @short Every kind, in the order in which they are offered */
    static QList<Kind> kinds();
    /** @short Translated name of @p kind, such as "Sender" */
    static QString kindName(Kind kind);
    /** @short The values of @p kind which the message @p message has; none when they are not known */
    static QList<Value> valuesOf(const QModelIndex &message, Kind kind, const Context &context);

    void setContext(const Context &context);
    Context context() const;

    /** @short The chosen values, in the order in which they were chosen */
    QList<Value> chosen() const;
    bool isEmpty() const;
    bool contains(const Value &value) const;
    /** @short Choose @p value, or stop choosing it when it is chosen */
    void toggle(const Value &value);
    void remove(const Value &value);
    void clear();

    /** @short Whether @p message has a chosen value of every kind which has any */
    bool matches(const QModelIndex &message) const;
    /** @short How many of @p messages have each value of @p kind, among those which the facets of the other kinds keep

    Chosen values of @p kind are counted even when no message has them. The values of dates and attachments are in
    their natural order, newest first; the others have the most messages first.
    */
    QList<Count> counts(const QModelIndexList &messages, Kind kind) const;

    /** @short The same chosen values, whatever the context */
    bool operator==(const SearchFacets &other) const { return m_chosen == other.m_chosen; }
    bool operator!=(const SearchFacets &other) const { return !(*this == other); }

private:
    bool matchesExcept(const QModelIndex &message, Kind ignored, bool ignoring) const;

    Context m_context;
    QList<Value> m_chosen;
};

}
