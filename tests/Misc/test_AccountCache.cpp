/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include "test_AccountCache.h"
#include "Imap/Model/Utils.h"

namespace {

void touch(const QString &path)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("synthetic");
}

/** @short Lay out a cache root shared by the migrated account and two account directories */
void seed(const QString &root)
{
    touch(root + QStringLiteral("/imap.cache.sqlite"));
    touch(root + QStringLiteral("/imap.cache.sqlite-journal"));
    // Mailbox part directories are named with the Base64 form of the mailbox name, which may contain a slash.
    touch(root + QStringLiteral("/SU5CT1g=/12_1.cache"));
    touch(root + QStringLiteral("/UHJvamVjdHMvMjAyNg/X29sZA==/7_TEXT.cache"));
    touch(root + QStringLiteral("/Drafts/draft-1.draft"));
    touch(root + QStringLiteral("/trojita-connection-log"));
    touch(root + QStringLiteral("/Unrelated/notes.txt"));
    touch(root + QStringLiteral("/account-1/imap.cache.sqlite"));
    touch(root + QStringLiteral("/account-1/SU5CT1g=/3_1.cache"));
    touch(root + QStringLiteral("/account-2/imap.cache.sqlite"));
}

}

void AccountCacheTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void AccountCacheTest::removesTheAccountDirectory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.filePath(QStringLiteral("cache"));
    seed(root);

    QVERIFY(Imap::removeAccountCache(root + QStringLiteral("/account-1/"), root + QLatin1Char('/')));
    QVERIFY(!QFileInfo::exists(root + QStringLiteral("/account-1")));
    QVERIFY(QFile::exists(root + QStringLiteral("/account-2/imap.cache.sqlite")));
    QVERIFY(QFile::exists(root + QStringLiteral("/imap.cache.sqlite")));
    QVERIFY(QFile::exists(root + QStringLiteral("/SU5CT1g=/12_1.cache")));
    QVERIFY(QFile::exists(root + QStringLiteral("/Drafts/draft-1.draft")));

    // An account that never cached anything has nothing to delete.
    QVERIFY(Imap::removeAccountCache(root + QStringLiteral("/account-3/"), root));
}

void AccountCacheTest::keepsTheSharedCacheOfTheMigratedAccount()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.filePath(QStringLiteral("cache"));
    seed(root);

    QVERIFY(Imap::removeAccountCache(root + QLatin1Char('/'), root));
    QVERIFY(!QFile::exists(root + QStringLiteral("/imap.cache.sqlite")));
    QVERIFY(!QFile::exists(root + QStringLiteral("/imap.cache.sqlite-journal")));
    QVERIFY(!QFileInfo::exists(root + QStringLiteral("/SU5CT1g=")));
    QVERIFY(!QFileInfo::exists(root + QStringLiteral("/UHJvamVjdHMvMjAyNg")));
    QVERIFY(QDir(root).exists());
    QVERIFY(QFile::exists(root + QStringLiteral("/Drafts/draft-1.draft")));
    QVERIFY(QFile::exists(root + QStringLiteral("/trojita-connection-log")));
    QVERIFY(QFile::exists(root + QStringLiteral("/Unrelated/notes.txt")));
    QVERIFY(QFile::exists(root + QStringLiteral("/account-1/SU5CT1g=/3_1.cache")));
    QVERIFY(QFile::exists(root + QStringLiteral("/account-2/imap.cache.sqlite")));
}

void AccountCacheTest::refusesDirectoriesOutsideTheCache()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.filePath(QStringLiteral("cache"));
    seed(root);
    touch(directory.filePath(QStringLiteral("elsewhere/keep.txt")));

    QVERIFY(!Imap::removeAccountCache(directory.filePath(QStringLiteral("elsewhere")), root));
    QVERIFY(!Imap::removeAccountCache(directory.filePath(QStringLiteral("cache-other")), root));
    QVERIFY(!Imap::removeAccountCache(QString(), root));
    QVERIFY(!Imap::removeAccountCache(root + QStringLiteral("/account-1"), QString()));
    QVERIFY(QFile::exists(directory.filePath(QStringLiteral("elsewhere/keep.txt"))));
    QVERIFY(QFile::exists(root + QStringLiteral("/account-1/imap.cache.sqlite")));
}

QTEST_GUILESS_MAIN(AccountCacheTest)
