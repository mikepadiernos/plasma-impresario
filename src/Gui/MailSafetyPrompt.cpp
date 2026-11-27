/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/MailSafetyPrompt.h"

#include <QMessageBox>
#include <QPushButton>

#include <KLocalizedString>

#include "Cryptography/MessageModel.h"
#include "Imap/Model/ItemRoles.h"

namespace Gui {

namespace MailSafetyPrompt {

using UiUtils::MailSafety::JunkVerdict;
using UiUtils::MailSafety::OpenDecision;
using UiUtils::MailSafety::ScanResult;

JunkVerdict junkVerdictOfPart(const QModelIndex &part)
{
    QModelIndex message;
    if (const auto *model = qobject_cast<const Cryptography::MessageModel *>(part.model()))
        message = model->message();
    // A part of the IMAP model: the message is the closest ancestor which has flags
    for (QModelIndex index = part; !message.isValid() && index.isValid(); index = index.parent()) {
        if (index.data(Imap::Mailbox::RoleMessageFlags).isValid())
            message = index;
    }
    if (!message.isValid())
        return JunkVerdict::Unclassified;
    return UiUtils::MailSafety::junkVerdict(message.data(Imap::Mailbox::RoleMessageFlags).toStringList());
}

QString junkBannerText()
{
    return i18nc("@info", "This message is marked as junk. Be careful with its links, attachments and remote content.");
}

QString scanResultText(ScanResult scan, const QString &fileName)
{
    switch (scan) {
    case ScanResult::NotScanned:
        return QString();
    case ScanResult::Clean:
        return i18nc("@info %1 is a file name", "No threat was found in %1.", fileName);
    case ScanResult::Infected:
        return i18nc("@info %1 is a file name", "The virus scanner found a threat in %1. It will not be opened.", fileName);
    case ScanResult::Failed:
        return i18nc("@info %1 is a file name", "The virus scanner could not check %1. It has not been found safe.", fileName);
    case ScanResult::Unavailable:
        return i18nc("@info %1 is a file name", "%1 was not scanned because the virus scanner is not available.", fileName);
    }
    return QString();
}

bool confirmOpening(QWidget *parent, JunkVerdict verdict, ScanResult scan, const QString &fileName)
{
    const OpenDecision decision = UiUtils::MailSafety::openDecision(verdict, scan);
    if (decision == OpenDecision::Open)
        return true;

    if (decision == OpenDecision::Block) {
        QMessageBox box(QMessageBox::Critical, i18nc("@title:window", "Attachment Blocked"), scanResultText(scan, fileName),
                        QMessageBox::Close, parent);
        box.setObjectName(QStringLiteral("attachmentBlocked"));
        box.setTextFormat(Qt::PlainText);
        box.setInformativeText(i18nc("@info", "You can still save it, for example to send it for analysis."));
        box.exec();
        return false;
    }

    QStringList reasons;
    if (verdict == JunkVerdict::Junk)
        reasons << i18nc("@info", "The message is marked as junk.");
    const QString scanText = scanResultText(scan, fileName);
    if (!scanText.isEmpty() && !UiUtils::MailSafety::isReportedClean(scan))
        reasons << scanText;

    QMessageBox box(QMessageBox::Warning, i18nc("@title:window", "Open Attachment?"),
                    i18nc("@info %1 is a file name", "Open %1 with another application?", fileName), QMessageBox::Cancel, parent);
    box.setObjectName(QStringLiteral("attachmentConfirmation"));
    box.setTextFormat(Qt::PlainText);
    box.setInformativeText(reasons.join(QLatin1Char('\n')));
    QPushButton *open = box.addButton(i18nc("@action:button", "Open Anyway"), QMessageBox::AcceptRole);
    open->setObjectName(QStringLiteral("openAttachmentAnyway"));
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    return box.clickedButton() == open;
}

}

}
