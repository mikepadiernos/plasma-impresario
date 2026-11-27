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

#include "UiUtils/Color.h"
#include <QColor>
#include <cmath>

namespace UiUtils {

/** @short Return the source color tinted with the tintColor

This is shamelessly stolen from Qt5's qtquick1 module.
*/
QColor tintColor(const QColor &color, const QColor &tintColor)
{
    QColor finalColor;
    int a = tintColor.alpha();
    if (a == 0xff) {
        finalColor = tintColor;
    } else if (a == 0x00) {
        finalColor = color;
    } else {
        qreal a = tintColor.alphaF();
        qreal inv_a = 1.0 - a;

        finalColor.setRgbF(tintColor.redF() * a + color.redF() * inv_a,
                           tintColor.greenF() * a + color.greenF() * inv_a,
                           tintColor.blueF() * a + color.blueF() * inv_a,
                           a + inv_a * color.alphaF());
    }
    return finalColor;
}

namespace {

qreal linearChannel(qreal value)
{
    return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
}

}

qreal relativeLuminance(const QColor &color)
{
    const QColor rgb = color.toRgb();
    return 0.2126 * linearChannel(rgb.redF()) + 0.7152 * linearChannel(rgb.greenF()) + 0.0722 * linearChannel(rgb.blueF());
}

qreal contrastRatio(const QColor &first, const QColor &second)
{
    const qreal a = relativeLuminance(first);
    const qreal b = relativeLuminance(second);
    return (qMax(a, b) + 0.05) / (qMin(a, b) + 0.05);
}

QColor readableTextColor(const QColor &background)
{
    const QColor black(Qt::black);
    const QColor white(Qt::white);
    return contrastRatio(black, background) >= contrastRatio(white, background) ? black : white;
}

QColor ensureContrast(const QColor &preferred, const QColor &background, qreal minimum)
{
    if (contrastRatio(preferred, background) >= minimum)
        return preferred;
    const bool darken = contrastRatio(QColor(Qt::black), background) >= contrastRatio(QColor(Qt::white), background);
    const QColor hsl = preferred.toHsl();
    const qreal hue = qMax<qreal>(0, hsl.hslHueF());
    qreal lightness = hsl.lightnessF();
    for (int step = 0; step < 20; ++step) {
        lightness = qBound<qreal>(0, lightness + (darken ? -0.05 : 0.05), 1);
        const QColor candidate = QColor::fromHslF(hue, hsl.hslSaturationF(), lightness, preferred.alphaF()).toRgb();
        if (contrastRatio(candidate, background) >= minimum)
            return candidate;
    }
    return readableTextColor(background);
}

}
