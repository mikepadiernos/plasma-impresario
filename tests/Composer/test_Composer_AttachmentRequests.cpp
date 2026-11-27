/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTest>
#include <QUrl>
#include <QUrlQuery>

#include "test_Composer_AttachmentRequests.h"
#include "Composer/AttachmentRequests.h"

using namespace Composer::AttachmentRequests;

namespace {

/** A mailto: URL built the way KIO's KEMailClientLauncherJob hands files to a mail client */
QUrl launcherUrl(const QStringList &to, const QList<QUrl> &attachments)
{
    QUrl url(QStringLiteral("mailto:") + to.join(QLatin1Char(',')));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("subject"), QStringLiteral("Files"));
    for (const QUrl &attachment : attachments)
        query.addQueryItem(QStringLiteral("attach"), QString::fromUtf8(QUrl::toPercentEncoding(attachment.toString())));
    url.setQuery(query);
    return url;
}

/** The same URL after it travelled as text, as it does over D-Bus and on the command line */
QUrl throughText(const QUrl &url)
{
    return QUrl::fromEncoded(url.toEncoded());
}

}

QString ComposerAttachmentRequestsTest::makeFile(const QString &relativePath, const QByteArray &content)
{
    const QString path = m_dir.filePath(relativePath);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size())
        qWarning() << "Cannot write" << path;
    return QFileInfo(path).absoluteFilePath();
}

void ComposerAttachmentRequestsTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(m_dir.isValid());
}

void ComposerAttachmentRequestsTest::launcherUrlsRoundTrip_data()
{
    QTest::addColumn<QString>("name");
    QTest::newRow("plain") << QStringLiteral("report.pdf");
    QTest::newRow("space") << QStringLiteral("annual report.pdf");
    QTest::newRow("percent") << QStringLiteral("100% done.txt");
    QTest::newRow("ampersand and equals") << QStringLiteral("a&b=c.txt");
    QTest::newRow("hash and question mark") << QStringLiteral("what#is?this.txt");
    QTest::newRow("plus") << QStringLiteral("c++ notes.txt");
    QTest::newRow("non-ASCII") << QStringLiteral("Größe – 東京.txt");
}

void ComposerAttachmentRequestsTest::launcherUrlsRoundTrip()
{
    QFETCH(QString, name);
    const QString path = makeFile(QStringLiteral("round/") + name);
    const QUrl url = launcherUrl({QStringLiteral("someone@example.org")}, {QUrl::fromLocalFile(path)});

    for (const QUrl &received : {url, throughText(url)}) {
        const QStringList requested = fromMailto(received);
        QCOMPARE(requested.size(), 1);
        QVERIFY(hasRequests(received));
        const Request request = resolve(requested.first(), Source::MailtoUrl);
        QVERIFY2(request.accepted(), qPrintable(request.problem));
        QCOMPARE(request.path, path);
        QCOMPARE(QFileInfo(request.path).fileName(), name);
    }
}

void ComposerAttachmentRequestsTest::attachmentKeys()
{
    const QString first = makeFile(QStringLiteral("keys/first.txt"));
    const QString second = makeFile(QStringLiteral("keys/second.txt"));
    const QString third = makeFile(QStringLiteral("keys/third.txt"));
    // Thunderbird-style handoffs name the files with attachment= and plain paths
    QUrl url(QStringLiteral("mailto:someone@example.org"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("ATTACH"), QString::fromUtf8(QUrl::toPercentEncoding(QUrl::fromLocalFile(first).toString())));
    query.addQueryItem(QStringLiteral("body"), QStringLiteral("not a file"));
    query.addQueryItem(QStringLiteral("attachment"), QString::fromUtf8(QUrl::toPercentEncoding(second)));
    query.addQueryItem(QStringLiteral("Attachment"), QString::fromUtf8(QUrl::toPercentEncoding(third)));
    query.addQueryItem(QStringLiteral("attach"), QString());
    query.addQueryItem(QStringLiteral("attachments"), QString::fromUtf8(QUrl::toPercentEncoding(third)));
    url.setQuery(query);

    const QStringList requested = fromMailto(throughText(url));
    QCOMPARE(requested, (QStringList{QUrl::fromLocalFile(first).toString(), second, third}));
    const QList<Request> resolved = resolveAll(requested, Source::MailtoUrl);
    QCOMPARE(resolved.size(), 3);
    QCOMPARE(resolved[0].path, first);
    QCOMPARE(resolved[1].path, second);
    QCOMPARE(resolved[2].path, third);

    QVERIFY(!hasRequests(QUrl(QStringLiteral("mailto:someone@example.org?subject=Hello&body=attach"))));
}

void ComposerAttachmentRequestsTest::otherSchemesIgnored()
{
    const QString path = makeFile(QStringLiteral("schemes/file.txt"));
    QUrl url(QStringLiteral("https://example.org/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("attach"), path);
    url.setQuery(query);
    QVERIFY(fromMailto(url).isEmpty());
    QVERIFY(!hasRequests(url));
    url.setScheme(QStringLiteral("MAILTO"));
    QCOMPARE(fromMailto(url), QStringList{path});
}

void ComposerAttachmentRequestsTest::absolutePathsAndFileUrls()
{
    const QString path = makeFile(QStringLiteral("forms/file.txt"));
    for (const auto source : {Source::MailtoUrl, Source::CommandLine}) {
        for (const QString &requested : QStringList{path, QUrl::fromLocalFile(path).toString(),
                                         m_dir.path() + QStringLiteral("/forms/../forms/./file.txt")}) {
            const Request request = resolve(requested, source);
            QVERIFY2(request.accepted(), qPrintable(requested + QLatin1String(": ") + request.problem));
            QCOMPARE(request.path, path);
            QCOMPARE(request.requested, requested);
        }
    }
}

void ComposerAttachmentRequestsTest::refusesRemoteAndRelative_data()
{
    QTest::addColumn<QString>("requested");
    QTest::newRow("sftp") << QStringLiteral("sftp://host.example.org/home/user/file.txt");
    QTest::newRow("https") << QStringLiteral("https://example.org/file.txt");
    QTest::newRow("smb") << QStringLiteral("smb://server/share/file.txt");
    QTest::newRow("relative") << QStringLiteral("forms/file.txt");
    QTest::newRow("dot relative") << QStringLiteral("./file.txt");
    QTest::newRow("home") << QStringLiteral("~/file.txt");
    QTest::newRow("empty") << QString();
}

void ComposerAttachmentRequestsTest::refusesRemoteAndRelative()
{
    QFETCH(QString, requested);
    for (const auto source : {Source::MailtoUrl, Source::CommandLine}) {
        const Request request = resolve(requested, source);
        QVERIFY(!request.accepted());
        QVERIFY(request.path.isEmpty());
        QCOMPARE(request.requested, requested);
    }
}

void ComposerAttachmentRequestsTest::refusesMissingFoldersAndLinks()
{
    const QString folder = m_dir.filePath(QStringLiteral("folder"));
    QVERIFY(QDir().mkpath(folder));
    const QString dangling = m_dir.filePath(QStringLiteral("dangling.txt"));
    QVERIFY(QFile::link(m_dir.filePath(QStringLiteral("nothing-here.txt")), dangling));

    for (const auto source : {Source::MailtoUrl, Source::CommandLine}) {
        const Request missing = resolve(m_dir.filePath(QStringLiteral("missing.txt")), source);
        const Request isFolder = resolve(folder, source);
        const Request isFolderUrl = resolve(QUrl::fromLocalFile(folder).toString(), source);
        const Request link = resolve(dangling, source);
        for (const Request &request : {missing, isFolder, isFolderUrl, link}) {
            QVERIFY2(!request.accepted(), qPrintable(request.requested));
            QVERIFY(request.path.isEmpty());
        }
        QCOMPARE(isFolder.problem, isFolderUrl.problem);
        QVERIFY(missing.problem != isFolder.problem);
        QVERIFY(link.problem != missing.problem);
    }
}

void ComposerAttachmentRequestsTest::hiddenFilesOnlyFromCommandLine()
{
    const QString hiddenFile = makeFile(QStringLiteral("hidden/.netrc"));
    const QString inHiddenFolder = makeFile(QStringLiteral("hidden/.ssh/id_ed25519"));
    for (const QString &path : {hiddenFile, inHiddenFolder}) {
        const Request fromUrl = resolve(QUrl::fromLocalFile(path).toString(), Source::MailtoUrl);
        QVERIFY2(!fromUrl.accepted(), qPrintable(path));
        QVERIFY(fromUrl.path.isEmpty());
        const Request fromCommandLine = resolve(path, Source::CommandLine);
        QVERIFY2(fromCommandLine.accepted(), qPrintable(fromCommandLine.problem));
        QCOMPARE(fromCommandLine.path, path);
    }
}

void ComposerAttachmentRequestsTest::linkIntoHiddenFolder()
{
    const QString secret = makeFile(QStringLiteral("linked/.gnupg/private.key"));
    const QString visible = m_dir.filePath(QStringLiteral("linked/holiday.jpg"));
    QVERIFY(QFile::link(secret, visible));
    const QString visibleFolder = m_dir.filePath(QStringLiteral("linked/keys"));
    QVERIFY(QFile::link(QFileInfo(secret).absolutePath(), visibleFolder));

    for (const QString &requested : QStringList{visible, visibleFolder + QStringLiteral("/private.key")}) {
        const Request fromUrl = resolve(requested, Source::MailtoUrl);
        QVERIFY2(!fromUrl.accepted(), qPrintable(requested));
        QVERIFY(fromUrl.path.isEmpty());
        QVERIFY(resolve(requested, Source::CommandLine).accepted());
    }
}

void ComposerAttachmentRequestsTest::systemFiles()
{
    QStringList candidates{QStringLiteral("/etc/hostname"), QStringLiteral("/etc/passwd"), QStringLiteral("/etc/hosts")};
    QString systemFile;
    for (const QString &candidate : std::as_const(candidates)) {
        if (QFileInfo(candidate).isFile() && QFileInfo(candidate).isReadable()) {
            systemFile = candidate;
            break;
        }
    }
    if (!systemFile.isEmpty()) {
        QVERIFY(!resolve(systemFile, Source::MailtoUrl).accepted());
        QVERIFY(!resolve(QUrl::fromLocalFile(systemFile).toString(), Source::MailtoUrl).accepted());
        QVERIFY(resolve(systemFile, Source::CommandLine).accepted());

        // Also through a link in an ordinary folder
        const QString link = m_dir.filePath(QStringLiteral("system-link.txt"));
        QVERIFY(QFile::link(QFileInfo(systemFile).canonicalFilePath(), link));
        QVERIFY(!resolve(link, Source::MailtoUrl).accepted());
    }

    if (QFileInfo::exists(QStringLiteral("/proc/self/environ"))) {
        const Request environment = resolve(QStringLiteral("/proc/self/environ"), Source::MailtoUrl);
        QVERIFY(!environment.accepted());
        QVERIFY(environment.path.isEmpty());
    }

    // Devices are not regular files, whoever asks
    if (QFileInfo::exists(QStringLiteral("/dev/null"))) {
        QVERIFY(!resolve(QStringLiteral("/dev/null"), Source::MailtoUrl).accepted());
        QVERIFY(!resolve(QStringLiteral("/dev/null"), Source::CommandLine).accepted());
    }

    // A name that merely starts like a system folder is not one
    const Request lookalike = resolve(makeFile(QStringLiteral("etcetera/notes.txt")), Source::MailtoUrl);
    QVERIFY2(lookalike.accepted(), qPrintable(lookalike.problem));
}

void ComposerAttachmentRequestsTest::unreadableFile()
{
    const QString path = makeFile(QStringLiteral("unreadable/locked.txt"));
    QVERIFY(QFile::setPermissions(path, QFileDevice::WriteOwner));
    if (QFileInfo(path).isReadable()) {
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        QSKIP("The file stays readable, as it does for the superuser");
    }
    for (const auto source : {Source::MailtoUrl, Source::CommandLine}) {
        const Request request = resolve(path, source);
        QVERIFY(!request.accepted());
        QVERIFY(request.path.isEmpty());
    }
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

void ComposerAttachmentRequestsTest::duplicatesDropped()
{
    const QString path = makeFile(QStringLiteral("twice/file.txt"));
    const QString other = makeFile(QStringLiteral("twice/other.txt"));
    const QString link = m_dir.filePath(QStringLiteral("twice/link.txt"));
    QVERIFY(QFile::link(path, link));
    const QList<Request> resolved = resolveAll({path, QUrl::fromLocalFile(path).toString(), link, other,
                                                QStringLiteral("sftp://host/file.txt"), QStringLiteral("sftp://host/file.txt")},
                                               Source::MailtoUrl);
    QCOMPARE(resolved.size(), 4);
    QCOMPARE(resolved[0].path, path);
    QCOMPARE(resolved[1].path, other);
    // Refused requests are each reported, as they were asked for
    QVERIFY(!resolved[2].accepted());
    QVERIFY(!resolved[3].accepted());
}

QTEST_GUILESS_MAIN(ComposerAttachmentRequestsTest)
