/* Copyright (C) 2013 Pali Rohár <pali.rohar@gmail.com>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDebug>

#include "Gui/Window.h"
#include "KRunnerBridge.h"
#include "MainWindowBridge.h"

#include "IPC.h"

namespace {

static constexpr QLatin1StringView path("/Trojita");

QString service()
{
    QString str = QStringLiteral("net.flaska.trojita");
    QString profileName = QString::fromUtf8(qgetenv("TROJITA_PROFILE"));
    return profileName.isEmpty() ? str : str + QLatin1Char('-') + profileName;
}
}

namespace IPC {
namespace Instance {

static QDBusInterface &interface()
{
    static QDBusInterface iface(service(), path, QStringLiteral("net.flaska.trojita"));
    return iface;
}

bool isRunning()
{
    return QDBusConnection::sessionBus().isConnected() &&
        QDBusConnection::sessionBus().interface()->isServiceRegistered(service());
}

void handOverActivationToken(const QString &token)
{
    // Messages on one connection arrive in order, so the token is there before the request it belongs to
    if (!token.isEmpty())
        interface().call(QDBus::NoBlock, QStringLiteral("setActivationToken"), token);
}

void showMainWindow()
{
    interface().call(QDBus::NoBlock, QStringLiteral("showMainWindow"));
}

void showAddressbookWindow()
{
    interface().call(QDBus::NoBlock, QStringLiteral("showAddressbookWindow"));
}

void composeMail(const QString &url)
{
    interface().call(QDBus::NoBlock, QStringLiteral("composeMail"), url);
}

void composeMail(const QString &url, const QStringList &files)
{
    interface().call(QDBus::NoBlock, QStringLiteral("composeMailWithFiles"), url, files);
}

void openMessage(const QString &messageId)
{
    interface().call(QDBus::NoBlock, QStringLiteral("openMessage"), messageId);
}

} //namespace Instance

bool registerInstance(Gui::MainWindow *window, QString &error)
{
    QDBusConnection conn = QDBusConnection::sessionBus();
    if (!conn.isConnected()) {
        error = conn.lastError().message();
        return false;
    }

    bool serviceOk = conn.registerService(service());
    if (!serviceOk) {
        error = conn.lastError().message();
        return false;
    }

    MainWindowBridge *object = new MainWindowBridge(window);
    bool objectOk = conn.registerObject(path, object, QDBusConnection::ExportAllSlots);
    if (!objectOk) {
        error = conn.lastError().message();
        delete object;
        conn.unregisterService(service());
        return false;
    }

    // Plasma Search asks the same service; the window answers only once the user turned it on
    auto *runner = new KRunnerBridge(
        [window](const QString &query) {
            RunnerMatches result;
            const auto matches = window->plasmaSearchMatches(query);
            for (const auto &match : matches) {
                RunnerMatch shown;
                shown.id = match.id;
                shown.text = match.title;
                shown.iconName = QStringLiteral("mail-message");
                shown.categoryRelevance = KRunnerBridge::categoryRelevanceLow;
                shown.relevance = match.relevance;
                shown.properties.insert(QStringLiteral("subtext"), match.subtext);
                result << shown;
            }
            return result;
        },
        [window](const QString &matchId) { window->openPlasmaSearchMatch(matchId); },
        [window](const QString &token) { window->setActivationToken(token); }, window);
    if (!conn.registerObject(KRunnerBridge::objectPath(), runner, QDBusConnection::ExportAllSlots)) {
        // Only Plasma Search goes without mail
        qWarning() << "Cannot offer mail to Plasma Search:" << conn.lastError().message();
        delete runner;
    }

    return true;
}

} //namespace IPC
