/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QColor>
#include <QModelIndex>
#include <QProxyStyle>

class QToolButton;

namespace Gui {

/** @short A view whose tree lines can take the colour of its folders */
class FolderColorSource {
public:
    virtual ~FolderColorSource() = default;
    /** @short The colour of the folder at @arg index, or an invalid colour */
    virtual QColor folderColorAt(const QModelIndex &index) const = 0;
};

/** @short Plasma's widget style with the appearance options of the Theme group
on top

Everything is passed to the widget style, Breeze in Plasma, unless an option
asks otherwise: spacing scales the style's margins and spacing; dividers, tree
lines, selection, hover, corner radii of item highlights, input fields and
tooltips, and status bar items are drawn here when they differ from the style.
Window corners stay with KWin, and controls such as buttons, combo boxes and
scroll bars keep the style's own shape.

Toolbar buttons with a menu show a small accent-coloured marker in a corner
instead of an arrow beside them; see addMenuMarker().
*/
class AppearanceStyle : public QProxyStyle {
    Q_OBJECT
public:
    explicit AppearanceStyle(QStyle *base = nullptr);

    /** @short Wrap the application's widget style, unless it is wrapped already
     */
    static void ensureInstalled();

    /** @short Let @arg button run its action when clicked and open its menu
    when held, right-clicked or on Alt+Down, with a corner marker for the menu

    The marker shows that a menu is there, not that anything is selected. The
    button keeps its accessible menu action and its focus frame.
    */
    static void addMenuMarker(QToolButton *button);
    /** @short Whether a selected item is filled with the selection colour, so
     * that its text uses the selected text colour */
    static bool selectionIsFilled();

    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override;
    int layoutSpacing(QSizePolicy::ControlType control1, QSizePolicy::ControlType control2, Qt::Orientation orientation,
                      const QStyleOption *option = nullptr, const QWidget *widget = nullptr) const override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget = nullptr) const override;
    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter,
                            const QWidget *widget = nullptr) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
                           const QWidget *widget = nullptr) const override;
    void polish(QWidget *widget) override;
    using QProxyStyle::polish;

private:
    bool drawItemPanel(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    bool drawBranch(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawMenuMarker(const QStyleOptionComplex *option, QPainter *painter) const;
};

} // namespace Gui
