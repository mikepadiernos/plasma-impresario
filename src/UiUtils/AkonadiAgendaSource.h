/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QSharedPointer>
#include <QTimer>

#include "Agenda.h"

namespace Akonadi {
class ETMCalendar;
}

namespace UiUtils {

/** @short Events and tasks of the KDE PIM calendars, read through Akonadi

Akonadi is never started from here: while its server is not running, the source is unavailable, and it starts
reading once the server runs. Every calendar of the user is read, and nothing is ever written.
*/
class AkonadiAgendaSource : public AgendaSource
{
    Q_OBJECT
public:
    explicit AkonadiAgendaSource(QObject *parent = nullptr);
    ~AkonadiAgendaSource() override;

    State state() const override;
    QList<Agenda::Item> items(const QDate &day) const override;

private:
    void serverStateChanged();
    void setState(State state);

    QSharedPointer<Akonadi::ETMCalendar> m_calendar;
    State m_state = State::Unavailable;
    /** Items arrive one collection at a time, so coalesce the notifications */
    QTimer m_changed;
};

}
