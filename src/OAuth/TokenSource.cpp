/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "TokenSource.h"

namespace OAuth {

TokenSource::TokenSource(QObject *parent)
    : QObject(parent)
{
}

TokenSource::~TokenSource() = default;

void TokenSource::setServer(const QString &host, quint16 port)
{
    m_host = host;
    m_port = port;
}

QString TokenSource::host() const
{
    return m_host;
}

quint16 TokenSource::port() const
{
    return m_port;
}

Error TokenSource::lastError() const
{
    return m_lastError;
}

void TokenSource::setLastError(Error error)
{
    m_lastError = error;
}

TokenSource *TokenSource::find(const QObject *owner)
{
    return owner ? owner->findChild<TokenSource *>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
}

}
