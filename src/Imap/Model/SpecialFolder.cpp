/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "SpecialFolder.h"

#include <QSet>

namespace Imap {
namespace Mailbox {
namespace SpecialFolder {

namespace {

struct AttributeName {
    const char *attribute;
    SpecialUse kind;
};

// Canonical order; legacy XLIST spellings sit next to their RFC 6154 counterparts.
constexpr AttributeName attributeNames[] = {
    {"\\INBOX", SpecialUse::Inbox},
    {"\\DRAFTS", SpecialUse::Drafts},
    {"\\SENT", SpecialUse::Sent},
    {"\\ARCHIVE", SpecialUse::Archive},
    {"\\JUNK", SpecialUse::Junk},
    {"\\SPAM", SpecialUse::Junk},
    {"\\TRASH", SpecialUse::Trash},
    {"\\ALL", SpecialUse::All},
    {"\\ALLMAIL", SpecialUse::All},
    {"\\FLAGGED", SpecialUse::Flagged},
    {"\\STARRED", SpecialUse::Flagged},
};

struct CommonName {
    const char *name;
    SpecialUse kind;
};

constexpr CommonName commonNames[] = {
    {"Drafts", SpecialUse::Drafts},
    {"Sent", SpecialUse::Sent},
    {"Sent Items", SpecialUse::Sent},
    {"Sent Messages", SpecialUse::Sent},
    {"Sent Mail", SpecialUse::Sent},
    {"Archive", SpecialUse::Archive},
    {"Archives", SpecialUse::Archive},
    {"Junk", SpecialUse::Junk},
    {"Junk E-mail", SpecialUse::Junk},
    {"Junk Email", SpecialUse::Junk},
    {"Spam", SpecialUse::Junk},
    {"Trash", SpecialUse::Trash},
    {"Deleted Items", SpecialUse::Trash},
    {"Deleted Messages", SpecialUse::Trash},
};

bool isInbox(const QString &mailbox)
{
    return mailbox.compare(QLatin1String("INBOX"), Qt::CaseInsensitive) == 0;
}

}

SpecialUse fromAttributes(const QStringList &flags)
{
    QSet<QString> upper;
    for (const QString &flag : flags) {
        upper.insert(flag.toUpper());
    }
    for (const auto &entry : attributeNames) {
        if (upper.contains(QLatin1String(entry.attribute))) {
            return entry.kind;
        }
    }
    return SpecialUse::None;
}

SpecialUse fromName(const QString &mailbox, const QString &separator)
{
    if (isInbox(mailbox)) {
        return SpecialUse::Inbox;
    }
    QString leaf = mailbox;
    if (!separator.isEmpty()) {
        const QStringList parts = mailbox.split(separator);
        if (parts.size() == 2 && isInbox(parts[0])) {
            leaf = parts[1];
        } else if (parts.size() != 1) {
            return SpecialUse::None;
        }
    }
    for (const auto &entry : commonNames) {
        if (leaf.compare(QLatin1String(entry.name), Qt::CaseInsensitive) == 0) {
            return entry.kind;
        }
    }
    return SpecialUse::None;
}

int sortRank(SpecialUse kind)
{
    return kind == SpecialUse::None ? 1000 : static_cast<int>(kind);
}

QList<SpecialUse> navigableKinds()
{
    return {SpecialUse::Inbox, SpecialUse::Drafts, SpecialUse::Sent,
            SpecialUse::Archive, SpecialUse::Junk, SpecialUse::Trash};
}

Resolution resolve(const QList<Candidate> &candidates, const QString &archiveFolderName, bool allMailArchives)
{
    // QSet<int> because qHash() for scoped enums is not available in every supported Qt 6 release
    QSet<int> announced;
    bool configuredArchiveExists = false;
    for (const Candidate &candidate : candidates) {
        const SpecialUse kind = fromAttributes(candidate.flags);
        if (kind != SpecialUse::None) {
            announced.insert(static_cast<int>(kind));
        }
        if (!archiveFolderName.isEmpty() && candidate.mailbox == archiveFolderName) {
            configuredArchiveExists = true;
        }
    }

    Resolution res;
    QSet<int> guessed;
    for (const Candidate &candidate : candidates) {
        SpecialUse kind = SpecialUse::None;
        if (isInbox(candidate.mailbox)) {
            kind = SpecialUse::Inbox;
        } else if (const SpecialUse attribute = fromAttributes(candidate.flags); attribute != SpecialUse::None) {
            kind = attribute;
        } else if (!archiveFolderName.isEmpty() && candidate.mailbox == archiveFolderName) {
            kind = SpecialUse::Archive;
        } else if (candidate.selectable) {
            const SpecialUse byName = fromName(candidate.mailbox, candidate.separator);
            const int key = static_cast<int>(byName);
            if (byName != SpecialUse::None && !announced.contains(key) && !guessed.contains(key)
                && !(byName == SpecialUse::Archive && (configuredArchiveExists || allMailArchives))) {
                kind = byName;
                guessed.insert(key);
            }
        }
        if (kind == SpecialUse::None) {
            continue;
        }
        res.byMailbox.insert(candidate.mailbox, kind);
        if (!candidate.selectable) {
            continue;
        }
        const bool preferred = (kind == SpecialUse::Archive && candidate.mailbox == archiveFolderName)
            || (kind == SpecialUse::Inbox && isInbox(candidate.mailbox));
        if (!res.primary.contains(kind) || preferred) {
            res.primary.insert(kind, candidate.mailbox);
        }
    }
    // Gmail has no archive folder: archiving removes the Inbox label, which a move to All Mail does
    if (allMailArchives && !res.primary.contains(SpecialUse::Archive) && res.primary.contains(SpecialUse::All)) {
        res.primary.insert(SpecialUse::Archive, res.primary.value(SpecialUse::All));
    }
    return res;
}

}
}
}
