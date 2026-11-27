/* Copyright (C) 2026 Plasma Mail contributors

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License or (at your option)
   version 3 or any later version accepted by the membership of KDE e.V.
   (or its successor approved by the membership of KDE e.V.).
*/
#include "Gui/WebEngineMessagePage.h"
#include "Gui/WebEngineMessageProfile.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "UiUtils/PlainTextFormatter.h"

#include <QApplication>
#include <QBuffer>
#include <QDesktopServices>
#include <QImage>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryFile>
#include <QTest>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include <memory>

class ExternalUrlHandler : public QObject
{
    Q_OBJECT
public:
    int calls = 0;
public slots:
    void handleUrl(const QUrl &) { ++calls; }
};

class MessagePart : public QStandardItem
{
public:
    QVariant data(int role = Qt::UserRole + 1) const override
    {
        if (role == Imap::Mailbox::RolePartData)
            ++fetchRequests;
        return QStandardItem::data(role);
    }

    mutable int fetchRequests = 0;
};

class MessageModel
{
public:
    MessageModel()
    {
        message = new QStandardItem;
        model.appendRow(message);
        body = addPart(QStringLiteral("text/html"), &bodyData);
        image = addPart(QStringLiteral("image/png"), &imageData);
        image->setData(QByteArrayLiteral("<logo@example>"), Imap::Mailbox::RolePartBodyFldId);
        QImage picture(1, 1, QImage::Format_RGB32);
        picture.fill(Qt::red);
        QBuffer buffer(&imageData);
        buffer.open(QIODevice::WriteOnly);
        picture.save(&buffer, "PNG");
    }

    MessagePart *addPart(const QString &mime, QByteArray *data)
    {
        auto *part = new MessagePart;
        part->setData(mime, Imap::Mailbox::RolePartMimeType);
        part->setData(QVariant::fromValue(data), Imap::Mailbox::RolePartBufferPtr);
        part->setData(true, Imap::Mailbox::RoleIsFetched);
        message->appendRow(part);
        return part;
    }

    QStandardItemModel model;
    QStandardItem *message = nullptr;
    MessagePart *body = nullptr;
    MessagePart *image = nullptr;
    QByteArray bodyData;
    QByteArray imageData;
};

class WebEngineMessageProfileTest : public QObject
{
    Q_OBJECT
private slots:
    void delegatesLinksWithoutNavigating()
    {
        MessageModel message;
        message.bodyData = QByteArrayLiteral(
            "<html><body><a id=\"web\" href=\"https://example.org/path\">Web</a>"
            "<a id=\"mail\" href=\"mailto:someone@example.org\">Mail</a>"
            "<a id=\"file\" href=\"file:///etc/passwd\">File</a>"
            "<a id=\"part\" href=\"trojita-imap://msg/1\">Other part</a>"
            "<a id=\"fake\" href=\"https://evil.test/login\"> https://bank.example.com/login </a>"
            "<a id=\"same\" href=\"https://evil.test/login\">Second</a></body></html>");
        Gui::WebEngineMessageProfile profile(message.message->index());
        Gui::WebEngineMessagePage page(&profile);
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        QSignalSpy links(&page, &Gui::WebEngineMessagePage::externalLinkRequested);
        QVERIFY(!page.loadMessagePart(QUrl(QStringLiteral("https://example.org"))));
        QVERIFY(!page.loadMessagePart(QUrl(QStringLiteral("trojita-imap://msg/0?query"))));
        QVERIFY(page.loadMessagePart(QUrl(QStringLiteral("trojita-imap://msg/0"))));
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 15000);
        QVERIFY(finished.last().at(0).toBool());

        page.runJavaScript(QStringLiteral("document.getElementById('web').click()"), QWebEngineScript::ApplicationWorld);
        QTRY_COMPARE_WITH_TIMEOUT(links.size(), 1, 10000);
        QCOMPARE(links.at(0).at(0).toUrl(), QUrl(QStringLiteral("https://example.org/path")));
        QCOMPARE(links.at(0).at(1).toString(), QStringLiteral("Web"));
        page.runJavaScript(QStringLiteral("document.getElementById('mail').click()"), QWebEngineScript::ApplicationWorld);
        QTRY_COMPARE_WITH_TIMEOUT(links.size(), 2, 10000);
        QCOMPARE(links.at(1).at(0).toUrl(), QUrl(QStringLiteral("mailto:someone@example.org")));
        QVERIFY(links.at(1).at(1).toString().isEmpty());
        page.runJavaScript(QStringLiteral("document.getElementById('file').click(); document.getElementById('part').click()"),
                           QWebEngineScript::ApplicationWorld);
        QTest::qWait(300);
        QCOMPARE(links.size(), 2);
        QCOMPARE(page.url(), QUrl(QStringLiteral("trojita-imap://msg/0")));

        // The text of the clicked link comes along, even when another link has the same target
        page.runJavaScript(QStringLiteral("document.getElementById('same').focus(); document.getElementById('fake').click()"),
                           QWebEngineScript::ApplicationWorld);
        QTRY_COMPARE_WITH_TIMEOUT(links.size(), 3, 10000);
        QCOMPARE(links.at(2).at(0).toUrl(), QUrl(QStringLiteral("https://evil.test/login")));
        QCOMPARE(links.at(2).at(1).toString().trimmed(), QStringLiteral("https://bank.example.com/login"));
        QCOMPARE(page.url(), QUrl(QStringLiteral("trojita-imap://msg/0")));

        // The text of the link under the mouse is found too, before anything is clicked
        const auto linkText = [&page](const QString &href, Gui::WebEngineMessagePage::LinkSource source) {
            QString result;
            bool done = false;
            page.linkText(href, source, [&](const QString &text) {
                result = text;
                done = true;
            });
            if (!QTest::qWaitFor([&done]() { return done; }, 10000))
                return QStringLiteral("(no answer)");
            return result.trimmed();
        };
        const QString evil = QStringLiteral("https://evil.test/login");
        QCOMPARE(linkText(evil, Gui::WebEngineMessagePage::LinkSource::Clicked), QStringLiteral("https://bank.example.com/login"));
        page.runJavaScript(QStringLiteral("document.getElementById('same').dispatchEvent("
                                          "new MouseEvent('mouseover', {bubbles: true}))"),
                           QWebEngineScript::ApplicationWorld);
        QTRY_COMPARE_WITH_TIMEOUT(linkText(evil, Gui::WebEngineMessagePage::LinkSource::Hovered), QStringLiteral("Second"), 10000);
        QCOMPARE(linkText(evil, Gui::WebEngineMessagePage::LinkSource::Clicked), QStringLiteral("https://bank.example.com/login"));
        QCOMPARE(linkText(QStringLiteral("https://example.org/path"), Gui::WebEngineMessagePage::LinkSource::Hovered),
                 QStringLiteral("Web"));
        QCOMPARE(linkText(QStringLiteral("https://nowhere.test/"), Gui::WebEngineMessagePage::LinkSource::Hovered), QString());
    }

    void delegatesUserInitiatedNewWindow_data()
    {
        QTest::addColumn<QString>("url");
        QTest::newRow("https") << QStringLiteral("https://example.org/new");
        QTest::newRow("mailto") << QStringLiteral("mailto:someone@example.org");
    }

    void delegatesUserInitiatedNewWindow()
    {
        QFETCH(QString, url);
        MessageModel message;
        message.bodyData = QStringLiteral(
            "<html><body style=\"margin:0\"><a href=\"%1\" target=\"_blank\" "
            "style=\"display:block;width:200px;height:80px\">Open link</a></body></html>").arg(url).toUtf8();
        Gui::WebEngineMessageProfile profile(message.message->index());
        Gui::WebEngineMessagePage page(&profile);
        QWebEngineView view;
        view.resize(300, 150);
        view.setPage(&page);
        view.show();
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        QSignalSpy links(&page, &Gui::WebEngineMessagePage::externalLinkRequested);
        QVERIFY(page.loadMessagePart(QUrl(QStringLiteral("trojita-imap://msg/0"))));
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 15000);
        QVERIFY(finished.last().at(0).toBool());
        auto clicked = std::make_shared<bool>(false);
        page.runJavaScript(QStringLiteral("document.querySelector('a').click()"), QWebEngineScript::ApplicationWorld,
                           [clicked](const QVariant &) { *clicked = true; });
        QTRY_VERIFY_WITH_TIMEOUT(*clicked, 10000);
        QTest::qWait(300);
        QVERIFY(links.isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(view.focusProxy(), 10000);
        QTest::mouseClick(view.focusProxy(), Qt::LeftButton, Qt::NoModifier, QPoint(20, 20));
        QTRY_COMPARE_WITH_TIMEOUT(links.size(), 1, 10000);
        QCOMPARE(links.at(0).at(0).toUrl(), QUrl(url));
        QCOMPARE(page.url(), QUrl(QStringLiteral("trojita-imap://msg/0")));
    }

    void blocksUnknownExternalProtocols()
    {
        ExternalUrlHandler handler;
        const QString scheme = QStringLiteral("trojita-test-external");
        QDesktopServices::setUrlHandler(scheme, &handler, "handleUrl");
        connect(&handler, &QObject::destroyed, [scheme] { QDesktopServices::unsetUrlHandler(scheme); });
        MessageModel message;
        message.bodyData = QByteArrayLiteral(
            "<html><body style=\"margin:0\"><a href=\"trojita-test-external:payload\" "
            "style=\"display:block;width:200px;height:80px\">Open link</a></body></html>");
        Gui::WebEngineMessageProfile profile(message.message->index());
        Gui::WebEngineMessagePage page(&profile);
        QWebEngineView view;
        view.resize(300, 150);
        view.setPage(&page);
        view.show();
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        QSignalSpy links(&page, &Gui::WebEngineMessagePage::externalLinkRequested);
        QVERIFY(page.loadMessagePart(QUrl(QStringLiteral("trojita-imap://msg/0"))));
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 15000);
        QVERIFY(finished.last().at(0).toBool());
        QTRY_VERIFY_WITH_TIMEOUT(view.focusProxy(), 10000);
        QTest::mouseClick(view.focusProxy(), Qt::LeftButton, Qt::NoModifier, QPoint(20, 20));
        QTest::qWait(300);
        QCOMPARE(handler.calls, 0);
        QVERIFY(links.isEmpty());
        QCOMPARE(page.url(), QUrl(QStringLiteral("trojita-imap://msg/0")));
    }

    void blocksUnsolicitedNavigation()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        MessageModel message;
        const QUrl remote(QStringLiteral("http://127.0.0.1:%1/submit").arg(server.serverPort()));
        message.bodyData = QStringLiteral(
            "<html><body><form id=\"form\" action=\"%1\"><input name=\"message\" value=\"secret\"></form>"
            "Original message</body></html>").arg(remote.toString()).toUtf8();
        Gui::WebEngineMessageProfile profile(message.message->index());
        profile.setRemoteImagesAllowed(true);
        Gui::WebEngineMessagePage page(&profile);
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        QSignalSpy links(&page, &Gui::WebEngineMessagePage::externalLinkRequested);
        QVERIFY(page.loadMessagePart(QUrl(QStringLiteral("trojita-imap://msg/0"))));
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 15000);
        QVERIFY(finished.last().at(0).toBool());
        auto submitted = std::make_shared<bool>(false);
        page.runJavaScript(QStringLiteral("document.getElementById('form').submit()"), QWebEngineScript::ApplicationWorld,
                           [submitted](const QVariant &) { *submitted = true; });
        QTRY_VERIFY_WITH_TIMEOUT(*submitted, 10000);
        QTest::qWait(300);
        QVERIFY(!server.hasPendingConnections());
        QVERIFY(links.isEmpty());
        QCOMPARE(page.url(), QUrl(QStringLiteral("trojita-imap://msg/0")));
        const auto previousLoads = finished.size();
        page.load(remote);
        QTRY_VERIFY_WITH_TIMEOUT(finished.size() > previousLoads, 10000);
        QVERIFY(!server.hasPendingConnections());
        QVERIFY(links.isEmpty());
        QCOMPARE(page.url(), QUrl(QStringLiteral("trojita-imap://msg/0")));
    }

    void blocksAutomaticRedirect()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        MessageModel message;
        const QString remote = QStringLiteral("http://127.0.0.1:%1/track").arg(server.serverPort());
        message.bodyData = QStringLiteral(
            "<html><head><meta http-equiv=\"refresh\" content=\"0;url=%1\"></head><body>Original message</body></html>")
            .arg(remote).toUtf8();
        Gui::WebEngineMessageProfile profile(message.message->index());
        profile.setRemoteImagesAllowed(true);
        Gui::WebEngineMessagePage page(&profile);
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        QSignalSpy links(&page, &Gui::WebEngineMessagePage::externalLinkRequested);
        QVERIFY(page.loadMessagePart(QUrl(QStringLiteral("trojita-imap://msg/0"))));
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 15000);
        QTest::qWait(500);
        QVERIFY(!server.hasPendingConnections());
        QVERIFY(links.isEmpty());
        QCOMPARE(page.url(), QUrl(QStringLiteral("trojita-imap://msg/0")));
    }

    void blocksRemoteImagesInSubframesWithoutConsent()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        MessageModel message;
        const QString remote = QStringLiteral("http://127.0.0.1:%1/track").arg(server.serverPort());
        message.bodyData = QStringLiteral(
            "<html><body><iframe srcdoc=\"&lt;img src='%1'&gt;\"></iframe>Original message</body></html>")
            .arg(remote).toUtf8();
        Gui::WebEngineMessageProfile profile(message.message->index());
        Gui::WebEngineMessagePage page(&profile);
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        QSignalSpy blockedImages(&profile, &Gui::WebEngineMessageProfile::remoteImageBlocked);
        QVERIFY(page.loadMessagePart(QUrl(QStringLiteral("trojita-imap://msg/0"))));
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(!blockedImages.isEmpty(), 10000);
        QVERIFY(!server.hasPendingConnections());
    }

    void loadsMessageAndCid()
    {
        MessageModel message;
        message.bodyData = QByteArrayLiteral("<html><body>Message body<img src=\"cid:logo@example\">"
                                             "<img src=\"trojita-imap://cid/logo%40example\"></body></html>");
        Gui::WebEngineMessageProfile profile(message.message->index());
        QVERIFY(profile.isOffTheRecord());
        QVERIFY(!profile.settings()->testAttribute(QWebEngineSettings::JavascriptEnabled));
        QVERIFY(!profile.settings()->testAttribute(QWebEngineSettings::LocalStorageEnabled));
        QWebEnginePage page(&profile);
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        page.load(QUrl(QStringLiteral("trojita-imap://msg/0")));
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 15000);
        QVERIFY(finished.last().at(0).toBool());

        auto text = std::make_shared<QString>();
        auto received = std::make_shared<bool>(false);
        page.toPlainText([text, received](const QString &result) { *text = result; *received = true; });
        QTRY_VERIFY_WITH_TIMEOUT(*received, 10000);
        QVERIFY(text->contains(QStringLiteral("Message body")));

        auto width = std::make_shared<int>(-1);
        page.runJavaScript(QStringLiteral("document.images[0].naturalWidth + document.images[1].naturalWidth"),
                           QWebEngineScript::ApplicationWorld,
                           [width](const QVariant &result) { *width = result.toInt(); });
        QTRY_COMPARE_WITH_TIMEOUT(*width, 2, 10000);
    }

    void remoteImagesRequirePerProfileConsent()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int requests = 0;
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            while (auto *socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [socket, &requests] {
                    if (socket->readAll().isEmpty())
                        return;
                    ++requests;
                    socket->write("HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    socket->disconnectFromHost();
                });
            }
        });

        MessageModel message;
        message.bodyData = QStringLiteral("<html><body><img src=\"http://127.0.0.1:%1/pixel\"></body></html>")
                               .arg(server.serverPort()).toUtf8();
        Gui::WebEngineMessageProfile allowed(message.message->index());
        Gui::WebEngineMessageProfile blocked(message.message->index());
        QWebEnginePage allowedPage(&allowed);
        QWebEnginePage blockedPage(&blocked);
        QSignalSpy blockedImages(&allowed, &Gui::WebEngineMessageProfile::remoteImageBlocked);
        QSignalSpy first(&allowedPage, &QWebEnginePage::loadFinished);
        allowedPage.load(QUrl(QStringLiteral("trojita-imap://msg/0")));
        QTRY_VERIFY_WITH_TIMEOUT(!first.isEmpty(), 15000);
        QTest::qWait(300);
        QCOMPARE(requests, 0);
        QTRY_COMPARE_WITH_TIMEOUT(blockedImages.size(), 1, 10000);
        QCOMPARE(blockedImages.at(0).at(0).toUrl(),
                 QUrl(QStringLiteral("http://127.0.0.1:%1/pixel").arg(server.serverPort())));

        allowed.setRemoteImagesAllowed(true);
        allowedPage.triggerAction(QWebEnginePage::Reload);
        QTRY_COMPARE_WITH_TIMEOUT(requests, 1, 10000);
        QSignalSpy second(&blockedPage, &QWebEnginePage::loadFinished);
        blockedPage.load(QUrl(QStringLiteral("trojita-imap://msg/0")));
        QTRY_VERIFY_WITH_TIMEOUT(!second.isEmpty(), 15000);
        QTest::qWait(300);
        QCOMPARE(requests, 1);
        QCOMPARE(blockedImages.size(), 1);
    }

    void waitsForImapPart()
    {
        MessageModel message;
        message.bodyData = QByteArrayLiteral("<html><body>Fetched later</body></html>");
        message.body->setData(false, Imap::Mailbox::RoleIsFetched);
        Gui::WebEngineMessageProfile profile(message.message->index());
        QWebEnginePage page(&profile);
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        page.load(QUrl(QStringLiteral("trojita-imap://msg/0")));
        QTRY_VERIFY_WITH_TIMEOUT(message.body->fetchRequests > 0, 10000);
        QVERIFY(finished.isEmpty());
        message.body->setData(true, Imap::Mailbox::RoleIsFetched);
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 15000);
        QVERIFY(finished.last().at(0).toBool());
        auto text = std::make_shared<QString>();
        auto received = std::make_shared<bool>(false);
        page.toPlainText([text, received](const QString &result) { *text = result; *received = true; });
        QTRY_VERIFY_WITH_TIMEOUT(*received, 10000);
        QVERIFY(text->contains(QStringLiteral("Fetched later")));
    }

    void preservesMessageCharset()
    {
        MessageModel message;
        message.bodyData = QByteArray("<html><body>caf\xe9</body></html>");
        message.body->setData(QStringLiteral("ISO-8859-1"), Imap::Mailbox::RolePartCharset);
        Gui::WebEngineMessageProfile profile(message.message->index());
        QWebEnginePage page(&profile);
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        page.load(QUrl(QStringLiteral("trojita-imap://msg/0")));
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 15000);
        QVERIFY(finished.last().at(0).toBool());
        auto text = std::make_shared<QString>();
        auto received = std::make_shared<bool>(false);
        page.toPlainText([text, received](const QString &result) { *text = result; *received = true; });
        QTRY_VERIFY_WITH_TIMEOUT(*received, 10000);
        QVERIFY(text->contains(QString::fromUtf8("caf\xc3\xa9")));
    }

    void blocksScriptsFilesAndNonImageRequests()
    {
        MessageModel message;
        QTemporaryFile localImage;
        QVERIFY(localImage.open());
        QCOMPARE(localImage.write(message.imageData), message.imageData.size());
        QVERIFY(localImage.flush());
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int requests = 0;
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            while (auto *socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [socket, &requests, &localImage] {
                    if (socket->readAll().isEmpty())
                        return;
                    ++requests;
                    socket->write("HTTP/1.1 302 Found\r\nLocation: " +
                                  QUrl::fromLocalFile(localImage.fileName()).toEncoded() +
                                  "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    socket->disconnectFromHost();
                });
            }
        });
        const QString base = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
        message.bodyData = QStringLiteral(
            "<html><head><title>Safe</title><link rel=\"stylesheet\" href=\"%1/style\">"
            "<script>document.title='Script ran'</script><script src=\"%1/script\"></script></head>"
            "<body><iframe src=\"%1/frame\"></iframe><img src=\"%1/redirect\">"
            "<img src=\"%2\"></body></html>")
            .arg(base, QUrl::fromLocalFile(localImage.fileName()).toString()).toUtf8();
        Gui::WebEngineMessageProfile profile(message.message->index());
        profile.setRemoteImagesAllowed(true);
        QWebEnginePage page(&profile);
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        page.load(QUrl(QStringLiteral("trojita-imap://msg/0")));
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 15000);
        QVERIFY(finished.last().at(0).toBool());
        QTRY_COMPARE_WITH_TIMEOUT(requests, 1, 10000);
        QCOMPARE(page.title(), QStringLiteral("Safe"));
        auto widths = std::make_shared<QString>();
        auto received = std::make_shared<bool>(false);
        page.runJavaScript(QStringLiteral("Array.from(document.images, i => i.naturalWidth).join(',')"),
                           QWebEngineScript::ApplicationWorld,
                           [widths, received](const QVariant &result) { *widths = result.toString(); *received = true; });
        QTRY_VERIFY_WITH_TIMEOUT(*received, 10000);
        QCOMPARE(*widths, QStringLiteral("0,0"));
        QTest::qWait(300);
        QCOMPARE(requests, 1);
    }

    void revokingConsentBlocksNewImages()
    {
        MessageModel message;
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int requests = 0;
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            while (auto *socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [socket, &requests, &message] {
                    if (socket->readAll().isEmpty())
                        return;
                    ++requests;
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nCache-Control: max-age=3600\r\nContent-Length: " +
                                  QByteArray::number(message.imageData.size()) +
                                  "\r\nConnection: close\r\n\r\n" + message.imageData);
                    socket->disconnectFromHost();
                });
            }
        });
        message.bodyData = QStringLiteral("<html><body><img src=\"http://127.0.0.1:%1/pixel\"></body></html>")
                               .arg(server.serverPort()).toUtf8();
        Gui::WebEngineMessageProfile profile(message.message->index());
        profile.setRemoteImagesAllowed(true);
        QWebEnginePage page(&profile);
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        page.load(QUrl(QStringLiteral("trojita-imap://msg/0")));
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 15000);
        QTRY_COMPARE_WITH_TIMEOUT(requests, 1, 10000);
        auto width = std::make_shared<int>(-1);
        page.runJavaScript(QStringLiteral("document.images[0].naturalWidth"), QWebEngineScript::ApplicationWorld,
                           [width](const QVariant &result) { *width = result.toInt(); });
        QTRY_COMPARE_WITH_TIMEOUT(*width, 1, 10000);
        profile.setRemoteImagesAllowed(false);
        message.bodyData = QStringLiteral(
            "<html><body><img src=\"http://127.0.0.1:%1/pixel\">"
            "<img src=\"http://127.0.0.1:%1/new-pixel\"></body></html>")
            .arg(server.serverPort()).toUtf8();
        page.triggerAction(QWebEnginePage::Reload);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, 15000);
        *width = -1;
        page.runJavaScript(QStringLiteral("document.images[1].naturalWidth"),
                           QWebEngineScript::ApplicationWorld,
                           [width](const QVariant &result) { *width = result.toInt(); });
        QTRY_COMPARE_WITH_TIMEOUT(*width, 0, 10000);
        QCOMPARE(requests, 1);
    }

    void missingPartFails()
    {
        MessageModel message;
        Gui::WebEngineMessageProfile profile(message.message->index());
        QWebEnginePage page(&profile);
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        page.load(QUrl(QStringLiteral("trojita-imap://msg/99")));
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 15000);
        QVERIFY(!finished.last().at(0).toBool());
    }

    void removedPartFailsPendingRequest()
    {
        MessageModel message;
        message.body->setData(false, Imap::Mailbox::RoleIsFetched);
        Gui::WebEngineMessageProfile profile(message.message->index());
        QWebEnginePage page(&profile);
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        page.load(QUrl(QStringLiteral("trojita-imap://msg/0")));
        QTRY_VERIFY_WITH_TIMEOUT(message.body->fetchRequests > 0, 10000);
        message.message->removeRow(0);
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 15000);
        QVERIFY(!finished.last().at(0).toBool());
    }

    void modelDestructionFailsPendingRequest()
    {
        auto message = std::make_unique<MessageModel>();
        message->body->setData(false, Imap::Mailbox::RoleIsFetched);
        Gui::WebEngineMessageProfile profile(message->message->index());
        QWebEnginePage page(&profile);
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        page.load(QUrl(QStringLiteral("trojita-imap://msg/0")));
        QTRY_VERIFY_WITH_TIMEOUT(message->body->fetchRequests > 0, 10000);
        message.reset();
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 15000);
        QVERIFY(!finished.last().at(0).toBool());
    }

    void switchingMessageCancelsPendingResult()
    {
        MessageModel message;
        message.bodyData = QByteArrayLiteral("<html><body>Stale message</body></html>");
        message.body->setData(false, Imap::Mailbox::RoleIsFetched);
        Gui::WebEngineMessageProfile profile(message.message->index());
        QWebEnginePage page(&profile);
        page.load(QUrl(QStringLiteral("trojita-imap://msg/0")));
        QTRY_VERIFY_WITH_TIMEOUT(message.body->fetchRequests > 0, 10000);
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        page.setHtml(QStringLiteral("<html><body>Replacement</body></html>"));
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 15000);
        message.body->setData(true, Imap::Mailbox::RoleIsFetched);

        auto text = std::make_shared<QString>();
        auto received = std::make_shared<bool>(false);
        page.toPlainText([text, received](const QString &result) { *text = result; *received = true; });
        QTRY_VERIFY_WITH_TIMEOUT(*received, 10000);
        QVERIFY(text->contains(QStringLiteral("Replacement")));
        QVERIFY(!text->contains(QStringLiteral("Stale message")));
    }

    void switchingPartCancelsPendingPage()
    {
        MessageModel message;
        message.bodyData = QByteArrayLiteral("<html><body>Stale part</body></html>");
        message.body->setData(false, Imap::Mailbox::RoleIsFetched);
        QByteArray nextData = QByteArrayLiteral("<html><body>Current part</body></html>");
        message.addPart(QStringLiteral("text/html"), &nextData);
        Gui::WebEngineMessageProfile profile(message.message->index());
        Gui::WebEngineMessagePage page(&profile);
        QVERIFY(page.loadMessagePart(QUrl(QStringLiteral("trojita-imap://msg/0"))));
        QTRY_VERIFY_WITH_TIMEOUT(message.body->fetchRequests > 0, 10000);
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        QVERIFY(page.loadMessagePart(QUrl(QStringLiteral("trojita-imap://msg/2"))));
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty() && finished.last().at(0).toBool(), 15000);
        message.body->setData(true, Imap::Mailbox::RoleIsFetched);
        auto text = std::make_shared<QString>();
        auto received = std::make_shared<bool>(false);
        page.toPlainText([text, received](const QString &result) { *text = result; *received = true; });
        QTRY_VERIFY_WITH_TIMEOUT(*received, 10000);
        QVERIFY(text->contains(QStringLiteral("Current part")));
        QVERIFY(!text->contains(QStringLiteral("Stale part")));
    }

    void quotesRenderedTextAsynchronously()
    {
        MessageModel message;
        message.bodyData = QByteArrayLiteral("<html><body>Quoted &amp; rendered text</body></html>");
        Gui::WebEngineMessageProfile profile(message.message->index());
        Gui::WebEngineMessagePage page(&profile);
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        QVERIFY(page.loadMessagePart(QUrl(QStringLiteral("trojita-imap://msg/0"))));
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 15000);
        QVERIFY(finished.last().at(0).toBool());

        QString quoted;
        bool received = false;
        page.quoteText([&](const QString &text) {
            quoted = text;
            received = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(received, 10000);
        QCOMPARE(quoted, QStringLiteral("Quoted & rendered text"));
    }

    void quotesSelectedText()
    {
        MessageModel message;
        message.bodyData = QByteArrayLiteral("<html><body>Selected message text</body></html>");
        Gui::WebEngineMessageProfile profile(message.message->index());
        Gui::WebEngineMessagePage page(&profile);
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        QVERIFY(page.loadMessagePart(QUrl(QStringLiteral("trojita-imap://msg/0"))));
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 15000);
        QVERIFY(finished.last().at(0).toBool());

        bool selected = false;
        page.runJavaScript(QStringLiteral("window.getSelection().selectAllChildren(document.body)"),
                           QWebEngineScript::ApplicationWorld,
                           [&](const QVariant &) { selected = true; });
        QTRY_VERIFY_WITH_TIMEOUT(selected, 10000);

        QString quoted;
        bool received = false;
        page.quoteText([&](const QString &text) {
            quoted = text;
            received = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(received, 10000);
        QCOMPARE(quoted, QStringLiteral("Selected message text"));
    }

    void quotesFormattedPlainText_data()
    {
        QTest::addColumn<QString>("original");
        QTest::addColumn<QString>("expected");
        QTest::newRow("nested") << QStringLiteral("Plain\n> Outer\n>> Inner\nEnd")
                                << QStringLiteral("Plain\n> Outer\n>> Inner\nEnd");
        QTest::newRow("collapsed") << QStringLiteral("Foo bar.\n> blesmrt\n>> 333\n>> 666\n>> 666-2\n>> 666-3\n>> 666-4")
                                   << QStringLiteral("Foo bar.\n> blesmrt ...");
    }

    void quotesFormattedPlainText()
    {
        QFETCH(QString, original);
        QFETCH(QString, expected);
        MessageModel message;
        message.body->setData(original, Imap::Mailbox::RolePartUnicodeText);
        Gui::WebEngineMessageProfile profile(message.message->index());
        Gui::WebEngineMessagePage page(&profile);
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        const QString html = UiUtils::htmlizedTextPart(message.body->index(), QFontInfo(QFont()),
                                                       Qt::white, Qt::black, Qt::blue, Qt::blue);
        QVERIFY(page.setMessageHtml(html, QUrl(QStringLiteral("trojita-imap://msg/0"))));
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 15000);
        QVERIFY(finished.last().at(0).toBool());

        QString quoted;
        bool received = false;
        page.quoteText([&](const QString &text) {
            quoted = text;
            received = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(received, 10000);
        QCOMPARE(quoted, expected);
    }

    void limitsPlainTextToReadableWidth()
    {
        MessageModel message;
        message.body->setData(QString(QStringLiteral("word ")).repeated(400), Imap::Mailbox::RolePartUnicodeText);
        Gui::WebEngineMessageProfile profile(message.message->index());
        Gui::WebEngineMessagePage page(&profile);
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        const QString html = UiUtils::htmlizedTextPart(message.body->index(), QFontInfo(QFont()),
                                                       Qt::white, Qt::black, Qt::blue, Qt::blue);
        QVERIFY(page.setMessageHtml(html, QUrl(QStringLiteral("trojita-imap://msg/0"))));
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 15000);
        QVERIFY(finished.last().at(0).toBool());

        // The paragraph is wrapped at about 90 characters of its own font, not at the page's width
        auto result = std::make_shared<QString>();
        page.runJavaScript(QStringLiteral(R"JS((() => {
            const text = document.querySelector(".plain-text");
            const probe = document.createElement("span");
            probe.style.whiteSpace = "pre";
            probe.textContent = "0".repeat(90);
            text.appendChild(probe);
            const measure = probe.getBoundingClientRect().width;
            probe.remove();
            return [getComputedStyle(text).maxWidth, measure].join("|");
        })())JS"),
                           QWebEngineScript::ApplicationWorld, [result](const QVariant &value) { *result = value.toString(); });
        QTRY_VERIFY_WITH_TIMEOUT(!result->isEmpty(), 10000);
        const QStringList parts = result->split(QLatin1Char('|'));
        QVERIFY2(parts.value(0).endsWith(QLatin1String("px")), qPrintable(*result));
        const double maxWidth = parts.value(0).chopped(2).toDouble();
        QVERIFY2(qAbs(maxWidth - parts.value(1).toDouble()) < 2, qPrintable(*result));
    }

    void displaysGeneratedMarkupWithMessageBaseUrl()
    {
        MessageModel message;
        Gui::WebEngineMessageProfile profile(message.message->index());
        Gui::WebEngineMessagePage page(&profile);
        QSignalSpy finished(&page, &QWebEnginePage::loadFinished);
        QVERIFY(page.loadMessagePart(QUrl(QStringLiteral("trojita-imap://msg/0"))));
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 15000);
        QVERIFY(finished.last().at(0).toBool());

        page.setMessageHtml(QStringLiteral("<html><body><pre>Formatted plain text</pre></body></html>"),
                            QUrl(QStringLiteral("trojita-imap://msg/0")));
        QTRY_VERIFY_WITH_TIMEOUT(finished.size() >= 2, 15000);
        QVERIFY(finished.last().at(0).toBool());

        auto text = std::make_shared<QString>();
        auto received = std::make_shared<bool>(false);
        page.toPlainText([text, received](const QString &result) { *text = result; *received = true; });
        QTRY_VERIFY_WITH_TIMEOUT(*received, 10000);
        QCOMPARE(*text, QStringLiteral("Formatted plain text"));

        QString quoted;
        bool quotedReceived = false;
        page.quoteText([&](const QString &result) {
            quoted = result;
            quotedReceived = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(quotedReceived, 10000);
        QCOMPARE(quoted, QStringLiteral("Formatted plain text"));
    }

    void rejectsGeneratedMarkupWithoutMessageBaseUrl()
    {
        MessageModel message;
        Gui::WebEngineMessageProfile profile(message.message->index());
        Gui::WebEngineMessagePage page(&profile);
        const QString html = QStringLiteral("<html><body>Generated text</body></html>");

        QVERIFY(!page.setMessageHtml(html, QUrl(QStringLiteral("https://example.com/"))));
        QVERIFY(!page.setMessageHtml(html, QUrl(QStringLiteral("trojita-imap://msg/0?query"))));
        QVERIFY(page.loadMessagePart(QUrl(QStringLiteral("trojita-imap://msg/0"))));
    }

    void adaptsColorsToThePalette_data()
    {
        QTest::addColumn<QString>("mimeType");
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<bool>("generated");
        QTest::addColumn<bool>("dark");
        QTest::addColumn<bool>("forced");
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QString>("background");
        QTest::addColumn<QString>("probeColor");

        const QString darkBase = QStringLiteral("rgb(35, 38, 41)");
        const QString darkText = QStringLiteral("rgb(239, 240, 241)");
        const QString white = QStringLiteral("rgb(255, 255, 255)");
        const QByteArray script = QByteArrayLiteral("<script>document.title = 'script ran';</script>");

        QTest::newRow("plain text, dark") << QStringLiteral("text/plain") << QByteArrayLiteral("Hello <b>there</b>") << false
                                          << true << false << QStringLiteral("plain") << darkBase << darkText;
        QTest::newRow("generated plain text, dark")
            << QStringLiteral("text/html") << QByteArray() << true << true << false << QStringLiteral("plain") << darkBase
            << darkText;
        QTest::newRow("html without background, dark")
            << QStringLiteral("text/html")
            << QByteArray("<html><body><p id=\"probe\" style=\"color:#333333\">Newsletter</p>" + script + "</body></html>")
            << false << true << false << QStringLiteral("page") << white << QStringLiteral("rgb(51, 51, 51)");
        QTest::newRow("html without any colors, dark")
            << QStringLiteral("text/html") << QByteArray("<html><body><p id=\"probe\">Plain HTML</p>" + script + "</body></html>")
            << false << true << false << QStringLiteral("page") << white << QStringLiteral("rgb(0, 0, 0)");
        QTest::newRow("html with its own background, dark")
            << QStringLiteral("text/html")
            << QByteArray("<html><body style=\"background:#123456;color:#abcdef\"><p id=\"probe\">Own</p>" + script
                          + "</body></html>")
            << false << true << false << QStringLiteral("own") << QStringLiteral("rgb(18, 52, 86)")
            << QStringLiteral("rgb(171, 205, 239)");
        QTest::newRow("html with a bgcolor attribute, dark")
            << QStringLiteral("text/html")
            << QByteArray("<html><body bgcolor=\"#ffeedd\"><p id=\"probe\">Old</p></body></html>") << false << true
            << false << QStringLiteral("own") << QStringLiteral("rgb(255, 238, 221)") << QStringLiteral("rgb(0, 0, 0)");
        QTest::newRow("html without background, light")
            << QStringLiteral("text/html") << QByteArray("<html><body><p id=\"probe\">Light</p></body></html>") << false
            << false << false << QStringLiteral("page") << QStringLiteral("rgb(252, 252, 252)")
            << QStringLiteral("rgb(35, 38, 41)");
        QTest::newRow("forced black on white")
            << QStringLiteral("text/html")
            << QByteArray("<html><body style=\"background:#123456;color:#abcdef\"><p id=\"probe\">Own</p></body></html>")
            << false << true << true << QStringLiteral("forced") << white << QStringLiteral("rgb(0, 0, 0)");
    }

    void adaptsColorsToThePalette()
    {
        QFETCH(QString, mimeType);
        QFETCH(QByteArray, body);
        QFETCH(bool, generated);
        QFETCH(bool, dark);
        QFETCH(bool, forced);
        QFETCH(QString, kind);
        QFETCH(QString, background);
        QFETCH(QString, probeColor);

        MessageModel message;
        message.body->setData(mimeType, Imap::Mailbox::RolePartMimeType);
        message.bodyData = body.isEmpty() ? QByteArrayLiteral("<html><body>raw</body></html>") : body;
        Gui::WebEngineMessageProfile profile(message.message->index());
        Gui::WebEngineMessagePage page(&profile);
        QSignalSpy applied(&page, &Gui::WebEngineMessagePage::colorsApplied);

        Gui::WebEngineMessagePage::Colors colors;
        colors.background = dark ? QColor(0x23, 0x26, 0x29) : QColor(0xfc, 0xfc, 0xfc);
        colors.text = dark ? QColor(0xef, 0xf0, 0xf1) : QColor(0x23, 0x26, 0x29);
        colors.link = QColor(0x1d, 0x99, 0xf3);
        colors.visitedLink = QColor(0x9b, 0x59, 0xb6);
        colors.forced = forced;
        page.setColors(colors);

        const QUrl url(QStringLiteral("trojita-imap://msg/0"));
        QVERIFY(page.loadMessagePart(url));
        QTRY_VERIFY_WITH_TIMEOUT(applied.size() >= 2 && !applied.last().at(0).toString().isEmpty(), 15000);
        if (generated) {
            applied.clear();
            QVERIFY(page.setMessageHtml(QStringLiteral("<html><body><pre class=\"plain-text\">Formatted</pre></body></html>"), url));
            QTRY_VERIFY_WITH_TIMEOUT(!applied.isEmpty() && !applied.last().at(0).toString().isEmpty(), 15000);
        }
        QCOMPARE(applied.last().at(0).toString(), kind);

        auto result = std::make_shared<QString>();
        page.runJavaScript(QStringLiteral(R"JS((() => {
            const bg = e => getComputedStyle(e).backgroundColor;
            const html = bg(document.documentElement);
            const page = html === "rgba(0, 0, 0, 0)" ? bg(document.body) : html;
            const probe = document.getElementById("probe") || document.body;
            return [page, getComputedStyle(probe).color, document.title].join("|");
        })())JS"),
                           QWebEngineScript::ApplicationWorld, [result](const QVariant &value) { *result = value.toString(); });
        QTRY_VERIFY_WITH_TIMEOUT(!result->isEmpty(), 10000);
        const QStringList parts = result->split(QLatin1Char('|'));
        QCOMPARE(parts.value(0), background);
        QCOMPARE(parts.value(1), probeColor);
        QVERIFY2(parts.value(2).isEmpty(), "a message script ran");
    }
};

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    Gui::WebEngineMessageProfile::registerScheme();
    QApplication app(argc, argv);
    WebEngineMessageProfileTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_WebEngineMessageProfile.moc"
