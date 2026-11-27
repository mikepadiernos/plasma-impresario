/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <KSharedConfig>

#include "EmbeddedPanel.h"

class QLabel;

namespace Imap {
namespace Mailbox {
class FavoriteTagsModel;
class PrettyUnifiedMsgListModel;
class UnifiedMsgListModel;
}
}

namespace Gui {

class MsgListView;

/** @short Embedded panel which lists the results of the quick search instead of the message list

While the panel is turned on, searches list their results here and the message list keeps showing the chosen
folder, so a result opens in the reader without leaving that folder. Turning the panel off brings the results
back to the message list. The window fills results() as it searches; the panel sorts them by relevance while
the search looks for text, or newest first.
*/
class SearchResultsPanel : public EmbeddedPanel
{
    Q_OBJECT
public:
    explicit SearchResultsPanel(Imap::Mailbox::FavoriteTagsModel *favoriteTags, QWidget *parent = nullptr);

    static Descriptor panelDescriptor();

    /** The messages found, one source per account, as the window adds them */
    Imap::Mailbox::UnifiedMsgListModel *results() const;
    /** The results as listed, sorted and narrowed by the quick filters and facets */
    Imap::Mailbox::PrettyUnifiedMsgListModel *prettyResults() const;
    MsgListView *view() const;

    /** Remember the sorting in the SearchResultsPanel group of @arg config (session state) */
    void setStateConfig(KSharedConfigPtr config);

    bool hasContent() const override;
    QWidget *focusTarget() const override;

    /** Whether the panel lists the results of a search rather than waiting for one */
    bool isSearchShown() const;
    /** Whether the results are sorted by relevance while the search looks for text (default: true) */
    bool sortsByRelevance() const;
    /** The message of the current row, of the IMAP model, or an invalid index */
    QModelIndex currentMessage() const;
    QString statusText() const;

public slots:
    /** List the results of a search looking for @arg terms (see UiUtils::relevanceTerms()), or wait for one */
    void setSearchShown(bool shown, const QStringList &terms = QStringList());
    void setSortsByRelevance(bool relevance);
    /** A summary of the search, with @arg details as its tooltip */
    void setStatus(const QString &text, const QString &details);
    /** Offer to stop the search, to search failed places again and to continue on the server */
    void setSearchActions(bool stop, bool retry, bool server);

signals:
    /** The user opened @arg message, of the IMAP model */
    void messageActivated(const QModelIndex &message);
    /** The user wants to see @arg message, of the IMAP model, in its folder */
    void revealRequested(const QModelIndex &message);
    void stopRequested();
    void retryRequested();
    void searchServerRequested();
    /** The panel was turned on or off, so the results belong in the panel or in the message list */
    void enabledChanged(bool enabled);

protected:
    void panelEnabledChanged() override;

private:
    void applySorting();
    void updateActions();
    void showContextMenu(const QPoint &position);

    Imap::Mailbox::UnifiedMsgListModel *m_results;
    Imap::Mailbox::PrettyUnifiedMsgListModel *m_pretty;
    QWidget *m_body;
    MsgListView *m_view;
    QLabel *m_status;
    QToolButton *m_revealButton;
    QToolButton *m_stopButton;
    QToolButton *m_retryButton;
    QToolButton *m_serverButton;
    QAction *m_relevanceAction;
    QAction *m_dateAction;
    KSharedConfigPtr m_stateConfig;
    QStringList m_terms;
    bool m_searchShown = false;
    bool m_sortsByRelevance = true;
};

}
