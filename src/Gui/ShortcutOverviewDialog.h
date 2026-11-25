/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDialog>
#include <QStringList>
#include <functional>

#include "UiUtils/ShortcutOverview.h"

class QLineEdit;
class QPushButton;
class QTreeWidget;

namespace Gui {

/** @short Read-only, searchable list of the keyboard shortcuts, grouped by menu or part of the window

The entries come from a provider, so that the list can follow changes made in the shortcut configuration dialog,
which the Configure Shortcuts button opens.
*/
class ShortcutOverviewDialog : public QDialog
{
    Q_OBJECT
public:
    using EntriesProvider = std::function<QList<UiUtils::ShortcutOverview::Entry>()>;

    /** @short Areas listed in @arg areaOrder come first, in that order */
    ShortcutOverviewDialog(const EntriesProvider &provider, const QStringList &areaOrder, QWidget *parent = nullptr);

    /** @short Read the entries again and show those which match the search text */
    void refresh();

private:
    void showGroups();
    void configureShortcuts();

    EntriesProvider m_provider;
    QStringList m_areaOrder;
    QList<UiUtils::ShortcutOverview::Group> m_groups;
    QLineEdit *m_search;
    QTreeWidget *m_list;
    QPushButton *m_configure;
};

}
