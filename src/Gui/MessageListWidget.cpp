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

#include "MessageListWidget.h"
#include <algorithm>
#include <utility>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QAction>
#include <QActionGroup>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QCompleter>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QStringView>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidgetAction>
#include "FlowLayout.h"
#include "InlineNotificationBar.h"
#include "LineEdit.h"
#include "MsgListView.h"
#include "ReplaceCharValidator.h"
#include "SearchBuilderDialog.h"
#include "StateColors.h"
#include "Util.h"
#include "UiUtils/IconLoader.h"
#include "UiUtils/MailingLists.h"
#include "UiUtils/QuickFilters.h"

namespace Gui {

namespace {

using UiUtils::SearchScope::DateRange;
using UiUtils::SearchScope::Dates;
using UiUtils::SearchScope::Places;
using SuggestionKind = UiUtils::SearchSuggestions::Kind;

constexpr int RoleSuggestionKind = Qt::UserRole + 1;
constexpr int RoleSuggestionText = Qt::UserRole + 2;
constexpr int RoleSuggestionFolder = Qt::UserRole + 3;

/** How many values of one kind the facets menu lists; chosen values are always listed */
constexpr int FacetMenuValues = 25;

QString facetMenuTitle(const UiUtils::SearchFacets::Kind kind)
{
    using Kind = UiUtils::SearchFacets::Kind;
    switch (kind) {
    case Kind::Account:
        return i18nc("@title:menu search facet", "&Account");
    case Kind::Folder:
        return i18nc("@title:menu search facet", "F&older");
    case Kind::Sender:
        return i18nc("@title:menu search facet", "&Sender");
    case Kind::Date:
        return i18nc("@title:menu search facet", "&Date");
    case Kind::Attachments:
        return i18nc("@title:menu search facet", "A&ttachments");
    case Kind::Tag:
        return i18nc("@title:menu search facet", "Ta&g");
    }
    return QString();
}

QString facetObjectName(const UiUtils::SearchFacets::Kind kind)
{
    using Kind = UiUtils::SearchFacets::Kind;
    switch (kind) {
    case Kind::Account:
        return QStringLiteral("searchFacetAccount");
    case Kind::Folder:
        return QStringLiteral("searchFacetFolder");
    case Kind::Sender:
        return QStringLiteral("searchFacetSender");
    case Kind::Date:
        return QStringLiteral("searchFacetDate");
    case Kind::Attachments:
        return QStringLiteral("searchFacetAttachments");
    case Kind::Tag:
        return QStringLiteral("searchFacetTag");
    }
    return QString();
}

/** @short Offers every suggestion as it is, without picking one which starts like the typed text

In UnfilteredPopupCompletion, QCompleter makes the first entry which starts with the typed text current, and Return
would then replace what the user typed with it. No entry starts with this path, so Return runs the typed text unless
the user moves to a suggestion first.
*/
class SuggestionCompleter : public QCompleter
{
public:
    using QCompleter::QCompleter;

    QStringList splitPath(const QString &) const override
    {
        return {QString(QChar(0x1))};
    }
};

QIcon suggestionIcon(const QString &name)
{
    return UiUtils::SymbolicIcon::fromTheme(name);
}

/** @short Whether to remember searches and suggest while typing, as chosen in the search options */
KConfigGroup quickSearchGroup()
{
    return KConfigGroup(KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")), QStringLiteral("QuickSearch"));
}

/** @short The searches of all profiles, in the state of the application rather than in its settings */
KConfigGroup searchHistoryGroup()
{
    return KConfigGroup(KSharedConfig::openStateConfig(), QStringLiteral("SearchHistory"));
}

QString searchHistoryKey()
{
    return UiUtils::SearchHistory::profileKey(QString::fromUtf8(qgetenv("TROJITA_PROFILE")));
}

QString dateRangeText(const DateRange &range)
{
    return searchDatesText(range);
}

}

QString searchDatesText(const DateRange &range)
{
    switch (range.dates) {
    case Dates::AnyTime:
        return i18nc("@item search date range", "Any Time");
    case Dates::Today:
        return i18nc("@item search date range", "Today");
    case Dates::PastWeek:
        return i18nc("@item search date range", "Past Week");
    case Dates::PastMonth:
        return i18nc("@item search date range", "Past Month");
    case Dates::PastYear:
        return i18nc("@item search date range", "Past Year");
    case Dates::Custom:
        break;
    }
    const UiUtils::SearchScope::Days days = UiUtils::SearchScope::resolve(range, QDate::currentDate());
    const QLocale locale;
    const QString first = locale.toString(days.first, QLocale::ShortFormat);
    const QString last = locale.toString(days.last, QLocale::ShortFormat);
    if (days.first.isValid() && days.last.isValid()) {
        if (days.first == days.last)
            return first;
        return i18nc("@item search date range, from the first to the last day", "%1 to %2", first, last);
    }
    if (days.first.isValid())
        return i18nc("@item search date range without an end", "Since %1", first);
    if (days.last.isValid())
        return i18nc("@item search date range without a start", "Until %1", last);
    return i18nc("@item search date range", "Any Time");
}

MessageListWidget::MessageListWidget(QWidget *parent, Imap::Mailbox::FavoriteTagsModel *m_favoriteTagsModel, const QString &accountId) :
    QWidget(parent), m_accountId(accountId), m_supportsFuzzySearch(false), m_favoriteTags(m_favoriteTagsModel)
{
    tree = new MsgListView(this, m_favoriteTagsModel);
    tree->setObjectName(QStringLiteral("messageListView"));
    tree->setAccessibleName(i18nc("@info:accessible", "Messages"));

    m_quickSearchText = new LineEdit(this);
    m_quickSearchText->setObjectName(QStringLiteral("quickSearch"));
    m_quickSearchText->setAccessibleName(i18nc("@info:accessible", "Quick search"));
    // Filter out newline. It will wreak havoc into the direct IMAP passthrough and could lead to data loss.
    QValidator *validator = new ReplaceCharValidator(QLatin1Char('\n'), QLatin1Char(' '), m_quickSearchText);
    m_quickSearchText->setValidator(validator);
    m_quickSearchText->setPlaceholderText(tr("Quick Search"));
    m_searchToolTip = i18nc("@info:tooltip field names, operators and values stay in English",
                            "Type words to search for in the fields chosen with the icon on the left; all of them must "
                            "match.<br/>"
                            "Search one field with <code>from:</code>, <code>to:</code>, <code>cc:</code>, <code>bcc:</code>, "
                            "<code>subject:</code>, <code>body:</code>, <code>text:</code> or <code>list:</code>, such as "
                            "<code>from:ann subject:\"release plan\"</code>.<br/>"
                            "Narrow with <code>is:unread</code>, <code>is:flagged</code>, <code>tag:name</code>, "
                            "<code>has:attachment</code>, <code>date&gt;=2025-06</code>, <code>date:today</code> or "
                            "<code>size&gt;2M</code>.<br/>"
                            "Combine terms with <code>OR</code>, <code>NOT</code> or a minus sign, and group them with "
                            "parentheses. Quote text with spaces, or an operator to search for the word."
                            "<br/><hr/>"
                            "Experts who have read RFC 3501 can allow raw IMAP search in the options and use the "
                            "<code>:=</code> prefix.");
    m_quickSearchText->setToolTip(m_searchToolTip);
    m_queryPlaceholder = tr("<query>");

    connect(m_quickSearchText, &QLineEdit::returnPressed, this, &MessageListWidget::runTypedSearch);
    connect(m_quickSearchText, &QLineEdit::textChanged, this, &MessageListWidget::slotConditionalSearchReset);
    connect(m_quickSearchText, &QLineEdit::textChanged, this, [this]() {
        // The explanation is about text which is no longer there
        m_searchError->hide();
    });
    connect(m_quickSearchText, &QLineEdit::cursorPositionChanged, this, &MessageListWidget::slotUpdateSearchCursor);
    connect(m_quickSearchText, &LineEdit::escapePressed, this, [this]() {
        // Escape only closed the suggestions
        if (m_suggestionsEscaped) {
            m_suggestionsEscaped = false;
            return;
        }
        // A field filter, as set by Show Related, goes away at once
        if (UiUtils::parseFieldFilter(m_quickSearchText->text()).field != UiUtils::FieldFilter::None) {
            m_quickSearchText->clear();
            m_searchResetTimer->stop();
            slotApplySearch();
        }
        tree->setFocus();
    });

    m_searchOptions = new QAction(UiUtils::loadIcon(QStringLiteral("imap-search-details")), QStringLiteral("*"), this);
    m_searchOptions->setToolTip(tr("Options for the IMAP search..."));
    QMenu *optionsMenu = new QMenu(this);
    m_searchOptions->setMenu(optionsMenu);
    QAction *searchBuilder = optionsMenu->addAction(UiUtils::loadIcon(QStringLiteral("edit-find")),
                                                    i18nc("@action", "Search Builder..."));
    searchBuilder->setObjectName(QStringLiteral("searchBuilder"));
    connect(searchBuilder, &QAction::triggered, this, &MessageListWidget::openSearchBuilder);
    optionsMenu->addSeparator();
    m_searchFuzzy = optionsMenu->addAction(tr("Fuzzy Search"));
    m_searchFuzzy->setCheckable(true);
    optionsMenu->addSeparator();
    QAction *saveSearch = optionsMenu->addAction(i18nc("@action", "Save Current Search..."));
    saveSearch->setObjectName(QStringLiteral("saveSearch"));
    connect(saveSearch, &QAction::triggered, this, &MessageListWidget::saveCurrentSearch);
    m_savedSearches = optionsMenu->addMenu(i18nc("@title:menu", "Saved Searches"));
    m_savedSearches->setObjectName(QStringLiteral("savedSearches"));
    connect(m_savedSearches, &QMenu::aboutToShow, this, &MessageListWidget::rebuildSavedSearchMenu);
    m_mailingLists = optionsMenu->addMenu(i18nc("@title:menu", "Mailing Lists"));
    m_mailingLists->setObjectName(QStringLiteral("mailingLists"));
    m_mailingLists->setToolTipsVisible(true);
    connect(m_mailingLists, &QMenu::aboutToShow, this, &MessageListWidget::rebuildMailingListMenu);
    optionsMenu->addSeparator();
    optionsMenu->setToolTipsVisible(true);
    const KConfigGroup quickSearch = quickSearchGroup();
    m_rememberSearches = optionsMenu->addAction(i18nc("@option:check", "Remember Searches"));
    m_rememberSearches->setObjectName(QStringLiteral("rememberSearches"));
    m_rememberSearches->setCheckable(true);
    m_rememberSearches->setChecked(quickSearch.readEntry("RememberSearches", true));
    m_rememberSearches->setToolTip(i18nc("@info:tooltip", "Keep the searches you run on this computer to offer them "
                                                          "again. Turning this off forgets the searches kept so far."));
    m_suggestWhileTyping = optionsMenu->addAction(i18nc("@option:check", "Suggest While Typing"));
    m_suggestWhileTyping->setObjectName(QStringLiteral("suggestWhileTyping"));
    m_suggestWhileTyping->setCheckable(true);
    m_suggestWhileTyping->setChecked(quickSearch.readEntry("SuggestWhileTyping", true));
    m_suggestWhileTyping->setToolTip(i18nc("@info:tooltip", "Offer recent searches, contacts, tags and folders below the "
                                                            "quick search as you type. The Down key offers them at any "
                                                            "time."));
    m_clearSearchHistory = optionsMenu->addAction(suggestionIcon(QStringLiteral("edit-clear-history")),
                                                  i18nc("@action", "Clear Search History"));
    m_clearSearchHistory->setObjectName(QStringLiteral("clearSearchHistory"));
    if (m_rememberSearches->isChecked())
        m_searchHistory = UiUtils::SearchHistory(searchHistoryGroup().readEntry(searchHistoryKey(), QStringList()));
    else
        saveSearchHistory();
    updateHistoryActions();
    connect(m_rememberSearches, &QAction::toggled, this, [this](bool remember) {
        KConfigGroup group = quickSearchGroup();
        group.writeEntry("RememberSearches", remember);
        group.sync();
        if (!remember) {
            m_searchHistory.clear();
            saveSearchHistory();
        }
        updateHistoryActions();
    });
    connect(m_suggestWhileTyping, &QAction::toggled, this, [this](bool suggest) {
        KConfigGroup group = quickSearchGroup();
        group.writeEntry("SuggestWhileTyping", suggest);
        group.sync();
    });
    connect(m_clearSearchHistory, &QAction::triggered, this, [this]() {
        m_searchHistory.clear();
        saveSearchHistory();
        updateHistoryActions();
    });
    optionsMenu->addSeparator();
    m_searchInSubject = optionsMenu->addAction(tr("Subject"));
    m_searchInSubject->setObjectName(QStringLiteral("searchInSubject"));
    m_searchInSubject->setCheckable(true);
    m_searchInSubject->setChecked(true);
    m_searchInBody = optionsMenu->addAction(tr("Body"));
    m_searchInBody->setObjectName(QStringLiteral("searchInBody"));
    m_searchInBody->setCheckable(true);
    m_searchInSenders = optionsMenu->addAction(tr("Senders"));
    m_searchInSenders->setObjectName(QStringLiteral("searchInSenders"));
    m_searchInSenders->setCheckable(true);
    m_searchInSenders->setChecked(true);
    m_searchInRecipients = optionsMenu->addAction(tr("Recipients"));
    m_searchInRecipients->setObjectName(QStringLiteral("searchInRecipients"));
    m_searchInRecipients->setCheckable(true);

    // The dates apply to every kind of search, so the menu stays enabled for raw queries
    m_dateMenu = optionsMenu->addMenu(i18nc("@title:menu", "Date"));
    m_dateMenu->setObjectName(QStringLiteral("searchDates"));
    m_dateActions = new QActionGroup(this);
    m_dateActions->setExclusive(true);
    const auto addDates = [this](Dates dates, const QString &text) {
        QAction *action = m_dateMenu->addAction(text);
        action->setObjectName(QLatin1String("searchDates-") + UiUtils::SearchScope::configName(dates));
        action->setCheckable(true);
        action->setData(static_cast<int>(dates));
        m_dateActions->addAction(action);
        return action;
    };
    addDates(Dates::AnyTime, i18nc("@option:radio search date range", "Any Time"))->setChecked(true);
    addDates(Dates::Today, i18nc("@option:radio search date range", "Today"));
    addDates(Dates::PastWeek, i18nc("@option:radio search date range", "Past Week"));
    addDates(Dates::PastMonth, i18nc("@option:radio search date range", "Past Month"));
    addDates(Dates::PastYear, i18nc("@option:radio search date range", "Past Year"));
    m_dateMenu->addSeparator();
    addDates(Dates::Custom, i18nc("@option:radio search date range", "Custom Range..."));
    connect(m_dateActions, &QActionGroup::triggered, this, [this](QAction *action) {
        const auto dates = static_cast<Dates>(action->data().toInt());
        if (dates == Dates::Custom)
            chooseCustomDateRange();
        else
            setDateRange({dates, {}, {}});
    });

    // Like the dates, the places apply to every kind of search
    m_placesMenu = optionsMenu->addMenu(i18nc("@title:menu", "Search In"));
    m_placesMenu->setObjectName(QStringLiteral("searchPlaces"));
    m_placesActions = new QActionGroup(this);
    m_placesActions->setExclusive(true);
    const auto addPlaces = [this](Places places, const QString &text) {
        QAction *action = m_placesMenu->addAction(text);
        action->setObjectName(QLatin1String("searchPlaces-") + UiUtils::SearchScope::configName(places));
        action->setCheckable(true);
        action->setData(static_cast<int>(places));
        m_placesActions->addAction(action);
        return action;
    };
    m_currentPlace = addPlaces(Places::CurrentFolder, i18nc("@option:radio search in", "Current Folder"));
    m_currentPlace->setChecked(true);
    addPlaces(Places::SelectedFolders, i18nc("@option:radio search in", "Selected Folders..."));
    m_placesMenu->addSeparator();
    addPlaces(Places::Account, i18nc("@option:radio search in", "Current Account"));
    addPlaces(Places::AllAccounts, i18nc("@option:radio search in", "All Accounts"));
    connect(m_placesActions, &QActionGroup::triggered, this, [this](QAction *action) {
        const auto places = static_cast<Places>(action->data().toInt());
        if (places == Places::SelectedFolders) {
            // Stays as it is unless the user picks some folders
            showDateRange();
            emit foldersChoiceRequested();
        } else {
            setSearchPlaces(places);
        }
    });

    optionsMenu->addSeparator();

    QMenu *complexMenu = new QMenu(tr("Complex IMAP query"), optionsMenu);
    connect(complexMenu, &QMenu::triggered, this, &MessageListWidget::slotComplexSearchInput);
    complexMenu->addAction(tr("Not ..."))->setData(QString(QLatin1String("NOT ") + m_queryPlaceholder));
    complexMenu->addAction(tr("Either... or..."))->setData(QString(QLatin1String("OR ") + m_queryPlaceholder + QLatin1Char(' ') + m_queryPlaceholder));
    complexMenu->addSeparator();
    complexMenu->addAction(tr("From sender"))->setData(QString(QLatin1String("FROM ") + m_queryPlaceholder));
    complexMenu->addAction(tr("To receiver"))->setData(QString(QLatin1String("TO ") + m_queryPlaceholder));
    complexMenu->addSeparator();
    complexMenu->addAction(tr("About subject"))->setData(QString(QLatin1String("SUBJECT " )+ m_queryPlaceholder));
    complexMenu->addAction(tr("Message contains ..."))->setData(QString(QLatin1String("BODY ") + m_queryPlaceholder));
    complexMenu->addSeparator();
    complexMenu->addAction(tr("Before date"))->setData(QLatin1String("BEFORE <d-mmm-yyyy>"));
    complexMenu->addAction(tr("Since date"))->setData(QLatin1String("SINCE <d-mmm-yyyy>"));
    complexMenu->addSeparator();
    complexMenu->addAction(tr("Has been seen"))->setData(QLatin1String("SEEN"));

    m_rawSearch = optionsMenu->addAction(tr("Allow raw IMAP search"));
    m_rawSearch->setObjectName(QStringLiteral("searchRaw"));
    m_rawSearch->setCheckable(true);
    QAction *rawSearchMenu = optionsMenu->addMenu(complexMenu);
    rawSearchMenu->setVisible(false);
    connect(m_rawSearch, &QAction::toggled, rawSearchMenu, &QAction::setVisible);
    connect(m_rawSearch, &QAction::toggled, this, &MessageListWidget::rawSearchSettingChanged);

    m_searchOptions->setMenu(optionsMenu);
    connect(optionsMenu, &QMenu::aboutToShow, this, &MessageListWidget::slotDeActivateSimpleSearch);

    m_quickSearchText->addAction(m_searchOptions, QLineEdit::LeadingPosition);
    connect(m_searchOptions, &QAction::triggered, optionsMenu, [this, optionsMenu](){
        optionsMenu->popup(m_quickSearchText->mapToGlobal(QPoint(0, m_quickSearchText->height())), nullptr);
    });

    m_toolRow = new QHBoxLayout();
    m_toolRow->setSpacing(0);
    m_toolRow->setContentsMargins(0, 0, 0, 0);
    m_toolRow->addWidget(m_quickSearchText, 1);

    m_searchError = new InlineNotificationBar(this);
    m_searchError->setObjectName(QStringLiteral("searchQueryError"));
    m_searchError->setAccessibleName(i18nc("@info:accessible", "Search problem"));

    m_savedSearchEdit = new InlineNotificationBar(this);
    m_savedSearchEdit->setObjectName(QStringLiteral("savedSearchEditBar"));
    m_savedSearchEdit->setAccessibleName(i18nc("@info:accessible", "Saved search being edited"));
    m_savedSearchEdit->setAutoHideTimeout(0);
    // Closing the bar is the same as Cancel; its buttons hide it before they act
    connect(m_savedSearchEdit->closeButton(), &QAbstractButton::clicked, this, &MessageListWidget::cancelSavedSearchEdit);

    m_listStatus = new QLabel(this);
    m_listStatus->setObjectName(QStringLiteral("messageListStatus"));
    m_listStatus->setAccessibleName(i18nc("@label", "Message list status"));
    // Long texts must not make the list wider
    m_listStatus->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    const int margin = style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 2;
    m_listStatus->setContentsMargins(margin, margin / 2, margin, margin / 2);
    m_listStatus->hide();

    m_searchSummary = new QWidget(this);
    m_searchSummary->setObjectName(QStringLiteral("searchSummary"));
    m_searchSummaryText = new QLabel(m_searchSummary);
    m_searchSummaryText->setObjectName(QStringLiteral("searchSummaryText"));
    m_searchSummaryText->setAccessibleName(i18nc("@label", "Search and filters in effect"));
    m_searchSummaryText->setTextFormat(Qt::PlainText);
    // Long queries must not make the list wider; the tooltip has the whole text
    m_searchSummaryText->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_searchSummaryText->setContentsMargins(margin, margin / 2, margin, margin / 2);
    m_clearAll = new QAction(UiUtils::loadIcon(QStringLiteral("edit-clear-all")), i18nc("@action:button", "Clear All"), this);
    m_clearAll->setObjectName(QStringLiteral("clearSearchAndFilters"));
    m_clearAll->setToolTip(i18nc("@info:tooltip", "Clear the search, its date range and the filters, and go back to the "
                                                  "messages shown before"));
    connect(m_clearAll, &QAction::triggered, this, &MessageListWidget::clearAllRequested);
    auto *clearAllButton = new QToolButton(m_searchSummary);
    clearAllButton->setObjectName(QStringLiteral("clearSearchAndFiltersButton"));
    clearAllButton->setDefaultAction(m_clearAll);
    clearAllButton->setAutoRaise(true);
    clearAllButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    clearAllButton->setAccessibleDescription(m_clearAll->toolTip());
    m_facetsMenu = new QMenu(i18nc("@title:menu", "&Narrow Results"), this);
    m_facetsMenu->setObjectName(QStringLiteral("searchFacets"));
    m_facetsMenu->setIcon(UiUtils::loadIcon(QStringLiteral("view-filter")));
    m_facetsMenu->menuAction()->setEnabled(false);
    // The counts follow the results as they load
    connect(m_facetsMenu, &QMenu::aboutToShow, this, &MessageListWidget::rebuildFacetsMenu);
    m_narrowButton = new QToolButton(m_searchSummary);
    m_narrowButton->setObjectName(QStringLiteral("narrowResultsButton"));
    m_narrowButton->setIcon(m_facetsMenu->icon());
    m_narrowButton->setText(i18nc("@action:button narrow the search results by account, folder, sender and more", "Narrow"));
    m_narrowButton->setToolTip(i18nc("@info:tooltip", "Narrow the results by account, folder, sender, date, attachments or tag"));
    m_narrowButton->setAccessibleDescription(m_narrowButton->toolTip());
    m_narrowButton->setAutoRaise(true);
    m_narrowButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_narrowButton->setPopupMode(QToolButton::InstantPopup);
    m_narrowButton->setMenu(m_facetsMenu);
    m_narrowButton->hide();
    rebuildFacetsMenu();
    m_facetChips = new QWidget(m_searchSummary);
    m_facetChips->setObjectName(QStringLiteral("searchFacetChips"));
    m_facetChips->setAccessibleName(i18nc("@label", "Facets narrowing the results"));
    m_facetChipsLayout = new FlowLayout(m_facetChips, margin / 2, 0, 0);
    m_facetChips->hide();
    auto *summaryRow = new QHBoxLayout();
    summaryRow->setSpacing(0);
    summaryRow->setContentsMargins(0, 0, 0, 0);
    summaryRow->addWidget(m_searchSummaryText, 1);
    summaryRow->addWidget(m_narrowButton);
    summaryRow->addWidget(clearAllButton);
    auto *summaryLayout = new QVBoxLayout(m_searchSummary);
    summaryLayout->setSpacing(0);
    summaryLayout->setContentsMargins(0, 0, 0, 0);
    summaryLayout->addLayout(summaryRow);
    summaryLayout->addWidget(m_facetChips);
    m_searchSummary->hide();

    m_statusRow = new QWidget(this);
    m_scope = new QLabel(m_statusRow);
    m_scope->setObjectName(QStringLiteral("messageListScope"));
    m_scope->setTextFormat(Qt::PlainText);
    // Shrinks rather than widening the list when the names are long
    m_scope->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    m_scope->setMinimumWidth(1);
    m_scope->setForegroundRole(QPalette::PlaceholderText);
    m_scope->setContentsMargins(margin, margin / 2, 0, margin / 2);
    m_scope->hide();
    m_listStatus->setParent(m_statusRow);
    m_statusLayout = new QHBoxLayout(m_statusRow);
    m_statusLayout->setSpacing(0);
    m_statusLayout->setContentsMargins(0, 0, 0, 0);
    m_statusLayout->addWidget(m_scope);
    m_statusLayout->addWidget(m_listStatus, 1);
    m_statusRow->hide();

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(m_toolRow);
    layout->addWidget(m_searchError);
    layout->addWidget(m_savedSearchEdit);
    layout->addWidget(m_searchSummary);
    layout->addWidget(tree);
    layout->addWidget(m_statusRow);

    m_suggestions = new QStandardItemModel(this);
    m_completer = new SuggestionCompleter(m_suggestions, this);
    m_completer->setObjectName(QStringLiteral("searchSuggestions"));
    m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    m_completer->setCompletionRole(RoleSuggestionText);
    m_completer->setMaxVisibleItems(12);
    m_quickSearchText->setCompleter(m_completer);
    m_completer->popup()->setObjectName(QStringLiteral("searchSuggestionsPopup"));
    m_completer->popup()->setAccessibleName(i18nc("@info:accessible", "Search suggestions"));
    // After the completer, so that this filter sees the keys first
    m_completer->popup()->installEventFilter(this);
    m_quickSearchText->installEventFilter(this);
    connect(m_completer, qOverload<const QModelIndex &>(&QCompleter::activated), this, &MessageListWidget::chooseSuggestion);
    connect(m_quickSearchText, &QLineEdit::textEdited, this, &MessageListWidget::updateSuggestions);

    m_searchResetTimer = new QTimer(this);
    m_searchResetTimer->setSingleShot(true);
    connect(m_searchResetTimer, &QTimer::timeout, this, &MessageListWidget::slotApplySearch);

    m_dayChangeTimer = new QTimer(this);
    m_dayChangeTimer->setObjectName(QStringLiteral("searchDayChange"));
    m_dayChangeTimer->setInterval(std::chrono::minutes(1));
    connect(m_dayChangeTimer, &QTimer::timeout, this, [this]() {
        if (QDate::currentDate() != m_searchDay)
            slotApplySearch();
    });

    slotAutoEnableDisableSearch();
}

void MessageListWidget::saveCurrentSearch()
{
    UiUtils::SearchQuery::Error problem;
    if (searchConditions(&problem, nullptr).isEmpty()) {
        if (!problem.isEmpty()) {
            showSearchError(problem);
            return;
        }
        Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Save Search"),
                                     i18nc("@info", "Enter a search and select at least one search field, or choose a date range."));
        return;
    }
    bool accepted = false;
    const QString name = QInputDialog::getText(this, i18nc("@title:window", "Save Search"),
                                               i18nc("@label:textbox", "Search name:"), QLineEdit::Normal,
                                               QString(), &accepted).trimmed();
    if (!accepted)
        return;
    if (name.isEmpty()) {
        Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Save Search"),
                                     i18nc("@info", "The search name cannot be empty."));
        return;
    }

    const KConfigGroup searches = savedSearchGroup();
    if (UiUtils::SavedSearches::isNameTaken(savedSearchesRoot(), m_accountId, name)) {
        Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Save Search"),
                                     i18nc("@info", "A saved search with this name already exists."));
        return;
    }
    UiUtils::SavedSearch search = currentSearch();
    search.owner = m_accountId;
    search.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    search.name = name;
    KConfigGroup group = searches.group(search.id);
    UiUtils::SavedSearches::write(group, search);
    if (!group.sync())
        Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Save Search"),
                                     i18nc("@info", "The search could not be saved."));
    // The list now shows what the saved search finds
    setActiveSavedSearch(search);
    emit savedSearchesChanged();
}

void MessageListWidget::rebuildSavedSearchMenu()
{
    qDeleteAll(m_savedSearches->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly));
    m_savedSearches->clear();
    // In the order of the virtual folders
    const auto searches = UiUtils::SavedSearches::load(savedSearchesRoot(), {m_accountId});
    for (const UiUtils::SavedSearch &search : searches)
        addSavedSearch(search);
}

void MessageListWidget::rebuildMailingListMenu()
{
    m_mailingLists->clear();
    const UiUtils::ParsedFieldFilter filter = UiUtils::parseFieldFilter(m_quickSearchText->text());
    const QString shown = filter.field == UiUtils::FieldFilter::MailingList ? filter.value.toLower() : QString();
    UiUtils::MailingList shownList;
    const auto lists = UiUtils::MailingLists::listsIn(tree->model());
    for (const UiUtils::MailingLists::Found &found : lists) {
        const UiUtils::MailingList list = found.list;
        QString text = list.name.isEmpty()
            ? i18ncp("@action:inmenu mailing list and its loaded messages", "%2 (%1 message)", "%2 (%1 messages)",
                     found.messages, list.searchValue())
            : i18ncp("@action:inmenu mailing list, its ID and its loaded messages", "%2 <%3> (%1 message)",
                     "%2 <%3> (%1 messages)", found.messages, list.name, list.searchValue());
        QAction *action = m_mailingLists->addAction(text.replace(QLatin1Char('&'), QLatin1String("&&")));
        action->setCheckable(true);
        action->setChecked(list.searchValue() == shown);
        if (action->isChecked())
            shownList = list;
        action->setToolTip(i18nc("@info:tooltip", "Show the messages of this list"));
        connect(action, &QAction::triggered, this, [this, list]() {
            showFieldFilter(UiUtils::fieldFilterText(UiUtils::FieldFilter::MailingList, list.searchValue()));
        });
    }
    if (lists.isEmpty()) {
        QAction *none = m_mailingLists->addAction(i18nc("@item:inmenu", "No Mailing Lists in the Loaded Messages"));
        none->setEnabled(false);
    }
    if (!shownList.isValid() && !shown.isEmpty())
        shownList.id = shown;
    m_mailingLists->addSeparator();
    QAction *pin = m_mailingLists->addAction(suggestionIcon(QStringLiteral("window-pin")),
                                             i18nc("@action:inmenu", "Pin Shown List as Saved Search"));
    pin->setObjectName(QStringLiteral("pinMailingList"));
    pin->setEnabled(shownList.isValid());
    pin->setToolTip(i18nc("@info:tooltip", "Save a search for the messages of the shown list in every folder of every "
                                           "account, and show it with the saved searches"));
    connect(pin, &QAction::triggered, this, [this, shownList]() { pinMailingList(shownList); });
}

bool MessageListWidget::pinMailingList(const UiUtils::MailingList &list)
{
    if (!list.isValid())
        return false;
    const QString query = UiUtils::fieldFilterText(UiUtils::FieldFilter::MailingList, list.searchValue());
    const KConfigGroup root = savedSearchesRoot();
    const auto existing = UiUtils::SavedSearches::load(root, {m_accountId});
    for (const UiUtils::SavedSearch &search : existing) {
        if (search.query == query && search.places == Places::AllAccounts
            && search.dates == UiUtils::SearchScope::DateRange())
            return runSavedSearch(search.owner, search.id);
    }

    UiUtils::SavedSearch search = currentSearch();
    search.owner = m_accountId;
    search.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    search.query = query;
    search.fuzzy = false;
    search.raw = false;
    search.dates = UiUtils::SearchScope::DateRange();
    search.places = Places::AllAccounts;
    search.folders.clear();
    search.name = list.displayName();
    // Two lists may share a description; their IDs tell them apart
    if (UiUtils::SavedSearches::isNameTaken(root, m_accountId, search.name))
        search.name = i18nc("@item saved search for a mailing list: its name and ID", "%1 (%2)", list.displayName(),
                            list.searchValue());
    if (UiUtils::SavedSearches::isNameTaken(root, m_accountId, search.name)) {
        Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Pin Mailing List"),
                                     i18nc("@info", "A saved search with this name already exists."));
        return false;
    }
    KConfigGroup group = savedSearchGroup().group(search.id);
    UiUtils::SavedSearches::write(group, search);
    if (!group.sync()) {
        Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Pin Mailing List"),
                                     i18nc("@info", "The search could not be saved."));
    }
    applySavedSearch(search);
    emit savedSearchesChanged();
    return true;
}

void MessageListWidget::setAccountId(const QString &accountId)
{
    if (accountId != m_accountId) {
        // Another account's search would be saved over the edited one
        endSavedSearchEdit();
        forgetActiveSavedSearch();
    }
    m_accountId = accountId;
}

QToolButton *MessageListWidget::addToolButton(QAction *action)
{
    auto *button = new QToolButton(this);
    button->setDefaultAction(action);
    button->setAutoRaise(true);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setAccessibleName(KLocalizedString::removeAcceleratorMarker(action->text()));
    button->setAccessibleDescription(action->toolTip());
    m_toolRow->addWidget(button);
    return button;
}

QToolButton *MessageListWidget::addToolMenuButton(QMenu *menu, const QIcon &icon, const QString &text)
{
    auto *button = new QToolButton(this);
    button->setMenu(menu);
    button->setPopupMode(QToolButton::InstantPopup);
    button->setAutoRaise(true);
    button->setIcon(icon);
    button->setToolTip(text);
    button->setAccessibleName(text);
    m_toolRow->addWidget(button);
    return button;
}

QToolButton *MessageListWidget::addStatusButton(QAction *action)
{
    auto *button = new QToolButton(m_statusRow);
    button->setDefaultAction(action);
    button->setAutoRaise(true);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setAccessibleName(KLocalizedString::removeAcceleratorMarker(action->text()));
    button->setAccessibleDescription(action->toolTip());
    button->setVisible(action->isVisible());
    connect(action, &QAction::changed, button, [this, button, action]() {
        if (button->isHidden() == action->isVisible()) {
            button->setVisible(action->isVisible());
            updateStatusRow();
        }
    });
    m_statusLayout->addWidget(button);
    m_statusButtons << button;
    updateStatusRow();
    return button;
}

void MessageListWidget::updateStatusRow()
{
    const bool buttons = std::any_of(m_statusButtons.cbegin(), m_statusButtons.cend(),
                                     [](const QToolButton *button) { return !button->isHidden(); });
    m_statusRow->setVisible(!m_scope->isHidden() || !m_listStatus->isHidden() || buttons);
}

void MessageListWidget::setListStatus(const QString &text, const QString &toolTip)
{
    m_listStatus->setText(text);
    m_listStatus->setToolTip(toolTip);
    m_listStatus->setVisible(!text.isEmpty());
    updateStatusRow();
}

void MessageListWidget::setScope(const QString &account, const QString &folder, const QString &fullFolderName)
{
    if (folder.isEmpty()) {
        m_scope->clear();
        m_scope->setToolTip(QString());
        m_scope->setAccessibleName(QString());
        m_scope->hide();
    } else {
        m_scope->setText(i18nc("@info:status account and folder shown in the message list", "%1 \u203A %2", account, folder));
        m_scope->setToolTip(i18nc("@info:tooltip", "Account: %1\nFolder: %2", account, fullFolderName));
        m_scope->setAccessibleName(i18nc("@info:accessible", "Showing folder %1 of account %2", folder, account));
        m_scope->show();
    }
    updateStatusRow();
}

void MessageListWidget::setCustomScope(const QString &text, const QString &toolTip)
{
    m_scope->setText(text);
    m_scope->setToolTip(toolTip);
    m_scope->setAccessibleName(text);
    m_scope->setVisible(!text.isEmpty());
    updateStatusRow();
}

void MessageListWidget::setActiveFilters(const QStringList &names)
{
    if (m_activeFilters == names)
        return;
    m_activeFilters = names;
    updateSearchSummary();
}

void MessageListWidget::updateSearchSummary()
{
    QStringList parts = m_searchDescription;
    if (!m_activeFilters.isEmpty()) {
        parts << i18nc("@info:status %1 is a list of quick filters such as Unread, Flagged", "Filters: %1",
                       m_activeFilters.join(i18nc("@info:status separator of quick filter names", ", ")));
    }
    const QString text = parts.join(QStringLiteral(" \u00B7 "));
    m_searchSummaryText->setText(text);
    m_searchSummaryText->setToolTip(text);
    const bool narrowing = !m_searchDescription.isEmpty() || !m_facets.isEmpty();
    m_narrowButton->setVisible(narrowing);
    m_facetsMenu->menuAction()->setEnabled(narrowing);
    const bool shown = !parts.isEmpty() || !m_facets.isEmpty();
    m_clearAll->setEnabled(shown);
    m_searchSummary->setVisible(shown);
}

void MessageListWidget::setFacetSource(const FacetSource &source)
{
    m_facetSource = source;
}

void MessageListWidget::setFacetContext(const UiUtils::SearchFacets::Context &context)
{
    const UiUtils::SearchFacets::Context previous = m_facets.context();
    if (previous.accountId == context.accountId && previous.accountName == context.accountName && previous.today == context.today)
        return;
    m_facets.setContext(context);
    // The account of the messages which do not name their own may now be another one
    if (!m_facets.isEmpty())
        emit facetsChanged();
}

UiUtils::SearchFacets MessageListWidget::facets() const
{
    return m_facets;
}

void MessageListWidget::clearFacets()
{
    if (m_facets.isEmpty())
        return;
    UiUtils::SearchFacets facets = m_facets;
    facets.clear();
    setFacets(facets);
}

QMenu *MessageListWidget::facetsMenu() const
{
    return m_facetsMenu;
}

void MessageListWidget::setFacets(const UiUtils::SearchFacets &facets)
{
    if (facets == m_facets)
        return;
    m_facets = facets;
    updateFacetChips();
    updateSearchSummary();
    emit facetsChanged();
}

void MessageListWidget::rebuildFacetsMenu()
{
    using UiUtils::SearchFacets;
    // Clearing the menu leaves its submenus behind
    qDeleteAll(m_facetsMenu->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly));
    m_facetsMenu->clear();
    const QModelIndexList messages = m_facetSource ? m_facetSource() : QModelIndexList();
    for (const SearchFacets::Kind kind : SearchFacets::kinds()) {
        QMenu *menu = m_facetsMenu->addMenu(facetMenuTitle(kind));
        menu->setObjectName(facetObjectName(kind));
        const QList<SearchFacets::Count> counts = m_facets.counts(messages, kind);
        if (counts.isEmpty()) {
            QAction *none = menu->addAction(i18nc("@item:inmenu", "No Values Among the Results"));
            none->setEnabled(false);
            continue;
        }
        int listed = 0;
        for (const SearchFacets::Count &count : counts) {
            const bool chosen = m_facets.contains(count.value);
            if (listed >= FacetMenuValues && !chosen)
                continue;
            ++listed;
            QString label = count.value.label;
            label.replace(QLatin1Char('&'), QStringLiteral("&&"));
            QAction *action = menu->addAction(i18nc("@item:inmenu %1 is a facet value such as a sender, %2 how many results "
                                                    "have it", "%1 (%2)", label, count.count));
            action->setCheckable(true);
            action->setChecked(chosen);
            const SearchFacets::Value value = count.value;
            connect(action, &QAction::triggered, this, [this, value]() {
                UiUtils::SearchFacets facets = m_facets;
                facets.toggle(value);
                setFacets(facets);
            });
        }
        if (listed < counts.size()) {
            menu->addSeparator();
            QAction *more = menu->addAction(i18ncp("@item:inmenu", "%1 more value not listed", "%1 more values not listed",
                                                   counts.size() - listed));
            more->setObjectName(QStringLiteral("searchFacetMore"));
            more->setEnabled(false);
        }
    }
    m_facetsMenu->addSeparator();
    QAction *clear = m_facetsMenu->addAction(UiUtils::loadIcon(QStringLiteral("edit-clear")), i18nc("@action:inmenu", "&Clear Facets"));
    clear->setObjectName(QStringLiteral("clearSearchFacets"));
    clear->setEnabled(!m_facets.isEmpty());
    connect(clear, &QAction::triggered, this, &MessageListWidget::clearFacets);
}

void MessageListWidget::updateFacetChips()
{
    using UiUtils::SearchFacets;
    while (QLayoutItem *item = m_facetChipsLayout->takeAt(0)) {
        if (QWidget *chip = item->widget()) {
            // A chip may be removing itself, so it is deleted later, but it is no longer one of the chips
            chip->hide();
            chip->setParent(nullptr);
            chip->deleteLater();
        }
        delete item;
    }
    const QList<SearchFacets::Value> chosen = m_facets.chosen();
    for (int i = 0; i < chosen.size(); ++i) {
        const SearchFacets::Value value = chosen[i];
        auto *chip = new QToolButton(m_facetChips);
        chip->setObjectName(QStringLiteral("searchFacetChip"));
        QString label = i18nc("@action:button %1 is a kind of facet such as Sender, %2 its value", "%1: %2",
                              SearchFacets::kindName(value.kind), value.label);
        const QString remove = i18nc("@info:tooltip %1 is a facet such as Sender: Alice", "Remove %1", label);
        label.replace(QLatin1Char('&'), QStringLiteral("&&"));
        chip->setText(label);
        chip->setIcon(UiUtils::loadIcon(QStringLiteral("tab-close")));
        chip->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        chip->setAutoRaise(true);
        chip->setToolTip(remove);
        chip->setAccessibleName(remove);
        connect(chip, &QToolButton::clicked, this, [this, value, i]() {
            const bool hadFocus = m_facetChips->isAncestorOf(QApplication::focusWidget());
            UiUtils::SearchFacets facets = m_facets;
            facets.remove(value);
            setFacets(facets);
            if (!hadFocus)
                return;
            // Focus stays among the chips, on the one which took the place of the removed one
            const int count = m_facetChipsLayout->count();
            if (count)
                m_facetChipsLayout->itemAt(std::min(i, count - 1))->widget()->setFocus(Qt::OtherFocusReason);
            else
                m_narrowButton->setFocus(Qt::OtherFocusReason);
        });
        m_facetChipsLayout->addWidget(chip);
        // Shown at once, to take the focus of a removed chip
        chip->show();
    }
    m_facetChips->setVisible(!chosen.isEmpty());
}

void MessageListWidget::setSearchAvailable(bool available)
{
    if (m_searchAvailable == available)
        return;
    m_searchAvailable = available;
    // A search which still filters a single folder would look like it applies to the shown list
    if (!available)
        clearSearch();
    slotAutoEnableDisableSearch();
}

void MessageListWidget::clearSearch()
{
    clearFacets();
    if (m_quickSearchText->text().isEmpty() && !UiUtils::SearchScope::isLimited(m_dateRange))
        return;
    m_quickSearchText->clear();
    m_dateRange = {};
    showDateRange();
    m_searchResetTimer->stop();
    slotApplySearch();
    slotAutoEnableDisableSearch();
}

KConfigGroup MessageListWidget::savedSearchesRoot()
{
    return KConfigGroup(KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")), QStringLiteral("SavedSearches"));
}

KConfigGroup MessageListWidget::savedSearchGroup() const
{
    return savedSearchesRoot().group(m_accountId);
}

void MessageListWidget::addSavedSearch(const UiUtils::SavedSearch &search)
{
    const QString id = search.id;
    QString menuName = search.name;
    menuName.replace(QLatin1Char('&'), QStringLiteral("&&"));
    QMenu *entry = m_savedSearches->addMenu(menuName);
    QAction *run = entry->addAction(i18nc("@action", "Run Search"));
    run->setObjectName(QStringLiteral("runSavedSearch"));
    connect(run, &QAction::triggered, this, [this, id]() { runSavedSearch(m_accountId, id); });
    QAction *edit = entry->addAction(i18nc("@action", "Edit Search"));
    edit->setObjectName(QStringLiteral("editSavedSearch"));
    connect(edit, &QAction::triggered, this, [this, id]() { editSavedSearch(m_accountId, id); });
    QAction *rename = entry->addAction(i18nc("@action", "Rename Search..."));
    rename->setObjectName(QStringLiteral("renameSavedSearch"));
    connect(rename, &QAction::triggered, this, [this, id]() { renameSavedSearch(m_accountId, id); });
    QAction *remove = entry->addAction(i18nc("@action", "Delete Search..."));
    remove->setObjectName(QStringLiteral("deleteSavedSearch"));
    connect(remove, &QAction::triggered, this, [this, id]() { deleteSavedSearch(m_accountId, id); });
}

UiUtils::SavedSearch MessageListWidget::currentSearch() const
{
    UiUtils::SavedSearch search;
    search.query = m_quickSearchText->text();
    search.subject = m_searchInSubject->isChecked();
    search.body = m_searchInBody->isChecked();
    search.senders = m_searchInSenders->isChecked();
    search.recipients = m_searchInRecipients->isChecked();
    const bool rawQuery = m_rawSearch->isChecked() && m_quickSearchText->text().startsWith(QLatin1String(":="));
    search.fuzzy = !rawQuery && m_supportsFuzzySearch && m_searchFuzzy->isChecked();
    search.raw = m_rawSearch->isChecked();
    search.dates = m_dateRange;
    search.places = m_places;
    search.folders = m_places == Places::SelectedFolders ? m_selectedFolders : QStringList();
    return search;
}

bool MessageListWidget::editSavedSearch(const QString &owner, const QString &id)
{
    endSavedSearchEdit();
    if (!runSavedSearch(owner, id))
        return false;
    m_editedSavedSearch = m_activeSavedSearch;
    showSavedSearchEdit();
    m_quickSearchText->setFocus(Qt::OtherFocusReason);
    return true;
}

void MessageListWidget::showSavedSearchEdit()
{
    if (m_editedSavedSearch.id.isEmpty())
        return;
    m_savedSearchEdit->showInfo(
        i18nc("@info %1 is the name of a saved search",
              "Editing the saved search \"%1\". Change the search, its dates or where it looks; the list shows what it "
              "finds until you save or cancel.",
              m_editedSavedSearch.name),
        {
            {i18nc("@action:button", "Save"), QStringLiteral("document-save"), QStringLiteral("saveSavedSearchEdit"),
             [this]() { saveSavedSearchEdit(); }},
            {i18nc("@action:button", "Cancel"), QStringLiteral("dialog-cancel"), QStringLiteral("cancelSavedSearchEdit"),
             [this]() { cancelSavedSearchEdit(); }},
        });
}

QString MessageListWidget::editedSavedSearchOwner() const
{
    return m_editedSavedSearch.owner;
}

QString MessageListWidget::editedSavedSearchId() const
{
    return m_editedSavedSearch.id;
}

void MessageListWidget::endSavedSearchEdit()
{
    if (m_editedSavedSearch.id.isEmpty())
        return;
    m_editedSavedSearch = UiUtils::SavedSearch();
    m_savedSearchEdit->dismiss();
}

void MessageListWidget::saveSavedSearchEdit()
{
    if (m_editedSavedSearch.id.isEmpty())
        return;
    const UiUtils::SavedSearch edited = m_editedSavedSearch;
    UiUtils::SavedSearch search = currentSearch();
    search.owner = edited.owner;
    search.id = edited.id;
    search.name = edited.name;
    UiUtils::SearchQuery::Error problem;
    if (searchConditions(&problem, nullptr).isEmpty() || !search.isValid()) {
        if (!problem.isEmpty())
            showSearchError(problem);
        else
            Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Save Search"),
                                         i18nc("@info", "Enter a search and select at least one search field, or choose a date range."));
        // Still editing
        showSavedSearchEdit();
        return;
    }
    KConfigGroup account = savedSearchesRoot().group(search.owner);
    if (!account.hasGroup(search.id)) {
        endSavedSearchEdit();
        Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Save Search"),
                                     i18nc("@info", "The saved search was deleted meanwhile, so the changes could not be saved."));
        emit savedSearchesChanged();
        return;
    }
    KConfigGroup group = account.group(search.id);
    UiUtils::SavedSearches::write(group, search);
    if (!group.sync())
        Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Save Search"),
                                     i18nc("@info", "The search could not be saved."));
    endSavedSearchEdit();
    // What the list shows is now what the saved search finds
    applySavedSearch(search);
    emit savedSearchesChanged();
}

void MessageListWidget::cancelSavedSearchEdit()
{
    if (m_editedSavedSearch.id.isEmpty())
        return;
    const UiUtils::SavedSearch edited = m_editedSavedSearch;
    endSavedSearchEdit();
    // Back to what the saved search finds, where it can still run
    if (!edited.dependsOnOwner() || edited.owner == m_accountId)
        runSavedSearch(edited.owner, edited.id);
}

bool MessageListWidget::renameSavedSearch(const QString &owner, const QString &id)
{
    KConfigGroup root = savedSearchesRoot();
    if (owner.isEmpty() || id.isEmpty() || !root.group(owner).hasGroup(id))
        return false;
    KConfigGroup group = root.group(owner).group(id);
    const QString current = group.readEntry("Name", QString());
    bool accepted = false;
    const QString name = QInputDialog::getText(this, i18nc("@title:window", "Rename Search"),
                                               i18nc("@label:textbox", "Search name:"), QLineEdit::Normal, current,
                                               &accepted).trimmed();
    if (!accepted || name == current)
        return false;
    if (name.isEmpty()) {
        Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Rename Search"),
                                     i18nc("@info", "The search name cannot be empty."));
        return false;
    }
    if (UiUtils::SavedSearches::isNameTaken(root, owner, name, id)) {
        Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Rename Search"),
                                     i18nc("@info", "A saved search with this name already exists."));
        return false;
    }
    group.writeEntry("Name", name);
    if (!group.sync())
        Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Rename Search"),
                                     i18nc("@info", "The search could not be renamed."));
    if (m_activeSavedSearch.owner == owner && m_activeSavedSearch.id == id)
        m_activeSavedSearch.name = name;
    if (m_editedSavedSearch.owner == owner && m_editedSavedSearch.id == id) {
        m_editedSavedSearch.name = name;
        showSavedSearchEdit();
    }
    emit savedSearchesChanged();
    return true;
}

bool MessageListWidget::runSavedSearch(const QString &owner, const QString &id)
{
    const KConfigGroup searches(KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")), QStringLiteral("SavedSearches"));
    const KConfigGroup account = searches.group(owner);
    if (owner.isEmpty() || id.isEmpty() || !account.hasGroup(id))
        return false;
    const UiUtils::SavedSearch search = UiUtils::SavedSearches::read(account.group(id), owner);
    if (!search.isValid())
        return false;
    if (search.fuzzy && !m_supportsFuzzySearch) {
        Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Run Search"),
                                     i18nc("@info", "This search requires fuzzy search support from the server."));
        return false;
    }
    applySavedSearch(search);
    return true;
}

void MessageListWidget::applySavedSearch(const UiUtils::SavedSearch &search)
{
    m_applyingSavedSearch = true;
    m_rawSearch->setChecked(search.raw);
    m_searchInSubject->setChecked(search.subject);
    m_searchInBody->setChecked(search.body);
    m_searchInSenders->setChecked(search.senders);
    m_searchInRecipients->setChecked(search.recipients);
    m_searchFuzzy->setChecked(search.fuzzy);
    m_quickSearchText->setText(search.query);
    m_dateRange = search.dates;
    m_places = search.places;
    if (m_places == Places::SelectedFolders)
        m_selectedFolders = search.folders;
    showDateRange();
    m_searchResetTimer->stop();
    setActiveSavedSearch(search);
    slotApplySearch();
    m_applyingSavedSearch = false;
}

bool MessageListWidget::matchesSavedSearch(const UiUtils::SavedSearch &search) const
{
    return m_quickSearchText->text() == search.query && m_rawSearch->isChecked() == search.raw
        && m_searchInSubject->isChecked() == search.subject && m_searchInBody->isChecked() == search.body
        && m_searchInSenders->isChecked() == search.senders && m_searchInRecipients->isChecked() == search.recipients
        && m_searchFuzzy->isChecked() == search.fuzzy && m_dateRange == search.dates && m_places == search.places
        && (m_places != Places::SelectedFolders || m_selectedFolders == search.folders);
}

void MessageListWidget::setActiveSavedSearch(const UiUtils::SavedSearch &search)
{
    if (search.owner == m_activeSavedSearch.owner && search.id == m_activeSavedSearch.id) {
        m_activeSavedSearch = search;
        return;
    }
    m_activeSavedSearch = search;
    emit activeSavedSearchChanged(search.owner, search.id);
}

QString MessageListWidget::activeSavedSearchOwner() const
{
    return m_activeSavedSearch.owner;
}

QString MessageListWidget::activeSavedSearchId() const
{
    return m_activeSavedSearch.id;
}

QString MessageListWidget::searchText() const
{
    return m_quickSearchText->text();
}

void MessageListWidget::forgetActiveSavedSearch()
{
    setActiveSavedSearch(UiUtils::SavedSearch());
}

bool MessageListWidget::deleteSavedSearch(const QString &owner, const QString &id)
{
    KConfigGroup root = savedSearchesRoot();
    if (owner.isEmpty() || id.isEmpty() || !root.group(owner).hasGroup(id))
        return false;
    KConfigGroup group = root.group(owner).group(id);
    const QString name = group.readEntry("Name", QString());
    if (QMessageBox::question(this, i18nc("@title:window", "Delete Search"),
                              Qt::convertFromPlainText(i18nc("@info", "Delete the saved search \"%1\"?", name))) != QMessageBox::Yes)
        return false;
    group.deleteGroup();
    QStringList order = UiUtils::SavedSearches::readOrder(root);
    if (order.removeAll(UiUtils::SavedSearches::countsKey(owner, id)) > 0)
        root.writeEntry("Order", order);
    if (!root.sync())
        Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Delete Search"),
                                     i18nc("@info", "The search could not be deleted."));
    if (m_editedSavedSearch.owner == owner && m_editedSavedSearch.id == id)
        endSavedSearchEdit();
    if (m_activeSavedSearch.owner == owner && m_activeSavedSearch.id == id)
        forgetActiveSavedSearch();
    emit savedSearchesChanged();
    return true;
}

UiUtils::SearchScope::DateRange MessageListWidget::dateRange() const
{
    return m_dateRange;
}

void MessageListWidget::setDateRange(const DateRange &range)
{
    m_dateRange = UiUtils::SearchScope::isLimited(range) ? range : DateRange();
    if (m_dateRange.dates != Dates::Custom) {
        m_dateRange.from = QDate();
        m_dateRange.to = QDate();
    }
    showDateRange();
    slotAutoEnableDisableSearch();
    m_searchResetTimer->stop();
    slotApplySearch();
}

UiUtils::SearchScope::Places MessageListWidget::searchPlaces() const
{
    return m_places;
}

void MessageListWidget::setSearchPlaces(const Places places)
{
    m_places = places;
    showDateRange();
    m_searchResetTimer->stop();
    slotApplySearch();
}

QStringList MessageListWidget::selectedFolders() const
{
    return m_selectedFolders;
}

void MessageListWidget::setSelectedFolders(const QStringList &keys)
{
    m_selectedFolders = keys;
    setSearchPlaces(Places::SelectedFolders);
}

void MessageListWidget::setCurrentPlaceText(const QString &text)
{
    m_currentPlaceText = text;
    m_currentPlace->setText(text.isEmpty() ? i18nc("@option:radio search in", "Current Folder") : text);
    showDateRange();
}

QString MessageListWidget::placesText() const
{
    switch (m_places) {
    case Places::CurrentFolder:
        return m_currentPlaceText.isEmpty() ? i18nc("@item search in", "Current Folder") : m_currentPlaceText;
    case Places::SelectedFolders:
        return i18ncp("@item search in", "%1 Selected Folder", "%1 Selected Folders", m_selectedFolders.size());
    case Places::Account:
        return i18nc("@item search in", "Current Account");
    case Places::AllAccounts:
        return i18nc("@item search in", "All Accounts");
    }
    return QString();
}

void MessageListWidget::chooseCustomDateRange()
{
    const QDate today = QDate::currentDate();
    const UiUtils::SearchScope::Days current = UiUtils::SearchScope::resolve(m_dateRange, today);
    const bool limited = UiUtils::SearchScope::isLimited(m_dateRange);

    QDialog dialog(this);
    dialog.setWindowTitle(i18nc("@title:window", "Custom Date Range"));
    auto *layout = new QFormLayout(&dialog);
    const auto addDay = [&dialog, layout](const QString &name, const QString &label, const QDate &day, bool enabled) {
        auto *check = new QCheckBox(label, &dialog);
        check->setObjectName(name + QLatin1String("Enabled"));
        check->setChecked(enabled);
        auto *edit = new QDateEdit(day, &dialog);
        edit->setObjectName(name);
        edit->setCalendarPopup(true);
        edit->setEnabled(enabled);
        connect(check, &QCheckBox::toggled, edit, &QWidget::setEnabled);
        layout->addRow(check, edit);
        return std::make_pair(check, edit);
    };
    // Without a range yet, offer the past month, which the user can then narrow
    const auto [fromCheck, fromEdit] = addDay(QStringLiteral("dateFrom"), i18nc("@option:check first day of a date range", "From:"),
                                              current.first.isValid() ? current.first : today.addMonths(-1),
                                              current.first.isValid() || !limited);
    const auto [toCheck, toEdit] = addDay(QStringLiteral("dateTo"), i18nc("@option:check last day of a date range", "To:"),
                                          current.last.isValid() ? current.last : today, current.last.isValid() || !limited);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addRow(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        // The menu has already checked Custom Range, so put back the range in effect
        showDateRange();
        return;
    }
    setDateRange({Dates::Custom, fromCheck->isChecked() ? fromEdit->date() : QDate(), toCheck->isChecked() ? toEdit->date() : QDate()});
}

void MessageListWidget::showDateRange()
{
    const auto actions = m_dateActions->actions();
    for (QAction *action : actions) {
        if (static_cast<Dates>(action->data().toInt()) == m_dateRange.dates)
            action->setChecked(true);
    }
    const auto placesActions = m_placesActions->actions();
    for (QAction *action : placesActions) {
        if (static_cast<Places>(action->data().toInt()) == m_places)
            action->setChecked(true);
    }
    const bool limited = UiUtils::SearchScope::isLimited(m_dateRange);
    const bool elsewhere = m_places != Places::CurrentFolder;
    const QString dates = dateRangeText(m_dateRange);
    const QString places = placesText();
    m_dateMenu->setTitle(limited ? i18nc("@title:menu %1 is a date range such as Past Week", "Date: %1", dates)
                                 : i18nc("@title:menu", "Date"));
    m_placesMenu->setTitle(elsewhere ? i18nc("@title:menu %1 is where to search, such as All Accounts", "Search In: %1", places)
                                     : i18nc("@title:menu", "Search In"));
    QString placeholder = tr("Quick Search");
    if (limited && elsewhere) {
        placeholder = i18nc("@info:placeholder %1 is where to search, such as All Accounts, %2 a date range such as Past Week",
                            "Quick Search (%1, %2)", places, dates);
    } else if (elsewhere) {
        placeholder = i18nc("@info:placeholder %1 is where to search, such as All Accounts", "Quick Search (%1)", places);
    } else if (limited) {
        placeholder = i18nc("@info:placeholder %1 is a date range such as Past Week", "Quick Search (%1)", dates);
    }
    m_quickSearchText->setPlaceholderText(placeholder);
    QString toolTip = m_searchToolTip;
    if (elsewhere) {
        toolTip += QLatin1String("<br/><hr/>")
            + i18nc("@info:tooltip %1 is where to search, such as All Accounts", "Searches in: %1", places.toHtmlEscaped());
    }
    if (limited) {
        toolTip += QLatin1String("<br/><hr/>")
            + i18nc("@info:tooltip %1 is a date range such as Past Week", "Only messages sent in this date range are shown: %1",
                    dates.toHtmlEscaped());
    }
    m_quickSearchText->setToolTip(toolTip);
}

QLineEdit *MessageListWidget::searchField() const
{
    return m_quickSearchText;
}

void MessageListWidget::focusSearch()
{
    if (!m_quickSearchText->isEnabled() || m_quickSearchText->hasFocus())
        return;
    m_quickSearchText->setFocus(Qt::ShortcutFocusReason);
}

void MessageListWidget::openSearchBuilder()
{
    if (!m_quickSearchText->isEnabled())
        return;
    auto *dialog = new SearchBuilderDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    const QString text = m_quickSearchText->text();
    if (m_rawSearch->isChecked() && text.startsWith(QLatin1String(":="))) {
        dialog->explainQuery(i18nc("@info", "Raw IMAP queries cannot be opened in the builder. The query you build "
                                            "replaces the raw query."));
    } else {
        dialog->setQuery(text);
    }
    connect(dialog, &QDialog::accepted, this, [this, dialog]() {
        m_quickSearchText->setText(dialog->query());
        m_searchResetTimer->stop();
        if (applySearch())
            rememberSearch(m_quickSearchText->text());
    });
    dialog->open();
}

void MessageListWidget::focusRawSearch()
{
    if (!m_quickSearchText->isEnabled() || m_quickSearchText->hasFocus() || !m_rawSearch->isChecked())
        return;
    m_quickSearchText->setFocus(Qt::ShortcutFocusReason);
    m_quickSearchText->setText(QStringLiteral(":="));
    m_quickSearchText->deselect();
    m_quickSearchText->setCursorPosition(m_quickSearchText->text().length());
}

void MessageListWidget::showFieldFilter(const QString &text)
{
    m_quickSearchText->setText(text);
    m_searchResetTimer->stop();
    slotAutoEnableDisableSearch();
    slotApplySearch();
}

void MessageListWidget::slotApplySearch()
{
    applySearch();
}

bool MessageListWidget::applySearch()
{
    // Another search, even one typed with the same words, is no longer the saved search
    if (!m_applyingSavedSearch && !m_activeSavedSearch.owner.isEmpty() && !matchesSavedSearch(m_activeSavedSearch))
        forgetActiveSavedSearch();
    m_searchDay = QDate::currentDate();
    UiUtils::SearchQuery::Error problem;
    bool relativeQuery = false;
    const QStringList conditions = searchConditions(&problem, &relativeQuery);
    if (!problem.isEmpty()) {
        // The list keeps showing the previous search rather than everything, or a server error for a typo
        showSearchError(problem);
        return false;
    }
    m_searchError->hide();
    const QStringList previousDescription = std::exchange(m_searchDescription, QStringList());
    if (!conditions.isEmpty()) {
        if (const QString query = m_quickSearchText->text().trimmed(); !query.isEmpty())
            m_searchDescription << i18nc("@info:status %1 is the search query", "Search: %1", query);
        if (UiUtils::SearchScope::isLimited(m_dateRange))
            m_searchDescription << dateRangeText(m_dateRange);
        m_searchDescription << i18nc("@info:status %1 is where the search looks, such as All Accounts", "In: %1", placesText());
    }
    // The values of the facets were those of the previous results
    if (m_searchDescription != previousDescription)
        clearFacets();
    updateSearchSummary();
    // Custom ranges are fixed days; the presets end today
    const bool relative = relativeQuery || (m_dateRange.dates != Dates::AnyTime && m_dateRange.dates != Dates::Custom);
    if (relative)
        m_dayChangeTimer->start();
    else
        m_dayChangeTimer->stop();
    emit requestingSearch(conditions);
    return true;
}

void MessageListWidget::runTypedSearch()
{
    m_completer->popup()->hide();
    if (applySearch())
        rememberSearch(m_quickSearchText->text());
}

void MessageListWidget::rememberSearch(const QString &query)
{
    if (!m_rememberSearches->isChecked() || !m_searchHistory.add(query))
        return;
    saveSearchHistory();
    updateHistoryActions();
}

void MessageListWidget::saveSearchHistory()
{
    // Only what the user typed is kept, and only here; queries are never logged
    KConfigGroup group = searchHistoryGroup();
    const QStringList entries = m_searchHistory.entries();
    if (entries.isEmpty())
        group.deleteEntry(searchHistoryKey());
    else
        group.writeEntry(searchHistoryKey(), entries);
    group.sync();
}

void MessageListWidget::updateHistoryActions()
{
    m_clearSearchHistory->setEnabled(!m_searchHistory.entries().isEmpty());
}

QStringList MessageListWidget::searchHistory() const
{
    return m_searchHistory.entries();
}

void MessageListWidget::setFolderSource(const FolderSource &source)
{
    m_folderSource = source;
}

void MessageListWidget::updateSuggestions()
{
    if (!m_suggestWhileTyping->isChecked() || m_quickSearchText->text().trimmed().isEmpty()) {
        m_suggestions->clear();
        m_completer->popup()->hide();
        return;
    }
    requestContacts();
    fillSuggestions();
    if (m_suggestions->rowCount() > 0)
        m_completer->complete();
    else
        m_completer->popup()->hide();
}

void MessageListWidget::requestContacts()
{
    const QString query = UiUtils::SearchSuggestions::contactQuery(m_quickSearchText->text());
    if (query.isEmpty() || query == m_contactQuery)
        return;
    m_contactQuery = query;
    m_contacts.clear();
    emit contactSuggestionsRequested(query);
}

void MessageListWidget::showContactSuggestions(const QString &query, const QList<UiUtils::SearchSuggestions::Contact> &contacts)
{
    // An answer about a word which is no longer typed
    if (query != m_contactQuery)
        return;
    m_contacts = contacts;
    if (UiUtils::SearchSuggestions::contactQuery(m_quickSearchText->text()) != query)
        return;
    const bool shown = m_completer->popup()->isVisible();
    if (!shown && !m_suggestWhileTyping->isChecked())
        return;
    fillSuggestions();
    if (m_suggestions->rowCount() > 0 && (shown || m_quickSearchText->hasFocus()))
        m_completer->complete();
}

void MessageListWidget::fillSuggestions()
{
    using namespace UiUtils::SearchSuggestions;
    const QString text = m_quickSearchText->text();
    const Token token = currentToken(text);
    QStringList tags;
    if (m_favoriteTags) {
        for (int row = 0; row < m_favoriteTags->rowCount(); ++row)
            tags << m_favoriteTags->tagNameByIndex(row);
    }
    // Only a plain word can name a folder, which saves walking the folders for every other key
    QList<FolderCandidate> folders;
    if (m_folderSource && token.isValid() && token.field.isEmpty() && token.prefix.isEmpty() && token.value.size() >= 2)
        folders = m_folderSource();
    const QList<Contact> contacts = contactQuery(text) == m_contactQuery ? m_contacts : QList<Contact>();
    const QStringList history = m_rememberSearches->isChecked() ? m_searchHistory.entries() : QStringList();

    m_suggestions->clear();
    const QList<Suggestion> suggestions = suggest(text, history, contacts, tags, folders);
    for (const Suggestion &suggestion : suggestions) {
        QString label;
        QString icon;
        QString toolTip;
        switch (suggestion.kind) {
        case Kind::RecentSearch:
            label = suggestion.value;
            icon = QStringLiteral("document-open-recent");
            toolTip = i18nc("@info:tooltip", "A recent search");
            break;
        case Kind::Contact: {
            const QString term = suggestion.text.mid(token.start);
            label = suggestion.detail.isEmpty()
                ? term
                : i18nc("@item:inlistbox search suggestion, %1 is a term such as from:ann@example.org, %2 the name of the person",
                        "%1 (%2)", term, suggestion.detail);
            icon = QStringLiteral("im-user");
            toolTip = i18nc("@info:tooltip", "A contact of the address book");
            break;
        }
        case Kind::Tag:
            label = suggestion.text.mid(token.start);
            icon = QStringLiteral("tag");
            toolTip = i18nc("@info:tooltip", "A tag");
            break;
        case Kind::Folder:
            label = i18nc("@item:inlistbox search suggestion, %1 is a folder such as Archive / 2024", "Search in %1", suggestion.value);
            icon = QStringLiteral("folder");
            toolTip = i18nc("@info:tooltip", "Search this folder for the rest of the query, instead of searching for the word");
            break;
        }
        auto *item = new QStandardItem(suggestionIcon(icon), label);
        item->setToolTip(toolTip);
        item->setEditable(false);
        item->setData(static_cast<int>(suggestion.kind), RoleSuggestionKind);
        item->setData(suggestion.text, RoleSuggestionText);
        item->setData(suggestion.folderKey, RoleSuggestionFolder);
        m_suggestions->appendRow(item);
    }
}

void MessageListWidget::chooseSuggestion(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    const auto kind = static_cast<SuggestionKind>(index.data(RoleSuggestionKind).toInt());
    const QString text = index.data(RoleSuggestionText).toString();
    const QString folder = index.data(RoleSuggestionFolder).toString();
    m_completer->popup()->hide();
    m_quickSearchText->setText(text);
    // An empty text would start the timer which applies a cleared search
    m_searchResetTimer->stop();
    if (kind == SuggestionKind::Folder) {
        m_selectedFolders = {folder};
        m_places = Places::SelectedFolders;
        showDateRange();
    }
    if (applySearch())
        rememberSearch(text);
}

bool MessageListWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::KeyPress)
        return QWidget::eventFilter(watched, event);
    const auto *key = static_cast<QKeyEvent *>(event);
    QAbstractItemView *popup = m_completer->popup();
    if (watched == m_quickSearchText)
        m_suggestionsEscaped = false;
    if (watched == popup && popup->isVisible()) {
        switch (key->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            // The completer would only put the text in place, and the line edit ignores Return while suggestions are
            // shown; the suggestion may stand for a folder as well
            if (popup->currentIndex().isValid())
                chooseSuggestion(popup->currentIndex());
            else
                runTypedSearch();
            return true;
        case Qt::Key_Escape:
            m_suggestionsEscaped = true;
            break;
        default:
            break;
        }
    } else if (watched == m_quickSearchText && key->key() == Qt::Key_Down && key->modifiers() == Qt::NoModifier) {
        requestContacts();
        fillSuggestions();
        if (m_suggestions->rowCount() > 0) {
            m_completer->complete();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MessageListWidget::showSearchError(const UiUtils::SearchQuery::Error &problem)
{
    m_searchError->showError(i18nc("@info %1 explains what is wrong with the search query", "Invalid search: %1", problem.message));
    if (problem.position >= 0 && problem.position < m_quickSearchText->text().size())
        m_quickSearchText->setSelection(problem.position, problem.length);
}

void MessageListWidget::slotAutoEnableDisableSearch()
{
    bool isEnabled;
    if (!m_searchAvailable) {
        isEnabled = false;
    } else if (!m_quickSearchText->text().isEmpty() || UiUtils::SearchScope::isLimited(m_dateRange)) {
        // Some search criteria are in effect and suddenly all matching messages
        // disappear. We have to make sure that the search bar remains enabled.
        isEnabled = true;
    } else if (tree && tree->model()) {
        isEnabled = tree->model()->rowCount();
    } else {
        isEnabled = false;
    }
    m_quickSearchText->setEnabled(isEnabled);
    m_searchOptions->setEnabled(isEnabled);
}

void MessageListWidget::slotSortingFailed()
{
    StateColors::applyToField(m_quickSearchText, StateColors::State::Negative);
    QTimer::singleShot(500, this, SLOT(slotResetSortingFailed()));
}

void MessageListWidget::slotResetSortingFailed()
{
    StateColors::applyToField(m_quickSearchText, StateColors::State::Normal);
}

void MessageListWidget::slotConditionalSearchReset()
{
    if (m_quickSearchText->text().isEmpty())
        m_searchResetTimer->start(250);
    else
        m_searchResetTimer->stop();
}

void MessageListWidget::slotUpdateSearchCursor()
{
    int cp = m_quickSearchText->cursorPosition();
    int ts = -1, te = -1;
    for (int i = cp-1; i > -1; --i) {
        if (m_quickSearchText->text().at(i) == QLatin1Char('>'))
            break; // invalid
        if (m_quickSearchText->text().at(i) == QLatin1Char('<')) {
            ts = i;
            break; // found TagStart
        }
    }
    if (ts < 0)
        return; // not inside tag!
    for (int i = cp; i < m_quickSearchText->text().length(); ++i) {
        if (m_quickSearchText->text().at(i) == QLatin1Char('<'))
            break; // invalid
        if (m_quickSearchText->text().at(i) == QLatin1Char('>')) {
            te = i;
            break; // found TagEnd
        }
    }
    if (te < 0)
        return; // not inside tag?
    if (QStringView(m_quickSearchText->text()).mid(ts, m_queryPlaceholder.length()) == m_queryPlaceholder)
        m_quickSearchText->setSelection(ts, m_queryPlaceholder.length());
}

void MessageListWidget::slotComplexSearchInput(QAction *act)
{
    QString s = act->data().toString();
    const int selectionStart = m_quickSearchText->selectionStart() - 1;
    if (selectionStart > -1 && m_quickSearchText->text().at(selectionStart) != QLatin1Char(' '))
            s.prepend(QLatin1Char(' '));
    m_quickSearchText->insert(s);
    if (!m_quickSearchText->text().startsWith(QLatin1String(":="))) {
        s = m_quickSearchText->text().trimmed();
        m_quickSearchText->setText(QLatin1String(":=") + s);
    }
    m_quickSearchText->setFocus();
    const int pos = m_quickSearchText->text().indexOf(m_queryPlaceholder);
    if (pos > -1)
        m_quickSearchText->setSelection(pos, m_queryPlaceholder.length());
}

void MessageListWidget::slotDeActivateSimpleSearch()
{
    const bool isEnabled = !(m_rawSearch->isChecked() && m_quickSearchText->text().startsWith(QLatin1String(":=")));
    m_searchInSubject->setEnabled(isEnabled);
    m_searchInBody->setEnabled(isEnabled);
    m_searchInSenders->setEnabled(isEnabled);
    m_searchInRecipients->setEnabled(isEnabled);
    m_searchFuzzy->setEnabled(isEnabled && m_supportsFuzzySearch);
}

QStringList MessageListWidget::searchConditions() const
{
    return searchConditions(nullptr, nullptr);
}

QStringList MessageListWidget::searchConditions(UiUtils::SearchQuery::Error *problem, bool *relativeDates) const
{
    using namespace UiUtils::SearchScope;
    const QString text = m_quickSearchText->text();
    if (!m_quickSearchText->isEnabled() || (text.isEmpty() && !isLimited(m_dateRange)))
        return QStringList();

    static QString rawPrefix = QStringLiteral(":=");
    const QDate today = QDate::currentDate();

    if (m_rawSearch->isChecked() && text.startsWith(rawPrefix)) {
        // It's a "raw" IMAP search, let's pass it through as a single item, which the parser sends verbatim
        const QString rawQuery = text.mid(rawPrefix.size());
        if (rawQuery.trimmed().isEmpty() && isLimited(m_dateRange))
            return dateConditions(m_dateRange, today);
        return QStringList() << withDateConditions(rawQuery, m_dateRange, today);
    }

    const auto query = UiUtils::SearchQuery::Query::parse(text, today);
    if (relativeDates)
        *relativeDates = query.usesRelativeDates();
    if (query.isEmpty())
        return dateConditions(m_dateRange, today);

    // Words without a field search the fields chosen in the options; "from:" and the like search that field only
    Fields fields;
    if (m_searchInSubject->isChecked())
        fields |= Field::Subject;
    if (m_searchInBody->isChecked())
        fields |= Field::Body;
    if (m_searchInRecipients->isChecked())
        fields |= Field::Recipients;
    if (m_searchInSenders->isChecked())
        fields |= Field::Senders;
    UiUtils::SearchQuery::Error error;
    const QStringList keys = query.imapConditions(fields, m_searchFuzzy->isChecked() && m_supportsFuzzySearch, &error);
    if (keys.isEmpty()) {
        if (problem)
            *problem = error;
        return keys;
    }
    return keys + dateConditions(m_dateRange, today);
}

void MessageListWidget::setFuzzySearchSupported(bool supported)
{
    m_supportsFuzzySearch = supported;
    m_searchFuzzy->setEnabled(supported);
    m_searchFuzzy->setChecked(supported);
}

void MessageListWidget::setRawSearchEnabled(bool enabled)
{
    m_rawSearch->setChecked(enabled);
}

}
