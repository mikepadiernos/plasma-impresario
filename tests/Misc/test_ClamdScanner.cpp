/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "test_ClamdScanner.h"

#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTest>
#include <QtEndian>

#include <memory>

#include "UiUtils/ClamdScanner.h"

using UiUtils::ClamdScanner;
using UiUtils::MailSafety::ScanResult;

namespace
{

/** @short Speaks clamd's side of INSTREAM on a local socket */
struct FakeClamd {
  QLocalServer server;
  /** Sent with its terminating zero byte once the stream ends; an empty reply closes the connection instead */
  QByteArray reply = QByteArrayLiteral("stream: OK");
  /** Never answer */
  bool silent = false;

  QByteArray command;
  QByteArray payload;
  int chunks = 0;
  bool ended = false;
  bool answered = false;
  QByteArray buffer;

  bool listen(const QString &path)
  {
    QObject::connect(&server, &QLocalServer::newConnection, &server, [this]() {
      QLocalSocket *client = server.nextPendingConnection();
      QObject::connect(client, &QLocalSocket::readyRead, client, [this, client]() { read(client); });
    });
    QLocalServer::removeServer(path);
    return server.listen(path);
  }

  void read(QLocalSocket *client)
  {
    buffer += client->readAll();
    if (command.isEmpty()) {
      const int end = buffer.indexOf('\0');
      if (end < 0)
        return;
      command = buffer.left(end);
      buffer.remove(0, end + 1);
    }
    while (!ended && buffer.size() >= 4) {
      const quint32 length = qFromBigEndian<quint32>(buffer.constData());
      if (length == 0) {
        ended = true;
        buffer.remove(0, 4);
        break;
      }
      if (buffer.size() < 4 + int(length))
        return;
      payload += buffer.mid(4, length);
      ++chunks;
      buffer.remove(0, 4 + int(length));
    }
    if (!ended || silent || answered)
      return;
    answered = true;
    if (reply.isEmpty()) {
      client->disconnectFromServer();
      return;
    }
    client->write(reply + '\0');
    client->flush();
  }
};

struct Outcome {
  int calls = 0;
  ScanResult result = ScanResult::NotScanned;
  QString threat;
};

std::shared_ptr<Outcome> startScan(ClamdScanner &scanner, const QString &file, const QString &socket)
{
  auto outcome = std::make_shared<Outcome>();
  QObject::connect(&scanner, &ClamdScanner::finished, &scanner, [outcome](ScanResult result, const QString &threat) {
    ++outcome->calls;
    outcome->result = result;
    outcome->threat = threat;
  });
  scanner.scanFile(file, socket);
  return outcome;
}

}

void ClamdScannerTest::initTestCase()
{
  QVERIFY(m_dir.isValid());
}

QString ClamdScannerTest::writeFile(const QString &name, const QByteArray &content)
{
  QFile file(m_dir.filePath(name));
  if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size())
    return QString();
  return file.fileName();
}

QString ClamdScannerTest::socketPath(const QString &name) const
{
  return m_dir.filePath(name);
}

void ClamdScannerTest::chunkHasItsLengthFirst()
{
  QCOMPARE(ClamdScanner::chunk(QByteArrayLiteral("abc")), QByteArray("\0\0\0\3abc", 7));
  QCOMPARE(ClamdScanner::chunk(QByteArray(258, 'x')).left(4), QByteArray("\0\0\1\2", 4));
  QCOMPARE(ClamdScanner::chunk(QByteArray()), QByteArray(4, '\0'));
}

void ClamdScannerTest::parsesReplies_data()
{
  QTest::addColumn<QByteArray>("reply");
  QTest::addColumn<int>("result");
  QTest::addColumn<QString>("threat");

  QTest::newRow("clean") << QByteArray("stream: OK\0", 11) << int(ScanResult::Clean) << QString();
  QTest::newRow("clean with a newline") << QByteArrayLiteral("stream: OK\n") << int(ScanResult::Clean) << QString();
  QTest::newRow("found") << QByteArray("stream: Win.Test.EICAR_HDB-1 FOUND\0", 35) << int(ScanResult::Infected)
                         << QStringLiteral("Win.Test.EICAR_HDB-1");
  QTest::newRow("size limit") << QByteArrayLiteral("INSTREAM size limit exceeded. ERROR") << int(ScanResult::Failed)
                              << QString();
  QTest::newRow("error") << QByteArrayLiteral("stream: Can't allocate memory ERROR") << int(ScanResult::Failed) << QString();
  QTest::newRow("empty") << QByteArray() << int(ScanResult::Failed) << QString();
  QTest::newRow("unexpected") << QByteArrayLiteral("PONG") << int(ScanResult::Failed) << QString();
  QTest::newRow("OK elsewhere is not clean") << QByteArrayLiteral("stream: OK but not quite") << int(ScanResult::Failed)
                                             << QString();
}

void ClamdScannerTest::parsesReplies()
{
  QFETCH(QByteArray, reply);
  QFETCH(int, result);
  QFETCH(QString, threat);

  QString found;
  QCOMPARE(int(ClamdScanner::parseReply(reply, &found)), result);
  QCOMPARE(found, threat);
}

void ClamdScannerTest::findsTheFirstExistingSocket()
{
  const QString present = writeFile(QStringLiteral("present.ctl"), QByteArray());
  QVERIFY(!present.isEmpty());
  const QString missing = m_dir.filePath(QStringLiteral("missing.ctl"));
  QCOMPARE(ClamdScanner::findSocket({missing, QString(), present}), present);
  QCOMPARE(ClamdScanner::findSocket({missing}), QString());
  QVERIFY(!ClamdScanner::defaultSockets().isEmpty());
}

void ClamdScannerTest::streamsTheFileInChunks()
{
  FakeClamd clamd;
  QVERIFY(clamd.listen(socketPath(QStringLiteral("chunks.sock"))));
  const QByteArray content = QByteArrayLiteral("0123456789");
  const QString file = writeFile(QStringLiteral("chunks.bin"), content);

  ClamdScanner scanner;
  scanner.setChunkSize(4);
  const auto outcome = startScan(scanner, file, clamd.server.fullServerName());
  QTRY_COMPARE(outcome->calls, 1);
  QCOMPARE(outcome->result, ScanResult::Clean);
  QCOMPARE(clamd.command, QByteArrayLiteral("zINSTREAM"));
  QCOMPARE(clamd.payload, content);
  QCOMPARE(clamd.chunks, 3);
  QVERIFY(clamd.ended);
  QTest::qWait(50);
  QCOMPARE(outcome->calls, 1);
}

void ClamdScannerTest::emptyFileIsStillScanned()
{
  FakeClamd clamd;
  QVERIFY(clamd.listen(socketPath(QStringLiteral("empty.sock"))));
  const QString file = writeFile(QStringLiteral("empty.bin"), QByteArray());

  ClamdScanner scanner;
  const auto outcome = startScan(scanner, file, clamd.server.fullServerName());
  QTRY_COMPARE(outcome->calls, 1);
  QCOMPARE(outcome->result, ScanResult::Clean);
  QCOMPARE(clamd.chunks, 0);
  QVERIFY(clamd.ended);
}

void ClamdScannerTest::reportsTheThreat()
{
  FakeClamd clamd;
  clamd.reply = QByteArrayLiteral("stream: Eicar-Signature FOUND");
  QVERIFY(clamd.listen(socketPath(QStringLiteral("found.sock"))));
  const QString file = writeFile(QStringLiteral("found.bin"), QByteArrayLiteral("not really a virus"));

  ClamdScanner scanner;
  const auto outcome = startScan(scanner, file, clamd.server.fullServerName());
  QTRY_COMPARE(outcome->calls, 1);
  QCOMPARE(outcome->result, ScanResult::Infected);
  QCOMPARE(outcome->threat, QStringLiteral("Eicar-Signature"));
}

void ClamdScannerTest::errorReplyIsAFailure()
{
  FakeClamd clamd;
  clamd.reply = QByteArrayLiteral("INSTREAM size limit exceeded. ERROR");
  QVERIFY(clamd.listen(socketPath(QStringLiteral("error.sock"))));
  const QString file = writeFile(QStringLiteral("error.bin"), QByteArrayLiteral("large"));

  ClamdScanner scanner;
  const auto outcome = startScan(scanner, file, clamd.server.fullServerName());
  QTRY_COMPARE(outcome->calls, 1);
  QCOMPARE(outcome->result, ScanResult::Failed);
}

void ClamdScannerTest::closedConnectionIsAFailure()
{
  FakeClamd clamd;
  clamd.reply.clear();
  QVERIFY(clamd.listen(socketPath(QStringLiteral("closed.sock"))));
  const QString file = writeFile(QStringLiteral("closed.bin"), QByteArrayLiteral("data"));

  ClamdScanner scanner;
  const auto outcome = startScan(scanner, file, clamd.server.fullServerName());
  QTRY_COMPARE(outcome->calls, 1);
  QCOMPARE(outcome->result, ScanResult::Failed);
}

void ClamdScannerTest::missingSocketIsUnavailable()
{
  const QString file = writeFile(QStringLiteral("missing.bin"), QByteArrayLiteral("data"));

  ClamdScanner scanner;
  const auto outcome = startScan(scanner, file, socketPath(QStringLiteral("nobody.sock")));
  QTRY_COMPARE(outcome->calls, 1);
  QCOMPARE(outcome->result, ScanResult::Unavailable);

  ClamdScanner unconfigured;
  const auto none = startScan(unconfigured, file, QString());
  // Never before scanFile() returns, so that a receiver can open a dialog or delete the scanner
  QCOMPARE(none->calls, 0);
  QTRY_COMPARE(none->calls, 1);
  QCOMPARE(none->result, ScanResult::Unavailable);
}

void ClamdScannerTest::unreadableFileIsAFailure()
{
  FakeClamd clamd;
  QVERIFY(clamd.listen(socketPath(QStringLiteral("unreadable.sock"))));

  ClamdScanner scanner;
  const auto outcome = startScan(scanner, m_dir.filePath(QStringLiteral("does-not-exist.bin")), clamd.server.fullServerName());
  QTRY_COMPARE(outcome->calls, 1);
  QCOMPARE(outcome->result, ScanResult::Failed);
  QVERIFY(clamd.command.isEmpty());
}

void ClamdScannerTest::silentScannerTimesOut()
{
  FakeClamd clamd;
  clamd.silent = true;
  QVERIFY(clamd.listen(socketPath(QStringLiteral("silent.sock"))));
  const QString file = writeFile(QStringLiteral("silent.bin"), QByteArrayLiteral("data"));

  ClamdScanner scanner;
  scanner.setTimeout(200);
  const auto outcome = startScan(scanner, file, clamd.server.fullServerName());
  QTRY_COMPARE(outcome->calls, 1);
  QCOMPARE(outcome->result, ScanResult::Failed);
  QVERIFY(clamd.ended);

  // A scanner destroyed during a scan stays quiet
  auto destroyed = std::make_unique<ClamdScanner>();
  const auto quiet = startScan(*destroyed, file, clamd.server.fullServerName());
  destroyed.reset();
  QTest::qWait(100);
  QCOMPARE(quiet->calls, 0);
}

void ClamdScannerTest::liveClamd()
{
  const QString socket = qEnvironmentVariable("PLASMA_TEST_CLAMD_SOCKET");
  const QByteArray marker = qgetenv("PLASMA_TEST_CLAMD_MARKER");
  if (socket.isEmpty() || marker.isEmpty())
    QSKIP("Needs a clamd; run tools/dev-container/live-clamd.sh");

  struct Case {
    const char *name;
    QByteArray content;
    int chunkSize;
    ScanResult result;
  };
  const QList<Case> cases{
    {"clean", QByteArrayLiteral("An ordinary attachment.\n").repeated(100), 64 * 1024, ScanResult::Clean},
    // The signature spans many small chunks, so clamd has to join the stream again
    {"marker", QByteArray(1000, 'a') + marker + QByteArray(1000, 'b'), 7, ScanResult::Infected},
    // Over the StreamMaxLength of 1 MiB which live-clamd.sh sets
    {"too large", QByteArray(2 * 1024 * 1024, 'x'), 64 * 1024, ScanResult::Failed},
  };
  for (const Case &scan : cases) {
    const QString file = writeFile(QStringLiteral("live-%1.bin").arg(QString::fromLatin1(scan.name).replace(QLatin1Char(' '), QLatin1Char('-'))),
                                   scan.content);
    ClamdScanner scanner;
    scanner.setChunkSize(scan.chunkSize);
    const auto outcome = startScan(scanner, file, socket);
    QTRY_COMPARE_WITH_TIMEOUT(outcome->calls, 1, 30000);
    QVERIFY2(outcome->result == scan.result, scan.name);
    if (scan.result == ScanResult::Infected)
      QVERIFY2(outcome->threat.startsWith(QLatin1String("Plasma.Test.Marker")), qPrintable(outcome->threat));
    else
      QVERIFY(outcome->threat.isEmpty());
  }
}

QTEST_GUILESS_MAIN(ClamdScannerTest)
