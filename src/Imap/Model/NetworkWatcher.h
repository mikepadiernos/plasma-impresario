/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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

#ifndef TROJITA_IMAP_NETWORKWATCHER_H
#define TROJITA_IMAP_NETWORKWATCHER_H

#include <QObject>
#include "Imap/ConnectionState.h"
#include "Imap/Model/NetworkPolicy.h"

class QTimer;

namespace Imap {
class ImapAccess;
namespace Mailbox {
class Model;

const int MIN_RECONNECT_TIMEOUT = 5*1000;
const int MAX_RECONNECT_TIMEOUT = 5*60*1000;

/** @short Provide network session management to the Model */
class NetworkWatcher: public QObject
{
    Q_OBJECT
public:
    NetworkWatcher(ImapAccess *parent, Model *model);

    NetworkPolicy desiredNetworkPolicy() const;
    NetworkPolicy effectiveNetworkPolicy() const;

    /** @short Whether the system is going to sleep or sleeping; see setSystemSleeping() */
    bool isSystemSleeping() const;

public slots:
    virtual void setNetworkOffline();
    virtual void setNetworkExpensive();
    virtual void setNetworkOnline();

    /** @short Log out before the system sleeps and connect again once it wakes

    The user's network policy is kept, so desiredNetworkPolicyChanged() is not emitted. Connections which
    would not survive the sleep are closed rather than left to time out after waking.
    */
    void setSystemSleeping(bool sleeping);

signals:
    void effectiveNetworkPolicyChanged();
    void reconnectAttemptScheduled(const int timeout);
    void resetReconnectState();
    void desiredNetworkPolicyChanged(const Imap::Mailbox::NetworkPolicy policy);

protected:
    virtual void setDesiredNetworkPolicy(const NetworkPolicy policy) = 0;
    ImapAccess *m_imapAccess;
    Model *m_model;
    NetworkPolicy m_desiredPolicy;
    bool m_systemSleeping = false;

protected slots:
    void attemptReconnect();
    void tryReconnect();
    void handleConnectionStateChanged(uint parserId, Imap::ConnectionState state);

protected:
    void resetReconnectTimer();

    /** @short Single shot timer to trigger reconnect attempts with time-outs */
    QTimer *m_reconnectTimer;
};

}
}

#endif
