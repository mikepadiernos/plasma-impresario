/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QModelIndex>
#include <QString>

class QAbstractItemModel;

namespace UiUtils {

/** @short The next folder after @arg current with unread messages, in tree order and wrapping around

@arg model is a mailbox tree which uses the Imap::Mailbox roles. Only branches which have already been listed are
searched, so that the search sends no LIST commands. A folder qualifies when it can hold messages and its unread count
is known and above zero; folders whose count is not known yet are skipped. @arg current itself never qualifies.

When @arg current is empty or not in the tree, the search starts at the first folder. Returns an invalid index when
no other folder has unread messages.
*/
QModelIndex nextUnreadMailbox(const QAbstractItemModel *model, const QString &current);

}
