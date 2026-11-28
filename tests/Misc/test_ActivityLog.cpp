/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTest>

#include "UiUtils/ActivityLog.h"

using UiUtils::ActivityLog;

namespace
{

ActivityLog::Entry entry(const QString &key, ActivityLog::State state, const QString &title,
                         const QString &account = QStringLiteral("work"))
{
    ActivityLog::Entry e;
    e.key = key;
    e.kind = ActivityLog::Kind::Sync;
    e.state = state;
    e.accountId = account;
    e.title = title;
    return e;
}

}

class ActivityLogTest : public QObject
{
    Q_OBJECT

private slots:
    void newestFirstAndCoalescedByKey()
    {
        ActivityLog log;
        QAbstractItemModelTester tester(&log, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy counts(&log, &ActivityLog::countsChanged);
        log.record(entry(QStringLiteral("sync work"), ActivityLog::State::Running, QStringLiteral("Syncing Work")));
        log.record(entry(QString(), ActivityLog::State::Done, QStringLiteral("Rule moved 2 messages")));
        log.record(entry(QString(), ActivityLog::State::Done, QStringLiteral("Rule moved 1 message")));
        QCOMPARE(log.count(), 3);
        QCOMPARE(log.runningCount(), 1);
        QCOMPARE(log.index(0).data().toString(), QStringLiteral("Rule moved 1 message"));

        // The same key updates its entry and brings it to the top
        auto failed = entry(QStringLiteral("sync work"), ActivityLog::State::Failed, QStringLiteral("Syncing Work"));
        failed.error = QStringLiteral("Connection refused");
        failed.action = ActivityLog::Action::Retry;
        log.record(failed);
        QCOMPARE(log.count(), 3);
        QCOMPARE(log.find(QStringLiteral("sync work")), 0);
        QCOMPARE(log.failedCount(), 1);
        QCOMPARE(log.runningCount(), 0);
        QCOMPARE(log.index(0).data(ActivityLog::ErrorRole).toString(), QStringLiteral("Connection refused"));
        QCOMPARE(log.index(0).data(ActivityLog::ActionRole).toInt(), static_cast<int>(ActivityLog::Action::Retry));
        QVERIFY(log.index(0).data(Qt::ToolTipRole).toString().contains(QStringLiteral("Connection refused")));
        QVERIFY(log.index(0).data(ActivityLog::TimeRole).toDateTime().isValid());
        QCOMPARE(counts.count(), 2);

        QVERIFY(log.finish(QStringLiteral("sync work"), ActivityLog::State::Done));
        QVERIFY(!log.finish(QStringLiteral("unknown"), ActivityLog::State::Done));
        QCOMPARE(log.failedCount(), 0);
        QVERIFY(log.entry(0).error.isEmpty());
    }

    void updatingTheTopEntryKeepsItsRow()
    {
        ActivityLog log;
        log.record(entry(QStringLiteral("k"), ActivityLog::State::Running, QStringLiteral("A")));
        QSignalSpy inserted(&log, &QAbstractItemModel::rowsInserted);
        QSignalSpy changed(&log, &QAbstractItemModel::dataChanged);
        auto progress = entry(QStringLiteral("k"), ActivityLog::State::Running, QStringLiteral("A"));
        progress.progress = 50;
        log.record(progress);
        QCOMPARE(inserted.count(), 0);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(log.index(0).data(ActivityLog::ProgressRole).toInt(), 50);
        log.finish(QStringLiteral("k"), ActivityLog::State::Done);
        QCOMPARE(log.entry(0).progress, 100);
    }

    void capacityDropsOldDoneEntriesBeforeFailures()
    {
        ActivityLog log;
        log.record(entry(QStringLiteral("failure"), ActivityLog::State::Failed, QStringLiteral("Old failure")));
        for (int i = 0; i < ActivityLog::capacity + 10; ++i)
            log.record(entry(QString(), ActivityLog::State::Done, QStringLiteral("Done %1").arg(i)));
        QCOMPARE(log.count(), ActivityLog::capacity);
        QVERIFY(log.find(QStringLiteral("failure")) >= 0);
        QCOMPARE(log.index(0).data().toString(), QStringLiteral("Done %1").arg(ActivityLog::capacity + 9));
    }

    void clearing()
    {
        ActivityLog log;
        log.record(entry(QStringLiteral("a"), ActivityLog::State::Failed, QStringLiteral("A")));
        log.record(entry(QStringLiteral("b"), ActivityLog::State::Done, QStringLiteral("B")));
        log.record(entry(QStringLiteral("c"), ActivityLog::State::Running, QStringLiteral("C"), QStringLiteral("home")));
        log.clearFinished();
        QCOMPARE(log.count(), 2);
        log.removeAccount(QStringLiteral("home"));
        QCOMPARE(log.count(), 1);
        QCOMPARE(log.entry(0).key, QStringLiteral("a"));
        log.clear();
        QCOMPARE(log.count(), 0);
        QCOMPARE(log.failedCount(), 0);
    }

    void retryRunsTheHandlerOnce()
    {
        ActivityLog log;
        QAbstractItemModelTester tester(&log, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QVERIFY(!log.setRetry(QStringLiteral("missing"), []() { }));
        auto failed = entry(QStringLiteral("move"), ActivityLog::State::Failed, QStringLiteral("Moving 3 messages"));
        failed.error = QStringLiteral("Server said NO");
        log.record(failed);
        QCOMPARE(log.entry(0).action, ActivityLog::Action::None);
        QVERIFY(!log.canRetry(QStringLiteral("move")));
        QCOMPARE(log.retryableCount(), 0);

        int runs = 0;
        QVERIFY(log.setRetry(QStringLiteral("move"), [&runs]() { ++runs; }));
        QCOMPARE(log.entry(0).action, ActivityLog::Action::Retry);
        QVERIFY(log.canRetry(QStringLiteral("move")));
        QCOMPARE(log.retryableCount(), 1);

        QVERIFY(log.retry(QStringLiteral("move")));
        QCOMPARE(runs, 1);
        QCOMPARE(log.entry(0).state, ActivityLog::State::Running);
        QVERIFY(log.entry(0).error.isEmpty());
        QCOMPARE(log.entry(0).action, ActivityLog::Action::None);
        QVERIFY(!log.canRetry(QStringLiteral("move")));
        // A handler runs once; the work sets a new one when it fails again
        QVERIFY(!log.retry(QStringLiteral("move")));
        QCOMPARE(runs, 1);
    }

    void recordingWithoutRetryForgetsTheHandler()
    {
        ActivityLog log;
        auto failed = entry(QStringLiteral("sync work"), ActivityLog::State::Failed, QStringLiteral("Connection"));
        log.record(failed);
        QVERIFY(log.setRetry(QStringLiteral("sync work"), []() { }));
        // Failing again in the same way keeps the handler
        failed.action = ActivityLog::Action::Retry;
        log.record(failed);
        QVERIFY(log.canRetry(QStringLiteral("sync work")));
        // Succeeding resolves the failure
        log.record(entry(QStringLiteral("sync work"), ActivityLog::State::Done, QStringLiteral("Connection")));
        QVERIFY(!log.canRetry(QStringLiteral("sync work")));
        QCOMPARE(log.entry(0).action, ActivityLog::Action::None);
    }

    void retryAllAndRemoval()
    {
        ActivityLog log;
        QStringList ran;
        for (const QString &key : {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}) {
            log.record(entry(key, ActivityLog::State::Failed, key,
                key == QStringLiteral("c") ? QStringLiteral("home") : QStringLiteral("work")));
            log.setRetry(key, [&ran, key]() { ran << key; });
        }
        log.record(entry(QStringLiteral("d"), ActivityLog::State::Failed, QStringLiteral("d")));
        QCOMPARE(log.retryableCount(), 3);
        log.removeAccount(QStringLiteral("home"));
        QVERIFY(!log.canRetry(QStringLiteral("c")));
        QCOMPARE(log.retryAll(), 2);
        ran.sort();
        QCOMPARE(ran, (QStringList{QStringLiteral("a"), QStringLiteral("b")}));
        QCOMPARE(log.retryableCount(), 0);
        QCOMPARE(log.failedCount(), 1);
        log.setRetry(QStringLiteral("d"), []() { });
        log.clear();
        QVERIFY(!log.canRetry(QStringLiteral("d")));
    }

    void names()
    {
        QCOMPARE(ActivityLog::stateName(ActivityLog::State::Failed), QStringLiteral("Failed"));
        QCOMPARE(ActivityLog::actionName(ActivityLog::Action::SignIn), QStringLiteral("Sign In"));
        QVERIFY(ActivityLog::actionName(ActivityLog::Action::None).isEmpty());
        ActivityLog log;
        QVERIFY(log.roleNames().contains(ActivityLog::StateRole));
    }
};

QTEST_GUILESS_MAIN(ActivityLogTest)

#include "test_ActivityLog.moc"
