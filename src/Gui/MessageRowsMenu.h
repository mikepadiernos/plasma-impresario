/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <KSharedConfig>
#include <QMenu>

namespace Gui {

class MsgListView;

/** @short Choice between two-line message rows, cards and single-line columns

The layout is saved in the Appearance settings; cards are the message list style of the Theme settings, which a
workspace can override.
*/
class MessageRowsMenu : public QMenu
{
    Q_OBJECT
public:
    MessageRowsMenu(MsgListView *messages, KSharedConfigPtr config, QWidget *parent = nullptr);

    /** @short Whether messages take two lines, as rows or as cards */
    bool isMultiLine() const;
    /** @short Switch the layout as if chosen from the menu, so that the choice is saved */
    void setMultiLine(bool multiLine);
    bool isCards() const;
    /** @short Switch to cards, or back to two-line rows, as if chosen from the menu */
    void setCards(bool cards);
    QAction *senderInitialsAction() const { return m_senderInitials; }

signals:
    /** @short The message list style of the Theme settings was saved, so the appearance should be loaded again */
    void appearanceChanged();

private:
    void apply();
    void save(bool styleChosen);
    void followAppearance();
    MsgListView *m_messages;
    KSharedConfigPtr m_config;
    QAction *m_multiLine;
    QAction *m_columns;
    QAction *m_cards;
    QAction *m_senderInitials;
};

}
