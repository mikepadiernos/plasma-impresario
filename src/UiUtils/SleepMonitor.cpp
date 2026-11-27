/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "SleepMonitor.h"

namespace UiUtils {

SleepMonitor::SleepMonitor(QObject *parent)
    : QObject(parent)
{
}

QString SleepMonitor::loginService()
{
    return QStringLiteral("org.freedesktop.login1");
}

bool SleepMonitor::start(const QDBusConnection &bus, const QString &service)
{
    if (!bus.isConnected())
        return false;
    QDBusConnection connection(bus);
    return connection.connect(service, QStringLiteral("/org/freedesktop/login1"),
                              QStringLiteral("org.freedesktop.login1.Manager"), QStringLiteral("PrepareForSleep"), this,
                              SLOT(prepareForSleep(bool)));
}

bool SleepMonitor::startOnSystemBus()
{
    return start(QDBusConnection::systemBus());
}

bool SleepMonitor::isSleeping() const
{
    return m_sleeping;
}

void SleepMonitor::prepareForSleep(bool sleeping)
{
    if (sleeping == m_sleeping)
        return;
    m_sleeping = sleeping;
    emit sleepingChanged(sleeping);
}

}
