/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <QPair>
#include <QStringList>

class QDBusServiceWatcher;

namespace Gui {

/** @short The Plasma Activities, read from the activity manager over D-Bus

Plasma starts the activity manager with the session; without it, there are no
activities and available() is false.
*/
class PlasmaActivities : public QObject
{
    Q_OBJECT
public:
    explicit PlasmaActivities(QObject *parent = nullptr);

    /** @short Whether the activity manager is running */
    bool available() const;
    /** @short The activities with their names, in the order of the activity manager */
    QList<QPair<QString, QString>> activities() const;
    /** @short The current activity, or an empty string */
    QString current() const;

signals:
    void currentChanged(const QString &activityId);
    void availableChanged(bool available);

private slots:
    void slotCurrentActivityChanged(const QString &activityId);

private:
    void connectToManager();

    QDBusServiceWatcher *m_watcher;
    bool m_connected = false;
};

} // namespace Gui
