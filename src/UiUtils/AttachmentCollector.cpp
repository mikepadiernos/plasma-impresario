/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "AttachmentCollector.h"

#include <QAbstractItemModel>

#include "Imap/Model/ItemRoles.h"

namespace UiUtils {

namespace {

constexpr int MaximumDepth = 100;

QByteArray mimeTypeOf(const QModelIndex &part)
{
    return part.data(Imap::Mailbox::RolePartMimeType).toByteArray().toLower();
}

QByteArray dispositionOf(const QModelIndex &part)
{
    return part.data(Imap::Mailbox::RolePartBodyDisposition).toByteArray().toLower();
}

bool isInlineText(const QModelIndex &part)
{
    const QByteArray disposition = dispositionOf(part);
    const bool isInline = disposition.isEmpty() || disposition == "inline";
    return isInline && part.data(Imap::Mailbox::RolePartFileName).toString().isEmpty();
}

void walk(const QModelIndex &part, int depth, QList<QPersistentModelIndex> &out)
{
    if (!part.isValid() || depth > MaximumDepth)
        return;

    const QAbstractItemModel *model = part.model();
    const int children = model->rowCount(part);
    const QByteArray mimeType = mimeTypeOf(part);

    if (mimeType == "multipart/signed") {
        // The second child is the detached signature, never an attachment.
        walk(model->index(0, 0, part), depth + 1, out);
    } else if (mimeType == "multipart/encrypted") {
        // Before decryption the children are the protocol version and the ciphertext; after decryption the
        // cryptography layer inserts the decrypted tree, which is walked normally.
        for (int row = 0; row < children; ++row) {
            const QModelIndex child = model->index(row, 0, part);
            const QByteArray childType = mimeTypeOf(child);
            if (childType == "application/pgp-encrypted" || childType == "application/octet-stream")
                continue;
            walk(child, depth + 1, out);
        }
    } else if (mimeType == "multipart/related") {
        // RFC 2387: the first part is the root unless a start parameter says otherwise; the other parts are
        // resources referenced by it. Only list those which the sender explicitly marked as attachments.
        walk(model->index(0, 0, part), depth + 1, out);
        for (int row = 1; row < children; ++row) {
            const QModelIndex child = model->index(row, 0, part);
            if (dispositionOf(child) == "attachment")
                out.append(QPersistentModelIndex(child));
        }
    } else if (mimeType.startsWith("multipart/")) {
        // RFC 2046: unrecognized multiparts are treated as multipart/mixed.
        for (int row = 0; row < children; ++row)
            walk(model->index(row, 0, part), depth + 1, out);
    } else if (mimeType == "text/plain" || mimeType == "text/html") {
        if (!isInlineText(part))
            out.append(QPersistentModelIndex(part));
    } else if (!mimeType.isEmpty()) {
        // Includes message/rfc822, which is offered as one item rather than expanded.
        out.append(QPersistentModelIndex(part));
    }
}

}

QList<QPersistentModelIndex> collectAttachments(const QModelIndex &part)
{
    QList<QPersistentModelIndex> out;
    walk(part, 0, out);
    return out;
}

QList<QPersistentModelIndex> collectAttachments(const QAbstractItemModel *model)
{
    if (!model)
        return {};
    const QModelIndex message = model->index(0, 0);
    if (!message.isValid())
        return {};
    QList<QPersistentModelIndex> out;
    for (int row = 0; row < model->rowCount(message); ++row)
        walk(model->index(row, 0, message), 0, out);
    return out;
}

}
