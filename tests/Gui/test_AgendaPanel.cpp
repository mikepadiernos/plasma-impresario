/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/AgendaPanel.h"

#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMap>
#include <QMenu>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QToolButton>
#include <QVBoxLayout>

using Gui::AgendaPanel;
using Gui::EmbeddedPanel;
using UiUtils::Agenda::Item;
using UiUtils::Agenda::Kind;

namespace {

const QDate today(2026, 11, 27);

QDateTime at(const QDate &day, int hour, int minute = 0)
{
    return QDateTime(day, QTime(hour, minute));
}

QString shortTime(int hour, int minute = 0)
{
    return QLocale().toString(QTime(hour, minute), QLocale::ShortFormat);
}

/** Events and tasks of each day, without Akonadi */
class FakeSource : public UiUtils::AgendaSource
{
public:
    using UiUtils::AgendaSource::AgendaSource;

    State state() const override { return m_state; }
    QList<Item> items(const QDate &day) const override
    {
        asked << day;
        return m_items.value(day);
    }

    void set(State state, const QMap<QDate, QList<Item>> &items = {})
    {
        m_state = state;
        m_items = items;
        emit changed();
    }

    mutable QList<QDate> asked;

private:
    State m_state = State::Ready;
    QMap<QDate, QList<Item>> m_items;
};

Item meeting(const QString &title, const QDateTime &start, const QDateTime &end, const QString &calendar,
           const QUrl &joinUrl = QUrl())
{
    Item item;
    item.kind = Kind::Event;
    item.title = title;
    item.start = start;
    item.end = end;
    item.source = calendar;
    item.joinUrl = joinUrl;
    item.incidenceUid = title;
    return item;
}

Item task(const QString &title, const QDateTime &due, const QString &calendar)
{
    Item item;
    item.kind = Kind::Task;
    item.title = title;
    item.start = due;
    item.source = calendar;
    item.incidenceUid = title;
    return item;
}

Item mail(const QString &title, uint uid, bool toDo = false)
{
    Item item;
    item.kind = Kind::Message;
    item.title = title;
    item.accountId = QStringLiteral("work");
    item.mailbox = QStringLiteral("INBOX");
    item.uidValidity = 7;
    item.uid = uid;
    item.sender = QStringLiteral("Alice");
    item.sent = today.addDays(-1);
    item.toDo = toDo;
    item.source = QStringLiteral("Work, INBOX");
    return item;
}

QStringList titles(const QList<Item> &items)
{
    QStringList out;
    for (const Item &item : items)
        out << item.title;
    return out;
}

/** Hosts the panel in a shown window, like a shell column does */
struct Host {
    QWidget window;
    AgendaPanel *panel;
    Host()
        : panel(new AgendaPanel)
    {
        auto *layout = new QVBoxLayout(&window);
        layout->addWidget(new QWidget(&window), 1);
        layout->addWidget(panel);
        panel->setPanelEnabled(true);
        window.resize(500, 700);
        window.show();
    }

    template<typename T>
    T *child(const QString &name) const
    {
        return panel->findChild<T *>(name);
    }

    QListWidget *list() const { return child<QListWidget>(QStringLiteral("agendaPanelList")); }
    QString placeholder() const { return child<QLabel>(QStringLiteral("agendaPanelPlaceholder"))->text(); }
};

}

class AgendaPanelTest : public QObject
{
    Q_OBJECT
private slots:
    void testDescriptor()
    {
        const auto descriptor = AgendaPanel::panelDescriptor();
        QCOMPARE(descriptor.id, QStringLiteral("Agenda"));
        QCOMPARE(descriptor.configKey, QStringLiteral("AgendaPanel"));
        QCOMPARE(descriptor.title, QStringLiteral("My Day"));
        QVERIFY(!descriptor.defaultEnabled);
        QCOMPARE(descriptor.defaultPlacement, EmbeddedPanel::Placement::RightColumn);
    }

    void testWithoutCalendars()
    {
        Host host;
        host.panel->setClock([]() { return at(today, 10); });
        QVERIFY(!host.panel->calendarsAvailable());
        QVERIFY(!host.panel->hasContent());
        QVERIFY(host.placeholder().contains(QLatin1String("Calendars are unavailable")));
        QVERIFY(!host.child<QToolButton>(QStringLiteral("agendaPanelCalendar"))->isEnabled());

        // Messages are listed all the same, with a notice instead of an empty schedule
        host.panel->setMessages({mail(QStringLiteral("Contract"), 4)});
        QCOMPARE(titles(host.panel->items()), QStringList{QStringLiteral("Contract")});
        auto *notice = host.child<QLabel>(QStringLiteral("agendaPanelNotice"));
        QVERIFY(!notice->isHidden());
        QVERIFY(notice->text().contains(QLatin1String("only flagged messages")));

        // So does a source which cannot read the calendars
        FakeSource source;
        source.set(UiUtils::AgendaSource::State::Unavailable, {{today, {meeting(QStringLiteral("Hidden"), at(today, 9), at(today, 10), QStringLiteral("Work"))}}});
        host.panel->setCalendarSource(&source);
        QCOMPARE(titles(host.panel->items()), QStringList{QStringLiteral("Contract")});
        QVERIFY(!notice->isHidden());

        source.set(UiUtils::AgendaSource::State::Loading);
        QVERIFY(notice->text().contains(QLatin1String("Loading")));
        host.panel->setMessages({});
        QVERIFY(host.placeholder().contains(QLatin1String("Loading")));

        source.set(UiUtils::AgendaSource::State::Ready);
        QVERIFY(host.panel->calendarsAvailable());
        QVERIFY(notice->isHidden());
        QVERIFY(host.placeholder().contains(QLatin1String("Nothing scheduled")));
        QVERIFY(host.child<QToolButton>(QStringLiteral("agendaPanelCalendar"))->isEnabled());
    }

    void testOrderAndAccessibleNames()
    {
        Host host;
        host.panel->setClock([]() { return at(today, 10, 30); });
        FakeSource source;
        source.set(UiUtils::AgendaSource::State::Ready,
                   {{today,
                     {meeting(QStringLiteral("Review"), at(today, 14), at(today, 15), QStringLiteral("Work"),
                            QUrl(QStringLiteral("https://meet.example.org/review"))),
                      task(QStringLiteral("Expenses"), at(today.addDays(-2), 17), QStringLiteral("Personal")),
                      meeting(QStringLiteral("Standup"), at(today, 9), at(today, 9, 15), QStringLiteral("Work"))}}});
        host.panel->setCalendarSource(&source);
        host.panel->setMessages({mail(QStringLiteral("Contract"), 4), mail(QStringLiteral("Invoice"), 5, true)});
        QCOMPARE(source.asked.constLast(), today);
        QCOMPARE(host.panel->day(), today);

        QCOMPARE(titles(host.panel->items()), (QStringList{QStringLiteral("Standup"), QStringLiteral("Review"), QStringLiteral("Expenses"),
                                                          QStringLiteral("Contract"), QStringLiteral("Invoice")}));
        QListWidget *list = host.list();
        QCOMPARE(list->count(), 5);
        QCOMPARE(host.panel->focusTarget(), list);
        QCOMPARE(list->item(1)->data(Qt::AccessibleTextRole).toString(),
                 QStringLiteral("%1–%2, Review, Work").arg(shortTime(14), shortTime(15)));
        QCOMPARE(AgendaPanel::accessibleName(host.panel->items().at(1), today), list->item(1)->data(Qt::AccessibleTextRole).toString());
        QVERIFY(list->item(2)->data(Qt::AccessibleTextRole).toString().startsWith(QLatin1String("Overdue since")));
        QVERIFY(list->item(2)->data(Qt::AccessibleTextRole).toString().endsWith(QLatin1String(", Expenses, Personal")));
        QCOMPARE(list->item(3)->data(Qt::AccessibleTextRole).toString(), QStringLiteral("Flagged, Contract, Work, INBOX"));
        QCOMPARE(list->item(4)->data(Qt::AccessibleTextRole).toString(), QStringLiteral("To do, Invoice, Work, INBOX"));
        QVERIFY(list->item(1)->toolTip().contains(QLatin1String("https://meet.example.org/review")));
        // The standup is over
        QCOMPARE(list->item(0)->foreground().color(), host.panel->palette().color(QPalette::Disabled, QPalette::Text));

        // The selection stays on the same item when the list is filled again
        list->setCurrentRow(3);
        source.set(UiUtils::AgendaSource::State::Ready, {{today, {meeting(QStringLiteral("Lunch"), at(today, 12), at(today, 13), QStringLiteral("Work"))}}});
        QCOMPARE(titles(host.panel->items()), (QStringList{QStringLiteral("Lunch"), QStringLiteral("Contract"), QStringLiteral("Invoice")}));
        QCOMPARE(list->currentRow(), 1);
    }

    void testActions()
    {
        Host host;
        host.panel->setClock([]() { return at(today, 10, 30); });
        FakeSource source;
        const QUrl link(QStringLiteral("https://meet.example.org/review"));
        source.set(UiUtils::AgendaSource::State::Ready,
                   {{today, {meeting(QStringLiteral("Review"), at(today, 14), at(today, 15), QStringLiteral("Work"), link),
                             task(QStringLiteral("Expenses"), at(today, 17), QStringLiteral("Personal"))}}});
        host.panel->setCalendarSource(&source);
        host.panel->setMessages({mail(QStringLiteral("Contract"), 4)});
        QSignalSpy message(host.panel, &AgendaPanel::messageActivated);
        QSignalSpy join(host.panel, &AgendaPanel::joinRequested);
        QSignalSpy calendar(host.panel, &AgendaPanel::calendarRequested);
        QListWidget *list = host.list();
        auto *joinButton = host.child<QToolButton>(QStringLiteral("agendaPanelJoin"));
        list->setFocus();

        // From the keyboard, in time order
        QCOMPARE(list->currentRow(), 0);
        QVERIFY(joinButton->isEnabled());
        joinButton->click();
        QCOMPARE(join.count(), 1);
        QCOMPARE(join.constFirst().constFirst().toUrl(), link);

        QTest::keyClick(list, Qt::Key_Down);
        QCOMPARE(list->currentRow(), 1);
        QVERIFY(!joinButton->isEnabled());
        QTest::keyClick(list, Qt::Key_Return);
        // Tasks and events are edited in the calendar application
        QCOMPARE(calendar.count(), 1);
        QCOMPARE(message.count(), 0);

        QTest::keyClick(list, Qt::Key_Down);
        QTest::keyClick(list, Qt::Key_Return);
        QCOMPARE(message.count(), 1);
        const auto opened = message.constFirst().constFirst().value<Item>();
        QCOMPARE(opened.uid, 4u);
        QCOMPARE(opened.mailbox, QStringLiteral("INBOX"));
        QCOMPARE(opened.accountId, QStringLiteral("work"));

        host.child<QToolButton>(QStringLiteral("agendaPanelCalendar"))->click();
        QCOMPARE(calendar.count(), 2);

        QSignalSpy enabled(host.panel, &AgendaPanel::enabledChanged);
        host.panel->setPanelEnabled(false);
        QCOMPARE(enabled.count(), 1);
        QCOMPARE(enabled.constFirst().constFirst().toBool(), false);
    }

    void testCondensed()
    {
        QDateTime now = at(today, 8);
        Host host;
        host.panel->setClock([&now]() { return now; });
        FakeSource source;
        source.set(UiUtils::AgendaSource::State::Ready,
                   {{today, {meeting(QStringLiteral("Standup"), at(today, 9), at(today, 9, 15), QStringLiteral("Work")),
                             task(QStringLiteral("Expenses"), at(today, 17), QStringLiteral("Personal"))}}});
        host.panel->setCalendarSource(&source);
        host.panel->setMessages({mail(QStringLiteral("Contract"), 4), mail(QStringLiteral("Invoice"), 5, true)});
        QVERIFY(!host.panel->isCondensed());
        auto *summary = host.child<QToolButton>(QStringLiteral("agendaPanelSummary"));
        QVERIFY(summary->isHidden());

        host.panel->setPlacement(EmbeddedPanel::Placement::Footer);
        QVERIFY(host.panel->isCondensed());
        QVERIFY(!summary->isHidden());
        QVERIFY(host.list()->isHidden());
        QCOMPARE(host.panel->focusTarget(), summary);
        QCOMPARE(host.panel->condensedText(),
                 QStringLiteral("Next: %1 Standup · 1 task due · 2 to follow up").arg(shortTime(9)));
        QVERIFY(summary->accessibleName().startsWith(QLatin1String("My Day: ")));

        // The whole list is a menu away
        QMenu *menu = summary->menu();
        QCOMPARE(menu->objectName(), QStringLiteral("agendaPanelSummaryMenu"));
        emit menu->aboutToShow();
        QStringList entries;
        for (const QAction *action : menu->actions()) {
            if (!action->isSeparator())
                entries << action->text();
        }
        QCOMPARE(entries.size(), 5);
        QVERIFY(entries.constFirst().endsWith(QLatin1String("Standup")));
        QCOMPARE(entries.constLast(), QStringLiteral("Open in Calendar"));
        QSignalSpy message(host.panel, &AgendaPanel::messageActivated);
        menu->actions().at(2)->trigger();
        QCOMPARE(message.count(), 1);

        now = at(today, 9, 5);
        host.panel->refresh();
        QCOMPARE(host.panel->condensedText(), QStringLiteral("Now: Standup · 1 task due · 2 to follow up"));
        now = at(today, 10);
        host.panel->refresh();
        QCOMPARE(host.panel->condensedText(), QStringLiteral("No more events today · 1 task due · 2 to follow up"));

        host.panel->setCalendarSource(nullptr);
        QCOMPARE(host.panel->condensedText(), QStringLiteral("2 to follow up · calendars unavailable"));

        host.panel->setPlacement(EmbeddedPanel::Placement::LeftColumn);
        QVERIFY(!host.panel->isCondensed());
        QVERIFY(summary->isHidden());
        QVERIFY(!host.list()->isHidden());
    }

    void testMidnight()
    {
        QDateTime now = at(today, 23, 59).addSecs(59).addMSecs(500);
        Host host;
        host.panel->setClock([&now]() { return now; });
        FakeSource source;
        const QDate tomorrow = today.addDays(1);
        source.set(UiUtils::AgendaSource::State::Ready,
                   {{today, {meeting(QStringLiteral("Today"), at(today, 9), at(today, 10), QStringLiteral("Work"))}},
                    {tomorrow, {meeting(QStringLiteral("Tomorrow"), at(tomorrow, 9), at(tomorrow, 10), QStringLiteral("Work"))}}});
        host.panel->setCalendarSource(&source);
        QCOMPARE(host.panel->day(), today);
        QCOMPARE(titles(host.panel->items()), QStringList{QStringLiteral("Today")});

        // The day changes when the clock passes midnight, without anything else happening
        now = at(tomorrow, 0).addMSecs(100);
        QTRY_COMPARE_WITH_TIMEOUT(host.panel->day(), tomorrow, 5000);
        QCOMPARE(titles(host.panel->items()), QStringList{QStringLiteral("Tomorrow")});
        QCOMPARE(source.asked.constLast(), tomorrow);
    }
};

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    AgendaPanelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_AgendaPanel.moc"
