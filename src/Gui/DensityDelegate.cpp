/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "DensityDelegate.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QPainter>
#include <QStyleOptionViewItem>

#include "AppearanceManager.h"
#include "AppearanceStyle.h"
#include "UiUtils/Color.h"
#include "UiUtils/FolderColors.h"

namespace Gui {

namespace {

/** @short Whether @arg icon is a plain folder, which a coloured folder icon can
 * replace */
bool isPlainFolder(const QIcon &icon)
{
    const QString name = icon.name();
    return name == QLatin1String("folder") || name == QLatin1String("folder-open") ||
        name == QLatin1String("folder-symbolic") || name == QLatin1String("mail-folder") ||
        name == QLatin1String("inode-directory");
}

QIcon tinted(const QIcon &icon, const QSize &size, const QColor &color, qreal ratio)
{
    QPixmap pixmap = icon.pixmap(size, ratio);
    if (pixmap.isNull())
        return icon;
    QPainter painter(&pixmap);
    // The icon keeps its shading under the colour
    painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    QColor tint(color);
    tint.setAlphaF(0.55);
    painter.fillRect(pixmap.rect(), tint);
    painter.end();
    return QIcon(pixmap);
}

} // namespace

DensityDelegate::DensityDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
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

QColor DensityDelegate::folderColor(const QModelIndex &index) const
{
    const auto *source = dynamic_cast<const FolderColorSource *>(parent());
    return source ? source->folderColorAt(index) : QColor();
}

void DensityDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
    QStyledItemDelegate::initStyleOption(option, index);
    const QColor color = folderColor(index);
    if (!color.isValid())
        return;
    const QString style = AppearanceManager::instance()->value(QStringLiteral("FolderColorStyle")).toString();
    if (style == QLatin1String("Icon") && isPlainFolder(option->icon)) {
        const QString name = UiUtils::FolderColors::iconName(color);
        const qreal ratio = option->widget ? option->widget->devicePixelRatioF() : qApp->devicePixelRatio();
        option->icon = QIcon::hasThemeIcon(name) ? QIcon::fromTheme(name)
                                                 : tinted(option->icon, option->decorationSize, color, ratio);
    } else if (style == QLatin1String("Name")) {
        for (QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
            const QColor base = option->palette.color(group, QPalette::Base);
            option->palette.setColor(group, QPalette::Text, UiUtils::ensureContrast(color, base));
        }
    }
}

void DensityDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyledItemDelegate::paint(painter, option, index);
    const QColor color = folderColor(index);
    if (!color.isValid())
        return;
    const QString style = AppearanceManager::instance()->value(QStringLiteral("FolderColorStyle")).toString();
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    const bool badge = style == QLatin1String("Badge");
    // Folders with their own icon, such as the inbox, keep it and show the colour
    // as a dot on its corner
    const bool iconDot = style == QLatin1String("Icon") && !isPlainFolder(opt.icon) && !opt.icon.isNull();
    if (!badge && !iconDot)
        return;
    QStyle *itemStyle = opt.widget ? opt.widget->style() : QApplication::style();
    const int dot = qMax(6, opt.fontMetrics.height() / 3);
    QRect dotRect(0, 0, dot, dot);
    if (iconDot) {
        const QRect icon = itemStyle->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, opt.widget);
        dotRect.moveCenter(opt.direction == Qt::RightToLeft ? icon.bottomLeft() : icon.bottomRight());
        dotRect.translate(opt.direction == Qt::RightToLeft ? dot / 4 : -dot / 4, -dot / 4);
    } else {
        const QRect text = itemStyle->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);
        const int margin = itemStyle->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, opt.widget) + 2;
        dotRect.moveCenter(
            QPoint(opt.direction == Qt::RightToLeft ? text.left() + margin + dot / 2 : text.right() - margin - dot / 2,
                   text.center().y()));
    }
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    // A ring in the base colour keeps the dot visible on the icon and on the
    // selection
    painter->setPen(QPen(opt.palette.color(QPalette::Base), 1));
    painter->setBrush(color);
    painter->drawEllipse(dotRect);
    painter->restore();
}

} // namespace Gui
