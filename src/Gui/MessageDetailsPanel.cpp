/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "MessageDetailsPanel.h"

#include <KLocalizedString>
#include <QDateTime>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QScrollArea>

#include "Cryptography/MessageModel.h"
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
    m_account = addField(QStringLiteral("Account"), i18nc("@label message details", "Account:"));
    m_status = addField(QStringLiteral("Status"), i18nc("@label message details", "Status:"));

    // The security row pairs an icon with the text so that the state never depends on the icon alone.
    m_securityRow = new QWidget;
    m_securityRow->setObjectName(objectName() + QLatin1String("SecurityRow"));
    auto *securityLayout = new QHBoxLayout(m_securityRow);
    securityLayout->setContentsMargins(0, 0, 0, 0);
    m_securityIcon = new QLabel(m_securityRow);
    m_securityIcon->setObjectName(objectName() + QLatin1String("SecurityIcon"));
    m_securityIcon->setAlignment(Qt::AlignTop);
    m_security = new QLabel(m_securityRow);
    m_security->setObjectName(objectName() + QLatin1String("Security"));
    m_security->setWordWrap(true);
    m_security->setTextFormat(Qt::PlainText);
    m_security->setTextInteractionFlags(Qt::TextSelectableByMouse);
    securityLayout->addWidget(m_securityIcon, 0, Qt::AlignTop);
    securityLayout->addWidget(m_security, 1);
    auto *securityTitle = new QLabel(i18nc("@label message details", "Security:"));
    securityTitle->setBuddy(m_security);
    m_form->addRow(securityTitle, m_securityRow);

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
    if (!value)
        return QString();
    QWidget *rowWidget = value == m_security ? m_securityRow : const_cast<QLabel *>(value);
    return m_form->isRowVisible(rowWidget) ? value->text() : QString();
}

UiUtils::MessageSecurity MessageDetailsPanel::security() const
{
    return m_securityState;
}

void MessageDetailsPanel::setAccountResolver(const std::function<QString(const QModelIndex &)> &resolver)
{
    m_accountName = resolver;
    scheduleRefresh();
}

void MessageDetailsPanel::setMessageModel(QAbstractItemModel *model)
{
    for (const auto &connection : std::as_const(m_partModelConnections))
        disconnect(connection);
    m_partModelConnections.clear();
    m_partModel = model;
    if (model) {
        // Verification and decryption finish asynchronously and announce their results through these signals.
        m_partModelConnections << connect(model, &QAbstractItemModel::dataChanged, this, &MessageDetailsPanel::scheduleRefresh)
                               << connect(model, &QAbstractItemModel::rowsInserted, this, &MessageDetailsPanel::scheduleRefresh)
                               << connect(model, &QAbstractItemModel::rowsRemoved, this, &MessageDetailsPanel::scheduleRefresh)
                               << connect(model, &QAbstractItemModel::modelReset, this, &MessageDetailsPanel::scheduleRefresh)
                               << connect(model, &QAbstractItemModel::layoutChanged, this, &MessageDetailsPanel::scheduleRefresh);
    }
    scheduleRefresh();
}

QString MessageDetailsPanel::securityText(const UiUtils::MessageSecurity &security)
{
    using Encryption = UiUtils::MessageSecurity::Encryption;
    using Signature = UiUtils::MessageSecurity::Signature;
    QStringList lines;
    switch (security.encryption) {
    case Encryption::None:
        break;
    case Encryption::Pending:
        lines << i18nc("@info message security", "Encrypted, decrypting…");
        break;
    case Encryption::NotDecrypted:
        lines << (security.inlinePgp ? i18nc("@info message security", "Encrypted with inline PGP, not decrypted")
                                     : i18nc("@info message security", "Encrypted, not decrypted"));
        break;
    case Encryption::Decrypted:
        lines << i18nc("@info message security", "Encrypted and decrypted");
        break;
    case Encryption::Failed:
        lines << i18nc("@info message security", "Encrypted, decryption failed");
        break;
    }
    const QString signer = security.signer;
    switch (security.signature) {
    case Signature::None:
        break;
    case Signature::Pending:
        lines << i18nc("@info message security", "Signed, verifying the signature…");
        break;
    case Signature::NotVerified:
        lines << (security.inlinePgp ? i18nc("@info message security", "Signed with inline PGP, not verified")
                                     : i18nc("@info message security", "Signed, not verified"));
        break;
    case Signature::Trusted:
        lines << (signer.isEmpty() ? i18nc("@info message security", "Valid signature")
                                   : i18nc("@info message security", "Valid signature from %1", signer));
        break;
    case Signature::Untrusted:
        lines << (signer.isEmpty() ? i18nc("@info message security", "Valid signature, but the signer is not trusted or is not the sender")
                                   : i18nc("@info message security", "Valid signature from %1, but the signer is not trusted or is not the sender", signer));
        break;
    case Signature::Bad:
        lines << (signer.isEmpty() ? i18nc("@info message security", "Invalid signature")
                                   : i18nc("@info message security", "Invalid signature from %1", signer));
        break;
    case Signature::UnknownKey:
        lines << i18nc("@info message security", "Signed with an unknown key, not verified");
        break;
    case Signature::Unverifiable:
        lines << i18nc("@info message security", "Signed, but the signature could not be verified");
        break;
    }
    return lines.join(QLatin1Char('\n'));
}

QString MessageDetailsPanel::securityIconName(const UiUtils::MessageSecurity &security)
{
    using Encryption = UiUtils::MessageSecurity::Encryption;
    using Signature = UiUtils::MessageSecurity::Signature;
    if (security.encryption == Encryption::Failed)
        return QStringLiteral("dialog-error");
    switch (security.signature) {
    case Signature::Bad:
        return QStringLiteral("security-low");
    case Signature::Pending:
        return QStringLiteral("clock");
    case Signature::Unverifiable:
        return QStringLiteral("dialog-warning");
    case Signature::UnknownKey:
    case Signature::Untrusted:
        return QStringLiteral("security-medium");
    case Signature::Trusted:
        return QStringLiteral("security-high");
    case Signature::NotVerified:
        return QStringLiteral("document-sign");
    case Signature::None:
        break;
    }
    switch (security.encryption) {
    case Encryption::Pending:
        return QStringLiteral("clock");
    case Encryption::Decrypted:
        return QStringLiteral("document-decrypt");
    case Encryption::NotDecrypted:
        return QStringLiteral("document-encrypt");
    case Encryption::Failed:
    case Encryption::None:
        break;
    }
    return QString();
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
    if (!index.isValid() || !m_hasContent)
        m_securityState = {};
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
        setField(m_account, m_accountName ? m_accountName(index) : QString());

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

        // Prefer the reader's results; the panel itself never verifies, decrypts or downloads anything.
        QModelIndex securityRoot = index;
        if (const auto *partModel = qobject_cast<const Cryptography::MessageModel *>(m_partModel.data());
            partModel && partModel->message() == index) {
            securityRoot = partModel->index(0, 0);
        }
        m_securityState = UiUtils::MessageSecurity::evaluate(securityRoot);
        const QString security = securityText(m_securityState);
        m_security->setText(security);
        const QString iconName = securityIconName(m_securityState);
        const int iconSize = m_security->fontMetrics().height();
        m_securityIcon->setPixmap(iconName.isEmpty() ? QPixmap() : themedIcon(iconName).pixmap(iconSize, iconSize));
        m_securityIcon->setVisible(!iconName.isEmpty());
        m_securityIcon->setAccessibleName(security);
        m_securityRow->setAccessibleName(i18nc("@label accessible name of the message security", "Security: %1", security));
        QStringList tooltip;
        if (!m_securityState.summary.isEmpty())
            tooltip << m_securityState.summary;
        if (m_securityState.signDate.isValid())
            tooltip << i18nc("@info:tooltip", "Signed on %1", QLocale().toString(m_securityState.signDate.toLocalTime(), QLocale::LongFormat));
        if (!m_securityState.details.isEmpty() && m_securityState.details != m_securityState.summary)
            tooltip << m_securityState.details;
        if (m_securityState.inlinePgp)
            tooltip << i18nc("@info:tooltip", "Inline PGP is detected but not verified or decrypted by the reader.");
        m_securityRow->setToolTip(tooltip.join(QLatin1Char('\n')));
        m_form->setRowVisible(m_securityRow, !security.isEmpty());
        showContent();
    }
    updateVisibility();
}

}
