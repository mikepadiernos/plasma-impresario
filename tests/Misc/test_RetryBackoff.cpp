/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QTest>

#include "Common/RetryBackoff.h"

using Common::RetryBackoff;

class RetryBackoffTest : public QObject
{
    Q_OBJECT

private slots:
    void doublesUpToTheLimit()
    {
        RetryBackoff backoff(5000, 300000, 0.2);
        // The middle of the jitter range gives the base delay
        backoff.setRandomSource([]() { return 0.5; });
        const QList<int> expected{5000, 10000, 20000, 40000, 80000, 160000, 300000, 300000};
        for (int delay : expected) {
            QCOMPARE(backoff.baseDelay(), delay);
            QCOMPARE(backoff.nextDelay(), delay);
        }
        QCOMPARE(backoff.attempts(), expected.size());
        backoff.reset();
        QCOMPARE(backoff.attempts(), 0);
        QCOMPARE(backoff.nextDelay(), 5000);
    }

    void jitterStaysWithinBounds()
    {
        RetryBackoff backoff(5000, 300000, 0.2);
        backoff.setRandomSource([]() { return 0.0; });
        // The first delay is never shortened
        QCOMPARE(backoff.nextDelay(), 5000);
        QCOMPARE(backoff.nextDelay(), 8000);
        backoff.setRandomSource([]() { return 0.999999; });
        QVERIFY(qAbs(backoff.nextDelay() - 24000) <= 1);
        for (int i = 0; i < 10; ++i)
            QVERIFY(backoff.nextDelay() <= 300000);
    }

    void defaultRandomSpreadsDelays()
    {
        QSet<int> seen;
        for (int i = 0; i < 20; ++i) {
            RetryBackoff backoff(5000, 300000, 0.2);
            backoff.nextDelay();
            const int delay = backoff.nextDelay();
            QVERIFY(delay >= 8000 && delay <= 12000);
            seen.insert(delay);
        }
        QVERIFY(seen.size() > 1);
    }

    void unusableArgumentsAreCorrected()
    {
        RetryBackoff backoff(0, -5, 3.0);
        QCOMPARE(backoff.initialDelay(), 1);
        QCOMPARE(backoff.maximumDelay(), 1);
        QCOMPARE(backoff.nextDelay(), 1);
    }
};

QTEST_GUILESS_MAIN(RetryBackoffTest)
#include "test_RetryBackoff.moc"
