/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef MSGLISTVIEW_H
#define MSGLISTVIEW_H

#include <QHeaderView>
#include <QTreeView>
#include <QPersistentModelIndex>
#include <functional>
#include <optional>
#include "Imap/Model/FavoriteTagsModel.h"

class QSignalMapper;

namespace Imap {
namespace Mailbox {
class PrettyMsgListModel;
}
}

namespace Gui {

/** @short A slightly tweaked QTreeView optimized for showing a list of messages in one mailbox

The optimizations (or rather modifications) include:
- automatically expanding a whole subtree when root item is expanded
- setting up reasonable size hints for all columns
- when the current message leaves the list, the next remaining one becomes current and the only selected message
- when messages arrive, leave or are sorted again, the current message stays where it was on the screen
*/
class MsgListView : public QTreeView
{
    Q_OBJECT
public:
    explicit MsgListView(QWidget *parent, Imap::Mailbox::FavoriteTagsModel *m_favoriteTagsModel);
    ~MsgListView() override {}
    void setModel(QAbstractItemModel *model) override;
    void setAutoActivateAfterKeyNavigation(bool enabled);
    void updateActionsAfterRestoredState();
    int sizeHintForColumn(int column) const override;
    QHeaderView::ResizeMode resizeModeForColumn(const int column) const;
    void keyboardSearch(const QString &search) override;
signals:
    /** @short Messages left the list, and @arg index became current and the only selected message

    Emitted when the current message was removed, or when the removal emptied the selection.
    */
    void currentReplacedAfterRemoval(const QModelIndex &index);
protected:
    void keyPressEvent(QKeyEvent *ke) override;
    void keyReleaseEvent(QKeyEvent *ke) override;
    void startDrag(Qt::DropActions supportedActions) override;
    bool event(QEvent *event) override;
    void rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end) override;
    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;
    /** @short Returns a list of all selected and related (collapsed) message indexes for column 0 */
    QModelIndexList selectedTree() const;
private slots:
    void slotFixSize();
    /** @short Expand all items below current root index */
    void slotExpandWholeSubtree(const QModelIndex &rootIndex);
    /** @short Update header actions for showing/hiding columns */
    void slotUpdateHeaderActions();
    /** @short Show/hide a corresponding column */
    void slotHeaderSectionVisibilityToggled(int section);
    /** @short Get the PrettyMsgListModel indexes of messages which are about to go away */
    void slotMsgListModelRowsAboutToBeRemoved(const QModelIndex &parent, int start, int end);
    /** @short Pick up the change of the sort critera */
    void slotHandleSortCriteriaChanged(int column, Qt::SortOrder order);
    /** @short conditionally emits activated(currentIndex()) for keyboard events */
    void slotCurrentActivated();
    void slotHandleNewColumns(int oldCount, int newCount);
private:
    /** @short Move the cursor away from the rows which @arg isRemoved reports, before they go */
    void prepareForRemoval(const std::function<bool(const QModelIndex &)> &isRemoved);
    /** @short Select the new current message once the rows are gone */
    void finishRemoval();
    static Imap::Mailbox::PrettyMsgListModel *findPrettyMsgListModel(QAbstractItemModel *model);
    /** @short Map an index of the PrettyMsgListModel to the model of the view, through proxies such as the groups */
    QModelIndex mapFromPrettyModel(const QModelIndex &prettyIndex) const;
    /** @short Remember where the current message, or else the top row, is on the screen before the rows change */
    void captureScrollAnchor(bool insertion);
    /** @short Scroll so that the remembered row is where it was */
    void restoreScrollAnchor();

    struct ScrollAnchor {
        QPersistentModelIndex index;
        int y = 0;
        /** At the top of the list before rows were inserted; stay there while the current message remains visible */
        bool keepTop = false;
    };
    std::optional<ScrollAnchor> m_scrollAnchor;
    int m_scrollAnchorDepth = 0;
    QList<QMetaObject::Connection> m_scrollAnchorConnections;

    QSignalMapper *headerFieldsMapper;
    QTimer *m_naviActivationTimer;
    bool m_autoActivateAfterKeyNavigation;
    bool m_autoResizeSections;
    bool m_currentReplacedByRemoval = false;
    bool m_selectionTouchedByRemoval = false;
    bool m_focusedBeforeRemoval = false;
    bool m_removalFinishQueued = false;

    friend class MainWindow; // needs access to slotHandleNewColumns
};

}

#endif // MSGLISTVIEW_H
