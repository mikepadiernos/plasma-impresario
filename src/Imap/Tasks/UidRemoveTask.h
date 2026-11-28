/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef IMAP_UIDREMOVE_TASK_H
#define IMAP_UIDREMOVE_TASK_H

#include "ImapTask.h"
#include <QPersistentModelIndex>

namespace Imap {
namespace Mailbox {

/** @short Remove one message, named by its UID, from a mailbox

The message is marked as \\Deleted and then expunged through UID EXPUNGE, so
other messages marked as \\Deleted stay. The task fails when UIDVALIDITY has
changed or the server lacks UIDPLUS.
*/
class UidRemoveTask : public ImapTask
{
    Q_OBJECT
public:
    UidRemoveTask(Model *model, const QString &mailbox, const uint uidValidity, const uint uid);
    void perform() override;

    bool handleStateHelper(const Imap::Responses::State *const resp) override;
    bool needsMailbox() const override
    {
        return true;
    }
    QVariant taskData(const int role) const override;

private:
    ImapTask *conn = nullptr;
    CommandHandle storeTag;
    CommandHandle expungeTag;
    QPersistentModelIndex m_mailboxIndex;
    uint m_uidValidity;
    uint m_uid;
};

} // namespace Mailbox
} // namespace Imap

#endif // IMAP_UIDREMOVE_TASK_H
