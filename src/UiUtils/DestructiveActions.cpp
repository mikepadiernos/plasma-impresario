/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "DestructiveActions.h"

#include <KConfigGroup>

namespace UiUtils {
namespace DestructiveActions {

DeleteAction chooseDeleteAction(bool moveToTrashEnabled, const QString &trashMailbox, const QString &currentMailbox)
{
    if (!moveToTrashEnabled || trashMailbox.isEmpty() || currentMailbox == trashMailbox)
        return DeleteAction::MarkDeleted;
    return DeleteAction::MoveToTrash;
}

QString configGroupName()
{
    return QStringLiteral("Confirmations");
}

QString configKey(Confirmation confirmation)
{
    switch (confirmation) {
    case Confirmation::Expunge:
        return QStringLiteral("ConfirmExpunge");
    case Confirmation::DeletePermanently:
        return QStringLiteral("ConfirmDeletePermanently");
    case Confirmation::EmptyFolder:
        return QStringLiteral("ConfirmEmptyFolder");
    }
    Q_UNREACHABLE();
}

bool confirm(KConfigGroup &group, Confirmation confirmation, const std::function<ConfirmationAnswer()> &prompt)
{
    const QString key = configKey(confirmation);
    if (!group.readEntry(key, true))
        return true;
    const ConfirmationAnswer answer = prompt ? prompt() : ConfirmationAnswer{};
    if (answer.accepted && answer.dontAskAgain) {
        group.writeEntry(key, false);
        group.sync();
    }
    return answer.accepted;
}

}
}
