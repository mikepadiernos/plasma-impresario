/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef IMPRESARIO_IMAP_CONNECTIONSTATES_H
#define IMPRESARIO_IMAP_CONNECTIONSTATES_H

#include <QHash>
#include "Imap/ConnectionState.h"

namespace Imap {
namespace Mailbox {

/** @short The state of an account made of the states of all its connections

The model may keep several connections to one server, each reporting its own state. An account is connected while any
of them is logged in, so one connection closing or logging in again does not make the whole account look disconnected.
A connection is forgotten once it reports CONN_STATE_LOGOUT or CONN_STATE_NONE.
*/
class ConnectionStates
{
public:
    enum class Summary {
        Disconnected, /**< No connection is open */
        Connecting, /**< Connections are open, none is logged in yet */
        Connected, /**< At least one connection is logged in */
    };

    /** @short Record @p state of @p connection; returns whether summary() changed */
    bool update(uint connection, ConnectionState state);
    void clear();
    Summary summary() const;
    /** @short The furthest state of the open connections, CONN_STATE_NONE without any */
    ConnectionState furthest() const;
    /** @short The number of open connections */
    int count() const;

private:
    QHash<uint, ConnectionState> m_states;
};

}
}

#endif
