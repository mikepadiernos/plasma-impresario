/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QTest>

#include "UiUtils/OutputSurfaces.h"

using namespace UiUtils::OutputSurfaces;

class OutputSurfacesTest : public QObject
{
    Q_OBJECT

private slots:
    void route_data()
    {
        QTest::addColumn<int>("event");
        QTest::addColumn<bool>("active");
        QTest::addColumn<bool>("shown");
        QTest::addColumn<bool>("quiet");
        QTest::addColumn<int>("expected");

        const auto add = [](const char *name, Event event, bool active, bool shown, bool quiet, Surfaces expected) {
            QTest::newRow(name) << static_cast<int>(event) << active << shown << quiet << static_cast<int>(expected.toInt());
        };
        add("progress shown", Event::SyncProgress, true, true, false, Surface::Activity | Surface::StatusBar);
        add("progress hidden", Event::SyncProgress, true, false, false, Surface::Activity);
        add("failure in use", Event::SyncFailed, true, true, false, Surface::Activity | Surface::InlineBar | Surface::StatusBar);
        add("failure away", Event::SyncFailed, false, true, false,
            Surface::Activity | Surface::InlineBar | Surface::DesktopNotification);
        add("failure background", Event::AccountAttention, true, false, false, Surface::Activity | Surface::DesktopNotification);
        add("failure background quiet", Event::AccountAttention, false, false, true, Surface::Activity);
        add("new mail", Event::NewMail, true, true, false, Surface::Activity | Surface::TaskManager | Surface::DesktopNotification);
        add("new mail quiet", Event::NewMail, false, false, true, Surface::Activity | Surface::TaskManager);
        add("rule applied in use", Event::RuleApplied, true, true, false, Surface::Activity | Surface::StatusBar);
        add("rule applied away", Event::RuleApplied, false, true, false, Surface::Activity);
        add("rule failed background", Event::RuleFailed, false, false, false, Surface::Activity | Surface::DesktopNotification);
        add("rule review shown", Event::RuleNeedsReview, false, true, false, Surface::Activity | Surface::InlineBar);
        add("rule review hidden", Event::RuleNeedsReview, true, false, false, Surface::Activity);
        add("job", Event::BackgroundJob, false, false, false, Surface::Activity | Surface::StatusBar);
    }

    void route()
    {
        QFETCH(int, event);
        QFETCH(bool, active);
        QFETCH(bool, shown);
        QFETCH(bool, quiet);
        QFETCH(int, expected);
        const Surfaces surfaces = UiUtils::OutputSurfaces::route(static_cast<Event>(event), Context{active, shown, quiet});
        QCOMPARE(surfaces.toInt(), expected);
    }

    void progressNeverReachesTheDesktop()
    {
        for (const Event event : {Event::SyncProgress, Event::BackgroundJob, Event::RuleApplied, Event::ScanFinding}) {
            for (int bits = 0; bits < 8; ++bits) {
                const Context context{bool(bits & 1), bool(bits & 2), bool(bits & 4)};
                QVERIFY(!UiUtils::OutputSurfaces::route(event, context).testFlag(Surface::DesktopNotification));
                QVERIFY(UiUtils::OutputSurfaces::route(event, context).testFlag(Surface::Activity));
            }
        }
    }

    void names()
    {
        QCOMPARE(surfaceName(Surface::DesktopNotification), QStringLiteral("Desktop notification"));
        QCOMPARE(eventName(Event::RuleFailed), QStringLiteral("Rule failed"));
    }
};

QTEST_GUILESS_MAIN(OutputSurfacesTest)

#include "test_OutputSurfaces.moc"
