/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef LOCALMAIL_IMAPSESSION_H
#define LOCALMAIL_IMAPSESSION_H

#include <memory>

#include <QHash>
#include <QObject>
#include <QStringList>

#include "LocalMail/Mime.h"
#include "LocalMail/Store.h"

namespace LocalMail {

/**
 * @short An IMAP server for one connection to mail kept in a Store
 *
 * The mail views read every account through IMAP; accounts whose mail is kept on this computer, such as POP3
 * accounts, are served by this class in the same process. The session greets with PREAUTH, so nothing signs in, and
 * offers IMAP4rev1 with LITERAL+, UIDPLUS, UNSELECT, IDLE, MOVE and CHILDREN.
 */
class ImapSession : public QObject
{
    Q_OBJECT
public:
    explicit ImapSession(std::shared_ptr<Store> store, QObject *parent = nullptr);
    ~ImapSession() override;

    static QByteArray capabilities();

    /** Send the greeting */
    void start();
    /** Handle bytes sent by the client */
    void receive(const QByteArray &data);

signals:
    /** Bytes for the client */
    void output(const QByteArray &data);
    /** The client logged out */
    void finished();

public:
    struct Token {
        enum class Kind { Atom, String, List };
        Kind kind = Kind::Atom;
        QByteArray text;
        QList<Token> items;
    };

    struct CachedMessage {
        QByteArray raw;
        Mime::Part part;
    };

private:
    void processInput();
    void handle(const QByteArray &command);
    void send(const QByteArray &line);
    void flush();
    void storeChanged(const QString &mailbox);

    void list(const QByteArray &tag, const QList<Token> &args, bool subscribedOnly);
    void status(const QByteArray &tag, const QList<Token> &args);
    void select(const QByteArray &tag, const QList<Token> &args, bool readOnly);
    void append(const QByteArray &tag, const QList<Token> &args);
    void fetch(const QByteArray &tag, const QList<Token> &args, bool uid);
    void storeFlags(const QByteArray &tag, const QList<Token> &args, bool uid);
    void copy(const QByteArray &tag, const QList<Token> &args, bool uid, bool move);
    void search(const QByteArray &tag, const QList<Token> &args, bool uid);
    void expunge(const QByteArray &tag, const QList<Token> &args, bool uid);

    /** Report changes made by others to the selected folder; removals only when @arg expunges are allowed */
    void synchronize(bool expunges);
    /** Remove the messages at @arg indexes from the selected folder and report each, unless @arg silently */
    void removeIndexes(QList<int> indexes, bool silently);
    void deselect();
    /** Indexes into the selected messages named by @arg set, or false when it cannot be read */
    bool resolve(const QByteArray &set, bool uid, QList<int> *indexes) const;
    /** The selected folder's messages present in the store, by UID */
    QHash<uint, MessageInfo> currentMessages() const;
    CachedMessage parsed(uint uid);
    QByteArray flagList(const QStringList &flags) const;

    std::shared_ptr<Store> m_store;
    QByteArray m_input;
    int m_scanPosition = 0;
    bool m_continuationSent = false;
    QByteArray m_output;
    bool m_idling = false;
    QByteArray m_idleTag;
    bool m_finished = false;

    QString m_selected;
    bool m_readOnly = false;
    QList<uint> m_uids;
    QHash<uint, QStringList> m_flags;

    QString m_cacheMailbox;
    uint m_cacheValidity = 0;
    QHash<uint, CachedMessage> m_cache;
    QList<uint> m_cacheOrder;
};

}

#endif
