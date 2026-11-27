/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QTest>
#include <QTreeWidget>

#include <KLocalizedString>

#include "Gui/RuleEditDialog.h"
#include "Gui/RulesDialog.h"

using Gui::RuleEditDialog;
using Gui::RulesDialog;
namespace MailRules = UiUtils::MailRules;

namespace {

const RulesDialog::Accounts accounts{{QStringLiteral("work"), QStringLiteral("Work")},
                                     {QStringLiteral("home"), QStringLiteral("Home")}};

MailRules::Rule rule(const QString &id, const QString &subject, MailRules::ActionKind kind,
                     const QString &value = QString())
{
    MailRules::Rule result;
    result.id = id;
    result.name = id;
    result.conditions = {{MailRules::Field::Subject, MailRules::Comparison::Contains, subject}};
    result.actions = {{kind, value}};
    return result;
}

MailRules::Message message(uint uid, const QString &subject)
{
    MailRules::Message result;
    result.accountId = QStringLiteral("work");
    result.folder = QStringLiteral("INBOX");
    result.uid = uid;
    result.subject = subject;
    result.senders = {QStringLiteral("Ann <ann@example.org>")};
    result.received = QDateTime(QDate(2026, 11, 27), QTime(9, 0));
    return result;
}

QPushButton *button(QWidget *dialog, const char *name)
{
    return dialog->findChild<QPushButton *>(QLatin1String(name));
}

}

class RulesDialogTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void editorRefusesIncompleteRule();
    void editorKeepsIdAndPreview();
    void listOrderAndToggle();
    void previewMarksReviewed();
    void applyAsksFirst();
    void addRuleGoesThroughEditor();
};

void RulesDialogTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    KLocalizedString::setApplicationDomain("plasma-impresario");
}

void RulesDialogTest::editorRefusesIncompleteRule()
{
    RuleEditDialog editor(accounts);
    MailRules::Rule empty;
    empty.id = QStringLiteral("r1");
    editor.setRule(empty);
    QVERIFY(!editor.canAccept());
    QVERIFY(!editor.feedback().isEmpty());

    auto *name = editor.findChild<QLineEdit *>(QStringLiteral("ruleName"));
    QVERIFY(name);
    name->setText(QStringLiteral("Invoices"));
    editor.addAction({MailRules::ActionKind::Flag, QString()});
    QVERIFY(editor.canAccept());

    editor.addCondition({MailRules::Field::Subject, MailRules::Comparison::Contains, QString()});
    QVERIFY(!editor.canAccept());
    QVERIFY(editor.feedback().contains(MailRules::fieldName(MailRules::Field::Subject)));
}

void RulesDialogTest::editorKeepsIdAndPreview()
{
    auto original = rule(QStringLiteral("r7"), QStringLiteral("invoice"), MailRules::ActionKind::Archive);
    original.accountId = QStringLiteral("home");
    original.folders = {QStringLiteral("INBOX"), QStringLiteral("Lists")};
    original.markReviewed();

    RuleEditDialog editor(accounts);
    editor.setRule(original);
    QVERIFY(editor.canAccept());
    const MailRules::Rule read = editor.rule();
    QCOMPARE(read.id, original.id);
    QCOMPARE(read.accountId, original.accountId);
    QCOMPARE(read.folders, original.folders);
    QCOMPARE(read.conditions, original.conditions);
    QCOMPARE(read.actions, original.actions);
    QVERIFY(read.isReviewed());

    // Renaming keeps the preview; changing the scope asks for a new one
    editor.findChild<QLineEdit *>(QStringLiteral("ruleName"))->setText(QStringLiteral("Renamed"));
    QVERIFY(editor.rule().isReviewed());
    editor.findChild<QPlainTextEdit *>(QStringLiteral("ruleFolders"))->setPlainText(QStringLiteral("INBOX"));
    QVERIFY(!editor.rule().isReviewed());
}

void RulesDialogTest::listOrderAndToggle()
{
    RulesDialog dialog(accounts);
    dialog.setRules({rule(QStringLiteral("a"), QStringLiteral("x"), MailRules::ActionKind::Flag),
                     rule(QStringLiteral("b"), QStringLiteral("y"), MailRules::ActionKind::MarkRead)});
    QCOMPARE(dialog.list()->count(), 2);
    QVERIFY(!button(&dialog, "ruleUp")->isEnabled());
    QVERIFY(button(&dialog, "ruleDown")->isEnabled());
    // Without a shown folder there is nothing to preview
    QVERIFY(!button(&dialog, "rulePreview")->isEnabled());

    button(&dialog, "ruleDown")->click();
    QCOMPARE(dialog.rules().at(0).id, QStringLiteral("b"));
    QCOMPARE(dialog.list()->currentRow(), 1);

    dialog.list()->item(0)->setCheckState(Qt::Unchecked);
    QVERIFY(!dialog.rules().at(0).enabled);

    dialog.selectRule(QStringLiteral("b"));
    button(&dialog, "ruleRemove")->click();
    QCOMPARE(dialog.rules().size(), 1);
    QCOMPARE(dialog.rules().at(0).id, QStringLiteral("a"));
}

void RulesDialogTest::previewMarksReviewed()
{
    RulesDialog dialog(accounts);
    auto archive = rule(QStringLiteral("a"), QStringLiteral("invoice"), MailRules::ActionKind::Archive);
    archive.folders = {QStringLiteral("Lists")};
    dialog.setRules({archive});
    QVERIFY(dialog.rules().at(0).needsReview());
    QVERIFY(!dialog.list()->item(0)->icon().isNull() || !dialog.list()->item(0)->toolTip().isEmpty());

    dialog.setShownFolder(QStringLiteral("work"), QStringLiteral("INBOX"), []() {
        return QList<MailRules::Message>{message(1, QStringLiteral("Invoice 12")), message(2, QStringLiteral("Hello")),
                                         message(3, QStringLiteral("Your invoice"))};
    });
    QVERIFY(button(&dialog, "rulePreview")->isEnabled());
    // No applier yet: applying is off
    QVERIFY(!button(&dialog, "ruleApply")->isEnabled());

    dialog.previewSelected();
    QCOMPARE(dialog.previewList()->topLevelItemCount(), 2);
    QVERIFY(!dialog.previewSummary().isEmpty());
    QVERIFY(dialog.rules().at(0).isReviewed());
    QVERIFY(dialog.rules().at(0).runsAutomatically());
}

void RulesDialogTest::applyAsksFirst()
{
    RulesDialog dialog(accounts);
    dialog.setRules({rule(QStringLiteral("a"), QStringLiteral("invoice"), MailRules::ActionKind::Flag)});
    dialog.setShownFolder(QStringLiteral("work"), QStringLiteral("INBOX"), []() {
        return QList<MailRules::Message>{message(1, QStringLiteral("Invoice 12")), message(2, QStringLiteral("Hello"))};
    });
    int applied = 0;
    QList<uint> uids;
    dialog.setApplier([&](const QList<MailRules::Rule> &rules, const QString &accountId, const QString &folder,
                          const QList<MailRules::Message> &messages) {
        ++applied;
        [&]() {
            QCOMPARE(rules.size(), 1);
            QCOMPARE(accountId, QStringLiteral("work"));
            QCOMPARE(folder, QStringLiteral("INBOX"));
        }();
        for (const auto &m : messages)
            uids << m.uid;
        return static_cast<int>(messages.size());
    });
    QString asked;
    bool answer = false;
    dialog.setConfirmation([&](const QString &question) {
        asked = question;
        return answer;
    });
    QVERIFY(button(&dialog, "ruleApply")->isEnabled());

    dialog.applySelected();
    QVERIFY(!asked.isEmpty());
    QCOMPARE(applied, 0);

    answer = true;
    dialog.applySelected();
    QCOMPARE(applied, 1);
    QCOMPARE(uids, QList<uint>{1});
}

void RulesDialogTest::addRuleGoesThroughEditor()
{
    RulesDialog dialog(accounts);
    dialog.setRules({rule(QStringLiteral("r1"), QStringLiteral("x"), MailRules::ActionKind::Flag)});
    bool accept = false;
    QString seenId;
    dialog.setEditor([&](MailRules::Rule &edited) {
        seenId = edited.id;
        edited.actions = {{MailRules::ActionKind::MarkRead, QString()}};
        return accept;
    });

    MailRules::Rule fromSearch;
    fromSearch.id = QStringLiteral("r1");
    fromSearch.name = QStringLiteral("From search");
    dialog.addRule(fromSearch);
    QCOMPARE(dialog.rules().size(), 1);
    // A taken ID is replaced before the editor sees the rule
    QVERIFY(!seenId.isEmpty());
    QVERIFY(seenId != QStringLiteral("r1"));

    accept = true;
    dialog.addRule(fromSearch);
    QCOMPARE(dialog.rules().size(), 2);
    QCOMPARE(dialog.rules().at(1).name, QStringLiteral("From search"));
    QCOMPARE(dialog.list()->currentRow(), 1);
}

QTEST_MAIN(RulesDialogTest)
#include "test_RulesDialog.moc"
