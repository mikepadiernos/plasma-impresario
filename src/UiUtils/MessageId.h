/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QByteArray>
#include <QStringList>
#include <QUrl>

namespace UiUtils {

/** @short Message-IDs given by the user, on the command line or as mid: URLs (RFC 2392) */
namespace MessageId {

enum class Problem {
    None,
    /** Nothing was given */
    Empty,
    /** Spaces, control characters or angle brackets inside the ID */
    InvalidCharacter,
    /** No "@" with text on both sides */
    MissingAt,
    /** Longer than a header line may be */
    TooLong,
};

struct Parsed {
    /** The Message-ID without angle brackets, or empty when it is not valid */
    QByteArray messageId;
    /** The Content-ID of a part named by a mid: URL, without angle brackets, or empty */
    QByteArray contentId;
    Problem problem = Problem::None;

    bool isValid() const { return problem == Problem::None; }
};

/** @short Read a Message-ID from @arg input

@arg input is a mid: URL, whose parts are percent-decoded and which may name a Content-ID after a slash, or a bare
Message-ID with or without angle brackets. Surrounding white space is ignored.
*/
Parsed parse(const QString &input);

/** @short Check a Message-ID without angle brackets */
Problem validate(const QByteArray &messageId);

/** @short The mid: URL of @arg messageId */
QUrl url(const QByteArray &messageId);

/** @short IMAP SEARCH keys matching the messages whose Message-ID contains @arg messageId; see sameId() */
QStringList searchConditions(const QByteArray &messageId);

/** @short Whether two Message-IDs are the same, with or without angle brackets */
bool sameId(const QByteArray &a, const QByteArray &b);

}

}
