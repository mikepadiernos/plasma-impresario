/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QObject>
#include <QPointer>
#include <QSet>

#include "UiUtils/MailRules.h"

class QModelIndex;
class QTimer;

namespace Imap {
namespace Mailbox {
class Model;
}
}

namespace UiUtils {

/** @short Notice the messages which arrive in one folder of an account

The watcher looks at the folder's messages as the account's model knows them. A message counts as arrived when its UID
is above the highest UID seen before; it is reported once its envelope is loaded, which the watcher asks for. The
first look at a folder only records what is there, so that the mail present before is not reported as new; the same
happens when the server renumbers the folder (a new UIDVALIDITY).

An account has one connection, which shows one folder at a time. A watcher which opens its folder makes the connection
select it, which only suits an account that is not shown; for the shown account, the watcher only reports what arrives
while its folder is open anyway.
*/
class NewMailWatcher : public QObject
{
    Q_OBJECT
public:
    /** @short What was seen in the folder, kept between runs so that mail which came meanwhile is reported */
    struct Baseline {
        uint uidValidity = 0;
        uint highestUid = 0;
        /** When the folder's messages were last looked at */
        QDateTime lastCheck;
        bool isValid() const { return uidValidity != 0; }
        bool operator==(const Baseline &other) const = default;
    };

    NewMailWatcher(Imap::Mailbox::Model *model, const QString &accountId, const QString &mailbox = QStringLiteral("INBOX"),
                   QObject *parent = nullptr);
    ~NewMailWatcher() override;

    QString accountId() const;
    QString mailbox() const;

    /** Whether the watcher opens its folder to check it, rather than waiting until it is opened; off by default */
    void setOpensMailbox(bool opens);
    bool opensMailbox() const;

    void setBaseline(const Baseline &baseline);
    Baseline baseline() const;

    /** Look at the folder's messages now instead of after the next change */
    void check();

    /** @short The message at @arg index as the rules see it */
    static MailRules::Message message(const QModelIndex &index, const QString &accountId, const QString &mailbox);

signals:
    /** @short Messages which arrived since the folder was last checked at @arg lastCheck */
    void arrived(const QList<UiUtils::MailRules::Message> &messages, const QDateTime &lastCheck);
    /** The baseline changed and may be saved */
    void baselineChanged();

private:
    void schedule();
    QModelIndex messageList() const;

    QPointer<Imap::Mailbox::Model> m_model;
    QString m_accountId;
    QString m_mailbox;
    bool m_opensMailbox = false;
    Baseline m_baseline;
    /** Arrived messages whose envelope is not loaded yet */
    QSet<uint> m_pending;
    /** When the messages were last looked at before the arrivals in m_pending */
    QDateTime m_pendingSince;
    QTimer *m_timer;
};

}

Q_DECLARE_METATYPE(QList<UiUtils::MailRules::Message>)
