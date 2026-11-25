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

#ifndef IMAP_COPYMOVEMESSAGES_TASK_H
#define IMAP_COPYMOVEMESSAGES_TASK_H

#include <QPersistentModelIndex>
#include "Imap/Model/Model.h"
#include "ImapTask.h"

namespace Imap
{
namespace Mailbox
{

/** @short Copy or move messages to a different mailbox */
class CopyMoveMessagesTask : public ImapTask
{
    Q_OBJECT
public:
    CopyMoveMessagesTask(Model *model, const QModelIndexList &messages, const QString &targetMailbox, const CopyMoveOperation op);
    /** @short Copy or move messages identified by their UIDs, which need not be loaded yet

    If @arg expectedUidValidity is non-zero, the task fails instead of touching any message when the source
    mailbox's UIDVALIDITY does not match it.
    */
    CopyMoveMessagesTask(Model *model, const QModelIndex &sourceMailbox, const Imap::Uids &uids, const QString &targetMailbox,
                         const CopyMoveOperation op, const uint expectedUidValidity = 0);
    void perform() override;

    bool handleStateHelper(const Imap::Responses::State *const resp) override;
    QVariant taskData(const int role) const override;
    bool needsMailbox() const override {return true;}

    QString targetMailboxName() const;
    /** @short UIDVALIDITY of the target mailbox as reported by COPYUID, or zero if the server did not report it */
    uint copyUidValidity() const;
    /** @short Source UIDs as reported by COPYUID (RFC 4315), in the server's order */
    Imap::Uids copyUidSourceUids() const;
    /** @short UIDs of the copies in the target mailbox as reported by COPYUID, matching copyUidSourceUids() */
    Imap::Uids copyUidDestinationUids() const;

signals:
    /** @short The server reported where the messages ended up (COPYUID); may be emitted more than once */
    void copyUidReceived(uint uidValidity, const Imap::Uids &sourceUids, const Imap::Uids &destinationUids);

private:
    bool handleCopyUid(const Imap::Responses::State *const resp);

    CommandHandle copyTag;
    CommandHandle moveTag;
    ImapTask *conn;
    QList<QPersistentModelIndex> messages;
    QString targetMailbox;
    bool shouldDelete;
    /** @short Source mailbox of the UID-based constructor; invalid otherwise */
    QPersistentModelIndex sourceMailbox;
    uint expectedUidValidity = 0;
    /** @short UIDs sent to the server */
    Imap::Uids requestedUids;
    uint m_copyUidValidity = 0;
    Imap::Uids m_copyUidSource;
    Imap::Uids m_copyUidDestination;
};

}
}

#endif // IMAP_COPYMOVEMESSAGES_TASK_H
