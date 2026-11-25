/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <KSharedConfig>
#include <QMenu>

namespace Gui {

class MsgListView;

/** @short Choice between two-line message rows and single-line columns, saved in the Appearance settings */
class MessageRowsMenu : public QMenu
{
    Q_OBJECT
public:
    MessageRowsMenu(MsgListView *messages, KSharedConfigPtr config, QWidget *parent = nullptr);

    bool isMultiLine() const;
    /** @short Switch the layout as if chosen from the menu, so that the choice is saved */
    void setMultiLine(bool multiLine);
    QAction *senderInitialsAction() const { return m_senderInitials; }

private:
    void apply();
    void save();
    MsgListView *m_messages;
    KSharedConfigPtr m_config;
    QAction *m_multiLine;
    QAction *m_columns;
    QAction *m_senderInitials;
};

}
