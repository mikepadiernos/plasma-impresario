/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/MailSafety.h"

namespace UiUtils {

namespace MailSafety {

namespace {

bool containsAny(const QStringList &flags, std::initializer_list<QLatin1String> names)
{
    for (const QString &flag : flags) {
        for (const QLatin1String &name : names) {
            if (flag.compare(name, Qt::CaseInsensitive) == 0)
                return true;
        }
    }
    return false;
}

}

JunkVerdict junkVerdict(const QStringList &flags)
{
    if (containsAny(flags, {QLatin1String("$NotJunk"), QLatin1String("NotJunk"), QLatin1String("NonJunk")}))
        return JunkVerdict::NotJunk;
    if (containsAny(flags, {QLatin1String("$Junk"), QLatin1String("Junk")}))
        return JunkVerdict::Junk;
    return JunkVerdict::Unclassified;
}

bool isReportedClean(ScanResult result)
{
    return result == ScanResult::Clean;
}

OpenDecision openDecision(JunkVerdict verdict, ScanResult scan)
{
    switch (scan) {
    case ScanResult::Infected:
        return OpenDecision::Block;
    case ScanResult::Failed:
    case ScanResult::Unavailable:
        return OpenDecision::Confirm;
    case ScanResult::NotScanned:
    case ScanResult::Clean:
        break;
    }
    // A clean scan says nothing about phishing, so junk still asks
    return verdict == JunkVerdict::Junk ? OpenDecision::Confirm : OpenDecision::Open;
}

}

}
