/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QUrl>

#include "EmbeddedPanel.h"

class QListWidget;
class QToolButton;

namespace Gui {

/** @short Embedded panel with the people involved in the message shown in the reader

Lists the sender, Reply-To, To and Cc addresses once each, together with their roles,
and offers to write to them, copy them, or open them in the address book.
*/
class ContactPanel : public MessageContextPanel
{
    Q_OBJECT
public:
    explicit ContactPanel(QWidget *parent = nullptr);

    static Descriptor panelDescriptor();

    bool hasContent() const override;
    QWidget *focusTarget() const override;
    QSize sizeHint() const override;

    int contactCount() const;
    /** The "Name <mailbox@host>" form of the contact in @arg row */
    QString contactText(int row) const;

public slots:
    void setAddressBookAvailable(bool available);
    void writeToCurrent();
    void copyCurrent();
    void openCurrentInAddressBook();

signals:
    /** Compose a message to a mailto: URL */
    void composeRequested(const QUrl &url);
    /** Open the address book for an x-trojita-manage-contact: URL */
    void manageContactRequested(const QUrl &url);

protected:
    void refresh() override;

private:
    void updateActions();
    void showContextMenu(const QPoint &pos);
    QUrl currentUrl() const;

    QListWidget *m_list;
    QToolButton *m_write;
    QToolButton *m_copy;
    QToolButton *m_addressBook;
    bool m_addressBookAvailable = false;
};

}
