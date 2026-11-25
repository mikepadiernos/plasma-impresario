/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "RelatedMessages.h"

#include <algorithm>
#include <QAbstractItemModel>
#include <QDateTime>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QVariant>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Utils.h"

namespace UiUtils {

QString normalizedSubject(const QString &subject)
{
    // Common reply and forward prefixes, optionally counted ("Re[2]:"), in English and a few other languages
    static const QRegularExpression prefixes(QStringLiteral("^\\s*((re|fw|fwd|aw|wg|sv|vs|antw|tr|rif|r)(\\[\\d+\\])?\\s*[:：]\\s*)+"),
                                             QRegularExpression::CaseInsensitiveOption);
    QString result = subject;
    result.remove(prefixes);
    return result.simplified().toCaseFolded();
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

QList<QByteArray> inReplyTo(const QModelIndex &index)
{
    return index.data(Imap::Mailbox::RoleMessageInReplyTo).value<QList<QByteArray>>();
}

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

}

RelatedMessages collectRelatedMessages(const QAbstractItemModel *messages, const QModelIndex &current, int maximumPerGroup,
                                       int maximumScannedRows)
{
    RelatedMessages result;
    if (!messages || !current.isValid() || !current.data(Imap::Mailbox::RoleIsFetched).toBool())
        return result;

    const QString subject = normalizedSubject(current.data(Imap::Mailbox::RoleMessageSubject).toString());
    const QString sender = firstAddress(current.data(Imap::Mailbox::RoleMessageFrom));
    const QByteArray messageId = current.data(Imap::Mailbox::RoleMessageMessageId).toByteArray();
    const QList<QByteArray> parents = inReplyTo(current);
    const QModelIndex currentMessage = Imap::deproxifiedIndex(current);

    const int rows = messages->rowCount();
    const int last = qMax(0, rows - maximumScannedRows);
    if (last > 0)
        result.truncated = true;
    for (int row = rows - 1; row >= last; --row) {
        if (result.conversation.size() >= maximumPerGroup && result.sameSender.size() >= maximumPerGroup) {
            result.truncated = true;
            break;
        }
        const QModelIndex index = messages->index(row, 0);
        // Asking for envelope data of an unfetched row would download it, so look at loaded rows only.
        if (!index.data(Imap::Mailbox::RoleIsFetched).toBool())
            continue;
        if (Imap::deproxifiedIndex(index) == currentMessage)
            continue;

        const QByteArray rowId = index.data(Imap::Mailbox::RoleMessageMessageId).toByteArray();
        bool sameConversation = !subject.isEmpty()
            && normalizedSubject(index.data(Imap::Mailbox::RoleMessageSubject).toString()) == subject;
        if (!sameConversation && !messageId.isEmpty())
            sameConversation = inReplyTo(index).contains(messageId);
        if (!sameConversation && !rowId.isEmpty())
            sameConversation = parents.contains(rowId);

        if (sameConversation) {
            if (result.conversation.size() < maximumPerGroup)
                result.conversation.append(index);
            else
                result.truncated = true;
        } else if (!sender.isEmpty() && firstAddress(index.data(Imap::Mailbox::RoleMessageFrom)) == sender) {
            if (result.sameSender.size() < maximumPerGroup)
                result.sameSender.append(index);
            else
                result.truncated = true;
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

    struct Message {
        QModelIndex index;
        QString subject;
        QByteArray id;
        QList<QByteArray> parents;
    };
    const auto describe = [](const QModelIndex &index) {
        return Message{index, normalizedSubject(index.data(Imap::Mailbox::RoleMessageSubject).toString()),
                       index.data(Imap::Mailbox::RoleMessageMessageId).toByteArray(), inReplyTo(index)};
    };

    // Index the loaded rows once, then walk the links from the current message
    QList<Message> rows;
    QMultiHash<QString, qsizetype> bySubject;
    QMultiHash<QByteArray, qsizetype> byId;
    QMultiHash<QByteArray, qsizetype> byParent;
    const QModelIndex currentMessage = Imap::deproxifiedIndex(current);
    const int count = messages->rowCount();
    for (int row = count - 1, last = qMax(0, count - maximumScannedRows); row >= last; --row) {
        const QModelIndex index = messages->index(row, 0);
        if (!index.data(Imap::Mailbox::RoleIsFetched).toBool() || Imap::deproxifiedIndex(index) == currentMessage)
            continue;
        const Message message = describe(index);
        const qsizetype at = rows.size();
        if (!message.subject.isEmpty())
            bySubject.insert(message.subject, at);
        if (!message.id.isEmpty())
            byId.insert(message.id, at);
        for (const QByteArray &parent : message.parents)
            byParent.insert(parent, at);
        rows.append(message);
    }

    QList<bool> taken(rows.size(), false);
    QSet<QString> seenSubjects;
    QSet<QByteArray> seenIds;
    QList<Message> pending{describe(current)};
    while (!pending.isEmpty() && result.size() < maximum) {
        const Message message = pending.takeFirst();
        QList<qsizetype> linked;
        if (!message.subject.isEmpty() && !seenSubjects.contains(message.subject)) {
            seenSubjects.insert(message.subject);
            linked += bySubject.values(message.subject);
        }
        if (!message.id.isEmpty() && !seenIds.contains(message.id)) {
            seenIds.insert(message.id);
            linked += byParent.values(message.id);
        }
        for (const QByteArray &parent : message.parents)
            linked += byId.values(parent);
        std::sort(linked.begin(), linked.end());
        for (const qsizetype at : std::as_const(linked)) {
            if (taken.at(at) || result.size() >= maximum)
                continue;
            taken[at] = true;
            result.append(rows.at(at).index);
            pending.append(rows.at(at));
        }
    }
    return result;
}

QModelIndex adjacentInConversation(const QAbstractItemModel *view, const QModelIndex &current, bool forward, bool threaded,
                                   const QList<QPersistentModelIndex> &related)
{
    if (!view || !current.isValid() || current.model() != view || isGroupHeader(current))
        return QModelIndex();
    const QModelIndex here = current.siblingAtColumn(0);

    QList<QModelIndex> members;
    if (threaded) {
        QModelIndex root = here;
        while (root.parent().isValid() && !isGroupHeader(root.parent()))
            root = root.parent();
        members.append(root);
        appendDescendants(view, root, members);
    } else {
        QSet<QPersistentModelIndex> wanted;
        for (const QPersistentModelIndex &message : related)
            wanted.insert(QPersistentModelIndex(Imap::deproxifiedIndex(message)));
        wanted.insert(QPersistentModelIndex(Imap::deproxifiedIndex(here)));
        QList<QModelIndex> rows;
        appendDescendants(view, QModelIndex(), rows);
        for (const QModelIndex &row : std::as_const(rows)) {
            if (!isGroupHeader(row) && wanted.contains(QPersistentModelIndex(Imap::deproxifiedIndex(row))))
                members.append(row);
        }
    }
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
