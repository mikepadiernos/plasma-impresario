/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QDataStream>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>
#include <memory>

#include <KConfig>
#include <KConfigGroup>

#include "Imap/Model/CombinedCache.h"
#include "Imap/Model/MailboxMetadata.h"
#include "Imap/Model/SearchIndex.h"
#include "Imap/Model/SearchIndexer.h"
#include "Imap/Parser/LowLevelParser.h"
#include "Imap/Parser/Message.h"
#include "UiUtils/MailStatus.h"
#include "UiUtils/PlasmaSearch.h"

using namespace Imap::Mailbox;
namespace MailStatus = UiUtils::MailStatus;
using MailStatus::Sharing;

namespace {

QByteArray plainStructure()
{
    int start = 0;
    const QVariantList list = Imap::LowLevelParser::parseList(
        '(', ')', "(\"text\" \"plain\" (\"charset\" \"utf-8\") NIL NIL \"8bit\" 40 1 NIL NIL NIL NIL)", start);
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_4_6);
    stream << list;
    return buffer;
}

AbstractCache::MessageDataBundle cachedMessage(uint uid, const QString &subject, const QString &from, const QDate &sent)
{
    AbstractCache::MessageDataBundle data;
    data.uid = uid;
    const QList<Imap::Message::MailAddress> sender{
        Imap::Message::MailAddress(from, QString(), from.toLower(), QStringLiteral("example.org"))};
    const QDateTime date = sent.isValid() ? QDateTime(sent, QTime(9, 0), QTimeZone::utc()) : QDateTime();
    data.envelope = Imap::Message::Envelope(date, subject, sender, sender, sender, sender, {}, {}, {},
                                            QByteArray("<") + QByteArray::number(uid) + "@example.org>");
    data.internalDate = QDateTime(QDate(2026, 11, 1), QTime(10, 0), QTimeZone::utc());
    data.size = 1000;
    data.serializedBodyStructure = plainStructure();
    data.hdrListPostNo = false;
    return data;
}

SyncState syncState(uint uidValidity, uint exists, uint uidNext)
{
    SyncState state;
    state.setUidValidity(uidValidity);
    state.setExists(exists);
    state.setUidNext(uidNext);
    state.setRecent(0);
    state.setUnSeenCount(0);
    return state;
}

/** An account with its cache and an index built from it */
struct IndexedAccount {
    std::shared_ptr<CombinedCache> cache;
    std::unique_ptr<SearchIndexer> indexer;
};

}

/** @short What the Plasma widgets may learn: the sharing setting, the records sent and the unread messages */
class MailStatusTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void sharingSetting();
    void sharingNames();
    void accountEntries();
    void messageEntries();
    void choosing();
    void unreadMessages();

private:
    bool buildIndex(IndexedAccount &account, const QString &name);

    QTemporaryDir m_dir;
};

void MailStatusTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(m_dir.isValid());
}

void MailStatusTest::sharingSetting()
{
    const QString path = m_dir.filePath(QStringLiteral("widgetsrc"));
    {
        KConfig config(path, KConfig::SimpleConfig);
        // Counts by default, nothing about the messages
        QCOMPARE(MailStatus::sharing(config), Sharing::Counts);
        MailStatus::setSharing(config, Sharing::Subjects);
        QCOMPARE(MailStatus::sharing(config), Sharing::Subjects);
    }
    {
        KConfig reread(path, KConfig::SimpleConfig);
        QCOMPARE(MailStatus::sharing(reread), Sharing::Subjects);
        MailStatus::setSharing(reread, Sharing::Nothing);
    }
    {
        KConfig again(path, KConfig::SimpleConfig);
        QCOMPARE(MailStatus::sharing(again), Sharing::Nothing);
        // A value written by a newer version, or by hand, shares nothing rather than guessing
        again.group(QStringLiteral("PlasmaWidgets")).writeEntry("Share", QStringLiteral("everything"));
        QCOMPARE(MailStatus::sharing(again), Sharing::Nothing);
    }
}

void MailStatusTest::sharingNames()
{
    for (Sharing sharing : {Sharing::Nothing, Sharing::Counts, Sharing::Senders, Sharing::Subjects})
        QCOMPARE(MailStatus::sharingFromName(MailStatus::sharingName(sharing)), sharing);
    QCOMPARE(MailStatus::sharingName(Sharing::Senders), QStringLiteral("senders"));
    QCOMPARE(MailStatus::sharingFromName(QString()), Sharing::Nothing);
    QCOMPARE(MailStatus::sharingFromName(QStringLiteral("Subjects")), Sharing::Nothing);
    QCOMPARE(MailStatus::stateName(MailStatus::State::SignInNeeded), QStringLiteral("signInNeeded"));
    QCOMPARE(MailStatus::stateName(MailStatus::State::Online), QStringLiteral("online"));
}

void MailStatusTest::accountEntries()
{
    MailStatus::Account account;
    account.id = QStringLiteral("work");
    account.name = QStringLiteral("Work");
    account.unread = 4;
    account.state = MailStatus::State::Offline;

    QVERIFY(MailStatus::toMap(account, Sharing::Nothing).isEmpty());
    const QVariantMap entry = MailStatus::toMap(account, Sharing::Counts);
    QCOMPARE(entry.keys(), (QStringList{QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("state"),
                                        QStringLiteral("unread")}));
    QCOMPARE(entry.value(QStringLiteral("unread")).toInt(), 4);
    QCOMPARE(entry.value(QStringLiteral("state")).toString(), QStringLiteral("offline"));
    QCOMPARE(MailStatus::toMap(account, Sharing::Subjects), entry);

    account.unread = -7;
    QCOMPARE(MailStatus::toMap(account, Sharing::Counts).value(QStringLiteral("unread")).toInt(), -1);
}

void MailStatusTest::messageEntries()
{
    MailStatus::Message message;
    message.id = QStringLiteral("message/work/INBOX/9/3");
    message.accountId = QStringLiteral("work");
    message.accountName = QStringLiteral("Work");
    message.folder = QStringLiteral("INBOX");
    message.sender = QStringLiteral("Carol");
    message.subject = QStringLiteral("Quarterly report");
    message.sent = QDate(2026, 11, 26);

    QVERIFY(MailStatus::toMap(message, Sharing::Nothing).isEmpty());
    QVERIFY(MailStatus::toMap(message, Sharing::Counts).isEmpty());

    // Senders leave the subject out entirely, not as an empty value
    const QVariantMap senders = MailStatus::toMap(message, Sharing::Senders);
    QVERIFY(!senders.contains(QStringLiteral("subject")));
    QCOMPARE(senders.value(QStringLiteral("sender")).toString(), QStringLiteral("Carol"));
    QCOMPARE(senders.value(QStringLiteral("sent")).toString(), QStringLiteral("2026-11-26"));
    QCOMPARE(senders.value(QStringLiteral("id")).toString(), message.id);

    const QVariantMap subjects = MailStatus::toMap(message, Sharing::Subjects);
    QCOMPARE(subjects.value(QStringLiteral("subject")).toString(), QStringLiteral("Quarterly report"));
    QCOMPARE(subjects.size(), senders.size() + 1);

    message.sent = QDate();
    QCOMPARE(MailStatus::toMap(message, Sharing::Senders).value(QStringLiteral("sent")).toString(), QString());
}

void MailStatusTest::choosing()
{
    QVERIFY(MailStatus::isChosen({}, QStringLiteral("work")));
    QVERIFY(MailStatus::isChosen({QStringLiteral("work")}, QStringLiteral("work")));
    QVERIFY(!MailStatus::isChosen({QStringLiteral("work")}, QStringLiteral("Work")));
    QVERIFY(!MailStatus::isChosen({QStringLiteral("inbox")}, QStringLiteral("INBOX")));
    QVERIFY(MailStatus::isChosen({QStringLiteral("inbox")}, QStringLiteral("INBOX"), true));
    QVERIFY(MailStatus::isChosen({QStringLiteral("INBOX")}, QStringLiteral("Inbox"), true));
    QVERIFY(!MailStatus::isChosen({QStringLiteral("Archive")}, QStringLiteral("archive"), true));
}

bool MailStatusTest::buildIndex(IndexedAccount &account, const QString &name)
{
    if (!QDir(m_dir.path()).mkpath(name))
        return false;
    account.cache = std::make_shared<CombinedCache>(QStringLiteral("mail-status-test-") + name, m_dir.filePath(name));
    account.cache->setErrorHandler([](const QString &error) { QFAIL(qPrintable(error)); });
    if (!account.cache->open())
        return false;
    std::shared_ptr<CombinedCache> cache = account.cache;
    account.indexer = std::make_unique<SearchIndexer>([cache]() { return std::static_pointer_cast<AbstractCache>(cache); });
    account.indexer->setTimings(0, 0, 2);
    if (!account.indexer->open())
        return false;
    return true;
}

void MailStatusTest::unreadMessages()
{
    IndexedAccount work;
    QVERIFY(buildIndex(work, QStringLiteral("work")));
    IndexedAccount home;
    QVERIFY(buildIndex(home, QStringLiteral("home")));

    const QString inbox = QStringLiteral("INBOX");
    const QString lists = QStringLiteral("Lists");
    work.cache->setChildMailboxes(QString(), {
        MailboxMetadata(inbox, QStringLiteral("/"), {QStringLiteral("\\HasNoChildren")}),
        MailboxMetadata(lists, QStringLiteral("/"), {QStringLiteral("\\HasNoChildren")}),
    });
    work.cache->setMailboxSyncState(inbox, syncState(100, 4, 5));
    work.cache->setUidMapping(inbox, Imap::Uids{1, 2, 3, 4});
    const QList<std::tuple<QString, QString, QDate, QStringList>> workInbox{
        {QStringLiteral("Unread"), QStringLiteral("Alice"), QDate(2026, 11, 24), {}},
        {QStringLiteral("Read"), QStringLiteral("Bob"), QDate(2026, 11, 25), {QStringLiteral("\\Seen")}},
        {QStringLiteral("Deleted"), QStringLiteral("Carol"), QDate(2026, 11, 26), {QStringLiteral("\\Deleted")}},
        {QStringLiteral("Undated"), QStringLiteral("Dave"), QDate(), {QStringLiteral("\\Flagged")}},
    };
    for (uint uid = 1; uid <= 4; ++uid) {
        const auto &[subject, from, sent, flags] = workInbox.at(uid - 1);
        work.cache->setMessageMetadata(inbox, uid, cachedMessage(uid, subject, from, sent));
        work.cache->setMsgFlags(inbox, uid, flags);
    }
    work.cache->setMailboxSyncState(lists, syncState(200, 1, 2));
    work.cache->setUidMapping(lists, Imap::Uids{1});
    work.cache->setMessageMetadata(lists, 1, cachedMessage(1, QStringLiteral("Digest"), QStringLiteral("List"), QDate(2026, 11, 27)));
    work.cache->setMsgFlags(lists, 1, {});

    home.cache->setChildMailboxes(QString(), {MailboxMetadata(inbox, QStringLiteral("/"), {QStringLiteral("\\HasNoChildren")})});
    home.cache->setMailboxSyncState(inbox, syncState(7, 1, 2));
    home.cache->setUidMapping(inbox, Imap::Uids{1});
    home.cache->setMessageMetadata(inbox, 1, cachedMessage(1, QStringLiteral("Dinner"), QStringLiteral("Erin"), QDate(2026, 11, 25)));
    home.cache->setMsgFlags(inbox, 1, {});

    for (IndexedAccount *account : {&work, &home}) {
        QSignalSpy finished(account->indexer.get(), &SearchIndexer::finished);
        account->indexer->update();
        QVERIFY(finished.wait(5000));
    }

    const QList<MailStatus::Source> sources{
        {QStringLiteral("work"), QStringLiteral("Work"), work.indexer->index()},
        {QStringLiteral("home"), QStringLiteral("Home"), home.indexer->index()},
        // An account without an index gives nothing, and does not stop the others
        {QStringLiteral("none"), QStringLiteral("None"), nullptr},
    };
    const auto subjects = [](const QList<MailStatus::Message> &messages) {
        QStringList out;
        for (const auto &message : messages)
            out << message.accountId + QLatin1Char(':') + message.subject;
        return out;
    };

    // Newest first across the accounts; read and deleted messages are left out, undated ones come last
    const auto all = MailStatus::unreadMessages(sources, {}, {});
    QCOMPARE(subjects(all), (QStringList{QStringLiteral("work:Digest"), QStringLiteral("home:Dinner"),
                                         QStringLiteral("work:Unread"), QStringLiteral("work:Undated")}));
    const MailStatus::Message &digest = all.constFirst();
    QCOMPARE(digest.accountName, QStringLiteral("Work"));
    QCOMPARE(digest.folder, lists);
    QCOMPARE(digest.sent, QDate(2026, 11, 27));
    QVERIFY(digest.sender.contains(QLatin1String("List")));
    // The ID opens the message the way Plasma Search does
    UiUtils::PlasmaSearch::Target target;
    QVERIFY(UiUtils::PlasmaSearch::parseMatchId(digest.id, &target));
    QCOMPARE(target.accountId, QStringLiteral("work"));
    QCOMPARE(target.mailbox, lists);
    QCOMPARE(target.uidValidity, 200u);
    QCOMPARE(target.uid, 1u);

    // Folders: the INBOX in any case, and other folders by their exact name
    QCOMPARE(subjects(MailStatus::unreadMessages(sources, {}, {QStringLiteral("inbox")})),
             (QStringList{QStringLiteral("home:Dinner"), QStringLiteral("work:Unread"), QStringLiteral("work:Undated")}));
    QCOMPARE(subjects(MailStatus::unreadMessages(sources, {}, {lists})), QStringList{QStringLiteral("work:Digest")});
    QVERIFY(MailStatus::unreadMessages(sources, {}, {QStringLiteral("lists")}).isEmpty());
    QVERIFY(MailStatus::unreadMessages(sources, {}, {QString()}).isEmpty());

    // Accounts, and the limit
    QCOMPARE(subjects(MailStatus::unreadMessages(sources, {QStringLiteral("home")}, {})), QStringList{QStringLiteral("home:Dinner")});
    QVERIFY(MailStatus::unreadMessages(sources, {QStringLiteral("none")}, {}).isEmpty());
    QCOMPARE(subjects(MailStatus::unreadMessages(sources, {}, {}, 2)),
             (QStringList{QStringLiteral("work:Digest"), QStringLiteral("home:Dinner")}));
    QVERIFY(MailStatus::unreadMessages(sources, {}, {}, 0).isEmpty());
    QCOMPARE(MailStatus::unreadMessages(sources, {}, {}, 1000).size(), 4);
    QVERIFY(MailStatus::unreadMessages({}, {}, {}).isEmpty());

    // Reading a message on another device takes it out once the index learns of it
    work.cache->setMsgFlags(lists, 1, {QStringLiteral("\\Seen")});
    QSignalSpy finished(work.indexer.get(), &SearchIndexer::finished);
    work.indexer->update();
    QVERIFY(finished.wait(5000));
    QCOMPARE(subjects(MailStatus::unreadMessages(sources, {QStringLiteral("work")}, {})),
             (QStringList{QStringLiteral("work:Unread"), QStringLiteral("work:Undated")}));
}

QTEST_GUILESS_MAIN(MailStatusTest)
#include "test_MailStatus.moc"
