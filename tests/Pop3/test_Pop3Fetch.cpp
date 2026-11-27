/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QPointer>
#include <QSet>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

#include "LocalMail/Store.h"
#include "Pop3/FetchJob.h"

/** @short A POP3 server on localhost whose behaviour each test chooses */
class FakePop3Server : public QObject
{
    Q_OBJECT
public:
    struct Message {
        QByteArray uidl;
        QByteArray content;
    };

    bool capa = true;
    bool uidl = true;
    bool top = true;
    bool saslPlain = false;
    bool stls = false;
    bool greet = true;
    QByteArray authError = "-ERR invalid credentials";
    QByteArray user = "alice";
    QByteArray password = "secret";
    /** Close the connection while sending this message number; 0 never */
    int dropDuringRetr = 0;
    QList<Message> messages;
    QList<QByteArray> commands;

    FakePop3Server()
    {
        connect(&m_server, &QTcpServer::newConnection, this, &FakePop3Server::onNewConnection);
        m_server.listen(QHostAddress::LocalHost);
    }

    quint16 port() const
    {
        return m_server.serverPort();
    }

    QList<QByteArray> uidls() const
    {
        QList<QByteArray> result;
        for (const Message &message : messages)
            result << message.uidl;
        return result;
    }

    bool received(const QByteArray &prefix) const
    {
        for (const QByteArray &command : commands) {
            if (command.startsWith(prefix))
                return true;
        }
        return false;
    }

private:
    void onNewConnection()
    {
        m_socket = m_server.nextPendingConnection();
        m_deleted.clear();
        m_authenticated = false;
        connect(m_socket, &QTcpSocket::readyRead, this, &FakePop3Server::onReadyRead);
        if (greet)
            m_socket->write("+OK fake POP3 ready\r\n");
    }

    void writeMultiLine(const QByteArray &status, const QByteArray &body)
    {
        QByteArray out = status + "\r\n";
        const QList<QByteArray> lines = body.split('\n');
        for (int i = 0; i < lines.size(); ++i) {
            QByteArray line = lines.at(i);
            if (i == lines.size() - 1 && line.isEmpty())
                break;
            if (line.endsWith('\r'))
                line.chop(1);
            if (line.startsWith('.'))
                line.prepend('.');
            out += line + "\r\n";
        }
        out += ".\r\n";
        m_socket->write(out);
    }

    void onReadyRead()
    {
        m_buffer += m_socket->readAll();
        int end;
        while (m_socket && (end = m_buffer.indexOf("\r\n")) >= 0) {
            const QByteArray line = m_buffer.left(end);
            m_buffer.remove(0, end + 2);
            commands << line;
            handle(line);
        }
    }

    void handle(const QByteArray &line)
    {
        const QList<QByteArray> parts = line.split(' ');
        const QByteArray command = parts.value(0).toUpper();
        const int number = parts.value(1).toInt();
        const bool valid = number >= 1 && number <= messages.size() && !m_deleted.contains(number);

        if (command == "CAPA") {
            if (!capa) {
                m_socket->write("-ERR unknown command\r\n");
                return;
            }
            QByteArray body = "USER\r\n";
            if (uidl)
                body += "UIDL\r\n";
            if (top)
                body += "TOP\r\n";
            if (saslPlain)
                body += "SASL PLAIN LOGIN\r\n";
            if (stls)
                body += "STLS\r\n";
            writeMultiLine("+OK capabilities", body);
        } else if (command == "USER") {
            m_user = parts.value(1);
            m_socket->write("+OK\r\n");
        } else if (command == "PASS") {
            if (m_user == user && line.mid(5) == password) {
                m_authenticated = true;
                m_socket->write("+OK signed in\r\n");
            } else {
                m_socket->write(authError + "\r\n");
            }
        } else if (command == "AUTH") {
            const QByteArray expected = QByteArray(QByteArray(1, '\0') + user + QByteArray(1, '\0') + password).toBase64();
            if (parts.value(1) == "PLAIN" && parts.value(2) == expected) {
                m_authenticated = true;
                m_socket->write("+OK signed in\r\n");
            } else {
                m_socket->write(authError + "\r\n");
            }
        } else if (!m_authenticated && command != "QUIT") {
            m_socket->write("-ERR sign in first\r\n");
        } else if (command == "UIDL" && uidl) {
            QByteArray body;
            for (int i = 0; i < messages.size(); ++i) {
                if (!m_deleted.contains(i + 1))
                    body += QByteArray::number(i + 1) + ' ' + messages.at(i).uidl + "\r\n";
            }
            writeMultiLine("+OK", body);
        } else if (command == "LIST") {
            QByteArray body;
            for (int i = 0; i < messages.size(); ++i) {
                if (!m_deleted.contains(i + 1))
                    body += QByteArray::number(i + 1) + ' ' + QByteArray::number(messages.at(i).content.size()) + "\r\n";
            }
            writeMultiLine("+OK", body);
        } else if (command == "TOP" && top && valid) {
            const QByteArray &content = messages.at(number - 1).content;
            const int split = content.indexOf("\r\n\r\n");
            writeMultiLine("+OK", split < 0 ? content : content.left(split + 4));
        } else if (command == "RETR" && valid) {
            if (number == dropDuringRetr) {
                m_socket->write("+OK\r\nSubject: half\r\n");
                m_socket->flush();
                m_socket->disconnectFromHost();
                m_socket = nullptr;
                return;
            }
            writeMultiLine("+OK", messages.at(number - 1).content);
        } else if (command == "DELE" && valid) {
            m_deleted.insert(number);
            m_socket->write("+OK deleted\r\n");
        } else if (command == "QUIT") {
            // Deletions take effect only at a proper end of the session
            QList<Message> kept;
            for (int i = 0; i < messages.size(); ++i) {
                if (!m_deleted.contains(i + 1))
                    kept << messages.at(i);
            }
            messages = kept;
            m_socket->write("+OK bye\r\n");
            m_socket->disconnectFromHost();
            m_socket = nullptr;
        } else {
            m_socket->write("-ERR no\r\n");
        }
    }

    QTcpServer m_server;
    QPointer<QTcpSocket> m_socket;
    QByteArray m_buffer;
    QByteArray m_user;
    bool m_authenticated = false;
    QSet<int> m_deleted;
};

class Pop3FetchTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void downloadsAndUnstuffs();
    void signsInWithAuthPlain();
    void downloadsOnlyNewMail();
    void identifiesMailWithoutUidl();
    void failsWithoutUidlOrTop();
    void reportsWrongPassword();
    void reportsBusyMailbox();
    void keepsMailWhenTheConnectionDrops();
    void removesMailAfterDownload();
    void keepsRecentMailForDays();
    void refusesToSignInWithoutStls();
    void timesOut();

private:
    struct Result {
        bool ok = false;
        Pop3::FetchJob::Failure failure = Pop3::FetchJob::Failure::Protocol;
        QString message;
        int downloaded = 0;
    };

    Pop3::Settings settingsFor(const FakePop3Server &server) const;
    Result run(const Pop3::Settings &settings, int timeout = 60);
    static QByteArray message(const QByteArray &subject, const QByteArray &body);

    std::unique_ptr<QTemporaryDir> m_dir;
    std::shared_ptr<LocalMail::Store> m_store;
};

void Pop3FetchTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void Pop3FetchTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    QString error;
    m_store = LocalMail::Store::open(m_dir->filePath(QStringLiteral("store")), &error);
    QVERIFY2(m_store, qPrintable(error));
}

void Pop3FetchTest::cleanup()
{
    m_store.reset();
    m_dir.reset();
}

QByteArray Pop3FetchTest::message(const QByteArray &subject, const QByteArray &body)
{
    return "From: Bob <bob@example.org>\r\nTo: alice@example.org\r\nSubject: " + subject
        + "\r\nMessage-ID: <" + subject + "@example.org>\r\n\r\n" + body;
}

Pop3::Settings Pop3FetchTest::settingsFor(const FakePop3Server &server) const
{
    Pop3::Settings settings;
    settings.host = QStringLiteral("127.0.0.1");
    settings.port = server.port();
    settings.security = Pop3::Security::None;
    settings.user = QStringLiteral("alice");
    settings.password = QStringLiteral("secret");
    return settings;
}

Pop3FetchTest::Result Pop3FetchTest::run(const Pop3::Settings &settings, int timeout)
{
    Result result;
    Pop3::FetchJob job(settings, m_store);
    job.setTimeout(timeout);
    bool done = false;
    connect(&job, &Pop3::FetchJob::succeeded, this, [&](int downloaded) {
        result.ok = true;
        result.downloaded = downloaded;
        done = true;
    });
    connect(&job, &Pop3::FetchJob::failed, this, [&](Pop3::FetchJob::Failure failure, const QString &message) {
        result.failure = failure;
        result.message = message;
        result.downloaded = job.downloaded();
        done = true;
    });
    job.start();
    [&]() { QTRY_VERIFY_WITH_TIMEOUT(done, 10000); }();
    // Let the fake server see the end of the connection
    QTest::qWait(50);
    return result;
}

void Pop3FetchTest::downloadsAndUnstuffs()
{
    FakePop3Server server;
    server.messages << FakePop3Server::Message{"u1", message("one", "Hello\r\n.\r\n..two dots\r\n.leading\r\n")}
                    << FakePop3Server::Message{"u2", message("two", "Bye\r\n")};
    const Result result = run(settingsFor(server));
    QVERIFY2(result.ok, qPrintable(result.message));
    QCOMPARE(result.downloaded, 2);
    QVERIFY(server.received("USER alice"));
    QVERIFY(!server.received("DELE"));

    const QList<LocalMail::MessageInfo> stored = m_store->messages(QStringLiteral("INBOX"));
    QCOMPARE(stored.size(), 2);
    QCOMPARE(m_store->content(QStringLiteral("INBOX"), stored.at(0).uid),
             message("one", "Hello\r\n.\r\n..two dots\r\n.leading\r\n"));
    QCOMPARE(stored.at(0).origin, QStringLiteral("uidl:u1"));
    QCOMPARE(stored.at(1).origin, QStringLiteral("uidl:u2"));
    QCOMPARE(server.messages.size(), 2);
}

void Pop3FetchTest::signsInWithAuthPlain()
{
    FakePop3Server server;
    server.saslPlain = true;
    server.messages << FakePop3Server::Message{"u1", message("one", "x\r\n")};
    const Result result = run(settingsFor(server));
    QVERIFY2(result.ok, qPrintable(result.message));
    QVERIFY(server.received("AUTH PLAIN "));
    QVERIFY(!server.received("USER"));
    QVERIFY(!server.received("PASS"));
}

void Pop3FetchTest::downloadsOnlyNewMail()
{
    FakePop3Server server;
    server.messages << FakePop3Server::Message{"u1", message("one", "x\r\n")};
    QCOMPARE(run(settingsFor(server)).downloaded, 1);

    server.messages << FakePop3Server::Message{"u2", message("two", "y\r\n")};
    server.commands.clear();
    const Result result = run(settingsFor(server));
    QVERIFY2(result.ok, qPrintable(result.message));
    QCOMPARE(result.downloaded, 1);
    QVERIFY(server.received("RETR 2"));
    QVERIFY(!server.received("RETR 1"));
    QCOMPARE(m_store->messages(QStringLiteral("INBOX")).size(), 2);

    // Mail the user removed here is not downloaded again while it stays on the server
    QVERIFY(m_store->remove(QStringLiteral("INBOX"), {m_store->messages(QStringLiteral("INBOX")).first().uid}));
    QCOMPARE(run(settingsFor(server)).downloaded, 0);
}

void Pop3FetchTest::identifiesMailWithoutUidl()
{
    FakePop3Server server;
    server.uidl = false;
    server.messages << FakePop3Server::Message{"", message("one", "x\r\n")}
                    << FakePop3Server::Message{"", message("one", "x\r\n")};
    Result result = run(settingsFor(server));
    QVERIFY2(result.ok, qPrintable(result.message));
    QCOMPARE(result.downloaded, 2);
    QVERIFY(server.received("TOP 1 0"));
    const QList<LocalMail::MessageInfo> stored = m_store->messages(QStringLiteral("INBOX"));
    QVERIFY(stored.at(0).origin.startsWith(QLatin1String("hdr:")));
    QVERIFY(stored.at(0).origin != stored.at(1).origin);

    result = run(settingsFor(server));
    QVERIFY(result.ok);
    QCOMPARE(result.downloaded, 0);
}

void Pop3FetchTest::failsWithoutUidlOrTop()
{
    FakePop3Server server;
    server.uidl = false;
    server.top = false;
    server.messages << FakePop3Server::Message{"", message("one", "x\r\n")};
    const Result result = run(settingsFor(server));
    QVERIFY(!result.ok);
    QCOMPARE(result.failure, Pop3::FetchJob::Failure::Protocol);
    QVERIFY(!server.received("RETR"));
}

void Pop3FetchTest::reportsWrongPassword()
{
    FakePop3Server server;
    Pop3::Settings settings = settingsFor(server);
    settings.password = QStringLiteral("wrong");
    const Result result = run(settings);
    QVERIFY(!result.ok);
    QCOMPARE(result.failure, Pop3::FetchJob::Failure::Authentication);
    QCOMPARE(result.message, QStringLiteral("invalid credentials"));
}

void Pop3FetchTest::reportsBusyMailbox()
{
    FakePop3Server server;
    server.authError = "-ERR [IN-USE] another session is open";
    Pop3::Settings settings = settingsFor(server);
    settings.password = QStringLiteral("wrong");
    const Result result = run(settings);
    QVERIFY(!result.ok);
    QCOMPARE(result.failure, Pop3::FetchJob::Failure::Network);
}

void Pop3FetchTest::keepsMailWhenTheConnectionDrops()
{
    FakePop3Server server;
    server.messages << FakePop3Server::Message{"u1", message("one", "x\r\n")}
                    << FakePop3Server::Message{"u2", message("two", "y\r\n")};
    server.dropDuringRetr = 2;
    Pop3::Settings settings = settingsFor(server);
    settings.retention = Pop3::Retention::AfterDownload;
    Result result = run(settings);
    QVERIFY(!result.ok);
    QCOMPARE(result.failure, Pop3::FetchJob::Failure::Network);
    QCOMPARE(result.downloaded, 1);
    QCOMPARE(m_store->messages(QStringLiteral("INBOX")).size(), 1);
    QCOMPARE(server.messages.size(), 2);
    QVERIFY(!server.received("DELE"));

    server.dropDuringRetr = 0;
    result = run(settings);
    QVERIFY2(result.ok, qPrintable(result.message));
    QCOMPARE(result.downloaded, 1);
    QCOMPARE(m_store->messages(QStringLiteral("INBOX")).size(), 2);
    QCOMPARE(server.messages.size(), 0);
}

void Pop3FetchTest::removesMailAfterDownload()
{
    FakePop3Server server;
    server.messages << FakePop3Server::Message{"u1", message("one", "x\r\n")}
                    << FakePop3Server::Message{"u2", message("two", "y\r\n")};
    Pop3::Settings settings = settingsFor(server);
    settings.retention = Pop3::Retention::AfterDownload;
    const Result result = run(settings);
    QVERIFY2(result.ok, qPrintable(result.message));
    QCOMPARE(server.messages.size(), 0);
    QCOMPARE(m_store->messages(QStringLiteral("INBOX")).size(), 2);
    // Forgotten once gone from the server, so a message sent again with the same UIDL would be new
    QVERIFY(!m_store->knowsOrigin(QStringLiteral("uidl:u1")));
}

void Pop3FetchTest::keepsRecentMailForDays()
{
    FakePop3Server server;
    server.messages << FakePop3Server::Message{"u1", message("one", "x\r\n")};
    Pop3::Settings settings = settingsFor(server);
    settings.retention = Pop3::Retention::AfterDays;
    settings.retentionDays = 7;
    QVERIFY(run(settings).ok);
    QVERIFY(run(settings).ok);
    QVERIFY(!server.received("DELE"));
    QCOMPARE(server.messages.size(), 1);
    QVERIFY(m_store->knowsOrigin(QStringLiteral("uidl:u1")));
}

void Pop3FetchTest::refusesToSignInWithoutStls()
{
    FakePop3Server server;
    server.saslPlain = true;
    Pop3::Settings settings = settingsFor(server);
    settings.security = Pop3::Security::StartTls;
    const Result result = run(settings);
    QVERIFY(!result.ok);
    QCOMPARE(result.failure, Pop3::FetchJob::Failure::Security);
    QVERIFY(!server.received("USER"));
    QVERIFY(!server.received("PASS"));
    QVERIFY(!server.received("AUTH"));
}

void Pop3FetchTest::timesOut()
{
    FakePop3Server server;
    server.greet = false;
    const Result result = run(settingsFor(server), 1);
    QVERIFY(!result.ok);
    QCOMPARE(result.failure, Pop3::FetchJob::Failure::Network);
}

QTEST_GUILESS_MAIN(Pop3FetchTest)
#include "test_Pop3Fetch.moc"
