/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QPersistentModelIndex>
#include <QPointer>

#include "EmbeddedPanel.h"

class QAbstractItemModel;
class QListWidget;
class QTimer;
class QToolButton;

namespace Imap {
namespace Network {
class MsgPartNetAccessManager;
}
}

namespace Gui {

/** @short Embedded panel listing the attachments of the message shown in the reader

The panel follows the reader's per-message part model and refreshes when parts
arrive later, for example after decryption. It is enabled by default and placed
below the reader; with auto-hide it is only shown while the message has attachments.
*/
class AttachmentPanel : public EmbeddedPanel
{
    Q_OBJECT
public:
    explicit AttachmentPanel(QWidget *parent = nullptr);

    static Descriptor panelDescriptor();

    void setNetworkAccessManager(Imap::Network::MsgPartNetAccessManager *manager);

    int attachmentCount() const;
    QList<QPersistentModelIndex> attachments() const;
    /** The attachment list, which receives keyboard focus when the panel is entered */
    QWidget *listView() const;

    bool hasContent() const override;
    QWidget *focusTarget() const override;

public slots:
    /** Follow the part model of a message; nullptr, or the model being destroyed, means no message */
    void setMessageModel(QAbstractItemModel *model);
    void openCurrent();
    void saveCurrent();

signals:
    void transferError(const QString &errorString);
    /** The number of attachments of the shown message changed, for example when its structure arrived */
    void attachmentCountChanged(int count);

private:
    void scheduleRefresh();
    void refresh();
    void updateActions();
    void showContextMenu(const QPoint &pos);
    QPersistentModelIndex currentPart() const;

    QPointer<QAbstractItemModel> m_model;
    QPointer<Imap::Network::MsgPartNetAccessManager> m_netAccess;
    QList<QPersistentModelIndex> m_attachments;
    QList<QMetaObject::Connection> m_modelConnections;
    QListWidget *m_list;
    QToolButton *m_open;
    QToolButton *m_save;
    QTimer *m_refreshTimer;
};

}
