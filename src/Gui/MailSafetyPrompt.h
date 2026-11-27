/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QString>

#include "UiUtils/MailSafety.h"

class QModelIndex;
class QWidget;

namespace Gui {

/** @short The user-visible side of UiUtils::MailSafety: junk verdicts, scan results and the questions they raise */
namespace MailSafetyPrompt {

/** @short The junk verdict of the message which contains @arg part, a part in the reader's model or the IMAP model */
UiUtils::MailSafety::JunkVerdict junkVerdictOfPart(const QModelIndex &part);

/** @short The text of the reader's banner on a message marked as junk */
QString junkBannerText();

/** @short What @arg scan means for the attachment @arg fileName; empty when there is nothing to say */
QString scanResultText(UiUtils::MailSafety::ScanResult scan, const QString &fileName);

/** @short Ask whether to open the attachment @arg fileName, if UiUtils::MailSafety::openDecision() requires it

Returns true when the attachment may be opened: without a question when nothing speaks against it, or after the user
chose "Open Anyway". A blocked attachment is reported and never opened.
*/
bool confirmOpening(QWidget *parent, UiUtils::MailSafety::JunkVerdict verdict, UiUtils::MailSafety::ScanResult scan,
                    const QString &fileName);

}

}
