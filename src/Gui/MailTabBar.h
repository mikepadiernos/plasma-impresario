/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

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
    };

    explicit MailTabBar(QWidget *parent = nullptr);

    /** @short Show @arg entries with @arg current chosen, without emitting tabActivated() */
    void setEntries(const QList<Entry> &entries, int current);
    /** @short Lay the tabs out for @arg region: side by side when @arg orientation is horizontal, stacked when vertical */
    void setPlacement(UiUtils::MailTabs::Region region, Qt::Orientation orientation);
    UiUtils::MailTabs::Region region() const;
    Qt::Orientation orientation() const;

    /** @short The shape of the tabs for @arg region and @arg orientation, opening towards the content */
    static QTabBar::Shape shapeFor(UiUtils::MailTabs::Region region, Qt::Orientation orientation);

signals:
    /** @short The user chose tab @arg index */
    void tabActivated(int index);

private:
    UiUtils::MailTabs::Region m_region = UiUtils::MailTabs::Region::Header;
    Qt::Orientation m_orientation = Qt::Horizontal;
    bool m_updating = false;
};

}
