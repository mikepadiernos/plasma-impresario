/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef LOCALMAIL_MIME_H
#define LOCALMAIL_MIME_H

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QPair>

namespace LocalMail {
namespace Mime {

/** @short Header fields of a message or a MIME part, in their original order */
class Header
{
public:
    Header() = default;
    explicit Header(const QByteArray &raw);

    /** The header as it was stored, including the empty line which ends it when there was one */
    QByteArray raw() const;
    /** The unfolded value of the first field called @arg name, compared without case; empty when absent */
    QByteArray value(const QByteArray &name) const;
    bool contains(const QByteArray &name) const;
    /** The original lines of the fields named in @arg names (or of all others when @arg exclude), and an empty line */
    QByteArray fields(const QList<QByteArray> &names, bool exclude) const;

private:
    struct Field {
        QByteArray name;
        /** The field exactly as stored, folding and line ends included */
        QByteArray original;
    };
    QByteArray m_raw;
    QList<Field> m_fields;
};

/** @short One part of a parsed message, as IMAP describes it */
struct Part {
    Header header;
    /** The part's content after its header, still transfer-encoded */
    QByteArray body;
    QByteArray type = "text";
    QByteArray subtype = "plain";
    QList<QPair<QByteArray, QByteArray>> parameters;
    QByteArray disposition;
    QList<QPair<QByteArray, QByteArray>> dispositionParameters;
    /** Parts of a multipart, or the single embedded message of message/rfc822 */
    QList<Part> children;

    bool isMultipart() const { return type == "multipart"; }
    bool isMessage() const { return type == "message" && (subtype == "rfc822" || subtype == "global"); }
};

/** Parse @arg raw, a complete message with CRLF line ends */
Part parse(const QByteArray &raw);

/** Line ends turned into CRLF, as IMAP requires */
QByteArray canonicalLineEnds(const QByteArray &raw);

/** An IMAP nstring: NIL, a quoted string or a literal, whichever @arg value needs */
QByteArray nstring(const QByteArray &value, bool emptyIsNil = true);

/** The IMAP ENVELOPE of a message with header @arg header */
QByteArray envelope(const Header &header);
/** The IMAP BODYSTRUCTURE of @arg part, with extension data when @arg extended, else the BODY form */
QByteArray bodyStructure(const Part &part, bool extended = true);

/**
 * The content of an IMAP section of @arg message: "" for all of it, HEADER, TEXT, HEADER.FIELDS (...),
 * HEADER.FIELDS.NOT (...), part numbers such as 1.2, and those followed by .MIME, .HEADER or .TEXT
 *
 * Returns false when the section names no part.
 */
bool section(const Part &message, const QByteArray &raw, const QByteArray &spec, QByteArray *out);

/** The date in the Date field of @arg header, invalid when there is none or it cannot be read */
QDateTime sentDate(const Header &header);

}
}

#endif
