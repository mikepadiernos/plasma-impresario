/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "ListHeaders.h"

namespace Imap {
namespace LowLevelParser {

QByteArrayList listHeaderNames()
{
    return {
        QByteArrayLiteral("list-id"),
        QByteArrayLiteral("list-unsubscribe"),
        QByteArrayLiteral("list-subscribe"),
        QByteArrayLiteral("list-help"),
        QByteArrayLiteral("list-archive"),
        QByteArrayLiteral("list-owner"),
    };
}

QMap<QByteArray, QByteArray> listHeaders(const QByteArray &rawHeaders)
{
    static const QByteArrayList names = listHeaderNames();
    QMap<QByteArray, QByteArray> result;
    QByteArray name;
    QByteArray value;
    const auto finish = [&]() {
        if (!name.isEmpty() && names.contains(name) && !result.contains(name))
            result.insert(name, value.simplified());
        name.clear();
        value.clear();
    };
    for (QByteArray line : rawHeaders.split('\n')) {
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.isEmpty()) {
            finish();
            continue;
        }
        if (line.front() == ' ' || line.front() == '\t') {
            if (!name.isEmpty())
                value += ' ' + line.trimmed();
            continue;
        }
        finish();
        const qsizetype colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        name = line.left(colon).trimmed().toLower();
        value = line.mid(colon + 1).trimmed();
    }
    finish();
    return result;
}

}
}
