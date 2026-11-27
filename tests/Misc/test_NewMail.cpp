/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QTest>

#include "UiUtils/NewMail.h"

using namespace UiUtils::NewMail;
using UiUtils::MailRules::Message;

namespace
{

const QDateTime now(QDate(2026, 11, 27), QTime(12, 0), QTimeZone::UTC);

Message message(const QString &folder = QStringLiteral("INBOX"))
{
    Message m;
    m.accountId = QStringLiteral("work");
    m.folder = folder;
    m.uid = 5;
    m.received = now.addSecs(-60);
    m.messageId = QStringLiteral("new@example.org");
    return m;
}

Entry entry(uint uid, const QString &sender, const QString &subject, int minutesAgo, const QString &folder = QStringLiteral("INBOX"))
{
    return Entry{folder, uid, sender, subject, QString(), now.addSecs(-60 * minutesAgo)};
}

}

class NewMailTest : public QObject
{
    Q_OBJECT

private slots:
    void folderDefaults()
    {
        Settings settings;
        QVERIFY(settings.announces(QStringLiteral("work"), QStringLiteral("INBOX")));
        QVERIFY(settings.announces(QStringLiteral("work"), QStringLiteral("inbox")));
        QVERIFY(!settings.announces(QStringLiteral("work"), QStringLiteral("Lists")));
        settings.setAnnounces(QStringLiteral("work"), QStringLiteral("Lists"), true);
        settings.setAnnounces(QStringLiteral("work"), QStringLiteral("INBOX"), false);
        QVERIFY(settings.announces(QStringLiteral("work"), QStringLiteral("Lists")));
        QVERIFY(!settings.announces(QStringLiteral("work"), QStringLiteral("INBOX")));
        QVERIFY(settings.announces(QStringLiteral("home"), QStringLiteral("INBOX")));
        QCOMPARE(settings.extraFolders(QStringLiteral("work")), QStringList{QStringLiteral("Lists")});
        // Back to the defaults, nothing of the account is left
        settings.setAnnounces(QStringLiteral("work"), QStringLiteral("Lists"), false);
        settings.setAnnounces(QStringLiteral("work"), QStringLiteral("INBOX"), true);
        QVERIFY(settings.folders.isEmpty());
        QCOMPARE(settings.mode(QStringLiteral("work")), Mode::Each);
    }

    void classify_data()
    {
        QTest::addColumn<int>("expected");
        QTest::addColumn<QString>("folder");
        QTest::addColumn<bool>("unread");
        QTest::addColumn<int>("receivedMinutesAgo");
        QTest::addColumn<QStringList>("reading");
        QTest::addColumn<bool>("ruleNotify");
        QTest::addColumn<bool>("off");

        const QString inbox = QStringLiteral("INBOX");
        QTest::newRow("new") << static_cast<int>(Skip::None) << inbox << true << 1 << QStringList() << false << false;
        QTest::newRow("account off") << static_cast<int>(Skip::AccountOff) << inbox << true << 1 << QStringList() << true << true;
        QTest::newRow("quiet folder") << static_cast<int>(Skip::FolderQuiet) << QStringLiteral("Lists") << true << 1
                                      << QStringList() << false << false;
        QTest::newRow("rule asks") << static_cast<int>(Skip::None) << QStringLiteral("Lists") << true << 1 << QStringList()
                                   << true << false;
        QTest::newRow("read") << static_cast<int>(Skip::Read) << inbox << false << 1 << QStringList() << false << false;
        QTest::newRow("moved in") << static_cast<int>(Skip::Old) << inbox << true << 60 * 24 << QStringList() << false << false;
        QTest::newRow("delayed delivery") << static_cast<int>(Skip::None) << inbox << true << 25 << QStringList() << false << false;
        QTest::newRow("being read") << static_cast<int>(Skip::Reading) << inbox << true << 1
                                    << QStringList{QStringLiteral("new@example.org")} << false << false;
        QTest::newRow("reply to what is read") << static_cast<int>(Skip::Reading) << inbox << true << 1
                                               << QStringList{QStringLiteral("parent@example.org")} << false << false;
    }

    void classify()
    {
        QFETCH(int, expected);
        QFETCH(QString, folder);
        QFETCH(bool, unread);
        QFETCH(int, receivedMinutesAgo);
        QFETCH(QStringList, reading);
        QFETCH(bool, ruleNotify);
        QFETCH(bool, off);
        Settings settings;
        if (off)
            settings.accountModes.insert(QStringLiteral("work"), Mode::Off);
        Message m = message(folder);
        m.unread = unread;
        m.received = now.addSecs(-60 * receivedMinutesAgo);
        m.inReplyTo = {QStringLiteral("parent@example.org")};
        // Checked 20 minutes ago: anything received well before that was moved in
        const QDateTime lastCheck = now.addSecs(-20 * 60);
        QCOMPARE(static_cast<int>(UiUtils::NewMail::classify(m, settings, lastCheck, reading, ruleNotify)), expected);
    }

    void noLastCheckSkipsTheAgeCheck()
    {
        Message m = message();
        m.received = now.addYears(-1);
        QCOMPARE(UiUtils::NewMail::classify(m, Settings(), QDateTime(), {}), Skip::None);
    }

    void delivery()
    {
        QCOMPARE(UiUtils::NewMail::delivery(Mode::Each, false, true, {}, now, 15), Delivery::Now);
        QCOMPARE(UiUtils::NewMail::delivery(Mode::Each, true, true, {}, now, 15), Delivery::Hold);
        QCOMPARE(UiUtils::NewMail::delivery(Mode::Each, true, false, {}, now, 15), Delivery::Now);
        QCOMPARE(UiUtils::NewMail::delivery(Mode::Off, false, true, {}, now, 15), Delivery::Never);
        QCOMPARE(UiUtils::NewMail::delivery(Mode::Summary, false, true, {}, now, 15), Delivery::Now);
        QCOMPARE(UiUtils::NewMail::delivery(Mode::Summary, false, true, now.addSecs(-5 * 60), now, 15), Delivery::Hold);
        QCOMPARE(UiUtils::NewMail::delivery(Mode::Summary, false, true, now.addSecs(-15 * 60), now, 15), Delivery::Now);
        QCOMPARE(nextSummary(now.addSecs(-5 * 60), now, 15), now.addSecs(10 * 60));
        QVERIFY(!nextSummary(now.addSecs(-20 * 60), now, 15).isValid());
    }

    void digestKeepsOneEntryPerMessage()
    {
        Digest digest;
        QVERIFY(digest.add(QStringLiteral("work"), {entry(1, QStringLiteral("Ann"), QStringLiteral("One"), 3)}));
        QVERIFY(!digest.add(QStringLiteral("work"), {entry(1, QStringLiteral("Ann"), QStringLiteral("One"), 3)}));
        QVERIFY(digest.add(QStringLiteral("work"), {entry(2, QStringLiteral("Bob"), QStringLiteral("Two"), 1)}));
        QVERIFY(digest.addCount(QStringLiteral("work"), QStringLiteral("Lists/KDE"), 4));
        QVERIFY(!digest.addCount(QStringLiteral("work"), QStringLiteral("Lists/KDE"), 0));
        QCOMPARE(digest.count(QStringLiteral("work")), 6);
        QCOMPARE(digest.accounts(), QStringList{QStringLiteral("work")});

        const Text text = digest.text(QStringLiteral("work"), QStringLiteral("Work"), true);
        QCOMPARE(text.title, QStringLiteral("6 new messages in Work"));
        QCOMPARE(text.body, QStringLiteral("Bob: Two\nAnn: One\n4 new in KDE"));
        QVERIFY(!text.single);

        const Text hidden = digest.text(QStringLiteral("work"), QStringLiteral("Work"), false);
        QCOMPARE(hidden.body, QStringLiteral("2 in Inbox\n4 new in KDE"));

        QVERIFY(digest.clearCount(QStringLiteral("work"), QStringLiteral("Lists/KDE")));
        QVERIFY(digest.remove(QStringLiteral("work"), QStringLiteral("inbox"), 2));
        QVERIFY(!digest.remove(QStringLiteral("work"), QStringLiteral("INBOX"), 2));
        const Text single = digest.text(QStringLiteral("work"), QStringLiteral("Work"), true);
        QCOMPARE(single.title, QStringLiteral("New message in Work"));
        QVERIFY(single.single);
        digest.clear(QStringLiteral("work"));
        QVERIFY(digest.isEmpty(QStringLiteral("work")));
        QVERIFY(digest.accounts().isEmpty());
        QCOMPARE(digest.text(QStringLiteral("work"), QStringLiteral("Work"), true).count, 0);
    }

    void digestListsAFewMessages()
    {
        Digest digest;
        QList<Entry> entries;
        for (uint uid = 1; uid <= 8; ++uid)
            entries << entry(uid, QStringLiteral("S%1").arg(uid), QStringLiteral("T%1").arg(uid), 10 - static_cast<int>(uid));
        digest.add(QStringLiteral("work"), entries);
        const QStringList lines = digest.text(QStringLiteral("work"), QStringLiteral("Work"), true).body.split(QLatin1Char('\n'));
        QCOMPARE(lines.first(), QStringLiteral("S8: T8"));
        QCOMPARE(lines.last(), QStringLiteral("and %1 more").arg(8 - (lines.size() - 1)));
    }
};

QTEST_GUILESS_MAIN(NewMailTest)

#include "test_NewMail.moc"
