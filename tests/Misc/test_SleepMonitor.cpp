/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusServer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "UiUtils/SleepMonitor.h"

using UiUtils::SleepMonitor;

class SleepMonitorTest : public QObject
{
    Q_OBJECT

private slots:
  void initTestCase()
  {
    QVERIFY(m_dir.isValid());
  }

  void followsPrepareForSleep()
  {
    QDBusServer server(QStringLiteral("unix:tmpdir=") + m_dir.path());
    if (!server.isConnected())
      QSKIP("No peer-to-peer D-Bus server could be started");
    server.setAnonymousAuthenticationAllowed(true);
    std::unique_ptr<QDBusConnection> serverSide;
    connect(&server, &QDBusServer::newConnection, this,
            [&serverSide](const QDBusConnection &connection) { serverSide = std::make_unique<QDBusConnection>(connection); });
    const QString name = QStringLiteral("sleep-monitor-test");
    {
      SleepMonitor monitor;
      QSignalSpy changes(&monitor, &SleepMonitor::sleepingChanged);
      QDBusConnection client = QDBusConnection::connectToPeer(server.address(), name);
      QVERIFY(client.isConnected());
      // A peer connection has no bus names, so the sender is not checked
      QVERIFY(monitor.start(client, QString()));
      QTRY_VERIFY(serverSide);
      QVERIFY(!monitor.isSleeping());

      QVERIFY(serverSide->send(prepareForSleep(true)));
      QTRY_COMPARE(changes.size(), 1);
      QVERIFY(monitor.isSleeping());
      QCOMPARE(changes.at(0).at(0).toBool(), true);

      // logind sends each state once, but a repeated one must not be reported twice
      QVERIFY(serverSide->send(prepareForSleep(true)));
      QVERIFY(serverSide->send(prepareForSleep(false)));
      QTRY_COMPARE(changes.size(), 2);
      QVERIFY(!monitor.isSleeping());
      QCOMPARE(changes.at(1).at(0).toBool(), false);

      // Other signals of the interface are ignored
      QDBusMessage other = QDBusMessage::createSignal(QStringLiteral("/org/freedesktop/login1"),
                                                      QStringLiteral("org.freedesktop.login1.Manager"),
                                                      QStringLiteral("PrepareForShutdown"));
      other << true;
      QVERIFY(serverSide->send(other));
      QVERIFY(serverSide->send(prepareForSleep(true)));
      QTRY_COMPARE(changes.size(), 3);
      QVERIFY(monitor.isSleeping());
    }
    QDBusConnection::disconnectFromPeer(name);
  }

  void withoutABusNothingIsReported()
  {
    SleepMonitor monitor;
    QVERIFY(!monitor.start(QDBusConnection(QStringLiteral("not-connected"))));
    QVERIFY(!monitor.isSleeping());
    QCOMPARE(SleepMonitor::loginService(), QStringLiteral("org.freedesktop.login1"));
  }

private:
  static QDBusMessage prepareForSleep(bool sleeping)
  {
    QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/org/freedesktop/login1"),
                                                      QStringLiteral("org.freedesktop.login1.Manager"),
                                                      QStringLiteral("PrepareForSleep"));
    message << sleeping;
    return message;
  }

  QTemporaryDir m_dir;
};

QTEST_GUILESS_MAIN(SleepMonitorTest)

#include "test_SleepMonitor.moc"
