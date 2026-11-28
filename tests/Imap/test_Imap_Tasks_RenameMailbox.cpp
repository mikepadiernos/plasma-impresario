/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtTest>
#include "test_Imap_Tasks_RenameMailbox.h"
#include "Common/MetaTypes.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/Model.h"

void ImapModelRenameMailboxTest::init()
{
    fakeListChildMailboxesMap[QLatin1String("")] = QStringList() << QStringLiteral("a") << QStringLiteral("b");
    fakeListChildMailboxesMap[QStringLiteral("b")] = QStringList() << QStringLiteral("old");
    LibMailboxSync::init();
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_ONLINE);
    renamedSpy = new QSignalSpy(model, &Imap::Mailbox::Model::mailboxRenameSucceeded);
    failedSpy = new QSignalSpy(model, &Imap::Mailbox::Model::mailboxRenameFailed);
    t.reset();
}

void ImapModelRenameMailboxTest::cleanup()
{
    delete renamedSpy;
    renamedSpy = nullptr;
    delete failedSpy;
    failedSpy = nullptr;
    LibMailboxSync::cleanup();
}

void ImapModelRenameMailboxTest::initTestCase()
{
    renamedSpy = nullptr;
    failedSpy = nullptr;
    LibMailboxSync::initTestCase();
}

void ImapModelRenameMailboxTest::initWithMailboxes()
{
    model->rowCount(QModelIndex());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(topLevelNames(), QStringList() << QStringLiteral("a") << QStringLiteral("b"));
    cEmpty();
}

QStringList ImapModelRenameMailboxTest::topLevelNames() const
{
    QStringList names;
    // Row 0 is the message list of the root item
    for (int row = 1; row < model->rowCount(QModelIndex()); ++row)
        names << model->index(row, 0, QModelIndex()).data(Imap::Mailbox::RoleMailboxName).toString();
    return names;
}

void ImapModelRenameMailboxTest::testRename()
{
    initWithMailboxes();

    model->renameMailbox(QStringLiteral("a"), QStringLiteral("c"));
    cClient(t.mk("RENAME a c\r\n"));
    cServer(t.last("OK renamed\r\n"));
    // The old entry is gone as soon as the server confirms the rename
    QCOMPARE(topLevelNames(), QStringList() << QStringLiteral("b"));
    cClient(t.mk("LIST \"\" c\r\n"));
    cServer("* LIST (\\HasNoChildren) \"^\" \"c\"\r\n" + t.last("OK list\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(topLevelNames(), QStringList() << QStringLiteral("b") << QStringLiteral("c"));
    cEmpty();
    QCOMPARE(renamedSpy->size(), 1);
    QCOMPARE(renamedSpy->at(0).at(0).toString(), QStringLiteral("a"));
    QCOMPARE(renamedSpy->at(0).at(1).toString(), QStringLiteral("c"));
    QVERIFY(failedSpy->isEmpty());
}

void ImapModelRenameMailboxTest::testRenameNested()
{
    initWithMailboxes();
    const QModelIndex b = model->index(2, 0, QModelIndex());
    QCOMPARE(b.data(Imap::Mailbox::RoleMailboxName).toString(), QStringLiteral("b"));
    model->rowCount(b);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // The message list plus the "old" child
    QCOMPARE(model->rowCount(b), 2);
    QCOMPARE(model->index(1, 0, b).data(Imap::Mailbox::RoleMailboxName).toString(), QStringLiteral("b^old"));

    model->renameMailbox(QStringLiteral("b^old"), QStringLiteral("b^new"));
    cClient(t.mk("RENAME \"b^old\" \"b^new\"\r\n"));
    cServer(t.last("OK renamed\r\n"));
    QCOMPARE(model->rowCount(b), 1);
    cClient(t.mk("LIST \"\" \"b^new\"\r\n"));
    cServer("* LIST (\\HasNoChildren) \"^\" \"b^new\"\r\n" + t.last("OK list\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // The renamed folder stays below its parent
    QCOMPARE(model->rowCount(b), 2);
    QCOMPARE(model->index(1, 0, b).data(Imap::Mailbox::RoleMailboxName).toString(), QStringLiteral("b^new"));
    QCOMPARE(topLevelNames(), QStringList() << QStringLiteral("a") << QStringLiteral("b"));
    cEmpty();
    QCOMPARE(renamedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
}

void ImapModelRenameMailboxTest::testRenameFail()
{
    initWithMailboxes();

    model->renameMailbox(QStringLiteral("a"), QStringLiteral("b"));
    cClient(t.mk("RENAME a b\r\n"));
    cServer(t.last("NO [ALREADYEXISTS] Mailbox exists\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(topLevelNames(), QStringList() << QStringLiteral("a") << QStringLiteral("b"));
    cEmpty();
    QVERIFY(renamedSpy->isEmpty());
    QCOMPARE(failedSpy->size(), 1);
    QCOMPARE(failedSpy->at(0).at(0).toString(), QStringLiteral("a"));
    QCOMPARE(failedSpy->at(0).at(1).toString(), QStringLiteral("b"));
    QCOMPARE(failedSpy->at(0).at(2).toString(), QStringLiteral("Mailbox exists"));
}

void ImapModelRenameMailboxTest::testRenameListFails()
{
    initWithMailboxes();

    model->renameMailbox(QStringLiteral("a"), QStringLiteral("c"));
    cClient(t.mk("RENAME a c\r\n"));
    cServer(t.last("OK renamed\r\n"));
    cClient(t.mk("LIST \"\" c\r\n"));
    cServer(t.last("NO list failed\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // The rename itself succeeded, so it is reported as such; the new name shows up on the next refresh
    QCOMPARE(topLevelNames(), QStringList() << QStringLiteral("b"));
    cEmpty();
    QCOMPARE(renamedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
}

void ImapModelRenameMailboxTest::testRenameOffline()
{
    initWithMailboxes();
    LibMailboxSync::setModelNetworkPolicy(model, Imap::Mailbox::NETWORK_OFFLINE);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    // Going offline closes the connection; ignore whatever the client says on its way out
    SOCK->writtenStuff();

    model->renameMailbox(QStringLiteral("a"), QStringLiteral("c"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(renamedSpy->isEmpty());
    QCOMPARE(failedSpy->size(), 1);
    QCOMPARE(failedSpy->at(0).at(0).toString(), QStringLiteral("a"));
    QCOMPARE(topLevelNames(), QStringList() << QStringLiteral("a") << QStringLiteral("b"));
}

/** @short The RENAME of the open folder waits until the command running on it is answered

Dovecot ends the session when the selected mailbox is renamed while another command on it is still running.
*/
void ImapModelRenameMailboxTest::testRenameWaitsForRunningCommand()
{
    initWithMailboxes();
    // This test lists its own mailboxes, so the fixture's indexes of "a" are set here
    idxA = model->index(1, 0, QModelIndex());
    msgListA = model->index(0, 0, idxA);
    initialMessages(3);
    QCOMPARE(model->index(0, 0, msgListA).data(Imap::Mailbox::RoleMessageSubject), QVariant());
    cClient(t.mk("UID FETCH 1:3 (" FETCH_METADATA_ITEMS ")\r\n"));
    const QByteArray fetchTag = t.last();

    model->renameMailbox(QStringLiteral("a"), QStringLiteral("c"));
    cEmpty();
    cServer(helperCreateTrivialEnvelope(1, 1, QStringLiteral("one"))
        + helperCreateTrivialEnvelope(2, 2, QStringLiteral("two"))
        + helperCreateTrivialEnvelope(3, 3, QStringLiteral("three")) + fetchTag + " OK fetched\r\n");
    cClient(t.mk("RENAME a c\r\n"));
    cServer(t.last("OK renamed\r\n"));
    QCOMPARE(renamedSpy->size(), 1);
    QVERIFY(failedSpy->isEmpty());
}

QTEST_GUILESS_MAIN(ImapModelRenameMailboxTest)
