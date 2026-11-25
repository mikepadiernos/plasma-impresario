/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "RelatedMessages.h"

#include <QAbstractItemModel>
#include <QRegularExpression>
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

}
