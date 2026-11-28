/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSslCertificate>
#include <QSslError>
#include <functional>

class QTimer;

namespace MSA {
class AbstractMSA;
}

namespace Composer {

/** @short Messages waiting to be sent, kept on disk until the server has them

Each message lives in a folder of its own: its state, the copy handed to the
submission server, the copy filed in Sent and the composer's draft. Messages are
sent one at a time, oldest first for each account, whenever the account's
transport says that it is online.

A message is recorded as Sending before the server gets it, and as Submitted as
soon as the server accepts it, before the copy is filed in Sent. A failure to
file therefore only retries filing; the message itself is never submitted again.
A message which was Sending when the application ended may or may not have
reached the server, so it waits as Interrupted until the user sends it again or
moves it to the drafts.

A server which refuses the message itself, such as an unknown recipient, gets
the same answer every time; the message goes back to the drafts folder together
with the server's reason. Other failures retry with a growing delay, and waking
an account, as when it comes online, retries its messages straight away.
*/
class Outbox : public QObject
{
    Q_OBJECT
public:
    enum class State {
        /** Waiting for its turn, the network or the next attempt */
        Queued,
        /** Handed to the server; only seen while the application runs */
        Sending,
        /** Accepted by the server; the copy in Sent is still to be filed */
        Submitted,
        /** The application ended while sending, so the message may have been sent */
        Interrupted,
        /** Refused by the server, and the draft could not be written */
        Refused,
    };
    Q_ENUM(State)

    struct Entry {
        QString id;
        QString accountId;
        State state = State::Queued;
        QString subject;
        QByteArray from;
        QList<QByteArray> recipients;
        QDateTime queued;
        /** The message's date, which the copy in Sent keeps */
        QDateTime timestamp;
        /** Where the copy is filed, or empty when it is not */
        QString sentFolder;
        /** Failed attempts since the last success */
        int attempts = 0;
        /** Not before this time, or invalid for as soon as possible */
        QDateTime nextAttempt;
        QString lastError;
        bool hasDraft = false;
    };

    struct Message {
        QString accountId;
        QString subject;
        QByteArray from;
        QList<QByteArray> recipients;
        /** As delivered, without Bcc */
        QByteArray data;
        /** As filed in Sent; empty when it is the same as data */
        QByteArray sentCopy;
        /** Empty when no copy is filed */
        QString sentFolder;
        QDateTime timestamp;
        /** The composer's draft, restored when the server refuses the message */
        QByteArray draft;
    };

    using PasswordReply = std::function<void(const QString &password)>;
    using FilingReply = std::function<void(bool ok, const QString &error)>;

    /** @short How the messages of one account reach the server */
    struct Transport {
        std::function<MSA::AbstractMSA *(QObject *parent)> createMsa;
        /** Whether to try now; no function means always */
        std::function<bool()> online;
        std::function<void(
            const QString &folder, const QByteArray &data, const QDateTime &timestamp, const FilingReply &reply)>
            fileSentCopy;
        /** Reply with a null string when no password is available without asking the user */
        std::function<void(const QString &user, const QString &host, const PasswordReply &reply)> password;
        /** Whether an untrusted certificate is accepted; nothing is accepted without a function */
        std::function<bool(const QList<QSslCertificate> &chain, const QList<QSslError> &errors)> certificate;
    };

    /** The suffix of the file beside a refused message's draft which holds the server's reason */
    static constexpr const char *reasonSuffix = ".reason";

    Outbox(const QString &folder, const QString &draftsFolder, QObject *parent = nullptr);
    ~Outbox() override;

    QString folder() const;
    QString draftsFolder() const;

    void setTransport(const QString &accountId, const Transport &transport);
    void removeTransport(const QString &accountId);

    /** @short Keep @p message until it is sent; returns its ID, or an empty string and @p error */
    QString enqueue(const Message &message, QString *error = nullptr);

    int count() const;
    int count(const QString &accountId) const;
    QList<Entry> entries() const;
    /** The entry @p id, or one with an empty ID */
    Entry entry(const QString &id) const;
    /** Whether @p id is being sent or filed right now */
    bool isBusy(const QString &id) const;
    /** @short Whether a message of @p accountId is being sent or filed, or is due and its transport is online

    Background work of the account waits meanwhile, so that it does not slow the message down.
    */
    bool hasWorkDue(const QString &accountId) const;

    /** @short Try the messages of @p accountId, or of all accounts, now instead of after their delay */
    void wake(const QString &accountId = QString());
    /** @short Try a queued or submitted message now */
    bool sendNow(const QString &id);
    /** @short Queue an interrupted or refused message again, accepting that it may arrive twice */
    bool sendAgain(const QString &id);
    /** @short Take the message out of the outbox into the drafts folder; returns the file, or an empty string */
    QString moveToDrafts(const QString &id, QString *error = nullptr);
    /** @short Forget a message which the server already has, giving up filing its copy */
    bool forget(const QString &id);

    /** @short The delay in milliseconds after @p attempts failed attempts */
    void setBackoff(const std::function<qint64(int attempts)> &backoff);
    /** @short How long one attempt may take before it is given up */
    void setAttemptTimeout(int milliseconds);
    static qint64 defaultBackoff(int attempts);

signals:
    void changed();
    void sending(const QString &id);
    void sent(const QString &id);
    /** @short An attempt failed and another one follows; @p filing when the message was sent but not filed */
    void failed(const QString &id, const QString &error, bool filing);
    /** @short The server refused the message; it is now the draft @p draftPath, or still here as Refused */
    void refused(const Entry &entry, const QString &reason, const QString &draftPath);
    /** @short An attempt gave no answer, so the message may have been sent; it waits for the user */
    void interrupted(const QString &id);

private:
    void load();
    bool save(const Entry &entry, QString *error = nullptr);
    QString entryFolder(const QString &id) const;
    int indexOf(const QString &id) const;
    void schedulePump();
    void pump();
    void startSending(Entry &entry);
    void startFiling(Entry &entry);
    void releaseMsa();
    void onSent(quint64 serial);
    void onError(quint64 serial, const QString &message, bool permanent);
    void onFiled(quint64 serial, bool ok, const QString &error);
    void onTimeout();
    void retryLater(Entry &entry, const QString &error, bool filing);
    void finish();
    /** @short Takes @p id by value, as it may be m_current, which finish() clears */
    void complete(QString id);
    void refuse(Entry &entry, const QString &reason);
    QString writeDraft(const Entry &entry, const QString &reason, QString *error) const;
    void removeEntry(const QString &id);
    void failCurrent(const QString &error);

    QString m_folder;
    QString m_draftsFolder;
    QList<Entry> m_entries;
    QHash<QString, Transport> m_transports;
    std::function<qint64(int)> m_backoff;
    int m_attemptTimeout;
    QTimer *m_retryTimer;
    QTimer *m_watchdog;
    bool m_pumpScheduled = false;

    QString m_current;
    bool m_currentFiling = false;
    quint64 m_serial = 0;
    QPointer<MSA::AbstractMSA> m_msa;
};

} // namespace Composer
