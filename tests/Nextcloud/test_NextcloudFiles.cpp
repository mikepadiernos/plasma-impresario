/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>

#include "Nextcloud/Account.h"
#include "Nextcloud/Files.h"
#include "Nextcloud/OnlineAccounts.h"
#include "Nextcloud/Shares.h"

using namespace Nextcloud;

class NextcloudFilesTest : public QObject
{
    Q_OBJECT
private slots:
    void serverAddresses_data();
    void serverAddresses();
    void paths();
    void fileUrls();
    void fileNames();
    void listing();
    void listingInSubfolderInstall();
    void brokenListings();
    void sharesParse();
    void sharesFailure();
    void tokens_data();
    void tokens();
    void linksInText();
    void descriptions();
    void onlineAccounts();
};

void NextcloudFilesTest::serverAddresses_data()
{
    QTest::addColumn<QString>("typed");
    QTest::addColumn<QString>("expected");
    QTest::newRow("bare host") << QStringLiteral("cloud.example.com") << QStringLiteral("https://cloud.example.com");
    QTest::newRow("trailing slash") << QStringLiteral("https://cloud.example.com/")
                                    << QStringLiteral("https://cloud.example.com");
    QTest::newRow("subfolder") << QStringLiteral("https://example.com/nextcloud/")
                               << QStringLiteral("https://example.com/nextcloud");
    QTest::newRow("copied page") << QStringLiteral("https://example.com/nextcloud/index.php/apps/files/?dir=/Docs")
                                 << QStringLiteral("https://example.com/nextcloud");
    QTest::newRow("login page") << QStringLiteral("https://cloud.example.com/login?redirect_url=x")
                                << QStringLiteral("https://cloud.example.com");
    QTest::newRow("port") << QStringLiteral("cloud.example.com:8443")
                          << QStringLiteral("https://cloud.example.com:8443");
    QTest::newRow("local http") << QStringLiteral("http://localhost:8080") << QStringLiteral("http://localhost:8080");
    QTest::newRow("remote http") << QStringLiteral("http://cloud.example.com") << QString();
    QTest::newRow("user info") << QStringLiteral("https://alice:secret@cloud.example.com") << QString();
    QTest::newRow("other scheme") << QStringLiteral("ftp://cloud.example.com") << QString();
    QTest::newRow("empty") << QStringLiteral("  ") << QString();
}

void NextcloudFilesTest::serverAddresses()
{
    QFETCH(QString, typed);
    QFETCH(QString, expected);
    QString error;
    const QUrl url = normalizedServer(typed, &error);
    if (expected.isEmpty()) {
        QVERIFY(!url.isValid() || url.isEmpty());
        QVERIFY(!error.isEmpty());
    } else {
        QCOMPARE(url.toString(), expected);
    }
}

void NextcloudFilesTest::paths()
{
    QCOMPARE(cleanPath(QString()), QStringLiteral("/"));
    QCOMPARE(cleanPath(QStringLiteral("Docs//Reports/")), QStringLiteral("/Docs/Reports"));
    QCOMPARE(cleanPath(QStringLiteral("/Docs/../../etc/./passwd")), QStringLiteral("/etc/passwd"));
    QCOMPARE(parentPath(QStringLiteral("/Docs/a.txt")), QStringLiteral("/Docs"));
    QCOMPARE(parentPath(QStringLiteral("/a.txt")), QStringLiteral("/"));
    QCOMPARE(parentPath(QStringLiteral("/")), QStringLiteral("/"));
    QCOMPARE(childPath(QStringLiteral("/"), QStringLiteral("a.txt")), QStringLiteral("/a.txt"));
    QCOMPARE(childPath(QStringLiteral("/Docs/"), QStringLiteral("a.txt")), QStringLiteral("/Docs/a.txt"));
}

void NextcloudFilesTest::fileUrls()
{
    Account account;
    account.id = QStringLiteral("nc1");
    account.server = QUrl(QStringLiteral("https://example.com/nextcloud"));
    account.loginName = QStringLiteral("alice@example.com");
    account.userId = QStringLiteral("alice");
    QVERIFY(account.isValid());
    QCOMPARE(account.davPath(), QStringLiteral("/nextcloud/remote.php/dav/files/alice"));
    QCOMPARE(account.fileUrl(QStringLiteral("/")).toString(QUrl::FullyEncoded),
        QStringLiteral("https://example.com/nextcloud/remote.php/dav/files/alice/"));
    // Characters which mean something in URLs stay part of the name
    QCOMPARE(account.fileUrl(QStringLiteral("/Q&A #1/50% done?.txt")).toString(QUrl::FullyEncoded),
        QStringLiteral("https://example.com/nextcloud/remote.php/dav/files/"
                       "alice/Q&A%20%231/50%25%20done%3F.txt"));
    QCOMPARE(account.webFolderUrl(QStringLiteral("/Q&A")).toString(QUrl::FullyEncoded),
        QStringLiteral("https://example.com/nextcloud/index.php/apps/files/?dir=/Q%26A"));
    QCOMPARE(account.title(), QStringLiteral("alice@example.com on example.com"));
    account.displayName = QStringLiteral("Alice");
    QCOMPARE(account.title(), QStringLiteral("Alice on example.com"));
}

void NextcloudFilesTest::fileNames()
{
    QCOMPARE(safeFileName(QStringLiteral("../../.bashrc")), QStringLiteral(".._.._.bashrc"));
    QCOMPARE(safeFileName(QStringLiteral("a\\b/c\nd.pdf")), QStringLiteral("a_b_c_d.pdf"));
    QCOMPARE(safeFileName(QStringLiteral("..")), QStringLiteral("Attachment"));
    QCOMPARE(safeFileName(QStringLiteral("  ")), QStringLiteral("Attachment"));
    QCOMPARE(numberedName(QStringLiteral("report.pdf"), 2), QStringLiteral("report (2).pdf"));
    QCOMPARE(numberedName(QStringLiteral("archive.tar.gz"), 3), QStringLiteral("archive.tar (3).gz"));
    QCOMPARE(numberedName(QStringLiteral(".hidden"), 2), QStringLiteral(".hidden (2)"));
    QCOMPARE(numberedName(QStringLiteral("README"), 2), QStringLiteral("README (2)"));
}

static QByteArray sampleListing(const QByteArray &root)
{
    return R"(<?xml version="1.0"?>
<d:multistatus xmlns:d="DAV:" xmlns:s="http://sabredav.org/ns" xmlns:oc="http://owncloud.org/ns" xmlns:nc="http://nextcloud.org/ns">
 <d:response><d:href>)"
        + root + R"(/Docs/</d:href>
  <d:propstat><d:prop><d:resourcetype><d:collection/></d:resourcetype><oc:permissions>RGDNVCK</oc:permissions><oc:fileid>10</oc:fileid><oc:size>300</oc:size><oc:share-types/></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>
  <d:propstat><d:prop><d:getcontentlength/><d:getcontenttype/></d:prop><d:status>HTTP/1.1 404 Not Found</d:status></d:propstat>
 </d:response>
 <d:response><d:href>)"
        + root + R"(/Docs/report%202026.pdf</d:href>
  <d:propstat><d:prop><d:resourcetype/><d:getcontentlength>200</d:getcontentlength><d:getcontenttype>application/pdf</d:getcontenttype>
   <d:getlastmodified>Fri, 27 Nov 2026 10:00:00 GMT</d:getlastmodified><d:getetag>"abc"</d:getetag><oc:fileid>11</oc:fileid>
   <oc:permissions>SRGDNVW</oc:permissions><oc:share-types><oc:share-type>3</oc:share-type><oc:share-type>0</oc:share-type></oc:share-types></d:prop>
   <d:status>HTTP/1.1 200 OK</d:status></d:propstat>
 </d:response>
 <d:response><d:href>)"
        + root + R"(/Docs/Shared%20with%20me/</d:href>
  <d:propstat><d:prop><d:resourcetype><d:collection/></d:resourcetype><oc:permissions>SRG</oc:permissions><oc:fileid>12</oc:fileid></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>
 </d:response>
 <d:response><d:href>)"
        + root + R"(/Docs/10.txt</d:href>
  <d:propstat><d:prop><d:resourcetype/><d:getcontentlength>5</d:getcontentlength><oc:permissions>RGDNVW</oc:permissions></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>
 </d:response>
 <d:response><d:href>)"
        + root + R"(/Docs/9.txt</d:href>
  <d:propstat><d:prop><d:resourcetype/><d:getcontentlength>5</d:getcontentlength></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>
 </d:response>
 <d:response><d:href>/remote.php/dav/files/mallory/Docs/evil.txt</d:href>
  <d:propstat><d:prop><d:resourcetype/></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat>
 </d:response>
</d:multistatus>)";
}

void NextcloudFilesTest::listing()
{
    Listing listing;
    QString error;
    QVERIFY2(parseListing(sampleListing("/remote.php/dav/files/alice"), QStringLiteral("/remote.php/dav/files/alice"),
                 QStringLiteral("/Docs"), &listing, &error),
        qPrintable(error));
    QCOMPARE(listing.folder.path, QStringLiteral("/Docs"));
    QVERIFY(listing.folder.folder);
    QVERIFY(listing.folder.permissions.canCreateFile());
    QCOMPARE(listing.folder.size, 300);
    QCOMPARE(listing.children.size(), 4);
    // Folders first, then numbers in numeric order; the other user's file is left
    // out
    QCOMPARE(listing.children[0].name, QStringLiteral("Shared with me"));
    QCOMPARE(listing.children[1].name, QStringLiteral("9.txt"));
    QCOMPARE(listing.children[2].name, QStringLiteral("10.txt"));
    const Entry report = listing.children[3];
    QCOMPARE(report.path, QStringLiteral("/Docs/report 2026.pdf"));
    QVERIFY(!report.folder);
    QCOMPARE(report.size, 200);
    QCOMPARE(report.contentType, QStringLiteral("application/pdf"));
    QCOMPARE(report.modified, QDateTime(QDate(2026, 11, 27), QTime(10, 0), QTimeZone::UTC));
    QCOMPARE(report.fileId, 11);
    QVERIFY(report.permissions.isShared());
    QVERIFY(report.permissions.canWrite());
    QVERIFY(report.permissions.canShare());
    QCOMPARE(report.shareTypes, (QList<int>{3, 0}));
    QVERIFY(report.hasLinkShare());

    const Entry shared = listing.children[0];
    QVERIFY(shared.folder);
    QVERIFY(shared.permissions.isKnown());
    QVERIFY(!shared.permissions.canCreateFile());
    QVERIFY(listing.children[2].permissions.isKnown());
    QVERIFY(!listing.children[1].permissions.isKnown());
}

void NextcloudFilesTest::listingInSubfolderInstall()
{
    Listing listing;
    QString error;
    QVERIFY2(parseListing(sampleListing("https://example.com/nextcloud/remote.php/dav/files/alice"),
                 QStringLiteral("/nextcloud/remote.php/dav/files/alice/"), QStringLiteral("Docs/"), &listing, &error),
        qPrintable(error));
    QCOMPARE(listing.children.size(), 4);
}

void NextcloudFilesTest::brokenListings()
{
    Listing listing;
    QString error;
    QVERIFY(!parseListing("<html>Maintenance</html>", QStringLiteral("/remote.php/dav/files/alice"),
        QStringLiteral("/"), &listing, &error));
    QVERIFY(!error.isEmpty());
    error.clear();
    QVERIFY(!parseListing(sampleListing("/remote.php/dav/files/alice"), QStringLiteral("/remote.php/dav/files/alice"),
        QStringLiteral("/Elsewhere"), &listing, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(propfindBody().contains("oc:permissions"));
}

void NextcloudFilesTest::sharesParse()
{
    const QByteArray json =
        R"({"ocs":{"meta":{"status":"ok","statuscode":200,"message":"OK"},"data":[
        {"id":"7","share_type":3,"path":"/Docs/report.pdf","item_type":"file","token":"AbCdEfGh1234",
         "url":"https://cloud.example.com/s/AbCdEfGh1234","permissions":1,"expiration":"2026-12-31 00:00:00",
         "password":"$2y$10$x","hide_download":1,"uid_owner":"alice","label":"Client"},
        {"id":8,"share_type":0,"path":"/Docs","item_type":"folder","permissions":31,"share_with":"bob",
         "share_with_displayname":"Bob","expiration":null,"password":null,"hide_download":0},
        {"id":"9","share_type":3,"path":"/Broken","item_type":"file","permissions":1}
    ]}})";
    QList<Share> shares;
    QString error;
    QVERIFY2(parseShares(json, &shares, &error), qPrintable(error));
    QCOMPARE(shares.size(), 2);
    const Share link = shares[0];
    QCOMPARE(link.id, QStringLiteral("7"));
    QVERIFY(link.isLink());
    QCOMPARE(link.url, QUrl(QStringLiteral("https://cloud.example.com/s/AbCdEfGh1234")));
    QCOMPARE(link.expiration, QDate(2026, 12, 31));
    QVERIFY(link.hasPassword);
    QVERIFY(link.hideDownload);
    QCOMPARE(link.label, QStringLiteral("Client"));
    QVERIFY(!allowsChanges(link));
    const Share user = shares[1];
    QCOMPARE(user.id, QStringLiteral("8"));
    QVERIFY(!user.isLink());
    QVERIFY(user.folder);
    QVERIFY(!user.hasPassword);
    QVERIFY(!user.expiration.isValid());
    QCOMPARE(user.shareWithName, QStringLiteral("Bob"));
    QVERIFY(allowsChanges(user));

    QCOMPARE(findLinkShare(shares, QStringLiteral("AbCdEfGh1234")), &shares[0]);
    QCOMPARE(findLinkShare(shares, QStringLiteral("Other")), nullptr);
    QCOMPARE(findLinkShare(shares, QString()), nullptr);

    // OCS v1 reports success as 100, and a single object instead of a list
    QVERIFY(parseShares(
        R"({"ocs":{"meta":{"statuscode":100},"data":{"id":1,"share_type":3,"url":"https://a/s/12345678","token":"12345678"}}})",
        &shares, &error));
    QCOMPARE(shares.size(), 1);
}

void NextcloudFilesTest::sharesFailure()
{
    QList<Share> shares;
    QString error;
    QVERIFY(!parseShares(R"({"ocs":{"meta":{"statuscode":404,"message":"Wrong path"},"data":[]}})", &shares, &error));
    QCOMPARE(error, QStringLiteral("Wrong path"));
    QVERIFY(!parseShares("not json", &shares, &error));
    QVERIFY(!error.isEmpty());
}

void NextcloudFilesTest::tokens_data()
{
    QTest::addColumn<QString>("link");
    QTest::addColumn<QString>("server");
    QTest::addColumn<QString>("token");
    const QString server = QStringLiteral("https://cloud.example.com");
    QTest::newRow("short") << QStringLiteral("https://cloud.example.com/s/AbCdEfGh1234") << server
                           << QStringLiteral("AbCdEfGh1234");
    QTest::newRow("index.php") << QStringLiteral("https://cloud.example.com/index.php/s/AbCdEfGh1234") << server
                               << QStringLiteral("AbCdEfGh1234");
    QTest::newRow("download") << QStringLiteral("https://cloud.example.com/s/AbCdEfGh1234/download") << server
                              << QStringLiteral("AbCdEfGh1234");
    QTest::newRow("subfolder") << QStringLiteral("https://example.com/nc/index.php/s/AbCdEfGh1234")
                               << QStringLiteral("https://example.com/nc") << QStringLiteral("AbCdEfGh1234");
    QTest::newRow("explicit port") << QStringLiteral("https://cloud.example.com:443/s/AbCdEfGh1234") << server
                                   << QStringLiteral("AbCdEfGh1234");
    QTest::newRow("other host") << QStringLiteral("https://evil.example.com/s/AbCdEfGh1234") << server << QString();
    QTest::newRow("lookalike host") << QStringLiteral("https://cloud.example.com.evil.net/s/AbCdEfGh1234") << server
                                    << QString();
    QTest::newRow("http") << QStringLiteral("http://cloud.example.com/s/AbCdEfGh1234") << server << QString();
    QTest::newRow("outside subfolder") << QStringLiteral("https://example.com/other/s/AbCdEfGh1234")
                                       << QStringLiteral("https://example.com/nc") << QString();
    QTest::newRow("not a share") << QStringLiteral("https://cloud.example.com/apps/files/") << server << QString();
    QTest::newRow("short token") << QStringLiteral("https://cloud.example.com/s/abc") << server << QString();
}

void NextcloudFilesTest::tokens()
{
    QFETCH(QString, link);
    QFETCH(QString, server);
    QFETCH(QString, token);
    QCOMPARE(shareToken(QUrl(link), QUrl(server)), token);
}

void NextcloudFilesTest::linksInText()
{
    const QString text = QStringLiteral("See https://cloud.example.com/s/AbCdEfGh1234. Also "
                                        "(https://example.com/nc/index.php/s/ZyXwVuTs9876/download), "
                                        "again https://cloud.example.com/s/AbCdEfGh1234 and "
                                        "https://cloud.example.com/apps/files/ "
                                        "and <https://x.example/s/Qwertyui12>");
    const QList<QUrl> links = findShareLinks(text);
    QCOMPARE(links.size(), 3);
    QCOMPARE(links[0], QUrl(QStringLiteral("https://cloud.example.com/s/AbCdEfGh1234")));
    QCOMPARE(links[1], QUrl(QStringLiteral("https://example.com/nc/index.php/s/ZyXwVuTs9876/download")));
    QCOMPARE(links[2], QUrl(QStringLiteral("https://x.example/s/Qwertyui12")));
    QVERIFY(findShareLinks(QStringLiteral("https://alice:pw@cloud.example.com/s/AbCdEfGh1234")).isEmpty());
}

void NextcloudFilesTest::descriptions()
{
    QCOMPARE(visibility(nullptr), Visibility::Unknown);
    QVERIFY(describe(nullptr).join(QLatin1Char(' ')).contains(QLatin1String("unknown")));

    Share share;
    share.type = Share::PublicLink;
    share.permissions = Share::Read;
    QCOMPARE(visibility(&share), Visibility::Public);
    QStringList lines = describe(&share);
    QCOMPARE(lines.value(0), QStringLiteral("Anyone with the link can view this file."));
    QVERIFY(lines.contains(QStringLiteral("The link has no password.")));
    QVERIFY(lines.contains(QStringLiteral("The link does not expire.")));

    share.permissions = Share::Read | Share::Update;
    share.hasPassword = true;
    share.expiration = QDate(2026, 12, 31);
    QCOMPARE(visibility(&share), Visibility::PasswordProtected);
    lines = describe(&share);
    QCOMPARE(lines.value(0),
        QStringLiteral("Anyone with the link and its password can view and "
                       "edit this file."));
    QVERIFY(lines.join(QLatin1Char(' ')).contains(QLatin1String("not included in the message")));
    QVERIFY(lines.join(QLatin1Char(' ')).contains(QLatin1String("expires on")));

    share.folder = true;
    share.hasPassword = false;
    share.permissions = Share::Create;
    QCOMPARE(describe(&share).value(0),
        QStringLiteral("Anyone with the link can upload files to this "
                       "folder, without seeing its contents."));
    share.permissions = Share::Read | Share::Update | Share::Create | Share::Delete;
    QCOMPARE(describe(&share).value(0),
        QStringLiteral("Anyone with the link can view, upload, change and "
                       "delete files in this folder."));
    share.permissions = Share::Read;
    share.hideDownload = true;
    lines = describe(&share);
    QCOMPARE(lines.value(0), QStringLiteral("Anyone with the link can view the files in this folder."));
    QVERIFY(lines.contains(QStringLiteral("The download button is hidden on the share's page.")));
}

void NextcloudFilesTest::onlineAccounts()
{
    // As the Nextcloud provider of KDE Online Accounts stores it
    const QVariantMap settings{{QStringLiteral("server"), QStringLiteral("https://example.com/nextcloud/")},
        {QStringLiteral("dav/host"), QStringLiteral("example.com")},
        {QStringLiteral("dav/storagePath"), QStringLiteral("/remote.php/dav/files/alice%40example.com")}};
    Account account = onlineAccount(12, QStringLiteral("alice@example.com"), settings);
    QVERIFY(account.isValid());
    QCOMPARE(account.id, onlineAccountKey(12));
    QCOMPARE(account.id, QStringLiteral("online-12"));
    QCOMPARE(account.source, Account::Source::OnlineAccounts);
    QCOMPARE(account.onlineAccountId, 12u);
    QCOMPARE(account.server, QUrl(QStringLiteral("https://example.com/nextcloud")));
    QCOMPARE(account.loginName, QStringLiteral("alice@example.com"));
    QCOMPARE(account.userId, QStringLiteral("alice@example.com"));
    QCOMPARE(account.fileUrl(QStringLiteral("/a.txt")).path(),
        QStringLiteral("/nextcloud/remote.php/dav/files/alice@example.com/a.txt"));

    // Only the host: https is assumed, and the user name stands in for the user ID
    account
        = onlineAccount(3, QStringLiteral("bob"), {{QStringLiteral("dav/host"), QStringLiteral("cloud.example.com")}});
    QVERIFY(account.isValid());
    QCOMPARE(account.server, QUrl(QStringLiteral("https://cloud.example.com")));
    QCOMPARE(account.userId, QStringLiteral("bob"));

    // A storage path below the user's folder does not name the user
    account = onlineAccount(4, QStringLiteral("bob"),
        {{QStringLiteral("server"), QStringLiteral("https://cloud.example.com")},
            {QStringLiteral("dav/storagePath"), QStringLiteral("/remote.php/dav/files/carol/Documents")}});
    QCOMPARE(account.userId, QStringLiteral("bob"));

    // Nothing to sign in to, no user, or no TLS
    QVERIFY(!onlineAccount(5, QStringLiteral("bob"), {}).isValid());
    QVERIFY(!onlineAccount(6, QString(), {{QStringLiteral("server"), QStringLiteral("https://cloud.example.com")}})
            .isValid());
    QVERIFY(!onlineAccount(
        7, QStringLiteral("bob"), {{QStringLiteral("server"), QStringLiteral("http://cloud.example.com")}})
            .isValid());
}

QTEST_GUILESS_MAIN(NextcloudFilesTest)

#include "test_NextcloudFiles.moc"
