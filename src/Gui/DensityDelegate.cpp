/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "DensityDelegate.h"

#include <QStyleOptionViewItem>

namespace Gui {

DensityDelegate::DensityDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
}

void DensityDelegate::setComfortable(bool comfortable)
{
    m_comfortable = comfortable;
}

QSize DensityDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    if (m_comfortable)
        size.rheight() += qMax(4, option.fontMetrics.height() / 2);
    return size;
}

}
