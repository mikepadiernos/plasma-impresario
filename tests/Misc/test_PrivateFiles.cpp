/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTest>

#include "test_PrivateFiles.h"
#include "Common/Paths.h"
#include "Common/PrivateFiles.h"

namespace Private = Common::PrivateFiles;

namespace {

constexpr QFile::Permissions allBits = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup
        | QFile::WriteGroup | QFile::ExeGroup | QFile::ReadOther | QFile::WriteOther | QFile::ExeOther;
constexpr QFile::Permissions userFolder = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
constexpr QFile::Permissions userFile = QFile::ReadOwner | QFile::WriteOwner;
constexpr QFile::Permissions openFolder = userFolder | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther;

QFile::Permissions modeOf(const QString &path)
{
    return QFileInfo(path).permissions() & allBits;
}

void writeFile(const QString &path, QFile::Permissions permissions)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("data");
    file.close();
    QVERIFY(QFile::setPermissions(path, permissions));
}

}

void PrivateFilesTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(m_dir.isValid());
}

void PrivateFilesTest::makeFolderCreatesUserOnlyParents()
{
    const QString top = m_dir.filePath(QStringLiteral("created"));
    const QString leaf = top + QStringLiteral("/a/b");
    QString error;
    QVERIFY2(Private::makeFolder(leaf, &error), qPrintable(error));
    QCOMPARE(modeOf(top), userFolder);
    QCOMPARE(modeOf(top + QStringLiteral("/a")), userFolder);
    QCOMPARE(modeOf(leaf), userFolder);
    // The temporary folder itself existed and is left as it is
    QVERIFY(Private::makeFolder(leaf));
}

void PrivateFilesTest::makeFolderTightensAnExistingFolder()
{
    const QString folder = m_dir.filePath(QStringLiteral("existing"));
    QVERIFY(QDir().mkdir(folder));
    QVERIFY(QFile::setPermissions(folder, openFolder));
    QVERIFY(Private::makeFolder(folder));
    QCOMPARE(modeOf(folder), userFolder);
}

void PrivateFilesTest::makeFolderRefusesAFile()
{
    const QString path = m_dir.filePath(QStringLiteral("plain-file"));
    writeFile(path, userFile);
    QString error;
    QVERIFY(!Private::makeFolder(path, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!Private::makeFolder(path + QStringLiteral("/inside"), &error));
}

void PrivateFilesTest::tightenFoldersAndFiles()
{
    const QString root = m_dir.filePath(QStringLiteral("cache"));
    QVERIFY(QDir().mkpath(root + QStringLiteral("/account-1/INBOX")));
    for (const QString &folder : QStringList{root, root + QStringLiteral("/account-1"), root + QStringLiteral("/account-1/INBOX")})
        QVERIFY(QFile::setPermissions(folder, openFolder));
    writeFile(root + QStringLiteral("/account-1/imap.cache.sqlite"),
              userFile | QFile::ReadGroup | QFile::ReadOther);
    writeFile(root + QStringLiteral("/account-1/INBOX/1_1.cache"), userFile | QFile::ExeOwner | QFile::ExeOther);
    writeFile(root + QStringLiteral("/account-1/INBOX/read-only"), QFile::ReadOwner | QFile::ReadOther);
    writeFile(root + QStringLiteral("/already-private"), userFile);

    QCOMPARE(Private::tighten(root), 6);
    QCOMPARE(modeOf(root), userFolder);
    QCOMPARE(modeOf(root + QStringLiteral("/account-1")), userFolder);
    QCOMPARE(modeOf(root + QStringLiteral("/account-1/INBOX")), userFolder);
    QCOMPARE(modeOf(root + QStringLiteral("/account-1/imap.cache.sqlite")), userFile);
    QCOMPARE(modeOf(root + QStringLiteral("/account-1/INBOX/1_1.cache")), userFile);
    // Read-only stays read-only, for the user alone
    QCOMPARE(modeOf(root + QStringLiteral("/account-1/INBOX/read-only")), QFile::Permissions(QFile::ReadOwner));
    QCOMPARE(modeOf(root + QStringLiteral("/already-private")), userFile);
    QCOMPARE(Private::tighten(root), 0);
    QCOMPARE(Private::tighten(m_dir.filePath(QStringLiteral("missing"))), 0);
}

void PrivateFilesTest::tightenLeavesLinkedFilesAlone()
{
    const QString outside = m_dir.filePath(QStringLiteral("outside"));
    QVERIFY(QDir().mkdir(outside));
    QVERIFY(QFile::setPermissions(outside, openFolder));
    const QString shared = outside + QStringLiteral("/shared.txt");
    writeFile(shared, userFile | QFile::ReadGroup | QFile::ReadOther);

    const QString root = m_dir.filePath(QStringLiteral("linking"));
    QVERIFY(QDir().mkdir(root));
    QVERIFY(QFile::link(shared, root + QStringLiteral("/file-link")));
    QVERIFY(QFile::link(outside, root + QStringLiteral("/folder-link")));
    Private::tighten(root);
    QCOMPARE(modeOf(outside), openFolder);
    QCOMPARE(modeOf(shared), userFile | QFile::ReadGroup | QFile::ReadOther);
    QCOMPARE(Private::tighten(root + QStringLiteral("/folder-link")), 0);
    QCOMPARE(modeOf(outside), openFolder);
}

void PrivateFilesTest::safeFileName_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("expected");

    QTest::newRow("plain") << QStringLiteral("report.pdf") << QStringLiteral("report.pdf");
    QTest::newRow("spaces kept") << QStringLiteral("Quarterly report 2026.pdf") << QStringLiteral("Quarterly report 2026.pdf");
    QTest::newRow("parent folders") << QStringLiteral("../../.bashrc") << QStringLiteral("bashrc");
    QTest::newRow("absolute") << QStringLiteral("/etc/passwd") << QStringLiteral("passwd");
    QTest::newRow("backslashes") << QStringLiteral("..\\..\\Startup\\run.bat") << QStringLiteral("run.bat");
    QTest::newRow("folder only") << QStringLiteral("folder/") << QStringLiteral("attachment");
    QTest::newRow("dots only") << QStringLiteral("..") << QStringLiteral("attachment");
    QTest::newRow("empty") << QString() << QStringLiteral("attachment");
    QTest::newRow("hidden") << QStringLiteral(".profile") << QStringLiteral("profile");
    QTest::newRow("control characters") << QStringLiteral("line\nbreak\t\x01.txt") << QStringLiteral("linebreak.txt");
    QTest::newRow("right-to-left override") << QString::fromUtf8("invoice\u202Efdp.exe") << QStringLiteral("invoicefdp.exe");
    QTest::newRow("isolates") << QString::fromUtf8("a\u2066b\u2069c.txt") << QStringLiteral("abc.txt");
    QTest::newRow("trailing dots and spaces") << QStringLiteral(" notes . . ") << QStringLiteral("notes");
    QTest::newRow("other scripts") << QString::fromUtf8("отчёт 報告.pdf") << QString::fromUtf8("отчёт 報告.pdf");
}

void PrivateFilesTest::safeFileName()
{
    QFETCH(QString, name);
    QFETCH(QString, expected);
    QCOMPARE(Private::safeFileName(name), expected);
}

void PrivateFilesTest::safeFileNameFitsTheFileSystem()
{
    const QString longAscii = Private::safeFileName(QString(300, QLatin1Char('a')) + QStringLiteral(".pdf"));
    QCOMPARE(longAscii.toUtf8().size(), 255);
    QVERIFY(longAscii.endsWith(QStringLiteral(".pdf")));

    QString wide;
    for (int i = 0; i < 200; ++i)
        wide += QString::fromUtf8("é");
    const QString longWide = Private::safeFileName(wide + QStringLiteral(".txt"));
    QVERIFY(longWide.toUtf8().size() <= 255);
    QVERIFY(longWide.endsWith(QStringLiteral(".txt")));

    // Four bytes each in UTF-8, two UTF-16 units; never cut in half
    QString emoji;
    for (int i = 0; i < 100; ++i)
        emoji += QString::fromUcs4(U"\U0001F4CE");
    const QString longEmoji = Private::safeFileName(emoji);
    QVERIFY(longEmoji.toUtf8().size() <= 255);
    QVERIFY(!longEmoji.back().isHighSurrogate());
    QCOMPARE(QString::fromUtf8(longEmoji.toUtf8()), longEmoji);
}

void PrivateFilesTest::openedAttachmentIsUserOnly()
{
    const QString folder = m_dir.filePath(QStringLiteral("opened-a"));
    QString error;
    const QString path = Private::createOpenedAttachment(folder, QStringLiteral("photo.jpg"), &error);
    QVERIFY2(!path.isEmpty(), qPrintable(error));
    QCOMPARE(QFileInfo(path).fileName(), QStringLiteral("photo.jpg"));
    QVERIFY(QFileInfo(path).isFile());
    QCOMPARE(QFileInfo(path).size(), 0);
    QCOMPARE(modeOf(path), userFile);
    QCOMPARE(modeOf(QFileInfo(path).path()), userFolder);
    QCOMPARE(modeOf(folder), userFolder);
    QCOMPARE(QFileInfo(QFileInfo(path).path()).path(), QFileInfo(folder).absoluteFilePath());

    // The download writes into the existing file, which keeps its permissions
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("JPEG");
    file.close();
    QCOMPARE(modeOf(path), userFile);
}

void PrivateFilesTest::openedAttachmentStaysInsideItsFolder()
{
    const QString folder = m_dir.filePath(QStringLiteral("opened-b"));
    const QString path = Private::createOpenedAttachment(folder, QStringLiteral("../../escape.sh"));
    QVERIFY(!path.isEmpty());
    QCOMPARE(QFileInfo(path).fileName(), QStringLiteral("escape.sh"));
    QVERIFY(QFileInfo(path).absoluteFilePath().startsWith(QFileInfo(folder).absoluteFilePath() + QLatin1Char('/')));
    QVERIFY(!QFileInfo::exists(m_dir.filePath(QStringLiteral("escape.sh"))));
    QVERIFY(!(QFileInfo(path).permissions() & (QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther)));
}

void PrivateFilesTest::openedAttachmentsDoNotCollide()
{
    const QString folder = m_dir.filePath(QStringLiteral("opened-c"));
    const QString first = Private::createOpenedAttachment(folder, QStringLiteral("same.txt"));
    const QString second = Private::createOpenedAttachment(folder, QStringLiteral("same.txt"));
    QVERIFY(!first.isEmpty());
    QVERIFY(!second.isEmpty());
    QVERIFY(first != second);
    QCOMPARE(QFileInfo(second).fileName(), QStringLiteral("same.txt"));
}

void PrivateFilesTest::openedAttachmentFolderInTheWay()
{
    const QString blocker = m_dir.filePath(QStringLiteral("blocker"));
    writeFile(blocker, userFile);
    QString error;
    QVERIFY(Private::createOpenedAttachment(blocker, QStringLiteral("a.txt"), &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

void PrivateFilesTest::removingACopyRemovesItsFolder()
{
    const QString folder = m_dir.filePath(QStringLiteral("opened-d"));
    const QString path = Private::createOpenedAttachment(folder, QStringLiteral("a.txt"));
    QVERIFY(!path.isEmpty());
    QVERIFY(QFile::setPermissions(path, QFile::ReadOwner));
    Private::removeOpenedAttachment(folder, path);
    QVERIFY(!QFileInfo::exists(path));
    QVERIFY(!QFileInfo::exists(QFileInfo(path).path()));
    QVERIFY(QFileInfo(folder).isDir());
    Private::removeOpenedAttachment(folder, QString());
}

void PrivateFilesTest::removingAFileOutsideKeepsItsFolder()
{
    const QString folder = m_dir.filePath(QStringLiteral("opened-e"));
    const QString elsewhere = m_dir.filePath(QStringLiteral("elsewhere"));
    QVERIFY(QDir().mkdir(elsewhere));
    writeFile(elsewhere + QStringLiteral("/keep.txt"), userFile);
    writeFile(elsewhere + QStringLiteral("/gone.txt"), userFile);
    Private::removeOpenedAttachment(folder, elsewhere + QStringLiteral("/gone.txt"));
    QVERIFY(QFileInfo::exists(elsewhere + QStringLiteral("/keep.txt")));
}

void PrivateFilesTest::oldCopiesAreRemovedAfterADay()
{
    const QString folder = m_dir.filePath(QStringLiteral("opened-f"));
    const QString older = Private::createOpenedAttachment(folder, QStringLiteral("older.txt"));
    const QString newer = Private::createOpenedAttachment(folder, QStringLiteral("newer.txt"));
    QVERIFY(!older.isEmpty());
    QVERIFY(!newer.isEmpty());
    // The newest change inside a copy's folder counts
    {
        QFile file(newer);
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(QDateTime::currentDateTimeUtc().addDays(2), QFileDevice::FileModificationTime));
    }
    QVERIFY(QFile::setPermissions(older, QFile::ReadOwner));

    const QDateTime now = QDateTime::currentDateTimeUtc();
    QCOMPARE(Private::removeOldOpenedAttachments(folder, now), 0);
    QCOMPARE(Private::removeOldOpenedAttachments(folder, now.addSecs(Private::openedAttachmentLifetime - 60)), 0);
    QCOMPARE(Private::removeOldOpenedAttachments(folder, now.addSecs(Private::openedAttachmentLifetime + 3600)), 1);
    QVERIFY(!QFileInfo::exists(older));
    QVERIFY(!QFileInfo::exists(QFileInfo(older).path()));
    QVERIFY(QFileInfo::exists(newer));
    QCOMPARE(Private::removeOldOpenedAttachments(m_dir.filePath(QStringLiteral("missing")), now), 0);
}

void PrivateFilesTest::cacheFolderIsInsideTheCache()
{
    const QString folder = Private::openedAttachmentsFolder();
    QVERIFY(folder.startsWith(Common::writablePath(Common::LOCATION_CACHE)));
    QVERIFY(!folder.endsWith(QLatin1Char('/')));
}

QTEST_GUILESS_MAIN(PrivateFilesTest)
