/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "PlasmaActivities.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusServiceWatcher>

namespace {
const QString service = QStringLiteral("org.kde.ActivityManager");
const QString path = QStringLiteral("/ActivityManager/Activities");
const QString interface = QStringLiteral("org.kde.ActivityManager.Activities");
// The activity manager answers at once; a short limit keeps the window responsive when it does not
constexpr int timeout = 2000;

QDBusMessage call(const QString &method, const QVariantList &arguments = {})
{
    QDBusMessage message = QDBusMessage::createMethodCall(service, path, interface, method);
    message.setArguments(arguments);
    return QDBusConnection::sessionBus().call(message, QDBus::Block, timeout);
}
}

namespace Gui {

PlasmaActivities::PlasmaActivities(QObject *parent)
    : QObject(parent)
    , m_watcher(new QDBusServiceWatcher(service, QDBusConnection::sessionBus(),
          QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration, this))
{
    connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this, [this]() {
        connectToManager();
        emit availableChanged(true);
        emit currentChanged(current());
    });
    connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this]() { emit availableChanged(false); });
    if (available())
        connectToManager();
}

bool PlasmaActivities::available() const
{
    const QDBusConnection bus = QDBusConnection::sessionBus();
    return bus.isConnected() && bus.interface() && bus.interface()->isServiceRegistered(service);
}

QList<QPair<QString, QString>> PlasmaActivities::activities() const
{
    QList<QPair<QString, QString>> result;
    if (!available())
        return result;
    const QDBusReply<QStringList> ids = call(QStringLiteral("ListActivities"));
    if (!ids.isValid())
        return result;
    for (const QString &id : ids.value()) {
        const QDBusReply<QString> name = call(QStringLiteral("ActivityName"), {id});
        result.append({id, name.isValid() && !name.value().isEmpty() ? name.value() : id});
    }
    return result;
}

QString PlasmaActivities::current() const
{
    if (!available())
        return QString();
    const QDBusReply<QString> reply = call(QStringLiteral("CurrentActivity"));
    return reply.isValid() ? reply.value() : QString();
}

void PlasmaActivities::connectToManager()
{
    if (m_connected)
        return;
    m_connected = QDBusConnection::sessionBus().connect(service, path, interface, QStringLiteral("CurrentActivityChanged"),
        this, SLOT(slotCurrentActivityChanged(QString)));
}

void PlasmaActivities::slotCurrentActivityChanged(const QString &activityId)
{
    emit currentChanged(activityId);
}

} // namespace Gui
