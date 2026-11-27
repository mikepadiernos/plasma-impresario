/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QStringList>

namespace UiUtils {

/** @short How spam classification and attachment scans are surfaced to the user

The junk verdict is read from the message's IMAP keywords, which the server's filter, another client or the user set.
The user's "Not Junk" mark always wins over a junk mark, so that a false positive is overridden once and stays
overridden. The keywords of RFC 5788 ($Junk, $NotJunk) and the older ones used by other clients (Junk, NonJunk,
NotJunk) are recognized without regard to case.

An attachment scan has a result only when a scanner was asked. A scan which could not run, because no scanner is
installed or reachable or because it failed, is never reported as clean: opening the attachment then needs the
user's confirmation. An infected attachment is not opened at all.

Nothing here classifies a message or scans data itself; these are only the rules for presenting results.
*/
namespace MailSafety {

enum class JunkVerdict {
    /** Neither the server nor the user classified the message */
    Unclassified,
    Junk,
    /** Marked as not junk, which overrides any junk mark */
    NotJunk,
};

/** @short The junk verdict of a message with the IMAP @arg flags */
JunkVerdict junkVerdict(const QStringList &flags);

enum class ScanResult {
    /** No scan was requested, for example because scanning is turned off */
    NotScanned,
    Clean,
    Infected,
    /** The scanner ran but could not decide, for example on a size limit or a broken archive */
    Failed,
    /** Scanning is turned on, but no scanner could be reached */
    Unavailable,
};

/** @short Whether @arg result may be presented as a clean scan; only an actual clean result is */
bool isReportedClean(ScanResult result);

enum class OpenDecision {
    /** Open without asking */
    Open,
    /** Ask the user first; opening stays possible */
    Confirm,
    /** Do not open; the user may still save the attachment */
    Block,
};

/** @short Whether an attachment of a message with @arg verdict whose scan gave @arg scan may be opened */
OpenDecision openDecision(JunkVerdict verdict, ScanResult scan);

}

}
