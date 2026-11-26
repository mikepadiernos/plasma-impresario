/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "RelatedMessages.h"

#include <algorithm>
#include <QAbstractItemModel>
#include <QDateTime>
#include <QSet>
#include <QStringList>
#include <QVariant>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Utils.h"
#include "Conversations.h"
#include "QuickFilters.h"

namespace UiUtils {

QString normalizedSubject(const QString &subject)
{
    return searchableSubject(subject).toCaseFolded();
}

QString firstAddress(const QVariant &addresses)
{
    const QVariantList list = addresses.toList();
    if (list.isEmpty())
        return QString();
    const QStringList parts = list.constFirst().toStringList();
    if (parts.size() < 4 || parts.at(2).isEmpty())
        return QString();
    return QString(parts.at(2) + QLatin1Char('@') + parts.at(3)).toCaseFolded();
}

namespace {

bool isGroupHeader(const QModelIndex &index)
{
    return index.data(Imap::Mailbox::RoleIsGroupHeader).toBool();
}

/** Appends the rows below @arg parent in display order, as if every row were expanded */
void appendDescendants(const QAbstractItemModel *model, const QModelIndex &parent, QList<QModelIndex> &rows)
{
    for (int row = 0, count = model->rowCount(parent); row < count; ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        rows.append(index);
        appendDescendants(model, index, rows);
    }
}

/** The loaded rows of a message list, in row order, followed by the current message */
struct LoadedRows {
    QList<QModelIndex> rows;
    QList<ConversationMessage> messages;
    bool truncated = false;
};

LoadedRows loadedRows(const QAbstractItemModel *messages, const QModelIndex &current, int maximumScannedRows)
{
    LoadedRows result;
    const QModelIndex currentMessage = Imap::deproxifiedIndex(current);
    const int count = messages->rowCount();
    const int first = qMax(0, count - maximumScannedRows);
    result.truncated = first > 0;
    for (int row = first; row < count; ++row) {
        const QModelIndex index = messages->index(row, 0);
        // Asking for envelope data of an unfetched row would download it, so look at loaded rows only.
        if (!index.data(Imap::Mailbox::RoleIsFetched).toBool() || Imap::deproxifiedIndex(index) == currentMessage)
            continue;
        result.rows.append(index);
        result.messages.append(conversationMessage(index));
    }
    result.rows.append(current);
    result.messages.append(conversationMessage(current));
    return result;
}

/** Positions in @arg loaded of the other messages in the conversation of its current message, last row first */
QList<qsizetype> conversationOf(const LoadedRows &loaded)
{
    const QList<qsizetype> groups = groupConversations(loaded.messages);
    const qsizetype self = loaded.rows.size() - 1;
    QList<qsizetype> result;
    for (qsizetype at = self - 1; at >= 0; --at) {
        if (groups.at(at) == groups.at(self))
            result.append(at);
    }
    return result;
}

}

RelatedMessages collectRelatedMessages(const QAbstractItemModel *messages, const QModelIndex &current, int maximumPerGroup,
                                       int maximumScannedRows)
{
    RelatedMessages result;
    if (!messages || !current.isValid() || !current.data(Imap::Mailbox::RoleIsFetched).toBool())
        return result;

    const LoadedRows loaded = loadedRows(messages, current, maximumScannedRows);
    result.truncated = loaded.truncated;
    QSet<qsizetype> inConversation;
    for (const qsizetype at : conversationOf(loaded)) {
        inConversation.insert(at);
        if (result.conversation.size() < maximumPerGroup)
            result.conversation.append(loaded.rows.at(at));
        else
            result.truncated = true;
    }

    const QString sender = firstAddress(current.data(Imap::Mailbox::RoleMessageFrom));
    if (sender.isEmpty())
        return result;
    for (qsizetype at = loaded.rows.size() - 2; at >= 0; --at) {
        if (inConversation.contains(at) || firstAddress(loaded.rows.at(at).data(Imap::Mailbox::RoleMessageFrom)) != sender)
            continue;
        if (result.sameSender.size() < maximumPerGroup) {
            result.sameSender.append(loaded.rows.at(at));
        } else {
            result.truncated = true;
            break;
        }
    }
    return result;
}

QList<QPersistentModelIndex> collectConversation(const QAbstractItemModel *messages, const QModelIndex &current, int maximum,
                                                 int maximumScannedRows)
{
    QList<QPersistentModelIndex> result;
    if (!messages || !current.isValid() || !current.data(Imap::Mailbox::RoleIsFetched).toBool())
        return result;
    const LoadedRows loaded = loadedRows(messages, current, maximumScannedRows);
    for (const qsizetype at : conversationOf(loaded)) {
        if (result.size() >= maximum)
            break;
        result.append(loaded.rows.at(at));
    }
    return result;
}

namespace {

/** The member after or before @arg here, once @arg members are put in date order */
QModelIndex stepByDate(QList<QModelIndex> members, const QModelIndex &here, bool forward)
{
    std::stable_sort(members.begin(), members.end(), [](const QModelIndex &a, const QModelIndex &b) {
        return a.data(Imap::Mailbox::RoleMessageDate).toDateTime() < b.data(Imap::Mailbox::RoleMessageDate).toDateTime();
    });
    members.removeIf([&here](const QModelIndex &index) {
        return index != here && (isGroupHeader(index) || index.data(Imap::Mailbox::RoleMessageUid).toUInt() == 0);
    });
    const qsizetype at = members.indexOf(here);
    if (at < 0)
        return QModelIndex();
    const qsizetype next = at + (forward ? 1 : -1);
    return next >= 0 && next < members.size() ? members.at(next) : QModelIndex();
}

}

QModelIndex adjacentInThread(const QAbstractItemModel *view, const QModelIndex &current, bool forward)
{
    if (!view || !current.isValid() || current.model() != view || isGroupHeader(current))
        return QModelIndex();
    const QModelIndex here = current.siblingAtColumn(0);
    QModelIndex root = here;
    while (root.parent().isValid() && !isGroupHeader(root.parent()))
        root = root.parent();
    QList<QModelIndex> members{root};
    appendDescendants(view, root, members);
    return stepByDate(members, here, forward);
}

QModelIndex adjacentByDate(const QModelIndex &current, bool forward, const QList<QPersistentModelIndex> &conversation)
{
    if (!current.isValid() || isGroupHeader(current))
        return QModelIndex();
    const QModelIndex here = Imap::deproxifiedIndex(current.siblingAtColumn(0));
    QList<QModelIndex> members{here};
    for (const QPersistentModelIndex &message : conversation) {
        const QModelIndex member = Imap::deproxifiedIndex(message);
        if (member.isValid() && !members.contains(member))
            members.append(member);
    }
    return stepByDate(members, here, forward);
}

}
