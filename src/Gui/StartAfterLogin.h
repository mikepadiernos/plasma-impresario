/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <QPointer>
#include <QVariantMap>

class QWidget;

namespace Gui {

/** @short Starting the application after login

Writes or removes an XDG autostart entry, or inside a sandbox asks the XDG Background portal, whose answer arrives
later. A refusal or failure is shown to the user, next to @arg parent.
*/
class StartAfterLogin : public QObject
{
    Q_OBJECT
public:
    /** Apply the choice stored in the settings; returns false when the choice could not be applied at once */
    static bool apply(bool enabled, QWidget *parent);

private slots:
    void portalResponse(uint response, const QVariantMap &results);

private:
    StartAfterLogin(bool enabled, QWidget *parent);
    static bool requestFromPortal(bool enabled, QWidget *parent);
    static QString portalReason();

    bool m_enabled;
    QPointer<QWidget> m_parent;
};

}
