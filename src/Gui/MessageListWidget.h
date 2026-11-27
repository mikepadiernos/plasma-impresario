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

#ifndef MESSAGELISTWIDGET_H
#define MESSAGELISTWIDGET_H

#include <QWidget>
#include <functional>
#include "Imap/Model/FavoriteTagsModel.h"
#include "UiUtils/MailingLists.h"
#include "UiUtils/SearchQuery.h"
#include "UiUtils/SavedSearches.h"
#include "UiUtils/SearchFacets.h"
#include "UiUtils/SearchScope.h"
#include "UiUtils/SearchSuggestions.h"

class LineEdit;
class KConfigGroup;
class QHBoxLayout;
class QIcon;
class QActionGroup;
class QCompleter;
class QLabel;
class QLineEdit;
class QMenu;
class QModelIndex;
class QStandardItemModel;
class QTimer;
class QToolButton;

namespace Gui {

class FlowLayout;
class InlineNotificationBar;
class MsgListView;

/** @short A short description of @p range, such as "Past Week" or "Since 3/5/26" */
QString searchDatesText(const UiUtils::SearchScope::DateRange &range);

/** @short Widget containing a list of messages along with quick filtering toolbar */
class MessageListWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MessageListWidget(QWidget *parent, Imap::Mailbox::FavoriteTagsModel *m_favoriteTagsModel, const QString &accountId);

    void setFuzzySearchSupported(bool supported);
    void setRawSearchEnabled(bool enabled);

    QStringList searchConditions() const;

    /** @short The dates which the quick search is limited to */
    UiUtils::SearchScope::DateRange dateRange() const;
    /** @short Limit the quick search to @p range and apply it */
    void setDateRange(const UiUtils::SearchScope::DateRange &range);

    /** @short Where the quick search looks: the shown list, chosen folders, the account or all accounts */
    UiUtils::SearchScope::Places searchPlaces() const;
    /** @short Search in @p places and apply the search */
    void setSearchPlaces(UiUtils::SearchScope::Places places);
    /** @short The folders chosen for searching selected folders, as keys of UiUtils::SearchScope::folderKey() */
    QStringList selectedFolders() const;
    /** @short Search the folders of @p keys, as chosen by the user, and apply the search */
    void setSelectedFolders(const QStringList &keys);
    /** @short Name the shown list in the Search In menu, such as "Unified Inbox"; empty for "Current Folder" */
    void setCurrentPlaceText(const QString &text);
    /** @short Empty the quick search and its date range, keeping the places, and apply that */
    void clearSearch();
    /** @short Use the search settings: the default places, the history limit, remembering and suggesting

    The places change only while no search is shown, so that a search in progress keeps its scope. */
    void applySearchSettings();

    /** Key saved searches to another account, for example after switching accounts. */
    void setAccountId(const QString &accountId);

    /** @short Add a compact button for @p action next to the quick search field */
    QToolButton *addToolButton(QAction *action);
    /** @short Add a button which opens @p menu next to the quick search field */
    QToolButton *addToolMenuButton(QMenu *menu, const QIcon &icon, const QString &text);
    /** @short Show @p text below the list, for example message counts, with details in @p toolTip; an empty text hides it */
    void setListStatus(const QString &text, const QString &toolTip = QString());
    /** @short Add a button for @p action after the status below the list; it is shown while the action is visible */
    QToolButton *addStatusButton(QAction *action);
    /** @short Show which account and folder the list belongs to next to the status; an empty folder hides it */
    void setScope(const QString &account, const QString &folder, const QString &fullFolderName);
    /** @short Show @p text as the scope instead of an account and a folder, with @p toolTip describing it */
    void setCustomScope(const QString &text, const QString &toolTip);
    /** @short Offer the quick search, or clear and disable it for a list which it cannot search */
    void setSearchAvailable(bool available);
    /** @short Name the quick filters which narrow the list, such as "Unread", in the summary above it */
    void setActiveFilters(const QStringList &names);

    using FacetSource = std::function<QModelIndexList()>;
    /** @short Count the facets among the messages which @p source lists, those of the results before any facet */
    void setFacetSource(const FacetSource &source);
    /** @short The account of a list of one folder, and the day which dates are relative to, for the facets */
    void setFacetContext(const UiUtils::SearchFacets::Context &context);
    /** @short The facets chosen to narrow the results of the search, with their context */
    UiUtils::SearchFacets facets() const;
    /** @short Stop narrowing the results by facets */
    void clearFacets();
    /** @short The menu which offers the facets of the results with their counts */
    QMenu *facetsMenu() const;

    using FolderSource = std::function<QList<UiUtils::SearchSuggestions::FolderCandidate>()>;
    /** @short Offer the folders which @p source lists, without contacting the server, as places to search */
    void setFolderSource(const FolderSource &source);
    /** @short Queries run from the quick search, most recent first, as remembered for this profile */
    QStringList searchHistory() const;

    /** @short Run the search which the account with the key @p owner saved as @p id; false when there is none
    The search looks where it was saved to look; for the shown folder or account, the caller shows the owner first.
    */
    bool runSavedSearch(const QString &owner, const QString &id);
    /** @short The owner and the ID of the saved search shown in the list, or empty strings
    A saved search stays shown until the search changes, is cleared or another account is shown.
    */
    QString activeSavedSearchOwner() const;
    QString activeSavedSearchId() const;
    /** @short The text of the quick search field */
    QString searchText() const;
    /** @short Treat the list as an ordinary search from now on, such as after another folder is chosen */
    void forgetActiveSavedSearch();
    /** @short Run the saved search @p id of @p owner and let the user change it, with Save and Cancel above the list

    The list shows what the changed search finds, so that it can be checked before it is saved. Save keeps the
    changes under the search's name; Cancel runs the saved search again. Another account or another edit ends the
    editing without saving. False when the search cannot run.
    */
    bool editSavedSearch(const QString &owner, const QString &id);
    /** @short The owner and the ID of the saved search being edited, or empty strings */
    QString editedSavedSearchOwner() const;
    QString editedSavedSearchId() const;
    /** @short Stop editing without saving and without running the search again */
    void endSavedSearchEdit();
    /** @short Ask for a new name of the saved search @p id of @p owner; false when it was not renamed */
    bool renameSavedSearch(const QString &owner, const QString &id);
    /** @short Delete the saved search @p id of @p owner after confirmation; false when it was not deleted */
    bool deleteSavedSearch(const QString &owner, const QString &id);
    /** @short The SavedSearches group of the settings, which holds the searches of every account */
    static KConfigGroup savedSearchesRoot();
    /** @short Save a search for the messages of @p list in every folder of every account, named after the list, and
    run it

    When this account already saved one, that search runs instead. False when the list is invalid or the search
    cannot be saved.
    */
    bool pinMailingList(const UiUtils::MailingList &list);

    // FIXME: consider making this private and moving the logic from Window with it
    MsgListView *tree;

    /** @short The quick search field, a stop of its own when focus moves between panes */
    QLineEdit *searchField() const;

public slots:
    void focusSearch();
    /** @short Put a field filter such as "from:ann@example.org" into the quick search and apply it */
    void showFieldFilter(const QString &text);
    void focusRawSearch();
    /** @short Build a query from conditions, starting with those of the quick search, and search for it */
    void openSearchBuilder();
    /** @short Offer @p contacts of the address book for the word @p query, as asked by contactSuggestionsRequested() */
    void showContactSuggestions(const QString &query, const QList<UiUtils::SearchSuggestions::Contact> &contacts);

signals:
    void requestingSearch(const QStringList &conditions);
    void rawSearchSettingChanged(bool enabled);
    /** @short The user wants to pick the folders to search; answer with setSelectedFolders() */
    void foldersChoiceRequested();
    /** @short Facets were chosen or removed; facets() has the new ones */
    void facetsChanged();
    /** @short The user wants the search, its date range and the quick filters cleared at once */
    void clearAllRequested();
    /** @short Suggestions want the contacts matching @p query; answer with showContactSuggestions() */
    void contactSuggestionsRequested(const QString &query);
    /** @short A search was saved, changed, renamed or deleted */
    void savedSearchesChanged();
    /** @short The saved search shown in the list changed; empty strings when none is shown */
    void activeSavedSearchChanged(const QString &owner, const QString &id);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

protected slots:
    void slotApplySearch();
    void slotAutoEnableDisableSearch();
    void slotSortingFailed();

private slots:
    /** Saves the current search for this account. */
    void saveCurrentSearch();
    /** Rebuilds the menu of searches saved for this account. */
    void rebuildSavedSearchMenu();
    /** Rebuilds the menu of the mailing lists of the loaded messages. */
    void rebuildMailingListMenu();
    void slotComplexSearchInput(QAction*);
    void slotConditionalSearchReset();
    void slotDeActivateSimpleSearch();
    void slotResetSortingFailed();
    void slotUpdateSearchCursor();

private:
    /** Returns the saved-search configuration for this account. */
    KConfigGroup savedSearchGroup() const;
    /** Adds @p search to the menu. */
    void addSavedSearch(const UiUtils::SavedSearch &search);
    /** The quick search as a saved search, without an owner, an ID or a name */
    UiUtils::SavedSearch currentSearch() const;
    void showSavedSearchEdit();
    void saveSavedSearchEdit();
    /** Stops editing and runs the saved search as it was */
    void cancelSavedSearchEdit();
    /** Applies @p search, with its fields, date range and places */
    void applySavedSearch(const UiUtils::SavedSearch &search);
    /** Whether the quick search is set up as @p search is */
    bool matchesSavedSearch(const UiUtils::SavedSearch &search) const;
    void setActiveSavedSearch(const UiUtils::SavedSearch &search);
    /** Fills the facets menu with the values of the shown results and their counts */
    void rebuildFacetsMenu();
    /** Shows a button to remove each chosen facet above the list */
    void updateFacetChips();
    void setFacets(const UiUtils::SearchFacets &facets);
    /** Asks for the first and last day of a custom date range. */
    void chooseCustomDateRange();
    /** Shows the date range and the places in the menus, the placeholder and the tooltip of the quick search. */
    void showDateRange();
    /** A short description of where the search looks, such as "All Accounts" */
    QString placesText() const;
    /** Shows the row below the list while it has anything to show */
    void updateStatusRow();
    /** Describes the search and the filters in effect above the list, or hides the summary when nothing narrows it */
    void updateSearchSummary();
    /** IMAP SEARCH keys for the quick search; when the query cannot be used, an empty list and the reason in
    @p problem. @p relativeDates says whether the query names days such as today. */
    QStringList searchConditions(UiUtils::SearchQuery::Error *problem, bool *relativeDates) const;
    /** Explains below the quick search why its query cannot be used and selects the text it is about */
    void showSearchError(const UiUtils::SearchQuery::Error &problem);
    /** Applies the quick search; false when its query cannot be used */
    bool applySearch();
    /** Applies the query typed in the quick search and remembers it */
    void runTypedSearch();
    /** Puts @p query first in the search history, unless searches are not remembered */
    void rememberSearch(const QString &query);
    void saveSearchHistory();
    void updateHistoryActions();
    /** Offers suggestions for the text being typed, unless they are turned off */
    void updateSuggestions();
    /** Asks for the contacts matching the word being typed, unless they have been asked for already */
    void requestContacts();
    /** Fills the list of suggestions for the quick search text */
    void fillSuggestions();
    /** Runs the search which the suggestion at @p index stands for */
    void chooseSuggestion(const QModelIndex &index);

    LineEdit *m_quickSearchText;
    /** The quick search field and the buttons next to it */
    QHBoxLayout *m_toolRow;
    /** Why the query in the quick search cannot be used */
    InlineNotificationBar *m_searchError;
    /** What narrows the list: the query, its dates and places, and the quick filters */
    QWidget *m_searchSummary;
    QLabel *m_searchSummaryText;
    QAction *m_clearAll;
    UiUtils::SearchFacets m_facets;
    FacetSource m_facetSource;
    QMenu *m_facetsMenu;
    QToolButton *m_narrowButton;
    /** A button for each chosen facet, which removes it */
    QWidget *m_facetChips;
    FlowLayout *m_facetChipsLayout;
    /** The query, dates and places of the search in effect, as the summary shows them; an invalid query typed
    later does not change them */
    QStringList m_searchDescription;
    QStringList m_activeFilters;
    QLabel *m_listStatus;
    /** Account and folder of the list, in the row of the status */
    QLabel *m_scope;
    QWidget *m_statusRow;
    QHBoxLayout *m_statusLayout;
    QList<QToolButton *> m_statusButtons;
    QAction *m_searchOptions;
    QAction *m_searchInSubject;
    QAction *m_searchInBody;
    QAction *m_searchInSenders;
    QAction *m_searchInRecipients;
    QAction *m_searchFuzzy;
    QAction *m_rawSearch;
    QMenu *m_dateMenu;
    QActionGroup *m_dateActions;
    UiUtils::SearchScope::DateRange m_dateRange;
    QMenu *m_placesMenu;
    QActionGroup *m_placesActions;
    QAction *m_currentPlace;
    UiUtils::SearchScope::Places m_places = UiUtils::SearchScope::Places::CurrentFolder;
    /** Where a new search looks, as chosen in the settings */
    UiUtils::SearchScope::Places m_defaultPlaces = UiUtils::SearchScope::Places::CurrentFolder;
    QStringList m_selectedFolders;
    QString m_currentPlaceText;
    QString m_searchToolTip;
    QMenu *m_savedSearches;
    QMenu *m_mailingLists;
    QString m_accountId;
    bool m_supportsFuzzySearch;
    bool m_searchAvailable = true;
    QTimer *m_searchResetTimer;
    /** Applies a preset date range or a query with date:today again once the day changes, so that "Today" moves on
    at midnight */
    QTimer *m_dayChangeTimer;
    /** The day on which the last search was applied */
    QDate m_searchDay;
    QString m_queryPlaceholder;
    Imap::Mailbox::FavoriteTagsModel *m_favoriteTags;
    QAction *m_rememberSearches;
    QAction *m_suggestWhileTyping;
    QAction *m_clearSearchHistory;
    UiUtils::SearchHistory m_searchHistory;
    QStandardItemModel *m_suggestions;
    QCompleter *m_completer;
    FolderSource m_folderSource;
    /** The word which contacts were last asked for, and the answer */
    QString m_contactQuery;
    QList<UiUtils::SearchSuggestions::Contact> m_contacts;
    /** Escape closed the suggestions, so its release must not leave the quick search */
    bool m_suggestionsEscaped = false;
    /** The saved search shown in the list; its owner is empty when none is */
    UiUtils::SavedSearch m_activeSavedSearch;
    /** A saved search is being set up, so the searches applied meanwhile do not end it */
    bool m_applyingSavedSearch = false;
    /** The saved search being edited; its owner is empty when none is */
    UiUtils::SavedSearch m_editedSavedSearch;
    InlineNotificationBar *m_savedSearchEdit;

    friend class MainWindow; // needs access to our private slots
};

}

#endif // MESSAGELISTWIDGET_H
