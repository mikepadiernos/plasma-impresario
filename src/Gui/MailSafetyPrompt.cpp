/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/MailSafetyPrompt.h"

#include <QApplication>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTimer>

#include <KLocalizedString>
#include <KSharedConfig>

#include "Common/PrivateFiles.h"
#include "Cryptography/MessageModel.h"
#include "Imap/Model/ItemRoles.h"
#include "UiUtils/ActivityLog.h"
#include "UiUtils/ClamdScanner.h"
#include "UiUtils/MailSafetySettings.h"

namespace Gui {

namespace MailSafetyPrompt {

using UiUtils::MailSafety::JunkVerdict;
using UiUtils::MailSafety::OpenDecision;
using UiUtils::MailSafety::ScanResult;

namespace {

QPointer<UiUtils::ActivityLog> &activityLog()
{
    static QPointer<UiUtils::ActivityLog> log;
    return log;
}

/** The entry of one scan; scans of the same file name share it, the latest one wins */
UiUtils::ActivityLog::Entry scanEntry(const QString &fileName)
{
    UiUtils::ActivityLog::Entry entry;
    entry.key = QLatin1String("scan:") + fileName;
    entry.kind = UiUtils::ActivityLog::Kind::Scan;
    entry.title = i18nc("@info %1 is a file name", "Virus scan of %1", fileName);
    return entry;
}

}

void setActivityLog(UiUtils::ActivityLog *log)
{
    activityLog() = log;
}

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

bool confirmOpening(QWidget *parent, JunkVerdict verdict, ScanResult scan, const QString &fileName, const QString &threat)
{
    const OpenDecision decision = UiUtils::MailSafety::openDecision(verdict, scan);
    if (decision == OpenDecision::Open)
        return true;

    if (decision == OpenDecision::Block) {
        QMessageBox box(QMessageBox::Critical, i18nc("@title:window", "Attachment Blocked"), scanResultText(scan, fileName),
                        QMessageBox::Close, parent);
        box.setObjectName(QStringLiteral("attachmentBlocked"));
        box.setTextFormat(Qt::PlainText);
        QString details = i18nc("@info", "You can still save it, for example to send it for analysis.");
        if (!threat.isEmpty())
            details = i18nc("@info %1 is the name of a virus or other threat", "Threat: %1", threat) + QLatin1Char('\n') + details;
        box.setInformativeText(details);
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

void scanBeforeOpening(QWidget *parent, const QString &file, const QString &fileName, std::function<void(bool)> done)
{
    const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
    if (!UiUtils::MailSafetySettings::scansAttachments(*config)) {
        done(true);
        return;
    }

    // Not owned by the parent, so that a parent closed during the scan does not leave the opened copy behind
    auto *scanner = new UiUtils::ClamdScanner();
    QPointer<QWidget> guard(parent);
    QApplication::setOverrideCursor(Qt::BusyCursor);
    if (UiUtils::ActivityLog *log = activityLog()) {
        UiUtils::ActivityLog::Entry entry = scanEntry(fileName);
        entry.state = UiUtils::ActivityLog::State::Running;
        entry.detail = i18nc("@info", "Scanning with ClamAV…");
        log->record(entry);
    }
    QObject::connect(scanner, &UiUtils::ClamdScanner::finished, scanner,
                     [scanner, guard, file, fileName, done](ScanResult result, const QString &threat) {
        scanner->deleteLater();
        QApplication::restoreOverrideCursor();
        if (UiUtils::ActivityLog *log = activityLog()) {
            UiUtils::ActivityLog::Entry entry = scanEntry(fileName);
            entry.state = UiUtils::MailSafety::isReportedClean(result) ? UiUtils::ActivityLog::State::Done
                                                                       : UiUtils::ActivityLog::State::Failed;
            if (result == ScanResult::Infected)
                entry.error = threat.isEmpty() ? i18nc("@info", "A threat was found") : threat;
            else if (!UiUtils::MailSafety::isReportedClean(result))
                entry.error = scanResultText(result, fileName);
            else
                entry.detail = i18nc("@info", "No threat found");
            log->record(entry);
        }
        if (!guard) {
            Common::PrivateFiles::removeOpenedAttachment(Common::PrivateFiles::openedAttachmentsFolder(), file);
            return;
        }
        // The question runs its own event loop, which must not run inside the scanner's signal
        QTimer::singleShot(0, guard, [guard, result, threat, fileName, done]() {
            done(confirmOpening(guard, JunkVerdict::Unclassified, result, fileName, threat));
        });
    });
    scanner->scanFile(file, UiUtils::MailSafetySettings::scannerSocketToUse(*config));
}

}

}
