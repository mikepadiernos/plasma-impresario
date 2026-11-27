/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDir>
#include <QFileInfo>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "LocalMail/Store.h"

using namespace LocalMail;

class LocalMailStoreTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void startsWithTheDefaultFolders()
    {
        QTemporaryDir dir;
        auto store = Store::open(dir.path() + QLatin1String("/mail"));
        QVERIFY(store);
        QCOMPARE(store->mailboxNames(), (QStringList{QStringLiteral("INBOX"), QStringLiteral("Drafts"), QStringLiteral("Sent"), QStringLiteral("Trash")}));
        QVERIFY(store->mailbox(QStringLiteral("INBOX")).uidValidity > 0);
        QVERIFY(store->mailbox(QStringLiteral("Sent")).uidValidity != store->mailbox(QStringLiteral("INBOX")).uidValidity);
        QCOMPARE(QFileInfo(dir.path() + QLatin1String("/mail")).permissions() & (QFile::ReadOther | QFile::ReadGroup),
                 QFile::Permissions());
    }

    void sharesOneStorePerFolder()
    {
        QTemporaryDir dir;
        auto first = Store::open(dir.path());
        auto second = Store::open(dir.path() + QLatin1String("/."));
        QCOMPARE(first.get(), second.get());
    }

    void appendsAndReadsMessages()
    {
        QTemporaryDir dir;
        auto store = Store::open(dir.path());
        QSignalSpy changed(store.get(), &Store::changed);
        const QDateTime when(QDate(2026, 11, 2), QTime(8, 30), QTimeZone::UTC);
        const uint first = store->append(QStringLiteral("INBOX"), "Subject: one\n\nbody\n", {QStringLiteral("\\Seen"), QStringLiteral("\\Recent")}, when);
        const uint second = store->append(QStringLiteral("INBOX"), "Subject: two\r\n\r\nbody\r\n");
        QCOMPARE(first, 1u);
        QCOMPARE(second, 2u);
        QCOMPARE(changed.count(), 2);
        QCOMPARE(changed.first().first().toString(), QStringLiteral("INBOX"));

        const QList<MessageInfo> messages = store->messages(QStringLiteral("INBOX"));
        QCOMPARE(messages.size(), 2);
        QCOMPARE(messages[0].flags, QStringList{QStringLiteral("\\Seen")});
        QCOMPARE(messages[0].internalDate, when);
        QCOMPARE(messages[0].size, qint64(QByteArray("Subject: one\r\n\r\nbody\r\n").size()));
        QCOMPARE(store->content(QStringLiteral("INBOX"), 1), QByteArray("Subject: one\r\n\r\nbody\r\n"));
        QCOMPARE(QFileInfo(dir.path() + QLatin1String("/messages/") + messages[0].file).permissions()
                     & (QFile::ReadGroup | QFile::ReadOther | QFile::WriteGroup | QFile::WriteOther),
                 QFile::Permissions());
        QCOMPARE(store->mailbox(QStringLiteral("INBOX")).uidNext, 3u);
        QVERIFY(!store->append(QStringLiteral("Missing"), "x"));
    }

    void changesFlagsCopiesAndRemoves()
    {
        QTemporaryDir dir;
        auto store = Store::open(dir.path());
        store->append(QStringLiteral("INBOX"), "Subject: one\r\n\r\n");
        store->append(QStringLiteral("INBOX"), "Subject: two\r\n\r\n");
        QVERIFY(store->setFlags(QStringLiteral("INBOX"), 2, {QStringLiteral("\\Flagged"), QStringLiteral("\\flagged"), QStringLiteral("$Work")}));
        QCOMPARE(store->messages(QStringLiteral("INBOX"))[1].flags, (QStringList{QStringLiteral("\\Flagged"), QStringLiteral("$Work")}));

        const QList<uint> copied = store->copy(QStringLiteral("INBOX"), {2, 1}, QStringLiteral("Trash"));
        QCOMPARE(copied, (QList<uint>{1, 2}));
        QCOMPARE(store->content(QStringLiteral("Trash"), 1), QByteArray("Subject: two\r\n\r\n"));
        QCOMPARE(store->messages(QStringLiteral("Trash"))[0].flags, (QStringList{QStringLiteral("\\Flagged"), QStringLiteral("$Work")}));

        const QString file = store->messages(QStringLiteral("INBOX"))[0].file;
        QVERIFY(store->remove(QStringLiteral("INBOX"), {1}));
        QCOMPARE(store->messages(QStringLiteral("INBOX")).size(), 1);
        QVERIFY(!QFile::exists(dir.path() + QLatin1String("/messages/") + file));
        QVERIFY(store->content(QStringLiteral("INBOX"), 1).isEmpty());
    }

    void managesFolders()
    {
        QTemporaryDir dir;
        auto store = Store::open(dir.path());
        QVERIFY(store->createMailbox(QStringLiteral("Work")));
        QVERIFY(store->createMailbox(QStringLiteral("Work/Projects")));
        QVERIFY(!store->createMailbox(QStringLiteral("Work")));
        store->append(QStringLiteral("Work/Projects"), "Subject: p\r\n\r\n");
        QVERIFY(store->renameMailbox(QStringLiteral("Work"), QStringLiteral("Jobs")));
        QVERIFY(store->hasMailbox(QStringLiteral("Jobs/Projects")));
        QVERIFY(!store->hasMailbox(QStringLiteral("Work/Projects")));
        QCOMPARE(store->content(QStringLiteral("Jobs/Projects"), 1), QByteArray("Subject: p\r\n\r\n"));
        QVERIFY(!store->renameMailbox(QStringLiteral("INBOX"), QStringLiteral("Old")));
        QVERIFY(!store->deleteMailbox(QStringLiteral("INBOX")));
        QVERIFY(store->setSubscribed(QStringLiteral("Jobs"), false));
        QVERIFY(!store->mailbox(QStringLiteral("Jobs")).subscribed);
        QVERIFY(store->deleteMailbox(QStringLiteral("Jobs/Projects")));
        QVERIFY(!store->hasMailbox(QStringLiteral("Jobs/Projects")));
        QCOMPARE(QDir(dir.path() + QLatin1String("/messages")).entryList(QDir::Files).size(), 0);

        // A folder made again under an old name never reuses a UIDVALIDITY
        const uint old = store->mailbox(QStringLiteral("Jobs")).uidValidity;
        QVERIFY(store->deleteMailbox(QStringLiteral("Jobs")));
        QVERIFY(store->createMailbox(QStringLiteral("Jobs")));
        QVERIFY(store->mailbox(QStringLiteral("Jobs")).uidValidity > old);
    }

    void remembersOrigins()
    {
        QTemporaryDir dir;
        auto store = Store::open(dir.path());
        const uint uid = store->append(QStringLiteral("INBOX"), "Subject: pop\r\n\r\n", {}, {}, QStringLiteral("uidl-1"));
        QVERIFY(store->knowsOrigin(QStringLiteral("uidl-1")));
        QVERIFY(store->originStored(QStringLiteral("uidl-1")).isValid());
        QVERIFY(!store->knowsOrigin(QStringLiteral("uidl-2")));
        QVERIFY(store->remove(QStringLiteral("INBOX"), {uid}));
        QVERIFY(store->knowsOrigin(QStringLiteral("uidl-1")));
        store->append(QStringLiteral("INBOX"), "Subject: pop2\r\n\r\n", {}, {}, QStringLiteral("uidl-2"));
        store->keepOnlyOrigins({QStringLiteral("uidl-2")});
        QCOMPARE(store->origins(), QStringList{QStringLiteral("uidl-2")});

        // A copy, such as one half of a move, is still the server's message
        const QList<uint> copied = store->copy(QStringLiteral("INBOX"), {2}, QStringLiteral("Trash"));
        QCOMPARE(copied.size(), 1);
        QCOMPARE(store->messages(QStringLiteral("Trash")).constFirst().origin, QStringLiteral("uidl-2"));
    }

    void keepsEverythingAcrossReopening()
    {
        QTemporaryDir dir;
        uint validity = 0;
        {
            auto store = Store::open(dir.path());
            store->createMailbox(QStringLiteral("Archive"));
            store->append(QStringLiteral("Archive"), "Subject: kept\r\n\r\n", {QStringLiteral("\\Answered")}, {}, QStringLiteral("o"));
            store->setSubscribed(QStringLiteral("Archive"), false);
            validity = store->mailbox(QStringLiteral("Archive")).uidValidity;
        }
        auto store = Store::open(dir.path());
        QVERIFY(store->hasMailbox(QStringLiteral("Archive")));
        QCOMPARE(store->mailbox(QStringLiteral("Archive")).uidValidity, validity);
        QCOMPARE(store->mailbox(QStringLiteral("Archive")).uidNext, 2u);
        QVERIFY(!store->mailbox(QStringLiteral("Archive")).subscribed);
        QCOMPARE(store->messages(QStringLiteral("Archive")).size(), 1);
        QCOMPARE(store->messages(QStringLiteral("Archive"))[0].flags, QStringList{QStringLiteral("\\Answered")});
        QCOMPARE(store->content(QStringLiteral("Archive"), 1), QByteArray("Subject: kept\r\n\r\n"));
        QVERIFY(store->knowsOrigin(QStringLiteral("o")));
    }
};

QTEST_GUILESS_MAIN(LocalMailStoreTest)
#include "test_LocalMailStore.moc"
