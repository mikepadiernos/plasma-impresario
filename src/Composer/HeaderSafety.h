/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QByteArray>
#include <QString>

#include "Imap/Parser/MailAddress.h"

namespace Composer {

/** @short Keep pasted or received text from adding header lines to an outgoing message

Every value that ends up in a header of a composed message passes through these functions, so a line break or a
control character can never start a new header line, whatever the subject, a name, a file name or a mailto: link
contains.
*/
namespace HeaderSafety {

/** Whether @arg ch may never appear in a header: C0 and C1 controls, DEL, NEL and the Unicode line and paragraph
separators */
bool isUnsafe(const QChar ch);

/** Replace each run of unsafe characters in @arg text with a single space */
QString text(const QString &text);

/** Remove unsafe characters from @arg text, for address parts where a space would change the address */
QString stripped(const QString &text);

/** Keep only the visible ASCII characters of a message identifier, without its angle brackets */
QByteArray messageId(const QByteArray &id);

/** Remove ASCII controls and spaces from a token such as a MIME type */
QByteArray token(const QByteArray &token);

/** Clean the name, route, mailbox and host of @arg address */
Imap::Message::MailAddress address(const Imap::Message::MailAddress &address);

}

}
