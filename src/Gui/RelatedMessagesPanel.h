/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "EmbeddedPanel.h"

class QTreeWidget;
class QTreeWidgetItem;

namespace Gui {

/** @short Embedded panel with messages related to the one shown in the reader

Groups messages from the current message list that belong to the same conversation
and other messages from the same sender. Only messages which the list has already
loaded are considered, so the panel never makes the IMAP model download envelopes.
*/
class RelatedMessagesPanel : public MessageContextPanel
{
    Q_OBJECT
public:
    explicit RelatedMessagesPanel(QWidget *parent = nullptr);

    static Descriptor panelDescriptor();

    /** Search the rows of a flat message list such as Imap::Mailbox::MsgListModel */
    void setMessageList(QAbstractItemModel *messages);

    bool hasContent() const override;
    QWidget *focusTarget() const override;

    QList<QPersistentModelIndex> conversation() const;
    QList<QPersistentModelIndex> sameSender() const;

public slots:
    void activateCurrent();

signals:
    /** The user picked a message; @arg message is a row of the message list */
    void messageActivated(const QModelIndex &message);

protected:
    void refresh() override;
    void panelEnabledChanged() override;

private:
    void scheduleListRefresh();
    void fillGroup(QTreeWidgetItem *group, const QList<QPersistentModelIndex> &messages);

    QPointer<QAbstractItemModel> m_messages;
    QList<QMetaObject::Connection> m_listConnections;
    QList<QPersistentModelIndex> m_conversation;
    QList<QPersistentModelIndex> m_sameSender;
    QTreeWidget *m_tree;
    QTimer *m_listTimer;
};

}
