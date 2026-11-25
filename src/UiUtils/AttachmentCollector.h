/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QPersistentModelIndex>

class QAbstractItemModel;

namespace UiUtils {

/** @short Return the attachment parts of a message, in MIME order.

The model is a per-message part model such as Cryptography::MessageModel, whose
index(0, 0) is the message and whose children are MIME parts. The rules follow
the message-list attachment indicator with three refinements for a summary
panel: an attached message/rfc822 is listed once instead of being recursed
into, multipart/related lists only explicit attachments besides its root part
(so cid: images such as signature logos are not listed), and the raw OpenPGP
control and ciphertext parts of multipart/encrypted are skipped.
*/
QList<QPersistentModelIndex> collectAttachments(const QAbstractItemModel *model);

/** @short Same as collectAttachments(model), starting at an arbitrary MIME part */
QList<QPersistentModelIndex> collectAttachments(const QModelIndex &part);

}
