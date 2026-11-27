/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef POP3_FETCHJOB_H
#define POP3_FETCHJOB_H

#include <functional>
#include <memory>

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSslCertificate>
#include <QSslError>
#include <QStringList>

class QSettings;
class QSslSocket;
class QTimer;

namespace LocalMail {
class Store;
}

namespace Pop3 {

/** @short How a POP3 connection is protected */
enum class Security {
    /** TLS from the start, usually on port 995 */
    Tls,
    /** Plain text upgraded through STLS, usually on port 110; nothing is sent when the server does not offer it */
    StartTls,
    /** No protection; the password crosses the network readable */
    None,
};

/** @short When downloaded messages are removed from the server */
enum class Retention {
    Keep,
    AfterDownload,
    AfterDays,
};

/** @short The server and sign-in of a POP3 account */
struct Settings {
    QString host;
    quint16 port = 0;
    Security security = Security::Tls;
    QString user;
    QString password;
    Retention retention = Retention::Keep;
    int retentionDays = 14;
    /** Minutes between automatic checks; 0 turns them off */
    int interval = 10;

    /** The port to use: the configured one or the default for @ref security */
    quint16 effectivePort() const;

    /** The settings stored for an account, without the password */
    static Settings fromSettings(const QSettings &account);
    /** Store everything except the password */
    void save(QSettings &account) const;

    static QString securityName(Security security);
    static Security securityFromName(const QString &name);
    static QString retentionName(Retention retention);
    static Retention retentionFromName(const QString &name);
};

/**
 * @short One visit to a POP3 server: sign in, download new messages into a local store, remove old ones, sign out
 *
 * Messages are told apart by their UIDL, or by a digest of their headers on servers without UIDL. Each message is
 * written to the store before the server is asked to delete it, and the server deletes nothing unless the session
 * ends with a successful QUIT, so a dropped connection loses no mail.
 */
class FetchJob : public QObject
{
    Q_OBJECT
public:
    enum class Failure {
        /** The server refused the user name or password */
        Authentication,
        /** The connection could not be made, broke, or timed out */
        Network,
        /** The server's certificate is not valid and was not accepted before; see certificateRejected() */
        Certificate,
        /** The protection asked for is not available, so the password was not sent */
        Security,
        /** The server answered something unexpected or lacks a required command */
        Protocol,
        /** The local store could not keep a message */
        Storage,
    };
    Q_ENUM(Failure)

    /** Whether a certificate the system does not trust was accepted by the user for this host */
    using TrustCheck = std::function<bool(const QString &host, const QSslCertificate &certificate)>;

    FetchJob(const Settings &settings, std::shared_ptr<LocalMail::Store> store, QObject *parent = nullptr);
    ~FetchJob() override;

    void setTrustCheck(TrustCheck check);
    /** Seconds without an answer before the visit fails; 60 unless changed */
    void setTimeout(int seconds);

    void start();
    void abort();
    bool isRunning() const;

    int downloaded() const;
    int removedFromServer() const;

signals:
    /** The server's certificate failed validation; @arg chain can be shown to the user and trusted for a retry */
    void certificateRejected(const QList<QSslCertificate> &chain, const QList<QSslError> &errors);
    void failed(Pop3::FetchJob::Failure failure, const QString &message);
    /** The visit ended; @arg downloaded new messages were stored */
    void succeeded(int downloaded);

private:
    enum class Step {
        Idle,
        Greeting,
        Capabilities,
        StartTls,
        AuthPlain,
        User,
        Pass,
        Uidl,
        List,
        Top,
        Retrieve,
        Delete,
        Quit,
        Done,
    };

    void onEncrypted();
    void onReadyRead();
    void onSslErrors(const QList<QSslError> &errors);
    void onSocketError();
    void onTimeout();

    void send(const QByteArray &command, Step step, bool multiLine);
    void handleResponse(bool ok, const QByteArray &status, const QList<QByteArray> &lines);
    void requestCapabilities();
    void signIn();
    void listMessages();
    void nextHeaderDigest();
    void nextDownload();
    void nextDeletion();
    void quit();
    void fail(Failure failure, const QString &message);
    void finish();
    static QString serverText(const QByteArray &status);

    Settings m_settings;
    std::shared_ptr<LocalMail::Store> m_store;
    TrustCheck m_trustCheck;
    QPointer<QSslSocket> m_socket;
    QTimer *m_timer;
    int m_timeoutSeconds = 60;

    Step m_step = Step::Idle;
    bool m_multiLine = false;
    bool m_statusRead = false;
    bool m_statusOk = false;
    QByteArray m_status;
    QByteArray m_buffer;
    QList<QByteArray> m_lines;

    bool m_upgraded = false;
    QStringList m_capabilities;
    QList<QSslError> m_sslErrors;
    bool m_certificateRejected = false;

    /** Message numbers and keys of the mail on the server */
    QList<QPair<int, QString>> m_serverMessages;
    int m_cursor = 0;
    QList<int> m_toDownload;
    QList<int> m_toDelete;
    QStringList m_deletedKeys;
    int m_downloaded = 0;
    int m_removed = 0;
};

}

#endif
