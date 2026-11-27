/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QSignalSpy>
#include <QTest>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Streams/FakeSocket.h"
#include "UiUtils/NewMailWatcher.h"
#include "Utils/LibMailboxSync.h"

using UiUtils::NewMailWatcher;

namespace {

/** A FETCH response with everything a message needs before the model counts it as loaded */
QByteArray metadata(const uint seq, const uint uid, const QString &subject)
{
    return QStringLiteral("* %1 FETCH (UID %2 RFC822.SIZE 89 INTERNALDATE \"15-Jan-2026 12:17:06 +0000\" "
                          "ENVELOPE (NIL \"%3\" ((NIL NIL \"ann\" \"example.org\")) NIL NIL NIL NIL NIL NIL NIL) "
                          "BODYSTRUCTURE (\"text\" \"plain\" () NIL NIL NIL 19 2 NIL NIL NIL NIL))\r\n")
        .arg(QString::number(seq), QString::number(uid), subject)
        .toUtf8();
}

}

class NewMailWatcherTest : public LibMailboxSync
{
    Q_OBJECT

private slots:
    void firstLookOnlyRecordsWhatIsThere();
    void reportsArrivalsOnceTheirEnvelopeIsLoaded();
    void restoredBaselineReportsMailWhichCameMeanwhile();
    void opensItsFolderOnlyWhenAsked();
    void newUidValidityStartsOver();
};

void NewMailWatcherTest::firstLookOnlyRecordsWhatIsThere()
{
    NewMailWatcher watcher(model, QStringLiteral("work"), QStringLiteral("a"));
    QSignalSpy arrived(&watcher, &NewMailWatcher::arrived);
    QSignalSpy baseline(&watcher, &NewMailWatcher::baselineChanged);
    initialMessages(3);
    QCoreApplication::processEvents();
    QCOMPARE(arrived.count(), 0);
    QVERIFY(baseline.count() >= 1);
    QCOMPARE(watcher.baseline().uidValidity, 333u);
    QCOMPARE(watcher.baseline().highestUid, 3u);
    QVERIFY(watcher.baseline().lastCheck.isValid());
    QVERIFY(model->isMailboxOpen(idxA));
    QVERIFY(!model->isMailboxOpen(idxB));
    cEmpty();
}

void NewMailWatcherTest::reportsArrivalsOnceTheirEnvelopeIsLoaded()
{
    NewMailWatcher watcher(model, QStringLiteral("work"), QStringLiteral("a"));
    QSignalSpy arrived(&watcher, &NewMailWatcher::arrived);
    initialMessages(3);
    QCoreApplication::processEvents();
    const QDateTime checked = watcher.baseline().lastCheck;

    cServer("* 4 EXISTS\r\n");
    cClient(t.mk("UID FETCH 4:* (FLAGS)\r\n"));
    cServer("* 4 FETCH (UID 4 FLAGS ())\r\n" + t.last("OK fetched\r\n"));
    QCoreApplication::processEvents();
    // The new message is known, but it is reported only with its envelope
    QCOMPARE(arrived.count(), 0);
    // The model loads the neighbours' envelopes together with the new one
    cClient(t.mk("UID FETCH 1:4 (" FETCH_METADATA_ITEMS ")\r\n"));
    cServer(metadata(1, 1, QStringLiteral("One")) + metadata(2, 2, QStringLiteral("Two"))
            + metadata(3, 3, QStringLiteral("Three")) + metadata(4, 4, QStringLiteral("Hello"))
            + t.last("OK fetched\r\n"));
    QCoreApplication::processEvents();
    QCOMPARE(arrived.count(), 1);
    const auto messages = arrived.first().at(0).value<QList<UiUtils::MailRules::Message>>();
    QCOMPARE(messages.size(), 1);
    QCOMPARE(messages.first().uid, 4u);
    QCOMPARE(messages.first().subject, QStringLiteral("Hello"));
    QCOMPARE(messages.first().folder, QStringLiteral("a"));
    QCOMPARE(messages.first().accountId, QStringLiteral("work"));
    QVERIFY(messages.first().unread);
    QCOMPARE(arrived.first().at(1).toDateTime(), checked);
    QCOMPARE(watcher.baseline().highestUid, 4u);

    // Later changes to the message do not report it again
    model->setMessageFlags({msgListA.model()->index(3, 0, msgListA)}, QStringLiteral("\\Flagged"), Imap::Mailbox::FLAG_ADD);
    cClient(t.mk("UID STORE 4 +FLAGS (\\Flagged)\r\n"));
    cServer(t.last("OK stored\r\n"));
    QCoreApplication::processEvents();
    QCOMPARE(arrived.count(), 1);
    cEmpty();
}

void NewMailWatcherTest::restoredBaselineReportsMailWhichCameMeanwhile()
{
    initialMessages(3);
    // A watcher made for a folder which is open already, with what was seen in the previous session
    NewMailWatcher watcher(model, QStringLiteral("work"), QStringLiteral("a"));
    const QDateTime before = QDateTime::currentDateTimeUtc().addSecs(-3600);
    watcher.setBaseline({333, 2, before});
    QSignalSpy arrived(&watcher, &NewMailWatcher::arrived);
    QCoreApplication::processEvents();
    cClient(t.mk("UID FETCH 1:3 (" FETCH_METADATA_ITEMS ")\r\n"));
    cServer(metadata(1, 1, QStringLiteral("One")) + metadata(2, 2, QStringLiteral("Two"))
            + metadata(3, 3, QStringLiteral("Meanwhile")) + t.last("OK fetched\r\n"));
    QCoreApplication::processEvents();
    QCOMPARE(arrived.count(), 1);
    const auto messages = arrived.first().at(0).value<QList<UiUtils::MailRules::Message>>();
    QCOMPARE(messages.size(), 1);
    QCOMPARE(messages.first().uid, 3u);
    QCOMPARE(arrived.first().at(1).toDateTime(), before);
    cEmpty();
}

void NewMailWatcherTest::opensItsFolderOnlyWhenAsked()
{
    initialMessages(1);
    NewMailWatcher watcher(model, QStringLiteral("work"), QStringLiteral("b"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    cEmpty();
    QVERIFY(!watcher.baseline().isValid());

    watcher.setOpensMailbox(true);
    QCoreApplication::processEvents();
    cClient(t.mk("SELECT b\r\n"));
    cServer("* 0 EXISTS\r\n* OK [UIDVALIDITY 7] .\r\n* OK [UIDNEXT 1] .\r\n" + t.last("OK selected\r\n"));
    QCoreApplication::processEvents();
    QCOMPARE(watcher.baseline().uidValidity, 7u);
    QCOMPARE(watcher.baseline().highestUid, 0u);
    cEmpty();
}

void NewMailWatcherTest::newUidValidityStartsOver()
{
    NewMailWatcher watcher(model, QStringLiteral("work"), QStringLiteral("a"));
    watcher.setBaseline({111, 50, QDateTime::currentDateTimeUtc()});
    QSignalSpy arrived(&watcher, &NewMailWatcher::arrived);
    initialMessages(3);
    QCoreApplication::processEvents();
    QCOMPARE(arrived.count(), 0);
    QCOMPARE(watcher.baseline().uidValidity, 333u);
    QCOMPARE(watcher.baseline().highestUid, 3u);
    cEmpty();
}

QTEST_GUILESS_MAIN(NewMailWatcherTest)

#include "test_NewMailWatcher.moc"
