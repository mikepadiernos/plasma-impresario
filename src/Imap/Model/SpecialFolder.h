/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

namespace Imap {
namespace Mailbox {

/** @short Role of a mailbox which the user reaches often

The order of the enumerators is the canonical presentation order. The numeric
values are exposed through RoleMailboxSpecialUse and must stay stable.
*/
enum class SpecialUse {
    None = 0,
    Inbox,
    Drafts,
    Sent,
    Archive,
    Junk,
    Trash,
    All,
    Flagged,
};

namespace SpecialFolder {

/** @short Kind announced by RFC 6154 SPECIAL-USE (or legacy XLIST) attributes in LIST flags

Matching is case-insensitive. When a mailbox carries several such attributes,
the one first in the canonical order wins.
*/
SpecialUse fromAttributes(const QStringList &flags);

/** @short Kind guessed from a common English folder name

Only INBOX itself, top-level mailboxes and direct children of INBOX (such as
"INBOX.Sent") are considered; the leaf name is compared case-insensitively.
*/
SpecialUse fromName(const QString &mailbox, const QString &separator);

/** @short Position in the canonical order; ordinary mailboxes sort after all special ones */
int sortRank(SpecialUse kind);

/** @short Kinds offered for quick navigation, in the canonical order */
QList<SpecialUse> navigableKinds();

struct Candidate {
    QString mailbox;
    QString separator;
    QStringList flags;
    bool selectable = true;
};

struct Resolution {
    /** Kind of each classified mailbox; ordinary mailboxes are absent */
    QMap<QString, SpecialUse> byMailbox;
    /** Selectable mailbox that navigation to the given kind opens */
    QMap<SpecialUse, QString> primary;

    SpecialUse kindOf(const QString &mailbox) const { return byMailbox.value(mailbox, SpecialUse::None); }
    QString mailboxFor(SpecialUse kind) const { return primary.value(kind); }
    bool operator==(const Resolution &other) const = default;
};

/** @short Classify all known mailboxes of one account

@arg candidates must be in tree order (parents before children, siblings in
display order) because name-based guesses keep only the first match.
@arg archiveFolderName is the account's configured archive folder.

Rules, from strongest to weakest:
- INBOX (any case) is the Inbox.
- A mailbox's own SPECIAL-USE attribute.
- The configured archive folder is an Archive.
- The common-name fallback, used only for selectable mailboxes, only for kinds
  that no mailbox announces through an attribute (and, for Archive, only when
  the configured archive folder does not exist), and only for the first
  matching mailbox of each kind.

The navigation target is the first selectable mailbox of each kind, except
that the configured archive folder is preferred for Archive and the real INBOX
for Inbox.
*/
Resolution resolve(const QList<Candidate> &candidates, const QString &archiveFolderName);

}

}
}
