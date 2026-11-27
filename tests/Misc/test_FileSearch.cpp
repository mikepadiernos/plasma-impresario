/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <QUrlQuery>

#include <KConfig>
#include <KConfigGroup>

#include "test_FileSearch.h"
#include "UiUtils/FileSearch.h"

namespace FileSearch = UiUtils::FileSearch;
using FileSearch::Coverage;

namespace {

FileSearch::Status coverageOf(const QString &configFile, const QString &folder, bool installed = true)
{
    const KConfig config(configFile, KConfig::SimpleConfig);
    return FileSearch::coverage(config, folder, installed);
}

}

void FileSearchTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
}

QString FileSearchTest::configPath(const QString &name, const QByteArray &contents)
{
    const QString path = m_dir.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QString();
    file.write(contents);
    return path;
}

void FileSearchTest::notInstalled()
{
    const QString config = configPath(QStringLiteral("installed"), "[General]\nfolders=/data\n");
    const auto status = coverageOf(config, QStringLiteral("/data/Saved"), false);
    QCOMPARE(status.coverage, Coverage::NotInstalled);
    QVERIFY(status.decidingFolder.isEmpty());
    QVERIFY(!status.isSearchable());
    QCOMPARE(coverageOf(config, QStringLiteral("/data/Saved")).coverage, Coverage::Covered);
}

void FileSearchTest::disabled()
{
    const QString config = configPath(QStringLiteral("disabled"),
                                      "[Basic Settings]\nIndexing-Enabled=false\n\n[General]\nfolders=/data\n");
    const auto status = coverageOf(config, QStringLiteral("/data/Saved"));
    QCOMPARE(status.coverage, Coverage::Disabled);
    QVERIFY(!status.isSearchable());
}

void FileSearchTest::defaultsToTheHomeFolder()
{
    // Without settings, Baloo indexes the home folder
    const QString config = configPath(QStringLiteral("empty"), QByteArray());
    const QString home = QDir::cleanPath(QDir::homePath());
    auto status = coverageOf(config, home + QStringLiteral("/Downloads"));
    QCOMPARE(status.coverage, Coverage::Covered);
    QCOMPARE(status.decidingFolder, home);
    QVERIFY(status.isSearchable());
    status = coverageOf(config, QStringLiteral("/nowhere-near-home/Saved"));
    QCOMPARE(status.coverage, Coverage::NotIncluded);
    QVERIFY(status.decidingFolder.isEmpty());
}

void FileSearchTest::closestFolderDecides()
{
    const QString config = configPath(QStringLiteral("nested"),
                                      "[General]\n"
                                      "folders=/data/,/data/private/shared\n"
                                      "exclude folders=/data/private/,/srv\n");
    auto status = coverageOf(config, QStringLiteral("/data/Saved"));
    QCOMPARE(status.coverage, Coverage::Covered);
    QCOMPARE(status.decidingFolder, QStringLiteral("/data"));

    status = coverageOf(config, QStringLiteral("/data/private/Saved/"));
    QCOMPARE(status.coverage, Coverage::Excluded);
    QCOMPARE(status.decidingFolder, QStringLiteral("/data/private"));
    QVERIFY(!status.isSearchable());

    status = coverageOf(config, QStringLiteral("/data/private/shared/Mail"));
    QCOMPARE(status.coverage, Coverage::Covered);
    QCOMPARE(status.decidingFolder, QStringLiteral("/data/private/shared"));

    QCOMPARE(coverageOf(config, QStringLiteral("/data/private")).coverage, Coverage::Excluded);
    QCOMPARE(coverageOf(config, QStringLiteral("/data")).coverage, Coverage::Covered);
    // A folder whose name only starts like an indexed one is not inside it
    QCOMPARE(coverageOf(config, QStringLiteral("/database/Saved")).coverage, Coverage::NotIncluded);
    QCOMPARE(coverageOf(config, QStringLiteral("/srv/Saved")).coverage, Coverage::Excluded);
    QCOMPARE(coverageOf(config, QStringLiteral("/data/x/../private/y")).coverage, Coverage::Excluded);

    // An exclusion wins over an inclusion of the same folder
    const QString both = configPath(QStringLiteral("both"), "[General]\nfolders=/data\nexclude folders=/data\n");
    QCOMPARE(coverageOf(both, QStringLiteral("/data/Saved")).coverage, Coverage::Excluded);

    const QString root = configPath(QStringLiteral("root"), "[General]\nfolders=/\n");
    status = coverageOf(root, QStringLiteral("/data/Saved"));
    QCOMPARE(status.coverage, Coverage::Covered);
    QCOMPARE(status.decidingFolder, QStringLiteral("/"));
}

void FileSearchTest::hiddenFolders()
{
    const QString config = configPath(QStringLiteral("hidden"), "[General]\nfolders=/data,/data/.kept\n");
    auto status = coverageOf(config, QStringLiteral("/data/.stash/attachments"));
    QCOMPARE(status.coverage, Coverage::Hidden);
    QCOMPARE(status.decidingFolder, QStringLiteral("/data/.stash"));
    QVERIFY(!status.isSearchable());
    // A hidden folder which is indexed explicitly is indexed
    QCOMPARE(coverageOf(config, QStringLiteral("/data/.kept/attachments")).coverage, Coverage::Covered);
    QCOMPARE(coverageOf(config, QStringLiteral("/data/.kept/.deeper")).coverage, Coverage::Hidden);

    const QString root = configPath(QStringLiteral("hiddenRoot"), "[General]\nfolders=/\n");
    status = coverageOf(root, QStringLiteral("/.snapshots/Saved"));
    QCOMPARE(status.coverage, Coverage::Hidden);
    QCOMPARE(status.decidingFolder, QStringLiteral("/.snapshots"));

    const QString all = configPath(QStringLiteral("hiddenToo"), "[General]\nfolders=/data\nindex hidden folders=true\n");
    QCOMPARE(coverageOf(all, QStringLiteral("/data/.stash/attachments")).coverage, Coverage::Covered);
}

void FileSearchTest::namesOnly()
{
    const QString config = configPath(QStringLiteral("basic"), "[General]\nfolders=/data\nonly basic indexing=true\n");
    const auto status = coverageOf(config, QStringLiteral("/data/Saved"));
    QCOMPARE(status.coverage, Coverage::NamesOnly);
    QCOMPARE(status.decidingFolder, QStringLiteral("/data"));
    QVERIFY(status.isSearchable());
    QCOMPARE(coverageOf(config, QStringLiteral("/elsewhere")).coverage, Coverage::NotIncluded);
}

void FileSearchTest::indexedSubfolders()
{
    const QString config = configPath(QStringLiteral("inside"),
                                      "[General]\n"
                                      "folders=/data/Reports,/srv/private/kept,/srv/private/gone\n"
                                      "exclude folders=/srv/private,/srv/private/gone\n");
    // A folder outside every indexed one can still hold one, which a search of the folder finds
    auto status = coverageOf(config, QStringLiteral("/data"));
    QCOMPARE(status.coverage, Coverage::NotIncluded);
    QCOMPARE(status.indexedSubfolder, QStringLiteral("/data/Reports"));
    QVERIFY(status.isSearchable());
    QVERIFY(!status.coversWholeFolder());

    status = coverageOf(config, QStringLiteral("/srv/private"));
    QCOMPARE(status.coverage, Coverage::Excluded);
    QCOMPARE(status.indexedSubfolder, QStringLiteral("/srv/private/kept"));
    QVERIFY(status.isSearchable());

    // An indexed folder that is also excluded does not count
    const QString gone = configPath(QStringLiteral("insideGone"), "[General]\nfolders=/srv/private/gone\n"
                                                                  "exclude folders=/srv/private/gone\n");
    status = coverageOf(gone, QStringLiteral("/srv"));
    QCOMPARE(status.coverage, Coverage::NotIncluded);
    QVERIFY(status.indexedSubfolder.isEmpty());
    QVERIFY(!status.isSearchable());

    // Neither do the folder itself, a folder whose name only starts like it, or a folder outside it
    const QString near = configPath(QStringLiteral("insideNear"), "[General]\nfolders=/database,/elsewhere/data\n");
    QVERIFY(coverageOf(near, QStringLiteral("/data")).indexedSubfolder.isEmpty());
    QVERIFY(coverageOf(config, QStringLiteral("/data/Reports")).indexedSubfolder.isEmpty());
    QVERIFY(coverageOf(config, QStringLiteral("/data/Reports")).coversWholeFolder());
}

void FileSearchTest::expandsHomeInPaths()
{
    // System Settings writes the folders as path entries relative to $HOME
    const QString config = configPath(QStringLiteral("expand"),
                                      "[General]\n"
                                      "folders[$e]=$HOME/Mail\n"
                                      "exclude folders[$e]=$HOME/Mail/Private/\n");
    const QString home = QDir::cleanPath(QDir::homePath());
    auto status = coverageOf(config, home + QStringLiteral("/Mail/Saved"));
    QCOMPARE(status.coverage, Coverage::Covered);
    QCOMPARE(status.decidingFolder, home + QStringLiteral("/Mail"));
    status = coverageOf(config, home + QStringLiteral("/Mail/Private/Saved"));
    QCOMPARE(status.coverage, Coverage::Excluded);
    QCOMPARE(status.decidingFolder, home + QStringLiteral("/Mail/Private"));
    QCOMPARE(coverageOf(config, home + QStringLiteral("/Documents")).coverage, Coverage::NotIncluded);
}

void FileSearchTest::indexedSearchUrl()
{
    const QString text = QStringLiteral("Q3 report & notes+draft #2 “final”");
    const QUrl url = FileSearch::indexedSearchUrl(text, QStringLiteral("/data/Saved/"), QStringLiteral("Saved files"));
    QCOMPARE(url.scheme(), QStringLiteral("baloosearch"));
    QVERIFY(url.path().isEmpty());

    // Read back the way Baloo::Query::fromSearchUrl() does, after a trip through text
    const QUrl reparsed(url.toString(QUrl::FullyEncoded));
    const QUrlQuery query(reparsed);
    const QJsonObject json = QJsonDocument::fromJson(query.queryItemValue(QStringLiteral("json"), QUrl::FullyDecoded).toUtf8()).object();
    QCOMPARE(json.value(QStringLiteral("searchString")).toString(), text);
    QCOMPARE(json.value(QStringLiteral("includeFolder")).toString(), QStringLiteral("/data/Saved"));
    QCOMPARE(json.size(), 2);
    QCOMPARE(query.queryItemValue(QStringLiteral("title"), QUrl::FullyDecoded), QStringLiteral("Saved files"));

    const QUrl bare = FileSearch::indexedSearchUrl(QStringLiteral("x"), QString(), QString());
    const QUrlQuery bareQuery(bare);
    QVERIFY(!bareQuery.hasQueryItem(QStringLiteral("title")));
    const QJsonObject bareJson = QJsonDocument::fromJson(bareQuery.queryItemValue(QStringLiteral("json"), QUrl::FullyDecoded).toUtf8()).object();
    QVERIFY(!bareJson.contains(QStringLiteral("includeFolder")));
}

void FileSearchTest::liveSearchUrl()
{
    const QString text = QStringLiteral("invoice & receipt");
    const QUrl url = FileSearch::liveSearchUrl(text, QStringLiteral("/data/My Saved/"), QStringLiteral("Saved files"));
    QCOMPARE(url.scheme(), QStringLiteral("filenamesearch"));
    const QUrlQuery query(QUrl(url.toString(QUrl::FullyEncoded)));
    QCOMPARE(query.queryItemValue(QStringLiteral("search"), QUrl::FullyDecoded), text);
    QCOMPARE(query.queryItemValue(QStringLiteral("checkContent")), QStringLiteral("yes"));
    QCOMPARE(QUrl(query.queryItemValue(QStringLiteral("url"), QUrl::FullyDecoded)),
             QUrl::fromLocalFile(QStringLiteral("/data/My Saved")));
    QCOMPARE(query.queryItemValue(QStringLiteral("title"), QUrl::FullyDecoded), QStringLiteral("Saved files"));
}

void FileSearchTest::savedFilesFolder()
{
    KConfig config(m_dir.filePath(QStringLiteral("state")), KConfig::SimpleConfig);
    KConfigGroup state = config.group(QStringLiteral("SavedFiles"));
    const QString fallback = QStringLiteral("/fallback");
    QCOMPARE(FileSearch::savedFilesFolder(state, fallback), fallback);

    QVERIFY(QDir(m_dir.path()).mkpath(QStringLiteral("Saved/Mail")));
    const QString saved = m_dir.filePath(QStringLiteral("Saved/Mail"));
    FileSearch::rememberSavedFile(state, saved + QStringLiteral("/report.pdf"));
    QCOMPARE(FileSearch::savedFilesFolder(state, fallback), saved);

    // Cancelled save dialogs keep the folder
    FileSearch::rememberSavedFile(state, QString());
    QCOMPARE(FileSearch::savedFilesFolder(state, fallback), saved);

    QVERIFY(QDir(saved).removeRecursively());
    QCOMPARE(FileSearch::savedFilesFolder(state, fallback), fallback);
}

QTEST_GUILESS_MAIN(FileSearchTest)
