/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QModelIndex>
#include <QObject>
#include <QPointer>
#include <QString>

class QAbstractItemModel;
class QTreeView;

namespace Gui {

class MessageSelectionRestorer : public QObject
{
    Q_OBJECT
public:
    explicit MessageSelectionRestorer(QTreeView *view, QObject *parent);

    /** Restore a message when its mailbox and IMAP identifiers are available. */
    void restore(const QString &mailbox, uint uidValidity, uint uid);

    /** @short Restore where the list of @p mailbox was left

    The message @p anchorUid goes to the top of the view, @p uid becomes current and the messages @p selectedUids
    are selected along with it. Either UID may be 0.
    */
    void restorePosition(const QString &mailbox, uint uidValidity, uint uid, uint anchorUid,
                         const QList<uint> &selectedUids = {});

    bool isPending() const;

    /** Cancel a pending message restoration. */
    void cancel();

    /** Recheck a pending selection after mailbox metadata changes. */
    void scheduleCheck();

signals:
    /** Emitted after the saved message has been selected. */
    void messageRestored(const QModelIndex &index);
    /** @short The mailbox's messages are there, but the saved current message is not, or its UIDVALIDITY changed

    Only restorePosition() reports a missing message; restore() keeps waiting for it to arrive.
    */
    void restoreFailed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void check();

    QPointer<QTreeView> m_view;
    QPointer<QAbstractItemModel> m_model;
    QString m_mailbox;
    uint m_uidValidity = 0;
    uint m_uid = 0;
    uint m_anchorUid = 0;
    QList<uint> m_selectedUids;
    bool m_pending = false;
    /** Whether a missing current message ends the restoration instead of waiting for it to arrive */
    bool m_reportMissing = false;
    bool m_checkScheduled = false;
};

}
