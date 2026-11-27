/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QColor>
#include <QTabBar>

#include "UiUtils/MailTabs.h"

namespace Gui {

/** @short The optional bar of mail tabs, in any shell region and either orientation

The bar only shows the tabs; the main window keeps the session, opens what a tab holds and saves it. Tabs are
closable while more than one is open and can be dragged into another order.
*/
class MailTabBar : public QTabBar
{
    Q_OBJECT
public:
    struct Entry {
        QString label;
        QString toolTip;
        /** The folder colour of the tab, else its account colour, if any */
        QColor color = QColor();
    };

    explicit MailTabBar(QWidget *parent = nullptr);

    /** @short Show @arg entries with @arg current chosen, without emitting tabActivated() */
    void setEntries(const QList<Entry> &entries, int current);
    /** @short Lay the tabs out for @arg region: side by side when @arg orientation is horizontal, stacked when vertical */
    void setPlacement(UiUtils::MailTabs::Region region, Qt::Orientation orientation);
    UiUtils::MailTabs::Region region() const;
    Qt::Orientation orientation() const;
    /** @short The colour of tab @arg index, which the Theme settings show as text, a tint or a strong fill */
    QColor tabColor(int index) const;

    /** @short The shape of the tabs for @arg region and @arg orientation, opening towards the content */
    static QTabBar::Shape shapeFor(UiUtils::MailTabs::Region region, Qt::Orientation orientation);

signals:
    /** @short The user chose tab @arg index */
    void tabActivated(int index);

private:
    /** @short Colour the text of the tabs as the colour strength of the Theme settings asks */
    void applyColors();

    QList<QColor> m_colors;
    UiUtils::MailTabs::Region m_region = UiUtils::MailTabs::Region::Header;
    Qt::Orientation m_orientation = Qt::Horizontal;
    bool m_updating = false;
};

}
