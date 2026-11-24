/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "RelatedMessagesPanel.h"

#include <KLocalizedString>
#include <QAbstractItemModel>
#include <QDateTime>
#include <QHeaderView>
#include <QLocale>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Parser/MailAddress.h"
#include "UiUtils/RelatedMessages.h"

namespace Gui {

namespace {

constexpr int MessageRole = Qt::UserRole;
/** The message list changes often while a mailbox syncs, so coalesce its signals */
constexpr int ListRefreshDelay = 250;

}

EmbeddedPanel::Descriptor RelatedMessagesPanel::panelDescriptor()
{
    Descriptor descriptor;
    descriptor.id = QStringLiteral("RelatedMessages");
    descriptor.configKey = QStringLiteral("RelatedMessagesPanel");
    descriptor.title = i18nc("@title embedded panel", "Related Messages");
    descriptor.autoHideToolTip = i18nc("@info:tooltip", "Show the related messages panel only while it has messages to list");
    descriptor.defaultEnabled = false;
    descriptor.defaultPlacement = Placement::RightColumn;
    return descriptor;
}

RelatedMessagesPanel::RelatedMessagesPanel(QWidget *parent)
    : MessageContextPanel(panelDescriptor(), QStringLiteral("relatedMessagesPanel"), parent)
    , m_tree(new QTreeWidget(this))
    , m_listTimer(new QTimer(this))
{
    m_tree->setObjectName(QStringLiteral("relatedMessagesPanelList"));
    m_tree->setAccessibleName(i18nc("@label", "Messages related to the current message"));
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setTextElideMode(Qt::ElideRight);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_tree, &QTreeWidget::itemActivated, this, &RelatedMessagesPanel::activateCurrent);
    setContentWidget(m_tree);
    setPreferredRows(5);

    m_listTimer->setSingleShot(true);
    m_listTimer->setInterval(ListRefreshDelay);
    connect(m_listTimer, &QTimer::timeout, this, &RelatedMessagesPanel::refresh);

    refresh();
}

void RelatedMessagesPanel::setMessageList(QAbstractItemModel *messages)
{
    for (const auto &connection : std::as_const(m_listConnections))
        disconnect(connection);
    m_listConnections.clear();
    m_messages = messages;
    if (messages) {
        m_listConnections << connect(messages, &QAbstractItemModel::rowsInserted, this, &RelatedMessagesPanel::scheduleListRefresh)
                          << connect(messages, &QAbstractItemModel::rowsRemoved, this, &RelatedMessagesPanel::scheduleListRefresh)
                          << connect(messages, &QAbstractItemModel::modelReset, this, &RelatedMessagesPanel::scheduleListRefresh)
                          << connect(messages, &QAbstractItemModel::layoutChanged, this, &RelatedMessagesPanel::scheduleListRefresh)
                          << connect(messages, &QAbstractItemModel::dataChanged, this, &RelatedMessagesPanel::scheduleListRefresh);
    }
    refresh();
}

void RelatedMessagesPanel::panelEnabledChanged()
{
    refresh();
}

void RelatedMessagesPanel::scheduleListRefresh()
{
    if (isPanelEnabled() && !m_listTimer->isActive())
        m_listTimer->start();
}

bool RelatedMessagesPanel::hasContent() const
{
    return !m_conversation.isEmpty() || !m_sameSender.isEmpty();
}

QWidget *RelatedMessagesPanel::focusTarget() const
{
    return m_tree;
}

QList<QPersistentModelIndex> RelatedMessagesPanel::conversation() const
{
    return m_conversation;
}

QList<QPersistentModelIndex> RelatedMessagesPanel::sameSender() const
{
    return m_sameSender;
}

void RelatedMessagesPanel::refresh()
{
    m_listTimer->stop();
    const QModelIndex current = message();
    // Scanning the list is the expensive part, so a turned-off panel does not do it.
    const auto related = isPanelEnabled() ? UiUtils::collectRelatedMessages(m_messages.data(), current) : UiUtils::RelatedMessages();
    m_conversation = related.conversation;
    m_sameSender = related.sameSender;

    const QPersistentModelIndex selected =
        m_tree->currentItem() ? m_tree->currentItem()->data(0, MessageRole).toPersistentModelIndex() : QPersistentModelIndex();
    m_tree->clear();
    if (!m_conversation.isEmpty()) {
        auto *group = new QTreeWidgetItem(m_tree, {i18ncp("@title:group related messages", "Same Conversation (%1)",
                                                          "Same Conversation (%1)", m_conversation.size())});
        fillGroup(group, m_conversation);
    }
    if (!m_sameSender.isEmpty()) {
        auto *group = new QTreeWidgetItem(m_tree, {i18ncp("@title:group related messages", "From the Same Sender (%1)",
                                                          "From the Same Sender (%1)", m_sameSender.size())});
        fillGroup(group, m_sameSender);
    }
    m_tree->expandAll();
    if (selected.isValid()) {
        for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
            if ((*it)->data(0, MessageRole).toPersistentModelIndex() == selected) {
                m_tree->setCurrentItem(*it);
                break;
            }
        }
    }

    if (hasContent())
        showContent();
    else if (!current.isValid())
        showPlaceholder(i18nc("@info", "No message selected"));
    else if (!messageIsFetched())
        showPlaceholder(i18nc("@info", "Loading message…"));
    else
        showPlaceholder(i18nc("@info", "No related messages in this folder"));
    updateVisibility();
}

void RelatedMessagesPanel::fillGroup(QTreeWidgetItem *group, const QList<QPersistentModelIndex> &messages)
{
    QFont font = group->font(0);
    font.setBold(true);
    group->setFont(0, font);
    group->setFlags(Qt::ItemIsEnabled);
    for (const auto &message : messages) {
        QString subject = message.data(Imap::Mailbox::RoleMessageSubject).toString();
        if (subject.isEmpty())
            subject = i18nc("@info message without a subject", "(no subject)");
        const QString from = Imap::Message::MailAddress::prettyList(message.data(Imap::Mailbox::RoleMessageFrom).toList(),
                                                                    Imap::Message::MailAddress::FORMAT_JUST_NAME);
        const QDateTime date = message.data(Imap::Mailbox::RoleMessageDate).toDateTime();
        const QString when = date.isValid() ? QLocale().toString(date.toLocalTime().date(), QLocale::ShortFormat) : QString();
        auto *item = new QTreeWidgetItem(group, {when.isEmpty() ? subject : i18nc("@item subject and date", "%1 — %2", subject, when)});
        item->setData(0, MessageRole, QVariant::fromValue(message));
        item->setToolTip(0, from.isEmpty() ? subject : i18nc("@info:tooltip subject and sender", "%1\nFrom: %2", subject, from));
        if (!message.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool()) {
            QFont unread = item->font(0);
            unread.setBold(true);
            item->setFont(0, unread);
        }
    }
}

void RelatedMessagesPanel::activateCurrent()
{
    const auto *item = m_tree->currentItem();
    if (!item)
        return;
    const QPersistentModelIndex message = item->data(0, MessageRole).toPersistentModelIndex();
    if (message.isValid())
        emit messageActivated(message);
}

}
