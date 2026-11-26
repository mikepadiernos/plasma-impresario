/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include "test_SqlCache.h"
#include "Imap/Model/SQLCache.h"

Q_DECLARE_METATYPE(QList<Imap::Mailbox::MailboxMetadata>)

namespace QTest {
template<>
char *toString(const QList<Imap::Mailbox::MailboxMetadata> &items)
{
    QString buf;
    QDebug dbg(&buf);
    dbg << "[";
    Q_FOREACH(const Imap::Mailbox::MailboxMetadata &metadata, items) {
        dbg << "MailboxMetadata(" << metadata.mailbox << metadata.separator << metadata.flags << "),";
    }
    dbg << "]";
    return qstrdup(buf.toUtf8().constData());
}
}

void TestSqlCache::initTestCase()
{
    errorLog.clear();
    cache = std::make_shared<Imap::Mailbox::SQLCache>();
    cache->setErrorHandler([this](const QString &e) { this->errorLog.push_back(e); });
    QCOMPARE(cache->open(QLatin1String("meh"), QLatin1String(":memory:")), true);
}

void TestSqlCache::cleanupTestCase()
{
}

#define CHECK_CACHE_ERRORS \
    if (!errorLog.empty()) { \
        QCOMPARE(errorLog[0], QString()); \
    }

void TestSqlCache::testMailboxOperation()
{
    using namespace Imap::Mailbox;

    QCOMPARE(cache->childMailboxesFresh(QString()), false);
    CHECK_CACHE_ERRORS;
    QCOMPARE(cache->childMailboxesFresh(QLatin1String("foo")), false);
    CHECK_CACHE_ERRORS;

    QList<MailboxMetadata> toplevel;
    toplevel << MailboxMetadata(QStringLiteral("INBOX"), QStringLiteral("."), QStringList());
    toplevel << MailboxMetadata(QStringLiteral("a"), QString(), QStringList());
    toplevel << MailboxMetadata(QStringLiteral("b"), QString(), QStringList() << QStringLiteral("\\HASNOCHILDREN"));
    toplevel << MailboxMetadata(QStringLiteral("c"), QString(), QStringList() << QStringLiteral("\\HASCHILDREN"));

    cache->setChildMailboxes(QString(), toplevel);
    CHECK_CACHE_ERRORS;
    QCOMPARE(cache->childMailboxes(QString()), toplevel);
    CHECK_CACHE_ERRORS;

    // Check that the cache really removes items
    toplevel.takeLast();
    cache->setChildMailboxes(QString(), toplevel);
    CHECK_CACHE_ERRORS;
    QCOMPARE(cache->childMailboxes(QString()), toplevel);
    CHECK_CACHE_ERRORS;

    QVERIFY(errorLog.empty());
}

namespace {

Imap::Mailbox::AbstractCache::MessageDataBundle listMessage(uint uid)
{
    Imap::Mailbox::AbstractCache::MessageDataBundle bundle;
    bundle.uid = uid;
    bundle.size = 1234;
    bundle.hdrReferences = {"a@example.org"};
    bundle.hdrListPost = {QUrl(QStringLiteral("mailto:dev@lists.example.org"))};
    bundle.hdrList = {{"list-id", "Developers <dev.lists.example.org>"},
                      {"list-unsubscribe", "<mailto:dev-leave@lists.example.org>"}};
    return bundle;
}

}

void TestSqlCache::keepsListHeaders()
{
    const auto bundle = listMessage(7);
    cache->setMessageMetadata(QStringLiteral("lists"), 7, bundle);
    CHECK_CACHE_ERRORS;
    QCOMPARE(cache->messageMetadata(QStringLiteral("lists"), 7), bundle);
    QVERIFY(errorLog.empty());
}

void TestSqlCache::dropsMetadataWithoutListHeaders()
{
    // Metadata cached before List-Id was fetched would never get it, so opening an older cache forgets it
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(QStringLiteral("cache.sqlite"));
    std::vector<QString> errors;
    {
        Imap::Mailbox::SQLCache older;
        older.setErrorHandler([&errors](const QString &e) { errors.push_back(e); });
        QVERIFY(older.open(QStringLiteral("older"), file));
        older.setMessageMetadata(QStringLiteral("lists"), 7, listMessage(7));
        QCOMPARE(older.messageMetadata(QStringLiteral("lists"), 7).uid, 7u);
    }
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("downgrade"));
        db.setDatabaseName(file);
        QVERIFY(db.open());
        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral("SELECT version FROM trojita")));
        QVERIFY(q.first());
        QCOMPARE(q.value(0).toUInt(), 8u);
        QVERIFY(q.exec(QStringLiteral("UPDATE trojita SET version = 7")));
        q = QSqlQuery();
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("downgrade"));
    {
        Imap::Mailbox::SQLCache upgraded;
        upgraded.setErrorHandler([&errors](const QString &e) { errors.push_back(e); });
        QVERIFY(upgraded.open(QStringLiteral("upgraded"), file));
        QCOMPARE(upgraded.messageMetadata(QStringLiteral("lists"), 7).uid, 0u);
        upgraded.setMessageMetadata(QStringLiteral("lists"), 7, listMessage(7));
        QCOMPARE(upgraded.messageMetadata(QStringLiteral("lists"), 7), listMessage(7));
    }
    QVERIFY(errors.empty());
}

QTEST_GUILESS_MAIN(TestSqlCache)
