/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QApplication>
#include <QPushButton>
#include <QStandardPaths>
#include <QTest>
#include <QTreeWidget>

#include <KLocalizedString>

#include "Gui/ActivityDialog.h"

using Gui::ActivityButton;
using Gui::ActivityDialog;
using UiUtils::ActivityLog;

class ActivityDialogTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void showsEntriesAndActions();
    void clearKeepsFailures();
    void buttonCounts();
};

namespace {

ActivityLog::Entry entry(const QString &key, ActivityLog::State state, const QString &title,
                         ActivityLog::Action action = ActivityLog::Action::None)
{
    ActivityLog::Entry result;
    result.key = key;
    result.state = state;
    result.title = title;
    result.action = action;
    result.target = key;
    return result;
}

}

void ActivityDialogTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    KLocalizedString::setApplicationDomain("plasma-impresario");
}

void ActivityDialogTest::showsEntriesAndActions()
{
    ActivityLog log;
    ActivityDialog dialog(&log);
    QCOMPARE(dialog.tree()->topLevelItemCount(), 0);

    log.record(entry(QStringLiteral("sync:a"), ActivityLog::State::Done, QStringLiteral("Synced")));
    auto review = entry(QStringLiteral("rule-review:r1"), ActivityLog::State::Waiting, QStringLiteral("Rule waits"),
                        ActivityLog::Action::ReviewRule);
    log.record(review);
    QCOMPARE(dialog.tree()->topLevelItemCount(), 2);
    // Newest first
    QCOMPARE(dialog.tree()->topLevelItem(0)->text(1), QStringLiteral("Rule waits"));

    auto *action = dialog.findChild<QPushButton *>(QStringLiteral("activityAction"));
    QVERIFY(action);
    dialog.tree()->setCurrentItem(dialog.tree()->topLevelItem(1));
    QVERIFY(!action->isEnabled());
    dialog.tree()->setCurrentItem(dialog.tree()->topLevelItem(0));
    QVERIFY(action->isEnabled());
    QCOMPARE(action->text(), ActivityLog::actionName(ActivityLog::Action::ReviewRule));

    QList<ActivityLog::Entry> requested;
    connect(&dialog, &ActivityDialog::actionRequested, this,
            [&requested](const ActivityLog::Entry &e) { requested << e; });
    action->click();
    QCOMPARE(requested.size(), 1);
    QCOMPARE(requested.at(0).target, QStringLiteral("rule-review:r1"));

    // An update keeps the selection on the same entry
    log.finish(QStringLiteral("sync:a"), ActivityLog::State::Failed, QStringLiteral("Connection refused"));
    QCOMPARE(dialog.tree()->currentItem()->text(1), QStringLiteral("Rule waits"));
    QCOMPARE(dialog.tree()->topLevelItem(0)->text(2), QStringLiteral("Connection refused"));
}

void ActivityDialogTest::clearKeepsFailures()
{
    ActivityLog log;
    ActivityDialog dialog(&log);
    log.record(entry(QStringLiteral("a"), ActivityLog::State::Done, QStringLiteral("A")));
    log.record(entry(QStringLiteral("b"), ActivityLog::State::Failed, QStringLiteral("B")));
    auto *clear = dialog.findChild<QPushButton *>(QStringLiteral("activityClear"));
    QVERIFY(clear->isEnabled());
    clear->click();
    QCOMPARE(dialog.tree()->topLevelItemCount(), 1);
    QCOMPARE(dialog.tree()->topLevelItem(0)->text(1), QStringLiteral("B"));
    QVERIFY(!clear->isEnabled());
}

void ActivityDialogTest::buttonCounts()
{
    ActivityLog log;
    ActivityButton button(&log);
    QVERIFY(button.text().isEmpty());
    QVERIFY(!button.toolTip().isEmpty());
    log.record(entry(QStringLiteral("a"), ActivityLog::State::Running, QStringLiteral("A")));
    log.record(entry(QStringLiteral("b"), ActivityLog::State::Failed, QStringLiteral("B")));
    QVERIFY(button.text().contains(QLatin1Char('1')));
    QCOMPARE(button.text().count(QLatin1Char('1')), 2);
    log.finish(QStringLiteral("a"), ActivityLog::State::Done);
    QCOMPARE(button.text().count(QLatin1Char('1')), 1);
}

QTEST_MAIN(ActivityDialogTest)
#include "test_ActivityDialog.moc"
