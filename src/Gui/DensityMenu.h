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

private:
    void applyDensity(bool comfortable);
    QTreeView *m_mailboxes;
    QTreeView *m_messages;
    KSharedConfigPtr m_config;
};

}
