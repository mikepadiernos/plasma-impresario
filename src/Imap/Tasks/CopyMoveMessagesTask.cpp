/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <algorithm>
#include <QTimer>
#include "CopyMoveMessagesTask.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxTree.h"
#include "ExpungeMessagesTask.h"
#include "KeepMailboxOpenTask.h"
#include "UpdateFlagsTask.h"

namespace Imap
{
namespace Mailbox
{


CopyMoveMessagesTask::CopyMoveMessagesTask(Model *model, const QModelIndexList &messages_, const QString &targetMailbox,
                                           const CopyMoveOperation op):
    ImapTask(model), targetMailbox(targetMailbox), shouldDelete(op == MOVE)
{
    if (messages_.isEmpty()) {
        throw CantHappen("CopyMoveMessagesTask called with empty message set");
    }
    Q_FOREACH(const QModelIndex& index, messages_) {
        messages << index;
    }
    QModelIndex mailboxIndex = model->findMailboxForItems(messages_);
    conn = model->findTaskResponsibleFor(mailboxIndex);
    conn->addDependentTask(this);
}

CopyMoveMessagesTask::CopyMoveMessagesTask(Model *model, const QModelIndex &sourceMailbox, const Imap::Uids &uids,
                                           const QString &targetMailbox, const CopyMoveOperation op,
                                           const uint expectedUidValidity):
    ImapTask(model), targetMailbox(targetMailbox), shouldDelete(op == MOVE), sourceMailbox(sourceMailbox),
    expectedUidValidity(expectedUidValidity), requestedUids(uids)
{
    if (uids.isEmpty()) {
        throw CantHappen("CopyMoveMessagesTask called with empty UID set");
    }
    std::sort(requestedUids.begin(), requestedUids.end());
    requestedUids.erase(std::unique(requestedUids.begin(), requestedUids.end()), requestedUids.end());
    conn = model->findTaskResponsibleFor(sourceMailbox);
    conn->addDependentTask(this);
}

void CopyMoveMessagesTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    if (!requestedUids.isEmpty()) {
        TreeItemMailbox *mailboxPtr = sourceMailbox.isValid() ? Model::mailboxForSomeItem(sourceMailbox) : nullptr;
        if (!mailboxPtr) {
            _failed(tr("The mailbox disappeared before we could copy the messages"));
            // No response will arrive for this task, so let the Model clean up the finished task
            QTimer::singleShot(0, model, SLOT(runReadyTasks()));
            return;
        }
        if (expectedUidValidity && mailboxPtr->syncState.uidValidity() != expectedUidValidity) {
            _failed(tr("The mailbox %1 has changed on the server (UIDVALIDITY), so the messages cannot be identified anymore")
                    .arg(mailboxPtr->mailbox()));
            QTimer::singleShot(0, model, SLOT(runReadyTasks()));
            return;
        }
        // The message nodes are only needed for flagging the originals after a plain COPY
        messages.clear();
        Q_FOREACH(TreeItemMessage *message, model->findMessagesByUids(mailboxPtr, requestedUids)) {
            messages << message->toIndex(model);
        }
        if (shouldDelete && messages.isEmpty() && !model->accessParser(parser).capabilities.contains(QStringLiteral("MOVE"))) {
            _failed(tr("All messages disappeared before we could have copied them"));
            QTimer::singleShot(0, model, SLOT(runReadyTasks()));
            return;
        }
        Sequence seq = Sequence::fromVector(requestedUids);
        if (shouldDelete && model->accessParser(parser).capabilities.contains(QStringLiteral("MOVE"))) {
            moveTag = parser->uidMove(seq, targetMailbox);
        } else {
            copyTag = parser->uidCopy(seq, targetMailbox);
        }
        return;
    }

    Sequence seq;
    bool first = true;

    Q_FOREACH(const QPersistentModelIndex& index, messages) {
        if (! index.isValid()) {
            // FIXME: add proper fix
            log(QStringLiteral("Some message got removed before we could copy them"));
        } else {
            TreeItem *item = static_cast<TreeItem *>(index.internalPointer());
            Q_ASSERT(item);
            TreeItemMessage *message = dynamic_cast<TreeItemMessage *>(item);
            Q_ASSERT(message);
            if (first) {
                seq = Sequence(message->uid());
                first = false;
            } else {
                seq.add(message->uid());
            }
            requestedUids << message->uid();
        }
    }

    if (first) {
        // No valid messages
        _failed(tr("All messages disappeared before we could have copied them"));
        return;
    }
    std::sort(requestedUids.begin(), requestedUids.end());

    if (shouldDelete && model->accessParser(parser).capabilities.contains(QStringLiteral("MOVE"))) {
        moveTag = parser->uidMove(seq, targetMailbox);
    } else {
        copyTag = parser->uidCopy(seq, targetMailbox);
    }
}

bool CopyMoveMessagesTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty()) {
        // UID MOVE reports COPYUID in an untagged OK (RFC 6851)
        return resp->kind == Responses::OK && (!moveTag.isEmpty() || !copyTag.isEmpty()) && handleCopyUid(resp);
    }

    if (resp->tag == copyTag) {
        if (resp->kind == Responses::OK) {
            handleCopyUid(resp);
            if (shouldDelete && messages.isEmpty()) {
                // UID-based copy whose originals are not known locally; nothing we could flag
                _completed();
                return true;
            }
            if (shouldDelete) {
                if (_dead) {
                    // Yeah, that's bad -- the COPY has succeeded, yet we cannot update the flags :(
                    log(QStringLiteral("COPY succeeded, but cannot update flags due to received die()"));
                    _failed(tr("Asked to die"));
                    return true;
                }
                // We ignore the _aborted status here, though -- we just want to finish in an "atomic" manner
                ImapTask *flagTask = new UpdateFlagsTask(model, this, messages, FLAG_ADD_SILENT, QStringLiteral("\\Deleted"));
                if (model->accessParser(parser).capabilities.contains(QStringLiteral("UIDPLUS"))) {
                    new ExpungeMessagesTask(model, flagTask, messages);
                }
            }
            _completed();
        } else {
            _failed(tr("The COPY operation has failed: %1").arg(resp->message));
        }
        return true;
    } else if (resp->tag == moveTag) {
        if (resp->kind == Responses::OK) {
            handleCopyUid(resp);
            _completed();
        } else {
            _failed(tr("The UID MOVE operation has failed: %1").arg(resp->message));
        }
        return true;
    } else {
        return false;
    }
}

bool CopyMoveMessagesTask::handleCopyUid(const Imap::Responses::State *const resp)
{
    if (resp->respCode != Responses::COPYUID)
        return false;
    const auto *const data =
            dynamic_cast<const Responses::RespData<QPair<uint, QPair<Sequence, Sequence> > > *>(resp->respCodeData.data());
    if (!data)
        return false;

    const uint uidValidity = data->data.first;
    const Imap::Uids sourceUids = data->data.second.first.toVector();
    const Imap::Uids destinationUids = data->data.second.second.toVector();
    if (sourceUids.size() != destinationUids.size()) {
        log(QStringLiteral("Ignoring COPYUID with mismatched UID sets"));
        return false;
    }
    // Several commands might be in flight on this connection; only take what belongs to us
    for (const uint uid : sourceUids) {
        if (!std::binary_search(requestedUids.constBegin(), requestedUids.constEnd(), uid))
            return false;
    }

    m_copyUidValidity = uidValidity;
    m_copyUidSource += sourceUids;
    m_copyUidDestination += destinationUids;
    emit copyUidReceived(uidValidity, sourceUids, destinationUids);
    return true;
}

QString CopyMoveMessagesTask::targetMailboxName() const
{
    return targetMailbox;
}

uint CopyMoveMessagesTask::copyUidValidity() const
{
    return m_copyUidValidity;
}

Imap::Uids CopyMoveMessagesTask::copyUidSourceUids() const
{
    return m_copyUidSource;
}

Imap::Uids CopyMoveMessagesTask::copyUidDestinationUids() const
{
    return m_copyUidDestination;
}

QVariant CopyMoveMessagesTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Copying messages")) : QVariant();
}

}
}
