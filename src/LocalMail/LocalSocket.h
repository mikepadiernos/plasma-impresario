/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef LOCALMAIL_LOCALSOCKET_H
#define LOCALMAIL_LOCALSOCKET_H

#include <memory>

#include "Streams/Socket.h"
#include "Streams/SocketFactory.h"

namespace LocalMail {

class ImapSession;
class Store;

/** @short A connection to an ImapSession in this process, serving mail from a Store */
class LocalSocket : public Streams::Socket
{
    Q_OBJECT
public:
    explicit LocalSocket(std::shared_ptr<Store> store);
    ~LocalSocket() override;

    bool canReadLine() override;
    QByteArray read(qint64 maxSize) override;
    QByteArray readLine(qint64 maxSize = 0) override;
    qint64 write(const QByteArray &byteArray) override;
    /** Nothing leaves the process, so there is nothing to encrypt; the session never offers STARTTLS */
    void startTls() override;
    void startDeflate() override;
    bool isDead() override;
    void close() override;

private:
    void connected();
    void deliver(const QByteArray &data);

    ImapSession *m_session = nullptr;
    QByteArray m_buffer;
    bool m_dead = false;
};

/** @short Make connections to the mail kept in one Store */
class LocalSocketFactory : public Streams::SocketFactory
{
    Q_OBJECT
public:
    explicit LocalSocketFactory(std::shared_ptr<Store> store);
    Streams::Socket *create() override;
    /** Proxies do not apply to a connection which stays in this process */
    void setProxySettings(const Streams::ProxySettings proxySettings, const QString &protocolTag) override;

private:
    std::shared_ptr<Store> m_store;
};

}

#endif
