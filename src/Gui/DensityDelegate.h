/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QStyledItemDelegate>

namespace Gui {

/** @short Rows with compact or comfortable spacing

In a view which is a FolderColorSource, such as the folder tree, folders also
show their colours: as a coloured folder icon, a dot or a coloured name, as the
folder colour style of the Theme settings asks.
*/
class DensityDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit DensityDelegate(QObject *parent = nullptr);
    /** Set whether rows use comfortable spacing. */
    void setComfortable(bool comfortable);
    bool isComfortable() const { return m_comfortable; }
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;

private:
    QColor folderColor(const QModelIndex &index) const;
    bool m_comfortable = false;
};

} // namespace Gui
