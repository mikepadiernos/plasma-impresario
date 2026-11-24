/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "ContactPanel.h"

#include <KLocalizedString>
#include <algorithm>
#include <QClipboard>
#include <QGuiApplication>
#include <QListWidget>
#include <QMenu>
#include <QToolButton>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Parser/MailAddress.h"

namespace Gui {

namespace {

constexpr int UrlRole = Qt::UserRole;
constexpr int AddressRole = Qt::UserRole + 1;
constexpr int MaximumRowsInSizeHint = 4;

}

EmbeddedPanel::Descriptor ContactPanel::panelDescriptor()
{
    Descriptor descriptor;
    descriptor.id = QStringLiteral("Contact");
    descriptor.configKey = QStringLiteral("ContactPanel");
    descriptor.title = i18nc("@title embedded panel", "Contacts");
    descriptor.autoHideToolTip = i18nc("@info:tooltip", "Show the contacts panel only while a message is shown");
    descriptor.defaultEnabled = false;
    descriptor.defaultPlacement = Placement::RightColumn;
    return descriptor;
}

ContactPanel::ContactPanel(QWidget *parent)
    : MessageContextPanel(panelDescriptor(), QStringLiteral("contactPanel"), parent)
    , m_list(new QListWidget(this))
{
    m_write = addHeaderButton(QStringLiteral("Write"), QStringLiteral("mail-message-new"), i18nc("@action:button", "Write Email"));
    m_copy = addHeaderButton(QStringLiteral("Copy"), QStringLiteral("edit-copy"), i18nc("@action:button", "Copy Address"));
    m_addressBook = addHeaderButton(QStringLiteral("AddressBook"), QStringLiteral("contact-new"),
                                    i18nc("@action:button", "Open in Address Book"));
    connect(m_write, &QToolButton::clicked, this, &ContactPanel::writeToCurrent);
    connect(m_copy, &QToolButton::clicked, this, &ContactPanel::copyCurrent);
    connect(m_addressBook, &QToolButton::clicked, this, &ContactPanel::openCurrentInAddressBook);

    m_list->setObjectName(QStringLiteral("contactPanelList"));
    m_list->setAccessibleName(i18nc("@label", "People in the current message"));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setTextElideMode(Qt::ElideMiddle);
    m_list->setUniformItemSizes(true);
    connect(m_list, &QListWidget::itemActivated, this, &ContactPanel::writeToCurrent);
    connect(m_list, &QListWidget::currentItemChanged, this, &ContactPanel::updateActions);
    connect(m_list, &QWidget::customContextMenuRequested, this, &ContactPanel::showContextMenu);
    setContentWidget(m_list);

    refresh();
}

bool ContactPanel::hasContent() const
{
    return m_list->count() > 0;
}

QWidget *ContactPanel::focusTarget() const
{
    return m_list;
}

QSize ContactPanel::sizeHint() const
{
    QSize hint = EmbeddedPanel::sizeHint();
    const int rows = qBound(1, m_list->count(), MaximumRowsInSizeHint);
    const int rowHeight = m_list->count() ? m_list->sizeHintForRow(0) : fontMetrics().height() * 2;
    hint.setHeight(chromeHeight() + rows * rowHeight + 2 * m_list->frameWidth());
    return hint;
}

int ContactPanel::contactCount() const
{
    return m_list->count();
}

QString ContactPanel::contactText(int row) const
{
    const auto *item = m_list->item(row);
    return item ? item->data(AddressRole).toString() : QString();
}

void ContactPanel::setAddressBookAvailable(bool available)
{
    m_addressBookAvailable = available;
    updateActions();
}

void ContactPanel::refresh()
{
    using namespace Imap::Mailbox;
    const QString previous = m_list->currentItem() ? m_list->currentItem()->data(AddressRole).toString() : QString();
    const int previousCount = m_list->count();
    m_list->clear();

    const QModelIndex index = message();
    if (!index.isValid()) {
        showPlaceholder(i18nc("@info", "No message selected"));
    } else if (!messageIsFetched()) {
        showPlaceholder(i18nc("@info", "Loading message…"));
    } else {
        struct Contact {
            Imap::Message::MailAddress address;
            QStringList roles;
        };
        QList<Contact> contacts;
        const auto collect = [&](int role, const QString &roleName) {
            for (const QVariant &entry : index.data(role).toList()) {
                const QStringList parts = entry.toStringList();
                if (parts.size() < 4 || parts.at(2).isEmpty() || parts.at(3).isEmpty())
                    continue; // group syntax markers and undisclosed recipients have no address
                const Imap::Message::MailAddress address(parts.at(0), parts.at(1), parts.at(2), parts.at(3));
                const QString key = QString::fromUtf8(address.asSMTPMailbox()).toCaseFolded();
                auto existing = std::find_if(contacts.begin(), contacts.end(), [&key](const Contact &contact) {
                    return QString::fromUtf8(contact.address.asSMTPMailbox()).toCaseFolded() == key;
                });
                if (existing == contacts.end()) {
                    contacts.append(Contact{address, {roleName}});
                } else {
                    if (!existing->roles.contains(roleName))
                        existing->roles.append(roleName);
                    if (existing->address.name.isEmpty())
                        existing->address.name = address.name;
                }
            }
        };
        collect(RoleMessageFrom, i18nc("@info contact role", "From"));
        collect(RoleMessageReplyTo, i18nc("@info contact role", "Reply-To"));
        collect(RoleMessageTo, i18nc("@info contact role", "To"));
        collect(RoleMessageCc, i18nc("@info contact role", "Cc"));

        for (const auto &contact : std::as_const(contacts)) {
            const QString pretty = contact.address.prettyName(Imap::Message::MailAddress::FORMAT_READABLE);
            const QString roles = contact.roles.join(i18nc("@info separator of contact roles", ", "));
            auto *item = new QListWidgetItem(QIcon::fromTheme(QStringLiteral("im-user")),
                                             i18nc("@item contact roles and address", "%1: %2", roles, pretty), m_list);
            item->setData(UrlRole, contact.address.asUrl());
            item->setData(AddressRole, pretty);
            item->setToolTip(pretty);
            if (pretty == previous)
                m_list->setCurrentItem(item);
        }
        if (!m_list->currentItem() && m_list->count())
            m_list->setCurrentRow(0);
        if (m_list->count())
            showContent();
        else
            showPlaceholder(i18nc("@info", "This message has no addresses"));
    }
    updateActions();
    if (previousCount != m_list->count())
        updateGeometry();
    updateVisibility();
}

QUrl ContactPanel::currentUrl() const
{
    const auto *item = m_list->currentItem();
    return item ? item->data(UrlRole).toUrl() : QUrl();
}

void ContactPanel::updateActions()
{
    const bool usable = currentUrl().isValid();
    m_write->setEnabled(usable);
    m_copy->setEnabled(usable);
    m_addressBook->setEnabled(usable && m_addressBookAvailable);
}

void ContactPanel::writeToCurrent()
{
    const QUrl url = currentUrl();
    if (url.isValid())
        emit composeRequested(url);
}

void ContactPanel::copyCurrent()
{
    const auto *item = m_list->currentItem();
    if (item)
        QGuiApplication::clipboard()->setText(item->data(AddressRole).toString());
}

void ContactPanel::openCurrentInAddressBook()
{
    const QUrl url = currentUrl();
    if (url.isValid() && m_addressBookAvailable)
        emit manageContactRequested(QUrl(QStringLiteral("x-trojita-manage-contact:") + url.toString(QUrl::RemoveScheme)));
}

void ContactPanel::showContextMenu(const QPoint &pos)
{
    auto *item = m_list->itemAt(pos);
    if (!item)
        return;
    m_list->setCurrentItem(item);
    QMenu menu(this);
    menu.addAction(themedIcon(QStringLiteral("mail-message-new")), i18nc("@action:inmenu", "Write Email"), this,
                   &ContactPanel::writeToCurrent);
    menu.addAction(themedIcon(QStringLiteral("edit-copy")), i18nc("@action:inmenu", "Copy Address"), this, &ContactPanel::copyCurrent);
    auto *addressBook = menu.addAction(themedIcon(QStringLiteral("contact-new")), i18nc("@action:inmenu", "Open in Address Book"), this,
                                       &ContactPanel::openCurrentInAddressBook);
    addressBook->setEnabled(m_addressBookAvailable);
    menu.exec(m_list->viewport()->mapToGlobal(pos));
}

}
