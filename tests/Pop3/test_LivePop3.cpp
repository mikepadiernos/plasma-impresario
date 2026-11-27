/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QSslCertificate>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "LocalMail/Store.h"
#include "Pop3/FetchJob.h"

/**
 * @short POP3 downloads against a real server
 *
 * Opt-in: run through tools/dev-container/live-pop3.sh, which starts a disposable Dovecot with synthetic users and
 * sets PLASMA_TEST_POP3_PORT, PLASMA_TEST_POP3S_PORT, PLASMA_TEST_POP3_CERT and PLASMA_TEST_POP3_HOME. Without them
 * every case is skipped.
 */
class LivePop3Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void rejectsAnUntrustedCertificate();
    void downloadsOverTlsOnce();
    void downloadsOverStartTls();
    void removesMailAfterDownload();
    void reportsAWrongPassword();

private:
    struct Result {
        bool ok = false;
        Pop3::FetchJob::Failure failure = Pop3::FetchJob::Failure::Protocol;
        QString message;
        int downloaded = 0;
        int removed = 0;
        int rejectedCertificates = 0;
    };

    Pop3::Settings settingsFor(const QString &user, Pop3::Security security) const;
    Result run(const Pop3::Settings &settings, bool trustServer = true);
    int messagesOnServer(const QString &user) const;

    quint16 m_pop3Port = 0;
    quint16 m_pop3sPort = 0;
    QSslCertificate m_certificate;
    QString m_home;
    std::unique_ptr<QTemporaryDir> m_dir;
    std::shared_ptr<LocalMail::Store> m_store;
};

void LivePop3Test::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    bool pop3Ok = false;
    bool pop3sOk = false;
    m_pop3Port = qEnvironmentVariableIntValue("PLASMA_TEST_POP3_PORT", &pop3Ok);
    m_pop3sPort = qEnvironmentVariableIntValue("PLASMA_TEST_POP3S_PORT", &pop3sOk);
    m_home = qEnvironmentVariable("PLASMA_TEST_POP3_HOME");
    if (!pop3Ok || !pop3sOk || m_home.isEmpty())
        QSKIP("Run through tools/dev-container/live-pop3.sh");
    QFile certificate(qEnvironmentVariable("PLASMA_TEST_POP3_CERT"));
    QVERIFY2(certificate.open(QIODevice::ReadOnly), "PLASMA_TEST_POP3_CERT must name the server's certificate");
    m_certificate = QSslCertificate(&certificate, QSsl::Pem);
    QVERIFY(!m_certificate.isNull());
}

void LivePop3Test::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    QString error;
    m_store = LocalMail::Store::open(m_dir->filePath(QStringLiteral("store")), &error);
    QVERIFY2(m_store, qPrintable(error));
}

void LivePop3Test::cleanup()
{
    m_store.reset();
    m_dir.reset();
}

Pop3::Settings LivePop3Test::settingsFor(const QString &user, Pop3::Security security) const
{
    Pop3::Settings settings;
    settings.host = QStringLiteral("127.0.0.1");
    settings.security = security;
    settings.port = security == Pop3::Security::Tls ? m_pop3sPort : m_pop3Port;
    settings.user = user + QLatin1String("@example.test");
    settings.password = QStringLiteral("synthetic-test-only");
    return settings;
}

LivePop3Test::Result LivePop3Test::run(const Pop3::Settings &settings, bool trustServer)
{
    Result result;
    Pop3::FetchJob job(settings, m_store);
    job.setTimeout(20);
    if (trustServer) {
        // Only the certificate this run generated, as if the user had accepted it once
        const QSslCertificate expected = m_certificate;
        job.setTrustCheck([expected](const QString &, const QSslCertificate &certificate) {
            return certificate == expected;
        });
    }
    bool done = false;
    connect(&job, &Pop3::FetchJob::certificateRejected, this, [&]() { ++result.rejectedCertificates; });
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
    // Dovecot delays its answer to a wrong password
    [&]() { QTRY_VERIFY_WITH_TIMEOUT(done, 30000); }();
    result.removed = job.removedFromServer();
    return result;
}

int LivePop3Test::messagesOnServer(const QString &user) const
{
    int count = 0;
    for (const QString &part : {QStringLiteral("new"), QStringLiteral("cur")})
        count += QDir(m_home + QLatin1Char('/') + user + QLatin1String("/Maildir/") + part).entryList(QDir::Files).size();
    return count;
}

void LivePop3Test::rejectsAnUntrustedCertificate()
{
    const Result result = run(settingsFor(QStringLiteral("work"), Pop3::Security::Tls), false);
    QVERIFY(!result.ok);
    QCOMPARE(result.failure, Pop3::FetchJob::Failure::Certificate);
    QCOMPARE(result.rejectedCertificates, 1);
    QVERIFY(m_store->messages(QStringLiteral("INBOX")).isEmpty());
}

void LivePop3Test::downloadsOverTlsOnce()
{
    const Pop3::Settings settings = settingsFor(QStringLiteral("work"), Pop3::Security::Tls);
    Result result = run(settings);
    QVERIFY2(result.ok, qPrintable(result.message));
    QCOMPARE(result.downloaded, 2);
    QCOMPARE(result.removed, 0);

    const QList<LocalMail::MessageInfo> stored = m_store->messages(QStringLiteral("INBOX"));
    QCOMPARE(stored.size(), 2);
    QStringList subjects;
    for (const LocalMail::MessageInfo &info : stored) {
        QVERIFY(info.origin.startsWith(QLatin1String("uidl:")));
        const QByteArray content = m_store->content(QStringLiteral("INBOX"), info.uid);
        QVERIFY(content.contains("\r\n\r\nSynthetic body.\r\n"));
        const int start = content.indexOf("Subject: ") + 9;
        subjects << QString::fromLatin1(content.mid(start, content.indexOf('\r', start) - start));
    }
    subjects.sort();
    QCOMPARE(subjects, QStringList({QStringLiteral("Synthetic work-first"), QStringLiteral("Synthetic work-second")}));

    result = run(settings);
    QVERIFY2(result.ok, qPrintable(result.message));
    QCOMPARE(result.downloaded, 0);
    QCOMPARE(m_store->messages(QStringLiteral("INBOX")).size(), 2);
    QCOMPARE(messagesOnServer(QStringLiteral("work")), 2);
}

void LivePop3Test::downloadsOverStartTls()
{
    const Result result = run(settingsFor(QStringLiteral("work"), Pop3::Security::StartTls));
    QVERIFY2(result.ok, qPrintable(result.message));
    QCOMPARE(result.downloaded, 2);
    QCOMPARE(m_store->messages(QStringLiteral("INBOX")).size(), 2);
}

void LivePop3Test::removesMailAfterDownload()
{
    QCOMPARE(messagesOnServer(QStringLiteral("drop")), 2);
    Pop3::Settings settings = settingsFor(QStringLiteral("drop"), Pop3::Security::Tls);
    settings.retention = Pop3::Retention::AfterDownload;
    Result result = run(settings);
    QVERIFY2(result.ok, qPrintable(result.message));
    QCOMPARE(result.downloaded, 2);
    QCOMPARE(result.removed, 2);
    QCOMPARE(messagesOnServer(QStringLiteral("drop")), 0);
    QCOMPARE(m_store->messages(QStringLiteral("INBOX")).size(), 2);

    result = run(settings);
    QVERIFY2(result.ok, qPrintable(result.message));
    QCOMPARE(result.downloaded, 0);
    QCOMPARE(m_store->messages(QStringLiteral("INBOX")).size(), 2);
}

void LivePop3Test::reportsAWrongPassword()
{
    Pop3::Settings settings = settingsFor(QStringLiteral("work"), Pop3::Security::Tls);
    settings.password = QStringLiteral("not-the-password");
    const Result result = run(settings);
    QVERIFY(!result.ok);
    QCOMPARE(result.failure, Pop3::FetchJob::Failure::Authentication);
    QVERIFY(m_store->messages(QStringLiteral("INBOX")).isEmpty());
}

QTEST_GUILESS_MAIN(LivePop3Test)
#include "test_LivePop3.moc"
