/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "AkonadiAgendaSource.h"

#include <Akonadi/CalendarUtils>
#include <Akonadi/ETMCalendar>
#include <Akonadi/EntityTreeModel>
#include <Akonadi/ServerManager>
#include <KCalendarCore/Event>
#include <KCalendarCore/OccurrenceIterator>
#include <KCalendarCore/Todo>
#include <QSet>
#include <QTimeZone>

namespace UiUtils {

namespace {

/** Events which began this many days before still show while they last */
constexpr int lookBackDays = 14;

QUrl joinUrlOf(const KCalendarCore::Incidence::Ptr &incidence)
{
    const auto conferences = incidence->conferences();
    for (const auto &conference : conferences) {
        const QUrl uri = conference.uri();
        if (uri.scheme() == QLatin1String("https") || uri.scheme() == QLatin1String("http"))
            return uri;
    }
    const QUrl url = incidence->url();
    if (url.isValid() && (url.scheme() == QLatin1String("https") || url.scheme() == QLatin1String("http")))
        return url;
    if (const QUrl found = Agenda::findMeetingUrl(incidence->location()); found.isValid())
        return found;
    return Agenda::findMeetingUrl(incidence->description());
}

}

AkonadiAgendaSource::AkonadiAgendaSource(QObject *parent)
    : AgendaSource(parent)
{
    m_changed.setSingleShot(true);
    m_changed.setInterval(200);
    connect(&m_changed, &QTimer::timeout, this, &AgendaSource::changed);
    connect(Akonadi::ServerManager::self(), &Akonadi::ServerManager::stateChanged, this, &AkonadiAgendaSource::serverStateChanged);
    serverStateChanged();
}

AkonadiAgendaSource::~AkonadiAgendaSource() = default;

AgendaSource::State AkonadiAgendaSource::state() const
{
    return m_state;
}

void AkonadiAgendaSource::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    m_changed.start();
}

void AkonadiAgendaSource::serverStateChanged()
{
    if (Akonadi::ServerManager::state() != Akonadi::ServerManager::Running) {
        m_calendar.reset();
        setState(State::Unavailable);
        return;
    }
    if (m_calendar)
        return;
    m_calendar.reset(new Akonadi::ETMCalendar({KCalendarCore::Event::eventMimeType(), KCalendarCore::Todo::todoMimeType()}));
    // Every calendar of the user, rather than those which a calendar application chose to show
    m_calendar->setCollectionFilteringEnabled(false);
    connect(m_calendar.data(), &Akonadi::ETMCalendar::calendarChanged, &m_changed, qOverload<>(&QTimer::start));
    Akonadi::EntityTreeModel *model = m_calendar->entityTreeModel();
    const auto checkLoaded = [this, model]() {
        if (model->isCollectionTreeFetched())
            setState(State::Ready);
        m_changed.start();
    };
    connect(model, &Akonadi::EntityTreeModel::collectionTreeFetched, this, checkLoaded);
    connect(model, &Akonadi::EntityTreeModel::collectionPopulated, this, checkLoaded);
    setState(model->isCollectionTreeFetched() ? State::Ready : State::Loading);
}

QList<Agenda::Item> AkonadiAgendaSource::items(const QDate &day) const
{
    QList<Agenda::Item> result;
    if (!m_calendar || m_state == State::Unavailable)
        return result;
    const QTimeZone zone = QTimeZone::systemTimeZone();
    const QDateTime from(day, QTime(0, 0), zone);
    const QDateTime to(day.addDays(1), QTime(0, 0), zone);
    const auto sourceOf = [this](const KCalendarCore::Incidence::Ptr &incidence) {
        const Akonadi::Item item = m_calendar->item(incidence);
        return Akonadi::CalendarUtils::displayName(m_calendar.data(), m_calendar->collection(item.parentCollection().id()));
    };

    QSet<QString> seen;
    KCalendarCore::OccurrenceIterator occurrences(*m_calendar, from.addDays(-lookBackDays), to);
    while (occurrences.hasNext()) {
        occurrences.next();
        const KCalendarCore::Incidence::Ptr incidence = occurrences.incidence();
        if (!incidence || incidence->type() != KCalendarCore::IncidenceBase::TypeEvent)
            continue;
        const QDateTime start = occurrences.occurrenceStartDate().toTimeZone(zone);
        QDateTime end = occurrences.occurrenceEndDate();
        end = end.isValid() ? end.toTimeZone(zone) : start;
        const bool allDay = incidence->allDay();
        // All-day events end on their last day; timed ones at a moment, which is not part of the event
        const bool overlaps = allDay ? (occurrences.occurrenceStartDate().date() <= day && occurrences.occurrenceEndDate().date() >= day)
                                     : (start < to && (end > from || (start == end && start >= from)));
        if (!overlaps)
            continue;
        const QString key = incidence->uid() + QLatin1Char('\n') + start.toString(Qt::ISODate);
        if (seen.contains(key))
            continue;
        seen.insert(key);
        Agenda::Item item;
        item.kind = Agenda::Kind::Event;
        item.title = incidence->summary();
        item.allDay = allDay;
        item.start = allDay ? QDateTime(occurrences.occurrenceStartDate().date(), QTime(0, 0), zone) : start;
        item.end = allDay ? QDateTime(occurrences.occurrenceEndDate().date(), QTime(0, 0), zone) : end;
        item.location = incidence->location();
        item.joinUrl = joinUrlOf(incidence);
        item.incidenceUid = incidence->uid();
        item.source = sourceOf(incidence);
        result << item;
    }

    const auto todos = m_calendar->rawTodos();
    for (const auto &todo : todos) {
        if (todo->isCompleted() || !todo->hasDueDate())
            continue;
        const QDateTime due = todo->dtDue().toTimeZone(zone);
        const QDate dueDay = todo->allDay() ? todo->dtDue().date() : due.date();
        if (dueDay > day)
            continue;
        Agenda::Item item;
        item.kind = Agenda::Kind::Task;
        item.title = todo->summary();
        item.allDay = todo->allDay();
        item.start = todo->allDay() ? QDateTime(dueDay, QTime(0, 0), zone) : due;
        item.incidenceUid = todo->uid();
        item.source = sourceOf(todo);
        result << item;
    }
    return result;
}

}
