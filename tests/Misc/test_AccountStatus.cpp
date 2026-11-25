/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QSet>
#include <QSignalSpy>
#include <QtTest>
#include "test_AccountStatus.h"
#include "Common/AccountStatus.h"

using namespace Common;

Q_DECLARE_METATYPE(AccountStatusKind)

void AccountStatusTest::unknownAccountIsIdle()
{
    AccountStatusTracker tracker;
    QCOMPARE(tracker.status(QStringLiteral("account-1")).kind, AccountStatusKind::Idle);
    QVERIFY(tracker.status(QStringLiteral("account-1")).detail.isEmpty());
}

void AccountStatusTest::recordsStatusAndDetail()
{
    AccountStatusTracker tracker;
    tracker.setStatus(QStringLiteral("a"), AccountStatusKind::NetworkError, QStringLiteral("Host lookup failed"));
    tracker.setStatus(QStringLiteral("b"), AccountStatusKind::Connected);
    QCOMPARE(tracker.status(QStringLiteral("a")).kind, AccountStatusKind::NetworkError);
    QCOMPARE(tracker.status(QStringLiteral("a")).detail, QStringLiteral("Host lookup failed"));
    QCOMPARE(tracker.status(QStringLiteral("b")).kind, AccountStatusKind::Connected);
}

void AccountStatusTest::emitsOnlyOnChange()
{
    AccountStatusTracker tracker;
    QSignalSpy spy(&tracker, &AccountStatusTracker::statusChanged);
    tracker.setStatus(QStringLiteral("a"), AccountStatusKind::Connecting);
    tracker.setStatus(QStringLiteral("a"), AccountStatusKind::Connecting);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("a"));
    tracker.setStatus(QStringLiteral("a"), AccountStatusKind::Connecting, QStringLiteral("retrying"));
    QCOMPARE(spy.count(), 2);
    tracker.setStatus(QStringLiteral("a"), AccountStatusKind::Idle);
    QCOMPARE(spy.count(), 3);
}

void AccountStatusTest::forgetResetsToIdle()
{
    AccountStatusTracker tracker;
    tracker.setStatus(QStringLiteral("a"), AccountStatusKind::AuthFailed, QStringLiteral("rejected"));
    QSignalSpy spy(&tracker, &AccountStatusTracker::statusChanged);
    tracker.forget(QStringLiteral("a"));
    QCOMPARE(tracker.status(QStringLiteral("a")).kind, AccountStatusKind::Idle);
    QVERIFY(tracker.accountsNeedingAttention().isEmpty());
    QCOMPARE(spy.count(), 1);
    tracker.forget(QStringLiteral("a"));
    QCOMPARE(spy.count(), 1);
}

void AccountStatusTest::attentionKinds_data()
{
    QTest::addColumn<AccountStatusKind>("kind");
    QTest::addColumn<bool>("attention");
    QTest::newRow("idle") << AccountStatusKind::Idle << false;
    QTest::newRow("connecting") << AccountStatusKind::Connecting << false;
    QTest::newRow("connected") << AccountStatusKind::Connected << false;
    QTest::newRow("offline") << AccountStatusKind::Offline << false;
    QTest::newRow("network error") << AccountStatusKind::NetworkError << true;
    QTest::newRow("auth required") << AccountStatusKind::AuthRequired << true;
    QTest::newRow("auth failed") << AccountStatusKind::AuthFailed << true;
}

void AccountStatusTest::attentionKinds()
{
    QFETCH(AccountStatusKind, kind);
    QFETCH(bool, attention);
    QCOMPARE(AccountStatusTracker::needsAttention(kind), attention);
}

void AccountStatusTest::listsAccountsNeedingAttention()
{
    AccountStatusTracker tracker;
    tracker.setStatus(QStringLiteral("c"), AccountStatusKind::AuthRequired);
    tracker.setStatus(QStringLiteral("a"), AccountStatusKind::NetworkError);
    tracker.setStatus(QStringLiteral("b"), AccountStatusKind::Connected);
    QCOMPARE(tracker.accountsNeedingAttention(), (QStringList{QStringLiteral("a"), QStringLiteral("c")}));
}

void AccountStatusTest::labelsAreDistinct()
{
    QSet<QString> labels;
    for (auto kind : {AccountStatusKind::Idle, AccountStatusKind::Connecting, AccountStatusKind::Connected,
                      AccountStatusKind::Offline, AccountStatusKind::NetworkError, AccountStatusKind::AuthRequired,
                      AccountStatusKind::AuthFailed}) {
        const QString label = AccountStatusTracker::label(kind);
        QVERIFY(!label.isEmpty());
        labels.insert(label);
    }
    QCOMPARE(labels.size(), 7);
}

QTEST_GUILESS_MAIN(AccountStatusTest)
