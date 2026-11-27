/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QTest>

#include "UiUtils/MailRules.h"
#include "UiUtils/SearchQuery.h"

using namespace UiUtils::MailRules;

Q_DECLARE_METATYPE(QList<UiUtils::MailRules::Action>)
Q_DECLARE_METATYPE(QList<UiUtils::MailRules::Condition>)

namespace
{

Message message()
{
    Message m;
    m.accountId = QStringLiteral("work");
    m.folder = QStringLiteral("INBOX");
    m.uid = 7;
    m.senders = {QStringLiteral("Ann Example <ann@example.org>")};
    m.recipients = {QStringLiteral("me@example.org"), QStringLiteral("Team <team@lists.example.org>")};
    m.subject = QStringLiteral("Invoice 2026-11");
    m.hasAttachment = true;
    m.flags = {QStringLiteral("\\Recent"), QStringLiteral("billing")};
    return m;
}

Rule rule(const QString &id, const QList<Condition> &conditions, const QList<Action> &actions)
{
    Rule r;
    r.id = id;
    r.name = id;
    r.conditions = conditions;
    r.actions = actions;
    r.markReviewed();
    return r;
}

}

class MailRulesTest : public QObject
{
    Q_OBJECT

private slots:
    void conditions_data()
    {
        QTest::addColumn<int>("field");
        QTest::addColumn<int>("comparison");
        QTest::addColumn<QString>("value");
        QTest::addColumn<bool>("expected");

        const auto add = [](const char *name, Field field, Comparison comparison, const QString &value, bool expected) {
            QTest::newRow(name) << static_cast<int>(field) << static_cast<int>(comparison) << value << expected;
        };
        add("sender contains address", Field::Sender, Comparison::Contains, QStringLiteral("EXAMPLE.org"), true);
        add("sender contains name", Field::Sender, Comparison::Contains, QStringLiteral("ann ex"), true);
        add("sender is address", Field::Sender, Comparison::Is, QStringLiteral("ann@example.org"), true);
        add("sender is name", Field::Sender, Comparison::Is, QStringLiteral("Ann Example"), true);
        add("sender is part", Field::Sender, Comparison::Is, QStringLiteral("ann"), false);
        add("sender is not", Field::Sender, Comparison::IsNot, QStringLiteral("bob@example.org"), true);
        add("recipients contains list", Field::Recipients, Comparison::Contains, QStringLiteral("lists."), true);
        add("recipients is address", Field::Recipients, Comparison::Is, QStringLiteral("me@example.org"), true);
        add("subject contains", Field::Subject, Comparison::Contains, QStringLiteral("invoice"), true);
        add("subject does not contain", Field::Subject, Comparison::DoesNotContain, QStringLiteral("invoice"), false);
        add("subject is whole", Field::Subject, Comparison::Is, QStringLiteral(" invoice 2026-11 "), true);
        add("attachment present", Field::Attachment, Comparison::Is, QString(), true);
        add("attachment absent", Field::Attachment, Comparison::IsNot, QString(), false);
        add("folder is inbox any case", Field::Folder, Comparison::Is, QStringLiteral("inbox"), true);
        add("tag is", Field::Tag, Comparison::Is, QStringLiteral("Billing"), true);
        add("system flag is no tag", Field::Tag, Comparison::Contains, QStringLiteral("Recent"), false);
        add("not junk", Field::Junk, Comparison::Is, QString(), false);
    }

    void conditions()
    {
        QFETCH(int, field);
        QFETCH(int, comparison);
        QFETCH(QString, value);
        QFETCH(bool, expected);
        const Condition condition{static_cast<Field>(field), static_cast<Comparison>(comparison), value};
        QCOMPARE(matches(condition, message()), expected);
    }

    void junkFlags()
    {
        QVERIFY(isJunk({QStringLiteral("$Junk")}));
        QVERIFY(isJunk({QStringLiteral("junk")}));
        QVERIFY(!isJunk({QStringLiteral("$Junk"), QStringLiteral("$NotJunk")}));
        QVERIFY(!isJunk({}));
    }

    void matchAllOrAny()
    {
        Rule r;
        r.conditions = {{Field::Subject, Comparison::Contains, QStringLiteral("invoice")},
                        {Field::Sender, Comparison::Contains, QStringLiteral("bob")}};
        QVERIFY(!matches(r, message()));
        r.match = Match::Any;
        QVERIFY(matches(r, message()));
        r.conditions.clear();
        QVERIFY(matches(r, message()));
    }

    void scope()
    {
        Rule r;
        QVERIFY(appliesTo(r, QStringLiteral("work"), QStringLiteral("INBOX")));
        QVERIFY(appliesTo(r, QStringLiteral("home"), QStringLiteral("inbox")));
        QVERIFY(!appliesTo(r, QStringLiteral("work"), QStringLiteral("Lists")));
        r.accountId = QStringLiteral("home");
        QVERIFY(!appliesTo(r, QStringLiteral("work"), QStringLiteral("INBOX")));
        r.accountId.clear();
        r.folders = {QStringLiteral("Lists")};
        QVERIFY(appliesTo(r, QStringLiteral("work"), QStringLiteral("Lists")));
        QVERIFY(!appliesTo(r, QStringLiteral("work"), QStringLiteral("INBOX")));
        QCOMPARE(r.checkedFolders(), QStringList{QStringLiteral("Lists")});
    }

    void marksAccumulateUntilAMove()
    {
        const QList<Rule> rules = {
            rule(QStringLiteral("flag"), {{Field::Subject, Comparison::Contains, QStringLiteral("invoice")}},
                 {{ActionKind::Flag, {}}, {ActionKind::AddTag, QStringLiteral("money")}}),
            rule(QStringLiteral("move"), {{Field::Attachment, Comparison::Is, {}}},
                 {{ActionKind::Move, QStringLiteral("Bills")}, {ActionKind::MarkRead, {}}}),
            rule(QStringLiteral("late"), {}, {{ActionKind::AddTag, QStringLiteral("never")}}),
        };
        const Outcome outcome = evaluate(rules, message());
        QCOMPARE(outcome.ruleIds, (QStringList{QStringLiteral("flag"), QStringLiteral("move")}));
        QCOMPARE(outcome.destination, Outcome::Destination::Move);
        QCOMPARE(outcome.folder, QStringLiteral("Bills"));
        QCOMPARE(outcome.destinationRuleId, QStringLiteral("move"));
        QVERIFY(outcome.flag);
        QVERIFY(outcome.markRead);
        QCOMPARE(outcome.tags, QStringList{QStringLiteral("money")});
        QVERIFY(outcome.moves());
    }

    void stopEndsProcessing()
    {
        Rule first = rule(QStringLiteral("first"), {}, {{ActionKind::Flag, {}}});
        first.stop = true;
        const Rule second = rule(QStringLiteral("second"), {}, {{ActionKind::Notify, {}}});
        const Outcome outcome = evaluate({first, second}, message());
        QCOMPARE(outcome.ruleIds, QStringList{QStringLiteral("first")});
        QVERIFY(!outcome.notify);
        QVERIFY(!outcome.moves());
    }

    void disabledAndOutOfScopeRulesDoNothing()
    {
        Rule off = rule(QStringLiteral("off"), {}, {{ActionKind::Flag, {}}});
        off.enabled = false;
        Rule elsewhere = rule(QStringLiteral("elsewhere"), {}, {{ActionKind::MarkRead, {}}});
        elsewhere.folders = {QStringLiteral("Lists")};
        QVERIFY(!evaluate({off, elsewhere}, message()).matched());
    }

    void movingRulesWaitForAPreview()
    {
        Rule r;
        r.id = QStringLiteral("trash");
        r.name = QStringLiteral("Trash invoices");
        r.conditions = {{Field::Subject, Comparison::Contains, QStringLiteral("invoice")}};
        r.actions = {{ActionKind::Delete, {}}};
        QVERIFY(r.needsReview());
        QVERIFY(!r.runsAutomatically());

        Outcome automatic = evaluate({r}, message(), Mode::Automatic);
        QVERIFY(!automatic.matched());
        QCOMPARE(automatic.waiting, QStringList{QStringLiteral("trash")});
        const Outcome manual = evaluate({r}, message(), Mode::Manual);
        QCOMPARE(manual.destination, Outcome::Destination::Trash);
        QVERIFY(manual.waiting.isEmpty());

        r.markReviewed();
        QVERIFY(r.runsAutomatically());
        automatic = evaluate({r}, message(), Mode::Automatic);
        QCOMPARE(automatic.destination, Outcome::Destination::Trash);

        // Renaming or switching off and on keeps the preview; changing what the rule does asks for a new one
        r.name = QStringLiteral("Renamed");
        r.enabled = false;
        r.enabled = true;
        QVERIFY(r.isReviewed());
        r.conditions.first().value = QStringLiteral("receipt");
        QVERIFY(r.needsReview());
    }

    void markingRulesNeedNoPreview()
    {
        Rule r;
        r.actions = {{ActionKind::Flag, {}}, {ActionKind::AddTag, QStringLiteral("x")}};
        QVERIFY(!r.needsReview());
        QVERIFY(r.runsAutomatically());
    }

    void problems_data()
    {
        QTest::addColumn<QList<Action>>("actions");
        QTest::addColumn<QString>("name");
        QTest::addColumn<int>("count");
        QTest::newRow("fine") << QList<Action>{{ActionKind::Flag, {}}} << QStringLiteral("ok") << 0;
        QTest::newRow("no name") << QList<Action>{{ActionKind::Flag, {}}} << QString() << 1;
        QTest::newRow("no action") << QList<Action>{} << QStringLiteral("ok") << 1;
        QTest::newRow("move without folder") << QList<Action>{{ActionKind::Move, {}}} << QStringLiteral("ok") << 1;
        QTest::newRow("move into checked folder") << QList<Action>{{ActionKind::Move, QStringLiteral("inbox")}}
                                                  << QStringLiteral("ok") << 1;
        QTest::newRow("two moves") << QList<Action>{{ActionKind::Archive, {}}, {ActionKind::Delete, {}}}
                                   << QStringLiteral("ok") << 1;
        QTest::newRow("tag with space") << QList<Action>{{ActionKind::AddTag, QStringLiteral("a b")}}
                                        << QStringLiteral("ok") << 1;
    }

    void problems()
    {
        QFETCH(QList<Action>, actions);
        QFETCH(QString, name);
        QFETCH(int, count);
        Rule r;
        r.name = name;
        r.actions = actions;
        r.conditions = {{Field::Subject, Comparison::Contains, QStringLiteral("x")}};
        QCOMPARE(UiUtils::MailRules::problems(r).size(), count);
    }

    void conditionWithoutValueIsAProblem()
    {
        Rule r;
        r.name = QStringLiteral("r");
        r.actions = {{ActionKind::Flag, {}}};
        r.conditions = {{Field::Sender, Comparison::Contains, QStringLiteral("  ")}, {Field::Attachment, Comparison::Is, {}}};
        QCOMPARE(UiUtils::MailRules::problems(r).size(), 1);
        r.conditions = {{Field::Junk, Comparison::Contains, {}}};
        QCOMPARE(UiUtils::MailRules::problems(r).size(), 1);
    }

    void warnings()
    {
        Rule r;
        r.actions = {{ActionKind::Archive, {}}};
        QCOMPARE(UiUtils::MailRules::warnings(r).size(), 2);
        r.markReviewed();
        r.conditions = {{Field::Subject, Comparison::Contains, QStringLiteral("x")}};
        // Adding the condition changed the rule after its preview
        QCOMPARE(UiUtils::MailRules::warnings(r).size(), 1);
        r.markReviewed();
        QVERIFY(UiUtils::MailRules::warnings(r).isEmpty());
    }

    void summary()
    {
        Rule r;
        r.conditions = {{Field::Sender, Comparison::Contains, QStringLiteral("ann")},
                        {Field::Attachment, Comparison::Is, {}}};
        r.actions = {{ActionKind::Move, QStringLiteral("Ann")}, {ActionKind::Flag, {}}};
        QCOMPARE(UiUtils::MailRules::summary(r),
                 QStringLiteral("If Sender contains “ann” and Attachments are present: move to “Ann”, flag"));
        r.conditions.clear();
        r.actions = {{ActionKind::Notify, {}}};
        QCOMPARE(UiUtils::MailRules::summary(r), QStringLiteral("Every new message: notify"));
    }

    void fromQuery_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<int>("match");
        QTest::addColumn<QList<Condition>>("conditions");
        QTest::addColumn<int>("unsupported");
        QTest::addColumn<int>("notes");

        QTest::newRow("and") << QStringLiteral("from:ann subject:invoice") << static_cast<int>(Match::All)
                             << QList<Condition>{{Field::Sender, Comparison::Contains, QStringLiteral("ann")},
                                                 {Field::Subject, Comparison::Contains, QStringLiteral("invoice")}}
                             << 0 << 0;
        QTest::newRow("or") << QStringLiteral("to:a@x.org OR cc:b@x.org") << static_cast<int>(Match::Any)
                            << QList<Condition>{{Field::Recipients, Comparison::Contains, QStringLiteral("a@x.org")},
                                                {Field::Recipients, Comparison::Contains, QStringLiteral("b@x.org")}}
                            << 0 << 0;
        QTest::newRow("negated") << QStringLiteral("-from:ann@x.org") << static_cast<int>(Match::All)
                                 << QList<Condition>{{Field::Sender, Comparison::DoesNotContain, QStringLiteral("ann@x.org")}}
                                 << 0 << 0;
        QTest::newRow("negated attachment") << QStringLiteral("NOT has:attachment") << static_cast<int>(Match::All)
                                            << QList<Condition>{{Field::Attachment, Comparison::IsNot, {}}} << 0 << 0;
        QTest::newRow("attachment and tag") << QStringLiteral("has:attachment tag:work") << static_cast<int>(Match::All)
                                            << QList<Condition>{{Field::Attachment, Comparison::Is, {}},
                                                                {Field::Tag, Comparison::Contains, QStringLiteral("work")}}
                                            << 0 << 0;
        QTest::newRow("words") << QStringLiteral("report") << static_cast<int>(Match::All)
                               << QList<Condition>{{Field::Subject, Comparison::Contains, QStringLiteral("report")}} << 0 << 1;
        QTest::newRow("body") << QStringLiteral("from:ann body:secret") << static_cast<int>(Match::All)
                              << QList<Condition>{{Field::Sender, Comparison::Contains, QStringLiteral("ann")}} << 1 << 0;
        QTest::newRow("state") << QStringLiteral("is:unread") << static_cast<int>(Match::All) << QList<Condition>{} << 1 << 0;
        QTest::newRow("date") << QStringLiteral("date>2026-01-01") << static_cast<int>(Match::All) << QList<Condition>{} << 1 << 0;
        QTest::newRow("nested") << QStringLiteral("from:a (subject:b OR subject:c)") << static_cast<int>(Match::All)
                                << QList<Condition>{{Field::Sender, Comparison::Contains, QStringLiteral("a")}} << 1 << 0;
        QTest::newRow("empty") << QString() << static_cast<int>(Match::All) << QList<Condition>{} << 1 << 0;
    }

    void fromQuery()
    {
        QFETCH(QString, text);
        QFETCH(int, match);
        QFETCH(QList<Condition>, conditions);
        QFETCH(int, unsupported);
        QFETCH(int, notes);
        const auto query = UiUtils::SearchQuery::Query::parse(text, QDate(2026, 11, 27));
        const Conversion conversion = UiUtils::MailRules::fromQuery(query);
        QCOMPARE(static_cast<int>(conversion.rule.match), match);
        QCOMPARE(conversion.rule.conditions, conditions);
        QCOMPARE(conversion.unsupported.size(), unsupported);
        QCOMPARE(conversion.notes.size(), notes);
        QCOMPARE(conversion.isUsable(), unsupported == 0);
        QVERIFY(conversion.rule.actions.isEmpty());
    }

    void newIdAvoidsTakenOnes()
    {
        const QString first = newId({});
        QCOMPARE(first.size(), 8);
        QVERIFY(newId({first}) != first);
    }
};

QTEST_GUILESS_MAIN(MailRulesTest)

#include "test_MailRules.moc"
