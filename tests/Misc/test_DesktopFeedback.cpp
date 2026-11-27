/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "test_DesktopFeedback.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusServer>
#include <QStandardPaths>
#include <QTest>

#include <KConfig>

#include <limits>
#include <memory>

#include "UiUtils/DesktopFeedback.h"

namespace DesktopFeedback = UiUtils::DesktopFeedback;

namespace
{

const QString desktopFileName = QStringLiteral("io.github.mikepadiernos.plasma_impresario");

/** Receives the task manager signal, as Plasma's task manager does */
class Listener : public QObject
{
    Q_OBJECT

public:
    QString uri;
    QVariantMap properties;
    int calls = 0;

public slots:
    void update(const QString &appUri, const QVariantMap &props)
    {
        uri = appUri;
        properties = props;
        ++calls;
    }
};

}

void DesktopFeedbackTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(m_dir.isValid());
}

void DesktopFeedbackTest::shownByDefault()
{
    const QString path = m_dir.filePath(QStringLiteral("feedbackrc"));
    {
        KConfig config(path, KConfig::SimpleConfig);
        QVERIFY(DesktopFeedback::showsUnreadCount(config));
        DesktopFeedback::setShowsUnreadCount(config, false);
        QVERIFY(!DesktopFeedback::showsUnreadCount(config));
    }
    KConfig reread(path, KConfig::SimpleConfig);
    QVERIFY(!DesktopFeedback::showsUnreadCount(reread));
    DesktopFeedback::setShowsUnreadCount(reread, true);
    QVERIFY(DesktopFeedback::showsUnreadCount(reread));
}

void DesktopFeedbackTest::totalSkipsUnknownCounts()
{
    QCOMPARE(DesktopFeedback::totalUnread({}), 0);
    QCOMPARE(DesktopFeedback::totalUnread({-1, -1}), 0);
    QCOMPARE(DesktopFeedback::totalUnread({3, -1, 0, 4}), 7);
    // A huge count stops at the largest number instead of wrapping around
    QCOMPARE(DesktopFeedback::totalUnread({std::numeric_limits<int>::max(), 5}), std::numeric_limits<int>::max());
}

void DesktopFeedbackTest::propertiesHideNothingUnread()
{
    QVariantMap properties = DesktopFeedback::launcherEntryProperties(12);
    QCOMPARE(properties.value(QStringLiteral("count")).metaType(), QMetaType::fromType<qint64>());
    QCOMPARE(properties.value(QStringLiteral("count")).toLongLong(), 12);
    QCOMPARE(properties.value(QStringLiteral("count-visible")).toBool(), true);
    // Only the number leaves the application
    QCOMPARE(properties.size(), 2);

    for (const int unread : {0, -1}) {
        properties = DesktopFeedback::launcherEntryProperties(unread);
        QCOMPARE(properties.value(QStringLiteral("count")).toLongLong(), 0);
        QCOMPARE(properties.value(QStringLiteral("count-visible")).toBool(), false);
    }
}

void DesktopFeedbackTest::updateNamesTheDesktopEntry()
{
    QCOMPARE(DesktopFeedback::launcherEntryUri(desktopFileName),
             QStringLiteral("application://io.github.mikepadiernos.plasma_impresario.desktop"));
    const QDBusMessage message = DesktopFeedback::launcherEntryUpdate(desktopFileName, 5);
    QCOMPARE(message.type(), QDBusMessage::SignalMessage);
    QCOMPARE(message.interface(), QStringLiteral("com.canonical.Unity.LauncherEntry"));
    QCOMPARE(message.member(), QStringLiteral("Update"));
    QVERIFY(message.path().startsWith(QLatin1String("/io/github/mikepadiernos/plasma_impresario/")));
    QCOMPARE(message.arguments().size(), 2);
    QCOMPARE(message.arguments().at(0).toString(), DesktopFeedback::launcherEntryUri(desktopFileName));
    QCOMPARE(message.arguments().at(1).toMap(), DesktopFeedback::launcherEntryProperties(5));
}

void DesktopFeedbackTest::badgeSendsOnlyChanges()
{
    DesktopFeedback::UnreadBadge badge;
    QCOMPARE(badge.shown(), -1);
    // The first count is always sent, clearing whatever an earlier run left behind
    QVERIFY(badge.update(0));
    QCOMPARE(badge.shown(), 0);
    QVERIFY(!badge.update(0));
    QVERIFY(!badge.update(-1));
    QVERIFY(badge.update(4));
    QVERIFY(!badge.update(4));
    QVERIFY(badge.update(3));
    QCOMPARE(badge.shown(), 3);
    badge.reset();
    QCOMPARE(badge.shown(), -1);
    QVERIFY(badge.update(3));
}

void DesktopFeedbackTest::signalReachesAListener()
{
    QDBusServer server(QStringLiteral("unix:tmpdir=") + m_dir.path());
    QVERIFY(server.isConnected());
    server.setAnonymousAuthenticationAllowed(true);
    std::unique_ptr<QDBusConnection> serverSide;
    Listener listener;
    connect(&server, &QDBusServer::newConnection, this, [&serverSide, &listener](const QDBusConnection &connection) {
        serverSide = std::make_unique<QDBusConnection>(connection);
        serverSide->connect(QString(), QString(), QStringLiteral("com.canonical.Unity.LauncherEntry"),
                            QStringLiteral("Update"), &listener, SLOT(update(QString, QVariantMap)));
    });
    const QString name = QStringLiteral("desktop-feedback-test");
    {
        QDBusConnection client = QDBusConnection::connectToPeer(server.address(), name);
        QVERIFY(client.isConnected());
        QTRY_VERIFY(serverSide);
        QVERIFY(client.send(DesktopFeedback::launcherEntryUpdate(desktopFileName, 9)));
        QTRY_COMPARE(listener.calls, 1);
    }
    QDBusConnection::disconnectFromPeer(name);
    QCOMPARE(listener.uri, DesktopFeedback::launcherEntryUri(desktopFileName));
    QCOMPARE(listener.properties.value(QStringLiteral("count")).toLongLong(), 9);
    QCOMPARE(listener.properties.value(QStringLiteral("count-visible")).toBool(), true);
}

QTEST_GUILESS_MAIN(DesktopFeedbackTest)

#include "test_DesktopFeedback.moc"
