/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QFrame>
#include <QPersistentModelIndex>
#include <QPointer>

class QAbstractItemModel;
class QLabel;
class QListWidget;
class QStackedWidget;
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
arrive later, for example after decryption. Whether it is shown depends on two
user preferences: the panel can be turned off, and it can hide itself while there
is nothing to list (no message, or a message without attachments).
*/
class AttachmentPanel : public QFrame
{
    Q_OBJECT
public:
    explicit AttachmentPanel(QWidget *parent = nullptr);

    void setNetworkAccessManager(Imap::Network::MsgPartNetAccessManager *manager);

    /** Whether the user wants the panel at all (default: true) */
    bool isPanelEnabled() const;
    /** Whether the panel hides itself while there are no attachments to list (default: false) */
    bool autoHide() const;
    /** Whether the panel should currently be shown, given the preferences and the current message */
    bool shouldBeShown() const;

    int attachmentCount() const;
    QList<QPersistentModelIndex> attachments() const;

    QSize sizeHint() const override;

public slots:
    /** Follow the part model of a message; nullptr, or the model being destroyed, means no message */
    void setMessageModel(QAbstractItemModel *model);
    void setPanelEnabled(bool enabled);
    void setAutoHide(bool autoHide);
    void openCurrent();
    void saveCurrent();

signals:
    void transferError(const QString &errorString);
    /** Emitted when the panel is shown or hidden because of the preferences or the current message */
    void relevanceChanged(bool shown);

private:
    void scheduleRefresh();
    void refresh();
    void updateActions();
    void updateVisibility();
    void showContextMenu(const QPoint &pos);
    QPersistentModelIndex currentPart() const;

    QPointer<QAbstractItemModel> m_model;
    QPointer<Imap::Network::MsgPartNetAccessManager> m_netAccess;
    QList<QPersistentModelIndex> m_attachments;
    QList<QMetaObject::Connection> m_modelConnections;
    QLabel *m_title;
    QStackedWidget *m_stack;
    QLabel *m_placeholder;
    QListWidget *m_list;
    QToolButton *m_open;
    QToolButton *m_save;
    QTimer *m_refreshTimer;
    bool m_enabled = true;
    bool m_autoHide = false;
    int m_appliedVisibility = -1;
};

}
