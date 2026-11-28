/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <functional>

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QSslCertificate>
#include <QSslError>
#include <QUrl>

#include "Nextcloud/Account.h"
#include "Nextcloud/Files.h"
#include "Nextcloud/Shares.h"

class QNetworkReply;

namespace Nextcloud {

/**
 * @short HTTP access to one Nextcloud account: sign-in, files over WebDAV and
 * the user's shares
 *
 * Requests go only over TLS, except to this computer. Redirects are followed
 * for at most five steps, and the app password goes along only while the
 * scheme, host and port stay the same. A certificate the system does not trust
 * is accepted when the trust check knows it; otherwise certificateRejected()
 * reports it before the request fails.
 *
 * Several requests may run at once. Each reports either its continuation or its
 * failure callback, never both, and neither after it was aborted or the client
 * was destroyed.
 */
class Client : public QObject
{
    Q_OBJECT
public:
    enum class Failure {
        /** The server refused the login name or app password, or the app password
           was revoked */
        Authentication,
        /** The server could not be reached, the connection broke, or it took too
           long */
        Network,
        /** The server's certificate is not valid and was not accepted before; see
           certificateRejected() */
        Certificate,
        /** The address is not protected by TLS, so nothing was sent */
        Security,
        /** The server answered something Nextcloud does not */
        Protocol,
        /** The user may not do this with the file or folder */
        Permission,
        NotFound,
        /** A file with the name exists, and replacing it was not asked for */
        Exists,
        /** The user's storage is full */
        Quota,
        /** The sign-in in the browser was not finished in time */
        Expired,
    };
    Q_ENUM(Failure)

    using Failed = std::function<void(Failure failure, const QString &message)>;
    using Progress = std::function<void(qint64 done, qint64 total)>;
    using TrustCheck = std::function<bool(const QString &host, const QSslCertificate &certificate)>;
    using RequestId = quint64;

    /** @short A sign-in started with Login Flow v2: the page to open and where to
     * ask for its result */
    struct LoginFlow {
        QUrl login;
        QUrl endpoint;
        QString token;
    };
    /** @short What a finished Login Flow v2 sign-in returns */
    struct Credentials {
        QUrl server;
        QString loginName;
        QString appPassword;
    };
    /** @short Who the app password belongs to */
    struct User {
        QString id;
        QString displayName;
        QString email;
        qint64 quotaFree = -1;
    };

    explicit Client(QObject *parent = nullptr);
    ~Client() override;

    /** @short The account to use; only its server is needed to sign in */
    void setAccount(const Account &account);
    const Account &account() const;
    void setAppPassword(const QString &password);
    void setTrustCheck(TrustCheck check);
    /** Errors to accept without asking, such as those of a certificate the user
     * accepted for this attempt */
    void setAcceptedErrors(const QList<QSslError> &errors);
    /** Seconds without progress before a request fails; 60 unless changed */
    void setTimeout(int seconds);

    /** @short Start a Login Flow v2 sign-in at the account's server */
    RequestId startLogin(std::function<void(const LoginFlow &)> done, Failed failed);
    /** @short Ask whether the sign-in in the browser is finished; @arg done gets
     * false while it is not */
    RequestId pollLogin(
        const LoginFlow &flow, std::function<void(bool finished, const Credentials &)> done, Failed failed);

    /** @short Who the login name and app password belong to */
    RequestId fetchUser(std::function<void(const User &)> done, Failed failed);
    /** @short The folder @arg path and what it contains */
    RequestId list(const QString &path, std::function<void(const Listing &)> done, Failed failed);
    RequestId download(
        const QString &path, std::function<void(const QByteArray &)> done, Failed failed, Progress progress = {});
    /**
     * @short Store @arg content as the file @arg path
     *
     * Unless @arg replace is set, an existing file is left alone and the upload
     * fails with Failure::Exists.
     */
    RequestId upload(const QString &path, const QByteArray &content, bool replace, std::function<void()> done,
        Failed failed, Progress progress = {});
    /** @short The shares of @arg path the user made, or all of the user's shares
     * for an empty path */
    RequestId shares(const QString &path, std::function<void(const QList<Share> &)> done, Failed failed);

    /** @short Revoke the app password in use, so that it no longer signs in; for disconnecting the account */
    RequestId revokeAppPassword(std::function<void()> done, Failed failed);

    /** @short Stop the request @arg id without reporting it */
    void abort(RequestId id);
    /** @short Stop every request without reporting them */
    void abortAll();
    int runningCount() const;

    /** The words for @arg failure, for messages which add details */
    static QString failureName(Failure failure);

signals:
    void certificateRejected(const QList<QSslCertificate> &chain, const QList<QSslError> &errors);

private:
    struct Request {
        QUrl url;
        QByteArray verb;
        QByteArray body;
        QList<QPair<QByteArray, QByteArray>> headers;
        bool authorize = true;
        int redirects = 0;
        Progress progress;
        /** Called for every answer which is not a redirect, a certificate problem
         * or a broken connection */
        std::function<void(int status, const QByteArray &body)> done;
        Failed failed;
    };

    RequestId send(Request request);
    void start(RequestId id, Request request);
    QByteArray authorization() const;
    /** The failure an HTTP @arg status means for a file request, with the
     * server's message if it gave one */
    static Failure statusFailure(int status);
    static QString statusMessage(int status, const QByteArray &body);

    Account m_account;
    QString m_appPassword;
    TrustCheck m_trustCheck;
    QList<QSslError> m_acceptedErrors;
    int m_timeoutSeconds = 60;
    QNetworkAccessManager m_network;
    QHash<RequestId, QPointer<QNetworkReply>> m_replies;
    RequestId m_nextId = 0;
};

} // namespace Nextcloud
