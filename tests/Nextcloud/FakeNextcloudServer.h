/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TESTS_NEXTCLOUD_FAKENEXTCLOUDSERVER_H
#define TESTS_NEXTCLOUD_FAKENEXTCLOUDSERVER_H

#include <memory>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QUrl>
#include <QUrlQuery>

/**
 * @short A Nextcloud server kept in memory: Login Flow v2, the OCS user and
 * share APIs, and WebDAV files
 *
 * Requests other than the sign-in must carry the Basic sign-in of @ref
 * loginName and @ref appPassword. Paths of files and folders start with a
 * slash; the root folder exists from the start.
 */
class FakeNextcloudServer : public QObject
{
public:
    struct Node {
        bool folder = false;
        QByteArray content;
        QString permissions;
        QList<int> shareTypes;
        qint64 fileId = 0;
    };
    struct Request {
        QByteArray verb;
        QString path;
        QMap<QByteArray, QByteArray> headers;
        QByteArray body;
    };

    QString loginName = QStringLiteral("alice@example.com");
    QString appPassword = QStringLiteral("app-password-1");
    QString userId = QStringLiteral("alice");
    QString displayName = QStringLiteral("Alice Example");
    /** The prefix of every path, as for a server installed in a subfolder */
    QString base;
    bool loginGranted = false;
    bool quotaFull = false;
    /** Answers to the paths, such as a redirect, before anything else is looked
     * at */
    QMap<QString, QPair<int, QByteArray>> overrides;

    QMap<QString, Node> nodes;
    QList<QJsonObject> shares;
    QList<Request> requests;
    int loginPolls = 0;

    FakeNextcloudServer()
    {
        nodes.insert(QStringLiteral("/"), Node{true, {}, QStringLiteral("RGDNVCK"), {}, 1});
        connect(&m_server, &QTcpServer::newConnection, this, &FakeNextcloudServer::accept);
        QVERIFY(m_server.listen(QHostAddress::LocalHost, 0));
    }

    QString address() const
    {
        return QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort()) + base;
    }

    void addFolder(const QString &path, const QString &permissions = QStringLiteral("RGDNVCK"))
    {
        nodes.insert(path, Node{true, {}, permissions, {}, ++m_nextId});
    }

    void addFile(const QString &path, const QByteArray &content, const QString &permissions = QStringLiteral("RGDNVW"))
    {
        nodes.insert(path, Node{false, content, permissions, {}, ++m_nextId});
    }

    /** A public link share of @arg path; returns its token */
    QString addLinkShare(
        const QString &path, int permissions = 1, bool password = false, const QString &expiration = QString())
    {
        const QString token = QStringLiteral("Tok%1abcdefgh").arg(++m_nextId);
        QJsonObject share{{QStringLiteral("id"), QString::number(m_nextId)}, {QStringLiteral("share_type"), 3},
            {QStringLiteral("path"), path},
            {QStringLiteral("item_type"), nodes.value(path).folder ? QStringLiteral("folder") : QStringLiteral("file")},
            {QStringLiteral("token"), token},
            {QStringLiteral("url"), QString(address() + QStringLiteral("/s/") + token)},
            {QStringLiteral("permissions"), permissions}, {QStringLiteral("uid_owner"), userId},
            {QStringLiteral("hide_download"), 0},
            {QStringLiteral("password"), password ? QJsonValue(QStringLiteral("$2y$10$hash")) : QJsonValue()},
            {QStringLiteral("expiration"),
                expiration.isEmpty() ? QJsonValue() : QJsonValue(QString(expiration + QStringLiteral(" 00:00:00")))}};
        shares.append(share);
        nodes[path].shareTypes.append(3);
        return token;
    }

    QStringList verbs() const
    {
        QStringList result;
        for (const Request &request : requests)
            result << QString::fromLatin1(request.verb) + QLatin1Char(' ') + request.path;
        return result;
    }

private:
    QByteArray basic() const
    {
        return "Basic " + (loginName + QLatin1Char(':') + appPassword).toUtf8().toBase64();
    }
    QString davRoot() const
    {
        return base + QStringLiteral("/remote.php/dav/files/") + userId;
    }

    void accept()
    {
        while (QTcpSocket *socket = m_server.nextPendingConnection()) {
            socket->setParent(this);
            auto buffer = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer] {
                *buffer += socket->readAll();
                const qsizetype headerEnd = buffer->indexOf("\r\n\r\n");
                if (headerEnd < 0)
                    return;
                Request request;
                qsizetype length = 0;
                const QList<QByteArray> lines = buffer->left(headerEnd).split('\n');
                for (int i = 1; i < lines.size(); ++i) {
                    const qsizetype colon = lines[i].indexOf(':');
                    if (colon <= 0)
                        continue;
                    const QByteArray name = lines[i].left(colon).trimmed().toLower();
                    request.headers.insert(name, lines[i].mid(colon + 1).trimmed());
                }
                length = request.headers.value("content-length").toLongLong();
                if (buffer->size() < headerEnd + 4 + length)
                    return;
                const QList<QByteArray> first = lines.value(0).trimmed().split(' ');
                request.verb = first.value(0);
                request.path = QString::fromLatin1(first.value(1));
                request.body = buffer->mid(headerEnd + 4, length);
                buffer->clear();
                requests.append(request);
                int status = 200;
                QByteArray headers;
                QByteArray type = "application/json";
                const QByteArray answer = handle(request, &status, &headers, &type);
                socket->write("HTTP/1.1 " + QByteArray::number(status) + " Fake\r\n" + headers + "Content-Type: " + type
                    + "\r\nContent-Length: " + QByteArray::number(answer.size()) + "\r\nConnection: close\r\n\r\n"
                    + answer);
                socket->disconnectFromHost();
            });
        }
    }

    static QByteArray ocs(int status, const QJsonValue &data, const QString &message = QString())
    {
        return QJsonDocument(
            QJsonObject{{QStringLiteral("ocs"),
                QJsonObject{{QStringLiteral("meta"),
                                QJsonObject{{QStringLiteral("status"),
                                                status == 200 || status == 100 ? QStringLiteral("ok")
                                                                               : QStringLiteral("failure")},
                                    {QStringLiteral("statuscode"), status}, {QStringLiteral("message"), message}}},
                    {QStringLiteral("data"), data}}}})
            .toJson(QJsonDocument::Compact);
    }

    static QByteArray davError(const QString &message)
    {
        return QByteArrayLiteral("<?xml version=\"1.0\"?><d:error xmlns:d=\"DAV:\" "
                                 "xmlns:s=\"http://sabredav.org/ns\"><s:message>")
            + message.toHtmlEscaped().toUtf8() + QByteArrayLiteral("</s:message></d:error>");
    }

    QByteArray propResponse(const QString &path, const Node &node) const
    {
        QString href = davRoot() + path;
        if (node.folder && !href.endsWith(QLatin1Char('/')))
            href += QLatin1Char('/');
        QByteArray types;
        for (int type : node.shareTypes)
            types += "<oc:share-type>" + QByteArray::number(type) + "</oc:share-type>";
        return "<d:response><d:href>" + QUrl::toPercentEncoding(href, "/") + "</d:href><d:propstat><d:prop>"
            + (node.folder ? QByteArray("<d:resourcetype><d:collection/></d:resourcetype>")
                           : QByteArray("<d:resourcetype/>"))
            + (node.folder ? QByteArray()
                           : "<d:getcontentlength>" + QByteArray::number(node.content.size())
                        + "</d:getcontentlength><d:getcontenttype>text/"
                          "plain</d:getcontenttype>")
            + "<d:getlastmodified>Fri, 27 Nov 2026 10:00:00 "
              "GMT</d:getlastmodified><d:getetag>\"e"
            + QByteArray::number(node.fileId) + "\"</d:getetag><oc:fileid>" + QByteArray::number(node.fileId)
            + "</oc:fileid><oc:permissions>" + node.permissions.toUtf8() + "</oc:permissions><oc:share-types>" + types
            + "</oc:share-types></d:prop><d:status>HTTP/1.1 200 "
              "OK</d:status></d:propstat>"
              "<d:propstat><d:prop><nc:missing/></d:prop><d:status>HTTP/1.1 404 "
              "Not Found</d:status></d:propstat></d:response>";
    }

    static QString parentOf(const QString &path)
    {
        const qsizetype slash = path.lastIndexOf(QLatin1Char('/'));
        return slash <= 0 ? QStringLiteral("/") : path.left(slash);
    }

    QByteArray handle(const Request &request, int *status, QByteArray *headers, QByteArray *type)
    {
        const QUrl url(request.path);
        const QString path = url.path(QUrl::FullyDecoded);
        if (overrides.contains(path)) {
            *status = overrides.value(path).first;
            if (*status >= 300 && *status < 400)
                *headers = "Location: " + overrides.value(path).second + "\r\n";
            return *status >= 300 && *status < 400 ? QByteArray() : overrides.value(path).second;
        }
        if (path == base + QLatin1String("/index.php/login/v2") && request.verb == "POST") {
            m_loginToken = QStringLiteral("flow-token-%1").arg(++m_nextId);
            return QJsonDocument(
                QJsonObject{{QStringLiteral("poll"),
                                QJsonObject{{QStringLiteral("token"), m_loginToken},
                                    {QStringLiteral("endpoint"),
                                        QString(address() + QStringLiteral("/index.php/login/v2/poll"))}}},
                    {QStringLiteral("login"),
                        QString(address() + QStringLiteral("/index.php/login/v2/flow/") + m_loginToken)}})
                .toJson(QJsonDocument::Compact);
        }
        if (path == base + QLatin1String("/index.php/login/v2/poll") && request.verb == "POST") {
            ++loginPolls;
            const QString token = QUrlQuery(QString::fromLatin1(request.body)).queryItemValue(QStringLiteral("token"));
            if (!loginGranted || token != m_loginToken || m_loginToken.isEmpty()) {
                *status = 404;
                return "[]";
            }
            m_loginToken.clear();
            return QJsonDocument(
                QJsonObject{{QStringLiteral("server"), address()}, {QStringLiteral("loginName"), loginName},
                    {QStringLiteral("appPassword"), appPassword}})
                .toJson(QJsonDocument::Compact);
        }
        if (request.headers.value("authorization") != basic()) {
            *status = 401;
            *type = "application/xml";
            return davError(QStringLiteral("No public access to this resource."));
        }
        if (path == base + QLatin1String("/ocs/v1.php/cloud/user")) {
            if (request.headers.value("ocs-apirequest") != "true") {
                *status = 401;
                return ocs(997, QJsonObject{}, QStringLiteral("CSRF check failed"));
            }
            return ocs(100,
                QJsonObject{{QStringLiteral("id"), userId}, {QStringLiteral("display-name"), displayName},
                    {QStringLiteral("email"), loginName},
                    {QStringLiteral("quota"), QJsonObject{{QStringLiteral("free"), quotaFull ? 0 : 1000000}}}});
        }
        if (path == base + QLatin1String("/ocs/v2.php/apps/files_sharing/api/v1/shares")) {
            const QString wanted = QUrlQuery(url).queryItemValue(QStringLiteral("path"), QUrl::FullyDecoded);
            if (!wanted.isEmpty() && !nodes.contains(wanted)) {
                *status = 404;
                return ocs(404, QJsonArray{}, QStringLiteral("Wrong path, file/folder does not exist"));
            }
            QJsonArray data;
            for (const QJsonObject &share : std::as_const(shares)) {
                if (wanted.isEmpty() || share.value(QLatin1String("path")).toString() == wanted)
                    data.append(share);
            }
            return ocs(200, data);
        }
        if (!path.startsWith(davRoot())) {
            *status = 404;
            return {};
        }
        QString file = path.mid(davRoot().size());
        while (file.size() > 1 && file.endsWith(QLatin1Char('/')))
            file.chop(1);
        if (file.isEmpty())
            file = QStringLiteral("/");
        *type = "application/xml; charset=utf-8";
        if (request.verb == "PROPFIND") {
            if (!nodes.contains(file)) {
                *status = 404;
                return davError(QStringLiteral("File with name %1 could not be located").arg(file));
            }
            QByteArray body = "<?xml version=\"1.0\"?><d:multistatus "
                              "xmlns:d=\"DAV:\" xmlns:s=\"http://sabredav.org/ns\" "
                              "xmlns:oc=\"http://owncloud.org/ns\" "
                              "xmlns:nc=\"http://nextcloud.org/ns\">";
            body += propResponse(file, nodes.value(file));
            if (nodes.value(file).folder && request.headers.value("depth") == "1") {
                for (auto it = nodes.cbegin(); it != nodes.cend(); ++it) {
                    if (it.key() != file && parentOf(it.key()) == file)
                        body += propResponse(it.key(), it.value());
                }
            }
            *status = 207;
            return body + "</d:multistatus>";
        }
        if (request.verb == "GET") {
            if (!nodes.contains(file) || nodes.value(file).folder) {
                *status = 404;
                return davError(QStringLiteral("File not found"));
            }
            *type = "text/plain";
            return nodes.value(file).content;
        }
        if (request.verb == "PUT") {
            const QString parent = parentOf(file);
            if (!nodes.contains(parent) || !nodes.value(parent).folder) {
                *status = 409;
                return davError(QStringLiteral("Files can only be created as children of collections"));
            }
            const bool exists = nodes.contains(file);
            if (exists && request.headers.value("if-none-match") == "*") {
                *status = 412;
                return davError(QStringLiteral("An If-None-Match header was specified, but the "
                                               "ETag matched (or * was specified)."));
            }
            if (!nodes.value(parent).permissions.contains(QLatin1Char('C'))) {
                *status = 403;
                return davError(QStringLiteral("Could not write to the folder"));
            }
            if (quotaFull) {
                *status = 507;
                return davError(QStringLiteral("Insufficient storage"));
            }
            addFile(file, request.body);
            *status = exists ? 204 : 201;
            return {};
        }
        *status = 405;
        return {};
    }

    QTcpServer m_server;
    QString m_loginToken;
    qint64 m_nextId = 100;
};

#endif
