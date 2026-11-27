/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <memory>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusServer>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QWidget>

#include "Gui/FileChooser.h"

using Gui::FileChooser;

/** @short A portal file chooser that answers each request after a moment, as a dialog would */
class FakePortal : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.FileChooser")
    Q_PROPERTY(uint version READ version)
public:
    uint version() const
    {
        return portalVersion;
    }

    uint portalVersion = 4;
    uint answer = 0;
    QStringList uris;
    QDBusConnection *connection = nullptr;
    QWidget *window = nullptr;

    int calls = 0;
    QString parentWindow;
    QString title;
    QVariantMap options;
    bool windowWasDisabled = false;

public slots:
    QDBusObjectPath OpenFile(const QString &parent, const QString &dialogTitle, const QVariantMap &dialogOptions)
    {
        ++calls;
        parentWindow = parent;
        title = dialogTitle;
        options = dialogOptions;
        windowWasDisabled = window && !window->isEnabled();
        const QString handle = QStringLiteral("/org/freedesktop/portal/desktop/request/peer/%1")
                                       .arg(dialogOptions.value(QStringLiteral("handle_token")).toString());
        QTimer::singleShot(100, this, [this, handle]() {
            QDBusMessage response = QDBusMessage::createSignal(handle, QStringLiteral("org.freedesktop.portal.Request"),
                                                               QStringLiteral("Response"));
            response << answer << QVariantMap{{QStringLiteral("uris"), uris}};
            connection->send(response);
        });
        return QDBusObjectPath(handle);
    }
};

class FileChooserTest : public QObject
{
    Q_OBJECT
private slots:
    void optionsFollowTheRequest();
    void responsesBecomeOutcomes();
    void requestPathFollowsTheUniqueName();
    void fallbackFilterListsThePatterns();
    void portalChoosesFiles();
    void portalCancels();
    void portalEndingOtherwiseFallsBack();
    void oldPortalCannotChooseFolders();
    void missingPortalFallsBack();

private:
    /** Serve @p portal on a peer-to-peer connection, and ask it for @p request from @p parent */
    FileChooser::Result ask(FakePortal *portal, const FileChooser::Request &request, QWidget *parent = nullptr);

    // One folder for every server, so that no server's socket is removed while its folder is
    QTemporaryDir m_sockets;
    int m_connections = 0;
};

void FileChooserTest::optionsFollowTheRequest()
{
    FileChooser::Request request;
    request.title = QStringLiteral("Edit draft");
    request.folder = QUrl::fromLocalFile(QStringLiteral("/home/u/Drafts"));
    request.filters = {{QStringLiteral("Drafts"), {QStringLiteral("*.draft")}}};
    QVariantMap options = FileChooser::portalOptions(request, QStringLiteral("token1"));
    QCOMPARE(options.value(QStringLiteral("handle_token")).toString(), QStringLiteral("token1"));
    QCOMPARE(options.value(QStringLiteral("modal")).toBool(), true);
    QVERIFY(!options.contains(QStringLiteral("multiple")));
    QVERIFY(!options.contains(QStringLiteral("directory")));
    // The folder is a null-terminated byte string
    QCOMPARE(options.value(QStringLiteral("current_folder")).toByteArray(), QByteArray("/home/u/Drafts\0", 15));
    const Gui::PortalFilters filters = options.value(QStringLiteral("filters")).value<Gui::PortalFilters>();
    QCOMPARE(filters.size(), 1);
    QCOMPARE(filters.first().name, QStringLiteral("Drafts"));
    QCOMPARE(filters.first().rules.size(), 1);
    QCOMPARE(filters.first().rules.first().type, 0u);
    QCOMPARE(filters.first().rules.first().pattern, QStringLiteral("*.draft"));

    request.mode = FileChooser::Mode::Files;
    request.filters.clear();
    // The portal sees local folders only
    request.folder = QUrl(QStringLiteral("sftp://server/home/u"));
    options = FileChooser::portalOptions(request, QStringLiteral("token2"));
    QCOMPARE(options.value(QStringLiteral("multiple")).toBool(), true);
    QVERIFY(!options.contains(QStringLiteral("current_folder")));
    QVERIFY(!options.contains(QStringLiteral("filters")));

    request.mode = FileChooser::Mode::Folder;
    request.folder = QUrl();
    options = FileChooser::portalOptions(request, QStringLiteral("token3"));
    QCOMPARE(options.value(QStringLiteral("directory")).toBool(), true);
    QVERIFY(!options.contains(QStringLiteral("multiple")));
    QVERIFY(!options.contains(QStringLiteral("current_folder")));
}

void FileChooserTest::responsesBecomeOutcomes()
{
    const QVariantMap chosen{{QStringLiteral("uris"),
                              QStringList{QStringLiteral("file:///home/u/a.pdf"), QStringLiteral("relative/b.pdf"),
                                          QStringLiteral("file:///home/u/c%20d.txt")}}};
    FileChooser::Result result = FileChooser::portalResult(0, chosen);
    QCOMPARE(result.outcome, FileChooser::Outcome::Chosen);
    QCOMPARE(result.urls, (QList<QUrl>{QUrl::fromLocalFile(QStringLiteral("/home/u/a.pdf")),
                                       QUrl::fromLocalFile(QStringLiteral("/home/u/c d.txt"))}));

    result = FileChooser::portalResult(0, {});
    QCOMPARE(result.outcome, FileChooser::Outcome::Cancelled);
    QVERIFY(result.urls.isEmpty());

    result = FileChooser::portalResult(1, chosen);
    QCOMPARE(result.outcome, FileChooser::Outcome::Cancelled);
    QVERIFY(result.urls.isEmpty());

    result = FileChooser::portalResult(2, chosen);
    QCOMPARE(result.outcome, FileChooser::Outcome::Unavailable);
    QVERIFY(result.urls.isEmpty());
}

void FileChooserTest::requestPathFollowsTheUniqueName()
{
    QCOMPARE(FileChooser::requestPath(QStringLiteral(":1.42"), QStringLiteral("plasma_impresario_7")),
             QStringLiteral("/org/freedesktop/portal/desktop/request/1_42/plasma_impresario_7"));
}

void FileChooserTest::fallbackFilterListsThePatterns()
{
    QCOMPARE(FileChooser::fallbackFilter({}), QString());
    QCOMPARE(FileChooser::fallbackFilter({{QStringLiteral("Drafts"), {QStringLiteral("*.draft")}},
                                          {QStringLiteral("Mail"), {QStringLiteral("*.eml"), QStringLiteral("*.mbox")}}}),
             QStringLiteral("Drafts (*.draft);;Mail (*.eml *.mbox)"));
}

FileChooser::Result FileChooserTest::ask(FakePortal *portal, const FileChooser::Request &request, QWidget *parent)
{
    if (!m_sockets.isValid())
        return {};
    QDBusServer server(QStringLiteral("unix:tmpdir=") + m_sockets.path());
    if (!server.isConnected())
        return {};
    server.setAnonymousAuthenticationAllowed(true);
    std::unique_ptr<QDBusConnection> serverSide;
    connect(&server, &QDBusServer::newConnection, this, [&serverSide, portal](const QDBusConnection &connection) {
        serverSide = std::make_unique<QDBusConnection>(connection);
        if (!portal)
            return;
        portal->connection = serverSide.get();
        serverSide->registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), portal,
                                   QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllProperties);
    });
    FileChooser::Result result;
    const QString name = QStringLiteral("portal-test-%1").arg(++m_connections);
    {
        const QDBusConnection client = QDBusConnection::connectToPeer(server.address(), name);
        if (!client.isConnected())
            return {};
        if (!QTest::qWaitFor([&serverSide]() { return bool(serverSide); }, 5000))
            return {};
        result = FileChooser::askPortal(parent, request, client, QString());
    }
    QDBusConnection::disconnectFromPeer(name);
    return result;
}

void FileChooserTest::portalChoosesFiles()
{
    QWidget window;
    FakePortal portal;
    portal.window = &window;
    portal.uris = {QStringLiteral("file:///home/u/a.pdf"), QStringLiteral("file:///home/u/b.pdf")};
    FileChooser::Request request;
    request.title = QStringLiteral("Attach File...");
    request.mode = FileChooser::Mode::Files;
    request.folder = QUrl::fromLocalFile(QStringLiteral("/home/u"));
    request.filters = {{QStringLiteral("Documents"), {QStringLiteral("*.pdf")}}};

    const FileChooser::Result result = ask(&portal, request, &window);
    QCOMPARE(portal.calls, 1);
    QCOMPARE(result.outcome, FileChooser::Outcome::Chosen);
    QCOMPARE(result.urls, (QList<QUrl>{QUrl::fromLocalFile(QStringLiteral("/home/u/a.pdf")),
                                       QUrl::fromLocalFile(QStringLiteral("/home/u/b.pdf"))}));
    QCOMPARE(portal.title, QStringLiteral("Attach File..."));
    // Qt's offscreen platform has no window handle that the portal could use
    QCOMPARE(portal.parentWindow, QString());
    QCOMPARE(portal.options.value(QStringLiteral("multiple")).toBool(), true);
    QCOMPARE(portal.options.value(QStringLiteral("current_folder")).toByteArray(), QByteArray("/home/u\0", 8));
    QVERIFY(portal.options.value(QStringLiteral("handle_token")).toString().startsWith(QLatin1String("plasma_impresario_")));
    const auto filters = qdbus_cast<Gui::PortalFilters>(portal.options.value(QStringLiteral("filters")));
    QCOMPARE(filters.size(), 1);
    QCOMPARE(filters.first().name, QStringLiteral("Documents"));
    QCOMPARE(filters.first().rules.first().pattern, QStringLiteral("*.pdf"));
    // The window waits for the dialog, then takes input again
    QVERIFY(portal.windowWasDisabled);
    QVERIFY(window.isEnabled());
}

void FileChooserTest::portalCancels()
{
    QWidget window;
    window.setEnabled(false);
    FakePortal portal;
    portal.answer = 1;
    portal.uris = {QStringLiteral("file:///home/u/a.pdf")};
    FileChooser::Request request;
    request.mode = FileChooser::Mode::Folder;
    const FileChooser::Result result = ask(&portal, request, &window);
    QCOMPARE(portal.calls, 1);
    QCOMPARE(portal.options.value(QStringLiteral("directory")).toBool(), true);
    QCOMPARE(result.outcome, FileChooser::Outcome::Cancelled);
    QVERIFY(result.urls.isEmpty());
    // A window that was disabled before stays so
    QVERIFY(!window.isEnabled());
}

void FileChooserTest::portalEndingOtherwiseFallsBack()
{
    FakePortal portal;
    portal.answer = 2;
    const FileChooser::Result result = ask(&portal, FileChooser::Request{});
    QCOMPARE(portal.calls, 1);
    QCOMPARE(result.outcome, FileChooser::Outcome::Unavailable);
}

void FileChooserTest::oldPortalCannotChooseFolders()
{
    FakePortal portal;
    portal.portalVersion = 2;
    FileChooser::Request request;
    request.mode = FileChooser::Mode::Folder;
    QCOMPARE(ask(&portal, request).outcome, FileChooser::Outcome::Unavailable);
    QCOMPARE(portal.calls, 0);

    // Files can still be chosen through it
    portal.uris = {QStringLiteral("file:///home/u/a.pdf")};
    request.mode = FileChooser::Mode::File;
    QCOMPARE(ask(&portal, request).outcome, FileChooser::Outcome::Chosen);
    QCOMPARE(portal.calls, 1);
}

void FileChooserTest::missingPortalFallsBack()
{
    // Nothing serves the file chooser
    QCOMPARE(ask(nullptr, FileChooser::Request{}).outcome, FileChooser::Outcome::Unavailable);

    // There is no bus at all
    const QDBusConnection none(QStringLiteral("no-such-connection"));
    QVERIFY(!none.isConnected());
    QCOMPARE(FileChooser::askPortal(nullptr, FileChooser::Request{}, none, FileChooser::portalService()).outcome,
             FileChooser::Outcome::Unavailable);
}

QTEST_MAIN(FileChooserTest)
#include "test_FileChooser.moc"
