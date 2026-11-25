/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "ReaderActionBar.h"

#include <KLocalizedString>
#include <QAbstractItemModel>
#include <QAction>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>

#include "Imap/Model/ItemRoles.h"
#include "UiUtils/IconLoader.h"

namespace Gui {

namespace {

/** Toolbar-sized buttons use the -symbolic variant where the theme has one */
QIcon barIcon(const QString &name)
{
    const QString symbolic = name + QLatin1String("-symbolic");
    return QIcon::hasThemeIcon(symbolic) ? QIcon::fromTheme(symbolic) : UiUtils::loadIcon(name);
}

}

ReaderActionBar::ReaderActionBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("readerActions"));
    setAccessibleName(i18nc("@info:whatsthis", "Message actions"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_toolBar = new QToolBar(this);
    m_toolBar->setObjectName(QStringLiteral("readerActionsToolBar"));
    m_toolBar->setMovable(false);
    m_toolBar->setFloatable(false);
    const int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
    m_toolBar->setIconSize(QSize(iconSize, iconSize));
    layout->addWidget(m_toolBar);

    m_reply = addButton(QStringLiteral("readerReply"), QStringLiteral("mail-reply-sender"), i18nc("@action:button", "Reply"));
    m_reply->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_forward = addButton(QStringLiteral("readerForward"), QStringLiteral("mail-forward"), i18nc("@action:button", "Forward"));
    addSeparator();

    m_archive = addButton(QStringLiteral("readerArchive"), QStringLiteral("mail-move"), i18nc("@action:button", "Archive"));
    connect(m_archive, &QToolButton::clicked, this, &ReaderActionBar::archiveRequested);
    m_trash = addButton(QStringLiteral("readerTrash"), QStringLiteral("user-trash"), i18nc("@action:button", "Move to Trash"));
    connect(m_trash, &QToolButton::clicked, this, &ReaderActionBar::trashRequested);
    m_read = addButton(QStringLiteral("readerRead"), QStringLiteral("mail-mark-read"), i18nc("@action:button", "Read"));
    m_read->setCheckable(true);
    // The buttons show the marks of the message, which change once the server has the request
    connect(m_read, &QToolButton::clicked, this, [this](bool read) {
        emit markReadRequested(read);
        updateState();
    });
    m_flag = addButton(QStringLiteral("readerFlag"), QStringLiteral("mail-mark-important"), i18nc("@action:button", "Flag"));
    m_flag->setCheckable(true);
    connect(m_flag, &QToolButton::clicked, this, [this](bool flagged) {
        emit flagRequested(flagged);
        updateState();
    });

    auto *spacer = new QWidget(m_toolBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolBar->addWidget(spacer);

    m_attachments = addButton(QStringLiteral("readerAttachments"), QStringLiteral("mail-attachment"), QString());
    m_attachmentsAction = m_toolBar->actions().constLast();
    m_attachments->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_attachments->setToolTip(i18nc("@info:tooltip", "Show the attachments of this message"));
    m_attachmentsAction->setVisible(false);
    connect(m_attachments, &QToolButton::clicked, this, &ReaderActionBar::attachmentsRequested);
    addSeparator();

    m_zoomOut = addButton(QStringLiteral("readerZoomOut"), QStringLiteral("zoom-out"), i18nc("@action:button", "Zoom Out"));
    connect(m_zoomOut, &QToolButton::clicked, this, &ReaderActionBar::zoomOutRequested);
    m_zoomReset = addButton(QStringLiteral("readerZoomReset"), QStringLiteral("zoom-original"), i18nc("@action:button", "Reset Zoom"));
    connect(m_zoomReset, &QToolButton::clicked, this, &ReaderActionBar::zoomResetRequested);
    m_zoomIn = addButton(QStringLiteral("readerZoomIn"), QStringLiteral("zoom-in"), i18nc("@action:button", "Zoom In"));
    connect(m_zoomIn, &QToolButton::clicked, this, &ReaderActionBar::zoomInRequested);
    m_plainText = addButton(QStringLiteral("readerPlainText"), QStringLiteral("text-plain"), i18nc("@action:button", "Prefer Plain Text"));
    m_plainText->setCheckable(true);
    m_plainText->setToolTip(i18nc("@info:tooltip", "Show the plain-text version of messages which also have formatted text"));
    connect(m_plainText, &QToolButton::toggled, this, &ReaderActionBar::preferPlainTextToggled);

    updateState();
}

QToolButton *ReaderActionBar::addButton(const QString &objectName, const QString &iconName, const QString &text)
{
    auto *button = new QToolButton(m_toolBar);
    button->setObjectName(objectName);
    button->setAutoRaise(true);
    button->setIcon(barIcon(iconName));
    button->setText(text);
    button->setToolTip(text);
    button->setAccessibleName(text);
    m_toolBar->addWidget(button);
    return button;
}

void ReaderActionBar::addSeparator()
{
    m_toolBar->addSeparator();
}

void ReaderActionBar::setReplyMenu(QMenu *menu)
{
    m_reply->setMenu(menu);
    m_reply->setPopupMode(QToolButton::MenuButtonPopup);
}

void ReaderActionBar::setReplyAction(QAction *action)
{
    if (m_reply->defaultAction() == action)
        return;
    QMenu *menu = m_reply->menu();
    m_reply->setDefaultAction(action);
    // The default action replaces the button's menu
    if (menu)
        setReplyMenu(menu);
}

void ReaderActionBar::setForwardAction(QAction *action)
{
    m_forward->setDefaultAction(action);
    m_forward->setToolButtonStyle(Qt::ToolButtonIconOnly);
}

void ReaderActionBar::setMessage(const QModelIndex &message)
{
    if (QPersistentModelIndex(message) == m_message && !m_modelConnections.isEmpty())
        return;
    for (const auto &connection : std::as_const(m_modelConnections))
        disconnect(connection);
    m_modelConnections.clear();
    m_message = message;
    if (const QAbstractItemModel *model = message.model()) {
        // The marks change on the server, from the list or from this bar
        m_modelConnections << connect(model, &QAbstractItemModel::dataChanged, this,
                                      [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
            if (m_message.isValid() && topLeft.parent() == m_message.parent() && topLeft.row() <= m_message.row()
                && m_message.row() <= bottomRight.row())
                updateState();
        });
        m_modelConnections << connect(model, &QAbstractItemModel::rowsRemoved, this, &ReaderActionBar::updateState);
        m_modelConnections << connect(model, &QAbstractItemModel::modelReset, this, &ReaderActionBar::updateState);
    }
    updateState();
}

QModelIndex ReaderActionBar::message() const
{
    return m_message;
}

void ReaderActionBar::setMarksEnabled(bool enabled)
{
    m_marksEnabled = enabled;
    updateState();
}

void ReaderActionBar::setArchiveEnabled(bool enabled)
{
    m_archiveEnabled = enabled;
    updateState();
}

void ReaderActionBar::setMovesToTrash(bool movesToTrash)
{
    m_movesToTrash = movesToTrash;
    updateState();
}

void ReaderActionBar::setAttachmentCount(int count)
{
    m_attachmentCount = count;
    m_attachments->setText(i18ncp("@action:button", "%1 attachment", "%1 attachments", count));
    m_attachments->setAccessibleName(m_attachments->text());
    updateState();
}

int ReaderActionBar::attachmentCount() const
{
    return m_attachmentCount;
}

void ReaderActionBar::setPreferPlainText(bool preferPlainText)
{
    const QSignalBlocker blocker(m_plainText);
    m_plainText->setChecked(preferPlainText);
}

bool ReaderActionBar::isAttachmentChipShown() const
{
    return m_attachmentsAction->isVisible();
}

void ReaderActionBar::updateState()
{
    const bool hasMessage = m_message.isValid();
    const bool read = hasMessage && m_message.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool();
    const bool flagged = hasMessage && m_message.data(Imap::Mailbox::RoleMessageIsMarkedFlagged).toBool();
    const bool deleted = hasMessage && m_message.data(Imap::Mailbox::RoleMessageIsMarkedDeleted).toBool();
    const bool marks = hasMessage && m_marksEnabled;

    m_read->setChecked(read);
    m_read->setEnabled(marks);
    m_read->setToolTip(read ? i18nc("@info:tooltip", "Mark this message as unread") : i18nc("@info:tooltip", "Mark this message as read"));
    m_flag->setChecked(flagged);
    m_flag->setEnabled(marks);
    m_flag->setToolTip(flagged ? i18nc("@info:tooltip", "Remove the flag from this message") : i18nc("@info:tooltip", "Flag this message"));

    QString trashText;
    QString trashIcon = QStringLiteral("user-trash");
    if (m_movesToTrash) {
        trashText = i18nc("@action:button", "Move to Trash");
    } else if (deleted) {
        trashText = i18nc("@action:button", "Undelete");
        trashIcon = QStringLiteral("edit-undo");
    } else {
        trashText = i18nc("@action:button", "Delete");
    }
    m_trash->setText(trashText);
    m_trash->setToolTip(trashText);
    m_trash->setAccessibleName(trashText);
    m_trash->setIcon(barIcon(trashIcon));
    m_trash->setEnabled(marks);
    m_archive->setEnabled(marks && m_archiveEnabled);

    m_attachmentsAction->setVisible(hasMessage && m_attachmentCount > 0);
    for (QToolButton *button : {m_zoomOut, m_zoomReset, m_zoomIn, m_plainText})
        button->setEnabled(hasMessage);
}

}
