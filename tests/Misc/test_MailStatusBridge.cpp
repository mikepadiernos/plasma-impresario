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
#include <memory>

#include "IPC/MailStatusBridge.h"

using IPC::MailStatusBridge;
using IPC::StatusEntries;

namespace {

QVariantMap account(const QString &id, int unread)
{
    return {{QStringLiteral("id"), id},
            {QStringLiteral("name"), id.toUpper()},
            {QStringLiteral("unread"), unread},
            {QStringLiteral("state"), QStringLiteral("online")}};
}

}

/** @short The interface through which the Plasma widgets follow the mail */
class MailStatusBridgeTest : public QObject
{
    Q_OBJECT
private slots:
    void names();
    void directCalls();
    void withoutHandlers();
    void changesCoalesced();
    void peerToPeer();

private:
    MailStatusBridge::Handlers handlers(QStringList *events);
};

MailStatusBridge::Handlers MailStatusBridgeTest::handlers(QStringList *events)
{
    MailStatusBridge::Handlers result;
    result.sharing = []() { return QStringLiteral("subjects"); };
    result.accounts = []() { return StatusEntries{account(QStringLiteral("work"), 3), account(QStringLiteral("home"), 0)}; };
    result.unreadMessages = [events](const QStringList &accounts, const QStringList &folders, int limit) {
        *events << QStringLiteral("unread %1 %2 %3")
                       .arg(accounts.join(QLatin1Char(',')), folders.join(QLatin1Char(',')), QString::number(limit));
        return StatusEntries{{{QStringLiteral("id"), QStringLiteral("message/work/INBOX/9/3")},
                              {QStringLiteral("subject"), QStringLiteral("Report")}}};
    };
    result.openMessage = [events](const QString &id) { *events << QStringLiteral("open ") + id; };
    result.compose = [events]() { *events << QStringLiteral("compose"); };
    result.showAccount = [events](const QString &id) { *events << QStringLiteral("show ") + id; };
    result.handOverToken = [events](const QString &token) { *events << QStringLiteral("token ") + token; };
    return result;
}

void MailStatusBridgeTest::names()
{
    MailStatusBridge::registerTypes();
    QCOMPARE(QString::fromLatin1(QDBusMetaType::typeToSignature(QMetaType::fromType<StatusEntries>())),
             QStringLiteral("aa{sv}"));
    QCOMPARE(MailStatusBridge::objectPath(), QStringLiteral("/MailStatus"));
    QCOMPARE(MailStatusBridge::interfaceName(), QStringLiteral("io.github.mikepadiernos.PlasmaImpresario.MailStatus1"));
    // The class info is what QtDBus exports; it has to agree with the name the widgets ask for
    const QMetaObject &meta = MailStatusBridge::staticMetaObject;
    QCOMPARE(QString::fromLatin1(meta.classInfo(meta.indexOfClassInfo("D-Bus Interface")).value()),
             MailStatusBridge::interfaceName());
}

void MailStatusBridgeTest::directCalls()
{
    QStringList events;
    MailStatusBridge bridge(handlers(&events));
    QCOMPARE(bridge.Version(), MailStatusBridge::interfaceVersion);
    QCOMPARE(bridge.Sharing(), QStringLiteral("subjects"));
    QCOMPARE(bridge.Accounts().size(), 2);

    QCOMPARE(bridge.UnreadMessages({QStringLiteral("work")}, {QStringLiteral("INBOX")}, 5).size(), 1);
    // A limit of nothing asks for nothing
    QVERIFY(bridge.UnreadMessages({}, {}, 0).isEmpty());
    QVERIFY(bridge.UnreadMessages({}, {}, -3).isEmpty());
    QCOMPARE(events, QStringList{QStringLiteral("unread work INBOX 5")});
    events.clear();

    // The token comes first, so that the window it brings forward may take the focus
    bridge.OpenMessage(QStringLiteral("message/work/INBOX/9/3"), QStringLiteral("t1"));
    bridge.OpenMessage(QString(), QStringLiteral("ignored"));
    bridge.Compose(QString());
    bridge.ShowAccount(QStringLiteral("home"), QStringLiteral("t2"));
    QCOMPARE(events, (QStringList{QStringLiteral("token t1"), QStringLiteral("open message/work/INBOX/9/3"),
                                  QStringLiteral("compose"), QStringLiteral("token t2"), QStringLiteral("show home")}));
}

void MailStatusBridgeTest::withoutHandlers()
{
    MailStatusBridge bridge({});
    QCOMPARE(bridge.Sharing(), QStringLiteral("nothing"));
    QVERIFY(bridge.Accounts().isEmpty());
    QVERIFY(bridge.UnreadMessages({}, {}, 10).isEmpty());
    bridge.OpenMessage(QStringLiteral("message/work/INBOX/9/3"), QStringLiteral("token"));
    bridge.Compose(QStringLiteral("token"));
    bridge.ShowAccount(QString(), QStringLiteral("token"));
}

void MailStatusBridgeTest::changesCoalesced()
{
    MailStatusBridge bridge({});
    QSignalSpy changed(&bridge, &MailStatusBridge::Changed);
    for (int i = 0; i < 20; ++i)
        bridge.notifyChanged();
    QVERIFY(changed.isEmpty());
    QVERIFY(changed.wait(MailStatusBridge::changeDelay * 8));
    QCOMPARE(changed.size(), 1);
    QTest::qWait(MailStatusBridge::changeDelay * 2);
    QCOMPARE(changed.size(), 1);
    bridge.notifyChanged();
    QVERIFY(changed.wait(MailStatusBridge::changeDelay * 8));
    QCOMPARE(changed.size(), 2);
}

void MailStatusBridgeTest::peerToPeer()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QDBusServer server(QStringLiteral("unix:tmpdir=") + dir.path());
    if (!server.isConnected())
        QSKIP("No peer-to-peer D-Bus server could be started");
    server.setAnonymousAuthenticationAllowed(true);

    QStringList events;
    MailStatusBridge bridge(handlers(&events));
    MailStatusBridge::registerTypes();
    std::unique_ptr<QDBusConnection> serverSide;
    connect(&server, &QDBusServer::newConnection, this, [&serverSide, &bridge](const QDBusConnection &connection) {
        serverSide = std::make_unique<QDBusConnection>(connection);
        QVERIFY(serverSide->registerObject(MailStatusBridge::objectPath(), &bridge,
                                           QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
    });

    {
        QDBusConnection client = QDBusConnection::connectToPeer(server.address(), QStringLiteral("mail-status-test"));
        QVERIFY2(client.isConnected(), qPrintable(client.lastError().message()));
        QTRY_VERIFY(serverSide);

        const auto call = [&client](const QString &method, const QVariantList &arguments = {}) {
            QDBusMessage message = QDBusMessage::createMethodCall(QString(), MailStatusBridge::objectPath(),
                                                                  MailStatusBridge::interfaceName(), method);
            message.setArguments(arguments);
            QDBusPendingCallWatcher watcher(client.asyncCall(message));
            QSignalSpy finished(&watcher, &QDBusPendingCallWatcher::finished);
            if (!watcher.isFinished())
                finished.wait(5000);
            return QDBusPendingCall(watcher);
        };

        QDBusPendingReply<int> version = call(QStringLiteral("Version"));
        QVERIFY2(!version.isError(), qPrintable(version.error().message()));
        QCOMPARE(version.value(), 1);

        QDBusPendingReply<QString> sharing = call(QStringLiteral("Sharing"));
        QVERIFY2(!sharing.isError(), qPrintable(sharing.error().message()));
        QCOMPARE(sharing.value(), QStringLiteral("subjects"));

        QDBusPendingReply<StatusEntries> accounts = call(QStringLiteral("Accounts"));
        QVERIFY2(!accounts.isError(), qPrintable(accounts.error().message()));
        QCOMPARE(accounts.reply().signature(), QStringLiteral("aa{sv}"));
        QCOMPARE(accounts.value().size(), 2);
        QCOMPARE(accounts.value().constFirst().value(QStringLiteral("unread")).toInt(), 3);

        QDBusPendingReply<StatusEntries> unread = call(
            QStringLiteral("UnreadMessages"), {QStringList{QStringLiteral("work")}, QStringList(), 20});
        QVERIFY2(!unread.isError(), qPrintable(unread.error().message()));
        QCOMPARE(unread.reply().signature(), QStringLiteral("aa{sv}"));
        QCOMPARE(unread.value().constFirst().value(QStringLiteral("subject")).toString(), QStringLiteral("Report"));

        QDBusPendingReply<> open = call(QStringLiteral("OpenMessage"),
                                        {QStringLiteral("message/work/INBOX/9/3"), QStringLiteral("kwin-token")});
        QVERIFY2(!open.isError(), qPrintable(open.error().message()));
        QDBusPendingReply<> compose = call(QStringLiteral("Compose"), {QString()});
        QVERIFY2(!compose.isError(), qPrintable(compose.error().message()));
        QCOMPARE(events, (QStringList{QStringLiteral("unread work  20"), QStringLiteral("token kwin-token"),
                                      QStringLiteral("open message/work/INBOX/9/3"), QStringLiteral("compose")}));
    }
    QDBusConnection::disconnectFromPeer(QStringLiteral("mail-status-test"));
    if (serverSide)
        QDBusConnection::disconnectFromPeer(serverSide->name());
}

QTEST_GUILESS_MAIN(MailStatusBridgeTest)
#include "test_MailStatusBridge.moc"
