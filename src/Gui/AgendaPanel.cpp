/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "AgendaPanel.h"

#include <algorithm>
#include <limits>
#include <KLocalizedString>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace Gui {

namespace {

using UiUtils::Agenda::Item;
using UiUtils::Agenda::Kind;

constexpr int ItemIndexRole = Qt::UserRole;
/** Past events are shown dimmed, which changes as time goes by */
constexpr int MinuteMs = 60 * 1000;
/** The day timer may fire a little early on some systems */
constexpr int MidnightMarginMs = 500;

QString shortTime(const QDateTime &when)
{
    return QLocale().toString(when.time(), QLocale::ShortFormat);
}

QString shortDay(const QDate &day)
{
    return QLocale().toString(day, QLocale::ShortFormat);
}

/** When the item happens, as the first words of its row */
QString timeText(const Item &item, const QDate &day)
{
    switch (item.kind) {
    case Kind::Event: {
        if (item.allDay)
            return i18nc("@item an event lasting the whole day", "All day");
        const bool startsToday = item.start.date() == day;
        const bool endsToday = item.end.isValid() && item.end.date() == day;
        if (startsToday && (endsToday || !item.end.isValid() || item.end == item.start)) {
            return item.end.isValid() && item.end != item.start
                ? i18nc("@item %1 and %2 are times of an event", "%1–%2", shortTime(item.start), shortTime(item.end))
                : shortTime(item.start);
        }
        if (startsToday)
            return i18nc("@item %1 is the time an event starts, lasting past today", "From %1", shortTime(item.start));
        if (endsToday)
            return i18nc("@item %1 is the time an event which began earlier ends", "Until %1", shortTime(item.end));
        return i18nc("@item an event lasting the whole day", "All day");
    }
    case Kind::Task:
        if (UiUtils::Agenda::isOverdue(item, day))
            return i18nc("@item %1 is the day a task was due", "Overdue since %1", shortDay(item.start.date()));
        if (item.allDay)
            return i18nc("@item a task due today, at no particular time", "Due today");
        return i18nc("@item %1 is the time a task is due", "Due %1", shortTime(item.start));
    case Kind::Message:
        return item.toDo ? i18nc("@item a message marked as a to-do", "To do") : i18nc("@item a flagged message", "Flagged");
    }
    return QString();
}

QString titleOf(const Item &item)
{
    if (!item.title.isEmpty())
        return item.title;
    switch (item.kind) {
    case Kind::Event:
        return i18nc("@item an event without a title", "(Untitled event)");
    case Kind::Task:
        return i18nc("@item a task without a title", "(Untitled task)");
    case Kind::Message:
        return i18nc("@item a message without a subject", "(No subject)");
    }
    return QString();
}

QString sourceOf(const Item &item)
{
    if (!item.source.isEmpty())
        return item.source;
    switch (item.kind) {
    case Kind::Event:
    case Kind::Task:
        return i18nc("@item the source of an event or task whose calendar is not known", "Calendar");
    case Kind::Message:
        return i18nc("@item the source of a message whose account is not known", "Mail");
    }
    return QString();
}

/** The second line of a row */
QString detailText(const Item &item)
{
    QStringList parts;
    if (item.kind == Kind::Message) {
        if (!item.sender.isEmpty())
            parts << item.sender;
        if (item.sent.isValid())
            parts << shortDay(item.sent);
    } else if (!item.location.isEmpty()) {
        parts << item.location;
    }
    parts << sourceOf(item);
    return parts.join(i18nc("@item separator of the details of an agenda item", " · "));
}

QString iconName(const Item &item)
{
    switch (item.kind) {
    case Kind::Event:
        return QStringLiteral("view-calendar-day");
    case Kind::Task:
        return QStringLiteral("view-calendar-tasks");
    case Kind::Message:
        return item.toDo ? QStringLiteral("mail-task") : QStringLiteral("flag");
    }
    return QString();
}

QString itemKey(const Item &item)
{
    if (item.kind == Kind::Message)
        return QStringLiteral("m\n%1\n%2\n%3").arg(item.accountId, item.mailbox, QString::number(item.uid));
    return QStringLiteral("c\n%1\n%2").arg(item.incidenceUid, item.start.toString(Qt::ISODate));
}

bool hasEnded(const Item &item, const QDateTime &now)
{
    if (item.kind != Kind::Event)
        return false;
    if (item.allDay)
        return item.end.isValid() && item.end.date() < now.date();
    const QDateTime end = item.end.isValid() ? item.end : item.start;
    return end <= now && end != item.start;
}

}

EmbeddedPanel::Descriptor AgendaPanel::panelDescriptor()
{
    Descriptor descriptor;
    descriptor.id = QStringLiteral("Agenda");
    descriptor.configKey = QStringLiteral("AgendaPanel");
    descriptor.title = i18nc("@title embedded panel", "My Day");
    descriptor.autoHideToolTip = i18nc("@info:tooltip", "Show My Day only while there are events, tasks or messages to follow up");
    descriptor.defaultEnabled = false;
    descriptor.defaultPlacement = Placement::RightColumn;
    return descriptor;
}

AgendaPanel::AgendaPanel(QWidget *parent)
    : EmbeddedPanel(panelDescriptor(), QStringLiteral("agendaPanel"), parent)
    , m_body(new QWidget(this))
    , m_list(new QListWidget(m_body))
    , m_notice(new QLabel(m_body))
    , m_condensed(new QToolButton(m_body))
    , m_dayTimer(new QTimer(this))
    , m_minuteTimer(new QTimer(this))
    , m_clock([]() { return QDateTime::currentDateTime(); })
{
    m_list->setObjectName(QStringLiteral("agendaPanelList"));
    m_list->setAccessibleName(i18nc("@label", "Today's events, tasks and messages to follow up"));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setTextElideMode(Qt::ElideRight);
    m_list->setWordWrap(false);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListWidget::itemActivated, this, &AgendaPanel::activateCurrent);
    connect(m_list, &QListWidget::currentRowChanged, this, &AgendaPanel::updateActions);
    connect(m_list, &QWidget::customContextMenuRequested, this, &AgendaPanel::showContextMenu);

    m_notice->setObjectName(QStringLiteral("agendaPanelNotice"));
    m_notice->setWordWrap(true);
    m_notice->setTextFormat(Qt::PlainText);
    m_notice->setForegroundRole(QPalette::PlaceholderText);

    m_condensed->setObjectName(QStringLiteral("agendaPanelSummary"));
    m_condensed->setAutoRaise(true);
    m_condensed->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_condensed->setIcon(themedIcon(QStringLiteral("view-calendar-day")));
    m_condensed->setIconSize(QSize(16, 16));
    m_condensed->setPopupMode(QToolButton::InstantPopup);
    m_condensed->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *menu = new QMenu(m_condensed);
    menu->setObjectName(QStringLiteral("agendaPanelSummaryMenu"));
    connect(menu, &QMenu::aboutToShow, this, [this, menu]() { fillMenu(menu); });
    m_condensed->setMenu(menu);

    auto *layout = new QVBoxLayout(m_body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(m_condensed);
    layout->addWidget(m_list, 1);
    layout->addWidget(m_notice);
    setContentWidget(m_body);

    m_joinButton = addHeaderButton(QStringLiteral("Join"), QStringLiteral("call-start"), i18nc("@action:button", "Join Meeting"));
    connect(m_joinButton, &QToolButton::clicked, this, &AgendaPanel::joinCurrent);
    m_calendarButton = addHeaderButton(QStringLiteral("Calendar"), QStringLiteral("view-pim-calendar"),
                                       i18nc("@action:button", "Open in Calendar"));
    m_calendarButton->setToolTip(i18nc("@info:tooltip", "Show and edit events and tasks in the calendar application"));
    connect(m_calendarButton, &QToolButton::clicked, this, &AgendaPanel::calendarRequested);

    m_dayTimer->setSingleShot(true);
    m_dayTimer->setTimerType(Qt::PreciseTimer);
    connect(m_dayTimer, &QTimer::timeout, this, &AgendaPanel::refresh);
    m_minuteTimer->setInterval(MinuteMs);
    connect(m_minuteTimer, &QTimer::timeout, this, [this]() {
        if (now().date() != m_day)
            refresh();
        else
            updateForm();
    });
    m_minuteTimer->start();
    connect(this, &EmbeddedPanel::placementChanged, this, &AgendaPanel::updateForm);

    refresh();
}

void AgendaPanel::setCalendarSource(UiUtils::AgendaSource *source)
{
    if (m_source == source)
        return;
    if (m_source)
        disconnect(m_source, nullptr, this, nullptr);
    m_source = source;
    if (m_source)
        connect(m_source, &UiUtils::AgendaSource::changed, this, &AgendaPanel::refresh);
    refresh();
}

void AgendaPanel::setMessages(const QList<Item> &messages)
{
    m_messages = messages;
    refresh();
}

void AgendaPanel::setClock(const std::function<QDateTime()> &clock)
{
    m_clock = clock ? clock : std::function<QDateTime()>([]() { return QDateTime::currentDateTime(); });
    refresh();
}

QDateTime AgendaPanel::now() const
{
    return m_clock();
}

bool AgendaPanel::calendarsAvailable() const
{
    return m_source && m_source->state() != UiUtils::AgendaSource::State::Unavailable;
}

QList<Item> AgendaPanel::items() const
{
    return m_items;
}

QDate AgendaPanel::day() const
{
    return m_day;
}

bool AgendaPanel::isCondensed() const
{
    return placement() == Placement::Footer;
}

QString AgendaPanel::condensedText() const
{
    return m_condensed->text();
}

QString AgendaPanel::accessibleName(const Item &item, const QDate &day)
{
    return i18nc("@info accessible name of an agenda item: %1 when, %2 title, %3 calendar or account", "%1, %2, %3",
                 timeText(item, day), titleOf(item), sourceOf(item));
}

void AgendaPanel::panelEnabledChanged()
{
    emit enabledChanged(isPanelEnabled());
}

bool AgendaPanel::hasContent() const
{
    return !m_items.isEmpty();
}

QWidget *AgendaPanel::focusTarget() const
{
    return isCondensed() ? static_cast<QWidget *>(m_condensed) : m_list;
}

int AgendaPanel::currentRow() const
{
    const QListWidgetItem *current = m_list->currentItem();
    return current ? current->data(ItemIndexRole).toInt() : -1;
}

void AgendaPanel::refresh()
{
    const QDateTime current = now();
    m_day = current.date();
    QList<Item> all;
    if (m_source && m_source->state() != UiUtils::AgendaSource::State::Unavailable)
        all = m_source->items(m_day);
    all += m_messages;
    const int previous = currentRow();
    const QString previousKey = previous >= 0 && previous < m_items.size() ? itemKey(m_items.at(previous)) : QString();
    m_items = UiUtils::Agenda::sorted(all, m_day);

    const QSignalBlocker blocker(m_list);
    m_list->clear();
    for (int i = 0; i < m_items.size(); ++i) {
        const Item &item = m_items.at(i);
        auto *row = new QListWidgetItem(themedIcon(iconName(item)),
                                        i18nc("@item %1 when, %2 title, %3 the details", "%1  %2\n%3", timeText(item, m_day),
                                              titleOf(item), detailText(item)),
                                        m_list);
        row->setData(ItemIndexRole, i);
        row->setData(Qt::AccessibleTextRole, accessibleName(item, m_day));
        QString toolTip = row->text();
        if (item.joinUrl.isValid())
            toolTip += QLatin1Char('\n') + i18nc("@info:tooltip %1 is a link", "Meeting link: %1", item.joinUrl.toDisplayString());
        row->setToolTip(toolTip);
        if (itemKey(item) == previousKey)
            m_list->setCurrentItem(row);
    }
    if (!m_list->currentItem() && m_list->count() > 0)
        m_list->setCurrentRow(0, QItemSelectionModel::NoUpdate);

    const bool loading = m_source && m_source->state() == UiUtils::AgendaSource::State::Loading;
    if (m_items.isEmpty()) {
        if (loading)
            showPlaceholder(i18nc("@info", "Loading calendars…"));
        else if (calendarsAvailable())
            showPlaceholder(i18nc("@info", "Nothing scheduled, due or flagged today."));
        else
            showPlaceholder(i18nc("@info", "Calendars are unavailable. No flagged messages to follow up."));
    } else {
        showContent();
    }
    m_notice->setText(loading ? i18nc("@info", "Loading calendars…")
                              : i18nc("@info", "Calendars are unavailable, so only flagged messages are listed."));
    scheduleNextDay();
    updateForm();
    updateVisibility();
}

void AgendaPanel::scheduleNextDay()
{
    m_dayTimer->start(int(std::min<qint64>(UiUtils::Agenda::msecsUntilNextDay(now()) + MidnightMarginMs, std::numeric_limits<int>::max())));
}

void AgendaPanel::updateForm()
{
    const QDateTime current = now();
    const bool condensed = isCondensed();
    const bool loading = m_source && m_source->state() == UiUtils::AgendaSource::State::Loading;
    m_list->setVisible(!condensed);
    m_notice->setVisible(!condensed && (loading || !calendarsAvailable()));
    m_condensed->setVisible(condensed);
    setPreferredRows(condensed ? 1 : 6);

    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *row = m_list->item(i);
        const int index = row->data(ItemIndexRole).toInt();
        const bool ended = index >= 0 && index < m_items.size() && hasEnded(m_items.at(index), current);
        row->setForeground(ended ? palette().brush(QPalette::Disabled, QPalette::Text) : palette().brush(QPalette::Text));
    }

    QStringList parts;
    const int next = UiUtils::Agenda::nextEvent(m_items, current);
    if (next >= 0) {
        const Item &item = m_items.at(next);
        parts << (item.start <= current ? i18nc("@info %1 is an event going on now", "Now: %1", titleOf(item))
                                        : i18nc("@info %1 is a time, %2 the next event", "Next: %1 %2", shortTime(item.start), titleOf(item)));
    } else if (calendarsAvailable() && !loading) {
        parts << i18nc("@info", "No more events today");
    }
    if (const int tasks = UiUtils::Agenda::dueTasks(m_items, m_day))
        parts << i18ncp("@info", "%1 task due", "%1 tasks due", tasks);
    if (const int messages = UiUtils::Agenda::messages(m_items))
        parts << i18ncp("@info", "%1 to follow up", "%1 to follow up", messages);
    if (loading)
        parts << i18nc("@info", "loading calendars");
    else if (!calendarsAvailable())
        parts << i18nc("@info", "calendars unavailable");
    const QString summary = parts.join(i18nc("@info separator of the parts of the My Day summary", " · "));
    m_condensed->setText(summary);
    m_condensed->setToolTip(summary);
    m_condensed->setAccessibleName(i18nc("@info %1 summarizes the day", "My Day: %1", summary));
    updateActions();
}

void AgendaPanel::updateActions()
{
    const int index = currentRow();
    const bool join = index >= 0 && index < m_items.size() && m_items.at(index).joinUrl.isValid();
    m_joinButton->setVisible(!isCondensed());
    m_joinButton->setEnabled(join);
    m_joinButton->setToolTip(join ? i18nc("@info:tooltip %1 is a link", "Join the online meeting at %1",
                                          m_items.at(index).joinUrl.toDisplayString())
                                  : i18nc("@info:tooltip", "Join the online meeting of the selected event"));
    m_calendarButton->setEnabled(calendarsAvailable());
}

void AgendaPanel::activateCurrent()
{
    const int index = currentRow();
    if (index >= 0 && index < m_items.size())
        activate(m_items.at(index));
}

void AgendaPanel::joinCurrent()
{
    const int index = currentRow();
    if (index >= 0 && index < m_items.size() && m_items.at(index).joinUrl.isValid())
        emit joinRequested(m_items.at(index).joinUrl);
}

void AgendaPanel::activate(const Item &item)
{
    if (item.kind == Kind::Message)
        emit messageActivated(item);
    else
        emit calendarRequested();
}

void AgendaPanel::fillMenu(QMenu *menu)
{
    menu->clear();
    if (m_items.isEmpty()) {
        menu->addAction(i18nc("@item:inmenu", "Nothing for today"))->setEnabled(false);
    }
    for (const Item &item : std::as_const(m_items)) {
        QAction *action = menu->addAction(themedIcon(iconName(item)),
                                          i18nc("@item:inmenu %1 when, %2 title", "%1  %2", timeText(item, m_day), titleOf(item)));
        action->setToolTip(accessibleName(item, m_day));
        connect(action, &QAction::triggered, this, [this, item]() { activate(item); });
        if (item.joinUrl.isValid()) {
            QAction *join = menu->addAction(themedIcon(QStringLiteral("call-start")),
                                            i18nc("@action:inmenu %1 is an event", "Join “%1”", titleOf(item)));
            connect(join, &QAction::triggered, this, [this, url = item.joinUrl]() { emit joinRequested(url); });
        }
    }
    if (calendarsAvailable()) {
        menu->addSeparator();
        menu->addAction(themedIcon(QStringLiteral("view-pim-calendar")), i18nc("@action:inmenu", "Open in Calendar"), this,
                        &AgendaPanel::calendarRequested);
    }
}

void AgendaPanel::showContextMenu(const QPoint &position)
{
    QListWidgetItem *row = m_list->itemAt(position);
    if (row)
        m_list->setCurrentItem(row);
    const int index = currentRow();
    QMenu menu(this);
    if (index >= 0 && index < m_items.size()) {
        const Item &item = m_items.at(index);
        if (item.kind == Kind::Message) {
            menu.addAction(themedIcon(QStringLiteral("mail-message")), i18nc("@action:inmenu", "Open Message"), this,
                           &AgendaPanel::activateCurrent);
        }
        if (item.joinUrl.isValid()) {
            menu.addAction(themedIcon(QStringLiteral("call-start")), i18nc("@action:inmenu", "Join Meeting"), this,
                           &AgendaPanel::joinCurrent);
        }
    }
    if (calendarsAvailable()) {
        menu.addAction(themedIcon(QStringLiteral("view-pim-calendar")), i18nc("@action:inmenu", "Open in Calendar"), this,
                       &AgendaPanel::calendarRequested);
    }
    if (!menu.isEmpty())
        menu.exec(m_list->viewport()->mapToGlobal(position));
}

}
