/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "MSA/SMTP.h"
#include "qwwsmtpclient/qwwsmtpclient.h"
#include <QRegularExpression>
#include <QSignalSpy>
#include <QSslSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

namespace {

/** @short A loopback submission server without authentication */
class FakeSmtpServer : public QObject {
public:
  enum class Behavior {
    /** Accept the message and close the connection after QUIT */
    Accept,
    /** Close the connection when the client sends MAIL FROM */
    DropOnMail,
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
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
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
          } else if (command == "QUIT") {
            socket->write("221 2.0.0 Bye\r\n");
            socket->disconnectFromHost();
          } else {
            socket->write("502 5.5.1 Unrecognized command\r\n");
          }
        }
      });
      socket->write("220 fake.example.test ESMTP\r\n");
    }
  }

  Behavior m_behavior;
  QTcpServer m_server;
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
};

QTEST_GUILESS_MAIN(SmtpClientTest)

#include "test_SmtpClient.moc"
