/* Copyright (C) Roland Pallai <dap78@magex.hu>

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

#include "ColoredItemDelegate.h"
#include "UiUtils/Color.h"

namespace Gui
{

ColoredItemDelegate::ColoredItemDelegate(QObject *parent) : DensityDelegate(parent)
{
}

void ColoredItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QColor itemForegroundColor = index.data(Qt::ForegroundRole).value<QColor>();
    paintWithForeground(painter, option, index, itemForegroundColor);
}

void ColoredItemDelegate::paintWithForeground(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index, const QColor &itemForegroundColor) const
{
    QStyleOptionViewItem viewOption(option);
    applyForegroundToSelection(viewOption, itemForegroundColor);
    QStyledItemDelegate::paint(painter, viewOption, index);
}

void ColoredItemDelegate::applyForegroundToSelection(QStyleOptionViewItem &viewOption, const QColor &itemForegroundColor)
{
    if (itemForegroundColor.isValid()) {
        // Thunderbird works like this and we like it
        viewOption.palette.setColor(QPalette::ColorGroup::Active, QPalette::Highlight, itemForegroundColor);

        // Keep the selected text readable on the tag color, at the WCAG AA contrast for normal text
        const QColor highlightedText = viewOption.palette.color(QPalette::ColorGroup::Active, QPalette::HighlightedText);
        viewOption.palette.setColor(QPalette::ColorGroup::Active, QPalette::HighlightedText,
                                    UiUtils::ensureContrast(highlightedText, itemForegroundColor));

        viewOption.palette.setColor(QPalette::ColorGroup::Inactive, QPalette::HighlightedText, itemForegroundColor);
    }
}

}
