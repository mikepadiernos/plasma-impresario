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
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QStringView>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidgetAction>
#include "LineEdit.h"
#include "MsgListView.h"
#include "ReplaceCharValidator.h"
#include "Util.h"
#include "UiUtils/IconLoader.h"
#include "UiUtils/QuickFilters.h"

namespace Gui {

namespace {

using UiUtils::SearchScope::DateRange;
using UiUtils::SearchScope::Dates;

/** @short A short description of @p range, such as "Past Week" or "Since 3/5/26" */
QString dateRangeText(const DateRange &range)
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

/** @short The date range stored with a saved search; searches saved without one use any time */
DateRange savedDateRange(const KConfigGroup &group)
{
    DateRange range;
    range.dates = UiUtils::SearchScope::datesFromConfigName(group.readEntry("Dates", QString()));
    if (range.dates == Dates::Custom) {
        range.from = QDate::fromString(group.readEntry("DateFrom", QString()), Qt::ISODate);
        range.to = QDate::fromString(group.readEntry("DateTo", QString()), Qt::ISODate);
    }
    return range;
}

}

MessageListWidget::MessageListWidget(QWidget *parent, Imap::Mailbox::FavoriteTagsModel *m_favoriteTagsModel, const QString &accountId) :
    QWidget(parent), m_accountId(accountId), m_supportsFuzzySearch(false)
{
    tree = new MsgListView(this, m_favoriteTagsModel);

    m_quickSearchText = new LineEdit(this);
    m_quickSearchText->setObjectName(QStringLiteral("quickSearch"));
    m_quickSearchText->setHistoryEnabled(true);
    // Filter out newline. It will wreak havoc into the direct IMAP passthrough and could lead to data loss.
    QValidator *validator = new ReplaceCharValidator(QLatin1Char('\n'), QLatin1Char(' '), m_quickSearchText);
    m_quickSearchText->setValidator(validator);
    m_quickSearchText->setPlaceholderText(tr("Quick Search"));
    m_searchToolTip = tr("Type in a text to search for within this mailbox. "
                         "The icon on the left can be used to limit the search options "
                         "(like whether to include addresses or message bodies, etc)."
                         "<br/><hr/>"
                         "Experts who have read RFC3501 can use the <code>:=</code> prefix and switch to a raw IMAP mode.");
    m_quickSearchText->setToolTip(m_searchToolTip);
    m_queryPlaceholder = tr("<query>");

    connect(m_quickSearchText, &QLineEdit::returnPressed, this, &MessageListWidget::slotApplySearch);
    connect(m_quickSearchText, &QLineEdit::textChanged, this, &MessageListWidget::slotConditionalSearchReset);
    connect(m_quickSearchText, &QLineEdit::cursorPositionChanged, this, &MessageListWidget::slotUpdateSearchCursor);
    connect(m_quickSearchText, &LineEdit::escapePressed, this, [this]() {
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
    m_searchFuzzy = optionsMenu->addAction(tr("Fuzzy Search"));
    m_searchFuzzy->setCheckable(true);
    optionsMenu->addSeparator();
    QAction *saveSearch = optionsMenu->addAction(i18nc("@action", "Save Current Search..."));
    saveSearch->setObjectName(QStringLiteral("saveSearch"));
    connect(saveSearch, &QAction::triggered, this, &MessageListWidget::saveCurrentSearch);
    m_savedSearches = optionsMenu->addMenu(i18nc("@title:menu", "Saved Searches"));
    m_savedSearches->setObjectName(QStringLiteral("savedSearches"));
    connect(m_savedSearches, &QMenu::aboutToShow, this, &MessageListWidget::rebuildSavedSearchMenu);
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

    m_listStatus = new QLabel(this);
    m_listStatus->setObjectName(QStringLiteral("messageListStatus"));
    m_listStatus->setAccessibleName(i18nc("@label", "Message list status"));
    // Long texts must not make the list wider
    m_listStatus->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    const int margin = style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 2;
    m_listStatus->setContentsMargins(margin, margin / 2, margin, margin / 2);
    m_listStatus->hide();

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
    auto *statusLayout = new QHBoxLayout(m_statusRow);
    statusLayout->setSpacing(0);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->addWidget(m_scope);
    statusLayout->addWidget(m_listStatus, 1);
    m_statusRow->hide();

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(m_toolRow);
    layout->addWidget(tree);
    layout->addWidget(m_statusRow);

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
    if (searchConditions().isEmpty()) {
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
    for (const QString &id : searches.groupList()) {
        if (searches.group(id).readEntry("Name", QString()).compare(name, Qt::CaseInsensitive) == 0) {
            Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Save Search"),
                                         i18nc("@info", "A saved search with this name already exists."));
            return;
        }
    }
    KConfigGroup group = searches.group(QUuid::createUuid().toString(QUuid::WithoutBraces));
    group.writeEntry("Name", name);
    group.writeEntry("Query", m_quickSearchText->text());
    group.writeEntry("Subject", m_searchInSubject->isChecked());
    group.writeEntry("Body", m_searchInBody->isChecked());
    group.writeEntry("Senders", m_searchInSenders->isChecked());
    group.writeEntry("Recipients", m_searchInRecipients->isChecked());
    const bool rawQuery = m_rawSearch->isChecked() && m_quickSearchText->text().startsWith(QLatin1String(":="));
    group.writeEntry("Fuzzy", !rawQuery && m_supportsFuzzySearch && m_searchFuzzy->isChecked());
    group.writeEntry("Raw", m_rawSearch->isChecked());
    // Presets stay relative, so a search for the past week keeps finding the latest week
    group.writeEntry("Dates", UiUtils::SearchScope::configName(m_dateRange.dates));
    group.writeEntry("DateFrom", m_dateRange.from.toString(Qt::ISODate));
    group.writeEntry("DateTo", m_dateRange.to.toString(Qt::ISODate));
    if (!group.sync())
        Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Save Search"),
                                     i18nc("@info", "The search could not be saved."));
}

void MessageListWidget::rebuildSavedSearchMenu()
{
    qDeleteAll(m_savedSearches->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly));
    m_savedSearches->clear();
    const auto ids = savedSearchGroup().groupList();
    for (const QString &id : ids)
        addSavedSearch(id);
}

void MessageListWidget::setAccountId(const QString &accountId)
{
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

void MessageListWidget::setListStatus(const QString &text)
{
    m_listStatus->setText(text);
    m_listStatus->setVisible(!text.isEmpty());
    m_statusRow->setVisible(!m_scope->isHidden() || !m_listStatus->isHidden());
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
    m_statusRow->setVisible(!m_scope->isHidden() || !m_listStatus->isHidden());
}

void MessageListWidget::setCustomScope(const QString &text, const QString &toolTip)
{
    m_scope->setText(text);
    m_scope->setToolTip(toolTip);
    m_scope->setAccessibleName(text);
    m_scope->setVisible(!text.isEmpty());
    m_statusRow->setVisible(!m_scope->isHidden() || !m_listStatus->isHidden());
}

void MessageListWidget::setSearchAvailable(bool available)
{
    if (m_searchAvailable == available)
        return;
    m_searchAvailable = available;
    // A search which still filters a single folder would look like it applies to the shown list
    if (!available && (!m_quickSearchText->text().isEmpty() || UiUtils::SearchScope::isLimited(m_dateRange))) {
        m_quickSearchText->clear();
        m_dateRange = {};
        showDateRange();
        m_searchResetTimer->stop();
        slotApplySearch();
    }
    slotAutoEnableDisableSearch();
}

KConfigGroup MessageListWidget::savedSearchGroup() const
{
    KConfigGroup searches(KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")), QStringLiteral("SavedSearches"));
    return searches.group(m_accountId);
}

void MessageListWidget::addSavedSearch(const QString &id)
{
    const KConfigGroup group = savedSearchGroup().group(id);
    const QString name = group.readEntry("Name", QString());
    if (name.isEmpty() || (group.readEntry("Query", QString()).isEmpty() && !UiUtils::SearchScope::isLimited(savedDateRange(group)))) {
        qWarning() << "Ignoring invalid saved search" << id;
        return;
    }
    QString menuName = name;
    menuName.replace(QLatin1Char('&'), QStringLiteral("&&"));
    QMenu *entry = m_savedSearches->addMenu(menuName);
    QAction *run = entry->addAction(i18nc("@action", "Run Search"));
    run->setObjectName(QStringLiteral("runSavedSearch"));
    connect(run, &QAction::triggered, this, [this, id]() { runSavedSearch(id); });
    QAction *remove = entry->addAction(i18nc("@action", "Delete Search..."));
    remove->setObjectName(QStringLiteral("deleteSavedSearch"));
    connect(remove, &QAction::triggered, this, [this, id]() { deleteSavedSearch(id); });
}

void MessageListWidget::runSavedSearch(const QString &id)
{
    const KConfigGroup group = savedSearchGroup().group(id);
    const bool fuzzy = group.readEntry("Fuzzy", false);
    if (fuzzy && !m_supportsFuzzySearch) {
        Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Run Search"),
                                     i18nc("@info", "This search requires fuzzy search support from the server."));
        return;
    }
    m_rawSearch->setChecked(group.readEntry("Raw", false));
    m_searchInSubject->setChecked(group.readEntry("Subject", false));
    m_searchInBody->setChecked(group.readEntry("Body", false));
    m_searchInSenders->setChecked(group.readEntry("Senders", false));
    m_searchInRecipients->setChecked(group.readEntry("Recipients", false));
    m_searchFuzzy->setChecked(fuzzy);
    m_quickSearchText->setText(group.readEntry("Query", QString()));
    m_dateRange = savedDateRange(group);
    showDateRange();
    m_searchResetTimer->stop();
    slotApplySearch();
}

void MessageListWidget::deleteSavedSearch(const QString &id)
{
    KConfigGroup group = savedSearchGroup().group(id);
    const QString name = group.readEntry("Name", QString());
    if (QMessageBox::question(this, i18nc("@title:window", "Delete Search"),
                              Qt::convertFromPlainText(i18nc("@info", "Delete the saved search \"%1\"?", name))) != QMessageBox::Yes)
        return;
    group.deleteGroup();
    if (!group.sync())
        Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Delete Search"),
                                     i18nc("@info", "The search could not be deleted."));
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
    if (!UiUtils::SearchScope::isLimited(m_dateRange)) {
        m_dateMenu->setTitle(i18nc("@title:menu", "Date"));
        m_quickSearchText->setPlaceholderText(tr("Quick Search"));
        m_quickSearchText->setToolTip(m_searchToolTip);
        return;
    }
    const QString text = dateRangeText(m_dateRange);
    m_dateMenu->setTitle(i18nc("@title:menu %1 is a date range such as Past Week", "Date: %1", text));
    m_quickSearchText->setPlaceholderText(i18nc("@info:placeholder %1 is a date range such as Past Week", "Quick Search (%1)", text));
    m_quickSearchText->setToolTip(m_searchToolTip + QLatin1String("<br/><hr/>")
                                  + i18nc("@info:tooltip %1 is a date range such as Past Week", "Only messages sent in this date range are shown: %1",
                                          text.toHtmlEscaped()));
}

void MessageListWidget::focusSearch()
{
    if (!m_quickSearchText->isEnabled() || m_quickSearchText->hasFocus())
        return;
    m_quickSearchText->setFocus(Qt::ShortcutFocusReason);
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
    m_searchDay = QDate::currentDate();
    // Custom ranges are fixed days; the presets end today
    const bool relative = m_dateRange.dates != Dates::AnyTime && m_dateRange.dates != Dates::Custom;
    if (relative)
        m_dayChangeTimer->start();
    else
        m_dayChangeTimer->stop();
    emit requestingSearch(searchConditions());
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
    QPalette pal = m_quickSearchText->palette();
    pal.setColor(m_quickSearchText->backgroundRole(), Qt::red);
    pal.setColor(m_quickSearchText->foregroundRole(), Qt::white);
    m_quickSearchText->setPalette(pal);
    QTimer::singleShot(500, this, SLOT(slotResetSortingFailed()));
}

void MessageListWidget::slotResetSortingFailed()
{
    m_quickSearchText->setPalette(QPalette());
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

    // "from:", "to:", "subject:" and "list:" search that field only, whatever the options say
    const UiUtils::ParsedFieldFilter fieldFilter = UiUtils::parseFieldFilter(text);
    if (fieldFilter.field != UiUtils::FieldFilter::None) {
        const QStringList keys = UiUtils::fieldFilterConditions(fieldFilter);
        return keys.isEmpty() ? keys : keys + dateConditions(m_dateRange, today);
    }

    Fields fields;
    if (m_searchInSubject->isChecked())
        fields |= Field::Subject;
    if (m_searchInBody->isChecked())
        fields |= Field::Body;
    if (m_searchInRecipients->isChecked())
        fields |= Field::Recipients;
    if (m_searchInSenders->isChecked())
        fields |= Field::Senders;
    return conditions(text, fields, m_searchFuzzy->isChecked() && m_supportsFuzzySearch, m_dateRange, today);
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
