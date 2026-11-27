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
