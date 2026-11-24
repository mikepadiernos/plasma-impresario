/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtTest>
#include "test_AccountCapabilities.h"
#include "Imap/Model/AccountCapabilities.h"

using namespace Imap;

Q_DECLARE_METATYPE(AccountOperation)
Q_DECLARE_METATYPE(AccountProtocol)

static void addOperations()
{
    QTest::addColumn<AccountOperation>("operation");
    QTest::newRow("folders") << AccountOperation::ServerFolders;
    QTest::newRow("flags") << AccountOperation::ServerSideFlags;
    QTest::newRow("search") << AccountOperation::ServerSideSearch;
    QTest::newRow("move") << AccountOperation::ServerSideMove;
    QTest::newRow("drafts") << AccountOperation::SaveDraftsOnServer;
}

void AccountCapabilitiesTest::imapSupportsEveryOperation_data()
{
    addOperations();
    QTest::newRow("leave on server") << AccountOperation::LeaveMessagesOnServer;
    QTest::newRow("push") << AccountOperation::PushNotifications;
}

void AccountCapabilitiesTest::imapSupportsEveryOperation()
{
    QFETCH(AccountOperation, operation);
    QVERIFY(AccountCapabilities(AccountProtocol::Imap).supports(operation));
}

void AccountCapabilitiesTest::pop3LacksServerSideOperations_data()
{
    addOperations();
    QTest::newRow("push") << AccountOperation::PushNotifications;
}

void AccountCapabilitiesTest::pop3LacksServerSideOperations()
{
    QFETCH(AccountOperation, operation);
    QVERIFY(!AccountCapabilities(AccountProtocol::Pop3).supports(operation));
}

void AccountCapabilitiesTest::pop3KeepsMessagesOnlyByOption()
{
    QVERIFY(AccountCapabilities(AccountProtocol::Pop3).supports(AccountOperation::LeaveMessagesOnServer));
}

void AccountCapabilitiesTest::jmapMatchesImapExceptPush()
{
    const AccountCapabilities jmap(AccountProtocol::Jmap);
    QVERIFY(jmap.supports(AccountOperation::ServerFolders));
    QVERIFY(jmap.supports(AccountOperation::ServerSideMove));
    QVERIFY(jmap.supports(AccountOperation::PushNotifications));
}

void AccountCapabilitiesTest::limitationIsEmptyOnlyWhenSupported_data()
{
    QTest::addColumn<AccountProtocol>("protocol");
    QTest::addColumn<AccountOperation>("operation");
    for (auto protocol : {AccountProtocol::Imap, AccountProtocol::Pop3, AccountProtocol::Jmap}) {
        for (auto operation : {AccountOperation::ServerFolders, AccountOperation::ServerSideFlags,
                               AccountOperation::ServerSideSearch, AccountOperation::ServerSideMove,
                               AccountOperation::SaveDraftsOnServer, AccountOperation::LeaveMessagesOnServer,
                               AccountOperation::PushNotifications}) {
            QTest::addRow("%d-%d", static_cast<int>(protocol), static_cast<int>(operation)) << protocol << operation;
        }
    }
}

void AccountCapabilitiesTest::limitationIsEmptyOnlyWhenSupported()
{
    QFETCH(AccountProtocol, protocol);
    QFETCH(AccountOperation, operation);
    const AccountCapabilities capabilities(protocol);
    QCOMPARE(capabilities.limitation(operation).isEmpty(), capabilities.supports(operation));
}

void AccountCapabilitiesTest::parsesStoredProtocolNames_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<AccountProtocol>("protocol");
    QTest::newRow("imap") << "imap" << true << AccountProtocol::Imap;
    QTest::newRow("upper case pop3") << "POP3" << true << AccountProtocol::Pop3;
    QTest::newRow("jmap") << "jmap" << true << AccountProtocol::Jmap;
    QTest::newRow("unknown") << "exchange" << false << AccountProtocol::Imap;
    QTest::newRow("empty") << "" << false << AccountProtocol::Imap;
}

void AccountCapabilitiesTest::parsesStoredProtocolNames()
{
    QFETCH(QString, name);
    QFETCH(bool, valid);
    QFETCH(AccountProtocol, protocol);
    bool ok = !valid;
    const AccountProtocol parsed = AccountCapabilities::protocolFromName(name, &ok);
    QCOMPARE(ok, valid);
    if (valid)
        QCOMPARE(parsed, protocol);
}

QTEST_GUILESS_MAIN(AccountCapabilitiesTest)
