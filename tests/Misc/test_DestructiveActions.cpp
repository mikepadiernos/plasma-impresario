/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <KConfig>
#include <KConfigGroup>
#include "test_DestructiveActions.h"
#include "UiUtils/DestructiveActions.h"

using namespace UiUtils::DestructiveActions;

namespace {

struct PromptCounter {
    int calls = 0;
    ConfirmationAnswer answer;

    std::function<ConfirmationAnswer()> prompt()
    {
        return [this]() {
            ++calls;
            return answer;
        };
    }
};

}

void DestructiveActionsTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void DestructiveActionsTest::chooseDeleteAction_data()
{
    QTest::addColumn<bool>("enabled");
    QTest::addColumn<QString>("trash");
    QTest::addColumn<QString>("current");
    QTest::addColumn<int>("expected");

    const int move = static_cast<int>(DeleteAction::MoveToTrash);
    const int mark = static_cast<int>(DeleteAction::MarkDeleted);
    QTest::newRow("inbox-to-trash") << true << QStringLiteral("Trash") << QStringLiteral("INBOX") << move;
    QTest::newRow("nested-trash") << true << QStringLiteral("INBOX.Trash") << QStringLiteral("INBOX") << move;
    QTest::newRow("disabled") << false << QStringLiteral("Trash") << QStringLiteral("INBOX") << mark;
    QTest::newRow("no-trash") << true << QString() << QStringLiteral("INBOX") << mark;
    QTest::newRow("in-trash") << true << QStringLiteral("Trash") << QStringLiteral("Trash") << mark;
    QTest::newRow("trash-child") << true << QStringLiteral("Trash") << QStringLiteral("Trash/Old") << move;
}

void DestructiveActionsTest::chooseDeleteAction()
{
    QFETCH(bool, enabled);
    QFETCH(QString, trash);
    QFETCH(QString, current);
    QFETCH(int, expected);
    QCOMPARE(static_cast<int>(UiUtils::DestructiveActions::chooseDeleteAction(enabled, trash, current)), expected);
}

void DestructiveActionsTest::keysAreDistinct()
{
    QCOMPARE(configGroupName(), QStringLiteral("Confirmations"));
    QCOMPARE(configKey(Confirmation::Expunge), QStringLiteral("ConfirmExpunge"));
    QCOMPARE(configKey(Confirmation::DeletePermanently), QStringLiteral("ConfirmDeletePermanently"));
    QCOMPARE(configKey(Confirmation::EmptyFolder), QStringLiteral("ConfirmEmptyFolder"));
}

void DestructiveActionsTest::asksByDefault()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    KConfig config(dir.filePath(QStringLiteral("rc")), KConfig::SimpleConfig);
    KConfigGroup group(&config, configGroupName());

    PromptCounter counter;
    counter.answer.accepted = true;
    QVERIFY(confirm(group, Confirmation::Expunge, counter.prompt()));
    QCOMPARE(counter.calls, 1);
    QVERIFY(!group.hasKey(configKey(Confirmation::Expunge)));
}

void DestructiveActionsTest::cancelDoesNotPersist()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    KConfig config(dir.filePath(QStringLiteral("rc")), KConfig::SimpleConfig);
    KConfigGroup group(&config, configGroupName());

    PromptCounter counter;
    QVERIFY(!confirm(group, Confirmation::DeletePermanently, counter.prompt()));
    QVERIFY(!confirm(group, Confirmation::DeletePermanently, counter.prompt()));
    QCOMPARE(counter.calls, 2);
}

void DestructiveActionsTest::cancelWithDontAskDoesNotPersist()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    KConfig config(dir.filePath(QStringLiteral("rc")), KConfig::SimpleConfig);
    KConfigGroup group(&config, configGroupName());

    PromptCounter counter;
    counter.answer.dontAskAgain = true;
    QVERIFY(!confirm(group, Confirmation::EmptyFolder, counter.prompt()));
    QVERIFY(!group.hasKey(configKey(Confirmation::EmptyFolder)));
    QVERIFY(!confirm(group, Confirmation::EmptyFolder, counter.prompt()));
    QCOMPARE(counter.calls, 2);
}

void DestructiveActionsTest::acceptWithoutDontAskKeepsAsking()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    KConfig config(dir.filePath(QStringLiteral("rc")), KConfig::SimpleConfig);
    KConfigGroup group(&config, configGroupName());

    PromptCounter counter;
    counter.answer.accepted = true;
    QVERIFY(confirm(group, Confirmation::Expunge, counter.prompt()));
    QVERIFY(confirm(group, Confirmation::Expunge, counter.prompt()));
    QCOMPARE(counter.calls, 2);
}

void DestructiveActionsTest::acceptWithDontAskPersists()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("rc"));
    {
        KConfig config(path, KConfig::SimpleConfig);
        KConfigGroup group(&config, configGroupName());
        PromptCounter counter;
        counter.answer = {true, true};
        QVERIFY(confirm(group, Confirmation::Expunge, counter.prompt()));
        QCOMPARE(counter.calls, 1);
    }

    // The choice survives a restart and the prompt is skipped from then on
    KConfig config(path, KConfig::SimpleConfig);
    KConfigGroup group(&config, configGroupName());
    QCOMPARE(group.readEntry(configKey(Confirmation::Expunge), true), false);
    PromptCounter counter;
    QVERIFY(confirm(group, Confirmation::Expunge, counter.prompt()));
    QCOMPARE(counter.calls, 0);
}

void DestructiveActionsTest::confirmationsAreIndependent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    KConfig config(dir.filePath(QStringLiteral("rc")), KConfig::SimpleConfig);
    KConfigGroup group(&config, configGroupName());

    PromptCounter counter;
    counter.answer = {true, true};
    QVERIFY(confirm(group, Confirmation::Expunge, counter.prompt()));
    QCOMPARE(counter.calls, 1);

    // Turning off the Expunge question keeps asking before deleting permanently
    PromptCounter other;
    QVERIFY(!confirm(group, Confirmation::DeletePermanently, other.prompt()));
    QCOMPARE(other.calls, 1);
    QVERIFY(confirm(group, Confirmation::Expunge, other.prompt()));
    QCOMPARE(other.calls, 1);
}

QTEST_GUILESS_MAIN(DestructiveActionsTest)
