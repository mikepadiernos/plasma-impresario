/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "ContactPanel.h"

#include <KLocalizedString>
#include <algorithm>
#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QPixmap>
#include <QListWidget>
#include <QMenu>
#include <QToolButton>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Parser/MailAddress.h"

namespace Gui {

namespace {

constexpr int UrlRole = Qt::UserRole;
constexpr int AddressRole = Qt::UserRole + 1;
/** The case-folded mailbox@host, which identifies the contact */
constexpr int KeyRole = Qt::UserRole + 2;
/** Name, source route, mailbox and host from the message headers */
constexpr int PartsRole = Qt::UserRole + 3;
constexpr int RolesRole = Qt::UserRole + 4;
constexpr int KnownRole = Qt::UserRole + 5;
constexpr int PhotoSize = 64;

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
    m_addressBook = addHeaderButton(QStringLiteral("AddressBook"), QStringLiteral("view-pim-contacts"),
                                    i18nc("@action:button", "Open in Address Book"));
    connect(m_write, &QToolButton::clicked, this, &ContactPanel::writeToCurrent);
    connect(m_copy, &QToolButton::clicked, this, &ContactPanel::copyCurrent);
    connect(m_addressBook, &QToolButton::clicked, this, [this]() {
        if (currentCanBeAdded())
            addCurrentToAddressBook();
        else
            openCurrentInAddressBook();
    });

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
    setPreferredRows(4);

    ContactPanel::refresh();
}

bool ContactPanel::hasContent() const
{
    return m_list->count() > 0;
}

QWidget *ContactPanel::focusTarget() const
{
    return m_list;
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

bool ContactPanel::contactIsKnown(int row) const
{
    const auto *item = m_list->item(row);
    return item && item->data(KnownRole).toBool();
}

bool ContactPanel::lookupsPending() const
{
    return std::any_of(m_jobs.cbegin(), m_jobs.cend(), [](const auto &job) { return !job.isNull(); });
}

void ContactPanel::setAddressBookAvailable(bool available)
{
    m_addressBookAvailable = available;
    updateActions();
}

void ContactPanel::setAddressBook(Plugins::AddressbookPlugin *addressBook)
{
    m_addressBookAvailable = addressBook != nullptr;
    if (m_addressBookPlugin == addressBook) {
        updateActions();
        return;
    }
    if (m_addressBookPlugin)
        disconnect(m_addressBookPlugin, nullptr, this, nullptr);
    cancelLookups();
    m_details.clear();
    m_failed.clear();
    m_addressBookPlugin = addressBook;
    if (addressBook) {
        connect(addressBook, &Plugins::AddressbookPlugin::contactsChanged, this, [this]() {
            // Keep showing the previous details until the new ones arrive
            cancelLookups();
            m_failed.clear();
            if (!canLookUp())
                return;
            for (int row = 0; row < m_list->count(); ++row) {
                const QStringList parts = m_list->item(row)->data(PartsRole).toStringList();
                lookUp(m_list->item(row)->data(KeyRole).toString(), parts.at(2) + QLatin1Char('@') + parts.at(3), true);
            }
        });
    }
    refresh();
}

bool ContactPanel::canLookUp() const
{
    return m_addressBookPlugin && m_addressBookPlugin->features().testFlag(Plugins::AddressbookPlugin::FeatureContactDetails);
}

bool ContactPanel::canAdd() const
{
    return m_addressBookAvailable && m_addressBookPlugin
        && m_addressBookPlugin->features().testFlag(Plugins::AddressbookPlugin::FeatureAddContact);
}

bool ContactPanel::currentCanBeAdded() const
{
    const auto *item = m_list->currentItem();
    if (!item || !canLookUp() || !canAdd())
        return false;
    const auto found = m_details.constFind(item->data(KeyRole).toString());
    return found != m_details.constEnd() && !found->known;
}

void ContactPanel::cancelLookups()
{
    ++m_generation;
    for (const auto &job : std::as_const(m_jobs)) {
        if (job) {
            disconnect(job, nullptr, this, nullptr);
            job->stop();
        }
    }
    m_jobs.clear();
}

void ContactPanel::lookUp(const QString &key, const QString &email, bool force)
{
    if (!canLookUp() || m_jobs.contains(key) || m_failed.contains(key) || (!force && m_details.contains(key)))
        return;
    auto *job = m_addressBookPlugin->requestContactDetails(email);
    if (!job)
        return;
    job->setAutoDelete(true);
    const quint64 generation = m_generation;
    connect(job, &Plugins::AddressbookContactJob::contactDetailsAvailable, this,
            [this, key, generation](const Plugins::ContactDetails &details) {
                if (generation != m_generation)
                    return;
                m_jobs.remove(key);
                m_details.insert(key, details);
                for (int row = 0; row < m_list->count(); ++row) {
                    if (m_list->item(row)->data(KeyRole).toString() == key)
                        applyDetails(m_list->item(row));
                }
                updateActions();
            });
    connect(job, &Plugins::AddressbookJob::error, this, [this, key, generation](Plugins::AddressbookJob::Error) {
        if (generation != m_generation)
            return;
        m_jobs.remove(key);
        // Not retried for this message, so a failing address book is not asked again on every flag change
        m_failed.insert(key);
        updateActions();
    });
    m_jobs.insert(key, job);
    job->start();
}

void ContactPanel::applyDetails(QListWidgetItem *item)
{
    const QStringList parts = item->data(PartsRole).toStringList();
    Imap::Message::MailAddress address(parts.at(0), parts.at(1), parts.at(2), parts.at(3));
    const auto found = m_details.constFind(item->data(KeyRole).toString());
    const bool lookedUp = found != m_details.constEnd();
    const bool known = lookedUp && found->known;
    if (known && !found->name.isEmpty())
        address.name = found->name;
    const QString pretty = address.prettyName(Imap::Message::MailAddress::FORMAT_READABLE);
    const QString roles = item->data(RolesRole).toString();
    const QString organization = known ? found->organization : QString();
    const QString text = organization.isEmpty()
        ? i18nc("@item contact roles and address", "%1: %2", roles, pretty)
        : i18nc("@item contact roles, address and organization", "%1: %2 (%3)", roles, pretty, organization);

    bool hasPhoto = false;
    QIcon icon;
    if (known && !found->photo.isEmpty()) {
        const QImage image = QImage::fromData(found->photo);
        if (!image.isNull()) {
            icon = QIcon(QPixmap::fromImage(image.width() > PhotoSize || image.height() > PhotoSize
                                                ? image.scaled(PhotoSize, PhotoSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                                                : image));
            hasPhoto = true;
        }
    }
    if (!hasPhoto)
        icon = QIcon::fromTheme(QStringLiteral("im-user"));

    QString description;
    if (known && hasPhoto)
        description = i18nc("@info accessible description of a contact", "In address book, with photo from address book");
    else if (known)
        description = i18nc("@info accessible description of a contact", "In address book");
    else if (lookedUp)
        description = i18nc("@info accessible description of a contact", "Not in address book");

    item->setIcon(icon);
    item->setText(text);
    item->setData(AddressRole, pretty);
    item->setData(KnownRole, known);
    item->setData(Qt::AccessibleTextRole, text);
    item->setData(Qt::AccessibleDescriptionRole, description);
    item->setToolTip(organization.isEmpty() ? pretty : i18nc("@info:tooltip contact address and organization", "%1\n%2", pretty, organization));
}

void ContactPanel::refresh()
{
    using namespace Imap::Mailbox;
    const QString previous = m_list->currentItem() ? m_list->currentItem()->data(KeyRole).toString() : QString();
    m_list->clear();

    const QModelIndex index = message();
    if (m_lookupMessage != index) {
        cancelLookups();
        m_details.clear();
        m_failed.clear();
        m_lookupMessage = index;
    }
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
        const auto mailboxes = [&index](int role) {
            QStringList result;
            for (const QVariant &entry : index.data(role).toList()) {
                const QStringList parts = entry.toStringList();
                if (parts.size() >= 4 && !parts.at(2).isEmpty() && !parts.at(3).isEmpty())
                    result << QString(parts.at(2) + QLatin1Char('@') + parts.at(3)).toCaseFolded();
            }
            result.sort();
            return result;
        };
        collect(RoleMessageFrom, i18nc("@info contact role", "From"));
        // IMAP servers report the sender as Reply-To when the header is missing (RFC 3501), so only
        // list Reply-To when it names someone else.
        if (mailboxes(RoleMessageReplyTo) != mailboxes(RoleMessageFrom))
            collect(RoleMessageReplyTo, i18nc("@info contact role", "Reply-To"));
        collect(RoleMessageTo, i18nc("@info contact role", "To"));
        collect(RoleMessageCc, i18nc("@info contact role", "Cc"));

        for (const auto &contact : std::as_const(contacts)) {
            const Imap::Message::MailAddress &address = contact.address;
            const QString email = address.mailbox + QLatin1Char('@') + address.host;
            const QString key = QString::fromUtf8(address.asSMTPMailbox()).toCaseFolded();
            auto *item = new QListWidgetItem(m_list);
            item->setData(UrlRole, address.asUrl());
            item->setData(KeyRole, key);
            item->setData(PartsRole, QStringList{address.name, address.adl, address.mailbox, address.host});
            item->setData(RolesRole, contact.roles.join(i18nc("@info separator of contact roles", ", ")));
            applyDetails(item);
            if (key == previous)
                m_list->setCurrentItem(item);
            lookUp(key, email);
        }
        if (!m_list->currentItem() && m_list->count())
            m_list->setCurrentRow(0);
        if (m_list->count())
            showContent();
        else
            showPlaceholder(i18nc("@info", "This message has no addresses"));
    }
    updateActions();
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
    const bool add = usable && currentCanBeAdded();
    const QString text = add ? i18nc("@action:button", "Add to Address Book") : i18nc("@action:button", "Open in Address Book");
    m_addressBook->setIcon(themedIcon(add ? QStringLiteral("contact-new") : QStringLiteral("view-pim-contacts")));
    m_addressBook->setText(text);
    m_addressBook->setToolTip(text);
    m_addressBook->setAccessibleName(text);
    m_addressBook->setEnabled(usable && m_addressBookAvailable);
}

QUrl ContactPanel::currentManageUrl() const
{
    const QUrl url = currentUrl();
    return url.isValid() ? QUrl(QStringLiteral("x-trojita-manage-contact:") + url.toString(QUrl::RemoveScheme)) : QUrl();
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
        emit manageContactRequested(currentManageUrl());
}

void ContactPanel::addCurrentToAddressBook()
{
    if (currentCanBeAdded())
        emit addContactRequested(currentManageUrl());
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
    auto *addressBook = menu.addAction(themedIcon(QStringLiteral("view-pim-contacts")), i18nc("@action:inmenu", "Open in Address Book"),
                                       this, &ContactPanel::openCurrentInAddressBook);
    addressBook->setObjectName(QStringLiteral("contactPanelOpenInAddressBook"));
    addressBook->setEnabled(m_addressBookAvailable);
    if (canLookUp() && canAdd()) {
        auto *add = menu.addAction(themedIcon(QStringLiteral("contact-new")), i18nc("@action:inmenu", "Add to Address Book"), this,
                                   &ContactPanel::addCurrentToAddressBook);
        add->setObjectName(QStringLiteral("contactPanelAddToAddressBook"));
        add->setEnabled(currentCanBeAdded());
    }
    menu.exec(m_list->viewport()->mapToGlobal(pos));
}

}
