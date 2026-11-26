/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QHash>
#include <QList>
#include <QWidget>

#include "UiUtils/SavedSearches.h"

class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace Gui {

/** @short The saved searches of every account, shown as virtual folders below the folder tree

Each row is a saved search, with the number of unread and of all messages which it found when it last ran. Activating
a row asks to run the search through searchActivated(); the row of the search shown in the list is selected. The
header collapses and expands the rows, which the state config remembers. The panel hides itself while there are no
saved searches.

Virtual folders only list what their search finds: nothing is moved, copied or changed to fill them.
*/
class SavedSearchFolders : public QWidget
{
    Q_OBJECT
public:
    explicit SavedSearchFolders(QWidget *parent = nullptr);

    /** @short Show @arg searches, in their order; @arg ownerNames gives the name of each account key

    The names appear in the tooltips when the searches belong to more than one account.
    */
    void setSearches(const QList<UiUtils::SavedSearch> &searches, const QHash<QString, QString> &ownerNames);
    /** @short Show @arg counts for the search @arg id of @arg owner; unknown counts are left blank */
    void setCounts(const QString &owner, const QString &id, const UiUtils::SavedSearches::Counts &counts);
    /** @short Mark the search shown in the list; empty strings when none is */
    void setActive(const QString &owner, const QString &id);

    QList<UiUtils::SavedSearch> searches() const { return m_searches; }
    UiUtils::SavedSearches::Counts counts(const QString &owner, const QString &id) const;
    QTreeWidget *view() const { return m_view; }
    bool isExpanded() const;
    void setExpanded(bool expanded);

signals:
    /** @short The user chose the search @arg id of @arg owner */
    void searchActivated(const QString &owner, const QString &id);

private:
    QTreeWidgetItem *itemFor(const QString &owner, const QString &id) const;
    void updateItem(QTreeWidgetItem *item);
    void updateHeader();
    void fitHeight();
    void requestActivation(QTreeWidgetItem *item);

    QToolButton *m_header;
    QTreeWidget *m_view;
    QList<UiUtils::SavedSearch> m_searches;
    QHash<QString, QString> m_ownerNames;
    QHash<QString, UiUtils::SavedSearches::Counts> m_counts;
    QString m_activeOwner;
    QString m_activeId;
    /** A click and the activation which follows it under single-click activation run the search once */
    QTreeWidgetItem *m_pendingActivation = nullptr;
};

}
