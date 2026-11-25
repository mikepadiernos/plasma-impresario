/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <KSharedConfig>
#include <QMenu>

class QTreeView;

namespace Gui {

class DensityMenu : public QMenu
{
    Q_OBJECT
public:
    /** Create a density selector for the mailbox and message views. */
    DensityMenu(QTreeView *mailboxes, QTreeView *messages, KSharedConfigPtr config, QWidget *parent = nullptr);

    bool isComfortable() const;
    /** Switch the density as if chosen from the menu, so that the choice is saved */
    void setComfortable(bool comfortable);

private:
    void applyDensity(bool comfortable);
    QTreeView *m_mailboxes;
    QTreeView *m_messages;
    KSharedConfigPtr m_config;
    QAction *m_compact;
    QAction *m_comfortable;
};

}
