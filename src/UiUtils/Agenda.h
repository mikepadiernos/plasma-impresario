/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QUrl>

namespace Imap {
namespace Mailbox {
class SearchIndex;
}
}

namespace UiUtils {

/** @short The items of the My Day panel: today's events, open tasks and messages to follow up

Events and tasks come from an AgendaSource, which only reads calendars. Messages come from the offline search indexes
and from the folder shown in the message list. This namespace holds the data and the rules for ordering it; the text
shown lives in the GUI.
*/
namespace Agenda {

enum class Kind {
    Event,
    Task,
    Message,
};

struct Item {
    Kind kind = Kind::Event;
    QString title;
    /** Events: start of the occurrence; tasks: when they are due; messages: invalid */
    QDateTime start;
    /** Events: end of the occurrence, the last day for all-day events; otherwise invalid */
    QDateTime end;
    /** All-day events, and tasks due on a day without a time */
    bool allDay = false;
    /** Events: the location as entered */
    QString location;
    /** Events and tasks: the calendar; messages: the account and folder, filled in by the GUI */
    QString source;
    /** Events: the link of an online meeting, from its conference data, URL, location or description */
    QUrl joinUrl;
    /** Events and tasks: the iCalendar UID */
    QString incidenceUid;

    /** Messages: where the message is */
    QString accountId;
    QString mailbox;
    uint uidValidity = 0;
    uint uid = 0;
    /** Messages: the Message-ID header, with or without angle brackets */
    QString messageId;
    QString sender;
    /** Messages: day of the Date header; invalid when unknown */
    QDate sent;
    /** Messages: marked with the $TODO keyword rather than, or besides, flagged */
    bool toDo = false;
};

/** @short Whether @arg item is a task which was due before @arg day */
bool isOverdue(const Item &item, const QDate &day);

/** @short The items in the order the panel lists them

All-day events, then timed events by their start, overdue tasks and the tasks due on @arg day by when they are due,
and last the messages, newest first. Items which compare equal keep their order.
*/
QList<Item> sorted(QList<Item> items, const QDate &day);

/** @short The first timed event of @arg items which has not ended at @arg now, or -1 */
int nextEvent(const QList<Item> &items, const QDateTime &now);
/** @short Tasks of @arg items due on @arg day or before */
int dueTasks(const QList<Item> &items, const QDate &day);
/** @short Messages of @arg items */
int messages(const QList<Item> &items);

/** @short Milliseconds from @arg now until the next day begins in the time zone of @arg now, at least one */
qint64 msecsUntilNextDay(const QDateTime &now);

/** @short The first http or https link in @arg text, which may be a location or a description; empty if none */
QUrl findMeetingUrl(const QString &text);

/** @short An account whose offline index lists flagged messages */
struct MessageSource {
    QString accountId;
    const Imap::Mailbox::SearchIndex *index = nullptr;
};

/** @short What the message list shows of one folder */
struct ShownFolder {
    QString accountId;
    QString mailbox;
    uint uidValidity = 0;
    /** Every message the list has loaded, whether it is to follow up or not */
    QSet<uint> loaded;
    /** The loaded messages which are flagged or marked with $TODO */
    QList<Item> followUps;
};

/** @short The newest flagged or $TODO messages of @arg sources and @arg shown, at most @arg limit

The message list knows the current flags of what it shows, so it decides for its folder: indexed messages which it
has loaded are replaced by what it says, and those of an older UIDVALIDITY are dropped. Each message is listed once.
*/
QList<Item> followUps(const QList<MessageSource> &sources, const ShownFolder &shown, int limit);

/** The most messages listed */
constexpr int maximumMessages = 50;

}

/** @short Read-only access to the events and tasks of the user's calendars */
class AgendaSource : public QObject
{
    Q_OBJECT
public:
    enum class State {
        /** The calendars cannot be read, for example because the PIM service is not running */
        Unavailable,
        Loading,
        Ready,
    };

    using QObject::QObject;

    virtual State state() const = 0;
    /** @short Events overlapping @arg day and open tasks due on @arg day or before, in the system time zone */
    virtual QList<Agenda::Item> items(const QDate &day) const = 0;

signals:
    /** The state, or the events and tasks, changed */
    void changed();
};

}
