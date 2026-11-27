/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "LocalMail/LocalSocket.h"

#include <stdexcept>

#include <QTimer>

#include "LocalMail/ImapSession.h"
#include "LocalMail/Store.h"

namespace LocalMail {

LocalSocket::LocalSocket(std::shared_ptr<Store> store)
    : m_session(new ImapSession(std::move(store), this))
{
    connect(m_session, &ImapSession::output, this, &LocalSocket::deliver);
    connect(m_session, &ImapSession::finished, this, [this]() {
        QTimer::singleShot(0, this, &LocalSocket::close);
    });
    QTimer::singleShot(0, this, &LocalSocket::connected);
}

LocalSocket::~LocalSocket() = default;

void LocalSocket::connected()
{
    if (m_dead)
        return;
    emit stateChanged(Imap::CONN_STATE_CONNECTED_PRETLS_PRECAPS, QString());
    m_session->start();
}

void LocalSocket::deliver(const QByteArray &data)
{
    if (m_dead)
        return;
    m_buffer += data;
    emit readyRead();
}

bool LocalSocket::canReadLine()
{
    return m_buffer.contains('\n');
}

QByteArray LocalSocket::read(qint64 maxSize)
{
    const QByteArray result = m_buffer.left(maxSize);
    m_buffer.remove(0, result.size());
    return result;
}

QByteArray LocalSocket::readLine(qint64 maxSize)
{
    const int end = m_buffer.indexOf('\n');
    qint64 size = end < 0 ? m_buffer.size() : end + 1;
    if (maxSize > 0)
        size = std::min(size, maxSize);
    return read(size);
}

qint64 LocalSocket::write(const QByteArray &byteArray)
{
    if (m_dead)
        return -1;
    const QByteArray data = byteArray;
    QMetaObject::invokeMethod(this, [this, data]() {
        if (!m_dead)
            m_session->receive(data);
    }, Qt::QueuedConnection);
    return byteArray.size();
}

void LocalSocket::startTls()
{
    throw std::invalid_argument("Mail kept on this computer is not reached over a network and cannot use STARTTLS");
}

void LocalSocket::startDeflate()
{
    throw std::invalid_argument("Mail kept on this computer is not reached over a network and cannot use compression");
}

bool LocalSocket::isDead()
{
    return m_dead;
}

void LocalSocket::close()
{
    if (m_dead)
        return;
    m_dead = true;
    emit disconnected(QString());
}

LocalSocketFactory::LocalSocketFactory(std::shared_ptr<Store> store)
    : m_store(std::move(store))
{
}

Streams::Socket *LocalSocketFactory::create()
{
    return new LocalSocket(m_store);
}

void LocalSocketFactory::setProxySettings(const Streams::ProxySettings proxySettings, const QString &protocolTag)
{
    Q_UNUSED(proxySettings);
    Q_UNUSED(protocolTag);
}

}
