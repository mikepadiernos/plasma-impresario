/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef JMAP_CLIENT_H
#define JMAP_CLIENT_H

#include <functional>

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QSslCertificate>
#include <QSslError>

#include "Jmap/Settings.h"

class QNetworkReply;

namespace Jmap {

/** @short The capability names of RFC 8620 and RFC 8621 */
namespace Capability {
extern const QString core;
extern const QString mail;
extern const QString submission;
}

/** @short What the server's session resource says about the account (RFC 8620, section 2) */
struct Session {
    QUrl apiUrl;
    /** URL templates with {accountId}, {blobId}, {type} and {name} */
    QString downloadUrl;
    QString uploadUrl;
    QString accountId;
    QString state;
    QString username;
    int maxObjectsInGet = 256;
    int maxObjectsInSet = 128;
    int maxCallsInRequest = 16;
    qint64 maxSizeUpload = 0;
    /** Whether the server and the account offer sending (urn:ietf:params:jmap:submission) */
    bool canSubmit = false;
    bool readOnly = false;

    /** Where blob @arg blobId can be downloaded */
    QUrl download(const QString &blobId, const QString &type, const QString &name) const;
    /** Where blobs of this account are uploaded */
    QUrl upload() const;

    /** The session in a server's answer, resolving relative URLs against @arg base; false and @arg error if unusable */
    static bool parse(const QJsonObject &json, const QUrl &base, Session *session, QString *error);

private:
    QUrl m_base;
};

/**
 * @short HTTP access to a JMAP server, one request at a time
 *
 * Each request is made only over TLS, except to this computer. Redirects are followed for at most five steps, and
 * the sign-in goes along only while the host stays the same. A certificate the system does not trust is accepted
 * when the trust check knows it; otherwise certificateRejected() reports it before the request fails.
 *
 * A request that fails emits failed() and does not call its continuation.
 */
class Client : public QObject
{
    Q_OBJECT
public:
    enum class Failure {
        /** The server refused the user name, password or token */
        Authentication,
        /** The server could not be reached, the connection broke, or it took too long */
        Network,
        /** The server's certificate is not valid and was not accepted before; see certificateRejected() */
        Certificate,
        /** The address is not protected by TLS, so the sign-in was not sent */
        Security,
        /** The server answered something that does not follow JMAP */
        Protocol,
        /** The server lacks something mail needs, such as the mail capability */
        Unsupported,
        /** The mail on this computer could not be changed */
        Storage,
    };
    Q_ENUM(Failure)

    using TrustCheck = std::function<bool(const QString &host, const QSslCertificate &certificate)>;

    explicit Client(const Settings &settings, QObject *parent = nullptr);
    ~Client() override;

    void setTrustCheck(TrustCheck check);
    /** Errors to accept without asking, such as those of a certificate the user accepted for this attempt */
    void setAcceptedErrors(const QList<QSslError> &errors);
    /** Seconds without progress before a request fails; 60 unless changed */
    void setTimeout(int seconds);

    const Session &session() const;

    /** Ask for the session resource, then call @arg done */
    void fetchSession(std::function<void()> done);
    /** Make one API request with @arg methodCalls, then call @arg done with the method responses */
    void call(const QJsonArray &methodCalls, std::function<void(const QJsonArray &responses)> done);
    void download(const QString &blobId, std::function<void(const QByteArray &content)> done);
    void upload(const QByteArray &content, const QByteArray &type, std::function<void(const QString &blobId)> done);
    /** Stop the request in progress without reporting it */
    void abort();

    /**
     * The arguments of the response to @arg callId
     *
     * @arg errorType is empty for a normal response, the error type for an "error" response, and "serverFail"
     * when the server did not answer the call.
     */
    static QJsonObject response(const QJsonArray &responses, const QString &callId, QString *errorType);
    /** One method call */
    static QJsonArray method(const QString &name, const QJsonObject &arguments, const QString &callId);

signals:
    void certificateRejected(const QList<QSslCertificate> &chain, const QList<QSslError> &errors);
    void failed(Jmap::Client::Failure failure, const QString &message);

private:
    void send(const QUrl &url, const QByteArray &verb, const QByteArray &body, const QByteArray &contentType,
              bool authorize, int redirects, std::function<void(const QByteArray &)> done);
    void fail(Failure failure, const QString &message);

    Settings m_settings;
    Session m_session;
    TrustCheck m_trustCheck;
    QList<QSslError> m_acceptedErrors;
    int m_timeoutSeconds = 60;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
};

}

#endif
