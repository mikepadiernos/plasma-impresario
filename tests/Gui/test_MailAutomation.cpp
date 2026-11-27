/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include "Gui/MailAutomation.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Streams/FakeSocket.h"
#include "UiUtils/ActivityLog.h"
#include "Utils/LibMailboxSync.h"

using Gui::MailAutomation;
namespace MailRules = UiUtils::MailRules;
namespace NewMail = UiUtils::NewMail;

namespace {

/** A FETCH response with everything a message needs before the model counts it as loaded, received just now */
QByteArray metadata(const uint seq, const uint uid, const QString &subject, const QString &flags = QString())
{
    const QString now = QLocale::c().toString(QDateTime::currentDateTimeUtc(), QStringLiteral("dd-MMM-yyyy hh:mm:ss"));
    return QStringLiteral("* %1 FETCH (UID %2 FLAGS (%5) RFC822.SIZE 89 INTERNALDATE \"%4 +0000\" "
                          "ENVELOPE (NIL \"%3\" ((\"Ann\" NIL \"ann\" \"example.org\")) NIL NIL NIL NIL NIL NIL NIL) "
                          "BODYSTRUCTURE (\"text\" \"plain\" () NIL NIL NIL 19 2 NIL NIL NIL NIL))\r\n")
        .arg(QString::number(seq), QString::number(uid), subject, now, flags)
        .toUtf8();
}

class FakeNotifier : public MailAutomation::Notifier
{
public:
    struct State {
        QHash<QString, NewMail::Text> shown;
        int shows = 0;
        int closes = 0;
    };
    explicit FakeNotifier(std::shared_ptr<State> state)
        : m_state(std::move(state))
    {
    }
    void show(const QString &accountId, const NewMail::Text &text) override
    {
        m_state->shown.insert(accountId, text);
        ++m_state->shows;
    }
    void close(const QString &accountId) override
    {
        if (m_state->shown.remove(accountId))
            ++m_state->closes;
    }

private:
    std::shared_ptr<State> m_state;
};

}

class MailAutomationTest : public LibMailboxSync
{
    Q_OBJECT

private slots:
    void initTestCase() override;
    void init() override;
    void cleanup() override;
    void rulesMarkNewMailAndItIsAnnounced();
    void mailInSightIsNotAnnounced();
    void markingAsReadFromTheNotification();
    void movingRulesWaitForAPreview();
    void readElsewhereLeavesTheNotification();
    void removedAccountsAreForgotten();

private:
    /** Watch folder "a" of the account "work", which is not shown, and deliver one new message */
    void deliverFourth(const QString &subject);

    std::unique_ptr<QTemporaryDir> m_dir;
    KSharedConfigPtr m_config;
    KSharedConfigPtr m_state;
    std::unique_ptr<MailAutomation> m_automation;
    std::shared_ptr<FakeNotifier::State> m_shown;
};

void MailAutomationTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    KLocalizedString::setApplicationDomain("plasma-impresario");
    LibMailboxSync::initTestCase();
}

void MailAutomationTest::init()
{
    LibMailboxSync::init();
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_config = KSharedConfig::openConfig(m_dir->filePath(QStringLiteral("rc")), KConfig::SimpleConfig);
    m_state = KSharedConfig::openConfig(m_dir->filePath(QStringLiteral("state")), KConfig::SimpleConfig);
    m_automation = std::make_unique<MailAutomation>(m_config, m_state);
    m_shown = std::make_shared<FakeNotifier::State>();
    m_automation->setNotifier(std::make_unique<FakeNotifier>(m_shown));
    m_automation->setFolderResolver([](const QString &) {
        return MailAutomation::Folders{QStringLiteral("b"), QStringLiteral("b")};
    });
    m_automation->setActiveAccount(QStringLiteral("home"));
    NewMail::Settings settings;
    settings.setAnnounces(QStringLiteral("work"), QStringLiteral("a"), true);
    settings.quietWhileActive = false;
    m_automation->setNotificationSettings(settings);
}

void MailAutomationTest::cleanup()
{
    m_automation.reset();
    m_config.reset();
    m_state.reset();
    m_dir.reset();
    LibMailboxSync::cleanup();
}

void MailAutomationTest::deliverFourth(const QString &subject)
{
    initialMessages(3);
    m_automation->addAccount(QStringLiteral("work"), QStringLiteral("Work"), model);
    QCoreApplication::processEvents();
    QCOMPARE(m_automation->watchedFolders(QStringLiteral("work")), (QStringList{QStringLiteral("INBOX"), QStringLiteral("a")}));
    // The first look only records what is there, and keeps it for the next session
    QCOMPARE(m_shown->shows, 0);
    QVERIFY(m_state->group(QStringLiteral("NewMailSeen")).keyList().size() == 1);

    cServer("* 4 EXISTS\r\n");
    cClient(t.mk("UID FETCH 4:* (FLAGS)\r\n"));
    cServer("* 4 FETCH (UID 4 FLAGS ())\r\n" + t.last("OK fetched\r\n"));
    cClient(t.mk("UID FETCH 1:4 (" FETCH_METADATA_ITEMS ")\r\n"));
    cServer(metadata(1, 1, QStringLiteral("One"), QStringLiteral("\\Seen")) + metadata(2, 2, QStringLiteral("Two"), QStringLiteral("\\Seen"))
            + metadata(3, 3, QStringLiteral("Three"), QStringLiteral("\\Seen")) + metadata(4, 4, subject)
            + t.last("OK fetched\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
}

void MailAutomationTest::rulesMarkNewMailAndItIsAnnounced()
{
    MailRules::Rule rule;
    rule.id = QStringLiteral("r1");
    rule.name = QStringLiteral("Invoices");
    rule.folders = {QStringLiteral("a")};
    rule.conditions = {{MailRules::Field::Subject, MailRules::Comparison::Contains, QStringLiteral("invoice")}};
    rule.actions = {{MailRules::ActionKind::Flag, QString()}, {MailRules::ActionKind::AddTag, QStringLiteral("$label1")}};
    m_automation->setRules({rule});
    deliverFourth(QStringLiteral("Invoice 4"));

    const QByteArray flag = t.mk("UID STORE 4 +FLAGS (\\Flagged)\r\n");
    const QByteArray flagged = t.last("OK stored\r\n");
    const QByteArray tag = t.mk("UID STORE 4 +FLAGS ($label1)\r\n");
    const QByteArray tagged = t.last("OK stored\r\n");
    cClient(flag + tag);
    cServer(flagged + tagged);
    QCOMPARE(m_shown->shows, 1);
    const NewMail::Text text = m_shown->shown.value(QStringLiteral("work"));
    QCOMPARE(text.count, 1);
    QVERIFY(text.single);
    QVERIFY(text.title.contains(QLatin1String("Work")));
    QVERIFY(text.body.contains(QLatin1String("Invoice 4")));

    UiUtils::ActivityLog *log = m_automation->activity();
    const int row = log->find(QStringLiteral("rules:work:a"));
    QVERIFY(row >= 0);
    QVERIFY(log->entry(row).detail.contains(QLatin1String("Invoices")));
    QVERIFY(log->find(QStringLiteral("new-mail:work")) >= 0);
    cEmpty();
}

void MailAutomationTest::mailInSightIsNotAnnounced()
{
    m_automation->setActiveAccount(QStringLiteral("work"));
    m_automation->setWindowActive(true);
    QSignalSpy status(m_automation.get(), &MailAutomation::statusMessage);
    deliverFourth(QStringLiteral("Hello"));
    QCOMPARE(m_shown->shows, 0);
    QCOMPARE(status.count(), 1);
    QVERIFY(m_automation->digest().isEmpty(QStringLiteral("work")));
    cEmpty();
}

void MailAutomationTest::markingAsReadFromTheNotification()
{
    deliverFourth(QStringLiteral("Hello"));
    QCOMPARE(m_shown->shows, 1);
    m_automation->markAnnouncedRead(QStringLiteral("work"));
    cClient(t.mk("UID STORE 4 +FLAGS (\\Seen)\r\n"));
    cServer(t.last("OK stored\r\n"));
    QVERIFY(!m_shown->shown.contains(QStringLiteral("work")));
    QVERIFY(m_automation->digest().isEmpty(QStringLiteral("work")));
    cEmpty();
}

void MailAutomationTest::movingRulesWaitForAPreview()
{
    MailRules::Rule rule;
    rule.id = QStringLiteral("r2");
    rule.name = QStringLiteral("Away");
    rule.folders = {QStringLiteral("a")};
    rule.actions = {{MailRules::ActionKind::Move, QStringLiteral("b")}};
    m_automation->setRules({rule});
    deliverFourth(QStringLiteral("Hello"));
    // Nothing is moved before the rule's matches were seen once; the mail is announced where it is
    QCOMPARE(m_shown->shows, 1);
    UiUtils::ActivityLog *log = m_automation->activity();
    const int row = log->find(QStringLiteral("rule-review:r2"));
    QVERIFY(row >= 0);
    QCOMPARE(log->entry(row).state, UiUtils::ActivityLog::State::Waiting);
    QCOMPARE(log->entry(row).action, UiUtils::ActivityLog::Action::ReviewRule);
    cEmpty();

    // Run by hand, as from the rules dialog, it moves the message
    const QModelIndex fourth = msgListA.model()->index(3, 0, msgListA);
    const MailRules::Message message = UiUtils::NewMailWatcher::message(fourth, QStringLiteral("work"), QStringLiteral("a"));
    QCOMPARE(m_automation->apply({rule}, QStringLiteral("work"), QStringLiteral("a"), {message}), 1);
    // The server has no MOVE, so the message is copied and marked as deleted
    cClient(t.mk("UID COPY 4 b\r\n"));
    cServer(t.last("OK copied\r\n"));
    cClient(t.mk("UID STORE 4 +FLAGS.SILENT \\Deleted\r\n"));
    cServer(t.last("OK stored\r\n"));
    const int applied = log->find(QStringLiteral("rules:work:a"));
    QVERIFY(applied >= 0);
    QCOMPARE(log->entry(applied).state, UiUtils::ActivityLog::State::Done);
    cEmpty();
}

void MailAutomationTest::readElsewhereLeavesTheNotification()
{
    deliverFourth(QStringLiteral("Hello"));
    QCOMPARE(m_shown->shows, 1);
    cServer("* 4 FETCH (FLAGS (\\Seen))\r\n");
    QCoreApplication::processEvents();
    QVERIFY(m_automation->digest().isEmpty(QStringLiteral("work")));
    QVERIFY(!m_shown->shown.contains(QStringLiteral("work")));
    cEmpty();
}

void MailAutomationTest::removedAccountsAreForgotten()
{
    MailRules::Rule rule;
    rule.id = QStringLiteral("r3");
    rule.name = QStringLiteral("Work only");
    rule.accountId = QStringLiteral("work");
    rule.actions = {{MailRules::ActionKind::Flag, QString()}};
    m_automation->setRules({rule});
    initialMessages(1);
    m_automation->addAccount(QStringLiteral("work"), QStringLiteral("Work"), model);
    QCoreApplication::processEvents();
    QSignalSpy rulesChanged(m_automation.get(), &MailAutomation::rulesChanged);
    m_automation->forgetAccount(QStringLiteral("work"));
    QCOMPARE(rulesChanged.count(), 1);
    QVERIFY(m_automation->rules().isEmpty());
    QVERIFY(m_automation->accounts().isEmpty());
    QVERIFY(!m_automation->notificationSettings().hasFolderSetting(QStringLiteral("work"), QStringLiteral("a")));
    QVERIFY(m_state->group(QStringLiteral("NewMailSeen")).keyList().isEmpty());
    cEmpty();
}

QTEST_GUILESS_MAIN(MailAutomationTest)
#include "test_MailAutomation.moc"
