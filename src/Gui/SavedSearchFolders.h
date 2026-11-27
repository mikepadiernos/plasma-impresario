/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QHash>
#include <QList>
#include <QWidget>

#include "UiUtils/SavedSearches.h"

class QAction;
class QMenu;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace Gui {

/** @short The saved searches of every account, shown as virtual folders below the folder tree

Each row is a saved search, with the number of unread and of all messages which it found when it last ran. Activating
a row asks to run the search through searchActivated(); the row of the search shown in the list is selected. The
header collapses and expands the rows, which the state config remembers. The panel hides itself while there are no
saved searches.

The context menu of a row, and the keys F2, Delete and Alt+Up or Alt+Down on the current row, ask to edit, rename,
delete or move the search; rows can also be dragged into another order. A search whose account was removed or whose
chosen folders are gone is marked, with options to update it or delete it. The panel only asks: whoever owns the
searches changes them and shows them again.

Virtual folders only list what their search finds: nothing is moved, copied or changed to fill them.
*/
class SavedSearchFolders : public QWidget
{
    Q_OBJECT
public:
    explicit SavedSearchFolders(QWidget *parent = nullptr);

    /** @short Show @arg searches, in their order

    @arg owners are the keys of the configured accounts, in their order, and @arg ownerNames their names; searches of
    other keys belong to removed accounts. The names appear in the tooltips when there is more than one account.
    */
    void setSearches(const QList<UiUtils::SavedSearch> &searches, const QStringList &owners,
                     const QHash<QString, QString> &ownerNames);
    /** @short Show @arg counts for the search @arg id of @arg owner; unknown counts are left blank */
    void setCounts(const QString &owner, const QString &id, const UiUtils::SavedSearches::Counts &counts);
    /** @short Mark what keeps the search @arg id of @arg owner from looking where it was saved to look */
    void setProblems(const QString &owner, const QString &id, const UiUtils::SavedSearches::Problems &problems);
    /** @short Mark the search shown in the list; empty strings when none is */
    void setActive(const QString &owner, const QString &id);

    QList<UiUtils::SavedSearch> searches() const { return m_searches; }
    UiUtils::SavedSearches::Counts counts(const QString &owner, const QString &id) const;
    UiUtils::SavedSearches::Problems problems(const QString &owner, const QString &id) const;
    QTreeWidget *view() const { return m_view; }
    bool isExpanded() const;
    void setExpanded(bool expanded);
    /** @short The menu of the row @arg item, as its context menu shows it; for tests */
    QMenu *menuFor(QTreeWidgetItem *item);

signals:
    /** @short The user chose the search @arg id of @arg owner */
    void searchActivated(const QString &owner, const QString &id);
    void editRequested(const QString &owner, const QString &id);
    void renameRequested(const QString &owner, const QString &id);
    void deleteRequested(const QString &owner, const QString &id);
    /** @short The search should belong to the account key @arg newOwner */
    void ownerChangeRequested(const QString &owner, const QString &id, const QString &newOwner);
    /** @short The user wants to choose the folders of the search */
    void foldersChoiceRequested(const QString &owner, const QString &id);
    /** @short The folders of the search which are gone should be left out */
    void missingFoldersRemovalRequested(const QString &owner, const QString &id);
    /** @short The user moved a search; @arg searches are in their new order */
    void orderChanged(const QList<UiUtils::SavedSearch> &searches);

private:
    /** @short Enable and show the actions which apply to @p item */
    void configureActions(QTreeWidgetItem *item);
    QTreeWidgetItem *itemFor(const QString &owner, const QString &id) const;
    const UiUtils::SavedSearch *searchOf(const QTreeWidgetItem *item) const;
    void fillView();
    void updateItem(QTreeWidgetItem *item);
    void updateHeader();
    void fitHeight();
    void requestActivation(QTreeWidgetItem *item);
    /** The row which the actions apply to: the one of the context menu, or the current row */
    QTreeWidgetItem *targetItem() const;
    void showContextMenu(const QPoint &pos);
    void moveTarget(int offset);
    /** Takes the order of the rows after they were dragged */
    void takeViewOrder();

    QToolButton *m_header;
    QTreeWidget *m_view;
    QList<UiUtils::SavedSearch> m_searches;
    QStringList m_owners;
    QHash<QString, QString> m_ownerNames;
    QHash<QString, UiUtils::SavedSearches::Counts> m_counts;
    QHash<QString, UiUtils::SavedSearches::Problems> m_problems;
    QString m_activeOwner;
    QString m_activeId;
    /** A click and the activation which follows it under single-click activation run the search once */
    QTreeWidgetItem *m_pendingActivation = nullptr;
    QTreeWidgetItem *m_menuItem = nullptr;
    QMenu *m_menu;
    QAction *m_run;
    QAction *m_edit;
    QAction *m_rename;
    QAction *m_moveUp;
    QAction *m_moveDown;
    QMenu *m_moveToAccount;
    QAction *m_chooseFolders;
    QAction *m_removeMissing;
    QAction *m_delete;
};

}
