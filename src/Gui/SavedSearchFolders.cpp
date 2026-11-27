/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "SavedSearchFolders.h"

#include "UiUtils/IconLoader.h"

#include <QAction>
#include <QDropEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QIcon>
#include <QMenu>
#include <QScrollBar>
#include <QSet>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <algorithm>
#include <functional>
#include <utility>

#include "MessageListWidget.h"

namespace {

using UiUtils::SavedSearch;
using UiUtils::SearchScope::Places;
namespace SavedSearches = UiUtils::SavedSearches;

const int ownerRole = Qt::UserRole + 1;
const int idRole = Qt::UserRole + 2;
/** Rows shown before the list scrolls */
const int maximumRows = 8;

QIcon themedIcon(const QString &name, const QIcon &fallback = QIcon())
{
    return UiUtils::SymbolicIcon::fromTheme(name, fallback);
}

const char viewKeyProperty[] = "savedSearchViewKey";

/** @short Reports a drop which moved rows, and triggers the actions whose keys it takes over while focused

The keys are not shortcuts of the actions, as the same keys move through conversations and delete messages elsewhere
in the window, and Qt would find those ambiguous.
*/
class SavedSearchView : public QTreeWidget
{
public:
    using QTreeWidget::QTreeWidget;
    std::function<void()> moved;
    QList<QAction *> keyed;

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::ShortcutOverride && actionFor(static_cast<QKeyEvent *>(event))) {
            event->accept();
            return true;
        }
        return QTreeWidget::event(event);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (QAction *action = actionFor(event)) {
            action->trigger();
            event->accept();
            return;
        }
        QTreeWidget::keyPressEvent(event);
    }

    void dropEvent(QDropEvent *event) override
    {
        QTreeWidget::dropEvent(event);
        if (event->isAccepted() && moved)
            moved();
    }

private:
    QAction *actionFor(const QKeyEvent *event) const
    {
        if (!currentItem())
            return nullptr;
        const QKeySequence pressed(event->keyCombination());
        for (QAction *action : keyed) {
            if (action->isEnabled() && action->property(viewKeyProperty).value<QKeySequence>() == pressed)
                return action;
        }
        return nullptr;
    }
};

KConfigGroup panelState()
{
    return KConfigGroup(KSharedConfig::openStateConfig(), QStringLiteral("SavedSearchFolders"));
}

/** @short Where @p search looks, for its tooltip; @p owner is the account's name, or empty with a single account */
QString scopeText(const SavedSearch &search, const QString &owner)
{
    switch (search.places) {
    case Places::CurrentFolder:
        return owner.isEmpty() ? i18nc("@info:tooltip where a saved search looks", "Searches the shown folder")
                               : i18nc("@info:tooltip where a saved search looks, %1 is an account",
                                       "Searches the shown folder of %1", owner);
    case Places::Account:
        return owner.isEmpty() ? i18nc("@info:tooltip where a saved search looks", "Searches all folders of the account")
                               : i18nc("@info:tooltip where a saved search looks, %1 is an account",
                                       "Searches all folders of %1", owner);
    case Places::SelectedFolders:
        return i18ncp("@info:tooltip where a saved search looks", "Searches %1 chosen folder",
                      "Searches %1 chosen folders", search.folders.size());
    case Places::AllAccounts:
        return i18nc("@info:tooltip where a saved search looks", "Searches all accounts");
    }
    return QString();
}

}

namespace Gui {

SavedSearchFolders::SavedSearchFolders(QWidget *parent)
    : QWidget(parent)
    , m_header(new QToolButton(this))
    , m_view(new SavedSearchView(this))
    , m_menu(new QMenu(this))
{
    setObjectName(QStringLiteral("savedSearchPanel"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_header->setObjectName(QStringLiteral("savedSearchHeader"));
    m_header->setText(i18nc("@title sidebar section of saved searches", "Saved Searches"));
    m_header->setAutoRaise(true);
    m_header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_header->setCheckable(true);
    m_header->setChecked(panelState().readEntry("Expanded", true));
    connect(m_header, &QToolButton::toggled, this, [this](bool expanded) {
        KConfigGroup state = panelState();
        state.writeEntry("Expanded", expanded);
        state.sync();
        m_view->setVisible(expanded);
        updateHeader();
    });
    layout->addWidget(m_header);

    m_view->setObjectName(QStringLiteral("savedSearchFolders"));
    m_view->setColumnCount(2);
    m_view->setHeaderHidden(true);
    m_view->setRootIsDecorated(false);
    m_view->setUniformRowHeights(true);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_view->header()->setStretchLastSection(false);
    m_view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_view->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_view->setAccessibleName(i18nc("@info:accessibility", "Saved searches"));
    m_view->setVisible(m_header->isChecked());
    connect(m_view, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) { requestActivation(item); });
    connect(m_view, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) { requestActivation(item); });
    // Rows are moved between rows, never into one another
    m_view->setDragDropMode(QAbstractItemView::InternalMove);
    m_view->setDefaultDropAction(Qt::MoveAction);
    m_view->setDragDropOverwriteMode(false);
    m_view->invisibleRootItem()->setFlags(Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
    static_cast<SavedSearchView *>(m_view)->moved = [this]() {
        // After the drag is over, as the rows are filled again
        QTimer::singleShot(0, this, &SavedSearchFolders::takeViewOrder);
    };
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QWidget::customContextMenuRequested, this, &SavedSearchFolders::showContextMenu);
    layout->addWidget(m_view);

    m_menu->setObjectName(QStringLiteral("savedSearchMenu"));
    const auto addAction = [this](const QString &name, const QString &icon, const QString &text,
                                  const QKeySequence &shortcut, void (SavedSearchFolders::*signal)(const QString &, const QString &)) {
        auto *action = new QAction(themedIcon(icon), text, this);
        action->setObjectName(name);
        if (!shortcut.isEmpty()) {
            // Menus show what follows a tab as the key
            action->setText(text + QLatin1Char('\t') + shortcut.toString(QKeySequence::NativeText));
            action->setProperty(viewKeyProperty, shortcut);
            static_cast<SavedSearchView *>(m_view)->keyed.append(action);
        }
        if (!signal)
            return action;
        connect(action, &QAction::triggered, this, [this, signal]() {
            QTreeWidgetItem *item = std::exchange(m_menuItem, nullptr);
            if (!item)
                item = m_view->currentItem();
            if (item)
                emit(this->*signal)(item->data(0, ownerRole).toString(), item->data(0, idRole).toString());
        });
        return action;
    };
    m_run = addAction(QStringLiteral("savedSearchRun"), QStringLiteral("system-search"),
                      i18nc("@action:inmenu saved search", "Run Search"), {}, &SavedSearchFolders::searchActivated);
    m_edit = addAction(QStringLiteral("savedSearchEdit"), QStringLiteral("document-edit"),
                       i18nc("@action:inmenu saved search", "Edit Search"), {}, &SavedSearchFolders::editRequested);
    m_makeRule = addAction(QStringLiteral("savedSearchMakeRule"), QStringLiteral("view-filter"),
                           i18nc("@action:inmenu saved search", "Create Rule..."), {}, &SavedSearchFolders::ruleRequested);
    m_rename = addAction(QStringLiteral("savedSearchRename"), QStringLiteral("edit-rename"),
                         i18nc("@action:inmenu saved search", "Rename..."), QKeySequence(Qt::Key_F2),
                         &SavedSearchFolders::renameRequested);
    m_moveUp = addAction(QStringLiteral("savedSearchMoveUp"), QStringLiteral("go-up"),
                         i18nc("@action:inmenu saved search", "Move Up"), QKeySequence(Qt::ALT | Qt::Key_Up), nullptr);
    m_moveDown = addAction(QStringLiteral("savedSearchMoveDown"), QStringLiteral("go-down"),
                           i18nc("@action:inmenu saved search", "Move Down"), QKeySequence(Qt::ALT | Qt::Key_Down), nullptr);
    connect(m_moveUp, &QAction::triggered, this, [this]() { moveTarget(-1); });
    connect(m_moveDown, &QAction::triggered, this, [this]() { moveTarget(1); });
    m_moveToAccount = new QMenu(i18nc("@title:menu saved search", "Move to Account"), this);
    m_moveToAccount->setObjectName(QStringLiteral("savedSearchMoveToAccount"));
    m_moveToAccount->setIcon(themedIcon(QStringLiteral("user-identity")));
    m_chooseFolders = addAction(QStringLiteral("savedSearchChooseFolders"), QStringLiteral("folder-open"),
                                i18nc("@action:inmenu saved search", "Choose Folders..."), {},
                                &SavedSearchFolders::foldersChoiceRequested);
    m_removeMissing = addAction(QStringLiteral("savedSearchRemoveMissing"), QStringLiteral("edit-clear"),
                                i18nc("@action:inmenu saved search", "Leave Out Missing Folders"), {},
                                &SavedSearchFolders::missingFoldersRemovalRequested);
    m_delete = addAction(QStringLiteral("savedSearchDelete"), QStringLiteral("edit-delete"),
                         i18nc("@action:inmenu saved search", "Delete..."), QKeySequence(Qt::Key_Delete),
                         &SavedSearchFolders::deleteRequested);
    m_menu->addAction(m_run);
    m_menu->addAction(m_edit);
    m_menu->addAction(m_makeRule);
    m_menu->addAction(m_rename);
    m_menu->addSeparator();
    m_menu->addAction(m_moveUp);
    m_menu->addAction(m_moveDown);
    m_menu->addMenu(m_moveToAccount);
    m_menu->addSeparator();
    m_menu->addAction(m_chooseFolders);
    m_menu->addAction(m_removeMissing);
    m_menu->addSeparator();
    m_menu->addAction(m_delete);
    // The keys act on the current row; an open context menu keeps acting on its own row
    connect(m_view, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *current) {
        if (!m_menuItem)
            configureActions(current);
    });

    updateHeader();
    setVisible(false);
}

void SavedSearchFolders::setSearches(const QList<SavedSearch> &searches, const QStringList &owners,
                                     const QHash<QString, QString> &ownerNames)
{
    m_searches = searches;
    m_owners = owners;
    m_ownerNames = ownerNames;
    QSet<QString> known;
    for (const SavedSearch &search : searches)
        known.insert(SavedSearches::countsKey(search.owner, search.id));
    for (auto it = m_counts.begin(); it != m_counts.end();) {
        if (known.contains(it.key()))
            ++it;
        else
            it = m_counts.erase(it);
    }
    for (auto it = m_problems.begin(); it != m_problems.end();) {
        if (known.contains(it.key()))
            ++it;
        else
            it = m_problems.erase(it);
    }
    fillView();
    setVisible(!searches.isEmpty());
}

void SavedSearchFolders::fillView()
{
    m_pendingActivation = nullptr;
    m_menuItem = nullptr;
    const QSignalBlocker blocker(m_view);
    m_view->clear();
    for (const SavedSearch &search : std::as_const(m_searches)) {
        auto *item = new QTreeWidgetItem(m_view);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        item->setData(0, ownerRole, search.owner);
        item->setData(0, idRole, search.id);
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        updateItem(item);
    }
    setActive(m_activeOwner, m_activeId);
    configureActions(m_view->currentItem());
    fitHeight();
}

void SavedSearchFolders::setCounts(const QString &owner, const QString &id, const UiUtils::SavedSearches::Counts &counts)
{
    QTreeWidgetItem *item = itemFor(owner, id);
    if (!item)
        return;
    m_counts.insert(UiUtils::SavedSearches::countsKey(owner, id), counts);
    updateItem(item);
}

UiUtils::SavedSearches::Counts SavedSearchFolders::counts(const QString &owner, const QString &id) const
{
    return m_counts.value(SavedSearches::countsKey(owner, id));
}

void SavedSearchFolders::setProblems(const QString &owner, const QString &id, const SavedSearches::Problems &problems)
{
    QTreeWidgetItem *item = itemFor(owner, id);
    if (!item)
        return;
    const QString key = SavedSearches::countsKey(owner, id);
    if (m_problems.value(key) == problems)
        return;
    if (problems.isEmpty())
        m_problems.remove(key);
    else
        m_problems.insert(key, problems);
    updateItem(item);
}

UiUtils::SavedSearches::Problems SavedSearchFolders::problems(const QString &owner, const QString &id) const
{
    return m_problems.value(SavedSearches::countsKey(owner, id));
}

void SavedSearchFolders::setActive(const QString &owner, const QString &id)
{
    m_activeOwner = owner;
    m_activeId = id;
    QTreeWidgetItem *item = itemFor(owner, id);
    if (item) {
        m_view->setCurrentItem(item);
    } else {
        m_view->clearSelection();
        m_view->setCurrentItem(nullptr);
    }
}

bool SavedSearchFolders::isExpanded() const
{
    return m_header->isChecked();
}

void SavedSearchFolders::setExpanded(bool expanded)
{
    m_header->setChecked(expanded);
}

QTreeWidgetItem *SavedSearchFolders::itemFor(const QString &owner, const QString &id) const
{
    if (owner.isEmpty() || id.isEmpty())
        return nullptr;
    for (int row = 0, rows = m_view->topLevelItemCount(); row < rows; ++row) {
        QTreeWidgetItem *item = m_view->topLevelItem(row);
        if (item->data(0, ownerRole).toString() == owner && item->data(0, idRole).toString() == id)
            return item;
    }
    return nullptr;
}

const SavedSearch *SavedSearchFolders::searchOf(const QTreeWidgetItem *item) const
{
    if (!item)
        return nullptr;
    const QString owner = item->data(0, ownerRole).toString();
    const QString id = item->data(0, idRole).toString();
    const auto search = std::find_if(m_searches.cbegin(), m_searches.cend(),
                                     [&](const SavedSearch &s) { return s.owner == owner && s.id == id; });
    return search == m_searches.cend() ? nullptr : &*search;
}

void SavedSearchFolders::updateItem(QTreeWidgetItem *item)
{
    const SavedSearch *search = searchOf(item);
    if (!search)
        return;
    const QString key = SavedSearches::countsKey(search->owner, search->id);
    const SavedSearches::Counts counts = m_counts.value(key);
    const SavedSearches::Problems problems = m_problems.value(key);
    const bool removedOwner = !m_owners.contains(search->owner);
    QString ownerName;
    if (removedOwner)
        ownerName = i18nc("@info:tooltip an account which was removed", "a removed account");
    else if (m_owners.size() > 1)
        ownerName = m_ownerNames.value(search->owner, search->owner);
    // A search of its own account's folders cannot run once the account is gone
    const bool runnable = !(problems.missingOwner && search->dependsOnOwner());

    item->setText(0, search->name);
    item->setIcon(0, problems.isEmpty() ? themedIcon(QStringLiteral("folder-saved-search"), themedIcon(QStringLiteral("edit-find")))
                                        : themedIcon(QStringLiteral("dialog-warning")));
    QFont font = m_view->font();
    font.setBold(runnable && counts.isKnown() && counts.unread > 0);
    item->setFont(0, font);
    item->setFont(1, font);
    const QVariant text = runnable ? QVariant() : QVariant(m_view->palette().brush(QPalette::PlaceholderText));
    item->setData(0, Qt::ForegroundRole, text);
    item->setData(1, Qt::ForegroundRole, text);

    QStringList tip;
    tip << search->name;
    if (!search->query.isEmpty())
        tip << i18nc("@info:tooltip the text of a saved search", "Query: %1", search->query);
    tip << scopeText(*search, ownerName);
    if (UiUtils::SearchScope::isLimited(search->dates))
        tip << i18nc("@info:tooltip %1 is a date range such as Past Week", "Dates: %1", searchDatesText(search->dates));
    QString accessible = search->name;
    if (counts.isKnown()) {
        item->setText(1, i18nc("@item unread and all messages found by a saved search", "%1 / %2", counts.unread,
                               counts.total));
        tip << i18ncp("@info:tooltip", "Found %1 message when it last ran", "Found %1 messages when it last ran",
                      counts.total)
            + QLatin1String(", ") + i18ncp("@info:tooltip", "%1 unread", "%1 unread", counts.unread);
        accessible = i18nc("@info:accessibility saved search, unread and all messages it found", "%1, %2 unread of %3",
                           search->name, counts.unread, counts.total);
    } else {
        item->setText(1, QString());
        tip << i18nc("@info:tooltip", "Not run yet");
    }
    if (problems.missingOwner) {
        tip << (runnable ? i18nc("@info:tooltip", "Its account was removed. It still runs; move it to another account "
                                                  "to keep it with that account.")
                         : i18nc("@info:tooltip", "Its account was removed, so it cannot run. Move it to another account "
                                                  "or delete it."));
    }
    if (!problems.missingFolders.isEmpty()) {
        QStringList names;
        for (const QString &folderKey : problems.missingFolders) {
            const UiUtils::SearchScope::Folder folder = UiUtils::SearchScope::folderFromKey(folderKey);
            const QString account = m_ownerNames.value(folder.accountId);
            names << (account.isEmpty() ? folder.mailbox
                                        : i18nc("@item folder and its account", "%1 (%2)", folder.mailbox, account));
        }
        tip << i18ncp("@info:tooltip %2 lists folders", "This chosen folder is gone: %2",
                      "These chosen folders are gone: %2", names.size(),
                      names.join(i18nc("@info:tooltip separator of folder names", ", ")))
            << i18nc("@info:tooltip", "Choose other folders or leave out the missing ones from its context menu.");
    }
    if (!problems.isEmpty())
        accessible = i18nc("@info:accessibility a saved search which needs attention", "%1, needs attention", accessible);
    item->setData(0, Qt::AccessibleTextRole, accessible);
    const QString toolTip = tip.join(QLatin1Char('\n'));
    item->setToolTip(0, toolTip);
    item->setToolTip(1, toolTip);
}

void SavedSearchFolders::updateHeader()
{
    const bool open = m_header->isChecked();
    m_header->setIcon(open ? themedIcon(QStringLiteral("go-down")) : themedIcon(QStringLiteral("go-next")));
    m_header->setAccessibleDescription(open ? i18nc("@info:accessibility saved searches", "Saved searches shown")
                                            : i18nc("@info:accessibility saved searches", "Saved searches hidden"));
}

void SavedSearchFolders::fitHeight()
{
    const int rows = m_view->topLevelItemCount();
    const int rowHeight = rows > 0 ? m_view->sizeHintForRow(0) : m_view->fontMetrics().height();
    const int shown = std::clamp(rows, 1, maximumRows);
    m_view->setFixedHeight(shown * rowHeight + 2 * m_view->frameWidth());
    m_view->setVerticalScrollBarPolicy(rows > maximumRows ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
}

void SavedSearchFolders::requestActivation(QTreeWidgetItem *item)
{
    if (!item || m_pendingActivation)
        return;
    m_pendingActivation = item;
    QTimer::singleShot(0, this, [this]() {
        QTreeWidgetItem *item = std::exchange(m_pendingActivation, nullptr);
        if (!item || m_view->indexOfTopLevelItem(item) < 0)
            return;
        emit searchActivated(item->data(0, ownerRole).toString(), item->data(0, idRole).toString());
    });
}

QTreeWidgetItem *SavedSearchFolders::targetItem() const
{
    return m_menuItem ? m_menuItem : m_view->currentItem();
}

QMenu *SavedSearchFolders::menuFor(QTreeWidgetItem *item)
{
    m_menuItem = item;
    configureActions(item);
    return m_menu;
}

void SavedSearchFolders::configureActions(QTreeWidgetItem *item)
{
    const SavedSearch *search = searchOf(item);
    const SavedSearches::Problems problems = search ? m_problems.value(SavedSearches::countsKey(search->owner, search->id))
                                                    : SavedSearches::Problems();
    const bool runnable = search && !(problems.missingOwner && search->dependsOnOwner());
    const int row = item ? m_view->indexOfTopLevelItem(item) : -1;
    m_run->setEnabled(runnable);
    m_edit->setEnabled(runnable);
    m_makeRule->setEnabled(search != nullptr);
    m_rename->setEnabled(search != nullptr);
    m_delete->setEnabled(search != nullptr);
    m_moveUp->setEnabled(search && row > 0);
    m_moveDown->setEnabled(search && row >= 0 && row < m_view->topLevelItemCount() - 1);

    m_moveToAccount->clear();
    if (search) {
        for (const QString &owner : std::as_const(m_owners)) {
            if (owner == search->owner)
                continue;
            QString name = m_ownerNames.value(owner, owner);
            name.replace(QLatin1Char('&'), QStringLiteral("&&"));
            QAction *move = m_moveToAccount->addAction(name);
            move->setData(owner);
            const QString from = search->owner;
            const QString id = search->id;
            connect(move, &QAction::triggered, this, [this, from, id, owner]() {
                m_menuItem = nullptr;
                emit ownerChangeRequested(from, id, owner);
            });
        }
    }
    m_moveToAccount->menuAction()->setVisible(!m_moveToAccount->isEmpty());

    const bool chosenFolders = search && search->places == Places::SelectedFolders;
    m_chooseFolders->setVisible(chosenFolders);
    m_chooseFolders->setEnabled(chosenFolders);
    // Leaving out every folder would leave nothing to search
    const bool someMissing = chosenFolders && !problems.missingFolders.isEmpty()
        && problems.missingFolders.size() < search->folders.size();
    m_removeMissing->setVisible(someMissing);
    m_removeMissing->setEnabled(someMissing);
}

void SavedSearchFolders::showContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_view->itemAt(pos);
    if (!item)
        return;
    menuFor(item)->exec(m_view->viewport()->mapToGlobal(pos));
    m_menuItem = nullptr;
    configureActions(m_view->currentItem());
}

void SavedSearchFolders::moveTarget(int offset)
{
    QTreeWidgetItem *item = std::exchange(m_menuItem, nullptr);
    if (!item)
        item = m_view->currentItem();
    const int row = item ? m_view->indexOfTopLevelItem(item) : -1;
    const int target = row + offset;
    if (row < 0 || target < 0 || target >= int(m_searches.size()) || row >= int(m_searches.size()))
        return;
    const QString owner = item->data(0, ownerRole).toString();
    const QString id = item->data(0, idRole).toString();
    m_searches.move(row, target);
    fillView();
    // The keyboard stays on the moved row
    if (QTreeWidgetItem *moved = itemFor(owner, id))
        m_view->setCurrentItem(moved, 0, QItemSelectionModel::NoUpdate);
    emit orderChanged(m_searches);
}

void SavedSearchFolders::takeViewOrder()
{
    QList<SavedSearch> ordered;
    for (int row = 0, rows = m_view->topLevelItemCount(); row < rows; ++row) {
        if (const SavedSearch *search = searchOf(m_view->topLevelItem(row)))
            ordered << *search;
    }
    if (ordered.size() != m_searches.size() || ordered == m_searches) {
        fillView();
        return;
    }
    m_searches = ordered;
    fillView();
    emit orderChanged(m_searches);
}

}
