/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ConnectionStates.h"

namespace Imap {
namespace Mailbox {

bool ConnectionStates::update(const uint connection, const ConnectionState state)
{
    const Summary before = summary();
    if (state == CONN_STATE_NONE || state == CONN_STATE_LOGOUT)
        m_states.remove(connection);
    else
        m_states.insert(connection, state);
    return summary() != before;
}

void ConnectionStates::clear()
{
    m_states.clear();
}

ConnectionStates::Summary ConnectionStates::summary() const
{
    if (m_states.isEmpty())
        return Summary::Disconnected;
    for (const ConnectionState state : m_states) {
        if (state >= CONN_STATE_AUTHENTICATED)
            return Summary::Connected;
    }
    return Summary::Connecting;
}

ConnectionState ConnectionStates::furthest() const
{
    ConnectionState result = CONN_STATE_NONE;
    for (const ConnectionState state : m_states)
        result = qMax(result, state);
    return result;
}

int ConnectionStates::count() const
{
    return m_states.size();
}

}
}
