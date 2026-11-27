/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef GUI_LINKTARGET_H
#define GUI_LINKTARGET_H

#include <QString>
#include <QStringList>
#include <QUrl>

namespace Gui {

/** @short Why a link from a message deserves a second look, as rich text

Each entry names what was found, such as link text which shows another site,
a user name or password before the host, a numeric address or a host name in
international characters together with its punycode form. Empty when nothing
was found.
@param url The link target.
@param text The link's text, or empty if unknown.
*/
QStringList linkWarningReasons(const QUrl &url, const QString &text);

/** @short Rich text with the full target of a link and any reasons for caution

Used for the tooltip while a link is hovered, the reader's context menu and
other places which show a link before it opens. The target is shown fully
encoded, with the host in its punycode form, and may be broken at any
punctuation. A very long target keeps its beginning and its end.
*/
QString linkTargetDescription(const QUrl &url, const QString &text);

}

#endif
