/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QPersistentModelIndex>
#include <QWidget>

class QAction;
class QMenu;
class QToolBar;
class QToolButton;

namespace Gui {

/** @short Actions and preview controls for the message open in the main window's reader

The bar sits above the message and acts on the open message only, whatever is selected in the
message list. It reflects the read, flag and deleted marks of that message, offers zoom and the
plain-text preference, and shows how many attachments the message has.
*/
class ReaderActionBar : public QWidget
{
    Q_OBJECT
public:
    explicit ReaderActionBar(QWidget *parent = nullptr);

    /** Reply through the window's reply menu; the button's default follows the window's reply mode */
    void setReplyMenu(QMenu *menu);
    void setReplyAction(QAction *action);
    void setForwardAction(QAction *action);

    /** Follow the marks of a message; an invalid index means that no message is open */
    void setMessage(const QModelIndex &message);
    QModelIndex message() const;
    /** Whether the marks of the open message can be changed, which needs a connection */
    void setMarksEnabled(bool enabled);
    void setArchiveEnabled(bool enabled);
    /** Delete moves the message to Trash, rather than toggling its deleted mark */
    void setMovesToTrash(bool movesToTrash);
    void setAttachmentCount(int count);
    int attachmentCount() const;
    void setPreferPlainText(bool preferPlainText);
    bool isAttachmentChipShown() const;

signals:
    void archiveRequested();
    /** Move to Trash, or toggle the deleted mark, depending on setMovesToTrash() */
    void trashRequested();
    void markReadRequested(bool read);
    void flagRequested(bool flagged);
    void zoomInRequested();
    void zoomOutRequested();
    void zoomResetRequested();
    void preferPlainTextToggled(bool preferPlainText);
    void attachmentsRequested();

private:
    QToolButton *addButton(const QString &objectName, const QString &iconName, const QString &text);
    void addSeparator();
    void updateState();

    /** Buttons which do not fit go into the toolbar's overflow menu, so the bar never widens the reader */
    QToolBar *m_toolBar;
    QAction *m_attachmentsAction = nullptr;
    QPersistentModelIndex m_message;
    QList<QMetaObject::Connection> m_modelConnections;
    bool m_marksEnabled = false;
    bool m_archiveEnabled = false;
    bool m_movesToTrash = true;
    int m_attachmentCount = 0;

    QToolButton *m_reply;
    QToolButton *m_forward;
    QToolButton *m_archive;
    QToolButton *m_trash;
    QToolButton *m_read;
    QToolButton *m_flag;
    QToolButton *m_attachments;
    QToolButton *m_zoomOut;
    QToolButton *m_zoomReset;
    QToolButton *m_zoomIn;
    QToolButton *m_plainText;
};

}
