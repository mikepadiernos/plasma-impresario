/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Composer/HeaderSafety.h"

namespace Composer {
namespace HeaderSafety {

bool isUnsafe(const QChar ch)
{
    const auto code = ch.unicode();
    return code < 0x20 || (code >= 0x7f && code <= 0x9f) || code == 0x2028 || code == 0x2029;
}

QString text(const QString &text)
{
    QString result;
    result.reserve(text.size());
    bool previousUnsafe = false;
    for (const QChar ch : text) {
        if (isUnsafe(ch)) {
            if (!previousUnsafe)
                result.append(QLatin1Char(' '));
            previousUnsafe = true;
        } else {
            result.append(ch);
            previousUnsafe = false;
        }
    }
    return result;
}

QString stripped(const QString &text)
{
    QString result;
    result.reserve(text.size());
    for (const QChar ch : text) {
        if (!isUnsafe(ch))
            result.append(ch);
    }
    return result;
}

QByteArray messageId(const QByteArray &id)
{
    QByteArray result;
    result.reserve(id.size());
    for (const char ch : id) {
        if (ch > 0x20 && ch < 0x7f && ch != '<' && ch != '>')
            result.append(ch);
    }
    return result;
}

QByteArray token(const QByteArray &token)
{
    QByteArray result;
    result.reserve(token.size());
    for (const char ch : token) {
        if (ch > 0x20 && ch < 0x7f && ch != ';' && ch != '"')
            result.append(ch);
    }
    return result;
}

Imap::Message::MailAddress address(const Imap::Message::MailAddress &address)
{
    return Imap::Message::MailAddress(text(address.name).trimmed(), stripped(address.adl), stripped(address.mailbox),
                                      stripped(address.host));
}

}
}
