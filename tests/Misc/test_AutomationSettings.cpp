/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <KConfigGroup>
#include <KSharedConfig>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "UiUtils/MailRulesSettings.h"
#include "UiUtils/NewMailSettings.h"

using namespace UiUtils;
using namespace UiUtils::MailRules;

class AutomationSettingsTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    KSharedConfigPtr config(const QString &name)
    {
        return KSharedConfig::openConfig(m_dir.filePath(name), KConfig::SimpleConfig);
    }

    static Rule sampleRule()
    {
        Rule rule;
        rule.id = QStringLiteral("r1");
        rule.name = QStringLiteral("Bills | receipts");
        rule.accountId = QStringLiteral("work");
        rule.folders = {QStringLiteral("INBOX"), QStringLiteral("Lists/KDE")};
        rule.match = Match::Any;
        rule.stop = true;
        rule.conditions = {{Field::Subject, Comparison::Contains, QStringLiteral("a|b")},
                           {Field::Attachment, Comparison::IsNot, {}}};
        rule.actions = {{ActionKind::Move, QStringLiteral("Bills")}, {ActionKind::AddTag, QStringLiteral("money")}};
        rule.markReviewed();
        return rule;
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
    }

    void rulesRoundTrip()
    {
        auto cfg = config(QStringLiteral("rules"));
        Rule second;
        second.id = QStringLiteral("r2");
        second.name = QStringLiteral("Flag all");
        second.enabled = false;
        second.actions = {{ActionKind::Flag, {}}};
        const QList<Rule> rules = {sampleRule(), second};
        MailRulesSettings::save(*cfg, rules);
        cfg->reparseConfiguration();
        QCOMPARE(MailRulesSettings::load(*cfg), rules);
        QVERIFY(MailRulesSettings::load(*cfg).first().isReviewed());

        // Reordering and removing keep only what is saved
        MailRulesSettings::save(*cfg, {second});
        QCOMPARE(MailRulesSettings::load(*cfg), QList<Rule>{second});
        QVERIFY(!cfg->group(QStringLiteral("MailRules")).hasGroup(QStringLiteral("Rule r1")));
    }

    void unreadableEntriesAreDropped()
    {
        auto cfg = config(QStringLiteral("broken"));
        KConfigGroup group = cfg->group(QStringLiteral("MailRules"));
        group.writeEntry("Order", QStringList{QStringLiteral("x"), QStringLiteral("missing"), QStringLiteral("x")});
        KConfigGroup rule = group.group(QStringLiteral("Rule x"));
        rule.writeEntry("Name", QStringLiteral("X"));
        rule.writeEntry("Conditions", QStringList{QStringLiteral("colour|is|red"), QStringLiteral("sender|contains|ann")});
        rule.writeEntry("Actions", QStringList{QStringLiteral("explode|"), QStringLiteral("flag|")});
        const QList<Rule> rules = MailRulesSettings::load(*cfg);
        QCOMPARE(rules.size(), 1);
        QCOMPARE(rules.first().conditions.size(), 1);
        QCOMPARE(rules.first().actions.size(), 1);
        QVERIFY(rules.first().enabled);
    }

    void accountRemovalAndFolderRename()
    {
        auto cfg = config(QStringLiteral("rename"));
        Rule everyAccount;
        everyAccount.id = QStringLiteral("all");
        everyAccount.name = QStringLiteral("All");
        everyAccount.folders = {QStringLiteral("Lists/KDE")};
        everyAccount.actions = {{ActionKind::Flag, {}}};
        MailRulesSettings::save(*cfg, {sampleRule(), everyAccount});

        QVERIFY(MailRulesSettings::renameFolder(*cfg, QStringLiteral("work"), QStringLiteral("Bills"), QStringLiteral("Money")));
        QVERIFY(MailRulesSettings::renameFolder(*cfg, QStringLiteral("work"), QStringLiteral("Lists/KDE"), QStringLiteral("KDE")));
        QVERIFY(!MailRulesSettings::renameFolder(*cfg, QStringLiteral("home"), QStringLiteral("Lists/KDE"), QStringLiteral("x")));
        const QList<Rule> renamed = MailRulesSettings::load(*cfg);
        QCOMPARE(renamed.at(0).actions.first().value, QStringLiteral("Money"));
        QCOMPARE(renamed.at(0).folders, (QStringList{QStringLiteral("INBOX"), QStringLiteral("KDE")}));
        QVERIFY(renamed.at(0).isReviewed());
        QCOMPARE(renamed.at(1).folders, QStringList{QStringLiteral("Lists/KDE")});

        MailRulesSettings::removeAccount(*cfg, QStringLiteral("work"));
        const QList<Rule> left = MailRulesSettings::load(*cfg);
        QCOMPARE(left.size(), 1);
        QCOMPARE(left.first().id, QStringLiteral("all"));
    }

    void notificationSettingsRoundTrip()
    {
        auto cfg = config(QStringLiteral("notifications"));
        QCOMPARE(NewMailSettings::load(*cfg), NewMail::Settings());

        NewMail::Settings settings;
        settings.quietWhileActive = false;
        settings.showContent = false;
        settings.summaryMinutes = 30;
        settings.accountModes.insert(QStringLiteral("work"), NewMail::Mode::Summary);
        settings.accountModes.insert(QStringLiteral("old"), NewMail::Mode::Off);
        settings.setAnnounces(QStringLiteral("work"), QStringLiteral("Lists/KDE, Plasma"), true);
        settings.setAnnounces(QStringLiteral("home"), QStringLiteral("INBOX"), false);
        NewMailSettings::save(*cfg, settings);
        cfg->reparseConfiguration();
        QCOMPARE(NewMailSettings::load(*cfg), settings);

        QVERIFY(NewMailSettings::renameFolder(*cfg, QStringLiteral("work"), QStringLiteral("Lists/KDE, Plasma"), QStringLiteral("KDE")));
        QVERIFY(!NewMailSettings::renameFolder(*cfg, QStringLiteral("work"), QStringLiteral("Nothing"), QStringLiteral("X")));
        QCOMPARE(NewMailSettings::load(*cfg).extraFolders(QStringLiteral("work")), QStringList{QStringLiteral("KDE")});

        NewMailSettings::removeAccount(*cfg, QStringLiteral("work"));
        const NewMail::Settings left = NewMailSettings::load(*cfg);
        QCOMPARE(left.mode(QStringLiteral("work")), NewMail::Mode::Each);
        QVERIFY(left.extraFolders(QStringLiteral("work")).isEmpty());
        QCOMPARE(left.mode(QStringLiteral("old")), NewMail::Mode::Off);
        QVERIFY(!left.announces(QStringLiteral("home"), QStringLiteral("INBOX")));
    }

    void baselines()
    {
        auto state = config(QStringLiteral("state"));
        QVERIFY(!NewMailSettings::loadBaseline(*state, QStringLiteral("work"), QStringLiteral("INBOX")).isValid());
        const NewMailWatcher::Baseline baseline{42, 1000, QDateTime(QDate(2026, 11, 27), QTime(9, 30), QTimeZone::UTC)};
        NewMailSettings::saveBaseline(*state, QStringLiteral("work"), QStringLiteral("INBOX"), baseline);
        NewMailSettings::saveBaseline(*state, QStringLiteral("work"), QStringLiteral("Lists/KDE ä"), baseline);
        NewMailSettings::saveBaseline(*state, QStringLiteral("home"), QStringLiteral("INBOX"), baseline);
        state->reparseConfiguration();
        QCOMPARE(NewMailSettings::loadBaseline(*state, QStringLiteral("work"), QStringLiteral("INBOX")), baseline);
        QCOMPARE(NewMailSettings::loadBaseline(*state, QStringLiteral("work"), QStringLiteral("Lists/KDE ä")), baseline);
        NewMailSettings::removeBaselines(*state, QStringLiteral("work"));
        QVERIFY(!NewMailSettings::loadBaseline(*state, QStringLiteral("work"), QStringLiteral("INBOX")).isValid());
        QCOMPARE(NewMailSettings::loadBaseline(*state, QStringLiteral("home"), QStringLiteral("INBOX")), baseline);
    }
};

QTEST_GUILESS_MAIN(AutomationSettingsTest)

#include "test_AutomationSettings.moc"
