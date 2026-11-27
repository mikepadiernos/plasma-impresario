/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "FetchGmailLabelsTask.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "KeepMailboxOpenTask.h"

namespace Imap
{
namespace Mailbox
{

FetchGmailLabelsTask::FetchGmailLabelsTask(Model *model, const QModelIndex &mailbox, const Imap::Uids &uids)
    : ImapTask(model)
    , mailbox(mailbox)
    , uids(uids)
{
    Q_ASSERT(!uids.isEmpty());
    conn = model->findTaskResponsibleFor(mailbox);
    conn->addDependentTask(this);
}

void FetchGmailLabelsTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    tag = parser->uidFetch(Sequence::fromVector(uids), QList<QByteArray>() << "X-GM-LABELS");
}

bool FetchGmailLabelsTask::handleFetch(const Imap::Responses::Fetch *const resp)
{
    if (!mailbox.isValid()) {
        _failed(tr("handleFetch: mailbox disappeared"));
        return false;
    }
    auto *mailboxPtr = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailbox.internalPointer()));
    Q_ASSERT(mailboxPtr);
    model->genericHandleFetch(mailboxPtr, resp);
    return true;
}

bool FetchGmailLabelsTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty() || resp->tag != tag)
        return false;

    if (resp->kind == Responses::OK)
        _completed();
    else
        _failed(tr("Fetching Gmail labels failed"));
    return true;
}

QString FetchGmailLabelsTask::debugIdentification() const
{
    if (!mailbox.isValid())
        return QStringLiteral("[invalid mailbox]");

    return QStringLiteral("%1: UIDs %2").arg(mailbox.data(RoleMailboxName).toString(),
                                             QString::fromUtf8(Sequence::fromVector(uids).toByteArray()));
}

QVariant FetchGmailLabelsTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Downloading labels")) : QVariant();
}

}
}
