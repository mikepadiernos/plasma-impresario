/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/TaskFactory.h"
#include "LocalMail/ImapSession.h"
#include "LocalMail/LocalSocket.h"
#include "LocalMail/Store.h"
#include "Utils/LibMailboxSync.h"

using namespace LocalMail;

namespace {

const QString inbox = QStringLiteral("INBOX");

QByteArray message(const QByteArray &subject, const QByteArray &body = "Hello there\r\n", const QByteArray &extra = {})
{
    return "From: Alice <alice@example.org>\r\nTo: bob@example.org\r\nSubject: " + subject
        + "\r\nDate: Mon, 02 Nov 2026 08:00:00 +0000\r\nMessage-ID: <" + subject + "@example.org>\r\n" + extra + "\r\n" + body;
}

/** Talks to one session the way a client would, collecting what it answers */
class Client
{
public:
    explicit Client(std::shared_ptr<Store> store)
        : session(std::move(store))
    {
        QObject::connect(&session, &ImapSession::output, [this](const QByteArray &data) { received += data; });
        session.start();
    }

    QByteArray take()
    {
        const QByteArray data = received;
        received.clear();
        return data;
    }

    /** Send @arg line and return everything up to its tagged answer */
    QByteArray command(const QByteArray &line)
    {
        take();
        session.receive(line + "\r\n");
        return take();
    }

    ImapSession session;
    QByteArray received;
};

}

class LocalImapServerTest : public QObject
{
    Q_OBJECT
private:
    QTemporaryDir m_dir;
    std::shared_ptr<Store> m_store;

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        m_store.reset();
        m_store = Store::open(m_dir.path() + QLatin1String("/") + QString::fromLatin1(QTest::currentTestFunction()));
        QVERIFY(m_store);
    }

    void cleanup()
    {
        m_store.reset();
    }

    void greetsAuthenticated()
    {
        Client client(m_store);
        QCOMPARE(client.take(), QByteArray("* PREAUTH [CAPABILITY IMAP4rev1 LITERAL+ UIDPLUS UNSELECT IDLE MOVE CHILDREN] Mail kept on this computer\r\n"));
        QCOMPARE(client.command("a CAPABILITY"), QByteArray("* CAPABILITY IMAP4rev1 LITERAL+ UIDPLUS UNSELECT IDLE MOVE CHILDREN\r\na OK CAPABILITY completed\r\n"));
        QCOMPARE(client.command("b FROB"), QByteArray("b BAD Unknown command FROB\r\n"));
        QCOMPARE(client.command("c FETCH 1 FLAGS"), QByteArray("c BAD No folder is selected\r\n"));
        QSignalSpy finished(&client.session, &ImapSession::finished);
        QCOMPARE(client.command("d LOGOUT"), QByteArray("* BYE Logging out\r\nd OK LOGOUT completed\r\n"));
        QCOMPARE(finished.count(), 1);
    }

    void listsFolders()
    {
        m_store->createMailbox(QStringLiteral("Work/Projects"));
        m_store->createMailbox(QStringLiteral("J\u00fcrgen"));
        Client client(m_store);
        client.take();
        QCOMPARE(client.command("a LIST \"\" \"\""), QByteArray("* LIST (\\Noselect) \"/\" \"\"\r\na OK LIST completed\r\n"));
        const QByteArray top = client.command("b LIST \"\" %");
        QVERIFY(top.contains("* LIST (\\HasNoChildren) \"/\" \"INBOX\"\r\n"));
        QVERIFY(top.contains("* LIST (\\Noselect \\HasChildren) \"/\" \"Work\"\r\n"));
        QVERIFY(top.contains("* LIST (\\HasNoChildren) \"/\" \"J&APw-rgen\"\r\n"));
        QVERIFY(!top.contains("Projects"));
        const QByteArray children = client.command("c LIST \"\" \"Work/%\"");
        QCOMPARE(children, QByteArray("* LIST (\\HasNoChildren) \"/\" \"Work/Projects\"\r\nc OK LIST completed\r\n"));
        QVERIFY(client.command("d LIST \"\" *").contains("Work/Projects"));
        QVERIFY(client.command("e LIST \"\" inbox").contains("\"INBOX\""));

        m_store->setSubscribed(QStringLiteral("Trash"), false);
        const QByteArray subscribed = client.command("f LSUB \"\" *");
        QVERIFY(!subscribed.contains("Trash"));
        QVERIFY(subscribed.contains("Sent"));
    }

    void managesFolders()
    {
        Client client(m_store);
        client.take();
        QCOMPARE(client.command("a CREATE \"New/Sub\""), QByteArray("a OK CREATE completed\r\n"));
        QVERIFY(m_store->hasMailbox(QStringLiteral("New/Sub")));
        QVERIFY(client.command("b CREATE New/Sub").startsWith("b NO [ALREADYEXISTS]"));
        QCOMPARE(client.command("c RENAME New/Sub Other"), QByteArray("c OK RENAME completed\r\n"));
        QVERIFY(m_store->hasMailbox(QStringLiteral("Other")));
        QCOMPARE(client.command("d UNSUBSCRIBE Other"), QByteArray("d OK UNSUBSCRIBE completed\r\n"));
        QVERIFY(!m_store->mailbox(QStringLiteral("Other")).subscribed);
        QCOMPARE(client.command("e DELETE Other"), QByteArray("e OK DELETE completed\r\n"));
        QVERIFY(client.command("f DELETE INBOX").startsWith("f NO"));
        QVERIFY(client.command("g SELECT Missing").startsWith("g NO [NONEXISTENT]"));
    }

    void selectsAndFetches()
    {
        m_store->append(inbox, message("first"), {QStringLiteral("\\Seen")}, QDateTime(QDate(2026, 11, 2), QTime(8, 1), QTimeZone::UTC));
        m_store->append(inbox, message("second", "Text body\r\n"));
        Client client(m_store);
        client.take();
        const QByteArray status = client.command("s STATUS INBOX (MESSAGES UNSEEN UIDNEXT RECENT)");
        QCOMPARE(status, QByteArray("* STATUS \"INBOX\" (MESSAGES 2 UNSEEN 1 UIDNEXT 3 RECENT 0)\r\ns OK STATUS completed\r\n"));

        const QByteArray selected = client.command("a SELECT INBOX");
        QVERIFY(selected.contains("* 2 EXISTS\r\n"));
        QVERIFY(selected.contains("* 0 RECENT\r\n"));
        QVERIFY(selected.contains("* OK [UIDNEXT 3]"));
        QVERIFY(selected.contains(QByteArray("* OK [UIDVALIDITY " + QByteArray::number(m_store->mailbox(inbox).uidValidity) + ']')));
        QVERIFY(selected.contains("[PERMANENTFLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft \\*)]"));
        QVERIFY(selected.endsWith("a OK [READ-WRITE] SELECT completed\r\n"));

        QCOMPARE(client.command("b FETCH 1 (FLAGS INTERNALDATE RFC822.SIZE)"),
                 "* 1 FETCH (INTERNALDATE \"02-Nov-2026 08:01:00 +0000\" RFC822.SIZE "
                     + QByteArray::number(message("first").size()) + " FLAGS (\\Seen))\r\nb OK FETCH completed\r\n");
        const QByteArray envelope = client.command("c UID FETCH 1:* (ENVELOPE)");
        QVERIFY(envelope.contains("* 1 FETCH (UID 1 ENVELOPE (\"Mon, 02 Nov 2026 08:00:00 +0000\" \"first\""));
        QVERIFY(envelope.contains("* 2 FETCH (UID 2 ENVELOPE"));

        QCOMPARE(client.command("d UID FETCH 2 (BODY.PEEK[1])"), QByteArray("* 2 FETCH (UID 2 BODY[1] {11}\r\nText body\r\n)\r\nd OK FETCH completed\r\n"));
        QVERIFY(!m_store->messages(inbox)[1].flags.contains(QStringLiteral("\\Seen")));
        QCOMPARE(client.command("e FETCH 2 (BODY[TEXT]<5.4>)"), QByteArray("* 2 FETCH (BODY[TEXT]<5> \"body\" FLAGS (\\Seen))\r\ne OK FETCH completed\r\n"));
        QVERIFY(m_store->messages(inbox)[1].flags.contains(QStringLiteral("\\Seen")));
        QVERIFY(client.command("f UID FETCH 1 (BODY.PEEK[HEADER.FIELDS (SUBJECT)])").contains("BODY[HEADER.FIELDS (SUBJECT)] {18}\r\nSubject: first\r\n\r\n)"));
        QVERIFY(client.command("g FETCH 1 BODYSTRUCTURE").contains("BODYSTRUCTURE (\"TEXT\" \"PLAIN\""));
        QVERIFY(client.command("h FETCH 3 FLAGS").startsWith("h BAD"));
        QVERIFY(client.command("i UID FETCH 7 FLAGS").startsWith("i OK"));
        QVERIFY(client.command("j UID FETCH 3:* FLAGS").startsWith("* 2 FETCH (UID 2 FLAGS"));
        QVERIFY(client.command("k FETCH 1 (BOGUS)").startsWith("k BAD"));
    }

    void acceptsLiterals()
    {
        Client client(m_store);
        client.take();
        const QByteArray mail = message("literal");
        client.session.receive("a APPEND INBOX (\\Seen $Work) \"01-Feb-2026 10:00:00 +0200\" {" + QByteArray::number(mail.size()) + "}\r\n");
        QCOMPARE(client.take(), QByteArray("+ Ready for literal data\r\n"));
        client.session.receive(mail.left(10));
        QCOMPARE(client.take(), QByteArray());
        client.session.receive(mail.mid(10) + "\r\n");
        QCOMPARE(client.take(), "a OK [APPENDUID " + QByteArray::number(m_store->mailbox(inbox).uidValidity) + " 1] APPEND completed\r\n");
        QCOMPARE(m_store->content(inbox, 1), mail);
        QCOMPARE(m_store->messages(inbox)[0].flags, (QStringList{QStringLiteral("\\Seen"), QStringLiteral("$Work")}));
        QCOMPARE(m_store->messages(inbox)[0].internalDate, QDateTime(QDate(2026, 2, 1), QTime(8, 0), QTimeZone::UTC));

        // LITERAL+ needs no continuation, and a quoted folder name may hold spaces
        m_store->createMailbox(QStringLiteral("My Folder"));
        QCOMPARE(client.command("b APPEND \"My Folder\" {" + QByteArray::number(mail.size()) + "+}\r\n" + mail),
                 "b OK [APPENDUID " + QByteArray::number(m_store->mailbox(QStringLiteral("My Folder")).uidValidity) + " 1] APPEND completed\r\n");
        QVERIFY(client.command("c APPEND Nowhere {1+}\r\nx").startsWith("c NO [TRYCREATE]"));
    }

    void storesFlagsAndExpunges()
    {
        for (int i = 1; i <= 4; ++i)
            m_store->append(inbox, message("m" + QByteArray::number(i)));
        Client client(m_store);
        client.take();
        client.command("a SELECT INBOX");
        QCOMPARE(client.command("b STORE 2:3 +FLAGS (\\Deleted)"),
                 QByteArray("* 2 FETCH (FLAGS (\\Deleted))\r\n* 3 FETCH (FLAGS (\\Deleted))\r\nb OK STORE completed\r\n"));
        QCOMPARE(client.command("c UID STORE 1 +FLAGS.SILENT (\\Flagged)"), QByteArray("c OK STORE completed\r\n"));
        QCOMPARE(m_store->messages(inbox)[0].flags, QStringList{QStringLiteral("\\Flagged")});
        QCOMPARE(client.command("d UID STORE 1 -FLAGS \\Flagged"), QByteArray("* 1 FETCH (UID 1 FLAGS ())\r\nd OK STORE completed\r\n"));
        QCOMPARE(client.command("e UID EXPUNGE 3"), QByteArray("* 3 EXPUNGE\r\ne OK EXPUNGE completed\r\n"));
        QCOMPARE(client.command("f EXPUNGE"), QByteArray("* 2 EXPUNGE\r\nf OK EXPUNGE completed\r\n"));
        QCOMPARE(m_store->messages(inbox).size(), 2);
        QCOMPARE(client.command("g UID SEARCH ALL"), QByteArray("* SEARCH 1 4\r\ng OK SEARCH completed\r\n"));

        QVERIFY(client.command("h EXAMINE INBOX").contains("[READ-ONLY]"));
        QVERIFY(client.command("i STORE 1 +FLAGS (\\Seen)").startsWith("i NO"));
        client.command("j FETCH 1 BODY[]");
        QVERIFY(m_store->messages(inbox)[0].flags.isEmpty());
    }

    void copiesAndMoves()
    {
        for (int i = 1; i <= 3; ++i)
            m_store->append(inbox, message("m" + QByteArray::number(i)));
        Client client(m_store);
        client.take();
        client.command("a SELECT INBOX");
        const QByteArray trash = QByteArray::number(m_store->mailbox(QStringLiteral("Trash")).uidValidity);
        QCOMPARE(client.command("b UID COPY 1,3 Trash"), "b OK [COPYUID " + trash + " 1,3 1:2] COPY completed\r\n");
        QCOMPARE(m_store->messages(QStringLiteral("Trash")).size(), 2);
        QCOMPARE(client.command("c UID MOVE 2 Trash"), "* OK [COPYUID " + trash + " 2 3] Moved\r\n* 2 EXPUNGE\r\nc OK MOVE completed\r\n");
        QCOMPARE(m_store->messages(inbox).size(), 2);
        QCOMPARE(m_store->content(QStringLiteral("Trash"), 3), message("m2"));
        QVERIFY(client.command("d COPY 1 Nowhere").startsWith("d NO [TRYCREATE]"));
        QCOMPARE(client.command("e COPY 1 INBOX"), "* 3 EXISTS\r\ne OK [COPYUID " + QByteArray::number(m_store->mailbox(inbox).uidValidity) + " 1 4] COPY completed\r\n");
    }

    void searches()
    {
        m_store->append(inbox, message("Invoice", "Please pay\r\n"), {QStringLiteral("\\Seen")}, QDateTime(QDate(2026, 1, 5), QTime(9, 0), QTimeZone::UTC));
        m_store->append(inbox, message("=?UTF-8?Q?Gr=C3=BC=C3=9Fe?=", "UGxlYXNlIGNhbGw=\r\n",
                                       "Content-Transfer-Encoding: base64\r\nX-Tag: blue\r\n"),
                        {QStringLiteral("$Work")}, QDateTime(QDate(2026, 3, 1), QTime(9, 0), QTimeZone::UTC));
        m_store->append(inbox, "From: carol@example.com\r\nSubject: Lunch\r\nDate: Sun, 01 Feb 2026 12:00:00 +0000\r\n\r\nsoon\r\n",
                        {QStringLiteral("\\Flagged")}, QDateTime(QDate(2026, 3, 2), QTime(9, 0), QTimeZone::UTC));
        Client client(m_store);
        client.take();
        client.command("a SELECT INBOX");
        const auto search = [&client](const QByteArray &criteria) {
            const QByteArray answer = client.command("s " + criteria);
            const int end = answer.indexOf("\r\n");
            return answer.left(end);
        };
        QCOMPARE(search("SEARCH ALL"), QByteArray("* SEARCH 1 2 3"));
        QCOMPARE(search("SEARCH UNSEEN"), QByteArray("* SEARCH 2 3"));
        QCOMPARE(search("SEARCH FLAGGED"), QByteArray("* SEARCH 3"));
        QCOMPARE(search("SEARCH KEYWORD $Work"), QByteArray("* SEARCH 2"));
        QCOMPARE(search("SEARCH FROM ALICE"), QByteArray("* SEARCH 1 2"));
        QCOMPARE(search("SEARCH CHARSET UTF-8 SUBJECT {4}\r\ngr\xc3\xbc"), QByteArray("* SEARCH 2"));
        QCOMPARE(search("SEARCH BODY \"please CALL\""), QByteArray("* SEARCH 2"));
        QCOMPARE(search("SEARCH TEXT invoice"), QByteArray("* SEARCH 1"));
        QCOMPARE(search("SEARCH HEADER X-Tag blue"), QByteArray("* SEARCH 2"));
        QCOMPARE(search("SEARCH HEADER X-Tag \"\""), QByteArray("* SEARCH 2"));
        QCOMPARE(search("SEARCH SINCE 1-Mar-2026"), QByteArray("* SEARCH 2 3"));
        QCOMPARE(search("SEARCH BEFORE 1-Mar-2026"), QByteArray("* SEARCH 1"));
        QCOMPARE(search("SEARCH ON 2-Mar-2026"), QByteArray("* SEARCH 3"));
        QCOMPARE(search("SEARCH SENTBEFORE 1-Mar-2026"), QByteArray("* SEARCH 3"));
        QCOMPARE(search("SEARCH OR FLAGGED SEEN"), QByteArray("* SEARCH 1 3"));
        QCOMPARE(search("SEARCH NOT (FROM alice SEEN)"), QByteArray("* SEARCH 2 3"));
        QCOMPARE(search("SEARCH 2:*"), QByteArray("* SEARCH 2 3"));
        QCOMPARE(search("UID SEARCH UID 1,3 UNSEEN"), QByteArray("* SEARCH 3"));
        QCOMPARE(search("SEARCH LARGER 100000"), QByteArray("* SEARCH"));
        QVERIFY(search("SEARCH FROB").startsWith("s BAD"));
        QVERIFY(search("SEARCH CHARSET KOI8-R ALL").startsWith("s NO [BADCHARSET"));
    }

    void reportsOtherChanges()
    {
        m_store->append(inbox, message("one"));
        m_store->append(inbox, message("two"));
        Client client(m_store);
        client.take();
        client.command("a SELECT INBOX");

        // Without IDLE, changes wait for the next command; removals wait for one which allows them
        m_store->append(inbox, message("three"));
        m_store->setFlags(inbox, 2, {QStringLiteral("\\Seen")});
        m_store->remove(inbox, {1});
        QCOMPARE(client.take(), QByteArray());
        const QByteArray fetched = client.command("b FETCH 1 UID");
        QCOMPARE(fetched, QByteArray("* 3 EXISTS\r\n* 2 FETCH (UID 2 FLAGS (\\Seen))\r\nb OK FETCH completed\r\n"));
        QCOMPARE(client.command("c NOOP"), QByteArray("* 1 EXPUNGE\r\nc OK NOOP completed\r\n"));

        QCOMPARE(client.command("d IDLE"), QByteArray("+ idling\r\n"));
        m_store->append(inbox, message("four"));
        QCOMPARE(client.take(), QByteArray("* 3 EXISTS\r\n"));
        m_store->remove(inbox, {2});
        QCOMPARE(client.take(), QByteArray("* 1 EXPUNGE\r\n"));
        QCOMPARE(client.command("DONE"), QByteArray("d OK IDLE terminated\r\n"));
        QCOMPARE(client.command("e UID SEARCH ALL"), QByteArray("* SEARCH 3 4\r\ne OK SEARCH completed\r\n"));
    }

    void servesTheMailModel()
    {
        m_store->append(inbox, message("Welcome", "First body\r\n"));
        m_store->createMailbox(QStringLiteral("Archive"));

        auto model = new Imap::Mailbox::Model(this, std::make_shared<Imap::Mailbox::MemoryCache>(),
                                              Imap::Mailbox::SocketFactoryPtr(new LocalSocketFactory(m_store)),
                                              Imap::Mailbox::TaskFactoryPtr(new Imap::Mailbox::TaskFactory()));
        QStringList errors;
        connect(model, &Imap::Mailbox::Model::imapError, this, [&errors](const QString &error) { errors << error; });
        connect(model, &Imap::Mailbox::Model::networkError, this, [&errors](const QString &error) { errors << error; });
        LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_ONLINE);

        const auto mailboxIndex = [model](const QString &name) {
            for (int row = 0; row < model->rowCount(QModelIndex()); ++row) {
                const QModelIndex index = model->index(row, 0, QModelIndex());
                if (index.data(Imap::Mailbox::RoleMailboxName).toString() == name)
                    return index;
            }
            return QModelIndex();
        };
        model->rowCount(QModelIndex());
        QTRY_VERIFY(mailboxIndex(inbox).isValid());
        QTRY_VERIFY(mailboxIndex(QStringLiteral("Archive")).isValid());

        const QModelIndex list = model->index(0, 0, mailboxIndex(inbox));
        model->rowCount(list);
        QTRY_COMPARE(model->rowCount(list), 1);
        const QModelIndex first = model->index(0, 0, list);
        QTRY_COMPARE(first.data(Imap::Mailbox::RoleMessageSubject).toString(), QStringLiteral("Welcome"));
        QVERIFY(!first.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool());

        const QModelIndex part = model->index(0, 0, first);
        QVERIFY(part.isValid());
        part.data(Imap::Mailbox::RolePartData);
        QTRY_COMPARE(part.data(Imap::Mailbox::RolePartData).toByteArray(), QByteArray("First body\r\n"));

        model->setMessageFlags({first}, QStringLiteral("\\Seen"), Imap::Mailbox::FLAG_ADD);
        QTRY_VERIFY(m_store->messages(inbox)[0].flags.contains(QStringLiteral("\\Seen")));

        // New mail reaches the open folder without a reload
        m_store->append(inbox, message("Later"));
        // The model starts IDLE a few seconds after it opens a folder
        QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(list), 2, 20000);
        QTRY_COMPARE(model->index(1, 0, list).data(Imap::Mailbox::RoleMessageSubject).toString(), QStringLiteral("Later"));

        model->copyMoveMessages(QStringLiteral("Archive"), {first}, Imap::Mailbox::MOVE);
        QTRY_COMPARE(m_store->messages(QStringLiteral("Archive")).size(), 1);
        QTRY_COMPARE(model->rowCount(list), 1);
        QVERIFY2(errors.isEmpty(), qPrintable(errors.join(QLatin1Char('\n'))));
        delete model;
    }
};

QTEST_GUILESS_MAIN(LocalImapServerTest)
#include "test_LocalImapServer.moc"
