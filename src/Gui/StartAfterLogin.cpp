/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/StartAfterLogin.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QGuiApplication>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QWidget>

#include <KLocalizedString>

#include "UiUtils/BackgroundRunning.h"

namespace Gui {

namespace {

const QString portalService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString portalPath = QStringLiteral("/org/freedesktop/portal/desktop");

QString profile()
{
    return QString::fromUtf8(qgetenv("TROJITA_PROFILE"));
}

void warn(QWidget *parent, const QString &text)
{
    QMessageBox::warning(parent, i18nc("@title:window", "Start After Login"), text);
}

}

StartAfterLogin::StartAfterLogin(bool enabled, QWidget *parent)
    : QObject(qApp)
    , m_enabled(enabled)
    , m_parent(parent)
{
}

QString StartAfterLogin::portalReason()
{
    return i18nc("@info shown by the desktop when asking to run in the background",
                 "Plasma Impresario checks for new mail and shows notifications while its window is closed.");
}

bool StartAfterLogin::apply(bool enabled, QWidget *parent)
{
    namespace Background = UiUtils::BackgroundRunning;
    if (Background::isSandboxed())
        return requestFromPortal(enabled, parent);

    const QString path = Background::autostartPath(QGuiApplication::desktopFileName());
    QString error;
    if (enabled) {
        const QByteArray entry = Background::autostartEntry(
                QGuiApplication::applicationDisplayName(), QGuiApplication::desktopFileName(),
                Background::commandLine(QCoreApplication::applicationFilePath(), profile()));
        if (Background::writeAutostartEntry(path, entry, &error))
            return true;
        warn(parent, xi18nc("@info", "Plasma Impresario could not be set to start after login, because the file "
                                     "<filename>%1</filename> could not be written: %2", path, error));
        return false;
    }
    if (Background::removeAutostartEntry(path, &error))
        return true;
    warn(parent, xi18nc("@info", "Plasma Impresario will still start after login, because the file "
                                 "<filename>%1</filename> could not be removed: %2", path, error));
    return false;
}

bool StartAfterLogin::requestFromPortal(bool enabled, QWidget *parent)
{
    namespace Background = UiUtils::BackgroundRunning;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        warn(parent, i18nc("@info", "The desktop's background service cannot be reached, so whether Plasma Impresario "
                                    "starts after login could not be changed."));
        return false;
    }

    // Listen on the request's path before asking, so that a quick answer is not missed
    const QString token = QStringLiteral("plasma_impresario_%1").arg(QRandomGenerator::global()->generate());
    QString sender = bus.baseService().mid(1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    const QString requestPath = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
    auto *request = new StartAfterLogin(enabled, parent);
    bus.connect(portalService, requestPath, QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
                request, SLOT(portalResponse(uint, QVariantMap)));

    QVariantMap options = Background::portalOptions(
            enabled, Background::commandLine(QCoreApplication::applicationFilePath(), profile()), portalReason());
    options.insert(QStringLiteral("handle_token"), token);
    QDBusMessage call = QDBusMessage::createMethodCall(portalService, portalPath,
                                                       QStringLiteral("org.freedesktop.portal.Background"),
                                                       QStringLiteral("RequestBackground"));
    call << QString() << options;
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(call), request);
    connect(watcher, &QDBusPendingCallWatcher::finished, request, [request](QDBusPendingCallWatcher *finished) {
        finished->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply = *finished;
        if (!reply.isError())
            return;
        warn(request->m_parent, xi18nc("@info", "The desktop's background service did not accept the request: %1",
                                       reply.error().message()));
        request->deleteLater();
    });
    return true;
}

void StartAfterLogin::portalResponse(uint response, const QVariantMap &results)
{
    deleteLater();
    // 0 is success, 1 is the user's refusal and 2 any other end
    if (response == 0 && results.value(QStringLiteral("autostart")).toBool() == m_enabled)
        return;
    if (m_enabled)
        warn(m_parent, i18nc("@info", "The desktop did not allow Plasma Impresario to start after login. You can allow "
                                      "it in the system settings of applications."));
    else
        warn(m_parent, i18nc("@info", "The desktop did not confirm that Plasma Impresario no longer starts after login. "
                                      "You can check it in the system settings of applications."));
}

}
