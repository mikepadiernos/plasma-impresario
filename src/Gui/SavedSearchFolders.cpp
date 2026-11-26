/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "SavedSearchFolders.h"

#include <QHeaderView>
#include <QIcon>
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
#include <utility>

namespace {

using UiUtils::SavedSearch;
using UiUtils::SearchScope::Places;

const int ownerRole = Qt::UserRole + 1;
const int idRole = Qt::UserRole + 2;
/** Rows shown before the list scrolls */
const int maximumRows = 8;

QIcon themedIcon(const QString &name, const QIcon &fallback = QIcon())
{
    return QIcon::fromTheme(name + QLatin1String("-symbolic"), QIcon::fromTheme(name, fallback));
}

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
    , m_view(new QTreeWidget(this))
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
    layout->addWidget(m_view);

    updateHeader();
    setVisible(false);
}

void SavedSearchFolders::setSearches(const QList<SavedSearch> &searches, const QHash<QString, QString> &ownerNames)
{
    m_searches = searches;
    m_ownerNames = ownerNames;
    m_pendingActivation = nullptr;
    QSet<QString> known;
    m_view->clear();
    for (const SavedSearch &search : searches) {
        known.insert(UiUtils::SavedSearches::countsKey(search.owner, search.id));
        auto *item = new QTreeWidgetItem(m_view);
        item->setData(0, ownerRole, search.owner);
        item->setData(0, idRole, search.id);
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        updateItem(item);
    }
    for (auto it = m_counts.begin(); it != m_counts.end();) {
        if (known.contains(it.key()))
            ++it;
        else
            it = m_counts.erase(it);
    }
    setActive(m_activeOwner, m_activeId);
    fitHeight();
    setVisible(!searches.isEmpty());
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
    return m_counts.value(UiUtils::SavedSearches::countsKey(owner, id));
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

void SavedSearchFolders::updateItem(QTreeWidgetItem *item)
{
    const QString owner = item->data(0, ownerRole).toString();
    const QString id = item->data(0, idRole).toString();
    const auto search = std::find_if(m_searches.cbegin(), m_searches.cend(),
                                     [&](const SavedSearch &s) { return s.owner == owner && s.id == id; });
    if (search == m_searches.cend())
        return;
    const UiUtils::SavedSearches::Counts counts = m_counts.value(UiUtils::SavedSearches::countsKey(owner, id));
    const bool severalOwners = m_ownerNames.size() > 1;
    const QString ownerName = severalOwners ? m_ownerNames.value(owner, owner) : QString();

    item->setText(0, search->name);
    item->setIcon(0, themedIcon(QStringLiteral("folder-saved-search"), themedIcon(QStringLiteral("edit-find"))));
    QFont font = m_view->font();
    font.setBold(counts.isKnown() && counts.unread > 0);
    item->setFont(0, font);
    item->setFont(1, font);

    QStringList tip;
    tip << search->name;
    if (!search->query.isEmpty())
        tip << i18nc("@info:tooltip the text of a saved search", "Query: %1", search->query);
    tip << scopeText(*search, ownerName);
    if (counts.isKnown()) {
        item->setText(1, i18nc("@item unread and all messages found by a saved search", "%1 / %2", counts.unread,
                               counts.total));
        tip << i18ncp("@info:tooltip", "Found %1 message when it last ran", "Found %1 messages when it last ran",
                      counts.total)
            + QLatin1String(", ") + i18ncp("@info:tooltip", "%1 unread", "%1 unread", counts.unread);
        item->setData(0, Qt::AccessibleTextRole,
                      i18nc("@info:accessibility saved search, unread and all messages it found", "%1, %2 unread of %3",
                            search->name, counts.unread, counts.total));
    } else {
        item->setText(1, QString());
        tip << i18nc("@info:tooltip", "Not run yet");
        item->setData(0, Qt::AccessibleTextRole, search->name);
    }
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

}
