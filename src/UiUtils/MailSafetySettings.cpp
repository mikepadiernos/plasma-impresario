/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/MailSafetySettings.h"

#include <KConfigGroup>

#include "UiUtils/ClamdScanner.h"

namespace UiUtils {

namespace MailSafetySettings {

namespace {

const char groupName[] = "MailSafety";
const char scanAttachmentsKey[] = "ScanAttachments";
const char clamdSocketKey[] = "ClamdSocket";
const char moveJunkKey[] = "MoveJunk";

void write(KConfigBase &config, const char *key, const QVariant &value)
{
    KConfigGroup group = config.group(QLatin1String(groupName));
    group.writeEntry(key, value);
    group.sync();
}

}

bool scansAttachments(const KConfigBase &config)
{
    return config.group(QLatin1String(groupName)).readEntry(scanAttachmentsKey, false);
}

void setScansAttachments(KConfigBase &config, bool enabled)
{
    write(config, scanAttachmentsKey, enabled);
}

QString scannerSocket(const KConfigBase &config)
{
    return config.group(QLatin1String(groupName)).readEntry(clamdSocketKey, QString()).trimmed();
}

void setScannerSocket(KConfigBase &config, const QString &path)
{
    write(config, clamdSocketKey, path.trimmed());
}

QString scannerSocketToUse(const KConfigBase &config)
{
    const QString configured = scannerSocket(config);
    return configured.isEmpty() ? ClamdScanner::findSocket(ClamdScanner::defaultSockets()) : configured;
}

bool movesJunk(const KConfigBase &config)
{
    return config.group(QLatin1String(groupName)).readEntry(moveJunkKey, false);
}

void setMovesJunk(KConfigBase &config, bool enabled)
{
    write(config, moveJunkKey, enabled);
}

}

}
