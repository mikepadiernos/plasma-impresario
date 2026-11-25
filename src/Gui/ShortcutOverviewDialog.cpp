/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ShortcutOverviewDialog.h"

#include <KLocalizedString>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "ShortcutHandler/ShortcutHandler.h"

namespace Gui {

ShortcutOverviewDialog::ShortcutOverviewDialog(const EntriesProvider &provider, const QStringList &areaOrder, QWidget *parent)
    : QDialog(parent)
    , m_provider(provider)
    , m_areaOrder(areaOrder)
    , m_search(new QLineEdit(this))
    , m_list(new QTreeWidget(this))
    , m_configure(new QPushButton(this))
{
    setObjectName(QStringLiteral("shortcutOverviewDialog"));
    setWindowTitle(i18nc("@title:window", "Keyboard Shortcuts"));
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(false);

    auto *searchLabel = new QLabel(i18nc("@label:textbox", "&Search:"), this);
    searchLabel->setBuddy(m_search);
    m_search->setObjectName(QStringLiteral("shortcutOverviewSearch"));
    m_search->setClearButtonEnabled(true);
    m_search->setPlaceholderText(i18nc("@info:placeholder", "Search actions and shortcuts"));
    connect(m_search, &QLineEdit::textChanged, this, &ShortcutOverviewDialog::showGroups);

    m_list->setObjectName(QStringLiteral("shortcutOverviewList"));
    m_list->setColumnCount(2);
    m_list->setHeaderLabels({i18nc("@title:column", "Action"), i18nc("@title:column", "Shortcut")});
    m_list->setAccessibleName(i18nc("@label", "Keyboard shortcuts"));
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setAlternatingRowColors(true);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_list->header()->setStretchLastSection(false);

    m_configure->setObjectName(QStringLiteral("shortcutOverviewConfigure"));
    m_configure->setText(i18nc("@action:button", "Configure Shortcuts…"));
    m_configure->setIcon(QIcon::fromTheme(QStringLiteral("configure-shortcuts")));
    connect(m_configure, &QPushButton::clicked, this, &ShortcutOverviewDialog::configureShortcuts);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->addButton(m_configure, QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *searchRow = new QHBoxLayout;
    searchRow->addWidget(searchLabel);
    searchRow->addWidget(m_search, 1);
    auto *layout = new QVBoxLayout(this);
    layout->addLayout(searchRow);
    layout->addWidget(m_list, 1);
    layout->addWidget(buttons);

    resize(fontMetrics().averageCharWidth() * 70, fontMetrics().height() * 32);
    refresh();
    m_search->setFocus();
}

void ShortcutOverviewDialog::refresh()
{
    m_groups = UiUtils::ShortcutOverview::group(m_provider ? m_provider() : QList<UiUtils::ShortcutOverview::Entry>(), m_areaOrder);
    showGroups();
}

void ShortcutOverviewDialog::showGroups()
{
    m_list->clear();
    const auto groups = UiUtils::ShortcutOverview::filter(m_groups, m_search->text());
    for (const UiUtils::ShortcutOverview::Group &group : groups) {
        auto *groupItem = new QTreeWidgetItem(m_list, {group.area});
        groupItem->setFlags(Qt::ItemIsEnabled);
        groupItem->setFirstColumnSpanned(true);
        QFont font = groupItem->font(0);
        font.setBold(true);
        groupItem->setFont(0, font);
        for (const UiUtils::ShortcutOverview::Entry &entry : group.entries) {
            const QString shortcut = UiUtils::ShortcutOverview::shortcutText(entry);
            auto *item = new QTreeWidgetItem(groupItem, {entry.text, shortcut});
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            item->setData(0, Qt::UserRole, entry.id);
            if (shortcut.isEmpty())
                item->setData(1, Qt::AccessibleTextRole, i18nc("@info no keyboard shortcut", "None"));
        }
        groupItem->setExpanded(true);
    }
}

void ShortcutOverviewDialog::configureShortcuts()
{
    // The configuration dialog is modal; afterwards the list shows the changed shortcuts
    ShortcutHandler::instance()->shortcutConfigAction()->trigger();
    refresh();
}

}
