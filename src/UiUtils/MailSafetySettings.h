/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QString>

class KConfigBase;

namespace UiUtils {

/** @short The settings of the optional mail safety hooks: the ClamAV scan of attachments and moving junk mail

Both are off by default. Scanning streams an attachment to the local clamd before it is opened; moving junk mail
moves a message marked as junk to the account's Junk folder, where the server's spam filter can learn from it.
*/
namespace MailSafetySettings {

/** Whether an attachment is scanned with clamd before it is opened, as stored in @arg config */
bool scansAttachments(const KConfigBase &config);
void setScansAttachments(KConfigBase &config, bool enabled);

/** The path of clamd's socket set in @arg config; empty when it is found automatically */
QString scannerSocket(const KConfigBase &config);
void setScannerSocket(KConfigBase &config, const QString &path);
/** The socket to scan through: the one set in @arg config, or else the first default socket which exists */
QString scannerSocketToUse(const KConfigBase &config);

/** Whether marking a message as junk moves it to the Junk folder, and marking it as not junk moves it back */
bool movesJunk(const KConfigBase &config);
void setMovesJunk(KConfigBase &config, bool enabled);

}

}
