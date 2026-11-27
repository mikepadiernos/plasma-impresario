/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "MailStatusClient.h"

#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QWindow>

#include <KIO/ApplicationLauncherJob>
#include <KService>
#include <KWaylandExtras>
#include <KWindowSystem>

#include <algorithm>

namespace PlasmaWidgets {

namespace {

using Entries = QList<QVariantMap>;

/** The application's desktop ID, which names it when it is started and when a token is asked for */
const QLatin1String desktopId("io.github.mikepadiernos.plasma_impresario");

constexpr int maximumMessages = 50;

QVariantList toList(const Entries &entries)
{
    QVariantList result;
    result.reserve(entries.size());
    for (const QVariantMap &entry : entries)
        result << entry;
    return result;
}

}

QString MailStatusClient::defaultService()
{
    // The application still registers the name it inherited from Trojitá
    return QStringLiteral("net.flaska.trojita");
}

QString MailStatusClient::objectPath()
{
    return QStringLiteral("/MailStatus");
}

QString MailStatusClient::interfaceName()
{
    return QStringLiteral("io.github.mikepadiernos.PlasmaImpresario.MailStatus1");
}

MailStatusClient::MailStatusClient(QObject *parent)
    : MailStatusClient(QDBusConnection::sessionBus(), parent)
{
}

MailStatusClient::MailStatusClient(const QDBusConnection &bus, QObject *parent)
    : QObject(parent)
    , m_bus(bus)
    , m_service(defaultService())
{
    qDBusRegisterMetaType<Entries>();
    m_watcher = new QDBusServiceWatcher(this);
    m_watcher->setConnection(m_bus);
    m_watcher->setWatchMode(QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration);
    connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this, &MailStatusClient::serviceAppeared);
    connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, &MailStatusClient::serviceVanished);
    watch();
}

QString MailStatusClient::service() const
{
    return m_service;
}

void MailStatusClient::setService(const QString &service)
{
    if (service == m_service)
        return;
    m_bus.disconnect(m_service, objectPath(), interfaceName(), QStringLiteral("Changed"), this, SLOT(onChanged()));
    m_service = service;
    ++m_generation;
    setRunning(false);
    setStale(true);
    m_allAccounts.clear();
    applyAccounts();
    setMessages({});
    emit serviceChanged();
    watch();
}

bool MailStatusClient::isRunning() const
{
    return m_running;
}

bool MailStatusClient::isStale() const
{
    return m_stale;
}

bool MailStatusClient::isLoaded() const
{
    return m_loaded;
}

QString MailStatusClient::sharing() const
{
    return m_sharing;
}

QVariantList MailStatusClient::accounts() const
{
    return m_accounts;
}

QVariantList MailStatusClient::allAccounts() const
{
    return m_allAccounts;
}

int MailStatusClient::totalUnread() const
{
    return m_totalUnread;
}

QVariantList MailStatusClient::messages() const
{
    return m_messages;
}

bool MailStatusClient::wantsMessages() const
{
    return m_wantsMessages;
}

void MailStatusClient::setWantsMessages(bool wants)
{
    if (wants == m_wantsMessages)
        return;
    m_wantsMessages = wants;
    emit wantsMessagesChanged();
    if (!wants)
        setMessages({});
    refresh();
}

QStringList MailStatusClient::accountFilter() const
{
    return m_accountFilter;
}

void MailStatusClient::setAccountFilter(const QStringList &filter)
{
    if (filter == m_accountFilter)
        return;
    m_accountFilter = filter;
    emit accountFilterChanged();
    applyAccounts();
    if (m_wantsMessages)
        refresh();
}

QStringList MailStatusClient::folderFilter() const
{
    return m_folderFilter;
}

void MailStatusClient::setFolderFilter(const QStringList &filter)
{
    if (filter == m_folderFilter)
        return;
    m_folderFilter = filter;
    emit folderFilterChanged();
    if (m_wantsMessages)
        refresh();
}

int MailStatusClient::messageLimit() const
{
    return m_messageLimit;
}

void MailStatusClient::setMessageLimit(int limit)
{
    limit = std::clamp(limit, 1, maximumMessages);
    if (limit == m_messageLimit)
        return;
    m_messageLimit = limit;
    emit messageLimitChanged();
    if (m_wantsMessages)
        refresh();
}

QDateTime MailStatusClient::lastUpdate() const
{
    return m_lastUpdate;
}

void MailStatusClient::setTokenSource(const TokenSource &source)
{
    m_tokenSource = source;
}

void MailStatusClient::watch()
{
    m_watcher->setWatchedServices({m_service});
    if (m_service.isEmpty())
        return;
    // The match follows the name to whichever process owns it, so it holds across restarts of the application
    m_bus.connect(m_service, objectPath(), interfaceName(), QStringLiteral("Changed"), this, SLOT(onChanged()));
    checkOwner();
}

void MailStatusClient::checkOwner()
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
                                                          QStringLiteral("/org/freedesktop/DBus"),
                                                          QStringLiteral("org.freedesktop.DBus"),
                                                          QStringLiteral("NameHasOwner"));
    message.setArguments({m_service});
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message, callTimeout), this);
    const QString asked = m_service;
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, asked]() {
        watcher->deleteLater();
        QDBusPendingReply<bool> reply = *watcher;
        // The watcher reports what happens later; only the state at the start is read here
        if (asked != m_service || reply.isError())
            return;
        if (reply.value())
            serviceAppeared();
    });
}

void MailStatusClient::serviceAppeared()
{
    // Both the first check and the watcher may report the same start
    if (m_running)
        return;
    setRunning(true);
    refresh();
}

void MailStatusClient::serviceVanished()
{
    ++m_generation;
    setRunning(false);
    setStale(true);
    // The accounts stay for reference; the messages could no longer be opened
    setMessages({});
}

void MailStatusClient::onChanged()
{
    refresh();
}

void MailStatusClient::setRunning(bool running)
{
    if (running == m_running)
        return;
    m_running = running;
    emit runningChanged();
}

void MailStatusClient::setStale(bool stale)
{
    if (stale == m_stale)
        return;
    m_stale = stale;
    emit staleChanged();
}

void MailStatusClient::applyAccounts()
{
    QVariantList chosen;
    int total = -1;
    for (const QVariant &entry : std::as_const(m_allAccounts)) {
        const QVariantMap account = entry.toMap();
        if (!m_accountFilter.isEmpty() && !m_accountFilter.contains(account.value(QStringLiteral("id")).toString()))
            continue;
        chosen << account;
        const int unread = account.value(QStringLiteral("unread"), -1).toInt();
        if (unread >= 0)
            total = std::max(total, 0) + unread;
    }
    m_accounts = chosen;
    m_totalUnread = total;
    emit accountsChanged();
}

void MailStatusClient::setMessages(const QVariantList &messages)
{
    if (messages == m_messages)
        return;
    m_messages = messages;
    emit messagesChanged();
}

bool MailStatusClient::sharesMessages() const
{
    return m_sharing == QLatin1String("senders") || m_sharing == QLatin1String("subjects");
}

QDBusPendingCallWatcher *MailStatusClient::call(const QString &method, const QVariantList &arguments)
{
    QDBusMessage message = QDBusMessage::createMethodCall(m_service, objectPath(), interfaceName(), method);
    message.setArguments(arguments);
    return new QDBusPendingCallWatcher(m_bus.asyncCall(message, callTimeout), this);
}

void MailStatusClient::refresh()
{
    if (!m_running)
        return;
    const int generation = ++m_generation;
    auto *sharingCall = call(QStringLiteral("Sharing"));
    connect(sharingCall, &QDBusPendingCallWatcher::finished, this, [this, sharingCall, generation]() {
        sharingCall->deleteLater();
        if (generation != m_generation)
            return;
        QDBusPendingReply<QString> sharing = *sharingCall;
        if (sharing.isError()) {
            fail(sharing.error().message());
            return;
        }
        if (sharing.value() != m_sharing) {
            m_sharing = sharing.value();
            emit sharingChanged();
        }
        auto *accountsCall = call(QStringLiteral("Accounts"));
        connect(accountsCall, &QDBusPendingCallWatcher::finished, this, [this, accountsCall, generation]() {
            accountsCall->deleteLater();
            if (generation != m_generation)
                return;
            QDBusPendingReply<Entries> accounts = *accountsCall;
            if (accounts.isError()) {
                fail(accounts.error().message());
                return;
            }
            m_allAccounts = toList(accounts.value());
            applyAccounts();
            if (m_wantsMessages && sharesMessages()) {
                requestMessages(generation);
            } else {
                setMessages({});
                finishRefresh();
            }
        });
    });
}

void MailStatusClient::requestMessages(int generation)
{
    auto *messagesCall = call(QStringLiteral("UnreadMessages"), {m_accountFilter, m_folderFilter, m_messageLimit});
    connect(messagesCall, &QDBusPendingCallWatcher::finished, this, [this, messagesCall, generation]() {
        messagesCall->deleteLater();
        if (generation != m_generation)
            return;
        QDBusPendingReply<Entries> messages = *messagesCall;
        if (messages.isError()) {
            fail(messages.error().message());
            return;
        }
        setMessages(toList(messages.value()));
        finishRefresh();
    });
}

void MailStatusClient::finishRefresh()
{
    m_lastUpdate = QDateTime::currentDateTime();
    setStale(false);
    m_loaded = true;
    emit loadedChanged();
}

void MailStatusClient::fail(const QString &message)
{
    setStale(true);
    emit requestFailed(message);
}

void MailStatusClient::callWithToken(const QString &method, const QVariantList &arguments)
{
    const auto send = [this, method, arguments](const QString &token) {
        QVariantList withToken = arguments;
        withToken << token;
        auto *watcher = call(method, withToken);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
            watcher->deleteLater();
            QDBusPendingReply<> reply = *watcher;
            if (reply.isError())
                emit requestFailed(reply.error().message());
        });
    };
    if (m_tokenSource) {
        m_tokenSource(send);
        return;
    }
    // On Wayland a window comes forward only with a token from the compositor, asked for by the widget's window
    if (KWindowSystem::isPlatformWayland()) {
        QWindow *window = QGuiApplication::focusWindow();
        const auto windows = QGuiApplication::topLevelWindows();
        if (!window && !windows.isEmpty())
            window = windows.constFirst();
        if (window) {
            auto *watcher = new QFutureWatcher<QString>(this);
            connect(watcher, &QFutureWatcherBase::finished, this, [watcher, send]() {
                watcher->deleteLater();
                send(watcher->future().resultCount() > 0 ? watcher->result() : QString());
            });
            watcher->setFuture(KWaylandExtras::xdgActivationToken(window, desktopId));
            return;
        }
    }
    send(QString());
}

void MailStatusClient::openMessage(const QString &messageId)
{
    if (messageId.isEmpty() || !m_running)
        return;
    callWithToken(QStringLiteral("OpenMessage"), {messageId});
}

void MailStatusClient::compose()
{
    if (!m_running)
        return;
    callWithToken(QStringLiteral("Compose"), {});
}

void MailStatusClient::showAccount(const QString &accountId)
{
    if (!m_running)
        return;
    callWithToken(QStringLiteral("ShowAccount"), {accountId});
}

void MailStatusClient::start()
{
    if (m_running) {
        showAccount(QString());
        return;
    }
    const KService::Ptr application = KService::serviceByDesktopName(desktopId);
    if (!application) {
        emit requestFailed(QStringLiteral("Plasma Impresario is not installed"));
        return;
    }
    // The launcher takes care of startup feedback and of the activation token
    auto *job = new KIO::ApplicationLauncherJob(application);
    connect(job, &KJob::result, this, [this, job]() {
        if (job->error())
            emit requestFailed(job->errorString());
    });
    job->start();
}

}
