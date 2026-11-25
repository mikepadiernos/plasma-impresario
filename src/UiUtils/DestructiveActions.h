/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QString>
#include <functional>

class KConfigGroup;

namespace UiUtils {
namespace DestructiveActions {

/** @short What the Delete action does for the current folder */
enum class DeleteAction {
    /** Move the messages to the account's Trash folder */
    MoveToTrash,
    /** Toggle the IMAP \Deleted flag; the messages stay until the folder is expunged */
    MarkDeleted,
};

/** @short Choose between moving to Trash and marking as deleted

@arg moveToTrashEnabled is the per-account "Move Deleted Messages to Trash" setting, @arg trashMailbox the name of the
account's Trash folder (empty if there is none) and @arg currentMailbox the folder the messages are in. Messages
which already are in the Trash are only marked as deleted.
*/
DeleteAction chooseDeleteAction(bool moveToTrashEnabled, const QString &trashMailbox, const QString &currentMailbox);

/** @short Permanent operations which ask for confirmation first */
enum class Confirmation {
    Expunge,
    DeletePermanently,
    EmptyFolder,
};

/** @short KConfig group which stores whether to ask, "Confirmations" */
QString configGroupName();

/** @short Key of @arg confirmation in configGroupName(); true or missing means "ask" */
QString configKey(Confirmation confirmation);

struct ConfirmationAnswer {
    bool accepted = false;
    /** The user ticked "Do not ask again" */
    bool dontAskAgain = false;
};

/** @short Whether to go ahead with @arg confirmation, asking through @arg prompt unless the user opted out

The prompt is not shown if @arg group says not to ask. "Do not ask again" is only stored when the user also
accepted, so cancelling never turns the question off.
*/
bool confirm(KConfigGroup &group, Confirmation confirmation, const std::function<ConfirmationAnswer()> &prompt);

}
}
