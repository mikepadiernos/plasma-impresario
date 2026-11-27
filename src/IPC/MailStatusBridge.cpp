/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "MailStatusBridge.h"

#include <QDBusMetaType>
#include <QTimer>

namespace IPC {

MailStatusBridge::MailStatusBridge(Handlers handlers, QObject *parent)
    : QObject(parent)
    , m_handlers(std::move(handlers))
    , m_changeTimer(new QTimer(this))
{
    m_changeTimer->setSingleShot(true);
    m_changeTimer->setInterval(changeDelay);
    connect(m_changeTimer, &QTimer::timeout, this, &MailStatusBridge::Changed);
}

QString MailStatusBridge::objectPath()
{
    return QStringLiteral("/MailStatus");
}

QString MailStatusBridge::interfaceName()
{
    return QStringLiteral("io.github.mikepadiernos.PlasmaImpresario.MailStatus1");
}

void MailStatusBridge::registerTypes()
{
    qDBusRegisterMetaType<StatusEntries>();
}

void MailStatusBridge::notifyChanged()
{
    if (!m_changeTimer->isActive())
        m_changeTimer->start();
}

int MailStatusBridge::Version()
{
    return interfaceVersion;
}

QString MailStatusBridge::Sharing()
{
    return m_handlers.sharing ? m_handlers.sharing() : QStringLiteral("nothing");
}

StatusEntries MailStatusBridge::Accounts()
{
    return m_handlers.accounts ? m_handlers.accounts() : StatusEntries();
}

StatusEntries MailStatusBridge::UnreadMessages(const QStringList &accounts, const QStringList &folders, int limit)
{
    if (!m_handlers.unreadMessages || limit <= 0)
        return {};
    return m_handlers.unreadMessages(accounts, folders, limit);
}

void MailStatusBridge::OpenMessage(const QString &messageId, const QString &activationToken)
{
    if (messageId.isEmpty() || !m_handlers.openMessage)
        return;
    handOver(activationToken);
    m_handlers.openMessage(messageId);
}

void MailStatusBridge::Compose(const QString &activationToken)
{
    if (!m_handlers.compose)
        return;
    handOver(activationToken);
    m_handlers.compose();
}

void MailStatusBridge::ShowAccount(const QString &accountId, const QString &activationToken)
{
    if (!m_handlers.showAccount)
        return;
    handOver(activationToken);
    m_handlers.showAccount(accountId);
}

void MailStatusBridge::handOver(const QString &token)
{
    if (!token.isEmpty() && m_handlers.handOverToken)
        m_handlers.handOverToken(token);
}

} // namespace IPC
