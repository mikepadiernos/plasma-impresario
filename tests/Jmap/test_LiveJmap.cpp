/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include "Jmap/Client.h"
#include "Jmap/Submission.h"
#include "Jmap/SyncJob.h"
#include "LocalMail/Store.h"

/**
 * @short JMAP synchronization and sending against a real server
 *
 * Opt-in: run through tools/dev-container/live-jmap.sh, which starts a disposable Stalwart with the synthetic users
 * "work" and "other" and sets PLASMA_TEST_JMAP_URL and PLASMA_TEST_JMAP_SMTP_PORT. Without them every case is
 * skipped. The server keeps its mail between cases, so each case uses subjects of its own.
 */
class LiveJmapTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void downloadsServerMailIntoSpecialFolders();
    void localChangesReachTheServer();
    void serverChangesComeDownIncrementally();
    void sendsThroughSubmission();
    void localRemovalReachesTheServer();
    void reportsAWrongPassword();

private:
    struct Result {
        bool ok = false;
        Jmap::Client::Failure failure = Jmap::Client::Failure::Protocol;
        QString message;
        int downloaded = 0;
        QStringList warnings;
    };

    Jmap::Settings settingsFor(const QString &user, const QString &password = QString()) const;
    Result sync(const Jmap::Settings &settings, const std::shared_ptr<LocalMail::Store> &store);
    QString subject(const char *what) const;
    bool deliver(const QString &to, const QString &subject);
    QList<QJsonObject> serverEmails(const QString &user, const QString &subject);
    QHash<QString, QString> serverMailboxNames(const QString &user);
    QJsonArray call(const QString &user, const QJsonArray &methodCalls);
    static uint findUid(const LocalMail::Store &store, const QString &folder, const QString &subject);

    QString m_url;
    quint16 m_smtpPort = 0;
    QString m_run;
    std::unique_ptr<QTemporaryDir> m_dir;
    std::shared_ptr<LocalMail::Store> m_store;
};

void LiveJmapTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    bool smtpOk = false;
    m_url = qEnvironmentVariable("PLASMA_TEST_JMAP_URL");
    m_smtpPort = qEnvironmentVariableIntValue("PLASMA_TEST_JMAP_SMTP_PORT", &smtpOk);
    if (m_url.isEmpty() || !smtpOk)
        QSKIP("Run through tools/dev-container/live-jmap.sh");
    m_run = QUuid::createUuid().toString(QUuid::Id128).left(8);
}

void LiveJmapTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    QString error;
    m_store = LocalMail::Store::open(m_dir->filePath(QStringLiteral("work")), &error);
    QVERIFY2(m_store, qPrintable(error));
}

void LiveJmapTest::cleanup()
{
    m_store.reset();
    m_dir.reset();
}

Jmap::Settings LiveJmapTest::settingsFor(const QString &user, const QString &password) const
{
    Jmap::Settings settings;
    settings.url = m_url;
    settings.user = user;
    settings.password = password.isEmpty() ? user + QLatin1String("-secret") : password;
    return settings;
}

LiveJmapTest::Result LiveJmapTest::sync(const Jmap::Settings &settings, const std::shared_ptr<LocalMail::Store> &store)
{
    Result result;
    Jmap::SyncJob job(settings, store);
    job.setTimeout(20);
    bool done = false;
    connect(&job, &Jmap::SyncJob::succeeded, this, [&](int downloaded) {
        result.ok = true;
        result.downloaded = downloaded;
        done = true;
    });
    connect(&job, &Jmap::SyncJob::failed, this, [&](Jmap::Client::Failure failure, const QString &message) {
        result.failure = failure;
        result.message = message;
        done = true;
    });
    job.start();
    [&]() { QTRY_VERIFY_WITH_TIMEOUT(done, 60000); }();
    result.warnings = job.warnings();
    return result;
}

QString LiveJmapTest::subject(const char *what) const
{
    return QStringLiteral("live-%1-%2").arg(m_run, QLatin1String(what));
}

bool LiveJmapTest::deliver(const QString &to, const QString &subject)
{
    QTcpSocket socket;
    socket.connectToHost(QStringLiteral("127.0.0.1"), m_smtpPort);
    if (!socket.waitForConnected(10000))
        return false;
    const auto expect = [&socket](const char *code) {
        QByteArray line;
        // Multi-line replies repeat the code with a dash; the last line has a space
        do {
            while (!socket.canReadLine()) {
                if (!socket.waitForReadyRead(10000))
                    return false;
            }
            line = socket.readLine();
        } while (line.size() > 3 && line.at(3) == '-');
        return line.startsWith(code);
    };
    const auto say = [&socket](const QByteArray &line) {
        socket.write(line + "\r\n");
        return socket.waitForBytesWritten(10000);
    };
    const QByteArray message = "From: Carol <carol@remote.test>\r\nTo: " + to.toUtf8() + "\r\nSubject: " + subject.toUtf8()
        + "\r\nMessage-ID: <" + subject.toUtf8() + "@remote.test>\r\nDate: Fri, 27 Nov 2026 10:00:00 +0000\r\n\r\n"
        + "Synthetic message for the live JMAP test.\r\n.";
    const bool ok = expect("220") && say("EHLO remote.test") && expect("250")
        && say("MAIL FROM:<carol@remote.test>") && expect("250") && say("RCPT TO:<" + to.toUtf8() + ">")
        && expect("250") && say("DATA") && expect("354") && say(message) && expect("250");
    say("QUIT");
    if (!ok)
        return false;
    // Delivery is queued; wait until the message can be seen
    const QString user = to.section(QLatin1Char('@'), 0, 0);
    for (int i = 0; i < 50; ++i) {
        if (!serverEmails(user, subject).isEmpty())
            return true;
        QTest::qWait(200);
    }
    return false;
}

QJsonArray LiveJmapTest::call(const QString &user, const QJsonArray &methodCalls)
{
    Jmap::Client client(settingsFor(user));
    QJsonArray responses;
    bool done = false;
    connect(&client, &Jmap::Client::failed, this, [&](Jmap::Client::Failure, const QString &message) {
        qWarning() << "JMAP request failed:" << message;
        done = true;
    });
    client.fetchSession([&]() {
        QJsonArray calls;
        for (const QJsonValue &value : methodCalls) {
            QJsonArray call = value.toArray();
            QJsonObject arguments = call.at(1).toObject();
            arguments[QStringLiteral("accountId")] = client.session().accountId;
            call[1] = arguments;
            calls.append(call);
        }
        client.call(calls, [&](const QJsonArray &answer) {
            responses = answer;
            done = true;
        });
    });
    [&]() { QTRY_VERIFY_WITH_TIMEOUT(done, 30000); }();
    return responses;
}

QList<QJsonObject> LiveJmapTest::serverEmails(const QString &user, const QString &subject)
{
    const QJsonArray responses = call(user, {
        Jmap::Client::method(QStringLiteral("Email/query"),
                             {{QStringLiteral("filter"), QJsonObject{{QStringLiteral("subject"), subject}}}},
                             QStringLiteral("q")),
        Jmap::Client::method(QStringLiteral("Email/get"),
                             {{QStringLiteral("#ids"), QJsonObject{{QStringLiteral("resultOf"), QStringLiteral("q")},
                                                                   {QStringLiteral("name"), QStringLiteral("Email/query")},
                                                                   {QStringLiteral("path"), QStringLiteral("/ids")}}},
                              {QStringLiteral("properties"),
                               QJsonArray{QStringLiteral("subject"), QStringLiteral("keywords"),
                                          QStringLiteral("mailboxIds")}}},
                             QStringLiteral("g")),
    });
    QString error;
    QList<QJsonObject> emails;
    for (const QJsonValue &email : Jmap::Client::response(responses, QStringLiteral("g"), &error)
                                       .value(QStringLiteral("list")).toArray()) {
        // Subject filters match words, so keep exact matches only
        if (email.toObject().value(QStringLiteral("subject")).toString() == subject)
            emails << email.toObject();
    }
    return emails;
}

QHash<QString, QString> LiveJmapTest::serverMailboxNames(const QString &user)
{
    // Not a braced list: one QJsonArray in braces would copy the call instead of wrapping it
    QJsonArray calls;
    calls.append(Jmap::Client::method(QStringLiteral("Mailbox/get"), {}, QStringLiteral("m")));
    const QJsonArray responses = call(user, calls);
    QString error;
    QHash<QString, QString> names;
    for (const QJsonValue &mailbox :
         Jmap::Client::response(responses, QStringLiteral("m"), &error).value(QStringLiteral("list")).toArray())
        names.insert(mailbox[QLatin1String("id")].toString(), mailbox[QLatin1String("name")].toString());
    return names;
}

uint LiveJmapTest::findUid(const LocalMail::Store &store, const QString &folder, const QString &subject)
{
    const QByteArray header = "Subject: " + subject.toUtf8() + "\r\n";
    for (const LocalMail::MessageInfo &message : store.messages(folder)) {
        if (store.content(folder, message.uid).contains(header))
            return message.uid;
    }
    return 0;
}

void LiveJmapTest::downloadsServerMailIntoSpecialFolders()
{
    const QString first = subject("first");
    QVERIFY(deliver(QStringLiteral("work@example.com"), first));

    const Result result = sync(settingsFor(QStringLiteral("work")), m_store);
    QVERIFY2(result.ok, qPrintable(result.message));
    QVERIFY(result.downloaded >= 1);
    QVERIFY2(result.warnings.isEmpty(), qPrintable(result.warnings.join(QLatin1Char('\n'))));
    for (const char *folder : {"INBOX", "Sent", "Drafts", "Trash", "Junk"})
        QVERIFY2(m_store->hasMailbox(QLatin1String(folder)), folder);

    const uint uid = findUid(*m_store, QStringLiteral("INBOX"), first);
    QVERIFY(uid != 0);
    const QList<QJsonObject> onServer = serverEmails(QStringLiteral("work"), first);
    QCOMPARE(onServer.size(), 1);
    bool found = false;
    for (const LocalMail::MessageInfo &message : m_store->messages(QStringLiteral("INBOX"))) {
        if (message.uid == uid) {
            QCOMPARE(message.origin, QLatin1String("jmap:") + onServer.first()[QLatin1String("id")].toString());
            QVERIFY(!message.flags.contains(QLatin1String("\\Seen")));
            found = true;
        }
    }
    QVERIFY(found);

    // A second visit without changes downloads nothing
    const Result again = sync(settingsFor(QStringLiteral("work")), m_store);
    QVERIFY2(again.ok, qPrintable(again.message));
    QCOMPARE(again.downloaded, 0);
}

void LiveJmapTest::localChangesReachTheServer()
{
    const QString marked = subject("marked");
    QVERIFY(deliver(QStringLiteral("work@example.com"), marked));
    const Result first = sync(settingsFor(QStringLiteral("work")), m_store);
    QVERIFY2(first.ok, qPrintable(first.message));

    const uint uid = findUid(*m_store, QStringLiteral("INBOX"), marked);
    QVERIFY(uid != 0);
    QVERIFY(m_store->setFlags(QStringLiteral("INBOX"), uid, {QStringLiteral("\\Seen"), QStringLiteral("\\Flagged")}));
    const QString folder = QStringLiteral("Projects-") + m_run;
    QVERIFY(m_store->createMailbox(folder));
    QCOMPARE(m_store->copy(QStringLiteral("INBOX"), {uid}, folder).size(), 1);

    const Result second = sync(settingsFor(QStringLiteral("work")), m_store);
    QVERIFY2(second.ok, qPrintable(second.message));
    QVERIFY2(second.warnings.isEmpty(), qPrintable(second.warnings.join(QLatin1Char('\n'))));

    const QHash<QString, QString> mailboxes = serverMailboxNames(QStringLiteral("work"));
    QVERIFY(mailboxes.values().contains(folder));
    const QList<QJsonObject> onServer = serverEmails(QStringLiteral("work"), marked);
    QCOMPARE(onServer.size(), 1);
    const QJsonObject keywords = onServer.first()[QLatin1String("keywords")].toObject();
    QVERIFY(keywords.contains(QLatin1String("$seen")));
    QVERIFY(keywords.contains(QLatin1String("$flagged")));
    QStringList in;
    for (const QString &id : onServer.first()[QLatin1String("mailboxIds")].toObject().keys())
        in << mailboxes.value(id);
    QVERIFY2(in.contains(folder), qPrintable(in.join(QLatin1String(", "))));
    QCOMPARE(in.size(), 2);

    // Renaming the folder on this computer renames it on the server
    const QString renamed = folder + QLatin1String("-renamed");
    QVERIFY(m_store->renameMailbox(folder, renamed));
    const Result third = sync(settingsFor(QStringLiteral("work")), m_store);
    QVERIFY2(third.ok, qPrintable(third.message));
    const QHash<QString, QString> after = serverMailboxNames(QStringLiteral("work"));
    QVERIFY(after.values().contains(renamed));
    QVERIFY(!after.values().contains(folder));
}

void LiveJmapTest::serverChangesComeDownIncrementally()
{
    const Result first = sync(settingsFor(QStringLiteral("work")), m_store);
    QVERIFY2(first.ok, qPrintable(first.message));

    const QString later = subject("later");
    QVERIFY(deliver(QStringLiteral("work@example.com"), later));
    const Result second = sync(settingsFor(QStringLiteral("work")), m_store);
    QVERIFY2(second.ok, qPrintable(second.message));
    QCOMPARE(second.downloaded, 1);
    QVERIFY(findUid(*m_store, QStringLiteral("INBOX"), later) != 0);
}

void LiveJmapTest::sendsThroughSubmission()
{
    const QString sent = subject("sent");
    const QByteArray message = "From: Work <work@example.com>\r\nTo: Other <other@example.com>\r\nSubject: "
        + sent.toUtf8() + "\r\nMessage-ID: <" + sent.toUtf8()
        + "@example.com>\r\nDate: Fri, 27 Nov 2026 11:00:00 +0000\r\n\r\nSent through JMAP.\r\n";
    Jmap::Submission submission(settingsFor(QStringLiteral("work")), {}, this);
    QSignalSpy done(&submission, &MSA::AbstractMSA::sent);
    QSignalSpy error(&submission, &MSA::AbstractMSA::error);
    submission.sendMail("work@example.com", {QByteArrayLiteral("other@example.com")}, message);
    QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty() || !error.isEmpty(), 30000);
    QVERIFY2(error.isEmpty(), qPrintable(error.isEmpty() ? QString() : error.first().first().toString()));

    // The recipient gets it, and the sender's Sent folder keeps the copy
    bool arrived = false;
    for (int i = 0; i < 50 && !arrived; ++i) {
        arrived = !serverEmails(QStringLiteral("other"), sent).isEmpty();
        if (!arrived)
            QTest::qWait(200);
    }
    QVERIFY(arrived);
    auto other = LocalMail::Store::open(m_dir->filePath(QStringLiteral("other")));
    QVERIFY(other);
    const Result received = sync(settingsFor(QStringLiteral("other")), other);
    QVERIFY2(received.ok, qPrintable(received.message));
    QVERIFY(findUid(*other, QStringLiteral("INBOX"), sent) != 0);

    const Result own = sync(settingsFor(QStringLiteral("work")), m_store);
    QVERIFY2(own.ok, qPrintable(own.message));
    QVERIFY(findUid(*m_store, QStringLiteral("Sent"), sent) != 0);
}

void LiveJmapTest::localRemovalReachesTheServer()
{
    const QString removed = subject("removed");
    QVERIFY(deliver(QStringLiteral("work@example.com"), removed));
    const Result first = sync(settingsFor(QStringLiteral("work")), m_store);
    QVERIFY2(first.ok, qPrintable(first.message));

    // Moving to Trash moves it on the server; removing it from Trash destroys it
    const uint uid = findUid(*m_store, QStringLiteral("INBOX"), removed);
    QVERIFY(uid != 0);
    QCOMPARE(m_store->copy(QStringLiteral("INBOX"), {uid}, QStringLiteral("Trash")).size(), 1);
    QVERIFY(m_store->remove(QStringLiteral("INBOX"), {uid}));
    const Result second = sync(settingsFor(QStringLiteral("work")), m_store);
    QVERIFY2(second.ok, qPrintable(second.message));
    const QList<QJsonObject> trashed = serverEmails(QStringLiteral("work"), removed);
    QCOMPARE(trashed.size(), 1);
    const QHash<QString, QString> mailboxes = serverMailboxNames(QStringLiteral("work"));
    const QStringList in = [&]() {
        QStringList names;
        for (const QString &id : trashed.first()[QLatin1String("mailboxIds")].toObject().keys())
            names << mailboxes.value(id);
        return names;
    }();
    QCOMPARE(in.size(), 1);

    const uint inTrash = findUid(*m_store, QStringLiteral("Trash"), removed);
    QVERIFY(inTrash != 0);
    QVERIFY(m_store->remove(QStringLiteral("Trash"), {inTrash}));
    const Result third = sync(settingsFor(QStringLiteral("work")), m_store);
    QVERIFY2(third.ok, qPrintable(third.message));
    QVERIFY(serverEmails(QStringLiteral("work"), removed).isEmpty());
}

void LiveJmapTest::reportsAWrongPassword()
{
    const Result result = sync(settingsFor(QStringLiteral("work"), QStringLiteral("not-the-password")), m_store);
    QVERIFY(!result.ok);
    QCOMPARE(result.failure, Jmap::Client::Failure::Authentication);
}

QTEST_GUILESS_MAIN(LiveJmapTest)
#include "test_LiveJmap.moc"
