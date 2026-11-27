/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "IPC/KRunnerBridge.h"

using IPC::KRunnerBridge;
using IPC::RunnerMatch;
using IPC::RunnerMatches;

/** @short The org.kde.krunner1 interface through which Plasma Search asks for matches */
class KRunnerBridgeTest : public QObject
{
    Q_OBJECT
private slots:
    void signatures();
    void directCalls();
    void peerToPeer();

private:
    static RunnerMatches search(const QString &query);
};

RunnerMatches KRunnerBridgeTest::search(const QString &query)
{
    if (query != QLatin1String("release"))
        return {};
    RunnerMatch match;
    match.id = QStringLiteral("message/work/INBOX/22/7");
    match.text = QStringLiteral("Release notes");
    match.iconName = QStringLiteral("mail-message");
    match.categoryRelevance = KRunnerBridge::categoryRelevanceLow;
    match.relevance = 0.5;
    match.properties.insert(QStringLiteral("subtext"), QStringLiteral("Carol, 2026-04-01"));
    return {match};
}

void KRunnerBridgeTest::signatures()
{
    KRunnerBridge::registerTypes();
    // As KRunner's DBusRunner reads them
    QCOMPARE(QString::fromLatin1(QDBusMetaType::typeToSignature(QMetaType::fromType<RunnerMatch>())),
             QStringLiteral("(sssida{sv})"));
    QCOMPARE(QString::fromLatin1(QDBusMetaType::typeToSignature(QMetaType::fromType<RunnerMatches>())),
             QStringLiteral("a(sssida{sv})"));
    QCOMPARE(QString::fromLatin1(QDBusMetaType::typeToSignature(QMetaType::fromType<IPC::RunnerAction>())),
             QStringLiteral("(sss)"));
    QCOMPARE(QString::fromLatin1(QDBusMetaType::typeToSignature(QMetaType::fromType<IPC::RunnerActions>())),
             QStringLiteral("a(sss)"));
    QCOMPARE(KRunnerBridge::objectPath(), QStringLiteral("/runner"));
    QCOMPARE(KRunnerBridge::categoryRelevanceLow, 30);
}

void KRunnerBridgeTest::directCalls()
{
    QStringList opened;
    KRunnerBridge bridge(&KRunnerBridgeTest::search, [&opened](const QString &id) { opened << id; });
    QVERIFY(bridge.Actions().isEmpty());
    QVERIFY(bridge.Match(QStringLiteral("lunch")).isEmpty());
    const RunnerMatches found = bridge.Match(QStringLiteral("release"));
    QCOMPARE(found.size(), 1);
    QCOMPARE(found.first().text, QStringLiteral("Release notes"));

    // Only the default action of a real match opens it
    bridge.Run(QString(), QString());
    bridge.Run(found.first().id, QStringLiteral("reply"));
    QVERIFY(opened.isEmpty());
    bridge.Run(found.first().id, QString());
    QCOMPARE(opened, QStringList{found.first().id});
    bridge.Teardown();

    KRunnerBridge empty(nullptr, nullptr);
    QVERIFY(empty.Match(QStringLiteral("release")).isEmpty());
    empty.Run(found.first().id, QString());
}

void KRunnerBridgeTest::peerToPeer()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QDBusServer server(QStringLiteral("unix:tmpdir=") + dir.path());
    if (!server.isConnected())
        QSKIP("No peer-to-peer D-Bus server could be started");
    server.setAnonymousAuthenticationAllowed(true);

    QStringList opened;
    KRunnerBridge bridge(&KRunnerBridgeTest::search, [&opened](const QString &id) { opened << id; });
    std::unique_ptr<QDBusConnection> serverSide;
    connect(&server, &QDBusServer::newConnection, this, [&serverSide, &bridge](const QDBusConnection &connection) {
        serverSide = std::make_unique<QDBusConnection>(connection);
        QVERIFY(serverSide->registerObject(KRunnerBridge::objectPath(), &bridge, QDBusConnection::ExportAllSlots));
    });

    {
        const QDBusConnection client = QDBusConnection::connectToPeer(server.address(), QStringLiteral("krunner-test"));
        QVERIFY2(client.isConnected(), qPrintable(client.lastError().message()));
        QTRY_VERIFY(serverSide);

        // The bridge answers on this thread, so replies are awaited with the event loop running
        const auto call = [&client](const QString &method, const QVariantList &arguments = {}) {
            QDBusMessage message = QDBusMessage::createMethodCall(QString(), KRunnerBridge::objectPath(),
                                                                  QStringLiteral("org.kde.krunner1"), method);
            message.setArguments(arguments);
            QDBusPendingCallWatcher watcher(client.asyncCall(message));
            QSignalSpy finished(&watcher, &QDBusPendingCallWatcher::finished);
            if (!watcher.isFinished())
                finished.wait(5000);
            return QDBusPendingCall(watcher);
        };

        QDBusPendingReply<RunnerMatches> matches = call(QStringLiteral("Match"), {QStringLiteral("release")});
        QVERIFY(matches.isFinished());
        QVERIFY2(!matches.isError(), qPrintable(matches.error().message()));
        QCOMPARE(matches.reply().signature(), QStringLiteral("a(sssida{sv})"));
        const RunnerMatches found = matches.value();
        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first().id, QStringLiteral("message/work/INBOX/22/7"));
        QCOMPARE(found.first().iconName, QStringLiteral("mail-message"));
        QCOMPARE(found.first().categoryRelevance, KRunnerBridge::categoryRelevanceLow);
        QCOMPARE(found.first().relevance, 0.5);
        QCOMPARE(found.first().properties.value(QStringLiteral("subtext")).toString(), QStringLiteral("Carol, 2026-04-01"));

        QDBusPendingReply<RunnerMatches> none = call(QStringLiteral("Match"), {QStringLiteral("lunch")});
        QVERIFY(none.isFinished());
        QVERIFY2(!none.isError(), qPrintable(none.error().message()));
        QVERIFY(none.value().isEmpty());

        QDBusPendingReply<IPC::RunnerActions> actions = call(QStringLiteral("Actions"));
        QVERIFY(actions.isFinished());
        QVERIFY2(!actions.isError(), qPrintable(actions.error().message()));
        QCOMPARE(actions.reply().signature(), QStringLiteral("a(sss)"));
        QVERIFY(actions.value().isEmpty());

        QDBusPendingReply<> run = call(QStringLiteral("Run"), {found.first().id, QString()});
        QVERIFY(run.isFinished());
        QVERIFY2(!run.isError(), qPrintable(run.error().message()));
        QCOMPARE(opened, QStringList{found.first().id});

        QDBusPendingReply<> teardown = call(QStringLiteral("Teardown"));
        QVERIFY(teardown.isFinished());
        QVERIFY2(!teardown.isError(), qPrintable(teardown.error().message()));
    }
    QDBusConnection::disconnectFromPeer(QStringLiteral("krunner-test"));
    if (serverSide)
        QDBusConnection::disconnectFromPeer(serverSide->name());
}

QTEST_GUILESS_MAIN(KRunnerBridgeTest)
#include "test_KRunnerBridge.moc"
