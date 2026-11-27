/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/AppearanceStyle.h"

#include <KLocalizedString>
#include <QAbstractItemView>
#include <QApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QSplitterHandle>
#include <QStyleFactory>
#include <QStyleOption>
#include <QToolButton>
#include <QTreeView>

#include "Gui/AppearanceManager.h"
#include "Gui/MailTabBar.h"
#include "UiUtils/Color.h"
#include "UiUtils/Theme.h"

namespace Gui {

namespace Theme = UiUtils::Theme;

namespace {

constexpr auto MenuMarkerProperty = "plasmaImpresarioMenuMarker";

QString option(const char *entry)
{
    return AppearanceManager::instance()->value(QLatin1String(entry)).toString();
}

bool followsStyle(const char *entry)
{
    return option(entry) == QLatin1String("FollowStyle");
}

int scaledRadius(const QWidget *widget)
{
    const int radius = AppearanceManager::instance()->cornerRadius();
    if (radius <= 0)
        return radius;
    // Device-independent pixels, which Qt scales for the display; text size
    // scales them along with the font
    const qreal fontScale = widget ? widget->fontMetrics().height() / 17.0 : 1.0;
    return qMax(1, qRound(radius * qBound(0.75, fontScale, 2.0)));
}

QColor neutralLine(const QPalette &palette)
{
    const QColor text = palette.color(QPalette::Text);
    const QColor base = palette.color(QPalette::Base);
    return QColor::fromRgbF(text.redF() * 0.35 + base.redF() * 0.65, text.greenF() * 0.35 + base.greenF() * 0.65,
                            text.blueF() * 0.35 + base.blueF() * 0.65);
}

/** @short A rounded rectangle for a cell of a row, rounding only the row's
 * outer corners */
QPainterPath cellPath(const QRectF &rect, qreal radius, QStyleOptionViewItem::ViewItemPosition position,
                      Qt::LayoutDirection direction)
{
    QPainterPath path;
    if (radius <= 0) {
        path.addRect(rect);
        return path;
    }
    using Position = QStyleOptionViewItem::ViewItemPosition;
    bool leading = position == Position::OnlyOne || position == Position::Invalid || position == Position::Beginning;
    bool trailing = position == Position::OnlyOne || position == Position::Invalid || position == Position::End;
    if (direction == Qt::RightToLeft)
        std::swap(leading, trailing);
    if (leading && trailing) {
        path.addRoundedRect(rect, radius, radius);
        return path;
    }
    QPainterPath rounded;
    rounded.addRoundedRect(rect.adjusted(leading ? 0 : -2 * radius, 0, trailing ? 0 : 2 * radius, 0), radius, radius);
    QPainterPath clip;
    clip.addRect(rect);
    return rounded.intersected(clip);
}

/** @short The edge of a tab which faces the content it shows */
Qt::Edge contentEdge(QTabBar::Shape shape)
{
    switch (shape) {
    case QTabBar::RoundedSouth:
    case QTabBar::TriangularSouth:
        return Qt::TopEdge;
    case QTabBar::RoundedWest:
    case QTabBar::TriangularWest:
        return Qt::RightEdge;
    case QTabBar::RoundedEast:
    case QTabBar::TriangularEast:
        return Qt::LeftEdge;
    default:
        return Qt::BottomEdge;
    }
}

/** @short The outline of a tab with rounded corners away from the content */
QPainterPath tabPath(const QRectF &rect, qreal radius, Qt::Edge content)
{
    QPainterPath path;
    if (radius <= 0) {
        path.addRect(rect);
        return path;
    }
    // Extend the rectangle under the content edge, so that only the outer corners are rounded
    QRectF rounded = rect;
    switch (content) {
    case Qt::TopEdge:
        rounded.setTop(rect.top() - radius);
        break;
    case Qt::BottomEdge:
        rounded.setBottom(rect.bottom() + radius);
        break;
    case Qt::LeftEdge:
        rounded.setLeft(rect.left() - radius);
        break;
    case Qt::RightEdge:
        rounded.setRight(rect.right() + radius);
        break;
    }
    path.addRoundedRect(rounded, radius, radius);
    QPainterPath clip;
    clip.addRect(rect);
    return path.intersected(clip);
}

QRect edgeLine(const QRect &rect, Qt::Edge edge, int width)
{
    switch (edge) {
    case Qt::TopEdge:
        return QRect(rect.left(), rect.top(), rect.width(), width);
    case Qt::BottomEdge:
        return QRect(rect.left(), rect.bottom() - width + 1, rect.width(), width);
    case Qt::LeftEdge:
        return QRect(rect.left(), rect.top(), width, rect.height());
    case Qt::RightEdge:
        return QRect(rect.right() - width + 1, rect.top(), width, rect.height());
    }
    return QRect();
}

/** @short Opens the menu of a button with a corner marker from the keyboard and
 * the context menu */
class MenuOpener : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        auto *button = qobject_cast<QToolButton *>(watched);
        if (!button || !button->menu())
            return false;
        if (event->type() == QEvent::ContextMenu) {
            button->showMenu();
            return true;
        }
        if (event->type() == QEvent::KeyPress) {
            const auto *key = static_cast<QKeyEvent *>(event);
            if ((key->key() == Qt::Key_Down && key->modifiers() == Qt::AltModifier) || key->key() == Qt::Key_Menu) {
                button->showMenu();
                return true;
            }
        }
        return false;
    }
};

} // namespace

AppearanceStyle::AppearanceStyle(QStyle *base)
    : QProxyStyle(base)
{
}

void AppearanceStyle::ensureInstalled()
{
    if (qobject_cast<AppearanceStyle *>(QApplication::style()))
        return;
    QStyle *base = QStyleFactory::create(QApplication::style()->name());
    QApplication::setStyle(new AppearanceStyle(base));
}

void AppearanceStyle::addMenuMarker(QToolButton *button)
{
    static QPointer<MenuOpener> opener;
    if (!opener)
        opener = new MenuOpener(qApp);
    if (button->popupMode() == QToolButton::MenuButtonPopup)
        button->setPopupMode(QToolButton::DelayedPopup);
    button->setProperty(MenuMarkerProperty, true);
    button->installEventFilter(opener);
    button->setAccessibleDescription(
        i18nc("@info:whatsthis", "Has a menu: press and hold, right-click or press Alt+Down to open it"));
}

bool AppearanceStyle::selectionIsFilled()
{
    const QString style = option("SelectionStyle");
    return style != QLatin1String("Outlined") && style != QLatin1String("AccentBar");
}

int AppearanceStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    const int value = QProxyStyle::pixelMetric(metric, option, widget);
    const int scale = AppearanceManager::instance()->spacingScale();
    if (scale == 100 || value <= 0)
        return value;
    switch (metric) {
    case PM_LayoutLeftMargin:
    case PM_LayoutTopMargin:
    case PM_LayoutRightMargin:
    case PM_LayoutBottomMargin:
    case PM_LayoutHorizontalSpacing:
    case PM_LayoutVerticalSpacing:
    case PM_ToolBarItemSpacing:
    case PM_ToolBarItemMargin:
    case PM_TabBarTabHSpace:
    case PM_TabBarTabVSpace:
    case PM_ButtonMargin:
    case PM_MenuBarItemSpacing:
    case PM_FocusFrameHMargin:
        return Theme::scaled(value, scale);
    default:
        return value;
    }
}

int AppearanceStyle::layoutSpacing(QSizePolicy::ControlType control1, QSizePolicy::ControlType control2,
                                   Qt::Orientation orientation, const QStyleOption *option, const QWidget *widget) const
{
    const int value = QProxyStyle::layoutSpacing(control1, control2, orientation, option, widget);
    const int scale = AppearanceManager::instance()->spacingScale();
    return scale == 100 || value <= 0 ? value : Theme::scaled(value, scale);
}

bool AppearanceStyle::drawItemPanel(const QStyleOption *opt, QPainter *painter, const QWidget *widget) const
{
    const auto *item = qstyleoption_cast<const QStyleOptionViewItem *>(opt);
    if (!item)
        return false;
    const QString style = option("SelectionStyle");
    const int radius = scaledRadius(widget);
    const bool hover = AppearanceManager::instance()->value(QStringLiteral("HoverHighlight")).toBool();
    if (style == QLatin1String("FollowStyle") && radius < 0) {
        if (hover)
            return false;
        QStyleOptionViewItem plain(*item);
        plain.state &= ~State_MouseOver;
        QProxyStyle::drawPrimitive(PE_PanelItemViewItem, &plain, painter, widget);
        return true;
    }

    const bool selected = item->state & State_Selected;
    const bool hovered = hover && (item->state & State_MouseOver) && !selected;
    const QPalette::ColorGroup group = !(item->state & State_Enabled) ? QPalette::Disabled
        : (item->state & State_Active)                                ? QPalette::Active
                                                                      : QPalette::Inactive;
    QColor highlight = item->palette.color(group, QPalette::Highlight);
    const qreal r = style == QLatin1String("Rounded") ? qMax(radius, 6) : qMax(radius, 0);
    const QRectF rect = QRectF(item->rect).adjusted(0.5, 0.5, -0.5, -0.5);
    const QPainterPath path = cellPath(rect, r, item->viewItemPosition, item->direction);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (item->backgroundBrush.style() != Qt::NoBrush)
        painter->fillPath(path, item->backgroundBrush);
    if (selected) {
        if (style == QLatin1String("Outlined")) {
            QColor fill = highlight;
            fill.setAlphaF(0.12);
            painter->fillPath(path, fill);
            painter->setPen(QPen(highlight, 1.5));
            painter->drawPath(
                cellPath(rect.adjusted(0.75, 0.75, -0.75, -0.75), r, item->viewItemPosition, item->direction));
        } else if (style == QLatin1String("AccentBar")) {
            QColor fill = highlight;
            fill.setAlphaF(0.22);
            painter->fillPath(path, fill);
            using Position = QStyleOptionViewItem::ViewItemPosition;
            if (item->viewItemPosition == Position::Beginning || item->viewItemPosition == Position::OnlyOne ||
                item->viewItemPosition == Position::Invalid) {
                const int bar = qMax(3, item->rect.height() / 8);
                const QRect barRect = QStyle::visualRect(
                    item->direction, item->rect, QRect(item->rect.left(), item->rect.top(), bar, item->rect.height()));
                painter->fillRect(barRect, AppearanceManager::instance()->accent());
            }
        } else {
            painter->fillPath(path, highlight);
        }
    } else if (hovered) {
        highlight.setAlphaF(0.15);
        painter->fillPath(path, highlight);
    }
    painter->restore();
    return true;
}

bool AppearanceStyle::drawBranch(const QStyleOption *opt, QPainter *painter, const QWidget *widget) const
{
    const QString lines = option("TreeLines");
    if (lines == QLatin1String("FollowStyle"))
        return false;
    QStyleOption arrow(*opt);
    arrow.state &= ~(State_Sibling | State_Item);
    if (lines == QLatin1String("Hidden")) {
        QProxyStyle::drawPrimitive(PE_IndicatorBranch, &arrow, painter, widget);
        return true;
    }

    int depth = 0;
    QColor folderColor;
    if (const auto *view = qobject_cast<const QTreeView *>(widget)) {
        const int indentation = qMax(1, view->indentation());
        const int column = view->header()->sectionViewportPosition(view->header()->logicalIndex(0));
        const int offset = opt->direction == Qt::RightToLeft
            ? column + view->header()->sectionSize(0) - opt->rect.right()
            : opt->rect.left() - column;
        depth = qMax(0, offset / indentation - (view->rootIsDecorated() ? 0 : 1));
        if (const auto *source = dynamic_cast<const FolderColorSource *>(view)) {
            QModelIndex index = view->indexAt(QPoint(opt->rect.center().x(), opt->rect.center().y()));
            int indexDepth = 0;
            for (QModelIndex parent = index.parent(); parent.isValid(); parent = parent.parent())
                ++indexDepth;
            while (index.isValid() && indexDepth > depth) {
                index = index.parent();
                --indexDepth;
            }
            folderColor = source->folderColorAt(index);
        }
    }
    const QColor color =
        Theme::treeLineColor(AppearanceManager::instance()->values(), depth, folderColor, neutralLine(opt->palette));

    painter->save();
    QPen pen(color, 1);
    if (lines == QLatin1String("Dotted"))
        pen.setStyle(Qt::DotLine);
    painter->setPen(pen);
    const QRect r = opt->rect;
    const int x = r.center().x();
    const int y = r.center().y();
    if (opt->state & State_Item) {
        // A line from the parent's line to this item, and down to the next sibling
        painter->drawLine(x, r.top(), x, (opt->state & State_Sibling) ? r.bottom() : y);
        const int end = opt->direction == Qt::RightToLeft ? r.left() : r.right();
        if (!(opt->state & State_Children))
            painter->drawLine(x, y, end, y);
        else
            painter->drawLine(opt->direction == Qt::RightToLeft ? x - 5 : x + 5, y, end, y);
    } else if (opt->state & State_Sibling) {
        painter->drawLine(x, r.top(), x, r.bottom());
    }
    painter->restore();
    QProxyStyle::drawPrimitive(PE_IndicatorBranch, &arrow, painter, widget);
    return true;
}

void AppearanceStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                                    const QWidget *widget) const
{
    switch (element) {
    case PE_PanelItemViewItem:
        if (drawItemPanel(option, painter, widget))
            return;
        break;
    case PE_IndicatorBranch:
        if (drawBranch(option, painter, widget))
            return;
        break;
    case PE_IndicatorToolBarSeparator: {
        const QString dividers = Gui::option("Dividers");
        if (dividers == QLatin1String("Hidden"))
            return;
        if (dividers == QLatin1String("Thin")) {
            painter->save();
            painter->setPen(neutralLine(option->palette));
            const QRect r = option->rect;
            if (option->state & State_Horizontal)
                painter->drawLine(r.center().x(), r.top() + 2, r.center().x(), r.bottom() - 2);
            else
                painter->drawLine(r.left() + 2, r.center().y(), r.right() - 2, r.center().y());
            painter->restore();
            return;
        }
        break;
    }
    case PE_FrameStatusBarItem: {
        const QString style = Gui::option("StatusBarStyle");
        if (style == QLatin1String("FollowStyle"))
            break;
        painter->save();
        const QRect r = option->rect;
        if (style == QLatin1String("Shaded")) {
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            painter->setBrush(option->palette.brush(QPalette::AlternateBase));
            const qreal radius = qMax(0, scaledRadius(widget));
            painter->drawRoundedRect(QRectF(r).adjusted(0.5, 1.5, -0.5, -1.5), radius, radius);
        } else {
            // A line before each item, except where an item starts at the edge of the bar
            const bool rtl = option->direction == Qt::RightToLeft;
            const int x = rtl ? r.right() : r.left();
            const bool atEdge = widget && (rtl ? x >= widget->rect().right() - 3 : x <= widget->rect().left() + 3);
            if (!atEdge) {
                painter->setPen(neutralLine(option->palette));
                painter->drawLine(x, r.top() + 3, x, r.bottom() - 3);
            }
        }
        painter->restore();
        return;
    }
    case PE_PanelLineEdit:
    case PE_PanelTipLabel: {
        const int radius = scaledRadius(widget);
        if (radius < 0)
            break;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF rect = QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5);
        const bool lineEdit = element == PE_PanelLineEdit;
        const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(option);
        if (lineEdit && frame && frame->lineWidth <= 0) {
            // A frameless field, such as the editor of an item, keeps the plain
            // background
            painter->fillRect(option->rect, option->palette.brush(QPalette::Base));
            painter->restore();
            return;
        }
        painter->setBrush(option->palette.brush(lineEdit ? QPalette::Base : QPalette::ToolTipBase));
        const bool focused = lineEdit && (option->state & State_HasFocus);
        painter->setPen(
            QPen(focused ? AppearanceManager::instance()->accent() : neutralLine(option->palette), focused ? 1.5 : 1));
        painter->drawRoundedRect(rect, radius, radius);
        painter->restore();
        return;
    }
    case PE_FrameLineEdit:
        if (scaledRadius(widget) >= 0)
            return;
        break;
    default:
        break;
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void AppearanceStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                                  const QWidget *widget) const
{
    switch (element) {
    case CE_ItemViewItem:
        if (!selectionIsFilled() && (option->state & State_Selected)) {
            if (const auto *item = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
                // The selection is not filled, so the selected text keeps the colour of
                // other text
                QStyleOptionViewItem readable(*item);
                for (QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled})
                    readable.palette.setColor(group, QPalette::HighlightedText,
                                              item->palette.color(group, QPalette::Text));
                QProxyStyle::drawControl(element, &readable, painter, widget);
                return;
            }
        }
        break;
    case CE_TabBarTabShape:
        if (drawTabShape(option, painter, widget))
            return;
        break;
    case CE_Splitter: {
        const QString dividers = Gui::option("Dividers");
        if (dividers == QLatin1String("FollowStyle"))
            break;
        const bool reveal = AppearanceManager::instance()->value(QStringLiteral("RevealDividersOnHover")).toBool() &&
            (option->state & (State_MouseOver | State_Sunken));
        if (dividers == QLatin1String("Hidden") && !reveal)
            return;
        // The handle keeps its width, so that it can still be dragged; only the
        // line is thin
        painter->save();
        painter->setPen(reveal ? AppearanceManager::instance()->accent() : neutralLine(option->palette));
        const QRect r = option->rect;
        if (option->state & State_Horizontal)
            painter->drawLine(r.center().x(), r.top(), r.center().x(), r.bottom());
        else
            painter->drawLine(r.left(), r.center().y(), r.right(), r.center().y());
        painter->restore();
        return;
    }
    case CE_ShapedFrame: {
        const QString dividers = Gui::option("Dividers");
        const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(option);
        if (!frame || dividers == QLatin1String("FollowStyle"))
            break;
        const bool line = frame->frameShape == QFrame::HLine || frame->frameShape == QFrame::VLine;
        const bool panel = frame->frameShape == QFrame::StyledPanel || frame->frameShape == QFrame::Panel ||
            frame->frameShape == QFrame::Box || frame->frameShape == QFrame::WinPanel;
        if (!line && !panel)
            break;
        if (dividers == QLatin1String("Hidden"))
            return;
        painter->save();
        painter->setPen(neutralLine(option->palette));
        const QRect r = option->rect;
        if (frame->frameShape == QFrame::HLine)
            painter->drawLine(r.left(), r.center().y(), r.right(), r.center().y());
        else if (frame->frameShape == QFrame::VLine)
            painter->drawLine(r.center().x(), r.top(), r.center().x(), r.bottom());
        else
            painter->drawRect(r.adjusted(0, 0, -1, -1));
        painter->restore();
        return;
    }
    case CE_HeaderSection: {
        const QString dividers = Gui::option("Dividers");
        if (dividers == QLatin1String("FollowStyle"))
            break;
        painter->fillRect(option->rect, option->palette.brush(QPalette::Button));
        if (dividers == QLatin1String("Thin")) {
            painter->save();
            painter->setPen(neutralLine(option->palette));
            const QRect r = option->rect;
            const int x = option->direction == Qt::RightToLeft ? r.left() : r.right();
            painter->drawLine(x, r.top() + 3, x, r.bottom() - 3);
            painter->drawLine(r.left(), r.bottom(), r.right(), r.bottom());
            painter->restore();
        }
        return;
    }
    default:
        break;
    }
    QProxyStyle::drawControl(element, option, painter, widget);
}

bool AppearanceStyle::drawTabShape(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    const auto *bar = qobject_cast<const MailTabBar *>(widget);
    const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option);
    if (!bar || !tab)
        return false;
    const QString shape = Gui::option("TabShape");
    const QString indicator = Gui::option("TabIndicator");
    const QString strength = Gui::option("TabColorStrength");
    const QColor color = bar->tabColor(tab->tabIndex);
    const bool fill = color.isValid() && strength != QLatin1String("Text");
    const bool followStyle = shape == QLatin1String("FollowStyle");
    // The defaults leave the tabs to the style, with its own indicator
    if (followStyle && !fill && indicator != QLatin1String("ThickLine"))
        return false;

    const bool selected = tab->state & State_Selected;
    const bool hovered = (tab->state & State_MouseOver) && (tab->state & State_Enabled);
    const Qt::Edge content = contentEdge(tab->shape);
    const QRect r = tab->rect;
    qreal radius = 0;
    if (shape == QLatin1String("Rounded")) {
        const int chosen = scaledRadius(widget);
        radius = chosen > 0 ? chosen : qMax(3, widget->fontMetrics().height() / 4);
    }
    const QPainterPath path = tabPath(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), radius, content);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (followStyle) {
        QProxyStyle::drawControl(CE_TabBarTabShape, option, painter, widget);
    } else {
        const QColor window = option->palette.color(QPalette::Window);
        QColor background;
        if (selected)
            background = option->palette.color(QPalette::Base);
        else if (hovered || shape != QLatin1String("Flat")) {
            QColor shade = option->palette.color(QPalette::WindowText);
            shade.setAlphaF(hovered ? 0.1 : 0.04);
            background = UiUtils::tintColor(window, shade);
        }
        if (background.isValid())
            painter->fillPath(path, background);
    }
    if (fill) {
        QColor tint = color;
        const bool strong = strength == QLatin1String("Strong");
        // Strong fills stay close to the colour, which the tab bar picks the text colour against
        tint.setAlphaF(strong ? (selected ? 0.9 : 0.75) : (selected ? 0.3 : 0.16));
        painter->fillPath(path, tint);
    }
    if (selected && indicator != QLatin1String("None") && (!followStyle || indicator == QLatin1String("ThickLine"))) {
        const int width = indicator == QLatin1String("ThickLine") ? 4 : 2;
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->fillRect(edgeLine(r, content, width), AppearanceManager::instance()->accent());
    }
    painter->restore();
    return true;
}

void AppearanceStyle::drawMenuMarker(const QStyleOptionComplex *option, QPainter *painter) const
{
    const QRect r = option->rect.adjusted(2, 2, -2, -2);
    const int size = qBound(4, qMin(r.width(), r.height()) / 5, 8);
    QColor color = AppearanceManager::instance()->accent();
    if (!(option->state & State_Enabled))
        color = option->palette.color(QPalette::Disabled, QPalette::WindowText);
    const bool rtl = option->direction == Qt::RightToLeft;
    const QPoint corner = rtl ? r.bottomLeft() : r.bottomRight();
    const int dx = rtl ? size : -size;
    QPainterPath triangle;
    triangle.moveTo(corner + QPoint(0, 1));
    triangle.lineTo(corner + QPoint(dx, 1));
    triangle.lineTo(corner + QPoint(0, 1 - size));
    triangle.closeSubpath();
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->fillPath(triangle, color);
    painter->restore();
}

void AppearanceStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter,
                                         const QWidget *widget) const
{
    if (control == CC_ToolButton && widget && widget->property(MenuMarkerProperty).toBool()) {
        if (const auto *button = qstyleoption_cast<const QStyleOptionToolButton *>(option)) {
            QStyleOptionToolButton plain(*button);
            const bool hasMenu = plain.features & QStyleOptionToolButton::HasMenu;
            plain.features &= ~(QStyleOptionToolButton::HasMenu | QStyleOptionToolButton::MenuButtonPopup |
                                QStyleOptionToolButton::PopupDelay);
            plain.subControls &= ~SC_ToolButtonMenu;
            QProxyStyle::drawComplexControl(control, &plain, painter, widget);
            if (hasMenu)
                drawMenuMarker(button, painter);
            return;
        }
    }
    QProxyStyle::drawComplexControl(control, option, painter, widget);
}

QSize AppearanceStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
                                        const QWidget *widget) const
{
    if (type == CT_ToolButton && widget && widget->property(MenuMarkerProperty).toBool()) {
        if (const auto *button = qstyleoption_cast<const QStyleOptionToolButton *>(option)) {
            QStyleOptionToolButton plain(*button);
            plain.features &= ~(QStyleOptionToolButton::HasMenu | QStyleOptionToolButton::MenuButtonPopup |
                                QStyleOptionToolButton::PopupDelay);
            return QProxyStyle::sizeFromContents(type, &plain, size, widget);
        }
    }
    return QProxyStyle::sizeFromContents(type, option, size, widget);
}

void AppearanceStyle::polish(QWidget *widget)
{
    QProxyStyle::polish(widget);
    // Hidden dividers can show themselves while the pointer is over them
    if (qobject_cast<QSplitterHandle *>(widget))
        widget->setAttribute(Qt::WA_Hover);
}

} // namespace Gui
