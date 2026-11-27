/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <functional>
#include <QPointer>

#include "EmbeddedPanel.h"
#include "UiUtils/Agenda.h"

class QLabel;
class QListWidget;
class QMenu;
class QTimer;
class QToolButton;

namespace Gui {

/** @short Embedded panel with the user's day: today's events, open tasks and messages to follow up

Events and tasks come from a read-only UiUtils::AgendaSource; without one, or while it is unavailable, the panel lists
only the messages and says that the calendars are unavailable. The window hands in the flagged and $TODO messages.

In the footer the panel shrinks to one line with the next event and the counts of due tasks and messages, which opens
the whole list as a menu. The day follows the system time zone and changes at midnight.
*/
class AgendaPanel : public EmbeddedPanel
{
    Q_OBJECT
public:
    explicit AgendaPanel(QWidget *parent = nullptr);

    static Descriptor panelDescriptor();

    /** @short Read events and tasks from @arg source, which stays owned by the caller; nullptr when there is none */
    void setCalendarSource(UiUtils::AgendaSource *source);
    /** @short The flagged and $TODO messages, with their source filled in */
    void setMessages(const QList<UiUtils::Agenda::Item> &messages);
    /** @short Use @arg clock instead of the system clock; for tests */
    void setClock(const std::function<QDateTime()> &clock);

    /** Whether events and tasks can be read */
    bool calendarsAvailable() const;
    /** The items as listed */
    QList<UiUtils::Agenda::Item> items() const;
    /** The day shown */
    QDate day() const;
    /** Whether the panel shows its one-line form, in the footer */
    bool isCondensed() const;
    QString condensedText() const;

    /** @short Text which names @arg item for assistive technology: its time, title and source */
    static QString accessibleName(const UiUtils::Agenda::Item &item, const QDate &day);

    bool hasContent() const override;
    QWidget *focusTarget() const override;

public slots:
    /** Read the events and tasks again and list them with the messages */
    void refresh();
    void activateCurrent();
    void joinCurrent();

signals:
    /** The user wants to read @arg message, a message item */
    void messageActivated(const UiUtils::Agenda::Item &message);
    /** The user wants to join the online meeting at @arg url */
    void joinRequested(const QUrl &url);
    /** The user wants to see or edit an event or task in the calendar application */
    void calendarRequested();
    /** The user turned the panel on or off */
    void enabledChanged(bool enabled);

protected:
    void panelEnabledChanged() override;

private:
    void activate(const UiUtils::Agenda::Item &item);
    void updateForm();
    void updateActions();
    void scheduleNextDay();
    void fillMenu(QMenu *menu);
    void showContextMenu(const QPoint &position);
    QDateTime now() const;
    int currentRow() const;

    QWidget *m_body;
    QListWidget *m_list;
    QLabel *m_notice;
    QToolButton *m_condensed;
    QToolButton *m_joinButton;
    QToolButton *m_calendarButton;
    QTimer *m_dayTimer;
    QTimer *m_minuteTimer;
    QPointer<UiUtils::AgendaSource> m_source;
    QList<UiUtils::Agenda::Item> m_messages;
    QList<UiUtils::Agenda::Item> m_items;
    QDate m_day;
    std::function<QDateTime()> m_clock;
};

}
