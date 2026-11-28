/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "RenameMailboxTask.h"
#include "Common/InvokeMethod.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/TaskFactory.h"
#include "GetAnyConnectionTask.h"
#include "KeepMailboxOpenTask.h"
#include "SortTask.h"

namespace Imap
{
namespace Mailbox
{

RenameMailboxTask::RenameMailboxTask(Model *model, const QString &oldName, const QString &newName):
    ImapTask(model), conn(nullptr), oldName(oldName), newName(newName)
{
    conn = model->m_taskFactory->createGetAnyConnectionTask(model);
    conn->addDependentTask(this);
}

void RenameMailboxTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    sendRenameWhenQuiet();
}

/** @short Send the RENAME once no other command is running on the connection

Dovecot ends the session ("IMAP session state is inconsistent") when the selected mailbox is renamed while another
command on it, such as a SEARCH, is still running. A search which only listens for updates does not count.
*/
void RenameMailboxTask::sendRenameWhenQuiet()
{
    if (!tagRename.isEmpty() || isFinished())
        return;
    IMAP_TASK_CHECK_ABORT_DIE;
    for (ImapTask *task : model->accessParser(parser).activeTasks) {
        if (task == this || task->isFinished() || qobject_cast<KeepMailboxOpenTask *>(task))
            continue;
        if (auto *sort = qobject_cast<SortTask *>(task); sort && sort->isJustUpdatingNow())
            continue;
        connect(task, &ImapTask::completed, this, &RenameMailboxTask::sendRenameWhenQuiet, Qt::QueuedConnection);
        connect(task, &ImapTask::failed, this, &RenameMailboxTask::sendRenameWhenQuiet, Qt::QueuedConnection);
        return;
    }
    tagRename = parser->rename(oldName, newName);
}

void RenameMailboxTask::removeOldMailboxItem()
{
    TreeItemMailbox *mailboxPtr = model->findMailboxByName(oldName);
    if (!mailboxPtr) {
        log(QStringLiteral("Renamed mailbox %1 is not known locally").arg(oldName));
        return;
    }
    TreeItem *parentPtr = mailboxPtr->parent();
    QModelIndex parentIndex = parentPtr == model->m_mailboxes ? QModelIndex() : parentPtr->toIndex(model);
    const int row = mailboxPtr->row();
    model->beginRemoveRows(parentIndex, row, row);
    parentPtr->m_children.erase(parentPtr->m_children.begin() + row);
    model->endRemoveRows();
    // If the mailbox was open, its KeepMailboxOpenTask notices the invalid index and closes it (UNSELECT)
    delete mailboxPtr;
}

bool RenameMailboxTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (resp->tag == tagRename) {
        if (resp->kind == Responses::OK) {
            removeOldMailboxItem();
            EMIT_LATER(model, mailboxRenameSucceeded, Q_ARG(QString, oldName), Q_ARG(QString, newName));
            if (_dead) {
                _failed(tr("Asked to die"));
                return true;
            }
            // Don't call _completed() yet, the new name has to appear in the mailbox list first
            tagList = parser->list(QLatin1String(""), newName);
        } else {
            EMIT_LATER(model, mailboxRenameFailed, Q_ARG(QString, oldName), Q_ARG(QString, newName),
                       Q_ARG(QString, resp->message));
            _failed(tr("Cannot rename mailbox"));
        }
        return true;
    } else if (resp->tag == tagList) {
        if (resp->kind == Responses::OK) {
            model->finalizeIncrementalList(parser, newName);
            _completed();
        } else {
            _failed(tr("Error with the LIST command after the RENAME"));
        }
        return true;
    } else {
        return false;
    }
}

QVariant RenameMailboxTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Renaming mailbox")) : QVariant();
}

}
}
