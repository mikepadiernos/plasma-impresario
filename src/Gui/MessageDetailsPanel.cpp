/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "MessageDetailsPanel.h"

#include <KLocalizedString>
#include <QDateTime>
#include <QFormLayout>
#include <QLabel>
#include <QLocale>
#include <QScrollArea>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Parser/MailAddress.h"
#include "UiUtils/Formatting.h"

namespace Gui {

EmbeddedPanel::Descriptor MessageDetailsPanel::panelDescriptor()
{
    Descriptor descriptor;
    descriptor.id = QStringLiteral("MessageDetails");
    descriptor.configKey = QStringLiteral("MessageDetailsPanel");
    descriptor.title = i18nc("@title embedded panel", "Message Details");
    descriptor.autoHideToolTip = i18nc("@info:tooltip", "Show the message details panel only while a message is shown");
    descriptor.defaultEnabled = false;
    descriptor.defaultPlacement = Placement::RightColumn;
    return descriptor;
}

MessageDetailsPanel::MessageDetailsPanel(QWidget *parent)
    : MessageContextPanel(panelDescriptor(), QStringLiteral("messageDetailsPanel"), parent)
    , m_scroll(new QScrollArea(this))
    , m_form(new QFormLayout)
{
    m_scroll->setObjectName(QStringLiteral("messageDetailsPanelContent"));
    m_scroll->setAccessibleName(i18nc("@label", "Details of the current message"));
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setFocusPolicy(Qt::StrongFocus);
    auto *form = new QWidget(m_scroll);
    m_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setLayout(m_form);
    m_scroll->setWidget(form);

    m_subject = addField(QStringLiteral("Subject"), i18nc("@label message details", "Subject:"));
    m_from = addField(QStringLiteral("From"), i18nc("@label message details", "From:"));
    m_to = addField(QStringLiteral("To"), i18nc("@label message details", "To:"));
    m_cc = addField(QStringLiteral("Cc"), i18nc("@label message details", "Cc:"));
    m_date = addField(QStringLiteral("Date"), i18nc("@label message details", "Date:"));
    m_size = addField(QStringLiteral("Size"), i18nc("@label message details", "Size:"));
    m_folder = addField(QStringLiteral("Folder"), i18nc("@label message details", "Folder:"));
    m_status = addField(QStringLiteral("Status"), i18nc("@label message details", "Status:"));
    m_security = addField(QStringLiteral("Security"), i18nc("@label message details", "Security:"));
    setContentWidget(m_scroll);
    setPreferredRows(5);

    refresh();
}

QLabel *MessageDetailsPanel::addField(const QString &field, const QString &label)
{
    auto *value = new QLabel;
    value->setObjectName(objectName() + field);
    value->setWordWrap(true);
    value->setTextFormat(Qt::PlainText);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *title = new QLabel(label);
    title->setBuddy(value);
    m_form->addRow(title, value);
    return value;
}

void MessageDetailsPanel::setField(QLabel *value, const QString &text)
{
    value->setText(text);
    m_form->setRowVisible(value, !text.isEmpty());
}

QString MessageDetailsPanel::fieldText(const QString &field) const
{
    const auto *value = findChild<QLabel *>(objectName() + field);
    return value && m_form->isRowVisible(const_cast<QLabel *>(value)) ? value->text() : QString();
}

bool MessageDetailsPanel::hasContent() const
{
    return m_hasContent;
}

QWidget *MessageDetailsPanel::focusTarget() const
{
    return m_scroll;
}

void MessageDetailsPanel::refresh()
{
    using namespace Imap::Mailbox;
    const QModelIndex index = message();
    m_hasContent = messageIsFetched();
    if (!index.isValid()) {
        showPlaceholder(i18nc("@info", "No message selected"));
    } else if (!m_hasContent) {
        showPlaceholder(i18nc("@info", "Loading message…"));
    } else {
        const auto addresses = [&index](int role) {
            return Imap::Message::MailAddress::prettyList(index.data(role).toList(), Imap::Message::MailAddress::FORMAT_READABLE);
        };
        const QString subject = index.data(RoleMessageSubject).toString();
        setField(m_subject, subject.isEmpty() ? i18nc("@info message without a subject", "(no subject)") : subject);
        setField(m_from, addresses(RoleMessageFrom));
        setField(m_to, addresses(RoleMessageTo));
        setField(m_cc, addresses(RoleMessageCc));
        const QDateTime date = index.data(RoleMessageDate).toDateTime();
        setField(m_date, date.isValid() ? QLocale().toString(date.toLocalTime(), QLocale::LongFormat) : QString());
        const QVariant size = index.data(RoleMessageSize);
        setField(m_size, size.isValid() ? UiUtils::Formatting::prettySize(size.toULongLong()) : QString());
        setField(m_folder, index.data(RoleMailboxName).toString());

        QStringList status;
        status << (index.data(RoleMessageIsMarkedRead).toBool() ? i18nc("@info message status", "Read")
                                                                : i18nc("@info message status", "Unread"));
        if (index.data(RoleMessageIsMarkedFlagged).toBool())
            status << i18nc("@info message status", "Flagged");
        if (index.data(RoleMessageIsMarkedReplied).toBool())
            status << i18nc("@info message status", "Replied");
        if (index.data(RoleMessageIsMarkedForwarded).toBool())
            status << i18nc("@info message status", "Forwarded");
        setField(m_status, status.join(i18nc("@info separator of message status", ", ")));

        // Only MIME-level protection is visible here; the reader verifies signatures and decrypts.
        QString security;
        const QAbstractItemModel *model = index.model();
        const QModelIndex root = model->rowCount(index) > 0 ? model->index(0, 0, index) : QModelIndex();
        const QString mimeType = root.data(RolePartMimeType).toString().toLower();
        if (mimeType == QLatin1String("multipart/signed"))
            security = i18nc("@info message security", "Signed");
        else if (mimeType == QLatin1String("multipart/encrypted"))
            security = i18nc("@info message security", "Encrypted");
        else if (mimeType == QLatin1String("application/pkcs7-mime") || mimeType == QLatin1String("application/x-pkcs7-mime"))
            security = i18nc("@info message security", "Encrypted or signed (S/MIME)");
        setField(m_security, security);
        showContent();
    }
    updateVisibility();
}

}
