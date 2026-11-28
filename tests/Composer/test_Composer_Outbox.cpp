/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "test_Composer_Outbox.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QTimeZone>

#include "MSA/FakeMSA.h"

using Composer::Outbox;

namespace {

QByteArray readAll(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QString storedState(const Outbox &outbox, const QString &id)
{
    const QByteArray json = readAll(outbox.folder() + QLatin1Char('/') + id + QLatin1String("/entry.json"));
    return QJsonDocument::fromJson(json).object().value(QLatin1String("state")).toString();
}

} // namespace

void ComposerOutboxTest::init()
{
    QStandardPaths::setTestModeEnabled(true);
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_factory = std::make_unique<MSA::FakeFactory>();
    m_online = true;
    m_filed.clear();
    m_filingReplies.clear();
    m_filingAnswers.clear();
}

void ComposerOutboxTest::cleanup()
{
    m_factory.reset();
    m_dir.reset();
}

Outbox::Message ComposerOutboxTest::message(const QString &subject) const
{
    Outbox::Message message;
    message.accountId = QStringLiteral("work");
    message.subject = subject;
    message.from = "me@example.org";
    message.recipients = {"you@example.org", "hidden@example.org"};
    message.data = "Subject: " + subject.toUtf8() + "\r\n\r\nBody\r\n";
    message.sentCopy = "Bcc: hidden@example.org\r\n" + message.data;
    message.sentFolder = QStringLiteral("Sent");
    message.timestamp = QDateTime(QDate(2026, 11, 28), QTime(9, 30), QTimeZone::fromSecondsAheadOfUtc(3600));
    message.draft = "draft of " + subject.toUtf8();
    return message;
}

Outbox::Transport ComposerOutboxTest::transport()
{
    Outbox::Transport transport;
    MSA::FakeFactory *factory = m_factory.get();
    transport.createMsa = [factory](QObject *parent) { return factory->create(parent); };
    transport.online = [this]() { return m_online; };
    transport.fileSentCopy = [this](const QString &folder, const QByteArray &data, const QDateTime &timestamp,
                                 const Outbox::FilingReply &reply) {
        QCOMPARE(folder, QStringLiteral("Sent"));
        QCOMPARE(timestamp.offsetFromUtc(), 3600);
        m_filed.append(data);
        if (m_filingAnswers.isEmpty())
            m_filingReplies.append(reply);
        else
            reply(m_filingAnswers.takeFirst(), QStringLiteral("NO [TRYCREATE] Mailbox is gone"));
    };
    return transport;
}

std::unique_ptr<Outbox> ComposerOutboxTest::makeOutbox()
{
    auto outbox = std::make_unique<Outbox>(
        m_dir->filePath(QStringLiteral("Outbox")), m_dir->filePath(QStringLiteral("Drafts")));
    outbox->setBackoff([](int) { return 0; });
    return outbox;
}

void ComposerOutboxTest::testPersistsAcrossRestart()
{
    QString id;
    {
        auto outbox = makeOutbox();
        QSignalSpy changed(outbox.get(), &Outbox::changed);
        id = outbox->enqueue(message());
        QVERIFY(!id.isEmpty());
        QCOMPARE(outbox->count(), 1);
        QCOMPARE(outbox->count(QStringLiteral("work")), 1);
        QCOMPARE(outbox->count(QStringLiteral("home")), 0);
        QCOMPARE(changed.count(), 1);
    }
    auto outbox = makeOutbox();
    QCOMPARE(outbox->count(), 1);
    const Outbox::Entry entry = outbox->entry(id);
    QCOMPARE(entry.id, id);
    QCOMPARE(entry.accountId, QStringLiteral("work"));
    QCOMPARE(entry.state, Outbox::State::Queued);
    QCOMPARE(entry.subject, QStringLiteral("Hello"));
    QCOMPARE(entry.from, QByteArray("me@example.org"));
    QCOMPARE(entry.recipients, (QList<QByteArray>{"you@example.org", "hidden@example.org"}));
    QCOMPARE(entry.sentFolder, QStringLiteral("Sent"));
    QCOMPARE(entry.timestamp, message().timestamp);
    QVERIFY(entry.hasDraft);
    QVERIFY(entry.queued.isValid());
    const QFileInfo folder(outbox->folder() + QLatin1Char('/') + id);
    QCOMPARE(folder.permissions() & (QFile::ReadGroup | QFile::ReadOther), QFile::Permissions());
}

void ComposerOutboxTest::testSendsThenFiles()
{
    auto outbox = makeOutbox();
    QSignalSpy requested(m_factory.get(), &MSA::FakeFactory::requestedSending);
    QSignalSpy sent(outbox.get(), &Outbox::sent);
    outbox->setTransport(QStringLiteral("work"), transport());
    const QString id = outbox->enqueue(message());
    QVERIFY(requested.wait(1000));
    QCOMPARE(requested.at(0).at(0).toByteArray(), QByteArray("me@example.org"));
    QCOMPARE(requested.at(0).at(1).value<QList<QByteArray>>(), message().recipients);
    // Recipients never see Bcc
    QCOMPARE(requested.at(0).at(2).toByteArray(), message().data);
    QCOMPARE(storedState(*outbox, id), QStringLiteral("sending"));
    QVERIFY(outbox->isBusy(id));

    m_factory->doEmitSent();
    // Recorded as sent before the copy is filed
    QCOMPARE(storedState(*outbox, id), QStringLiteral("submitted"));
    QCOMPARE(m_filed, QList<QByteArray>{message().sentCopy});
    QCOMPARE(sent.count(), 0);
    m_filingReplies.takeFirst()(true, QString());
    QCOMPARE(sent.count(), 1);
    QCOMPARE(sent.at(0).at(0).toString(), id);
    QCOMPARE(outbox->count(), 0);
    QVERIFY(!QDir(outbox->folder() + QLatin1Char('/') + id).exists());
}

void ComposerOutboxTest::testFilingFailureNeverSendsAgain()
{
    auto outbox = makeOutbox();
    QSignalSpy requested(m_factory.get(), &MSA::FakeFactory::requestedSending);
    QSignalSpy failed(outbox.get(), &Outbox::failed);
    QSignalSpy sent(outbox.get(), &Outbox::sent);
    outbox->setTransport(QStringLiteral("work"), transport());
    const QString id = outbox->enqueue(message());
    QVERIFY(requested.wait(1000));
    m_filingAnswers = {false};
    m_factory->doEmitSent();
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.at(0).at(2).toBool(), true);
    QVERIFY(failed.at(0).at(1).toString().contains(QLatin1String("TRYCREATE")));
    QCOMPARE(outbox->entry(id).state, Outbox::State::Submitted);
    QCOMPARE(outbox->entry(id).attempts, 1);

    // The next try only files the copy
    QTRY_COMPARE(m_filed.size(), 2);
    m_filingReplies.takeFirst()(true, QString());
    QCOMPARE(sent.count(), 1);
    QCOMPARE(requested.count(), 1);
    QCOMPARE(outbox->count(), 0);
}

void ComposerOutboxTest::testRestartWhileFilingOnlyFiles()
{
    QString id;
    {
        auto outbox = makeOutbox();
        QSignalSpy requested(m_factory.get(), &MSA::FakeFactory::requestedSending);
        outbox->setTransport(QStringLiteral("work"), transport());
        id = outbox->enqueue(message());
        QVERIFY(requested.wait(1000));
        m_factory->doEmitSent();
        QCOMPARE(m_filingReplies.size(), 1);
    }
    // The pending reply belongs to the old outbox and is ignored
    m_filingReplies.takeFirst()(true, QString());
    auto outbox = makeOutbox();
    QCOMPARE(outbox->entry(id).state, Outbox::State::Submitted);
    QSignalSpy requested(m_factory.get(), &MSA::FakeFactory::requestedSending);
    QSignalSpy sent(outbox.get(), &Outbox::sent);
    m_filingAnswers = {true};
    outbox->setTransport(QStringLiteral("work"), transport());
    QTRY_COMPARE(sent.count(), 1);
    QCOMPARE(requested.count(), 0);
    QCOMPARE(m_filed.size(), 2);
}

void ComposerOutboxTest::testInterruptedWaitsForTheUser()
{
    QString id;
    {
        auto outbox = makeOutbox();
        QSignalSpy requested(m_factory.get(), &MSA::FakeFactory::requestedSending);
        outbox->setTransport(QStringLiteral("work"), transport());
        id = outbox->enqueue(message());
        QVERIFY(requested.wait(1000));
    }
    auto outbox = makeOutbox();
    QCOMPARE(outbox->entry(id).state, Outbox::State::Interrupted);
    QVERIFY(!outbox->entry(id).lastError.isEmpty());
    QCOMPARE(storedState(*outbox, id), QStringLiteral("interrupted"));
    QSignalSpy requested(m_factory.get(), &MSA::FakeFactory::requestedSending);
    outbox->setTransport(QStringLiteral("work"), transport());
    outbox->wake();
    QVERIFY(!requested.wait(100));
    QVERIFY(!outbox->sendNow(id));

    QVERIFY(outbox->sendAgain(id));
    QVERIFY(requested.wait(1000));
}

void ComposerOutboxTest::testRefusedBecomesDraftWithReason()
{
    auto outbox = makeOutbox();
    QSignalSpy requested(m_factory.get(), &MSA::FakeFactory::requestedSending);
    QString refusedId, reason, path;
    connect(outbox.get(), &Outbox::refused, this,
        [&](const Outbox::Entry &entry, const QString &why, const QString &draftPath) {
            refusedId = entry.id;
            reason = why;
            path = draftPath;
        });
    outbox->setTransport(QStringLiteral("work"), transport());
    const QString id = outbox->enqueue(message(QStringLiteral("Quarterly/report")));
    QVERIFY(requested.wait(1000));
    m_factory->doEmitRefusal(QStringLiteral("550 5.1.1 No such user"));
    QCOMPARE(refusedId, id);
    QCOMPARE(reason, QStringLiteral("550 5.1.1 No such user"));
    QVERIFY(path.startsWith(outbox->draftsFolder()));
    QVERIFY(path.endsWith(QLatin1String(".draft")));
    QVERIFY(!QFileInfo(path).fileName().contains(QLatin1Char('/')));
    QCOMPARE(readAll(path), QByteArray("draft of Quarterly/report"));
    QCOMPARE(readAll(path + QLatin1String(Outbox::reasonSuffix)), QByteArray("550 5.1.1 No such user"));
    QCOMPARE(outbox->count(), 0);
    QVERIFY(m_filed.isEmpty());

    // Another refusal with the same subject keeps the first draft
    outbox->enqueue(message(QStringLiteral("Quarterly/report")));
    QVERIFY(requested.wait(1000));
    const QString first = path;
    m_factory->doEmitRefusal(QStringLiteral("552 Message too large"));
    QVERIFY(path != first);
    QVERIFY(QFile::exists(first));
}

void ComposerOutboxTest::testRefusedWithoutDraftKeepsTheMessage()
{
    auto outbox = makeOutbox();
    QSignalSpy requested(m_factory.get(), &MSA::FakeFactory::requestedSending);
    QString path;
    connect(outbox.get(), &Outbox::refused, this,
        [&](const Outbox::Entry &, const QString &, const QString &draftPath) { path = draftPath; });
    outbox->setTransport(QStringLiteral("work"), transport());
    Outbox::Message bounce = message(QStringLiteral("Redirected"));
    bounce.draft.clear();
    outbox->enqueue(bounce);
    QVERIFY(requested.wait(1000));
    m_factory->doEmitRefusal(QStringLiteral("554 Relaying denied"));
    QVERIFY(path.endsWith(QLatin1String(".eml")));
    QCOMPARE(readAll(path), bounce.data);
}

void ComposerOutboxTest::testTransientFailureRetriesWithBackoff()
{
    auto outbox = makeOutbox();
    QList<int> asked;
    outbox->setBackoff([&asked](int attempts) {
        asked.append(attempts);
        return 50;
    });
    QSignalSpy requested(m_factory.get(), &MSA::FakeFactory::requestedSending);
    QSignalSpy failed(outbox.get(), &Outbox::failed);
    outbox->setTransport(QStringLiteral("work"), transport());
    const QString id = outbox->enqueue(message());
    QVERIFY(requested.wait(1000));
    m_factory->doEmitError(QStringLiteral("Connection refused"));
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.at(0).at(2).toBool(), false);
    Outbox::Entry entry = outbox->entry(id);
    QCOMPARE(entry.state, Outbox::State::Queued);
    QCOMPARE(entry.attempts, 1);
    QCOMPARE(entry.lastError, QStringLiteral("Connection refused"));
    QVERIFY(entry.nextAttempt > QDateTime::currentDateTimeUtc().addMSecs(-1));

    QVERIFY(requested.wait(1000));
    QCOMPARE(requested.count(), 2);
    m_factory->doEmitError(QStringLiteral("Connection refused"));
    QCOMPARE(asked, (QList<int>{1, 2}));
    QVERIFY(requested.wait(1000));
    m_factory->doEmitSent();
    QCOMPARE(outbox->entry(id).attempts, 0);
}

void ComposerOutboxTest::testOfflineWaitsUntilWoken()
{
    auto outbox = makeOutbox();
    QSignalSpy requested(m_factory.get(), &MSA::FakeFactory::requestedSending);
    m_online = false;
    outbox->setTransport(QStringLiteral("work"), transport());
    outbox->enqueue(message());
    QVERIFY(!requested.wait(100));
    m_online = true;
    outbox->wake(QStringLiteral("home"));
    QVERIFY(requested.wait(1000));
}

void ComposerOutboxTest::testOneAtATimeInOrder()
{
    auto outbox = makeOutbox();
    QSignalSpy requested(m_factory.get(), &MSA::FakeFactory::requestedSending);
    m_filingAnswers = {true, true};
    const QString first = outbox->enqueue(message(QStringLiteral("First")));
    const QString second = outbox->enqueue(message(QStringLiteral("Second")));
    outbox->setTransport(QStringLiteral("work"), transport());
    QVERIFY(requested.wait(1000));
    QVERIFY(!requested.wait(50));
    QCOMPARE(requested.count(), 1);
    QVERIFY(requested.at(0).at(2).toByteArray().contains("First"));
    QVERIFY(outbox->isBusy(first));

    // A failed first message keeps the second one waiting behind it
    outbox->setBackoff([](int) { return 60 * 60 * 1000; });
    m_factory->doEmitError(QStringLiteral("Timeout"));
    QVERIFY(!requested.wait(50));
    QVERIFY(outbox->sendNow(second));
    QVERIFY(requested.wait(1000));
    QVERIFY(requested.at(1).at(2).toByteArray().contains("First"));
    m_factory->doEmitSent();
    QVERIFY(requested.wait(1000));
    QVERIFY(requested.at(2).at(2).toByteArray().contains("Second"));
}

void ComposerOutboxTest::testMissingPasswordRetriesLater()
{
    auto outbox = makeOutbox();
    QSignalSpy requested(m_factory.get(), &MSA::FakeFactory::requestedSending);
    QSignalSpy cancelled(m_factory.get(), &MSA::FakeFactory::requestedCancelling);
    QSignalSpy failed(outbox.get(), &Outbox::failed);
    Outbox::Transport withoutPassword = transport();
    QStringList askedFor;
    withoutPassword.password
        = [&askedFor](const QString &user, const QString &host, const Outbox::PasswordReply &reply) {
              askedFor << user << host;
              reply(QString());
          };
    outbox->setBackoff([](int) { return 60 * 60 * 1000; });
    outbox->setTransport(QStringLiteral("work"), withoutPassword);
    const QString id = outbox->enqueue(message());
    QVERIFY(requested.wait(1000));
    emit m_factory->lastMSA()->passwordRequested(QStringLiteral("me"), QStringLiteral("smtp.example.org"));
    QCOMPARE(askedFor, (QStringList{QStringLiteral("me"), QStringLiteral("smtp.example.org")}));
    QCOMPARE(failed.count(), 1);
    QCOMPARE(cancelled.count(), 1);
    QCOMPARE(outbox->entry(id).state, Outbox::State::Queued);
}

void ComposerOutboxTest::testSilentServerInterrupts()
{
    auto outbox = makeOutbox();
    outbox->setAttemptTimeout(50);
    QSignalSpy requested(m_factory.get(), &MSA::FakeFactory::requestedSending);
    QSignalSpy interrupted(outbox.get(), &Outbox::interrupted);
    QSignalSpy cancelled(m_factory.get(), &MSA::FakeFactory::requestedCancelling);
    outbox->setTransport(QStringLiteral("work"), transport());
    const QString id = outbox->enqueue(message());
    QVERIFY(requested.wait(1000));
    QVERIFY(interrupted.wait(1000));
    QCOMPARE(cancelled.count(), 1);
    QCOMPARE(outbox->entry(id).state, Outbox::State::Interrupted);
    QCOMPARE(storedState(*outbox, id), QStringLiteral("interrupted"));
    QVERIFY(!requested.wait(100));
}

void ComposerOutboxTest::testMoveToDrafts()
{
    auto outbox = makeOutbox();
    const QString id = outbox->enqueue(message(QStringLiteral("Later")));
    QString error;
    const QString path = outbox->moveToDrafts(id, &error);
    QVERIFY2(!path.isEmpty(), qPrintable(error));
    QCOMPARE(QFileInfo(path).fileName(), QStringLiteral("Later.draft"));
    QCOMPARE(readAll(path), QByteArray("draft of Later"));
    QVERIFY(!QFile::exists(path + QLatin1String(Outbox::reasonSuffix)));
    QCOMPARE(outbox->count(), 0);
    QVERIFY(outbox->moveToDrafts(id).isEmpty());
}

void ComposerOutboxTest::testDefaultBackoff()
{
    QCOMPARE(Outbox::defaultBackoff(1), 60 * 1000);
    QCOMPARE(Outbox::defaultBackoff(2), 2 * 60 * 1000);
    QCOMPARE(Outbox::defaultBackoff(5), 16 * 60 * 1000);
    QCOMPARE(Outbox::defaultBackoff(6), 30 * 60 * 1000);
    QCOMPARE(Outbox::defaultBackoff(40), 30 * 60 * 1000);
}

QTEST_GUILESS_MAIN(ComposerOutboxTest)
