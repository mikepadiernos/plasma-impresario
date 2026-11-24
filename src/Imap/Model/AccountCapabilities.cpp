/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QObject>
#include "AccountCapabilities.h"

namespace Imap {

AccountCapabilities::AccountCapabilities(AccountProtocol protocol)
    : m_protocol(protocol)
{
}

AccountProtocol AccountCapabilities::protocol() const
{
    return m_protocol;
}

bool AccountCapabilities::supports(AccountOperation operation) const
{
    switch (m_protocol) {
    case AccountProtocol::Imap:
    case AccountProtocol::Jmap:
        return true;
    case AccountProtocol::Pop3:
        return operation == AccountOperation::LeaveMessagesOnServer;
    }
    Q_UNREACHABLE();
}

QString AccountCapabilities::limitation(AccountOperation operation) const
{
    if (supports(operation))
        return QString();
    switch (operation) {
    case AccountOperation::ServerFolders:
        return QObject::tr("POP3 accounts have a single inbox and no server-side folders.");
    case AccountOperation::ServerSideFlags:
        return QObject::tr("POP3 does not store read, flagged or answered state on the server.");
    case AccountOperation::ServerSideSearch:
        return QObject::tr("POP3 cannot search messages on the server.");
    case AccountOperation::ServerSideMove:
        return QObject::tr("POP3 cannot move messages between server folders.");
    case AccountOperation::SaveDraftsOnServer:
        return QObject::tr("POP3 cannot store drafts on the server.");
    case AccountOperation::PushNotifications:
        return QObject::tr("This protocol does not provide push notifications.");
    case AccountOperation::LeaveMessagesOnServer:
        break;
    }
    return QObject::tr("This operation is not available for the account protocol.");
}

AccountProtocol AccountCapabilities::protocolFromName(const QString &name, bool *ok)
{
    const QString normalized = name.toLower();
    *ok = true;
    if (normalized == QLatin1String("imap"))
        return AccountProtocol::Imap;
    if (normalized == QLatin1String("pop3"))
        return AccountProtocol::Pop3;
    if (normalized == QLatin1String("jmap"))
        return AccountProtocol::Jmap;
    *ok = false;
    return AccountProtocol::Imap;
}

}
