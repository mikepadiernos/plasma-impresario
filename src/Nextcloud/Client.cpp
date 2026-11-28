/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Nextcloud/Client.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QUrlQuery>
#include <QXmlStreamReader>

#include <memory>

namespace Nextcloud {

namespace {

constexpr int maxRedirects = 5;

int defaultPort(const QUrl &url)
{
    return url.port(url.scheme() == QLatin1String("https") ? 443 : 80);
}

bool sameOrigin(const QUrl &a, const QUrl &b)
{
    return a.scheme() == b.scheme() && a.host().compare(b.host(), Qt::CaseInsensitive) == 0
        && defaultPort(a) == defaultPort(b);
}

QUrl serverUrl(const QUrl &server, const QString &path)
{
    QUrl url = server;
    QString base = server.path(QUrl::FullyDecoded);
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    url.setPath(base + path, QUrl::DecodedMode);
    url.setQuery(QString());
    url.setFragment(QString());
    url.setUserInfo(QString());
    return url;
}

QJsonObject ocsData(const QByteArray &body, int *status)
{
    const QJsonObject ocs = QJsonDocument::fromJson(body).object().value(QLatin1String("ocs")).toObject();
    *status = ocs.value(QLatin1String("meta")).toObject().value(QLatin1String("statuscode")).toInt(-1);
    return ocs.value(QLatin1String("data")).toObject();
}

} // namespace

Client::Client(QObject *parent)
    : QObject(parent)
{
    m_network.setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
}

Client::~Client()
{
    abortAll();
}

void Client::setAccount(const Account &account)
{
    m_account = account;
}

const Account &Client::account() const
{
    return m_account;
}

void Client::setAppPassword(const QString &password)
{
    m_appPassword = password;
}

void Client::setTrustCheck(TrustCheck check)
{
    m_trustCheck = std::move(check);
}

void Client::setAcceptedErrors(const QList<QSslError> &errors)
{
    m_acceptedErrors = errors;
}

void Client::setTimeout(int seconds)
{
    m_timeoutSeconds = seconds;
}

int Client::runningCount() const
{
    return m_replies.size();
}

QString Client::failureName(Failure failure)
{
    switch (failure) {
    case Failure::Authentication:
        return tr("The Nextcloud server refused the sign-in.");
    case Failure::Network:
        return tr("The Nextcloud server could not be reached.");
    case Failure::Certificate:
        return tr("The certificate of the Nextcloud server is not trusted.");
    case Failure::Security:
        return tr("The Nextcloud server's address is not protected by TLS.");
    case Failure::Protocol:
        return tr("The Nextcloud server answered something unexpected.");
    case Failure::Permission:
        return tr("You do not have permission to do this.");
    case Failure::NotFound:
        return tr("The file or folder no longer exists.");
    case Failure::Exists:
        return tr("A file with this name already exists.");
    case Failure::Quota:
        return tr("Your Nextcloud storage is full.");
    case Failure::Expired:
        return tr("The sign-in was not finished in time.");
    }
    return {};
}

Client::Failure Client::statusFailure(int status)
{
    switch (status) {
    case 401:
        return Failure::Authentication;
    case 403:
    case 423:
        return Failure::Permission;
    case 404:
    case 409:
    case 410:
        return Failure::NotFound;
    case 412:
        return Failure::Exists;
    case 507:
        return Failure::Quota;
    default:
        return status >= 500 ? Failure::Network : Failure::Protocol;
    }
}

QString Client::statusMessage(int status, const QByteArray &body)
{
    // Sabre reports WebDAV errors as <d:error><s:message>, OCS as
    // ocs.meta.message
    QString detail;
    QXmlStreamReader xml(body);
    while (!xml.atEnd() && detail.isEmpty()) {
        if (xml.readNext() == QXmlStreamReader::StartElement && xml.name() == QLatin1String("message"))
            detail = xml.readElementText().trimmed();
    }
    if (detail.isEmpty()) {
        detail = QJsonDocument::fromJson(body)
                     .object()
                     .value(QLatin1String("ocs"))
                     .toObject()
                     .value(QLatin1String("meta"))
                     .toObject()
                     .value(QLatin1String("message"))
                     .toString()
                     .trimmed();
    }
    const QString base = failureName(statusFailure(status));
    if (status == 423)
        return tr("The file is locked by another user or application.");
    return detail.isEmpty() ? tr("%1 (HTTP status %2)").arg(base).arg(status) : tr("%1 %2").arg(base, detail);
}

QByteArray Client::authorization() const
{
    return "Basic " + (m_account.loginName + QLatin1Char(':') + m_appPassword).toUtf8().toBase64();
}

Client::RequestId Client::startLogin(std::function<void(const LoginFlow &)> done, Failed failed)
{
    Request request;
    request.url = serverUrl(m_account.server, QStringLiteral("/index.php/login/v2"));
    request.verb = "POST";
    request.authorize = false;
    request.failed = failed;
    const QUrl server = m_account.server;
    request.done = [this, server, done = std::move(done), failed](int status, const QByteArray &body) {
        const QJsonObject answer = QJsonDocument::fromJson(body).object();
        const QJsonObject poll = answer.value(QLatin1String("poll")).toObject();
        LoginFlow flow;
        flow.login = QUrl(answer.value(QLatin1String("login")).toString());
        flow.endpoint = QUrl(poll.value(QLatin1String("endpoint")).toString());
        flow.token = poll.value(QLatin1String("token")).toString();
        if (status == 404) {
            failed(Failure::Protocol, tr("The server at %1 does not look like Nextcloud.").arg(server.host()));
            return;
        }
        if (status != 200 || flow.token.isEmpty() || !flow.login.isValid() || !flow.endpoint.isValid()) {
            failed(Failure::Protocol, tr("The Nextcloud server did not start the sign-in."));
            return;
        }
        if (!isAcceptable(flow.login) || !isAcceptable(flow.endpoint)) {
            failed(Failure::Security,
                tr("The Nextcloud server asked to sign in over "
                   "an address without TLS."));
            return;
        }
        done(flow);
    };
    return send(std::move(request));
}

Client::RequestId Client::pollLogin(
    const LoginFlow &flow, std::function<void(bool, const Credentials &)> done, Failed failed)
{
    Request request;
    request.url = flow.endpoint;
    request.verb = "POST";
    request.authorize = false;
    request.body = "token=" + QUrl::toPercentEncoding(flow.token);
    request.headers.emplaceBack("Content-Type", "application/x-www-form-urlencoded");
    request.failed = failed;
    request.done = [done = std::move(done), failed](int status, const QByteArray &body) {
        if (status == 404) {
            // Pending until the user grants access in the browser; the token expires
            // after 20 minutes
            done(false, {});
            return;
        }
        const QJsonObject answer = QJsonDocument::fromJson(body).object();
        Credentials credentials;
        credentials.server = QUrl(answer.value(QLatin1String("server")).toString());
        credentials.loginName = answer.value(QLatin1String("loginName")).toString();
        credentials.appPassword = answer.value(QLatin1String("appPassword")).toString();
        if (status != 200 || credentials.loginName.isEmpty() || credentials.appPassword.isEmpty()) {
            failed(Failure::Protocol,
                tr("The Nextcloud server's answer to the "
                   "sign-in could not be read."));
            return;
        }
        done(true, credentials);
    };
    return send(std::move(request));
}

Client::RequestId Client::fetchUser(std::function<void(const User &)> done, Failed failed)
{
    Request request;
    request.url = serverUrl(m_account.server, QStringLiteral("/ocs/v1.php/cloud/user"));
    request.url.setQuery(QStringLiteral("format=json"));
    request.verb = "GET";
    request.headers.emplaceBack("OCS-APIRequest", "true");
    request.failed = failed;
    request.done = [done = std::move(done), failed](int status, const QByteArray &body) {
        if (status != 200) {
            failed(statusFailure(status), statusMessage(status, body));
            return;
        }
        int ocsStatus = -1;
        const QJsonObject data = ocsData(body, &ocsStatus);
        User user;
        user.id = data.value(QLatin1String("id")).toString();
        user.displayName = data.value(QLatin1String("display-name")).toString();
        if (user.displayName.isEmpty())
            user.displayName = data.value(QLatin1String("displayname")).toString();
        user.email = data.value(QLatin1String("email")).toString();
        const QJsonValue free = data.value(QLatin1String("quota")).toObject().value(QLatin1String("free"));
        user.quotaFree = free.isDouble() ? free.toInteger() : -1;
        if ((ocsStatus != 100 && ocsStatus != 200) || user.id.isEmpty()) {
            failed(ocsStatus == 997 ? Failure::Authentication : Failure::Protocol,
                tr("The Nextcloud server did not say who the app password belongs "
                   "to."));
            return;
        }
        done(user);
    };
    return send(std::move(request));
}

Client::RequestId Client::list(const QString &path, std::function<void(const Listing &)> done, Failed failed)
{
    Request request;
    request.url = m_account.fileUrl(path);
    request.verb = "PROPFIND";
    request.body = propfindBody();
    request.headers.emplaceBack("Depth", "1");
    request.headers.emplaceBack("Content-Type", "application/xml; charset=utf-8");
    request.failed = failed;
    const QString davPath = m_account.davPath();
    request.done = [davPath, path, done = std::move(done), failed](int status, const QByteArray &body) {
        if (status != 207) {
            failed(statusFailure(status), statusMessage(status, body));
            return;
        }
        Listing listing;
        QString error;
        if (!parseListing(body, davPath, path, &listing, &error)) {
            failed(Failure::Protocol, error);
            return;
        }
        done(listing);
    };
    return send(std::move(request));
}

Client::RequestId Client::download(
    const QString &path, std::function<void(const QByteArray &)> done, Failed failed, Progress progress)
{
    Request request;
    request.url = m_account.fileUrl(path);
    request.verb = "GET";
    request.failed = failed;
    request.progress = std::move(progress);
    request.done = [done = std::move(done), failed](int status, const QByteArray &body) {
        if (status != 200) {
            failed(statusFailure(status), statusMessage(status, body));
            return;
        }
        done(body);
    };
    return send(std::move(request));
}

Client::RequestId Client::upload(const QString &path, const QByteArray &content, bool replace,
    std::function<void()> done, Failed failed, Progress progress)
{
    Request request;
    request.url = m_account.fileUrl(path);
    request.verb = "PUT";
    request.body = content;
    request.headers.emplaceBack("Content-Type", "application/octet-stream");
    if (!replace)
        request.headers.emplaceBack("If-None-Match", "*");
    request.failed = failed;
    request.progress = std::move(progress);
    request.done = [done = std::move(done), failed](int status, const QByteArray &body) {
        if (status != 201 && status != 204) {
            failed(statusFailure(status), statusMessage(status, body));
            return;
        }
        done();
    };
    return send(std::move(request));
}

Client::RequestId Client::shares(const QString &path, std::function<void(const QList<Share> &)> done, Failed failed)
{
    Request request;
    request.url = serverUrl(m_account.server, QStringLiteral("/ocs/v2.php/apps/files_sharing/api/v1/shares"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    if (!path.isEmpty()) {
        query.addQueryItem(QStringLiteral("path"), QString::fromLatin1(QUrl::toPercentEncoding(cleanPath(path), "/")));
        query.addQueryItem(QStringLiteral("reshares"), QStringLiteral("false"));
    }
    request.url.setQuery(query.toString(QUrl::FullyEncoded), QUrl::StrictMode);
    request.verb = "GET";
    request.headers.emplaceBack("OCS-APIRequest", "true");
    request.failed = failed;
    request.done = [done = std::move(done), failed](int status, const QByteArray &body) {
        if (status != 200) {
            failed(statusFailure(status), statusMessage(status, body));
            return;
        }
        QList<Share> shares;
        QString error;
        if (!parseShares(body, &shares, &error)) {
            failed(Failure::Protocol, error);
            return;
        }
        done(shares);
    };
    return send(std::move(request));
}

Client::RequestId Client::revokeAppPassword(std::function<void()> done, Failed failed)
{
    Request request;
    request.url = serverUrl(m_account.server, QStringLiteral("/ocs/v2.php/core/apppassword"));
    request.verb = "DELETE";
    request.headers.emplaceBack("OCS-APIRequest", "true");
    request.failed = failed;
    request.done = [done = std::move(done), failed](int status, const QByteArray &body) {
        // A password which no longer signs in is as good as revoked
        if (status == 200 || status == 401) {
            done();
            return;
        }
        failed(statusFailure(status), statusMessage(status, body));
    };
    return send(std::move(request));
}

void Client::abort(RequestId id)
{
    if (QPointer<QNetworkReply> reply = m_replies.take(id)) {
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
}

void Client::abortAll()
{
    const QList<RequestId> ids = m_replies.keys();
    for (const RequestId id : ids)
        abort(id);
}

Client::RequestId Client::send(Request request)
{
    const RequestId id = ++m_nextId;
    if (!isAcceptable(request.url)) {
        // Report after returning, so that callers can rely on the id first
        m_replies.insert(id, nullptr);
        QMetaObject::invokeMethod(
            this,
            [this, id, failed = request.failed, url = request.url]() {
                if (m_replies.remove(id))
                    failed(Failure::Security,
                        tr("The address %1 is not protected by TLS, so nothing was "
                           "sent.")
                            .arg(url.toDisplayString(QUrl::RemoveUserInfo)));
            },
            Qt::QueuedConnection);
        return id;
    }
    start(id, std::move(request));
    return id;
}

void Client::start(RequestId id, Request request)
{
    QNetworkRequest networkRequest(request.url);
    networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    networkRequest.setTransferTimeout(m_timeoutSeconds * 1000);
    networkRequest.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Plasma Impresario"));
    for (const auto &header : std::as_const(request.headers))
        networkRequest.setRawHeader(header.first, header.second);
    if (request.authorize && !m_account.loginName.isEmpty())
        networkRequest.setRawHeader("Authorization", authorization());
    QNetworkReply *reply = m_network.sendCustomRequest(networkRequest, request.verb, request.body);
    m_replies.insert(id, reply);
    if (!m_acceptedErrors.isEmpty())
        reply->ignoreSslErrors(m_acceptedErrors);

    if (request.progress) {
        auto progress = request.progress;
        connect(reply, &QNetworkReply::downloadProgress, this, [progress](qint64 done, qint64 total) {
            if (done > 0)
                progress(done, total);
        });
        connect(reply, &QNetworkReply::uploadProgress, this, [progress](qint64 done, qint64 total) {
            if (done > 0)
                progress(done, total);
        });
    }

    auto rejected = std::make_shared<QPair<QList<QSslCertificate>, QList<QSslError>>>();
    connect(reply, &QNetworkReply::sslErrors, this, [this, reply, rejected](const QList<QSslError> &errors) {
        const QList<QSslCertificate> chain = reply->sslConfiguration().peerCertificateChain();
        bool accepted = !m_acceptedErrors.isEmpty();
        for (const QSslError &error : errors)
            accepted = accepted && m_acceptedErrors.contains(error);
        if (accepted || (!chain.isEmpty() && m_trustCheck && m_trustCheck(reply->url().host(), chain.first()))) {
            reply->ignoreSslErrors(errors);
            return;
        }
        *rejected = qMakePair(chain, errors);
    });
    connect(reply, &QNetworkReply::finished, this, [this, id, reply, rejected, request = std::move(request)]() mutable {
        if (m_replies.value(id) != reply)
            return;
        m_replies.remove(id);
        reply->deleteLater();
        if (!rejected->second.isEmpty()) {
            emit certificateRejected(rejected->first, rejected->second);
            request.failed(Failure::Certificate, tr("The certificate of %1 is not trusted.").arg(request.url.host()));
            return;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 300 && status < 400 && status != 304) {
            // The typed Location header drops relative references, which RFC 9110
            // allows
            const QUrl location = QUrl::fromEncoded(reply->rawHeader("Location").trimmed());
            const bool keepsBody = status == 307 || status == 308;
            if (location.isEmpty() || !location.isValid() || request.redirects >= maxRedirects
                || (request.verb != "GET" && !keepsBody)) {
                request.failed(Failure::Protocol,
                    tr("The Nextcloud server redirected the request too "
                       "often or to nowhere."));
                return;
            }
            const QUrl next = request.url.resolved(location);
            if (!isAcceptable(next)) {
                request.failed(Failure::Security,
                    tr("The Nextcloud server redirected to an address "
                       "without TLS."));
                return;
            }
            request.authorize = request.authorize && sameOrigin(next, request.url);
            request.url = next;
            ++request.redirects;
            start(id, std::move(request));
            return;
        }
        const QByteArray body = reply->readAll();
        if (status == 0) {
            request.failed(Failure::Network,
                reply->error() == QNetworkReply::OperationCanceledError
                    ? tr("The Nextcloud server did not answer in time.")
                    : reply->errorString());
            return;
        }
        request.done(status, body);
    });
}

} // namespace Nextcloud
