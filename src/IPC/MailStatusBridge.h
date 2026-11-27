/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef IPC_MAILSTATUSBRIDGE_H
#define IPC_MAILSTATUSBRIDGE_H

#include <QList>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

#include <functional>

class QTimer;

namespace IPC {

/** @short Entries as the widgets read them over D-Bus, signature aa{sv} */
using StatusEntries = QList<QVariantMap>;

/** @short The interface through which the optional Plasma widgets follow the mail

The widgets ask for the accounts and the unread messages, and are told with Changed() when either may have changed. What
they receive is limited by the application's sharing setting, which Sharing() names; the entries come from
UiUtils::MailStatus. The requests which bring a window forward carry Plasma's activation token, which is handed over
before the request is made. Finding the data and acting on the requests are left to the functions given, so that this
class knows nothing of the windows.
*/
class MailStatusBridge : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.mikepadiernos.PlasmaImpresario.MailStatus1")

public:
    struct Handlers {
        std::function<QString()> sharing;
        std::function<StatusEntries()> accounts;
        std::function<StatusEntries(const QStringList &accounts, const QStringList &folders, int limit)> unreadMessages;
        std::function<void(const QString &messageId)> openMessage;
        std::function<void()> compose;
        std::function<void(const QString &accountId)> showAccount;
        std::function<void(const QString &token)> handOverToken;
    };

    /** The version of the interface, which grows when methods are added */
    static constexpr int interfaceVersion = 1;
    /** How long changes are gathered before Changed() is sent */
    static constexpr int changeDelay = 250;

    explicit MailStatusBridge(Handlers handlers, QObject *parent = nullptr);

    static QString objectPath();
    static QString interfaceName();
    /** Register the D-Bus types; needed before the object is exported */
    static void registerTypes();

    /** @short Tell the widgets that the accounts or the messages may have changed; bursts send one Changed() */
    void notifyChanged();

public slots:
    int Version();
    /** One of "nothing", "counts", "senders" or "subjects" */
    QString Sharing();
    /** Each account with "id", "name", "unread" and "state" */
    IPC::StatusEntries Accounts();
    /** @short The newest unread messages of the chosen accounts and folders, at most @arg limit

    Each has "id", "account", "accountName", "folder", "sender", "sent" and, when shared, "subject". Empty filters
    choose everything.
    */
    IPC::StatusEntries UnreadMessages(const QStringList &accounts, const QStringList &folders, int limit);
    /** Open the message named by an "id" of UnreadMessages() */
    void OpenMessage(const QString &messageId, const QString &activationToken);
    void Compose(const QString &activationToken);
    void ShowAccount(const QString &accountId, const QString &activationToken);

signals:
    void Changed();

private:
    void handOver(const QString &token);

    Handlers m_handlers;
    QTimer *m_changeTimer;
};

} // namespace IPC

#endif
