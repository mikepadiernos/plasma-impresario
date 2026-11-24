/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <KSharedConfig>
#include <QMenu>

#include "EmbeddedPanel.h"

namespace Gui {

/** @short View menu for showing, auto-hiding and placing embedded panels

Each panel gets a submenu. Preferences are stored in the Appearance group under the
panel's configuration key: <Key>Visible (default from the panel), <Key>AutoHide
(default false) and <Key>Placement (default from the panel). Action object names
are embeddedPanel<Id>, embeddedPanel<Id>AutoHide and embeddedPanel<Id>Placement<Position>.
*/
class EmbeddedPanelsMenu : public QMenu
{
    Q_OBJECT
public:
    explicit EmbeddedPanelsMenu(KSharedConfigPtr config, QWidget *parent = nullptr);

    /** Add a submenu for @arg panel and apply its saved preferences to it */
    void addPanel(EmbeddedPanel *panel);

    QAction *visibleAction(const EmbeddedPanel *panel) const;
    QAction *autoHideAction(const EmbeddedPanel *panel) const;
    QAction *placementAction(const EmbeddedPanel *panel, EmbeddedPanel::Placement placement) const;

private:
    struct Entry {
        EmbeddedPanel *panel;
        QAction *visible;
        QAction *autoHide;
        QList<QAction *> placements;
    };

    const Entry *entry(const EmbeddedPanel *panel) const;
    void save(const QString &key, const QString &value);

    KSharedConfigPtr m_config;
    QList<Entry> m_entries;
};

}
