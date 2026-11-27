/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TESTS_JMAP_FAKEJMAPSERVER_H
#define TESTS_JMAP_FAKEJMAPSERVER_H

#include <memory>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QUrl>

/**
 * @short A JMAP server kept in memory, enough for one mail account (RFC 8620, RFC 8621)
 *
 * The session is found through a redirect from /.well-known/jmap and names its API with a relative URL. Every
 * request must carry the Basic sign-in of @ref user and @ref password.
 */
class FakeJmapServer : public QObject
{
public:
    struct Mailbox {
        QString name;
        QString parentId;
        QString role;
    };
    struct Email {
        QString blobId;
        QSet<QString> mailboxIds;
        QSet<QString> keywords;
        QString receivedAt;
    };

    QString user = QStringLiteral("alice");
    QString password = QStringLiteral("secret");
    QString accountId = QStringLiteral("A1");
    bool offersMail = true;
    bool canSubmit = true;
    bool refuseSubmission = false;
    /** Email/changes from states before this one cannot be calculated */
    int changesFloor = 0;
    QJsonArray identities{QJsonObject{{QStringLiteral("id"), QStringLiteral("I1")},
                                      {QStringLiteral("email"), QStringLiteral("alice@example.com")}}};

    QMap<QString, Mailbox> mailboxes;
    QMap<QString, Email> emails;
    QHash<QString, QByteArray> blobs;
    QList<QJsonObject> submissions;
    /** Every method called, in order */
    QStringList methods;
    int requests = 0;

    FakeJmapServer()
    {
        connect(&m_server, &QTcpServer::newConnection, this, &FakeJmapServer::accept);
        QVERIFY(m_server.listen(QHostAddress::LocalHost, 0));
    }

    QString address() const { return QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort()); }

    QString addMailbox(const QString &name, const QString &role = QString(), const QString &parentId = QString())
    {
        const QString id = QStringLiteral("M%1").arg(++m_nextId);
        mailboxes.insert(id, Mailbox{name, parentId, role});
        return id;
    }

    QString addEmail(const QByteArray &content, const QSet<QString> &mailboxIds, const QSet<QString> &keywords = {})
    {
        const QString blobId = QStringLiteral("B%1").arg(++m_nextId);
        blobs.insert(blobId, content);
        const QString id = QStringLiteral("E%1").arg(++m_nextId);
        emails.insert(id, Email{blobId, mailboxIds, keywords, QStringLiteral("2026-11-20T08:30:00Z")});
        touch(id, true);
        return id;
    }

    void changeEmail(const QString &id, const std::function<void(Email &)> &change)
    {
        change(emails[id]);
        touch(id, false);
    }

    void destroyEmail(const QString &id)
    {
        emails.remove(id);
        ++m_state;
        m_destroyed.insert(id, m_state);
    }

    QString mailboxByRole(const QString &role) const
    {
        for (auto it = mailboxes.cbegin(); it != mailboxes.cend(); ++it) {
            if (it->role == role)
                return it.key();
        }
        return {};
    }

    QString mailboxByName(const QString &name) const
    {
        for (auto it = mailboxes.cbegin(); it != mailboxes.cend(); ++it) {
            if (it->name == name)
                return it.key();
        }
        return {};
    }

    int state() const { return m_state; }

private:
    void touch(const QString &id, bool created)
    {
        ++m_state;
        if (created)
            m_created.insert(id, m_state);
        m_updated.insert(id, m_state);
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
                qsizetype length = 0;
                QByteArray authorization;
                const QList<QByteArray> lines = buffer->left(headerEnd).split('\n');
                for (const QByteArray &line : lines) {
                    const QByteArray lower = line.toLower();
                    if (lower.startsWith("content-length:"))
                        length = line.mid(15).trimmed().toLongLong();
                    if (lower.startsWith("authorization:"))
                        authorization = line.mid(14).trimmed();
                }
                if (buffer->size() < headerEnd + 4 + length)
                    return;
                const QList<QByteArray> request = lines.value(0).trimmed().split(' ');
                const QByteArray body = buffer->mid(headerEnd + 4, length);
                buffer->clear();
                ++requests;
                int status = 200;
                QByteArray extraHeaders;
                const QByteArray answer = handle(request.value(0), QString::fromLatin1(request.value(1)), body,
                                                 authorization, &status, &extraHeaders);
                socket->write("HTTP/1.1 " + QByteArray::number(status) + " Fake\r\n" + extraHeaders
                              + "Content-Type: application/json\r\nContent-Length: " + QByteArray::number(answer.size())
                              + "\r\nConnection: close\r\n\r\n" + answer);
                socket->disconnectFromHost();
            });
        }
    }

    QByteArray handle(const QByteArray &verb, const QString &path, const QByteArray &body, const QByteArray &authorization,
                      int *status, QByteArray *headers)
    {
        if (authorization != "Basic " + (user + QLatin1Char(':') + password).toUtf8().toBase64()) {
            *status = 401;
            return "{}";
        }
        if (path == QLatin1String("/.well-known/jmap")) {
            *status = 307;
            *headers = "Location: /jmap/session\r\n";
            return {};
        }
        if (path == QLatin1String("/jmap/session"))
            return QJsonDocument(session()).toJson();
        if (path == QLatin1String("/jmap/api") && verb == "POST")
            return api(QJsonDocument::fromJson(body).object());
        const QString download = QStringLiteral("/jmap/download/%1/").arg(accountId);
        if (path.startsWith(download) && verb == "GET") {
            const QString blobId = path.mid(download.size()).section(QLatin1Char('/'), 0, 0);
            if (!blobs.contains(blobId)) {
                *status = 404;
                return {};
            }
            return blobs.value(blobId);
        }
        if (path == QStringLiteral("/jmap/upload/%1/").arg(accountId) && verb == "POST") {
            const QString blobId = QStringLiteral("B%1").arg(++m_nextId);
            blobs.insert(blobId, body);
            return QJsonDocument(QJsonObject{{QStringLiteral("blobId"), blobId}, {QStringLiteral("size"), body.size()}}).toJson();
        }
        *status = 404;
        return {};
    }

    QJsonObject session() const
    {
        QJsonObject capabilities{{QStringLiteral("urn:ietf:params:jmap:core"),
                                  QJsonObject{{QStringLiteral("maxObjectsInGet"), 2}, {QStringLiteral("maxObjectsInSet"), 2}}}};
        QJsonObject accountCapabilities;
        if (offersMail) {
            capabilities.insert(QStringLiteral("urn:ietf:params:jmap:mail"), QJsonObject());
            accountCapabilities.insert(QStringLiteral("urn:ietf:params:jmap:mail"), QJsonObject());
        }
        if (canSubmit) {
            capabilities.insert(QStringLiteral("urn:ietf:params:jmap:submission"), QJsonObject());
            accountCapabilities.insert(QStringLiteral("urn:ietf:params:jmap:submission"), QJsonObject());
        }
        return QJsonObject{
            {QStringLiteral("capabilities"), capabilities},
            {QStringLiteral("accounts"), QJsonObject{{accountId, QJsonObject{{QStringLiteral("name"), user},
                                                                             {QStringLiteral("isReadOnly"), false},
                                                                             {QStringLiteral("accountCapabilities"), accountCapabilities}}}}},
            {QStringLiteral("primaryAccounts"), QJsonObject{{QStringLiteral("urn:ietf:params:jmap:mail"), accountId}}},
            {QStringLiteral("username"), user},
            {QStringLiteral("apiUrl"), QStringLiteral("/jmap/api")},
            {QStringLiteral("downloadUrl"), QStringLiteral("/jmap/download/{accountId}/{blobId}/{name}?type={type}")},
            {QStringLiteral("uploadUrl"), QStringLiteral("/jmap/upload/{accountId}/")},
            {QStringLiteral("state"), QStringLiteral("S1")},
        };
    }

    static QJsonObject trueSet(const QSet<QString> &keys)
    {
        QJsonObject object;
        for (const QString &key : keys)
            object.insert(key, true);
        return object;
    }

    static QString unescape(QString segment)
    {
        return segment.replace(QLatin1String("~1"), QLatin1String("/")).replace(QLatin1String("~0"), QLatin1String("~"));
    }

    QByteArray api(const QJsonObject &request)
    {
        QJsonArray responses;
        for (const QJsonValue &value : request.value(QLatin1String("methodCalls")).toArray()) {
            const QJsonArray call = value.toArray();
            const QString name = call.at(0).toString();
            const QJsonObject arguments = call.at(1).toObject();
            methods << name;
            QString responseName = name;
            const QJsonObject result = method(name, arguments, &responseName);
            responses.append(QJsonArray{responseName, result, call.at(2)});
        }
        return QJsonDocument(QJsonObject{{QStringLiteral("methodResponses"), responses},
                                         {QStringLiteral("sessionState"), QStringLiteral("S1")}})
            .toJson();
    }

    QJsonObject error(const QString &type, QString *responseName)
    {
        *responseName = QStringLiteral("error");
        return QJsonObject{{QStringLiteral("type"), type}};
    }

    QJsonObject method(const QString &name, const QJsonObject &arguments, QString *responseName)
    {
        const QString state = QString::number(m_state);
        if (name == QLatin1String("Mailbox/get")) {
            QJsonArray list;
            for (auto it = mailboxes.cbegin(); it != mailboxes.cend(); ++it) {
                list.append(QJsonObject{{QStringLiteral("id"), it.key()},
                                        {QStringLiteral("name"), it->name},
                                        {QStringLiteral("parentId"), it->parentId.isEmpty() ? QJsonValue() : QJsonValue(it->parentId)},
                                        {QStringLiteral("role"), it->role.isEmpty() ? QJsonValue() : QJsonValue(it->role)}});
            }
            return QJsonObject{{QStringLiteral("list"), list}, {QStringLiteral("state"), state}};
        }
        if (name == QLatin1String("Mailbox/set"))
            return mailboxSet(arguments);
        if (name == QLatin1String("Email/get")) {
            QJsonArray list, notFound;
            const QJsonArray ids = arguments.value(QLatin1String("ids")).toArray();
            for (const QJsonValue &id : ids) {
                if (!emails.contains(id.toString())) {
                    notFound.append(id);
                    continue;
                }
                const Email &email = emails[id.toString()];
                list.append(QJsonObject{{QStringLiteral("id"), id},
                                        {QStringLiteral("blobId"), email.blobId},
                                        {QStringLiteral("mailboxIds"), trueSet(email.mailboxIds)},
                                        {QStringLiteral("keywords"), trueSet(email.keywords)},
                                        {QStringLiteral("receivedAt"), email.receivedAt}});
            }
            return QJsonObject{{QStringLiteral("list"), list}, {QStringLiteral("notFound"), notFound}, {QStringLiteral("state"), state}};
        }
        if (name == QLatin1String("Email/query")) {
            const QStringList all = emails.keys();
            const int position = arguments.value(QLatin1String("position")).toInt();
            const int limit = qMin(arguments.value(QLatin1String("limit")).toInt(), 3);
            return QJsonObject{{QStringLiteral("ids"), QJsonArray::fromStringList(all.mid(position, limit))},
                               {QStringLiteral("total"), all.size()},
                               {QStringLiteral("position"), position}};
        }
        if (name == QLatin1String("Email/changes")) {
            const int since = arguments.value(QLatin1String("sinceState")).toString().toInt();
            if (since < changesFloor)
                return error(QStringLiteral("cannotCalculateChanges"), responseName);
            QJsonArray created, updated, destroyed;
            for (auto it = m_updated.cbegin(); it != m_updated.cend(); ++it) {
                if (it.value() <= since || !emails.contains(it.key()))
                    continue;
                if (m_created.value(it.key()) > since)
                    created.append(it.key());
                else
                    updated.append(it.key());
            }
            for (auto it = m_destroyed.cbegin(); it != m_destroyed.cend(); ++it) {
                if (it.value() > since && m_created.value(it.key()) <= since)
                    destroyed.append(it.key());
            }
            return QJsonObject{{QStringLiteral("oldState"), QString::number(since)},
                               {QStringLiteral("newState"), state},
                               {QStringLiteral("hasMoreChanges"), false},
                               {QStringLiteral("created"), created},
                               {QStringLiteral("updated"), updated},
                               {QStringLiteral("destroyed"), destroyed}};
        }
        if (name == QLatin1String("Email/set"))
            return emailSet(arguments);
        if (name == QLatin1String("Email/import")) {
            QJsonObject created, notCreated;
            const QJsonObject list = arguments.value(QLatin1String("emails")).toObject();
            for (auto it = list.begin(); it != list.end(); ++it) {
                const QJsonObject email = it.value().toObject();
                const QString blobId = email.value(QLatin1String("blobId")).toString();
                if (!blobs.contains(blobId)) {
                    notCreated.insert(it.key(), QJsonObject{{QStringLiteral("type"), QStringLiteral("blobNotFound")}});
                    continue;
                }
                QSet<QString> mailboxIds, keywords;
                const QJsonObject boxes = email.value(QLatin1String("mailboxIds")).toObject();
                for (auto box = boxes.begin(); box != boxes.end(); ++box)
                    mailboxIds.insert(box.key());
                const QJsonObject words = email.value(QLatin1String("keywords")).toObject();
                for (auto word = words.begin(); word != words.end(); ++word)
                    keywords.insert(word.key());
                const QString id = QStringLiteral("E%1").arg(++m_nextId);
                emails.insert(id, Email{blobId, mailboxIds, keywords,
                                        email.value(QLatin1String("receivedAt")).toString(QStringLiteral("2026-11-27T00:00:00Z"))});
                touch(id, true);
                created.insert(it.key(), QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("blobId"), blobId}});
            }
            return QJsonObject{{QStringLiteral("created"), created}, {QStringLiteral("notCreated"), notCreated}};
        }
        if (name == QLatin1String("Identity/get"))
            return QJsonObject{{QStringLiteral("list"), identities}, {QStringLiteral("state"), state}};
        if (name == QLatin1String("EmailSubmission/set")) {
            QJsonObject created, notCreated;
            const QJsonObject create = arguments.value(QLatin1String("create")).toObject();
            for (auto it = create.begin(); it != create.end(); ++it) {
                if (refuseSubmission || !emails.contains(it.value().toObject().value(QLatin1String("emailId")).toString())) {
                    notCreated.insert(it.key(), QJsonObject{{QStringLiteral("type"), QStringLiteral("forbiddenToSend")},
                                                            {QStringLiteral("description"), QStringLiteral("Sending is off")}});
                    continue;
                }
                submissions << it.value().toObject();
                created.insert(it.key(), QJsonObject{{QStringLiteral("id"), QStringLiteral("S%1").arg(++m_nextId)}});
            }
            return QJsonObject{{QStringLiteral("created"), created}, {QStringLiteral("notCreated"), notCreated}};
        }
        return error(QStringLiteral("unknownMethod"), responseName);
    }

    QJsonObject mailboxSet(const QJsonObject &arguments)
    {
        QJsonObject created, updated, notUpdated;
        QJsonArray destroyed;
        QHash<QString, QString> creations;
        const auto resolve = [&creations](const QJsonValue &parent) {
            const QString text = parent.toString();
            return text.startsWith(QLatin1Char('#')) ? creations.value(text.mid(1)) : text;
        };
        const QJsonObject create = arguments.value(QLatin1String("create")).toObject();
        for (auto it = create.begin(); it != create.end(); ++it) {
            const QJsonObject mailbox = it.value().toObject();
            const QString id = addMailbox(mailbox.value(QLatin1String("name")).toString(), QString(),
                                          resolve(mailbox.value(QLatin1String("parentId"))));
            creations.insert(it.key(), id);
            created.insert(it.key(), QJsonObject{{QStringLiteral("id"), id}});
        }
        const QJsonObject update = arguments.value(QLatin1String("update")).toObject();
        for (auto it = update.begin(); it != update.end(); ++it) {
            if (!mailboxes.contains(it.key())) {
                notUpdated.insert(it.key(), QJsonObject{{QStringLiteral("type"), QStringLiteral("notFound")}});
                continue;
            }
            const QJsonObject patch = it.value().toObject();
            if (patch.contains(QLatin1String("name")))
                mailboxes[it.key()].name = patch.value(QLatin1String("name")).toString();
            if (patch.contains(QLatin1String("parentId")))
                mailboxes[it.key()].parentId = resolve(patch.value(QLatin1String("parentId")));
            updated.insert(it.key(), QJsonValue());
        }
        for (const QJsonValue &id : arguments.value(QLatin1String("destroy")).toArray()) {
            mailboxes.remove(id.toString());
            const QStringList ids = emails.keys();
            for (const QString &email : ids) {
                if (emails[email].mailboxIds.remove(id.toString())) {
                    if (emails[email].mailboxIds.isEmpty())
                        destroyEmail(email);
                    else
                        touch(email, false);
                }
            }
            destroyed.append(id);
        }
        return QJsonObject{{QStringLiteral("created"), created}, {QStringLiteral("updated"), updated},
                           {QStringLiteral("notUpdated"), notUpdated}, {QStringLiteral("destroyed"), destroyed}};
    }

    QJsonObject emailSet(const QJsonObject &arguments)
    {
        QJsonObject updated, notUpdated, notDestroyed;
        QJsonArray destroyed;
        const QJsonObject update = arguments.value(QLatin1String("update")).toObject();
        for (auto it = update.begin(); it != update.end(); ++it) {
            if (!emails.contains(it.key())) {
                notUpdated.insert(it.key(), QJsonObject{{QStringLiteral("type"), QStringLiteral("notFound")}});
                continue;
            }
            Email &email = emails[it.key()];
            const QJsonObject patch = it.value().toObject();
            for (auto change = patch.begin(); change != patch.end(); ++change) {
                const QString property = change.key().section(QLatin1Char('/'), 0, 0);
                const QString key = unescape(change.key().section(QLatin1Char('/'), 1));
                QSet<QString> &set = property == QLatin1String("keywords") ? email.keywords : email.mailboxIds;
                if (change.value().toBool())
                    set.insert(key);
                else
                    set.remove(key);
            }
            touch(it.key(), false);
            updated.insert(it.key(), QJsonValue());
        }
        for (const QJsonValue &id : arguments.value(QLatin1String("destroy")).toArray()) {
            if (!emails.contains(id.toString())) {
                notDestroyed.insert(id.toString(), QJsonObject{{QStringLiteral("type"), QStringLiteral("notFound")}});
                continue;
            }
            destroyEmail(id.toString());
            destroyed.append(id);
        }
        return QJsonObject{{QStringLiteral("updated"), updated}, {QStringLiteral("notUpdated"), notUpdated},
                           {QStringLiteral("destroyed"), destroyed}, {QStringLiteral("notDestroyed"), notDestroyed}};
    }

    QTcpServer m_server;
    int m_nextId = 0;
    int m_state = 0;
    QHash<QString, int> m_created;
    QHash<QString, int> m_updated;
    QHash<QString, int> m_destroyed;
};

#endif
