/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "MessageId.h"

namespace UiUtils {
namespace MessageId {

namespace {

/** RFC 5322 limits a header line to 998 characters, so a longer ID cannot have come from a message */
constexpr int maximumLength = 998;

QByteArray withoutBrackets(QByteArray id)
{
    id = id.trimmed();
    if (id.size() >= 2 && id.startsWith('<') && id.endsWith('>'))
        id = id.mid(1, id.size() - 2);
    return id;
}

}

Problem validate(const QByteArray &messageId)
{
    if (messageId.isEmpty())
        return Problem::Empty;
    if (messageId.size() > maximumLength)
        return Problem::TooLong;
    for (const char c : messageId) {
        const auto byte = static_cast<unsigned char>(c);
        // Bytes above 0x7F are allowed, for IDs written in UTF-8 (RFC 6532)
        if (byte <= 0x20 || byte == 0x7F || c == '<' || c == '>')
            return Problem::InvalidCharacter;
    }
    const int at = messageId.lastIndexOf('@');
    if (at <= 0 || at == messageId.size() - 1)
        return Problem::MissingAt;
    return Problem::None;
}

Parsed parse(const QString &input)
{
    Parsed result;
    const QString text = input.trimmed();
    QByteArray messageId;
    if (text.startsWith(QLatin1String("mid:"), Qt::CaseInsensitive)) {
        const QByteArray rest = text.mid(4).toUtf8();
        const int slash = rest.indexOf('/');
        messageId = withoutBrackets(QByteArray::fromPercentEncoding(slash < 0 ? rest : rest.left(slash)));
        if (slash >= 0)
            result.contentId = withoutBrackets(QByteArray::fromPercentEncoding(rest.mid(slash + 1)));
    } else {
        messageId = withoutBrackets(text.toUtf8());
    }
    result.problem = validate(messageId);
    if (result.problem == Problem::None)
        result.messageId = messageId;
    else
        result.contentId.clear();
    return result;
}

QUrl url(const QByteArray &messageId)
{
    // RFC 2392 percent-encodes the characters which a URL cannot hold; "@" stays readable
    const QByteArray encoded = QUrl::toPercentEncoding(QString::fromUtf8(withoutBrackets(messageId)), QByteArrayLiteral("@!$&'()*+,;=:"));
    return QUrl(QStringLiteral("mid:") + QString::fromLatin1(encoded));
}

QStringList searchConditions(const QByteArray &messageId)
{
    return {QStringLiteral("HEADER"), QStringLiteral("Message-ID"), QString::fromUtf8(withoutBrackets(messageId))};
}

bool sameId(const QByteArray &a, const QByteArray &b)
{
    const QByteArray left = withoutBrackets(a);
    return !left.isEmpty() && left == withoutBrackets(b);
}

}
}
