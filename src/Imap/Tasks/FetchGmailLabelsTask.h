/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef IMAP_FETCHGMAILLABELSTASK_H
#define IMAP_FETCHGMAILLABELSTASK_H

#include <QPersistentModelIndex>
#include "ImapTask.h"

namespace Imap
{
namespace Mailbox
{

/** @short Fetch Gmail's labels (X-GM-LABELS) of some messages

Labels change without any notice from the server, so they are fetched again whenever they are needed.
*/
class FetchGmailLabelsTask : public ImapTask
{
    Q_OBJECT
public:
    FetchGmailLabelsTask(Model *model, const QModelIndex &mailbox, const Imap::Uids &uids);
    void perform() override;

    bool handleFetch(const Imap::Responses::Fetch *const resp) override;
    bool handleStateHelper(const Imap::Responses::State *const resp) override;

    QString debugIdentification() const override;
    QVariant taskData(const int role) const override;
    bool needsMailbox() const override { return true; }

private:
    CommandHandle tag;
    ImapTask *conn;
    QPersistentModelIndex mailbox;
    Imap::Uids uids;
};

}
}

#endif // IMAP_FETCHGMAILLABELSTASK_H
