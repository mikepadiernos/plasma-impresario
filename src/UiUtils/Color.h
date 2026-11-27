/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef TROJITA_UIUTILS_COLOR_H
#define TROJITA_UIUTILS_COLOR_H

#include <QtGlobal>

class QColor;

namespace UiUtils {

QColor tintColor(const QColor &color, const QColor &tintColor);

/** @short WCAG 2 relative luminance of an opaque color, from 0 (black) to 1 (white) */
qreal relativeLuminance(const QColor &color);

/** @short WCAG 2 contrast ratio between two opaque colors, from 1 (none) to 21 (black on white) */
qreal contrastRatio(const QColor &first, const QColor &second);

/** @short Black or white, whichever contrasts more with @arg background */
QColor readableTextColor(const QColor &background);

/** @short @arg preferred when it reaches @arg minimum contrast against @arg background

Otherwise its lightness is moved away from the background, keeping the hue, until the
contrast is reached; black or white is the last resort. The default is the WCAG AA
level for normal text.
*/
QColor ensureContrast(const QColor &preferred, const QColor &background, qreal minimum = 4.5);

}

#endif
