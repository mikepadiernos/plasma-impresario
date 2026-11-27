/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef LOCALMAIL_STORE_H
#define LOCALMAIL_STORE_H

#include <memory>

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QStringList>

namespace LocalMail {

/** @short A message kept on this computer */
struct MessageInfo {
    uint uid = 0;
    QStringList flags;
    /** When the message was stored, in UTC */
    QDateTime internalDate;
    qint64 size = 0;
    /** Where the message came from, such as a POP3 UIDL; empty for messages stored by the user */
    QString origin;
    QString file;
};

/** @short A folder kept on this computer */
struct MailboxInfo {
    QString name;
    uint uidValidity = 0;
    uint uidNext = 1;
    bool subscribed = true;
};

/**
 * @short Mail kept only on this computer, for accounts whose server has no folders, such as POP3
 *
 * Folders and flags are kept in an SQLite database and each message in its own user-only file inside @ref folder().
 * A message is on disk before append() returns its UID, so a POP3 server may delete it from then on.
 *
 * Every user of one folder in a process shares a single Store, which reports each change through changed().
 */
class Store : public QObject
{
    Q_OBJECT
public:
    /** The folders a new store starts with: INBOX, Sent, Drafts and Trash */
    static QStringList defaultMailboxes();

    /** The store in @arg folder, created when missing; nullptr and @arg error when it cannot be opened */
    static std::shared_ptr<Store> open(const QString &folder, QString *error = nullptr);
    ~Store() override;

    QString folder() const;

    /** Folder names, INBOX first and the others sorted */
    QStringList mailboxNames() const;
    bool hasMailbox(const QString &name) const;
    MailboxInfo mailbox(const QString &name) const;
    bool createMailbox(const QString &name, QString *error = nullptr);
    /** Remove a folder and its messages; INBOX cannot be removed */
    bool deleteMailbox(const QString &name, QString *error = nullptr);
    /** Rename a folder and the folders below it, which are separated by "/" */
    bool renameMailbox(const QString &from, const QString &to, QString *error = nullptr);
    bool setSubscribed(const QString &name, bool subscribed);

    /** The messages in folder @arg name, by ascending UID */
    QList<MessageInfo> messages(const QString &name) const;
    /** The message @arg uid in @arg mailbox as it was stored, or an empty array */
    QByteArray content(const QString &mailbox, uint uid) const;

    /**
     * Store @arg raw in @arg mailbox, with its line ends made CRLF, and return its UID, or 0 and @arg error
     *
     * A non-empty @arg origin is remembered with the time it was stored, also after the message is removed.
     */
    uint append(const QString &mailbox, const QByteArray &raw, const QStringList &flags = {},
                const QDateTime &internalDate = {}, const QString &origin = {}, QString *error = nullptr);
    bool setFlags(const QString &mailbox, uint uid, const QStringList &flags);
    /** Remove the messages @arg uids of @arg mailbox, with their files */
    bool remove(const QString &mailbox, const QList<uint> &uids, QString *error = nullptr);
    /**
     * Copy messages, returning the new UIDs in the order of @arg uids, or an empty list and @arg error
     *
     * Copies keep the origin, so a message moved between folders is still known as the server's message.
     */
    QList<uint> copy(const QString &from, const QList<uint> &uids, const QString &to, QString *error = nullptr);

    /** Whether a message from @arg origin was stored, even if it was removed since */
    bool knowsOrigin(const QString &origin) const;
    /** When the message from @arg origin was stored; invalid when it was not */
    QDateTime originStored(const QString &origin) const;
    QStringList origins() const;
    /** Forget the origins which are not in @arg kept, such as messages no longer on a server */
    void keepOnlyOrigins(const QStringList &kept);

signals:
    void changed(const QString &mailbox);

private:
    explicit Store(const QString &folder);
    bool initialize(QString *error);
    bool exec(const QString &statement, const QVariantList &values = {}, QString *error = nullptr);
    QString messageFilePath(const QString &file) const;
    QString writeMessage(const QByteArray &content, QString *error);

    QString m_folder;
    QString m_connection;
    QHash<QString, MailboxInfo> m_mailboxes;
    QHash<QString, QList<MessageInfo>> m_messages;
    QHash<QString, QDateTime> m_origins;
};

}

#endif
