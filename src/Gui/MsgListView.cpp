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
#include "MsgListView.h"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QDrag>
#include <QFontMetrics>
#include <QHeaderView>
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include <QScrollBar>
#include <QSet>
#include <QSignalMapper>
#include <QStyle>
#include <QTimer>
#include "MsgItemDelegate.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/PrettyMsgListModel.h"
#include "Imap/Model/ThreadingMsgListModel.h"
#include "UiUtils/SelectionAfterRemoval.h"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

namespace {

// left might collapse a thread, question is whether ending there (on closing the thread) should be
// taken as mail loading request (i don't think so, but it's sth. that needs to be figured over time)
// NOTICE: reasonably Triggers should be a (non strict) subset of Blockers (user changed his mind)

// the list of key events which pot. lead to loading a new message.
static constexpr auto gs_naviActivationTriggers = std::to_array<int>({ Qt::Key_Up, Qt::Key_Down, Qt::Key_Right, Qt::Key_Left,
                                                                       Qt::Key_PageUp, Qt::Key_PageDown, Qt::Key_Home, Qt::Key_End});

// the list of key events which cancel naviActivationTrigger induced action.
static constexpr auto gs_naviActivationBlockers = std::to_array<int>({Qt::Key_Up, Qt::Key_Down, Qt::Key_Left, Qt::Key_PageUp,
                                                                      Qt::Key_PageDown, Qt::Key_Home, Qt::Key_End});
}

namespace Gui
{

MsgListView::MsgListView(QWidget *parent, Imap::Mailbox::FavoriteTagsModel *m_favoriteTagsModel):
    QTreeView(parent), m_autoActivateAfterKeyNavigation(true), m_autoResizeSections(true)
{
    connect(header(), &QHeaderView::geometriesChanged, this, &MsgListView::slotFixSize);
    connect(this, &QTreeView::expanded, this, &MsgListView::slotExpandWholeSubtree);
    connect(header(), &QHeaderView::sectionCountChanged, this, &MsgListView::slotUpdateHeaderActions);
    header()->setContextMenuPolicy(Qt::ActionsContextMenu);
    headerFieldsMapper = new QSignalMapper(this);
    connect(headerFieldsMapper, &QSignalMapper::mappedInt, this, &MsgListView::slotHeaderSectionVisibilityToggled);

    setUniformRowHeights(true);
    setAllColumnsShowFocus(true);
    setSelectionMode(ExtendedSelection);
    setDragEnabled(true);
    setRootIsDecorated(false);
    // Some subthreads might be huuuuuuuuuuge, so prevent indenting them too heavily
    setIndentation(15);

    setItemDelegate(new MsgItemDelegate(this, m_favoriteTagsModel));
    // Hovering a multi-line row shows the toggles of its read state and flag
    viewport()->setAttribute(Qt::WA_Hover);

    setSortingEnabled(true);
    // By default, we don't do any sorting
    header()->setSortIndicator(-1, Qt::AscendingOrder);

    m_naviActivationTimer = new QTimer(this);
    m_naviActivationTimer->setSingleShot(true);
    connect(m_naviActivationTimer, &QTimer::timeout, this, &MsgListView::slotCurrentActivated);
}

void MsgListView::setMultiLineRows(bool enabled)
{
    auto *delegate = qobject_cast<MsgItemDelegate *>(itemDelegate());
    if (!delegate || (enabled == m_multiLineRows && delegate->isMultiLine() == enabled))
        return;
    m_multiLineRows = enabled;
    delegate->setMultiLine(enabled);
    setHeaderHidden(enabled);
    setHorizontalScrollBarPolicy(enabled ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    if (enabled)
        horizontalScrollBar()->setValue(0);
    doItemsLayout();
    viewport()->update();
}

void MsgListView::setSenderInitials(bool shown)
{
    if (auto *delegate = qobject_cast<MsgItemDelegate *>(itemDelegate())) {
        delegate->setSenderInitials(shown);
        viewport()->update();
    }
}

QModelIndex MsgListView::rowAt(int y) const
{
    for (int visual = 0; visual < header()->count(); ++visual) {
        const int logical = header()->logicalIndex(visual);
        if (header()->isSectionHidden(logical) || header()->sectionSize(logical) <= 0)
            continue;
        return QTreeView::indexAt(QPoint(header()->sectionViewportPosition(logical) + header()->sectionSize(logical) - 1, y));
    }
    return QModelIndex();
}

QRect MsgListView::multiLineRowRect(const QModelIndex &index, const QRect &row) const
{
    int depth = 0;
    for (QModelIndex parent = index.parent(); parent.isValid(); parent = parent.parent())
        ++depth;
    const int indent = indentation() * (depth + (rootIsDecorated() ? 1 : 0));
    QRect rect(0, row.top(), viewport()->width(), row.height());
    if (isRightToLeft())
        rect.setRight(rect.right() - indent);
    else
        rect.setLeft(indent);
    return rect;
}

QStyleOptionViewItem MsgListView::multiLineOption(const QModelIndex &index, const QRect &rect) const
{
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.rect = rect;
    option.index = index;
    if (selectionModel() && selectionModel()->isSelected(index))
        option.state |= QStyle::State_Selected;
    const QModelIndex current = currentIndex();
    if (hasFocus() && current.isValid() && current.row() == index.row() && current.parent() == index.parent())
        option.state |= QStyle::State_HasFocus;
    if (viewport()->underMouse() && rect.contains(viewport()->mapFromGlobal(QCursor::pos())))
        option.state |= QStyle::State_MouseOver;
    if (!isActiveWindow())
        option.state &= ~QStyle::State_Active;
    return option;
}

void MsgListView::drawRow(QPainter *painter, const QStyleOptionViewItem &options, const QModelIndex &index) const
{
    auto *delegate = qobject_cast<MsgItemDelegate *>(itemDelegate());
    if (!m_multiLineRows || !delegate || index.data(Imap::Mailbox::RoleIsGroupHeader).toBool()) {
        QTreeView::drawRow(painter, options, index);
        return;
    }
    const QModelIndex message = index.sibling(index.row(), 0);
    const QRect rect = multiLineRowRect(message, options.rect);
    if (rect.left() > 0 || rect.right() < viewport()->width() - 1) {
        const QRect branches = isRightToLeft() ? QRect(rect.right() + 1, rect.top(), viewport()->width() - rect.right() - 1, rect.height())
                                               : QRect(0, rect.top(), rect.left(), rect.height());
        drawBranches(painter, branches, message);
    }
    delegate->paintMultiLineRow(painter, multiLineOption(message, rect), message);
}

QModelIndex MsgListView::indexAt(const QPoint &point) const
{
    if (!m_multiLineRows)
        return QTreeView::indexAt(point);
    const QModelIndex row = rowAt(point.y());
    if (!row.isValid() || row.data(Imap::Mailbox::RoleIsGroupHeader).toBool())
        return row.isValid() ? row.sibling(row.row(), 0) : row;
    const QModelIndex message = row.sibling(row.row(), 0);
    auto *delegate = qobject_cast<MsgItemDelegate *>(itemDelegate());
    const QRect rect = multiLineRowRect(message, QTreeView::visualRect(row));
    if (delegate && model()->columnCount(message.parent()) > Imap::Mailbox::MsgListModel::FLAGGED) {
        // Clicking an indicator acts like clicking its column in the single-line layout
        switch (delegate->multiLineHit(multiLineOption(message, rect), message, point)) {
        case MsgItemDelegate::RowHit::ReadState:
            return message.sibling(message.row(), Imap::Mailbox::MsgListModel::SEEN);
        case MsgItemDelegate::RowHit::Flag:
            return message.sibling(message.row(), Imap::Mailbox::MsgListModel::FLAGGED);
        case MsgItemDelegate::RowHit::None:
            break;
        }
    }
    return message;
}

QRect MsgListView::visualRect(const QModelIndex &index) const
{
    if (!m_multiLineRows || !index.isValid())
        return QTreeView::visualRect(index);
    QRect rect = QTreeView::visualRect(index);
    for (int column = 0; !rect.isValid() && column < header()->count(); ++column) {
        if (!header()->isSectionHidden(column))
            rect = QTreeView::visualRect(index.sibling(index.row(), column));
    }
    if (rect.isValid()) {
        rect.setLeft(0);
        rect.setRight(viewport()->width() - 1);
    }
    return rect;
}

MsgListView::~MsgListView()
{
    // QWidget deletes the header after this destructor, and the header still reports its geometry
    // and section count while it goes away
    disconnect(header(), nullptr, this, nullptr);
}

void MsgListView::keyPressEvent(QKeyEvent *ke)
{
    const bool hasNaviActivationBlocker = std::find(gs_naviActivationBlockers.begin(),
                                                    gs_naviActivationBlockers.end(),
                                                    ke->key()) != gs_naviActivationBlockers.end();

    if (hasNaviActivationBlocker)
        m_naviActivationTimer->stop();

    // Ctrl moves the cursor without touching the selection. Most styles let QAbstractItemView do that already.
    if (ke->modifiers() == Qt::ControlModifier && selectionModel()
            && !style()->styleHint(QStyle::SH_ItemView_MovementWithoutUpdatingSelection, nullptr, this)) {
        std::optional<CursorAction> action;
        switch (ke->key()) {
        case Qt::Key_Up:
            action = MoveUp;
            break;
        case Qt::Key_Down:
            action = MoveDown;
            break;
        case Qt::Key_PageUp:
            action = MovePageUp;
            break;
        case Qt::Key_PageDown:
            action = MovePageDown;
            break;
        case Qt::Key_Home:
            action = MoveHome;
            break;
        case Qt::Key_End:
            action = MoveEnd;
            break;
        default:
            break;
        }
        if (action) {
            const QModelIndex next = moveCursor(*action, ke->modifiers());
            if (next.isValid()) {
                selectionModel()->setCurrentIndex(next, QItemSelectionModel::NoUpdate);
                scrollTo(next);
            }
            ke->accept();
            return;
        }
    }
    QTreeView::keyPressEvent(ke);
}

void MsgListView::keyboardSearch(const QString &search)
{
    // A letter which is not an enabled shortcut must not collapse a selection of several messages to one
    if (selectionModel() && selectionModel()->selectedRows().size() > 1)
        return;
    QTreeView::keyboardSearch(search);
}

void MsgListView::keyReleaseEvent(QKeyEvent *ke)
{
    const bool hasNaviActivationTrigger = std::find(gs_naviActivationTriggers.begin(),
                                                    gs_naviActivationTriggers.end(),
                                                    ke->key()) != gs_naviActivationTriggers.end();

    if (ke->modifiers() == Qt::NoModifier && hasNaviActivationTrigger)
        m_naviActivationTimer->start(150); // few ms for the user to re-orientate. 150ms is not much
    QTreeView::keyReleaseEvent(ke);
}

bool MsgListView::event(QEvent *event)
{
    const bool hasNaviActivationBlocker = std::find(gs_naviActivationBlockers.begin(),
                                                    gs_naviActivationBlockers.end(),
                                                    static_cast<QKeyEvent *>(event)->key()) != gs_naviActivationBlockers.end();

    if (event->type() == QEvent::ShortcutOverride
            && !hasNaviActivationBlocker
            && m_naviActivationTimer->isActive()) {
        // Make sure that the delayed timer is broken ASAP when the key looks like something which might possibly be a shortcut
        m_naviActivationTimer->stop();
        slotCurrentActivated();
    }
    return QTreeView::event(event);
}

void MsgListView::slotCurrentActivated()
{
    if (currentIndex().isValid() && m_autoActivateAfterKeyNavigation) {
        // The "current index" is the one with that funny dot which only triggers the read/unread status toggle.
        // If we don't do anything, subsequent pressing of key_up or key_down will move the cursor up/down one row
        // while preserving the column which will lead to toggling the read/unread state of *that* message.
        // That's unexpected; the key shall just move the cursor and change the current message.
        emit activated(currentIndex().sibling(currentIndex().row(), Imap::Mailbox::MsgListModel::SUBJECT));
    }
}

int MsgListView::sizeHintForColumn(int column) const
{
    QFont boldFont = font();
    boldFont.setBold(true);
    QFontMetrics metric(boldFont);
    switch (column) {
    case Imap::Mailbox::MsgListModel::SEEN:
        return 0;
    case Imap::Mailbox::MsgListModel::FLAGGED:
    case Imap::Mailbox::MsgListModel::ATTACHMENT:
        return style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, nullptr);
    case Imap::Mailbox::MsgListModel::SUBJECT:
        return metric.size(Qt::TextSingleLine, QStringLiteral("Blesmrt Trojita Foo Bar Random Text")).width();
    case Imap::Mailbox::MsgListModel::FROM:
    case Imap::Mailbox::MsgListModel::TO:
    case Imap::Mailbox::MsgListModel::CC:
    case Imap::Mailbox::MsgListModel::BCC:
        return metric.size(Qt::TextSingleLine, QStringLiteral("Blesmrt Trojita")).width();
    case Imap::Mailbox::MsgListModel::DATE:
    case Imap::Mailbox::MsgListModel::RECEIVED_DATE:
        return metric.size(Qt::TextSingleLine,
                           //: Translators: use a text which is returned for e-mails older than one day but newer than one week
                           //: (see UiUtils::Formatting::prettyDate() for the string formats); the idea here
                           //: is to have a text which is "wide enough" in a typical UI font.
                           //: The English version uses "Mon" because of the M letter; you should use something similar.
                           tr("Mon 10")).width();
    case Imap::Mailbox::MsgListModel::SIZE:
        return metric.size(Qt::TextSingleLine, tr("88.8 kB")).width();
    default:
        return QTreeView::sizeHintForColumn(column);
    }
}

/** @short Reimplemented to show custom pixmap during drag&drop

  Qt's model-view classes don't provide any means of interfering with the
  QDrag's pixmap so we just rip off QAbstractItemView::startDrag and provide
  our own QPixmap.
*/
void MsgListView::startDrag(Qt::DropActions supportedActions)
{
    // indexes for column 0, i.e. subject
    QModelIndexList baseIndexes;

    Q_FOREACH(const QModelIndex &index, selectedTree()) {
        if (model()->flags(index) & Qt::ItemIsDragEnabled) {
            Q_ASSERT(index.column() == Imap::Mailbox::MsgListModel::SUBJECT);
            baseIndexes << index;
        }
    }

    if (!baseIndexes.isEmpty()) {
        QMimeData *data = model()->mimeData(baseIndexes);
        if (!data)
            return;

        // use screen width and itemDelegate()->sizeHint() to determine size of the pixmap
        int screenWidth = screen()->availableGeometry().width();
        int maxWidth = qMax(400, screenWidth / 4);
        QSize size(maxWidth, 0);

        // Show a "+ X more items" text after so many entries
        const int maxItems = 20;

        QStyleOptionViewItem opt;
        opt.initFrom(this);
        opt.rect.setWidth(maxWidth);
        opt.rect.setHeight(itemDelegate()->sizeHint(opt, baseIndexes.at(0)).height());
        size.setHeight(qMin(maxItems + 1, baseIndexes.size()) * opt.rect.height());
        // State_Selected provides for nice background of the items
        opt.state |= QStyle::State_Selected;

        // paint list of selected items using itemDelegate() to be consistent with style
        QPixmap pixmap(size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);

        for (int i = 0; i < baseIndexes.size(); ++i) {
            opt.rect.moveTop(i * opt.rect.height());
            if (i == maxItems) {
                p.fillRect(opt.rect, palette().color(QPalette::Disabled, QPalette::Highlight));
                p.setBrush(palette().color(QPalette::Disabled, QPalette::HighlightedText));
                //~ singular + %n additional item
                //~ plural + %n additional items
                p.drawText(opt.rect, Qt::AlignRight, tr("+ %n additional item(s)", nullptr, baseIndexes.size() - maxItems));
                break;
            }
            itemDelegate()->paint(&p, opt, baseIndexes.at(i));
        }

        QDrag *drag = new QDrag(this);
        drag->setPixmap(pixmap);
        drag->setMimeData(data);
        drag->setHotSpot(QPoint(0, 0));

        Qt::DropAction dropAction = Qt::IgnoreAction;
        if (defaultDropAction() != Qt::IgnoreAction && (supportedActions & defaultDropAction()))
            dropAction = defaultDropAction();
        else if (supportedActions & Qt::CopyAction && dragDropMode() != QAbstractItemView::InternalMove)
            dropAction = Qt::CopyAction;
        if (drag->exec(supportedActions, dropAction) == Qt::MoveAction) {
            // QAbstractItemView::startDrag calls d->clearOrRemove() here
            if (!dragDropOverwriteMode()) {
                Q_FOREACH(const QModelIndex &index, baseIndexes) {
                    model()->removeRow(index.row(), index.parent());
                }
            } else {
                // we can't remove the rows so reset the items (i.e. the view is like a table)
                QModelIndexList list = selectedIndexes();
                for (int i = 0; i < list.size(); ++i) {
                    QModelIndex index = list.at(i);
                    QMap<int, QVariant> roles = model()->itemData(index);
                    for (QMap<int, QVariant>::Iterator it = roles.begin(); it != roles.end(); ++it)
                        it.value() = QVariant();
                    model()->setItemData(index, roles);
                }
            }
        }
    }
}

QModelIndexList MsgListView::selectedTree() const
{
    QModelIndexList indexes;
    QModelIndexList selected = selectedIndexes();
    const int originalItems = selected.length(); // only check collapsed/expanded status on original selection
    for (int i = 0; i < selected.length(); ++i) {
        const QModelIndex item = selected[i];
        if (item.column() != 0 || !item.data(Imap::Mailbox::RoleMessageUid).isValid())
            continue;
        indexes << item;
        // Now see if this is a collapsed thread and include all the collapsed items as needed
        // Also note that this is recursive - each child found is run through this same item loop for validity/child checks as well
        if (i >= originalItems || !isExpanded(item)) {
            for (int j = 0; j < item.model()->rowCount(item); ++j) {
                selected << item.model()->index(j, 0, item); // Make sure this is run through the main loop as well - don't add it directly
            }
        }
    }
    return indexes;
}

void MsgListView::slotFixSize()
{
    if (!m_autoResizeSections)
        return;

    if (header()->visualIndex(Imap::Mailbox::MsgListModel::SUBJECT) == -1) {
        // calling setResizeMode() would assert()
        return;
    }

    header()->setStretchLastSection(false);
    for (int i = 0; i < Imap::Mailbox::MsgListModel::COLUMN_COUNT; ++i) {
        QHeaderView::ResizeMode resizeMode = resizeModeForColumn(i);
        header()->setSectionResizeMode(i, resizeMode);
        setColumnWidth(i, sizeHintForColumn(i));
    }
}

QHeaderView::ResizeMode MsgListView::resizeModeForColumn(const int column) const
{
    switch (column) {
    case Imap::Mailbox::MsgListModel::SUBJECT:
        return QHeaderView::Stretch;
    case Imap::Mailbox::MsgListModel::SEEN:
    case Imap::Mailbox::MsgListModel::FLAGGED:
    case Imap::Mailbox::MsgListModel::ATTACHMENT:
        return QHeaderView::Fixed;
    default:
        return QHeaderView::Interactive;
    }
}

void MsgListView::slotExpandWholeSubtree(const QModelIndex &rootIndex)
{
    if (rootIndex.parent().isValid())
        return;

    QVector<QModelIndex> queue(1, rootIndex);
    for (int i = 0; i < queue.size(); ++i) {
        const QModelIndex currentIndex = queue[i];
        // Append all children to the queue...
        for (int j = 0; j < currentIndex.model()->rowCount(currentIndex); ++j)
            queue.append(currentIndex.model()->index(j, 0, currentIndex));
        // ...and expand the current index
        if (currentIndex.model()->hasChildren(currentIndex))
            expand(currentIndex);
    }
}

void MsgListView::slotUpdateHeaderActions()
{
    Q_ASSERT(header());
    // At first, remove all actions
    QList<QAction *> actions = header()->actions();
    Q_FOREACH(QAction *action, actions) {
        header()->removeAction(action);
        headerFieldsMapper->removeMappings(action);
        action->deleteLater();
    }
    actions.clear();
    // Now add them again
    for (int i = 0; i < header()->count(); ++i) {
        QString message = header()->model() ? header()->model()->headerData(i, Qt::Horizontal).toString() : QString::number(i);
        QAction *action = new QAction(message, this);
        action->setCheckable(true);
        action->setChecked(true);
        connect(action, &QAction::toggled, headerFieldsMapper, static_cast<void (QSignalMapper::*)()>(&QSignalMapper::map));
        headerFieldsMapper->setMapping(action, i);
        header()->addAction(action);

        // Next, add some special handling of certain columns
        switch (i) {
        case Imap::Mailbox::MsgListModel::SEEN:
            // This column doesn't have a textual description
            action->setText(tr("Seen status"));
            break;
        case Imap::Mailbox::MsgListModel::FLAGGED:
            action->setText(tr("Flagged status"));
            break;
        case Imap::Mailbox::MsgListModel::ATTACHMENT:
            action->setText(tr("Attachment"));
            break;
        case Imap::Mailbox::MsgListModel::TO:
        case Imap::Mailbox::MsgListModel::CC:
        case Imap::Mailbox::MsgListModel::BCC:
        case Imap::Mailbox::MsgListModel::RECEIVED_DATE:
            // And these should be hidden by default
            action->toggle();
            break;
        default:
            break;
        }
    }

    // Make sure to kick the header again so that it shows reasonable sizing
    slotFixSize();
}

/** @short Handle columns added to MsgListModel and set their default properties
 *
 * When a new version of the underlying model got a new column, the old saved state of the GUI might only contain data for the old columns.
 * Therefore it is important to explicitly restore the default for new columns, if any.
 */
void MsgListView::slotHandleNewColumns(int oldCount, int newCount)
{
    for (int i = oldCount; i < newCount; ++i) {
        switch(i) {
        case Imap::Mailbox::MsgListModel::FLAGGED:
            header()->moveSection(i,0);
            break;
        case Imap::Mailbox::MsgListModel::ATTACHMENT:
            header()->moveSection(i,0);
            break;
        }

        setColumnWidth(i, sizeHintForColumn(i));
    }
}

void MsgListView::slotHeaderSectionVisibilityToggled(int section)
{
    QList<QAction *> actions = header()->actions();
    if (section >= actions.size() || section < 0)
        return;
    bool hide = ! actions[section]->isChecked();

    if (hide && header()->hiddenSectionCount() == header()->count() - 1) {
        // This would hide the very last section, which would hide the whole header view
        actions[section]->setChecked(true);
    } else {
        header()->setSectionHidden(section, hide);
    }
}

void MsgListView::updateActionsAfterRestoredState()
{
    m_autoResizeSections = false;
    QList<QAction *> actions = header()->actions();
    for (int i = 0; i < actions.size(); ++i) {
        actions[i]->setChecked(!header()->isSectionHidden(i));
    }
}

/** @short Overridden from QTreeView::setModel

The whole point is that we have to listen for sortingPreferenceChanged to update your header view when sorting is requested
but cannot be fulfilled.
*/
void MsgListView::setModel(QAbstractItemModel *model)
{
    if (this->model()) {
        if (Imap::Mailbox::PrettyMsgListModel *prettyModel = findPrettyMsgListModel(this->model())) {
            disconnect(prettyModel, &Imap::Mailbox::PrettyMsgListModel::sortingPreferenceChanged,
                       this, &MsgListView::slotHandleSortCriteriaChanged);
            disconnect(qobject_cast<Imap::Mailbox::ThreadingMsgListModel*>(prettyModel->sourceModel())->sourceModel(),
                    &QAbstractItemModel::rowsAboutToBeRemoved,
                    this, &MsgListView::slotMsgListModelRowsAboutToBeRemoved);
        }
    }
    for (const QMetaObject::Connection &connection : std::as_const(m_scrollAnchorConnections))
        disconnect(connection);
    m_scrollAnchorConnections.clear();
    m_scrollAnchor.reset();
    m_scrollAnchorDepth = 0;
    QTreeView::setModel(model);
    if (model) {
        // After the view's own handlers, so that the rows have their new places already
        m_scrollAnchorConnections
            << connect(model, &QAbstractItemModel::layoutAboutToBeChanged, this, [this]() { captureScrollAnchor(false); })
            << connect(model, &QAbstractItemModel::layoutChanged, this, &MsgListView::restoreScrollAnchor)
            << connect(model, &QAbstractItemModel::rowsAboutToBeInserted, this, [this]() { captureScrollAnchor(true); })
            << connect(model, &QAbstractItemModel::rowsInserted, this, &MsgListView::restoreScrollAnchor)
            << connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this, [this]() { captureScrollAnchor(false); })
            << connect(model, &QAbstractItemModel::rowsRemoved, this, &MsgListView::restoreScrollAnchor)
            << connect(model, &QAbstractItemModel::modelReset, this, [this]() {
                   m_scrollAnchor.reset();
                   m_scrollAnchorDepth = 0;
               });
    }
    if (Imap::Mailbox::PrettyMsgListModel *prettyModel = findPrettyMsgListModel(model)) {
        connect(prettyModel, &Imap::Mailbox::PrettyMsgListModel::sortingPreferenceChanged,
                this, &MsgListView::slotHandleSortCriteriaChanged);
        connect(qobject_cast<Imap::Mailbox::ThreadingMsgListModel*>(prettyModel->sourceModel())->sourceModel(),
                &QAbstractItemModel::rowsAboutToBeRemoved,
                this, &MsgListView::slotMsgListModelRowsAboutToBeRemoved);
    }
}

/** @short Get the PrettyMsgListModel indexes of messages which are about to go away

The ThreadingMsgListModel only marks such messages and prunes them later, so the rows of the view stay in place
for a while. Messages which are on their way out have no UID any more.
*/
void MsgListView::slotMsgListModelRowsAboutToBeRemoved(const QModelIndex &parent, int start, int end)
{
    Q_ASSERT(!parent.isValid());

    Imap::Mailbox::PrettyMsgListModel *prettyModel = findPrettyMsgListModel(model());
    auto threadingModel = qobject_cast<Imap::Mailbox::ThreadingMsgListModel*>(prettyModel->sourceModel());
    QSet<QModelIndex> removed;
    for (int i = start; i <= end; ++i) {
        QModelIndex index = threadingModel->sourceModel()->index(i, 0, parent);
        Q_ASSERT(index.isValid());
        QModelIndex translated = threadingModel->mapFromSource(index);

        if (translated.isValid())
            translated = prettyModel->mapFromSource(translated);
        if (translated.isValid())
            translated = mapFromPrettyModel(translated);
        if (translated.isValid())
            removed.insert(translated);
    }
    if (removed.isEmpty())
        return;
    prepareForRemoval([&removed](const QModelIndex &index) {
        return removed.contains(index.siblingAtColumn(0));
    });
}

QModelIndex MsgListView::mapFromPrettyModel(const QModelIndex &prettyIndex) const
{
    QList<QAbstractProxyModel *> proxies;
    QAbstractItemModel *current = model();
    while (current && current != prettyIndex.model()) {
        auto proxy = qobject_cast<QAbstractProxyModel *>(current);
        if (!proxy)
            return {};
        proxies.prepend(proxy);
        current = proxy->sourceModel();
    }
    if (!current)
        return {};
    QModelIndex index = prettyIndex;
    for (QAbstractProxyModel *proxy : std::as_const(proxies))
        index = proxy->mapFromSource(index);
    return index;
}

/** @short Step over the headers of expanded groups, which are not messages

A collapsed group's header stays reachable, so that Right can expand it. Left on a message moves to its header.
*/
QModelIndex MsgListView::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
{
    QModelIndex next = QTreeView::moveCursor(cursorAction, modifiers);
    const auto isExpandedHeader = [this](const QModelIndex &index) {
        return index.isValid() && index.data(Imap::Mailbox::RoleIsGroupHeader).toBool()
            && isExpanded(index.siblingAtColumn(0));
    };
    if (!isExpandedHeader(next))
        return next;
    bool downwards;
    switch (cursorAction) {
    case MoveDown:
    case MovePageDown:
    case MoveHome:
        downwards = true;
        break;
    case MoveUp:
    case MovePageUp:
    case MoveEnd:
        downwards = false;
        break;
    default:
        return next;
    }
    while (isExpandedHeader(next))
        next = downwards ? indexBelow(next) : indexAbove(next);
    if (!next.isValid())
        return currentIndex();
    return next;
}

void MsgListView::rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end)
{
    // Messages of an IMAP mailbox are followed through the MsgListModel instead, see setModel()
    if (!findPrettyMsgListModel(model())) {
        prepareForRemoval([&parent, start, end](const QModelIndex &index) {
            return UiUtils::SelectionAfterRemoval::isWithinRemovedRows(index, parent, start, end);
        });
    }
    // Last, because QTreeView forgets its rows here, and indexBelow() needs them
    QTreeView::rowsAboutToBeRemoved(parent, start, end);
}

/** @short Keep the cursor in the list for better keyboard usability

When the current message goes away, the next remaining message becomes current, or the previous one at the end of
the list. Other selected messages are skipped while possible, because they are usually on their way out as well:
moving a selection sends one command, but the server may report the removed messages one at a time.
*/
void MsgListView::prepareForRemoval(const std::function<bool(const QModelIndex &)> &isRemoved)
{
    QItemSelectionModel *selection = selectionModel();
    if (!selection)
        return;
    const QModelIndex current = currentIndex().isValid() ? currentIndex().siblingAtColumn(0) : QModelIndex();
    const QModelIndexList selected = selection->selectedRows(0);
    const bool currentRemoved = current.isValid() && isRemoved(current);
    const bool selectionTouched = std::any_of(selected.cbegin(), selected.cend(), isRemoved);
    if (!currentRemoved && !selectionTouched)
        return;

    if (currentRemoved) {
        QSet<QModelIndex> pending;
        if (selected.contains(current))
            pending = QSet<QModelIndex>(selected.cbegin(), selected.cend());
        const QModelIndex next = UiUtils::SelectionAfterRemoval::successor(
            current,
            [this](const QModelIndex &index) { return indexBelow(index); },
            [this](const QModelIndex &index) { return indexAbove(index); },
            [&isRemoved](const QModelIndex &index) {
                return !isRemoved(index) && index.data(Imap::Mailbox::RoleMessageUid).isValid();
            },
            [&pending](const QModelIndex &index) { return pending.contains(index.siblingAtColumn(0)); });
        if (next.isValid()) {
            // Not activated yet: the models are still busy removing rows
            selection->setCurrentIndex(next, QItemSelectionModel::NoUpdate);
            m_currentReplacedByRemoval = true;
        }
    }
    m_selectionTouchedByRemoval = m_selectionTouchedByRemoval || selectionTouched;
    m_focusedBeforeRemoval = m_focusedBeforeRemoval || hasFocus();
    if (!m_removalFinishQueued) {
        m_removalFinishQueued = true;
        QMetaObject::invokeMethod(this, &MsgListView::finishRemoval, Qt::QueuedConnection);
    }
}

void MsgListView::finishRemoval()
{
    m_removalFinishQueued = false;
    const bool replaced = std::exchange(m_currentReplacedByRemoval, false);
    const bool touched = std::exchange(m_selectionTouchedByRemoval, false);
    const bool focused = std::exchange(m_focusedBeforeRemoval, false);
    QItemSelectionModel *selection = selectionModel();
    const QModelIndex current = currentIndex();
    // Nothing is selected in an empty list
    if (!selection || !current.isValid() || !current.data(Imap::Mailbox::RoleMessageUid).isValid())
        return;
    if (!replaced) {
        if (!touched)
            return;
        // The cursor was elsewhere; only an emptied selection falls back to the current message
        const QModelIndexList selected = selection->selectedRows(0);
        const bool selectionRemains = std::any_of(selected.cbegin(), selected.cend(), [](const QModelIndex &index) {
            return index.data(Imap::Mailbox::RoleMessageUid).isValid();
        });
        if (selectionRemains)
            return;
    }
    selection->select(current, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    scrollTo(current);
    if (focused && !hasFocus())
        setFocus(Qt::OtherFocusReason);
    emit currentReplacedAfterRemoval(current);
}

/** @short Remember the place of the row which shall stay put while the rows change

Without this, the view keeps its scroll offset, so new messages above the current one push it down and sorting again
moves it anywhere. The current message is the anchor while it is visible, otherwise the top row is; a list which
shows its top keeps showing it.
*/
void MsgListView::captureScrollAnchor(bool insertion)
{
    if (m_scrollAnchorDepth++ > 0)
        return;
    m_scrollAnchor.reset();
    if (!isVisible())
        return;
    const QRect area = viewport()->rect();
    const QScrollBar *bar = verticalScrollBar();
    const bool atTop = bar->value() == bar->minimum();
    QModelIndex anchor = currentIndex();
    QRect rect = anchor.isValid() ? visualRect(anchor) : QRect();
    if (!rect.isValid() || !rect.intersects(area)) {
        if (atTop)
            return;
        anchor = indexAt(QPoint(area.left() + 1, area.top() + 1));
        rect = anchor.isValid() ? visualRect(anchor) : QRect();
        if (!rect.isValid())
            return;
    }
    m_scrollAnchor = ScrollAnchor{QPersistentModelIndex(anchor), rect.top(), insertion && atTop};
}

void MsgListView::restoreScrollAnchor()
{
    if (m_scrollAnchorDepth == 0 || --m_scrollAnchorDepth > 0)
        return;
    const std::optional<ScrollAnchor> anchor = std::exchange(m_scrollAnchor, std::nullopt);
    if (!anchor || !anchor->index.isValid())
        return;
    executeDelayedItemsLayout();
    updateGeometries();
    QScrollBar *bar = verticalScrollBar();
    QRect rect = visualRect(anchor->index);
    // Collapsed into a thread or a group, for example
    if (!rect.isValid())
        return;
    if (anchor->keepTop) {
        bar->setValue(bar->minimum());
        rect = visualRect(anchor->index);
        if (viewport()->rect().contains(rect))
            return;
    }
    const int delta = rect.top() - anchor->y;
    if (!delta)
        return;
    if (verticalScrollMode() == ScrollPerPixel) {
        bar->setValue(bar->value() + delta);
    } else if (rect.height() > 0) {
        bar->setValue(bar->value() + qRound(double(delta) / rect.height()));
    }
}

void MsgListView::slotHandleSortCriteriaChanged(int column, Qt::SortOrder order)
{
    // The if-clause is needed to prevent infinite recursion
    if (header()->sortIndicatorSection() != column || header()->sortIndicatorOrder() != order) {
        header()->setSortIndicator(column, order);
    }
}

/** @short Walk the hierarchy of proxy models up until we stop at the PrettyMsgListModel or the first non-proxy model */
Imap::Mailbox::PrettyMsgListModel *MsgListView::findPrettyMsgListModel(QAbstractItemModel *model)
{
    while (QAbstractProxyModel *proxy = qobject_cast<QAbstractProxyModel*>(model)) {
        Imap::Mailbox::PrettyMsgListModel *prettyModel = qobject_cast<Imap::Mailbox::PrettyMsgListModel*>(proxy);
        if (prettyModel)
            return prettyModel;
        else
            model = proxy->sourceModel();
    }
    return nullptr;
}

void MsgListView::setAutoActivateAfterKeyNavigation(bool enabled)
{
    m_autoActivateAfterKeyNavigation = enabled;
}

}


