/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

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

    /** Cancel a pending message restoration. */
    void cancel();

    /** Recheck a pending selection after mailbox metadata changes. */
    void scheduleCheck();

signals:
    /** Emitted after the saved message has been selected. */
    void messageRestored(const QModelIndex &index);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void check();

    QPointer<QTreeView> m_view;
    QPointer<QAbstractItemModel> m_model;
    QString m_mailbox;
    uint m_uidValidity = 0;
    uint m_uid = 0;
    bool m_pending = false;
    bool m_checkScheduled = false;
};

}
