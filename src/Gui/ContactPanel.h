/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QHash>
#include <QList>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QSet>
#include <QUrl>

#include "EmbeddedPanel.h"
#include "Plugins/AddressbookPlugin.h"

class QListWidget;
class QListWidgetItem;
class QToolButton;

namespace Gui {

/** @short Embedded panel with the people involved in the message shown in the reader

Lists the sender, Reply-To, To and Cc addresses once each, together with their roles,
and offers to write to them, copy them, or open them in the address book.

When the address book plugin can describe contacts, each address is looked up there
asynchronously; known contacts show their name, organization and stored photo, and
unknown addresses can be added. Lookups are cancelled when the message changes.
*/
class ContactPanel : public MessageContextPanel
{
    Q_OBJECT
public:
    explicit ContactPanel(QWidget *parent = nullptr);

    static Descriptor panelDescriptor();

    bool hasContent() const override;
    QWidget *focusTarget() const override;

    int contactCount() const;
    /** The "Name <mailbox@host>" form of the contact in @arg row */
    QString contactText(int row) const;
    /** Whether the address book has a contact for the address in @arg row, as far as known yet */
    bool contactIsKnown(int row) const;
    /** Whether address book lookups are still running for the current message */
    bool lookupsPending() const;

public slots:
    void setAddressBookAvailable(bool available);
    /** Use @arg addressBook for opening, adding and describing contacts; nullptr means none */
    void setAddressBook(Plugins::AddressbookPlugin *addressBook);
    void writeToCurrent();
    void copyCurrent();
    void openCurrentInAddressBook();
    void addCurrentToAddressBook();

signals:
    /** Compose a message to a mailto: URL */
    void composeRequested(const QUrl &url);
    /** Open the address book for an x-trojita-manage-contact: URL */
    void manageContactRequested(const QUrl &url);
    /** Add the address of an x-trojita-manage-contact: URL to the address book */
    void addContactRequested(const QUrl &url);

protected:
    void refresh() override;

private:
    void updateActions();
    void showContextMenu(const QPoint &pos);
    QUrl currentUrl() const;
    QUrl currentManageUrl() const;
    bool canLookUp() const;
    bool canAdd() const;
    bool currentCanBeAdded() const;
    void cancelLookups();
    void lookUp(const QString &key, const QString &email, bool force = false);
    void applyDetails(QListWidgetItem *item);

    QListWidget *m_list;
    QToolButton *m_write;
    QToolButton *m_copy;
    QToolButton *m_addressBook;
    bool m_addressBookAvailable = false;
    QPointer<Plugins::AddressbookPlugin> m_addressBookPlugin;
    /** The message whose addresses were looked up, so a change of message drops the results */
    QPersistentModelIndex m_lookupMessage;
    /** Results by case-folded address */
    QHash<QString, Plugins::ContactDetails> m_details;
    /** Addresses whose lookup failed, which are shown from the headers only */
    QSet<QString> m_failed;
    QHash<QString, QPointer<Plugins::AddressbookContactJob>> m_jobs;
    quint64 m_generation = 0;
};

}
