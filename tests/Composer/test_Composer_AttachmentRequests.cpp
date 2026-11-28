/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QUrl>
#include <QUrlQuery>

#include "test_Composer_AttachmentRequests.h"
#include "Common/PrivateFiles.h"
#include "Composer/AttachmentRequests.h"
#include "Composer/MessageComposer.h"
#include "Imap/Parser/MailAddress.h"

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

void ComposerAttachmentRequestsTest::acceptsRemote_data()
{
    QTest::addColumn<QString>("requested");
    QTest::newRow("sftp") << QStringLiteral("sftp://host.example.org/home/user/file.txt");
    QTest::newRow("https") << QStringLiteral("https://example.org/file.txt");
    QTest::newRow("smb") << QStringLiteral("smb://server/share/file.txt");
    QTest::newRow("webdavs with port") << QStringLiteral("webdavs://cloud.example.org:8443/remote.php/f%20g.txt");
    QTest::newRow("upper-case scheme") << QStringLiteral("SFTP://host.example.org/file.txt");
}

void ComposerAttachmentRequestsTest::acceptsRemote()
{
    QFETCH(QString, requested);
    for (const auto source : {Source::MailtoUrl, Source::CommandLine}) {
        const Request request = resolve(requested, source);
        QVERIFY2(request.accepted(), qPrintable(request.problem));
        QVERIFY(request.path.isEmpty());
        QCOMPARE(request.remote, QUrl(requested));
        QCOMPARE(request.requested, requested);
    }
}

void ComposerAttachmentRequestsTest::mailtoRemoteRules_data()
{
    QTest::addColumn<QString>("requested");
    QTest::addColumn<bool>("fromCommandLine");
    // A web page could otherwise reach this computer's own files through a network protocol
    QTest::newRow("localhost") << QStringLiteral("sftp://localhost/etc/passwd") << true;
    QTest::newRow("loopback IPv4") << QStringLiteral("http://127.0.0.1:8080/secret.txt") << true;
    QTest::newRow("loopback IPv6") << QStringLiteral("sftp://[::1]/home/user/file.txt") << true;
    QTest::newRow("any address") << QStringLiteral("ftp://0.0.0.0/file.txt") << true;
    QTest::newRow("localhost subdomain") << QStringLiteral("smb://files.localhost/share/file.txt") << true;
    QTest::newRow("fish to localhost") << QStringLiteral("fish://localhost/home/user/file.txt") << true;
    // Local KIO places and unknown protocols are not other computers
    QTest::newRow("trash") << QStringLiteral("trash:/file.txt") << true;
    QTest::newRow("desktop") << QStringLiteral("desktop:/file.txt") << true;
    QTest::newRow("unknown") << QStringLiteral("gopher://example.org/file.txt") << true;
    QTest::newRow("no host") << QStringLiteral("sftp:///home/user/file.txt") << true;
}

void ComposerAttachmentRequestsTest::mailtoRemoteRules()
{
    QFETCH(QString, requested);
    QFETCH(bool, fromCommandLine);
    const Request fromUrl = resolve(requested, Source::MailtoUrl);
    QVERIFY(!fromUrl.accepted());
    QVERIFY(fromUrl.remote.isEmpty());
    QVERIFY(fromUrl.path.isEmpty());
    const Request typed = resolve(requested, Source::CommandLine);
    QCOMPARE(typed.accepted(), fromCommandLine);
    QCOMPARE(typed.remote.isEmpty(), !fromCommandLine);
}

void ComposerAttachmentRequestsTest::notFiles()
{
    for (const QString &requested : {QStringLiteral("mailto:someone@example.org"), QStringLiteral("data:text/plain,hello"),
                                     QStringLiteral("javascript:alert(1)"), QStringLiteral("about:blank"),
                                     QStringLiteral("cid:part1@example.org"), QStringLiteral("tel:+15551234"),
                                     QStringLiteral("sftp://host.example.org")}) {
        for (const auto source : {Source::MailtoUrl, Source::CommandLine}) {
            const Request request = resolve(requested, source);
            QVERIFY2(!request.accepted(), qPrintable(requested));
            QVERIFY(request.remote.isEmpty());
        }
        QVERIFY(!isFetchable(QUrl(requested)));
    }
    QVERIFY(!isFetchable(QUrl::fromLocalFile(QStringLiteral("/tmp/file.txt"))));
    QVERIFY(isFetchable(QUrl(QStringLiteral("trash:/file.txt"))));
}

void ComposerAttachmentRequestsTest::displayUrlHidesCredentials()
{
    const QString shown = displayUrl(QUrl(QStringLiteral("sftp://alice:hunter2@host.example.org:2222/docs/a%20b.txt?token=t0ps3cret#part")));
    QCOMPARE(shown, QStringLiteral("sftp://host.example.org:2222/docs/a b.txt"));
    for (const char *secret : {"alice", "hunter2", "t0ps3cret", "part"})
        QVERIFY2(!shown.contains(QLatin1String(secret)), secret);
    QCOMPARE(displayUrl(QUrl::fromLocalFile(QStringLiteral("/tmp/a b.txt"))), QStringLiteral("/tmp/a b.txt"));

    // Refused requests are reported without them as well
    const Request refused = resolve(QStringLiteral("sftp://alice:hunter2@localhost/etc/shadow"), Source::MailtoUrl);
    QVERIFY(!refused.accepted());
    QVERIFY(!refused.problem.contains(QLatin1String("hunter2")));
}

void ComposerAttachmentRequestsTest::refusesRelative_data()
{
    QTest::addColumn<QString>("requested");
    QTest::newRow("relative") << QStringLiteral("forms/file.txt");
    QTest::newRow("dot relative") << QStringLiteral("./file.txt");
    QTest::newRow("home") << QStringLiteral("~/file.txt");
    QTest::newRow("empty") << QString();
}

void ComposerAttachmentRequestsTest::refusesRelative()
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
                                                QStringLiteral("sftp://host/file.txt"), QStringLiteral("sftp://host/dir/../file.txt"),
                                                QStringLiteral("sftp://localhost/file.txt"), QStringLiteral("sftp://localhost/file.txt")},
                                               Source::MailtoUrl);
    QCOMPARE(resolved.size(), 5);
    QCOMPARE(resolved[0].path, path);
    QCOMPARE(resolved[1].path, other);
    QCOMPARE(resolved[2].remote, QUrl(QStringLiteral("sftp://host/file.txt")));
    // Refused requests are each reported, as they were asked for
    QVERIFY(!resolved[3].accepted());
    QVERIFY(!resolved[4].accepted());
}

namespace {

std::unique_ptr<Composer::MessageComposer> makeComposer()
{
    auto composer = std::make_unique<Composer::MessageComposer>(nullptr);
    composer->setFrom(Imap::Message::MailAddress(QString(), QString(), QStringLiteral("me"), QStringLiteral("example.org")));
    composer->setSubject(QStringLiteral("files"));
    composer->setText(QStringLiteral("Attached"));
    composer->setReportImpresarioVersions(false);
    return composer;
}

QByteArray serialize(const Composer::MessageComposer &composer)
{
    QByteArray message;
    QBuffer buffer(&message);
    buffer.open(QIODevice::WriteOnly);
    QString error;
    if (!composer.asRawMessage(&buffer, &error))
        qWarning() << "Serialization failed:" << error;
    return message;
}

}

void ComposerAttachmentRequestsTest::remoteAttachmentCopies()
{
    auto composer = makeComposer();
    QSignalSpy added(composer.get(), &Composer::MessageComposer::remoteFileAttachmentAdded);
    const QUrl source(QStringLiteral("sftp://alice:hunter2@host.example.org/docs/annual%20report.txt?token=t0ps3cret"));
    QString error;
    const QString copy = composer->addRemoteFileAttachment(source, &error);
    QVERIFY2(!copy.isEmpty(), qPrintable(error));
    QCOMPARE(added.size(), 1);
    QCOMPARE(added[0][0].toUrl(), source);
    QCOMPARE(added[0][1].toString(), copy);

    // A private copy, in a private folder, which the source is never written to
    const QFileInfo info(copy);
    QCOMPARE(info.fileName(), QStringLiteral("annual report.txt"));
    // Each copy has a folder of its own, so that files of the same name do not collide
    QCOMPARE(QFileInfo(QFileInfo(info.absolutePath()).absolutePath()).canonicalFilePath(),
             QFileInfo(Common::PrivateFiles::fetchedAttachmentsFolder()).canonicalFilePath());
    QCOMPARE(info.permissions() & ~(QFileDevice::ReadUser | QFileDevice::WriteUser),
             QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QCOMPARE(QFileInfo(info.absolutePath()).permissions() & (QFileDevice::ReadGroup | QFileDevice::ReadOther
                                                             | QFileDevice::ExeGroup | QFileDevice::ExeOther),
             QFileDevice::Permissions());

    // Shown as being fetched, without the credentials of its URL, and not sent before it is complete
    const QModelIndex index = composer->index(0, 0);
    QVERIFY(index.data(Qt::DisplayRole).toString().contains(QLatin1String("annual report.txt")));
    QVERIFY(index.data(Qt::DisplayRole).toString().contains(QLatin1String("fetching")));
    const QString tooltip = index.data(Qt::ToolTipRole).toString();
    QVERIFY(tooltip.contains(QLatin1String("host.example.org")));
    for (const char *secret : {"alice", "hunter2", "t0ps3cret"})
        QVERIFY2(!tooltip.contains(QLatin1String(secret)), secret);
    QVERIFY(!composer->isReadyForSerialization());

    {
        QFile file(copy);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::ExistingOnly));
        file.write("fetched content\n");
    }
    QSignalSpy changed(composer.get(), &QAbstractItemModel::dataChanged);
    QVERIFY(composer->setRemoteFileFetched(copy));
    QCOMPARE(changed.size(), 1);
    QVERIFY(composer->isReadyForSerialization());
    QCOMPARE(index.data(Qt::DisplayRole).toString(), QStringLiteral("annual report.txt"));
    QVERIFY(!index.data(Qt::ToolTipRole).toString().contains(QLatin1String("hunter2")));
    const QByteArray message = serialize(*composer);
    QVERIFY(message.contains("annual report.txt") || message.contains("annual%20report.txt"));
    QVERIFY(message.contains("fetched content") || message.contains(QByteArray("fetched content\n").toBase64()));
    QVERIFY(!message.contains("hunter2"));

    QVERIFY(!composer->setRemoteFileFetched(copy + QStringLiteral(".other")));
    composer.reset();
    QVERIFY(!QFile::exists(copy));
}

void ComposerAttachmentRequestsTest::remoteAttachmentRemoved()
{
    auto composer = makeComposer();
    QSignalSpy removed(composer.get(), &Composer::MessageComposer::remoteFileAttachmentRemoved);
    const QString local = makeFile(QStringLiteral("remote/local.txt"));
    QVERIFY(composer->addFileAttachment(local));
    const QString first = composer->addRemoteFileAttachment(QUrl(QStringLiteral("smb://server/share/same.txt")));
    const QString second = composer->addRemoteFileAttachment(QUrl(QStringLiteral("ftp://other.example.org/same.txt")));
    QVERIFY(!first.isEmpty());
    QVERIFY(!second.isEmpty());
    // Files of the same name get copies of their own
    QVERIFY(first != second);
    QCOMPARE(composer->rowCount(), 3);

    composer->removeAttachment(composer->index(1, 0));
    QVERIFY(!QFile::exists(first));
    QCOMPARE(removed.size(), 1);
    QCOMPARE(removed[0][0].toString(), first);

    composer->removeRemoteFileAttachment(second);
    QVERIFY(!QFile::exists(second));
    QCOMPARE(removed.size(), 2);
    QCOMPARE(composer->rowCount(), 1);

    // Local files are never removed with their attachment
    composer->removeAttachment(composer->index(0, 0));
    QCOMPARE(removed.size(), 2);
    QVERIFY(QFile::exists(local));
}

void ComposerAttachmentRequestsTest::remoteAttachmentFetchedByCaller()
{
    // Files from Nextcloud are fetched by the window, not through the URL
    auto composer = makeComposer();
    QSignalSpy added(composer.get(), &Composer::MessageComposer::remoteFileAttachmentAdded);
    QSignalSpy removed(composer.get(), &Composer::MessageComposer::remoteFileAttachmentRemoved);
    const QString copy = composer->addRemoteFileAttachment(
        QUrl(QStringLiteral("https://cloud.example.org/remote.php/dav/files/alice/report.pdf")), nullptr, false);
    QVERIFY(!copy.isEmpty());
    QCOMPARE(added.size(), 0);
    QCOMPARE(composer->rowCount(), 1);
    QVERIFY(!composer->isReadyForSerialization());
    composer->removeRemoteFileAttachment(copy);
    QCOMPARE(removed.size(), 1);
    QVERIFY(!QFile::exists(copy));
    QVERIFY(!composer->setRemoteFileFetched(copy));
}

QTEST_GUILESS_MAIN(ComposerAttachmentRequestsTest)
