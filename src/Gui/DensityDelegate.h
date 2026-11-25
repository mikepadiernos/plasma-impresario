/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QStyledItemDelegate>

namespace Gui {

class DensityDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit DensityDelegate(QObject *parent = nullptr);

    /** Set whether rows use comfortable spacing. */
    void setComfortable(bool comfortable);
    bool isComfortable() const { return m_comfortable; }
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    bool m_comfortable = false;
};

}
