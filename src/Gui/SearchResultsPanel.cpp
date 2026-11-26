/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "SearchResultsPanel.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <QActionGroup>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/UnifiedMsgListModel.h"
#include "Imap/Model/Utils.h"
#include "MsgListView.h"

namespace Gui {

namespace {

const auto stateGroup = QStringLiteral("SearchResultsPanel");

/** The message behind a row of the panel, or an invalid index for a row which is not a message yet */
QModelIndex messageOf(const QModelIndex &row)
{
    const QModelIndex message = Imap::deproxifiedIndex(row);
    return message.isValid() && message.data(Imap::Mailbox::RoleMessageUid).toUInt() ? message : QModelIndex();
}

}

EmbeddedPanel::Descriptor SearchResultsPanel::panelDescriptor()
{
    Descriptor descriptor;
    descriptor.id = QStringLiteral("SearchResults");
    descriptor.configKey = QStringLiteral("SearchResultsPanel");
    descriptor.title = i18nc("@title embedded panel", "Search Results");
    descriptor.autoHideToolTip = i18nc("@info:tooltip", "Show the search results panel only while a search lists its results");
    descriptor.defaultEnabled = false;
    descriptor.defaultPlacement = Placement::RightColumn;
    return descriptor;
}

SearchResultsPanel::SearchResultsPanel(Imap::Mailbox::FavoriteTagsModel *favoriteTags, QWidget *parent)
    : EmbeddedPanel(panelDescriptor(), QStringLiteral("searchResultsPanel"), parent)
    , m_results(new Imap::Mailbox::UnifiedMsgListModel(this))
    , m_pretty(new Imap::Mailbox::PrettyUnifiedMsgListModel(this))
    , m_body(new QWidget(this))
    , m_view(new MsgListView(m_body, favoriteTags))
    , m_status(new QLabel(m_body))
{
    m_results->setObjectName(QStringLiteral("searchResultsPanelResults"));
    m_results->setShowsFolders(true);
    m_pretty->setObjectName(QStringLiteral("searchResultsPanelModel"));
    m_pretty->setSourceModel(m_results);

    m_view->setObjectName(QStringLiteral("searchResultsPanelList"));
    m_view->setAccessibleName(i18nc("@label", "Messages found by the search"));
    // The panel decides the order, and a narrow panel has no room for columns
    m_view->setSortingEnabled(false);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->setModel(m_pretty);
    m_view->setMultiLineRows(true);
    m_view->setSearchSnippets(true);
    const auto activate = [this](const QModelIndex &row) {
        const QModelIndex message = messageOf(row);
        if (message.isValid())
            emit messageActivated(message);
    };
    connect(m_view, &QAbstractItemView::clicked, this, activate);
    connect(m_view, &QAbstractItemView::activated, this, activate);
    connect(m_view, &QWidget::customContextMenuRequested, this, &SearchResultsPanel::showContextMenu);
    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &current) {
        // A message which stops matching the quick filters once it was read stays until another one is opened
        if (current.isValid())
            m_pretty->setKeptVisibleIndex(current);
        updateActions();
    });
    connect(m_pretty, &QAbstractItemModel::modelReset, this, &SearchResultsPanel::updateActions);
    connect(m_pretty, &QAbstractItemModel::rowsRemoved, this, &SearchResultsPanel::updateActions);

    m_status->setObjectName(QStringLiteral("searchResultsPanelStatus"));
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    m_status->setForegroundRole(QPalette::PlaceholderText);
    m_status->hide();

    auto *layout = new QVBoxLayout(m_body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(m_view, 1);
    layout->addWidget(m_status);
    setContentWidget(m_body);
    setPreferredRows(6);

    m_revealButton = addHeaderButton(QStringLiteral("Reveal"), QStringLiteral("go-jump"), i18nc("@action:button", "Show in Folder"));
    m_revealButton->setToolTip(i18nc("@info:tooltip", "Open the folder of the selected result and select the message there"));
    connect(m_revealButton, &QToolButton::clicked, this, [this]() {
        const QModelIndex message = currentMessage();
        if (message.isValid())
            emit revealRequested(message);
    });

    QToolButton *sortButton = addHeaderButton(QStringLiteral("Sort"), QStringLiteral("view-sort"), i18nc("@action:button", "Sort Results"));
    auto *sortMenu = new QMenu(sortButton);
    auto *sortGroup = new QActionGroup(sortMenu);
    m_relevanceAction = sortMenu->addAction(i18nc("@option:radio sorting of search results", "Most Relevant First"));
    m_relevanceAction->setObjectName(QStringLiteral("searchResultsPanelSortRelevance"));
    m_relevanceAction->setToolTip(
        i18nc("@info:tooltip", "Show the messages which match the searched text best first; searches without text list the newest first"));
    m_dateAction = sortMenu->addAction(i18nc("@option:radio sorting of search results", "Newest First"));
    m_dateAction->setObjectName(QStringLiteral("searchResultsPanelSortDate"));
    for (QAction *action : {m_relevanceAction, m_dateAction}) {
        action->setCheckable(true);
        sortGroup->addAction(action);
    }
    m_relevanceAction->setChecked(true);
    connect(sortGroup, &QActionGroup::triggered, this, [this](QAction *action) { setSortsByRelevance(action == m_relevanceAction); });
    sortButton->setMenu(sortMenu);
    sortButton->setPopupMode(QToolButton::InstantPopup);

    m_stopButton = addHeaderButton(QStringLiteral("Stop"), QStringLiteral("process-stop"), i18nc("@action:button", "Stop"));
    m_stopButton->setToolTip(i18nc("@info:tooltip", "Stop searching and keep the messages found so far"));
    connect(m_stopButton, &QToolButton::clicked, this, &SearchResultsPanel::stopRequested);
    m_retryButton = addHeaderButton(QStringLiteral("Retry"), QStringLiteral("view-refresh"), i18nc("@action:button", "Retry"));
    m_retryButton->setToolTip(i18nc("@info:tooltip", "Search the places which could not be searched again"));
    connect(m_retryButton, &QToolButton::clicked, this, &SearchResultsPanel::retryRequested);
    m_serverButton = addHeaderButton(QStringLiteral("SearchServer"), QStringLiteral("network-server"),
                                     i18nc("@action:button", "Search Server"));
    m_serverButton->setToolTip(i18nc("@info:tooltip", "Search the server as well and add the messages which it finds"));
    connect(m_serverButton, &QToolButton::clicked, this, &SearchResultsPanel::searchServerRequested);
    setSearchActions(false, false, false);

    setSearchShown(false);
}

Imap::Mailbox::UnifiedMsgListModel *SearchResultsPanel::results() const
{
    return m_results;
}

Imap::Mailbox::PrettyUnifiedMsgListModel *SearchResultsPanel::prettyResults() const
{
    return m_pretty;
}

MsgListView *SearchResultsPanel::view() const
{
    return m_view;
}

void SearchResultsPanel::setStateConfig(KSharedConfigPtr config)
{
    m_stateConfig = config;
    if (m_stateConfig)
        setSortsByRelevance(KConfigGroup(m_stateConfig, stateGroup).readEntry("SortByRelevance", true));
}

bool SearchResultsPanel::hasContent() const
{
    return m_searchShown;
}

QWidget *SearchResultsPanel::focusTarget() const
{
    return m_view;
}

bool SearchResultsPanel::isSearchShown() const
{
    return m_searchShown;
}

bool SearchResultsPanel::sortsByRelevance() const
{
    return m_sortsByRelevance;
}

QModelIndex SearchResultsPanel::currentMessage() const
{
    return messageOf(m_view->currentIndex());
}

QString SearchResultsPanel::statusText() const
{
    return m_status->text();
}

void SearchResultsPanel::setSearchShown(bool shown, const QStringList &terms)
{
    m_searchShown = shown;
    m_terms = shown ? terms : QStringList();
    if (shown) {
        showContent();
    } else {
        showPlaceholder(i18nc("@info", "While this panel is shown, the results of the quick search are listed here "
                                       "and the message list keeps showing its folder."));
        setStatus(QString(), QString());
        setSearchActions(false, false, false);
    }
    applySorting();
    updateActions();
    updateVisibility();
}

void SearchResultsPanel::setSortsByRelevance(bool relevance)
{
    m_relevanceAction->setChecked(relevance);
    m_dateAction->setChecked(!relevance);
    if (m_sortsByRelevance == relevance)
        return;
    m_sortsByRelevance = relevance;
    if (m_stateConfig) {
        KConfigGroup group(m_stateConfig, stateGroup);
        group.writeEntry("SortByRelevance", relevance);
        group.sync();
    }
    applySorting();
}

void SearchResultsPanel::setStatus(const QString &text, const QString &details)
{
    m_status->setText(text);
    m_status->setToolTip(details);
    m_status->setVisible(!text.isEmpty());
}

void SearchResultsPanel::setSearchActions(bool stop, bool retry, bool server)
{
    m_stopButton->setVisible(stop);
    m_retryButton->setVisible(retry);
    m_serverButton->setVisible(server);
}

void SearchResultsPanel::panelEnabledChanged()
{
    emit enabledChanged(isPanelEnabled());
}

void SearchResultsPanel::applySorting()
{
    if (m_sortsByRelevance && !m_terms.isEmpty()) {
        if (m_pretty->relevanceTerms() != m_terms)
            m_pretty->setRelevanceTerms(m_terms, Qt::AscendingOrder);
        return;
    }
    if (m_pretty->isSortingByRelevance())
        m_pretty->setRelevanceTerms(QStringList(), Qt::DescendingOrder);
    m_pretty->sort(Imap::Mailbox::MsgListModel::DATE, Qt::DescendingOrder);
}

void SearchResultsPanel::updateActions()
{
    m_revealButton->setEnabled(m_searchShown && currentMessage().isValid());
}

void SearchResultsPanel::showContextMenu(const QPoint &position)
{
    const QModelIndex message = messageOf(m_view->indexAt(position));
    if (!message.isValid())
        return;
    QMenu menu(this);
    QAction *open = menu.addAction(themedIcon(QStringLiteral("mail-message")), i18nc("@action:inmenu", "Open"));
    open->setObjectName(QStringLiteral("searchResultsPanelOpen"));
    QAction *reveal = menu.addAction(themedIcon(QStringLiteral("go-jump")), i18nc("@action:inmenu", "Show in Folder"));
    reveal->setObjectName(QStringLiteral("searchResultsPanelShowInFolder"));
    QAction *chosen = menu.exec(m_view->viewport()->mapToGlobal(position));
    if (chosen == open)
        emit messageActivated(message);
    else if (chosen == reveal)
        emit revealRequested(message);
}

}
