/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "Jmap/FakeJmapServer.h"
#include "Jmap/SyncJob.h"
#include "LocalMail/Store.h"

using LocalMail::Store;

namespace {

const QByteArray first = "From: bob@example.com\r\nSubject: first\r\n\r\nHello\r\n";
const QByteArray second = "From: carol@example.com\r\nSubject: second\r\n\r\nAgenda\r\n";
const QByteArray third = "From: dave@example.com\r\nSubject: third\r\n\r\nLater\r\n";

QString origin(const QString &id)
{
    return QStringLiteral("jmap:") + id;
}

QStringList originsIn(const Store &store, const QString &folder)
{
    QStringList origins;
    for (const LocalMail::MessageInfo &message : store.messages(folder))
        origins << message.origin;
    origins.sort();
    return origins;
}

LocalMail::MessageInfo messageFrom(const Store &store, const QString &folder, const QString &id)
{
    for (const LocalMail::MessageInfo &message : store.messages(folder)) {
        if (message.origin == origin(id))
            return message;
    }
    return {};
}

}

class TestJmapSync : public QObject
{
    Q_OBJECT

private:
    Jmap::Settings settingsFor(const FakeJmapServer &server, const QString &password = QStringLiteral("secret"))
    {
        Jmap::Settings settings;
        settings.url = server.address();
        settings.user = QStringLiteral("alice");
        settings.password = password;
        return settings;
    }

    bool sync(const FakeJmapServer &server, const std::shared_ptr<Store> &store, Jmap::SyncJob **finished = nullptr)
    {
        auto *job = new Jmap::SyncJob(settingsFor(server), store, this);
        QSignalSpy succeeded(job, &Jmap::SyncJob::succeeded);
        QSignalSpy failed(job, &Jmap::SyncJob::failed);
        job->start();
        const bool ok = succeeded.wait(10000) && failed.isEmpty();
        if (!ok && !failed.isEmpty())
            qWarning() << "Synchronization failed:" << failed.constFirst().at(1).toString();
        if (!job->warnings().isEmpty())
            qWarning() << "Warnings:" << job->warnings();
        if (finished)
            *finished = job;
        else
            job->deleteLater();
        return ok;
    }

    /** A server with Inbox, Sent Items, Drafts and Projects/Q4, and three messages */
    void populate(FakeJmapServer &server)
    {
        m_inbox = server.addMailbox(QStringLiteral("Inbox"), QStringLiteral("inbox"));
        m_sent = server.addMailbox(QStringLiteral("Sent Items"), QStringLiteral("sent"));
        m_drafts = server.addMailbox(QStringLiteral("Drafts"), QStringLiteral("drafts"));
        m_projects = server.addMailbox(QStringLiteral("Projects"));
        m_q4 = server.addMailbox(QStringLiteral("Q4"), QString(), m_projects);
        m_first = server.addEmail(first, {m_inbox}, {QStringLiteral("$seen")});
        m_second = server.addEmail(second, {m_inbox, m_q4}, {QStringLiteral("$flagged"), QStringLiteral("$label1")});
        m_third = server.addEmail(third, {m_sent}, {QStringLiteral("$seen")});
    }

    QString m_inbox, m_sent, m_drafts, m_projects, m_q4, m_first, m_second, m_third;

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void findsTheSessionUrl()
    {
        QCOMPARE(Jmap::Settings::sessionUrlFor(QStringLiteral("mail.example.com")),
                 QUrl(QStringLiteral("https://mail.example.com/.well-known/jmap")));
        QCOMPARE(Jmap::Settings::sessionUrlFor(QStringLiteral("https://api.example.com/jmap/session")),
                 QUrl(QStringLiteral("https://api.example.com/jmap/session")));
        QVERIFY(!Jmap::Settings::sessionUrlFor(QString()).isValid());
        QVERIFY(Jmap::Settings::isAcceptable(QUrl(QStringLiteral("http://127.0.0.1:8080/"))));
        QVERIFY(Jmap::Settings::isAcceptable(QUrl(QStringLiteral("http://localhost/"))));
        QVERIFY(!Jmap::Settings::isAcceptable(QUrl(QStringLiteral("http://mail.example.com/"))));
    }

    void mapsKeywordsToFlags()
    {
        const QStringList flags = Jmap::SyncJob::keywordsToFlags({QStringLiteral("$seen"), QStringLiteral("$forwarded"),
                                                                   QStringLiteral("$label1")});
        QCOMPARE(flags, (QStringList{QStringLiteral("\\Seen"), QStringLiteral("$Forwarded"), QStringLiteral("$label1")}));
        QCOMPARE(Jmap::SyncJob::flagsToKeywords({QStringLiteral("\\Seen"), QStringLiteral("\\Deleted"), QStringLiteral("\\Recent"),
                                                 QStringLiteral("$Label1"), QStringLiteral("\\Answered")}),
                 (QStringList{QStringLiteral("$answered"), QStringLiteral("$label1"), QStringLiteral("$seen")}));
    }

    void firstVisitDownloadsFoldersAndMessages()
    {
        FakeJmapServer server;
        populate(server);
        QTemporaryDir dir;
        auto store = Store::open(dir.path());
        Jmap::SyncJob *job = nullptr;
        QVERIFY(sync(server, store, &job));
        QCOMPARE(job->downloaded(), 3);
        QVERIFY(job->canSubmit());

        // Special mailboxes keep the usual names; the local Trash, which the server lacks, stays
        QCOMPARE(store->mailboxNames(), (QStringList{QStringLiteral("INBOX"), QStringLiteral("Drafts"), QStringLiteral("Projects"),
                                                      QStringLiteral("Projects/Q4"), QStringLiteral("Sent"), QStringLiteral("Trash")}));
        QCOMPARE(originsIn(*store, QStringLiteral("INBOX")), (QStringList{origin(m_first), origin(m_second)}));
        QCOMPARE(originsIn(*store, QStringLiteral("Projects/Q4")), QStringList{origin(m_second)});
        QCOMPARE(originsIn(*store, QStringLiteral("Sent")), QStringList{origin(m_third)});
        QCOMPARE(messageFrom(*store, QStringLiteral("INBOX"), m_first).flags, QStringList{QStringLiteral("\\Seen")});
        QCOMPARE(messageFrom(*store, QStringLiteral("Projects/Q4"), m_second).flags,
                 (QStringList{QStringLiteral("\\Flagged"), QStringLiteral("$label1")}));
        const LocalMail::MessageInfo copy = messageFrom(*store, QStringLiteral("INBOX"), m_second);
        QCOMPARE(store->content(QStringLiteral("INBOX"), copy.uid), second);
        QCOMPARE(copy.internalDate, QDateTime(QDate(2026, 11, 20), QTime(8, 30), QTimeZone::UTC));

        // A full list the first time, read in pages and chunks
        QVERIFY(server.methods.contains(QStringLiteral("Email/query")));
        QVERIFY(!server.methods.contains(QStringLiteral("Email/changes")));
        const QFile state(dir.path() + QLatin1Char('/') + Jmap::SyncJob::stateFileName());
        QVERIFY(state.exists());
        QCOMPARE(state.permissions() & (QFile::ReadGroup | QFile::ReadOther), QFile::Permissions());
        job->deleteLater();
    }

    void sendsLocalChangesToTheServer()
    {
        FakeJmapServer server;
        populate(server);
        QTemporaryDir dir;
        auto store = Store::open(dir.path());
        QVERIFY(sync(server, store));
        server.methods.clear();

        // Read state, a move, a copy, a removal and a draft written on this computer
        const uint firstUid = messageFrom(*store, QStringLiteral("INBOX"), m_first).uid;
        QVERIFY(store->setFlags(QStringLiteral("INBOX"), firstUid, {QStringLiteral("\\Seen"), QStringLiteral("\\Flagged")}));
        QVERIFY(!store->copy(QStringLiteral("INBOX"), {firstUid}, QStringLiteral("Projects")).isEmpty());
        QVERIFY(store->remove(QStringLiteral("INBOX"), {messageFrom(*store, QStringLiteral("INBOX"), m_second).uid}));
        QVERIFY(store->remove(QStringLiteral("Sent"), {messageFrom(*store, QStringLiteral("Sent"), m_third).uid}));
        const QByteArray draft = "From: alice@example.com\r\nSubject: draft\r\n\r\nUnfinished\r\n";
        QVERIFY(store->append(QStringLiteral("Drafts"), draft, {QStringLiteral("\\Draft"), QStringLiteral("\\Seen")}));

        Jmap::SyncJob *job = nullptr;
        QVERIFY(sync(server, store, &job));
        QCOMPARE(job->uploaded(), 1);
        job->deleteLater();
        QCOMPARE(server.emails.value(m_first).keywords, (QSet<QString>{QStringLiteral("$seen"), QStringLiteral("$flagged")}));
        QCOMPARE(server.emails.value(m_first).mailboxIds, (QSet<QString>{m_inbox, m_projects}));
        QCOMPARE(server.emails.value(m_second).mailboxIds, QSet<QString>{m_q4});
        QVERIFY(!server.emails.contains(m_third));

        QString draftId;
        for (auto it = server.emails.cbegin(); it != server.emails.cend(); ++it) {
            if (it->mailboxIds.contains(m_drafts))
                draftId = it.key();
        }
        QVERIFY(!draftId.isEmpty());
        QCOMPARE(server.blobs.value(server.emails.value(draftId).blobId), draft);
        QCOMPARE(server.emails.value(draftId).keywords, (QSet<QString>{QStringLiteral("$draft"), QStringLiteral("$seen")}));
        // The local draft is now the server's, and is not uploaded again
        QCOMPARE(originsIn(*store, QStringLiteral("Drafts")), QStringList{origin(draftId)});
        server.methods.clear();
        QVERIFY(sync(server, store));
        QVERIFY(!server.methods.contains(QStringLiteral("Email/import")));
        QVERIFY(!server.methods.contains(QStringLiteral("Email/set")));
        QCOMPARE(originsIn(*store, QStringLiteral("Projects")), QStringList{origin(m_first)});
    }

    void receivesServerChanges()
    {
        FakeJmapServer server;
        populate(server);
        QTemporaryDir dir;
        auto store = Store::open(dir.path());
        QVERIFY(sync(server, store));
        const uint q4Validity = store->mailbox(QStringLiteral("Projects/Q4")).uidValidity;
        server.methods.clear();

        server.changeEmail(m_first, [](FakeJmapServer::Email &email) { email.keywords.clear(); });
        server.destroyEmail(m_second);
        const QString fourth = server.addEmail("Subject: fourth\r\n\r\nNew\r\n", {m_inbox});
        server.mailboxes[m_projects].name = QStringLiteral("Work");
        server.changeEmail(m_third, [this](FakeJmapServer::Email &email) { email.mailboxIds = {m_sent, m_q4}; });

        Jmap::SyncJob *job = nullptr;
        QVERIFY(sync(server, store, &job));
        QCOMPARE(job->downloaded(), 1);
        job->deleteLater();
        QVERIFY(server.methods.contains(QStringLiteral("Email/changes")));
        QVERIFY(!server.methods.contains(QStringLiteral("Email/query")));
        QVERIFY(!store->hasMailbox(QStringLiteral("Projects")));
        QCOMPARE(store->mailbox(QStringLiteral("Work/Q4")).uidValidity, q4Validity);
        QCOMPARE(originsIn(*store, QStringLiteral("INBOX")), (QStringList{origin(fourth), origin(m_first)}));
        QVERIFY(messageFrom(*store, QStringLiteral("INBOX"), m_first).flags.isEmpty());
        QCOMPARE(originsIn(*store, QStringLiteral("Work/Q4")), QStringList{origin(m_third)});
        QCOMPARE(originsIn(*store, QStringLiteral("Sent")), QStringList{origin(m_third)});
    }

    void comparesEverythingWhenChangesAreUnknown()
    {
        FakeJmapServer server;
        populate(server);
        QTemporaryDir dir;
        auto store = Store::open(dir.path());
        QVERIFY(sync(server, store));
        server.methods.clear();

        server.destroyEmail(m_first);
        const QString fourth = server.addEmail("Subject: fourth\r\n\r\nNew\r\n", {m_inbox}, {QStringLiteral("$seen")});
        server.changesFloor = server.state() + 1;

        QVERIFY(sync(server, store));
        QVERIFY(server.methods.contains(QStringLiteral("Email/changes")));
        QVERIFY(server.methods.contains(QStringLiteral("Email/query")));
        QCOMPARE(originsIn(*store, QStringLiteral("INBOX")), (QStringList{origin(fourth), origin(m_second)}));
        server.changesFloor = 0;
        server.methods.clear();
        QVERIFY(sync(server, store));
        QVERIFY(!server.methods.contains(QStringLiteral("Email/query")));
    }

    void managesFoldersOnTheServer()
    {
        FakeJmapServer server;
        populate(server);
        QTemporaryDir dir;
        auto store = Store::open(dir.path());
        QVERIFY(sync(server, store));

        QVERIFY(store->createMailbox(QStringLiteral("Receipts")));
        QVERIFY(store->createMailbox(QStringLiteral("Receipts/2026")));
        QVERIFY(store->renameMailbox(QStringLiteral("Projects"), QStringLiteral("Clients")));
        QVERIFY(!store->copy(QStringLiteral("INBOX"), {messageFrom(*store, QStringLiteral("INBOX"), m_first).uid},
                             QStringLiteral("Receipts/2026")).isEmpty());
        QVERIFY(sync(server, store));

        const QString receipts = server.mailboxByName(QStringLiteral("Receipts"));
        const QString year = server.mailboxByName(QStringLiteral("2026"));
        QVERIFY(!receipts.isEmpty());
        QCOMPARE(server.mailboxes.value(year).parentId, receipts);
        QCOMPARE(server.mailboxes.value(m_projects).name, QStringLiteral("Clients"));
        QCOMPARE(server.emails.value(m_first).mailboxIds, (QSet<QString>{m_inbox, year}));
        QVERIFY(store->hasMailbox(QStringLiteral("Clients/Q4")));

        // Removing a folder removes the server's mailbox, and the messages only there
        QVERIFY(store->deleteMailbox(QStringLiteral("Clients/Q4")));
        QVERIFY(sync(server, store));
        QVERIFY(!server.mailboxes.contains(m_q4));
        QCOMPARE(server.emails.value(m_second).mailboxIds, QSet<QString>{m_inbox});
        QVERIFY(!store->hasMailbox(QStringLiteral("Clients/Q4")));
    }

    void movingIntoALocalOnlyFolderRemovesFromTheServer()
    {
        FakeJmapServer server;
        populate(server);
        QTemporaryDir dir;
        auto store = Store::open(dir.path());
        QVERIFY(sync(server, store));

        const uint uid = messageFrom(*store, QStringLiteral("INBOX"), m_first).uid;
        QVERIFY(!store->copy(QStringLiteral("INBOX"), {uid}, QStringLiteral("Trash")).isEmpty());
        QVERIFY(store->remove(QStringLiteral("INBOX"), {uid}));
        QVERIFY(sync(server, store));
        QVERIFY(!server.emails.contains(m_first));
        QCOMPARE(originsIn(*store, QStringLiteral("Trash")), QStringList{origin(m_first)});
        QVERIFY(server.mailboxByName(QStringLiteral("Trash")).isEmpty());

        // Moved back, it is a new message for the server
        const uint trashed = store->messages(QStringLiteral("Trash")).constFirst().uid;
        QVERIFY(!store->copy(QStringLiteral("Trash"), {trashed}, QStringLiteral("INBOX")).isEmpty());
        QVERIFY(sync(server, store));
        int inInbox = 0;
        for (const FakeJmapServer::Email &email : std::as_const(server.emails))
            inInbox += email.mailboxIds.contains(m_inbox) ? 1 : 0;
        QCOMPARE(inInbox, 2);
    }

    void reportsWhatWentWrong()
    {
        FakeJmapServer server;
        populate(server);
        QTemporaryDir dir;
        auto store = Store::open(dir.path());

        auto *job = new Jmap::SyncJob(settingsFor(server, QStringLiteral("wrong")), store, this);
        QSignalSpy failed(job, &Jmap::SyncJob::failed);
        job->start();
        QVERIFY(failed.wait(10000));
        QCOMPARE(failed.constFirst().at(0).value<Jmap::Client::Failure>(), Jmap::Client::Failure::Authentication);
        QVERIFY(!job->isRunning());
        delete job;

        Jmap::Settings insecure = settingsFor(server);
        insecure.url = QStringLiteral("http://mail.example.com/jmap/session");
        job = new Jmap::SyncJob(insecure, store, this);
        QSignalSpy refused(job, &Jmap::SyncJob::failed);
        job->start();
        QVERIFY(refused.count() == 1 || refused.wait(5000));
        QCOMPARE(refused.constFirst().at(0).value<Jmap::Client::Failure>(), Jmap::Client::Failure::Security);
        QCOMPARE(server.requests, 1);
        delete job;

        server.offersMail = false;
        job = new Jmap::SyncJob(settingsFor(server), store, this);
        QSignalSpy unsupported(job, &Jmap::SyncJob::failed);
        job->start();
        QVERIFY(unsupported.wait(10000));
        QCOMPARE(unsupported.constFirst().at(0).value<Jmap::Client::Failure>(), Jmap::Client::Failure::Unsupported);
        delete job;
        // Nothing was changed on this computer
        QVERIFY(store->messages(QStringLiteral("INBOX")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestJmapSync)
#include "test_JmapSync.moc"
