/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDBusConnection>
#include <QObject>

namespace UiUtils {

/** @short Follow systemd-logind's PrepareForSleep signal, so that mail connections can close before the system
sleeps and reconnect as soon as it wakes

Without logind on the system bus nothing is reported, and connections recover through their usual time-outs.
*/
class SleepMonitor : public QObject
{
    Q_OBJECT
public:
    explicit SleepMonitor(QObject *parent = nullptr);

    /** The D-Bus service name of systemd-logind */
    static QString loginService();

    /** Listen on @arg bus, accepting the signal from @arg service; an empty service accepts any sender

    Returns false when the bus is not connected.
    */
    bool start(const QDBusConnection &bus, const QString &service = loginService());

    /** Listen to logind on the system bus */
    bool startOnSystemBus();

    /** Whether the system announced that it is going to sleep and has not woken yet */
    bool isSleeping() const;

signals:
    void sleepingChanged(bool sleeping);

private slots:
    void prepareForSleep(bool sleeping);

private:
    bool m_sleeping = false;
};

}
