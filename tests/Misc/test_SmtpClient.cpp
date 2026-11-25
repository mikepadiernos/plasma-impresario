/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Common/SettingsNames.h"
#include "MSA/Account.h"
#include "MSA/SMTP.h"
#include "qwwsmtpclient/qwwsmtpclient.h"
#include <QRegularExpression>
#include <QSettings>
#include <QSignalSpy>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

namespace {

/** @short A self-signed certificate for fake.example.test, which no system
 * trusts */
const QByteArray serverCertificate = QByteArrayLiteral(
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBjzCCATWgAwIBAgIUHXfIolb9MD+LIRAPE6YYNqam5sUwCgYIKoZIzj0EAwIw\n"
    "HDEaMBgGA1UEAwwRZmFrZS5leGFtcGxlLnRlc3QwIBcNMjYxMTI1MTUyNjU3WhgP\n"
    "MjEyNjExMDExNTI2NTdaMBwxGjAYBgNVBAMMEWZha2UuZXhhbXBsZS50ZXN0MFkw\n"
    "EwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE9I9ZgooTMc2guwHUUfvH0z+J1jt1jCow\n"
    "zLhYaisWdQZQJwqVerPFxc52doJ80amEJBR0DhpR3fS5WOxpdVbYbKNTMFEwHQYD\n"
    "VR0OBBYEFNafphxjnPL/GloSBDWLKmeuCVUKMB8GA1UdIwQYMBaAFNafphxjnPL/\n"
    "GloSBDWLKmeuCVUKMA8GA1UdEwEB/wQFMAMBAf8wCgYIKoZIzj0EAwIDSAAwRQIg\n"
    "WK80Z+14xvDKXdnhC+PLr3bj4vhz2PSk9a1Sr+noQi8CIQCVERIG3ekwZ/sjh0ok\n"
    "7aymUG+iqeSvUXNV3HW27qtx5A==\n"
    "-----END CERTIFICATE-----\n");

/** @short The test-only private key of serverCertificate */
const QByteArray serverKey = QByteArrayLiteral(
    "-----BEGIN PRIVATE KEY-----\n"
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg/AHHyBBDfcI/XOs0\n"
    "NiS2fYTGTZFOqVq3DcqKvZd04TShRANCAAT0j1mCihMxzaC7AdRR+8fTP4nWO3WM\n"
    "KjDMuFhqKxZ1BlAnCpV6s8XFznZ2gnzRqYQkFHQOGlHd9LlY7Gl1Vths\n"
    "-----END PRIVATE KEY-----\n");

QByteArray serverPublicKey() {
  return QSslCertificate(serverCertificate).publicKey().toPem();
}

/** @short Accepts plain TCP connections and hands them out as QSslSocket */
class SslCapableServer : public QTcpServer {
protected:
  void incomingConnection(qintptr descriptor) override {
    auto socket = new QSslSocket(this);
    if (!socket->setSocketDescriptor(descriptor)) {
      delete socket;
      return;
    }
    socket->setLocalCertificate(QSslCertificate(serverCertificate));
    socket->setPrivateKey(QSslKey(serverKey, QSsl::Ec));
    addPendingConnection(socket);
  }
};

/** @short A loopback submission server without authentication */
class FakeSmtpServer : public QObject {
public:
  enum class Behavior {
    /** Accept the message and close the connection after QUIT */
    Accept,
    /** Close the connection when the client sends MAIL FROM */
    DropOnMail,
    /** Like Accept, but over implicit TLS with an untrusted certificate */
    ImplicitTls,
    /** Like Accept, but upgrades the connection on STARTTLS with an untrusted
       certificate */
    StartTls,
  };

  explicit FakeSmtpServer(Behavior behavior) : m_behavior(behavior) {
    connect(&m_server, &QTcpServer::newConnection, this,
            &FakeSmtpServer::accept);
    QVERIFY(m_server.listen(QHostAddress::LocalHost, 0));
  }

  quint16 port() const { return m_server.serverPort(); }

  QList<QByteArray> messages;

private:
  void accept() {
    while (auto socket =
               static_cast<QSslSocket *>(m_server.nextPendingConnection())) {
      socket->setParent(this);
      auto inData = std::make_shared<bool>(false);
      auto message = std::make_shared<QByteArray>();
      connect(socket, &QTcpSocket::readyRead, this, [=, this] {
        while (socket->canReadLine()) {
          const QByteArray line = socket->readLine();
          if (*inData) {
            if (line == ".\r\n") {
              *inData = false;
              messages << *message;
              socket->write("250 2.0.0 Queued\r\n");
            } else {
              *message += line;
            }
            continue;
          }
          const QByteArray command = line.left(4).toUpper();
          if (command == "EHLO") {
            socket->write("250-fake.example.test\r\n250 8BITMIME\r\n");
          } else if (command == "MAIL" && m_behavior == Behavior::DropOnMail) {
            socket->abort();
            return;
          } else if (command == "MAIL" || command == "RCPT") {
            socket->write("250 2.1.0 OK\r\n");
          } else if (command == "DATA") {
            *inData = true;
            socket->write("354 Go ahead\r\n");
          } else if (command == "STAR" && m_behavior == Behavior::StartTls) {
            socket->write("220 2.0.0 Ready to start TLS\r\n");
            socket->startServerEncryption();
          } else if (command == "QUIT") {
            socket->write("221 2.0.0 Bye\r\n");
            socket->disconnectFromHost();
          } else {
            socket->write("502 5.5.1 Unrecognized command\r\n");
          }
        }
      });
      if (m_behavior == Behavior::ImplicitTls)
        socket->startServerEncryption();
      socket->write("220 fake.example.test ESMTP\r\n");
    }
  }

  Behavior m_behavior;
  SslCapableServer m_server;
};

/** @short A port on the loopback interface with nothing listening on it */
quint16 closedPort() {
  QTcpServer server;
  if (!server.listen(QHostAddress::LocalHost, 0))
    return 0;
  const quint16 port = server.serverPort();
  server.close();
  return port;
}

void send(MSA::SMTP &smtp) {
  smtp.sendMail(QByteArrayLiteral("from@example.com"),
                {QByteArrayLiteral("rcpt@example.com")},
                QByteArrayLiteral("Subject: test\r\n\r\nbody\r\n"));
}

/** @short Records certificate checks and optionally answers them like the
 * composer */
struct CertificateCheck {
  enum class Answer { Accept, Reject, None };

  CertificateCheck(MSA::SMTP &smtp, Answer answer) {
    QObject::connect(&smtp, &MSA::AbstractMSA::certificateCheckRequested,
                     &smtp,
                     [this, &smtp, answer](const QList<QSslCertificate> &chain,
                                           const QList<QSslError> &errors) {
                       ++count;
                       offeredKey = chain.isEmpty()
                                        ? QByteArray()
                                        : chain.first().publicKey().toPem();
                       this->errors = errors;
                       if (answer != Answer::None)
                         smtp.setCertificateAccepted(answer == Answer::Accept);
                     });
  }

  int count = 0;
  QByteArray offeredKey;
  QList<QSslError> errors;
};

} // namespace

/** @short Delivery outcomes of the SMTP client, including socket failures */
class SmtpClientTest : public QObject {
  Q_OBJECT

private slots:
  void init() {
    // A connection made by signal name to a signal Qt no longer has only warns
    // at runtime
    QTest::failOnWarning(QRegularExpression(
        QStringLiteral("QObject::connect.*No such (signal|slot)")));
  }

  void connectsToTheSocketSignals() {
    QwwSmtpClient client;
    Q_UNUSED(client);
  }

  void deliveryReportsSentOnce() {
    FakeSmtpServer server(FakeSmtpServer::Behavior::Accept);
    MSA::SMTP smtp(nullptr, QStringLiteral("127.0.0.1"), server.port(), false,
                   false, false, QString());
    QSignalSpy sent(&smtp, &MSA::AbstractMSA::sent);
    QSignalSpy errors(&smtp, &MSA::AbstractMSA::error);

    send(smtp);
    QVERIFY2(sent.wait(10000),
             qPrintable(errors.isEmpty() ? QString()
                                         : errors.first().first().toString()));
    QCOMPARE(server.messages.size(), 1);
    // The server closing the connection after QUIT is not an error
    QTest::qWait(300);
    QCOMPARE(sent.size(), 1);
    QVERIFY(errors.isEmpty());
  }

  void connectionRefusedReportsOneError() {
    const quint16 port = closedPort();
    QVERIFY(port);
    MSA::SMTP smtp(nullptr, QStringLiteral("127.0.0.1"), port, false, false,
                   false, QString());
    QSignalSpy sent(&smtp, &MSA::AbstractMSA::sent);
    QSignalSpy errors(&smtp, &MSA::AbstractMSA::error);

    send(smtp);
    QVERIFY(errors.wait(10000));
    QTest::qWait(300);
    QCOMPARE(errors.size(), 1);
    QVERIFY(!errors.first().first().toString().isEmpty());
    QVERIFY(sent.isEmpty());
  }

  void droppedConnectionReportsOneError() {
    FakeSmtpServer server(FakeSmtpServer::Behavior::DropOnMail);
    MSA::SMTP smtp(nullptr, QStringLiteral("127.0.0.1"), server.port(), false,
                   false, false, QString());
    QSignalSpy sent(&smtp, &MSA::AbstractMSA::sent);
    QSignalSpy errors(&smtp, &MSA::AbstractMSA::error);

    send(smtp);
    QVERIFY(errors.wait(10000));
    QTest::qWait(300);
    QCOMPARE(errors.size(), 1);
    QVERIFY(sent.isEmpty());
    QVERIFY(server.messages.isEmpty());
  }

  void failedTlsHandshakeReportsOneError() {
    if (!QSslSocket::supportsSsl())
      QSKIP("TLS is not available");
    // A plain-text server cannot complete the handshake of an implicit TLS
    // connection
    FakeSmtpServer server(FakeSmtpServer::Behavior::Accept);
    MSA::SMTP smtp(nullptr, QStringLiteral("127.0.0.1"), server.port(), true,
                   false, false, QString());
    QSignalSpy sent(&smtp, &MSA::AbstractMSA::sent);
    QSignalSpy errors(&smtp, &MSA::AbstractMSA::error);

    send(smtp);
    QVERIFY(errors.wait(10000));
    QTest::qWait(300);
    QCOMPARE(errors.size(), 1);
    QVERIFY(sent.isEmpty());
    QVERIFY(server.messages.isEmpty());
  }

  void untrustedCertificateCanBeAccepted_data() {
    QTest::addColumn<bool>("startTls");
    QTest::newRow("implicit TLS") << false;
    QTest::newRow("STARTTLS") << true;
  }

  void untrustedCertificateCanBeAccepted() {
    if (!QSslSocket::supportsSsl())
      QSKIP("TLS is not available");
    QFETCH(bool, startTls);
    FakeSmtpServer server(startTls ? FakeSmtpServer::Behavior::StartTls
                                   : FakeSmtpServer::Behavior::ImplicitTls);
    MSA::SMTP smtp(nullptr, QStringLiteral("127.0.0.1"), server.port(),
                   !startTls, startTls, false, QString());
    CertificateCheck check(smtp, CertificateCheck::Answer::Accept);
    QSignalSpy sent(&smtp, &MSA::AbstractMSA::sent);
    QSignalSpy errors(&smtp, &MSA::AbstractMSA::error);

    send(smtp);
    QVERIFY2(sent.wait(10000),
             qPrintable(errors.isEmpty() ? QString()
                                         : errors.first().first().toString()));
    QCOMPARE(check.count, 1);
    QCOMPARE(check.offeredKey, serverPublicKey());
    QVERIFY(!check.errors.isEmpty());
    QCOMPARE(server.messages.size(), 1);
    QTest::qWait(300);
    QCOMPARE(sent.size(), 1);
    QVERIFY(errors.isEmpty());
  }

  void rejectedCertificateReportsOneError() {
    if (!QSslSocket::supportsSsl())
      QSKIP("TLS is not available");
    FakeSmtpServer server(FakeSmtpServer::Behavior::ImplicitTls);
    MSA::SMTP smtp(nullptr, QStringLiteral("127.0.0.1"), server.port(), true,
                   false, false, QString());
    CertificateCheck check(smtp, CertificateCheck::Answer::Reject);
    QSignalSpy sent(&smtp, &MSA::AbstractMSA::sent);
    QSignalSpy errors(&smtp, &MSA::AbstractMSA::error);

    send(smtp);
    QVERIFY(errors.wait(10000));
    QTest::qWait(300);
    QCOMPARE(check.count, 1);
    QCOMPARE(errors.size(), 1);
    QVERIFY(errors.first().first().toString().contains(QLatin1String("SSL")));
    QVERIFY(sent.isEmpty());
    QVERIFY(server.messages.isEmpty());
  }

  void pinnedCertificateIsAcceptedWithoutAsking() {
    if (!QSslSocket::supportsSsl())
      QSKIP("TLS is not available");
    FakeSmtpServer server(FakeSmtpServer::Behavior::ImplicitTls);
    MSA::SMTPFactory factory(QStringLiteral("127.0.0.1"), server.port(), true,
                             false, false, QString());
    factory.setPinnedPublicKey(serverPublicKey());
    std::unique_ptr<MSA::AbstractMSA> msa(factory.create(nullptr));
    auto &smtp = static_cast<MSA::SMTP &>(*msa);
    CertificateCheck check(smtp, CertificateCheck::Answer::Reject);
    QSignalSpy sent(&smtp, &MSA::AbstractMSA::sent);
    QSignalSpy errors(&smtp, &MSA::AbstractMSA::error);

    send(smtp);
    QVERIFY2(sent.wait(10000),
             qPrintable(errors.isEmpty() ? QString()
                                         : errors.first().first().toString()));
    QCOMPARE(check.count, 0);
    QCOMPARE(server.messages.size(), 1);
  }

  void changedCertificateAsksAgain() {
    if (!QSslSocket::supportsSsl())
      QSKIP("TLS is not available");
    FakeSmtpServer server(FakeSmtpServer::Behavior::ImplicitTls);
    MSA::SMTP smtp(nullptr, QStringLiteral("127.0.0.1"), server.port(), true,
                   false, false, QString());
    smtp.setPinnedPublicKey(QByteArrayLiteral("a key the server no longer has"));
    CertificateCheck check(smtp, CertificateCheck::Answer::Reject);
    QSignalSpy errors(&smtp, &MSA::AbstractMSA::error);

    send(smtp);
    QVERIFY(errors.wait(10000));
    QCOMPARE(check.count, 1);
    QVERIFY(server.messages.isEmpty());
  }

  void unansweredCertificateFailsWithoutListener() {
    if (!QSslSocket::supportsSsl())
      QSKIP("TLS is not available");
    FakeSmtpServer server(FakeSmtpServer::Behavior::ImplicitTls);
    MSA::SMTP smtp(nullptr, QStringLiteral("127.0.0.1"), server.port(), true,
                   false, false, QString());
    QSignalSpy sent(&smtp, &MSA::AbstractMSA::sent);
    QSignalSpy errors(&smtp, &MSA::AbstractMSA::error);

    send(smtp);
    QVERIFY(errors.wait(10000));
    QTest::qWait(300);
    QCOMPARE(errors.size(), 1);
    QVERIFY(sent.isEmpty());
    QVERIFY(server.messages.isEmpty());
  }

  void cancellingDuringCertificateCheckReportsOneError() {
    if (!QSslSocket::supportsSsl())
      QSKIP("TLS is not available");
    FakeSmtpServer server(FakeSmtpServer::Behavior::ImplicitTls);
    MSA::SMTP smtp(nullptr, QStringLiteral("127.0.0.1"), server.port(), true,
                   false, false, QString());
    CertificateCheck check(smtp, CertificateCheck::Answer::None);
    QSignalSpy sent(&smtp, &MSA::AbstractMSA::sent);
    QSignalSpy errors(&smtp, &MSA::AbstractMSA::error);

    send(smtp);
    QTRY_COMPARE_WITH_TIMEOUT(check.count, 1, 10000);
    smtp.cancel();
    // A late answer from a prompt that was still open has no effect
    smtp.setCertificateAccepted(true);
    QTest::qWait(300);
    QCOMPARE(errors.size(), 1);
    QVERIFY(sent.isEmpty());
    QVERIFY(server.messages.isEmpty());
  }

  void changingTheServerForgetsTheAcceptedCertificate() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings(dir.filePath(QStringLiteral("account.conf")),
                       QSettings::IniFormat);
    using Common::SettingsNames;
    settings.setValue(SettingsNames::msaMethodKey, SettingsNames::methodSSMTP);
    settings.setValue(SettingsNames::smtpHostKey,
                      QStringLiteral("smtp.example.org"));
    settings.setValue(SettingsNames::smtpSslPemPubKey, serverPublicKey());

    MSA::Account account(nullptr, &settings, QStringLiteral("account"));
    account.saveSettings();
    QCOMPARE(settings.value(SettingsNames::smtpSslPemPubKey).toByteArray(),
             serverPublicKey());

    account.setServer(QStringLiteral("smtp.example.net"));
    account.saveSettings();
    QVERIFY(!settings.contains(SettingsNames::smtpSslPemPubKey));
  }
};

QTEST_GUILESS_MAIN(SmtpClientTest)

#include "test_SmtpClient.moc"
