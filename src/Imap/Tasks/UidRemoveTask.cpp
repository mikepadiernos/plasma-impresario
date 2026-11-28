/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UidRemoveTask.h"
#include "GetAnyConnectionTask.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/TaskFactory.h"
#include "KeepMailboxOpenTask.h"

namespace Imap {
namespace Mailbox {

UidRemoveTask::UidRemoveTask(Model *model, const QString &mailbox,
                             const uint uidValidity, const uint uid)
    : ImapTask(model), m_uidValidity(uidValidity), m_uid(uid) {
  if (TreeItemMailbox *mailboxPtr = model->findMailboxByName(mailbox)) {
    m_mailboxIndex = mailboxPtr->toIndex(model);
    conn = model->findTaskResponsibleFor(m_mailboxIndex);
  } else {
    // perform() reports the unknown mailbox
    conn = model->m_taskFactory->createGetAnyConnectionTask(model);
  }
  conn->addDependentTask(this);
}

void UidRemoveTask::perform() {
  parser = conn->parser;
  Q_ASSERT(parser);
  markAsActiveTask();

  IMAP_TASK_CHECK_ABORT_DIE;

  if (!m_mailboxIndex.isValid()) {
    _failed(tr("The mailbox is not known"));
    return;
  }
  const uint realUidValidity =
      m_mailboxIndex.data(RoleMailboxUidValidity).toUInt();
  if (realUidValidity != m_uidValidity) {
    _failed(tr("UIDVALIDITY mismatch: expected %1, got %2")
                .arg(QString::number(m_uidValidity),
                     QString::number(realUidValidity)));
    return;
  }
  if (!model->accessParser(parser).capabilities.contains(
          QStringLiteral("UIDPLUS"))) {
    _failed(tr("The IMAP server doesn't support the UIDPLUS extension"));
    return;
  }

  storeTag = parser->uidStore(Sequence(m_uid), QStringLiteral("+FLAGS.SILENT"),
                              QStringLiteral("(\\Deleted)"));
}

bool UidRemoveTask::handleStateHelper(
    const Imap::Responses::State *const resp) {
  if (resp->tag.isEmpty())
    return false;

  if (resp->tag == storeTag) {
    if (resp->kind == Responses::OK) {
      expungeTag = parser->uidExpunge(Sequence(m_uid));
    } else {
      _failed(tr("UID STORE failed: %1").arg(resp->message));
    }
    return true;
  }
  if (resp->tag == expungeTag) {
    if (resp->kind == Responses::OK) {
      _completed();
    } else {
      _failed(tr("UID EXPUNGE failed: %1").arg(resp->message));
    }
    return true;
  }
  return false;
}

QVariant UidRemoveTask::taskData(const int role) const {
  return role == RoleTaskCompactName ? QVariant(tr("Removing a message"))
                                     : QVariant();
}

} // namespace Mailbox
} // namespace Imap
