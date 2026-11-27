/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QCoreApplication>
#include <QList>
#include <QString>
#include <QStringList>
#include "Imap/Parser/Response.h"

namespace Imap {
namespace Mailbox {

/** @short What a server can search for, and why a search could not run

IMAP4rev1 (RFC 3501) search keys work everywhere, but some keys belong to
extensions which a server has to advertise first, such as FUZZY (RFC 6203) or
OLDER and YOUNGER (RFC 5032). A server which does not know a key answers BAD,
and a server which cannot search in the requested charset answers NO with
BADCHARSET. These helpers find such keys before the search is sent and turn
the server's refusal into an explanation, so that a failed search does not
look like a search without matches.
*/
class SearchSupport
{
    Q_DECLARE_TR_FUNCTIONS(Imap::Mailbox::SearchSupport)
public:
    struct MissingKey {
        QString key;
        QString capability;

        bool operator==(const MissingKey &other) const = default;
    };

    /** @short Keys of @arg conditions which need an extension that @arg capabilities do not include

    A single condition is a raw query which the parser sends verbatim; several
    conditions are keys and their values, one per item. Values are skipped, so
    a subject which reads "older" is not taken for a key. Scanning stops at a
    key which is not known here, because its arguments cannot be told apart
    from the keys which follow; the server reports such a key itself.
    */
    static QList<MissingKey> unsupportedKeys(const QStringList &conditions, const QStringList &capabilities);

    /** @short Whether a value in @arg conditions needs more than US-ASCII */
    static bool needsUnicode(const QStringList &conditions);

    static QString explainUnsupported(const QList<MissingKey> &missing);
    static QString explainAsciiOnly();

    /** @short Explain a tagged NO or BAD answer to a search

    @arg charsets are those named by a BADCHARSET response code, @arg rawQuery
    says whether the user typed the query in IMAP syntax.
    */
    static QString explainRejection(Responses::Kind kind, Responses::Code code, const QStringList &charsets,
                                    const QString &serverText, bool rawQuery);
};

}
}
