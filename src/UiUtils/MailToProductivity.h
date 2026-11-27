/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

namespace UiUtils {

/** @short A message turned into a calendar event or a note, for the mail-to-calendar and mail-to-notes actions

Nothing is saved here: the event is handed to the calendar application, which shows it for review before it is
saved, and the note is written as a Markdown file to the notes folder, as Marknote keeps its notes.
*/
namespace MailToProductivity {

struct Message {
    QString subject;
    QString from;
    QStringList to;
    QDateTime date;
    /** Message-ID without angle brackets */
    QString messageId;
    QString accountName;
    QString folder;
    /** The plain text of the message, or an excerpt of it */
    QString text;
};

/** @short @arg text escaped as an iCalendar TEXT value (RFC 5545, 3.3.11) */
QString escapeText(const QString &text);
/** @short @arg line folded to lines of at most 75 octets, each continuation starting with a space (RFC 5545, 3.1) */
QByteArray fold(const QByteArray &line);

/** @short The first full hour after @arg now, when an event from a message starts unless the user changes it */
QDateTime suggestedStart(const QDateTime &now);

/** @short An iCalendar object with one event about @arg message, lasting @arg minutes from @arg start */
QByteArray calendarEvent(const Message &message, const QDateTime &start, int minutes, const QString &uid,
                         const QDateTime &stamp);

/** @short The Markdown text of a note about @arg message */
QString noteText(const Message &message);

/** @short A file name for the note, such as "2026-11-27 Quarterly report.md", not among @arg existing

Characters which are not allowed in file names on common file systems are replaced, and the subject is shortened.
*/
QString noteFileName(const Message &message, const QStringList &existing);

}

}
