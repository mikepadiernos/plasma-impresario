/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Jmap/Client.h"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>

namespace Jmap {

namespace Capability {
const QString core = QStringLiteral("urn:ietf:params:jmap:core");
const QString mail = QStringLiteral("urn:ietf:params:jmap:mail");
const QString submission = QStringLiteral("urn:ietf:params:jmap:submission");
}

namespace {

constexpr int maxRedirects = 5;

QString expand(QString pattern, const QList<QPair<QString, QString>> &values)
{
    for (const auto &value : values)
        pattern.replace(QLatin1Char('{') + value.first + QLatin1Char('}'),
                        QString::fromLatin1(QUrl::toPercentEncoding(value.second)));
    return pattern;
}

int positiveOr(const QJsonValue &value, int fallback)
{
    const int number = value.toInt(0);
    return number > 0 ? number : fallback;
}

}

QUrl Session::download(const QString &blobId, const QString &type, const QString &name) const
{
    return m_base.resolved(QUrl(expand(downloadUrl, {{QStringLiteral("accountId"), accountId},
                                                     {QStringLiteral("blobId"), blobId},
                                                     {QStringLiteral("type"), type},
                                                     {QStringLiteral("name"), name}})));
}

QUrl Session::upload() const
{
    return m_base.resolved(QUrl(expand(uploadUrl, {{QStringLiteral("accountId"), accountId}})));
}

bool Session::parse(const QJsonObject &json, const QUrl &base, Session *session, QString *error)
{
    const QJsonObject capabilities = json.value(QLatin1String("capabilities")).toObject();
    if (!capabilities.contains(Capability::core) || !json.value(QLatin1String("apiUrl")).isString()) {
        *error = QObject::tr("The server does not answer as a JMAP server.");
        return false;
    }
    if (!capabilities.contains(Capability::mail)) {
        *error = QObject::tr("The JMAP server does not offer mail.");
        return false;
    }
    const QString accountId = json.value(QLatin1String("primaryAccounts")).toObject().value(Capability::mail).toString();
    const QJsonObject account = json.value(QLatin1String("accounts")).toObject().value(accountId).toObject();
    if (accountId.isEmpty() || account.isEmpty()) {
        *error = QObject::tr("The JMAP server has no mail account for this user.");
        return false;
    }
    const QJsonObject core = capabilities.value(Capability::core).toObject();
    Session parsed;
    parsed.m_base = base;
    parsed.apiUrl = base.resolved(QUrl(json.value(QLatin1String("apiUrl")).toString()));
    parsed.downloadUrl = json.value(QLatin1String("downloadUrl")).toString();
    parsed.uploadUrl = json.value(QLatin1String("uploadUrl")).toString();
    parsed.accountId = accountId;
    parsed.state = json.value(QLatin1String("state")).toString();
    parsed.username = json.value(QLatin1String("username")).toString();
    parsed.maxObjectsInGet = qMin(positiveOr(core.value(QLatin1String("maxObjectsInGet")), 256), 500);
    parsed.maxObjectsInSet = qMin(positiveOr(core.value(QLatin1String("maxObjectsInSet")), 128), 500);
    parsed.maxCallsInRequest = positiveOr(core.value(QLatin1String("maxCallsInRequest")), 16);
    parsed.maxSizeUpload = core.value(QLatin1String("maxSizeUpload")).toInteger(0);
    parsed.canSubmit = capabilities.contains(Capability::submission)
        && account.value(QLatin1String("accountCapabilities")).toObject().contains(Capability::submission);
    parsed.readOnly = account.value(QLatin1String("isReadOnly")).toBool();
    if (!parsed.apiUrl.isValid() || parsed.downloadUrl.isEmpty() || parsed.uploadUrl.isEmpty()) {
        *error = QObject::tr("The JMAP server did not say where to send requests.");
        return false;
    }
    *session = parsed;
    return true;
}

Client::Client(const Settings &settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    m_network.setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
}

Client::~Client()
{
    abort();
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

const Session &Client::session() const
{
    return m_session;
}

QJsonArray Client::method(const QString &name, const QJsonObject &arguments, const QString &callId)
{
    return QJsonArray{name, arguments, callId};
}

QJsonObject Client::response(const QJsonArray &responses, const QString &callId, QString *errorType)
{
    for (const QJsonValue &value : responses) {
        const QJsonArray response = value.toArray();
        if (response.at(2).toString() != callId)
            continue;
        const QJsonObject arguments = response.at(1).toObject();
        *errorType = response.at(0).toString() == QLatin1String("error")
            ? arguments.value(QLatin1String("type")).toString(QStringLiteral("serverFail"))
            : QString();
        return arguments;
    }
    *errorType = QStringLiteral("serverFail");
    return {};
}

void Client::fetchSession(std::function<void()> done)
{
    const QUrl url = m_settings.sessionUrl();
    if (!url.isValid()) {
        fail(Failure::Protocol, tr("The JMAP server address is not valid."));
        return;
    }
    send(url, "GET", {}, {}, true, 0, [this, url, done = std::move(done)](const QByteArray &body) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        QString error;
        if (!document.isObject()) {
            fail(Failure::Unsupported, tr("The server does not answer as a JMAP server."));
            return;
        }
        if (!Session::parse(document.object(), url, &m_session, &error)) {
            fail(Failure::Unsupported, error);
            return;
        }
        done();
    });
}

void Client::call(const QJsonArray &methodCalls, std::function<void(const QJsonArray &)> done)
{
    QJsonArray using_{Capability::core, Capability::mail};
    for (const QJsonValue &call : methodCalls) {
        const QString name = call.toArray().at(0).toString();
        if ((name.startsWith(QLatin1String("EmailSubmission/")) || name.startsWith(QLatin1String("Identity/")))
            && !using_.contains(Capability::submission))
            using_.append(Capability::submission);
    }
    const QByteArray body = QJsonDocument(QJsonObject{{QStringLiteral("using"), using_},
                                                      {QStringLiteral("methodCalls"), methodCalls}})
                                .toJson(QJsonDocument::Compact);
    send(m_session.apiUrl, "POST", body, "application/json", true, 0, [this, done = std::move(done)](const QByteArray &answer) {
        const QJsonDocument document = QJsonDocument::fromJson(answer);
        if (!document.object().value(QLatin1String("methodResponses")).isArray()) {
            fail(Failure::Protocol, tr("The JMAP server's answer could not be read."));
            return;
        }
        done(document.object().value(QLatin1String("methodResponses")).toArray());
    });
}

void Client::download(const QString &blobId, std::function<void(const QByteArray &)> done)
{
    send(m_session.download(blobId, QStringLiteral("message/rfc822"), QStringLiteral("message.eml")), "GET", {}, {},
         true, 0, std::move(done));
}

void Client::upload(const QByteArray &content, const QByteArray &type, std::function<void(const QString &)> done)
{
    if (m_session.maxSizeUpload > 0 && content.size() > m_session.maxSizeUpload) {
        fail(Failure::Protocol, tr("The message is larger than the JMAP server accepts."));
        return;
    }
    send(m_session.upload(), "POST", content, type, true, 0, [this, done = std::move(done)](const QByteArray &answer) {
        const QString blobId = QJsonDocument::fromJson(answer).object().value(QLatin1String("blobId")).toString();
        if (blobId.isEmpty()) {
            fail(Failure::Protocol, tr("The JMAP server did not accept the message."));
            return;
        }
        done(blobId);
    });
}

void Client::abort()
{
    if (QNetworkReply *reply = m_reply) {
        m_reply = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
}

void Client::send(const QUrl &url, const QByteArray &verb, const QByteArray &body, const QByteArray &contentType,
                  bool authorize, int redirects, std::function<void(const QByteArray &)> done)
{
    if (!Settings::isAcceptable(url)) {
        fail(Failure::Security, tr("The server address %1 is not protected by TLS, so the sign-in was not sent.")
                                    .arg(url.toDisplayString(QUrl::RemoveUserInfo)));
        return;
    }
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(m_timeoutSeconds * 1000);
    request.setRawHeader("Accept", "application/json");
    if (authorize)
        request.setRawHeader("Authorization", m_settings.authorization());
    if (!contentType.isEmpty())
        request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    QNetworkReply *reply = m_network.sendCustomRequest(request, verb, body);
    m_reply = reply;
    if (!m_acceptedErrors.isEmpty())
        reply->ignoreSslErrors(m_acceptedErrors);

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
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, url, verb, body, contentType, authorize, redirects, rejected, done = std::move(done)]() {
        if (m_reply != reply)
            return;
        m_reply = nullptr;
        reply->deleteLater();
        if (!rejected->second.isEmpty()) {
            emit certificateRejected(rejected->first, rejected->second);
            fail(Failure::Certificate, tr("The certificate of %1 is not trusted.").arg(url.host()));
            return;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 300 && status < 400 && status != 304) {
            // The typed Location header drops relative references, which RFC 9110 allows
            const QUrl location = QUrl::fromEncoded(reply->rawHeader("Location").trimmed());
            const bool keepsBody = status == 307 || status == 308;
            if (location.isEmpty() || !location.isValid() || redirects >= maxRedirects || (verb != "GET" && !keepsBody)) {
                fail(Failure::Protocol, tr("The JMAP server redirected the request too often or to nowhere."));
                return;
            }
            const QUrl next = url.resolved(location);
            send(next, verb, body, contentType, authorize && next.host() == url.host() && next.port() == url.port(),
                 redirects + 1, done);
            return;
        }
        if (status == 401 || status == 403) {
            fail(Failure::Authentication, tr("The JMAP server refused the sign-in."));
            return;
        }
        const QByteArray answer = reply->readAll();
        if (status == 0 || (reply->error() != QNetworkReply::NoError && status < 400)) {
            fail(Failure::Network, reply->error() == QNetworkReply::OperationCanceledError
                     ? tr("The JMAP server did not answer in time.")
                     : reply->errorString());
            return;
        }
        if (status >= 400) {
            // RFC 8620 section 3.6.1: request-level errors are problem details (RFC 7807)
            const QJsonObject problem = QJsonDocument::fromJson(answer).object();
            const QString type = problem.value(QLatin1String("type")).toString();
            const QString detail = problem.value(QLatin1String("detail")).toString();
            const QString message = detail.isEmpty() ? tr("The JMAP server answered with status %1.").arg(status) : detail;
            fail(type.endsWith(QLatin1String(":unknownCapability")) ? Failure::Unsupported : Failure::Protocol, message);
            return;
        }
        done(answer);
    });
}

void Client::fail(Failure failure, const QString &message)
{
    emit failed(failure, message);
}

}
