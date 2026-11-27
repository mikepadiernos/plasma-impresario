/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QString>

#include <functional>

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
chose "Open Anyway". A blocked attachment is reported and never opened; @arg threat, when known, names what the
scanner found.
*/
bool confirmOpening(QWidget *parent, UiUtils::MailSafety::JunkVerdict verdict, UiUtils::MailSafety::ScanResult scan,
                    const QString &fileName, const QString &threat = QString());

/** @short Scan the downloaded copy @arg file of the attachment @arg fileName, if scanning is turned on, then ask

When the ClamAV scan is turned off in the settings, @arg done receives true at once. Otherwise @arg file is streamed to
the local clamd, and @arg done receives what confirmOpening() decides about the result. The junk verdict is not asked
again; it was confirmed before the download. When @arg parent is destroyed during the scan, @arg done is not called and
the opened copy @arg file is removed.
*/
void scanBeforeOpening(QWidget *parent, const QString &file, const QString &fileName, std::function<void(bool)> done);

}

}
