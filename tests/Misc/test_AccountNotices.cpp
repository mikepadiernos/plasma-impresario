/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QPointer>
#include <QTest>

#include "UiUtils/AccountNotices.h"

using UiUtils::AccountNotices;

namespace
{

/** Closes and deletes itself like a KNotification */
class Notice : public QObject
{
    Q_OBJECT

public:
    int closed = 0;

public slots:
    void close()
    {
        ++closed;
        deleteLater();
    }
};

}

class AccountNoticesTest : public QObject
{
    Q_OBJECT

private slots:
  void newerNoticeReplacesOlder()
  {
    AccountNotices notices;
    auto *first = new Notice;
    QPointer<Notice> firstAlive(first);
    notices.show(QStringLiteral("a"), first);
    QVERIFY(notices.isShown(QStringLiteral("a")));
    QVERIFY(!notices.isShown(QStringLiteral("b")));

    auto *second = new Notice;
    notices.show(QStringLiteral("a"), second);
    QCOMPARE(first->closed, 1);
    QCOMPARE(second->closed, 0);
    QVERIFY(notices.isShown(QStringLiteral("a")));
    QTRY_VERIFY(!firstAlive);

    // Showing the same notice again does not close it
    notices.show(QStringLiteral("a"), second);
    QCOMPARE(second->closed, 0);
    notices.close(QStringLiteral("a"));
    QVERIFY(!notices.isShown(QStringLiteral("a")));
  }

  void closesOnlyTheAccountsNotice()
  {
    AccountNotices notices;
    auto *a = new Notice;
    auto *b = new Notice;
    notices.show(QStringLiteral("a"), a);
    notices.show(QStringLiteral("b"), b);
    notices.close(QStringLiteral("a"));
    QCOMPARE(a->closed, 1);
    QCOMPARE(b->closed, 0);
    QVERIFY(!notices.isShown(QStringLiteral("a")));
    QVERIFY(notices.isShown(QStringLiteral("b")));
    // Closing twice, or an account without a notice, does nothing
    notices.close(QStringLiteral("a"));
    notices.close(QStringLiteral("c"));
    QCOMPARE(a->closed, 1);
    notices.close(QStringLiteral("b"));
  }

  void noticeClosedElsewhereIsForgotten()
  {
    AccountNotices notices;
    auto *a = new Notice;
    notices.show(QStringLiteral("a"), a);
    // The user dismissed it, or it timed out
    delete a;
    QVERIFY(!notices.isShown(QStringLiteral("a")));
    notices.close(QStringLiteral("a"));
  }

  void everyNoticeClosesWithTheOwner()
  {
    QPointer<Notice> a = new Notice;
    QPointer<Notice> b = new Notice;
    {
      AccountNotices notices;
      notices.show(QStringLiteral("a"), a);
      notices.show(QStringLiteral("b"), b);
    }
    QCOMPARE(a->closed, 1);
    QCOMPARE(b->closed, 1);
    QTRY_VERIFY(!a && !b);

    AccountNotices notices;
    auto *c = new Notice;
    notices.show(QStringLiteral("c"), c);
    notices.closeAll();
    QCOMPARE(c->closed, 1);
    QVERIFY(!notices.isShown(QStringLiteral("c")));
  }
};

QTEST_GUILESS_MAIN(AccountNoticesTest)

#include "test_AccountNotices.moc"
