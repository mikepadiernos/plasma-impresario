/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/FileChooser.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>
#include <QEventLoop>
#include <QFile>
#include <QFutureWatcher>
#include <QPointer>
#include <QRandomGenerator>
#include <QTimer>
#include <QWidget>
#include <QWindow>

#include <KWaylandExtras>
#include <KWindowSystem>
#include <kwindowsystem_version.h>

namespace Gui {

namespace {

const QString portalPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString fileChooserInterface = QStringLiteral("org.freedesktop.portal.FileChooser");
const QString requestInterface = QStringLiteral("org.freedesktop.portal.Request");
const QString responseSignal = QStringLiteral("Response");
const char *const responseSlot = SLOT(receive(uint,QVariantMap));
// Long enough for the session bus to start the portal; a missing portal answers at once
constexpr int portalStartTimeout = 10000;
constexpr int exportTimeout = 1000;

void registerTypes()
{
    static const bool registered = [] {
        qDBusRegisterMetaType<PortalFilterRule>();
        qDBusRegisterMetaType<QList<PortalFilterRule>>();
        qDBusRegisterMetaType<PortalFilter>();
        qDBusRegisterMetaType<PortalFilters>();
        return true;
    }();
    Q_UNUSED(registered);
}

/** Wait for the answer to @p call with the event loop running, so that a peer on this thread can answer */
QDBusMessage await(const QDBusConnection &bus, const QDBusMessage &call, int timeout)
{
    QDBusPendingCallWatcher watcher(bus.asyncCall(call, timeout));
    if (!watcher.isFinished()) {
        QEventLoop loop;
        QObject::connect(&watcher, &QDBusPendingCallWatcher::finished, &loop, &QEventLoop::quit);
        loop.exec();
    }
    // When the application quits meanwhile, there is no answer yet and the message is invalid
    return watcher.reply();
}

/** @short The portal's handle of a top-level window, exported for as long as this object lives */
class ParentWindow
{
public:
    explicit ParentWindow(QWidget *widget)
    {
        if (!widget)
            return;
        m_window = widget->window()->windowHandle();
        if (!m_window)
            return;
        if (KWindowSystem::isPlatformX11()) {
            m_handle = QStringLiteral("x11:%1").arg(m_window->winId(), 0, 16);
        } else if (KWindowSystem::isPlatformWayland()) {
            const QString exported = exportToplevel();
            if (!exported.isEmpty()) {
                m_handle = QStringLiteral("wayland:") + exported;
                m_exported = true;
            }
        }
    }

    ~ParentWindow()
    {
        if (!m_exported || !m_window)
            return;
#if KWINDOWSYSTEM_VERSION >= QT_VERSION_CHECK(6, 28, 0)
        KWaylandExtras::unexportToplevel(m_window);
#else
        KWaylandExtras::unexportWindow(m_window);
#endif
    }

    QString handle() const
    {
        return m_handle;
    }

private:
    QString exportToplevel()
    {
        QEventLoop loop;
        QTimer::singleShot(exportTimeout, &loop, &QEventLoop::quit);
        QString exported;
#if KWINDOWSYSTEM_VERSION >= QT_VERSION_CHECK(6, 28, 0)
        QFutureWatcher<QString> watcher;
        QObject::connect(&watcher, &QFutureWatcher<QString>::finished, &loop, &QEventLoop::quit);
        watcher.setFuture(KWaylandExtras::exportToplevel(m_window));
        if (!watcher.isFinished())
            loop.exec();
        if (watcher.isFinished() && watcher.future().resultCount() > 0)
            exported = watcher.result();
#else
        QWindow *window = m_window;
        QObject::connect(KWaylandExtras::self(), &KWaylandExtras::windowExported, &loop,
                         [&exported, &loop, window](QWindow *from, const QString &handle) {
                             if (from != window)
                                 return;
                             exported = handle;
                             loop.quit();
                         });
        KWaylandExtras::exportWindow(m_window);
        loop.exec();
#endif
        return exported;
    }

    QPointer<QWindow> m_window;
    QString m_handle;
    bool m_exported = false;
};

/** @short Keeps a window disabled while the portal's dialog stands in for a modal one */
class DisabledWindow
{
public:
    explicit DisabledWindow(QWidget *widget)
        : m_window(widget ? widget->window() : nullptr)
    {
        if (m_window && m_window->isEnabled())
            m_window->setEnabled(false);
        else
            m_window = nullptr;
    }

    ~DisabledWindow()
    {
        if (m_window)
            m_window->setEnabled(true);
    }

private:
    QPointer<QWidget> m_window;
};

}

/** @short Receives the portal's Response signal for one request */
class PortalResponse : public QObject
{
    Q_OBJECT
public:
    bool answered = false;
    uint response = 2;
    QVariantMap results;

signals:
    void received();

public slots:
    void receive(uint code, const QVariantMap &values)
    {
        if (answered)
            return;
        answered = true;
        response = code;
        results = values;
        emit received();
    }
};

QString FileChooser::portalService()
{
    return QStringLiteral("org.freedesktop.portal.Desktop");
}

QVariantMap FileChooser::portalOptions(const Request &request, const QString &handleToken)
{
    registerTypes();
    QVariantMap options{
        {QStringLiteral("handle_token"), handleToken},
        {QStringLiteral("modal"), true},
    };
    if (request.mode == Mode::Files)
        options.insert(QStringLiteral("multiple"), true);
    if (request.mode == Mode::Folder)
        options.insert(QStringLiteral("directory"), true);
    if (request.folder.isLocalFile()) {
        const QString folder = request.folder.toLocalFile();
        // The portal takes the folder as a null-terminated byte string
        if (!folder.isEmpty())
            options.insert(QStringLiteral("current_folder"), QFile::encodeName(folder).append('\0'));
    }
    if (!request.filters.isEmpty()) {
        PortalFilters filters;
        for (const Filter &filter : request.filters) {
            PortalFilter portalFilter{filter.name, {}};
            for (const QString &pattern : filter.patterns)
                portalFilter.rules.append(PortalFilterRule{0, pattern});
            filters.append(portalFilter);
        }
        options.insert(QStringLiteral("filters"), QVariant::fromValue(filters));
    }
    return options;
}

FileChooser::Result FileChooser::portalResult(uint response, const QVariantMap &results)
{
    Result result;
    // 0 is success, 1 the user's cancellation and 2 any other end, such as a failing dialog
    if (response == 1) {
        result.outcome = Outcome::Cancelled;
        return result;
    }
    if (response != 0)
        return result;
    const QStringList uris = results.value(QStringLiteral("uris")).toStringList();
    for (const QString &uri : uris) {
        const QUrl url(uri);
        if (url.isValid() && !url.isRelative())
            result.urls.append(url);
    }
    result.outcome = result.urls.isEmpty() ? Outcome::Cancelled : Outcome::Chosen;
    return result;
}

QString FileChooser::requestPath(const QString &uniqueName, const QString &handleToken)
{
    QString sender = uniqueName.startsWith(QLatin1Char(':')) ? uniqueName.mid(1) : uniqueName;
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    return portalPath + QStringLiteral("/request/%1/%2").arg(sender, handleToken);
}

QString FileChooser::fallbackFilter(const QList<Filter> &filters)
{
    QStringList parts;
    for (const Filter &filter : filters)
        parts.append(QStringLiteral("%1 (%2)").arg(filter.name, filter.patterns.join(QLatin1Char(' '))));
    return parts.join(QLatin1String(";;"));
}

FileChooser::Result FileChooser::askPortal(QWidget *parent, const Request &request, QDBusConnection bus,
                                           const QString &service)
{
    if (!bus.isConnected())
        return {};
    registerTypes();

    QDBusMessage getVersion = QDBusMessage::createMethodCall(service, portalPath,
                                                             QStringLiteral("org.freedesktop.DBus.Properties"),
                                                             QStringLiteral("Get"));
    getVersion << fileChooserInterface << QStringLiteral("version");
    const QDBusMessage versionReply = await(bus, getVersion, portalStartTimeout);
    if (versionReply.type() != QDBusMessage::ReplyMessage || versionReply.arguments().isEmpty())
        return {};
    const uint version = qvariant_cast<QDBusVariant>(versionReply.arguments().constFirst()).variant().toUInt();
    // Choosing folders came with version 3 of the file chooser
    if (version == 0 || (request.mode == Mode::Folder && version < 3))
        return {};

    const ParentWindow parentWindow(parent);
    const DisabledWindow disabled(parent);
    const QString token = QStringLiteral("plasma_impresario_%1").arg(QRandomGenerator::global()->generate());

    // Listen on the request's path before asking, so that a quick answer is not missed. A peer-to-peer connection
    // has no unique name, so only the path the portal returns is known there.
    PortalResponse response;
    const QString expectedPath = bus.baseService().isEmpty() ? QString() : requestPath(bus.baseService(), token);
    if (!expectedPath.isEmpty())
        bus.connect(service, expectedPath, requestInterface, responseSignal, &response, responseSlot);

    QDBusMessage open = QDBusMessage::createMethodCall(service, portalPath, fileChooserInterface,
                                                       QStringLiteral("OpenFile"));
    open << parentWindow.handle() << request.title << portalOptions(request, token);
    const QDBusMessage openReply = await(bus, open, portalStartTimeout);
    QString handle;
    if (openReply.type() == QDBusMessage::ReplyMessage && !openReply.arguments().isEmpty())
        handle = qvariant_cast<QDBusObjectPath>(openReply.arguments().constFirst()).path();
    if (handle.isEmpty()) {
        if (!expectedPath.isEmpty())
            bus.disconnect(service, expectedPath, requestInterface, responseSignal, &response, responseSlot);
        return {};
    }
    // Older portals return another path than the expected one
    if (handle != expectedPath) {
        if (!expectedPath.isEmpty())
            bus.disconnect(service, expectedPath, requestInterface, responseSignal, &response, responseSlot);
        bus.connect(service, handle, requestInterface, responseSignal, &response, responseSlot);
    }

    if (!response.answered) {
        QEventLoop loop;
        QObject::connect(&response, &PortalResponse::received, &loop, &QEventLoop::quit);
        loop.exec();
    }
    bus.disconnect(service, handle, requestInterface, responseSignal, &response, responseSlot);
    if (response.answered)
        return portalResult(response.response, response.results);

    // The application quits while the dialog is open: close it, and choose nothing
    bus.call(QDBusMessage::createMethodCall(service, handle, requestInterface, QStringLiteral("Close")), QDBus::NoBlock);
    Result cancelled;
    cancelled.outcome = Outcome::Cancelled;
    return cancelled;
}

QList<QUrl> FileChooser::choose(QWidget *parent, const Request &request)
{
    const Result asked = askPortal(parent, request, QDBusConnection::sessionBus(), portalService());
    if (asked.outcome != Outcome::Unavailable)
        return asked.urls;

    const QStringList schemes = request.localOnly ? QStringList{QStringLiteral("file")} : QStringList{};
    switch (request.mode) {
    case Mode::File: {
        const QUrl url = QFileDialog::getOpenFileUrl(parent, request.title, request.folder,
                                                     fallbackFilter(request.filters), nullptr,
                                                     request.fallbackOptions, schemes);
        return url.isEmpty() ? QList<QUrl>{} : QList<QUrl>{url};
    }
    case Mode::Files:
        return QFileDialog::getOpenFileUrls(parent, request.title, request.folder, fallbackFilter(request.filters),
                                            nullptr, request.fallbackOptions, schemes);
    case Mode::Folder: {
        const QUrl url = QFileDialog::getExistingDirectoryUrl(parent, request.title, request.folder,
                                                              request.fallbackOptions | QFileDialog::ShowDirsOnly,
                                                              schemes);
        return url.isEmpty() ? QList<QUrl>{} : QList<QUrl>{url};
    }
    }
    return {};
}

QString FileChooser::chooseLocalPath(QWidget *parent, const Request &request)
{
    Request local = request;
    local.localOnly = true;
    const QList<QUrl> urls = choose(parent, local);
    return !urls.isEmpty() && urls.constFirst().isLocalFile() ? urls.constFirst().toLocalFile() : QString();
}

QDBusArgument &operator<<(QDBusArgument &argument, const PortalFilterRule &rule)
{
    argument.beginStructure();
    argument << rule.type << rule.pattern;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalFilterRule &rule)
{
    argument.beginStructure();
    argument >> rule.type >> rule.pattern;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const PortalFilter &filter)
{
    argument.beginStructure();
    argument << filter.name << filter.rules;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalFilter &filter)
{
    argument.beginStructure();
    argument >> filter.name >> filter.rules;
    argument.endStructure();
    return argument;
}

}

#include "FileChooser.moc"
