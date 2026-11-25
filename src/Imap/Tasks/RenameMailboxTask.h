/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef IMAP_RENAMEMAILBOX_TASK_H
#define IMAP_RENAMEMAILBOX_TASK_H

#include "ImapTask.h"

namespace Imap
{
namespace Mailbox
{

/** @short Rename a mailbox (RFC 3501 RENAME) and refresh its entry in the mailbox tree

The old tree item, including any loaded children, is removed once the server confirms the rename. The new mailbox is
then listed and inserted below the same parent; its own children are listed lazily.
*/
class RenameMailboxTask : public ImapTask
{
    Q_OBJECT
public:
    RenameMailboxTask(Model *model, const QString &oldName, const QString &newName);
    void perform() override;

    bool handleStateHelper(const Imap::Responses::State *const resp) override;
    QVariant taskData(const int role) const override;
    bool needsMailbox() const override {return false;}
private:
    void removeOldMailboxItem();

    CommandHandle tagRename, tagList;
    ImapTask *conn;
    QString oldName;
    QString newName;
};

}
}

#endif // IMAP_RENAMEMAILBOX_TASK_H
