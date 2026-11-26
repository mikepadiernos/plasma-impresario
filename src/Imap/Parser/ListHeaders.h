/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QByteArray>
#include <QByteArrayList>
#include <QMap>

namespace Imap {
namespace LowLevelParser {

/** @short The mailing list headers which the message list fetches with the other metadata

List-Id (RFC 2919) names the list; the others (RFC 2369) offer its actions. List-Post has its own parser and is not
included.
*/
QByteArrayList listHeaderNames();

/** @short The values of the headers of listHeaderNames() in @arg rawHeaders, keyed by lower-case name

Folded values are unfolded and their whitespace collapsed. A header which appears more than once keeps its first
value. Other headers are ignored.
*/
QMap<QByteArray, QByteArray> listHeaders(const QByteArray &rawHeaders);

}
}
