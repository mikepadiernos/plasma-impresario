/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QSignalSpy>
#include <QTest>

#include "Jmap/FakeJmapServer.h"
#include "Jmap/Submission.h"

namespace {

const QByteArray message = "From: Alice <alice@example.com>\r\nTo: bob@example.org\r\nSubject: hello\r\n\r\nHi Bob\r\n";

}

class TestJmapSubmission : public QObject
{
    Q_OBJECT

private:
    Jmap::Settings settingsFor(const FakeJmapServer &server, const QString &password = QStringLiteral("secret"))
    {
        Jmap::Settings settings;
        settings.url = server.address();
        settings.user = QStringLiteral("alice");
        settings.password = password;
        settings.sending = Jmap::Sending::Jmap;
        return settings;
    }

    /** Sends @ref message from @arg from and returns the error, or an empty string once it was sent */
    QString send(FakeJmapServer &server, const QByteArray &from = "Alice <alice@example.com>",
                 QString *emailId = nullptr)
    {
        Jmap::SubmissionFactory factory(settingsFor(server));
        std::unique_ptr<MSA::AbstractMSA> msa(factory.create(this));
        QSignalSpy sent(msa.get(), &MSA::AbstractMSA::sent);
        QSignalSpy error(msa.get(), &MSA::AbstractMSA::error);
        msa->sendMail(from, {QByteArrayLiteral("bob@example.org"), QByteArrayLiteral("Carol <carol@example.net>")},
                      message);
        if (!QTest::qWaitFor([&]() { return !sent.isEmpty() || !error.isEmpty(); }, 10000))
            return QStringLiteral("timed out");
        if (emailId)
            *emailId = static_cast<Jmap::Submission *>(msa.get())->emailId();
        return error.isEmpty() ? QString() : error.constFirst().constFirst().toString();
    }

    void addMailboxes(FakeJmapServer &server)
    {
        server.addMailbox(QStringLiteral("Inbox"), QStringLiteral("inbox"));
        server.addMailbox(QStringLiteral("Sent Items"), QStringLiteral("sent"));
        server.addMailbox(QStringLiteral("Drafts"), QStringLiteral("drafts"));
    }

private slots:
    void sendsAndKeepsTheMessageInSent()
    {
        FakeJmapServer server;
        addMailboxes(server);
        QString emailId;
        QCOMPARE(send(server, "Alice <alice@example.com>", &emailId), QString());
        QVERIFY(!emailId.isEmpty());
        QVERIFY(server.emails.contains(emailId));
        const FakeJmapServer::Email email = server.emails.value(emailId);
        QCOMPARE(email.mailboxIds, QSet<QString>{server.mailboxByRole(QStringLiteral("sent"))});
        QVERIFY(email.keywords.contains(QStringLiteral("$seen")));
        QCOMPARE(server.blobs.value(email.blobId), message);

        QCOMPARE(server.submissions.size(), 1);
        const QJsonObject submission = server.submissions.constFirst();
        QCOMPARE(submission.value(QLatin1String("identityId")).toString(), QStringLiteral("I1"));
        QCOMPARE(submission.value(QLatin1String("emailId")).toString(), emailId);
        const QJsonObject envelope = submission.value(QLatin1String("envelope")).toObject();
        QCOMPARE(envelope.value(QLatin1String("mailFrom")).toObject().value(QLatin1String("email")).toString(),
                 QStringLiteral("alice@example.com"));
        QStringList recipients;
        for (const QJsonValue &value : envelope.value(QLatin1String("rcptTo")).toArray())
            recipients << value.toObject().value(QLatin1String("email")).toString();
        QCOMPARE(recipients, (QStringList{QStringLiteral("bob@example.org"), QStringLiteral("carol@example.net")}));
    }

    void keepsTheMessageInDraftsWithoutSent()
    {
        FakeJmapServer server;
        server.addMailbox(QStringLiteral("Inbox"), QStringLiteral("inbox"));
        server.addMailbox(QStringLiteral("Drafts"), QStringLiteral("drafts"));
        QString emailId;
        QCOMPARE(send(server, "alice@example.com", &emailId), QString());
        QCOMPARE(server.emails.value(emailId).mailboxIds, QSet<QString>{server.mailboxByRole(QStringLiteral("drafts"))});
    }

    void refusedMessagesLeaveNoCopy()
    {
        FakeJmapServer server;
        addMailboxes(server);
        server.refuseSubmission = true;
        const QString error = send(server);
        QVERIFY2(error.contains(QLatin1String("Sending is off")), qPrintable(error));
        QVERIFY(server.submissions.isEmpty());
        QVERIFY(server.emails.isEmpty());
        QVERIFY(server.methods.contains(QStringLiteral("Email/set")));
    }

    void serversThatDoNotSendPointToSmtp()
    {
        FakeJmapServer server;
        addMailboxes(server);
        server.canSubmit = false;
        const QString error = send(server);
        QVERIFY2(error.contains(QLatin1String("SMTP")), qPrintable(error));
        QVERIFY(server.emails.isEmpty());
        QVERIFY(!server.methods.contains(QStringLiteral("Email/import")));
    }

    void sendersNeedAnIdentity()
    {
        FakeJmapServer server;
        addMailboxes(server);
        const QString error = send(server, "Mallory <mallory@example.com>");
        QVERIFY2(error.contains(QLatin1String("mallory@example.com")), qPrintable(error));
        QVERIFY(server.emails.isEmpty());
        QVERIFY(server.submissions.isEmpty());

        server.identities.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("I2")},
                                             {QStringLiteral("email"), QStringLiteral("*@example.com")}});
        QCOMPARE(send(server, "Mallory <mallory@example.com>"), QString());
        QCOMPARE(server.submissions.size(), 1);
        QCOMPARE(server.submissions.constFirst().value(QLatin1String("identityId")).toString(), QStringLiteral("I2"));
    }

    void asksForTheMissingPassword()
    {
        FakeJmapServer server;
        addMailboxes(server);
        Jmap::Submission submission(settingsFor(server, QString()), {}, this);
        QSignalSpy password(&submission, &MSA::AbstractMSA::passwordRequested);
        QSignalSpy sent(&submission, &MSA::AbstractMSA::sent);
        QSignalSpy error(&submission, &MSA::AbstractMSA::error);
        submission.sendMail("alice@example.com", {QByteArrayLiteral("bob@example.org")}, message);
        QCOMPARE(password.size(), 1);
        QCOMPARE(password.constFirst().at(0).toString(), QStringLiteral("alice"));
        QCOMPARE(password.constFirst().at(1).toString(), QStringLiteral("127.0.0.1"));
        QCOMPARE(server.requests, 0);

        submission.setPassword(QStringLiteral("secret"));
        QVERIFY(sent.wait(10000));
        QVERIFY(error.isEmpty());
        QCOMPARE(server.submissions.size(), 1);
    }

    void wrongPasswordsAreReported()
    {
        FakeJmapServer server;
        addMailboxes(server);
        server.password = QStringLiteral("other");
        const QString error = send(server);
        QVERIFY(!error.isEmpty());
        QVERIFY(error != QLatin1String("timed out"));
        QVERIFY(server.emails.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestJmapSubmission)
#include "test_JmapSubmission.moc"
