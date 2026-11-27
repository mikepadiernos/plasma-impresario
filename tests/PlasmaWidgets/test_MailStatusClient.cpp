/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QDBusConnection>
#include <QSignalSpy>
#include <QTest>
#include <memory>

#include "IPC/MailStatusBridge.h"
#include "PlasmaWidgets/MailStatusClient.h"

using IPC::MailStatusBridge;
using IPC::StatusEntries;
using PlasmaWidgets::MailStatusClient;

namespace {

const QString serviceName = QStringLiteral("io.github.mikepadiernos.PlasmaImpresario.MailStatusTest");

QVariantMap account(const QString &id, int unread)
{
    return {{QStringLiteral("id"), id},
            {QStringLiteral("name"), id.toUpper()},
            {QStringLiteral("unread"), unread},
            {QStringLiteral("state"), QStringLiteral("online")}};
}

/** What the fake application answers, and what it was asked */
struct FakeApplication {
    QString sharing = QStringLiteral("counts");
    StatusEntries accounts{account(QStringLiteral("work"), 3), account(QStringLiteral("home"), 0)};
    StatusEntries messages{{{QStringLiteral("id"), QStringLiteral("message/work/INBOX/9/3")},
                            {QStringLiteral("account"), QStringLiteral("work")},
                            {QStringLiteral("sender"), QStringLiteral("Carol")},
                            {QStringLiteral("subject"), QStringLiteral("Report")}}};
    QStringList events;
};

}

/** @short The widgets' side: following the application on the session bus, honestly about stale data */
class MailStatusClientTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanup();
    void notRunning();
    void followsTheApplication();
    void filtersAndMessages();
    void actions();
    void failedAnswers();
    void olderAnswersDropped();
    void serviceChange();

private:
    void publish();
    MailStatusBridge::Handlers handlers();

    QDBusConnection m_application{QString()};
    FakeApplication m_state;
    std::unique_ptr<MailStatusBridge> m_bridge;
};

void MailStatusClientTest::initTestCase()
{
    if (!QDBusConnection::sessionBus().isConnected())
        QSKIP("No session bus; run the test with dbus-run-session");
    // The fake application has a connection of its own, as the real one is another process
    m_application = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("fake-application"));
    QVERIFY(m_application.isConnected());
    MailStatusBridge::registerTypes();
}

void MailStatusClientTest::cleanup()
{
    m_application.unregisterService(serviceName);
    m_application.unregisterObject(MailStatusBridge::objectPath());
    m_bridge.reset();
    m_state = FakeApplication();
}

MailStatusBridge::Handlers MailStatusClientTest::handlers()
{
    MailStatusBridge::Handlers result;
    FakeApplication *state = &m_state;
    result.sharing = [state]() { return state->sharing; };
    result.accounts = [state]() {
        state->events << QStringLiteral("accounts");
        return state->accounts;
    };
    result.unreadMessages = [state](const QStringList &accounts, const QStringList &folders, int limit) {
        state->events << QStringLiteral("unread %1|%2|%3")
                             .arg(accounts.join(QLatin1Char(',')), folders.join(QLatin1Char(',')), QString::number(limit));
        return state->messages;
    };
    result.openMessage = [state](const QString &id) { state->events << QStringLiteral("open ") + id; };
    result.compose = [state]() { state->events << QStringLiteral("compose"); };
    result.showAccount = [state](const QString &id) { state->events << QStringLiteral("show ") + id; };
    result.handOverToken = [state](const QString &token) { state->events << QStringLiteral("token ") + token; };
    return result;
}

void MailStatusClientTest::publish()
{
    m_bridge = std::make_unique<MailStatusBridge>(handlers());
    QVERIFY(m_application.registerObject(MailStatusBridge::objectPath(), m_bridge.get(),
                                         QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
    QVERIFY(m_application.registerService(serviceName));
}

void MailStatusClientTest::notRunning()
{
    MailStatusClient client(QDBusConnection::sessionBus(), nullptr);
    QCOMPARE(client.service(), QStringLiteral("net.flaska.trojita"));
    client.setService(serviceName);
    QTest::qWait(200);
    QVERIFY(!client.isRunning());
    QVERIFY(client.isStale());
    QVERIFY(!client.isLoaded());
    QCOMPARE(client.totalUnread(), -1);
    QVERIFY(client.sharing().isEmpty());
    // Nothing is sent while the application is away, and nothing breaks
    client.refresh();
    client.openMessage(QStringLiteral("message/work/INBOX/9/3"));
    client.compose();
    QTest::qWait(100);
    QVERIFY(!client.isLoaded());
}

void MailStatusClientTest::followsTheApplication()
{
    MailStatusClient client(QDBusConnection::sessionBus(), nullptr);
    client.setService(serviceName);
    client.setWantsMessages(true);
    QSignalSpy running(&client, &MailStatusClient::runningChanged);

    publish();
    QTRY_VERIFY(client.isRunning());
    QTRY_VERIFY(client.isLoaded());
    QVERIFY(!client.isStale());
    QCOMPARE(client.sharing(), QStringLiteral("counts"));
    QCOMPARE(client.accounts().size(), 2);
    QCOMPARE(client.totalUnread(), 3);
    QVERIFY(client.lastUpdate().isValid());
    // Counts only: the messages are not even asked for
    QVERIFY(client.messages().isEmpty());
    QVERIFY(!m_state.events.join(QLatin1Char(';')).contains(QLatin1String("unread")));

    // The application reports a change, and the widget asks again
    m_state.accounts = {account(QStringLiteral("work"), 5), account(QStringLiteral("home"), 2)};
    m_bridge->notifyChanged();
    QTRY_COMPARE(client.totalUnread(), 7);

    // It quits: the counts stay for reference, but they are stale
    m_application.unregisterService(serviceName);
    QTRY_VERIFY(!client.isRunning());
    QVERIFY(client.isStale());
    QCOMPARE(client.totalUnread(), 7);
    QVERIFY(client.isLoaded());

    // It comes back, and the widget catches up without being asked
    m_state.accounts = {account(QStringLiteral("work"), 1)};
    QVERIFY(m_application.registerService(serviceName));
    QTRY_VERIFY(client.isRunning());
    QTRY_COMPARE(client.totalUnread(), 1);
    QVERIFY(!client.isStale());
    QCOMPARE(running.size(), 3);
}

void MailStatusClientTest::filtersAndMessages()
{
    m_state.sharing = QStringLiteral("subjects");
    publish();
    MailStatusClient client(QDBusConnection::sessionBus(), nullptr);
    client.setService(serviceName);
    QTRY_VERIFY(client.isLoaded());
    QVERIFY(client.messages().isEmpty());

    client.setWantsMessages(true);
    QTRY_COMPARE(client.messages().size(), 1);
    QCOMPARE(client.messages().constFirst().toMap().value(QStringLiteral("subject")).toString(), QStringLiteral("Report"));
    QCOMPARE(m_state.events.last(), QStringLiteral("unread ||20"));

    QSignalSpy loaded(&client, &MailStatusClient::loadedChanged);
    client.setAccountFilter({QStringLiteral("work")});
    // The accounts are chosen at once; the messages are asked for again
    QCOMPARE(client.accounts().size(), 1);
    QCOMPARE(client.allAccounts().size(), 2);
    QCOMPARE(client.totalUnread(), 3);
    client.setFolderFilter({QStringLiteral("INBOX")});
    client.setMessageLimit(500);
    QCOMPARE(client.messageLimit(), 50);
    QTRY_COMPARE(m_state.events.last(), QStringLiteral("unread work|INBOX|50"));
    QTRY_VERIFY(!loaded.isEmpty());

    // An account which is not there counts as nothing known
    client.setAccountFilter({QStringLiteral("gone")});
    QVERIFY(client.accounts().isEmpty());
    QCOMPARE(client.totalUnread(), -1);

    // Sharing less takes the messages away at the next answer
    m_state.sharing = QStringLiteral("counts");
    m_bridge->notifyChanged();
    QTRY_VERIFY(client.messages().isEmpty());
    QCOMPARE(client.sharing(), QStringLiteral("counts"));

    // A widget which does not want messages drops them at once
    m_state.sharing = QStringLiteral("senders");
    m_bridge->notifyChanged();
    QTRY_COMPARE(client.messages().size(), 1);
    client.setWantsMessages(false);
    QVERIFY(client.messages().isEmpty());
}

void MailStatusClientTest::actions()
{
    publish();
    MailStatusClient client(QDBusConnection::sessionBus(), nullptr);
    client.setService(serviceName);
    int asked = 0;
    client.setTokenSource([&asked](const std::function<void(const QString &)> &done) {
        ++asked;
        done(QStringLiteral("token-%1").arg(asked));
    });
    QTRY_VERIFY(client.isLoaded());
    m_state.events.clear();

    client.openMessage(QStringLiteral("message/work/INBOX/9/3"));
    client.openMessage(QString());
    client.compose();
    client.showAccount(QStringLiteral("home"));
    // Started while running only brings the window forward
    client.start();
    QTRY_COMPARE(m_state.events.size(), 8);
    // Each request carries its own token, handed over before the request is acted on
    QCOMPARE(m_state.events,
             (QStringList{QStringLiteral("token token-1"), QStringLiteral("open message/work/INBOX/9/3"),
                          QStringLiteral("token token-2"), QStringLiteral("compose"), QStringLiteral("token token-3"),
                          QStringLiteral("show home"), QStringLiteral("token token-4"), QStringLiteral("show ")}));
    QCOMPARE(asked, 4);
}

void MailStatusClientTest::failedAnswers()
{
    publish();
    MailStatusClient client(QDBusConnection::sessionBus(), nullptr);
    client.setService(serviceName);
    QTRY_VERIFY(client.isLoaded());
    QVERIFY(!client.isStale());

    // An application which owns the name but does not answer for the status, such as an older version
    m_application.unregisterObject(MailStatusBridge::objectPath());
    QSignalSpy failed(&client, &MailStatusClient::requestFailed);
    client.refresh();
    QVERIFY(failed.wait(MailStatusClient::callTimeout + 1000));
    QVERIFY(client.isStale());
    QVERIFY(client.isRunning());
    QCOMPARE(client.accounts().size(), 2);

    QVERIFY(m_application.registerObject(MailStatusBridge::objectPath(), m_bridge.get(),
                                         QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
    client.refresh();
    QTRY_VERIFY(!client.isStale());
}

void MailStatusClientTest::olderAnswersDropped()
{
    publish();
    MailStatusClient client(QDBusConnection::sessionBus(), nullptr);
    client.setService(serviceName);
    QTRY_VERIFY(client.isLoaded());
    QTest::qWait(MailStatusBridge::changeDelay * 2);
    m_state.events.clear();

    // Two refreshes in a row: the answer to the first one arrives after the second was sent and is dropped
    QSignalSpy accounts(&client, &MailStatusClient::accountsChanged);
    client.refresh();
    client.refresh();
    QTRY_COMPARE(accounts.size(), 1);
    QTest::qWait(200);
    QCOMPARE(accounts.size(), 1);
    QCOMPARE(m_state.events, QStringList{QStringLiteral("accounts")});
}

void MailStatusClientTest::serviceChange()
{
    publish();
    MailStatusClient client(QDBusConnection::sessionBus(), nullptr);
    client.setService(serviceName);
    QTRY_VERIFY(client.isLoaded());
    QSignalSpy changed(&client, &MailStatusClient::serviceChanged);

    // Another profile, which is not running: nothing of the first one is kept
    client.setService(serviceName + QStringLiteral(".Other"));
    QCOMPARE(changed.size(), 1);
    QVERIFY(!client.isRunning());
    QVERIFY(client.isStale());
    QVERIFY(client.allAccounts().isEmpty());
    QCOMPARE(client.totalUnread(), -1);

    client.setService(serviceName);
    QTRY_VERIFY(client.isRunning());
    QTRY_COMPARE(client.totalUnread(), 3);
}

QTEST_GUILESS_MAIN(MailStatusClientTest)
#include "test_MailStatusClient.moc"
