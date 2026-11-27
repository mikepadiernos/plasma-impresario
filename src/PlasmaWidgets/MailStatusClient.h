/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef PLASMAWIDGETS_MAILSTATUSCLIENT_H
#define PLASMAWIDGETS_MAILSTATUSCLIENT_H

#include <QDBusConnection>
#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>

class QDBusPendingCallWatcher;
class QDBusServiceWatcher;

namespace PlasmaWidgets {

/** @short What a Plasma widget knows of the running Plasma Impresario

The widget never reads mail or credentials; it asks the application over the session bus, through the interface which
IPC::MailStatusBridge offers, and refreshes when the application reports a change. Every call is asynchronous, so that a
busy or stuck application cannot freeze Plasma.

The state is honest about its age: when the application quits, stops answering or answers with an error, the last data
stay for reference but stale turns true, and the account states are no longer current.
*/
class MailStatusClient : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    /** The D-Bus service of the application; changed only by tests and profiles */
    Q_PROPERTY(QString service READ service WRITE setService NOTIFY serviceChanged)
    /** Whether the application runs, as seen on the session bus */
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    /** Whether the data shown are not current: the application is not running or its last answer failed */
    Q_PROPERTY(bool stale READ isStale NOTIFY staleChanged)
    /** True once the first answer arrived, so that a widget can tell "nothing yet" from "nothing unread" */
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY loadedChanged)
    /** What the application shares: "nothing", "counts", "senders" or "subjects"; empty while not known */
    Q_PROPERTY(QString sharing READ sharing NOTIFY sharingChanged)
    /** The accounts chosen by accountFilter, each with "id", "name", "unread" and "state" */
    Q_PROPERTY(QVariantList accounts READ accounts NOTIFY accountsChanged)
    /** Every account, for choosing them in the widget settings */
    Q_PROPERTY(QVariantList allAccounts READ allAccounts NOTIFY accountsChanged)
    /** The unread messages in the chosen accounts' INBOX; -1 while no count is known */
    Q_PROPERTY(int totalUnread READ totalUnread NOTIFY accountsChanged)
    /** The newest unread messages in the chosen accounts and folders, when the application shares them */
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)
    /** Whether to ask for messages at all; the status widget leaves it off */
    Q_PROPERTY(bool wantsMessages READ wantsMessages WRITE setWantsMessages NOTIFY wantsMessagesChanged)
    /** Account IDs to show; empty shows every account */
    Q_PROPERTY(QStringList accountFilter READ accountFilter WRITE setAccountFilter NOTIFY accountFilterChanged)
    /** Folder names whose unread messages to show; empty shows every indexed folder */
    Q_PROPERTY(QStringList folderFilter READ folderFilter WRITE setFolderFilter NOTIFY folderFilterChanged)
    /** The most messages asked for */
    Q_PROPERTY(int messageLimit READ messageLimit WRITE setMessageLimit NOTIFY messageLimitChanged)
    /** When the last complete answer arrived */
    Q_PROPERTY(QDateTime lastUpdate READ lastUpdate NOTIFY loadedChanged)

public:
    /** The service of the application without a profile */
    static QString defaultService();
    static QString objectPath();
    static QString interfaceName();
    /** How long an answer is awaited before the data count as stale */
    static constexpr int callTimeout = 5000;

    explicit MailStatusClient(QObject *parent = nullptr);
    /** For tests: talk over @arg bus instead of the session bus */
    MailStatusClient(const QDBusConnection &bus, QObject *parent);

    QString service() const;
    void setService(const QString &service);
    bool isRunning() const;
    bool isStale() const;
    bool isLoaded() const;
    QString sharing() const;
    QVariantList accounts() const;
    QVariantList allAccounts() const;
    int totalUnread() const;
    QVariantList messages() const;
    bool wantsMessages() const;
    void setWantsMessages(bool wants);
    QStringList accountFilter() const;
    void setAccountFilter(const QStringList &filter);
    QStringList folderFilter() const;
    void setFolderFilter(const QStringList &filter);
    int messageLimit() const;
    void setMessageLimit(int limit);
    QDateTime lastUpdate() const;

    /** @short Replace how a window activation token is obtained, for tests; the
     * callback gets an empty token without one */
    using TokenSource = std::function<void(const std::function<void(const QString &)> &)>;
    void setTokenSource(const TokenSource &source);

    Q_INVOKABLE void refresh();
    /** Open the message named by the "id" of an entry of messages */
    Q_INVOKABLE void openMessage(const QString &messageId);
    Q_INVOKABLE void compose();
    /** Show the window with the account; an empty ID only shows the window */
    Q_INVOKABLE void showAccount(const QString &accountId);
    /** Start the application when it is not running, or show its window */
    Q_INVOKABLE void start();

signals:
    void serviceChanged();
    void runningChanged();
    void staleChanged();
    void loadedChanged();
    void sharingChanged();
    void accountsChanged();
    void messagesChanged();
    void wantsMessagesChanged();
    void accountFilterChanged();
    void folderFilterChanged();
    void messageLimitChanged();
    /** A request could not be delivered; @arg message is for logs, not for the widget */
    void requestFailed(const QString &message);

private slots:
    void serviceAppeared();
    void serviceVanished();
    /** The application reported a change */
    void onChanged();

private:
    void watch();
    void checkOwner();
    void setRunning(bool running);
    void setStale(bool stale);
    void applyAccounts();
    void setMessages(const QVariantList &messages);
    void requestMessages(int generation);
    void finishRefresh();
    void fail(const QString &message);
    QDBusPendingCallWatcher *call(const QString &method, const QVariantList &arguments = {});
    void callWithToken(const QString &method, const QVariantList &arguments);
    bool sharesMessages() const;

    QDBusConnection m_bus;
    QString m_service;
    QDBusServiceWatcher *m_watcher = nullptr;
    bool m_running = false;
    bool m_stale = true;
    bool m_loaded = false;
    QString m_sharing;
    QVariantList m_allAccounts;
    QVariantList m_accounts;
    int m_totalUnread = -1;
    QVariantList m_messages;
    bool m_wantsMessages = false;
    QStringList m_accountFilter;
    QStringList m_folderFilter;
    int m_messageLimit = 20;
    QDateTime m_lastUpdate;
    /** Counts the refreshes, so that an answer to an older one is dropped */
    int m_generation = 0;
    TokenSource m_tokenSource;
};

} // namespace PlasmaWidgets

#endif
