/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "MailingLists.h"

#include <QAbstractItemModel>
#include <QHash>
#include <QVariant>

#include <algorithm>

#include "Imap/Encoders.h"
#include "Imap/Model/ItemRoles.h"
#include "QuickFilters.h"

namespace UiUtils {

QString MailingList::displayName() const
{
    if (!name.isEmpty())
        return name;
    return searchValue();
}

namespace MailingLists {

namespace {

QString withoutSpaces(QString text)
{
    text.remove(QLatin1Char(' '));
    text.remove(QLatin1Char('\t'));
    return text;
}

QString unquoted(const QString &phrase)
{
    if (phrase.size() < 2 || !phrase.startsWith(QLatin1Char('"')) || !phrase.endsWith(QLatin1Char('"')))
        return phrase;
    QString result;
    const QString inner = phrase.mid(1, phrase.size() - 2);
    for (qsizetype i = 0; i < inner.size(); ++i) {
        if (inner.at(i) == QLatin1Char('\\') && i + 1 < inner.size())
            ++i;
        result += inner.at(i);
    }
    return result;
}

QString listHeader(const QModelIndex &message, const char *name)
{
    return message.data(Imap::Mailbox::RoleMessageHeaderList).toMap().value(QLatin1String(name)).toString();
}

bool isFetched(const QModelIndex &message)
{
    return message.isValid() && message.data(Imap::Mailbox::RoleIsFetched).toBool();
}

}

MailingList parseListId(const QString &value)
{
    MailingList list;
    const QString text = value.trimmed();
    const qsizetype open = text.lastIndexOf(QLatin1Char('<'));
    const qsizetype close = open < 0 ? -1 : text.indexOf(QLatin1Char('>'), open);
    if (close > open) {
        list.id = withoutSpaces(text.mid(open + 1, close - open - 1)).toLower();
        const QString phrase = text.left(open).trimmed();
        list.name = unquoted(Imap::decodeRFC2047String(phrase.toUtf8()).trimmed()).trimmed();
    } else if (!text.contains(QLatin1Char(' '))) {
        list.id = text.toLower();
    }
    return list;
}

MailingList fromHeaders(const QVariant &listHeaders, const QVariant &listPost)
{
    MailingList list = parseListId(listHeaders.toMap().value(QStringLiteral("list-id")).toString());
    list.address = listPostAddress(listPost);
    return list;
}

MailingList fromMessage(const QModelIndex &message)
{
    if (!isFetched(message))
        return MailingList();
    return fromHeaders(message.data(Imap::Mailbox::RoleMessageHeaderList),
                       message.data(Imap::Mailbox::RoleMessageHeaderListPost));
}

QStringList searchKeys(const QString &value)
{
    return {QStringLiteral("OR"), QStringLiteral("HEADER"), QStringLiteral("List-Id"), value,
            QStringLiteral("HEADER"), QStringLiteral("List-Post"), value};
}

QList<QUrl> headerUrls(const QString &value)
{
    QList<QUrl> urls;
    qsizetype depth = 0;
    qsizetype start = -1;
    for (qsizetype i = 0; i < value.size(); ++i) {
        const QChar c = value.at(i);
        // Comments can hold angle brackets which are no URL
        if (start < 0 && c == QLatin1Char('(')) {
            ++depth;
        } else if (start < 0 && c == QLatin1Char(')') && depth > 0) {
            --depth;
        } else if (depth == 0 && start < 0 && c == QLatin1Char('<')) {
            start = i + 1;
        } else if (start >= 0 && c == QLatin1Char('>')) {
            const QUrl url(withoutSpaces(value.mid(start, i - start)), QUrl::StrictMode);
            if (url.isValid() && !url.scheme().isEmpty())
                urls << url;
            start = -1;
        }
    }
    return urls;
}

QUrl usableUrl(const QList<QUrl> &urls)
{
    for (const QUrl &url : urls) {
        const QString scheme = url.scheme().toLower();
        if (scheme == QLatin1String("mailto") && !url.path().isEmpty())
            return url;
        if ((scheme == QLatin1String("https") || scheme == QLatin1String("http")) && !url.host().isEmpty())
            return url;
    }
    return QUrl();
}

QList<ActionTarget> actions(const QModelIndex &message)
{
    QList<ActionTarget> result;
    if (!isFetched(message))
        return result;
    const auto add = [&result](Action action, const QList<QUrl> &urls) {
        const QUrl url = usableUrl(urls);
        if (url.isValid())
            result << ActionTarget{action, url};
    };
    if (!message.data(Imap::Mailbox::RoleMessageHeaderListPostNo).toBool()) {
        QList<QUrl> post;
        for (const QVariant &item : message.data(Imap::Mailbox::RoleMessageHeaderListPost).toList())
            post << item.toUrl();
        add(Action::Post, post);
    }
    add(Action::Subscribe, headerUrls(listHeader(message, "list-subscribe")));
    add(Action::Unsubscribe, headerUrls(listHeader(message, "list-unsubscribe")));
    add(Action::Help, headerUrls(listHeader(message, "list-help")));
    add(Action::Archive, headerUrls(listHeader(message, "list-archive")));
    add(Action::Owner, headerUrls(listHeader(message, "list-owner")));
    return result;
}

QList<Found> listsIn(const QAbstractItemModel *model)
{
    QList<Found> result;
    if (!model)
        return result;
    QHash<QString, qsizetype> positions;
    QModelIndexList pending;
    for (int row = model->rowCount() - 1; row >= 0; --row)
        pending.append(model->index(row, 0));
    while (!pending.isEmpty()) {
        const QModelIndex index = pending.takeLast();
        for (int row = model->rowCount(index) - 1; row >= 0; --row)
            pending.append(model->index(row, 0, index));
        if (index.data(Imap::Mailbox::RoleMessageUid).toUInt() == 0)
            continue;
        const MailingList list = fromMessage(index);
        if (!list.isValid())
            continue;
        const auto known = positions.constFind(list.searchValue());
        if (known != positions.constEnd()) {
            Found &found = result[*known];
            ++found.messages;
            // A later message may carry the description which an earlier one lacked
            if (found.list.name.isEmpty())
                found.list.name = list.name;
            if (found.list.address.isEmpty())
                found.list.address = list.address;
            continue;
        }
        positions.insert(list.searchValue(), result.size());
        result << Found{list, 1};
    }
    std::stable_sort(result.begin(), result.end(), [](const Found &left, const Found &right) {
        const int byName = QString::localeAwareCompare(left.list.displayName().toCaseFolded(),
                                                       right.list.displayName().toCaseFolded());
        if (byName != 0)
            return byName < 0;
        return left.list.searchValue() < right.list.searchValue();
    });
    return result;
}

}

}
