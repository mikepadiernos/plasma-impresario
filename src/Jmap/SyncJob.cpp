/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Jmap/SyncJob.h"

#include <algorithm>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include "LocalMail/Store.h"

namespace Jmap {

namespace {

const QString originPrefix = QStringLiteral("jmap:");
const QString inbox = QStringLiteral("INBOX");
constexpr int queryPageSize = 500;

/** Keywords with a usual IMAP spelling (RFC 8621 section 4.1.1, RFC 5788) */
const QList<QPair<QString, QString>> &knownKeywords()
{
    static const QList<QPair<QString, QString>> known = {
        {QStringLiteral("$seen"), QStringLiteral("\\Seen")},
        {QStringLiteral("$flagged"), QStringLiteral("\\Flagged")},
        {QStringLiteral("$answered"), QStringLiteral("\\Answered")},
        {QStringLiteral("$draft"), QStringLiteral("\\Draft")},
        {QStringLiteral("$forwarded"), QStringLiteral("$Forwarded")},
        {QStringLiteral("$junk"), QStringLiteral("$Junk")},
        {QStringLiteral("$notjunk"), QStringLiteral("$NotJunk")},
        {QStringLiteral("$mdnsent"), QStringLiteral("$MDNSent")},
        {QStringLiteral("$phishing"), QStringLiteral("$Phishing")},
    };
    return known;
}

/** Local folder names for the mailbox roles of RFC 8621 section 2 */
QString folderForRole(const QString &role)
{
    static const QHash<QString, QString> folders = {
        {QStringLiteral("inbox"), inbox},
        {QStringLiteral("sent"), QStringLiteral("Sent")},
        {QStringLiteral("drafts"), QStringLiteral("Drafts")},
        {QStringLiteral("trash"), QStringLiteral("Trash")},
        {QStringLiteral("junk"), QStringLiteral("Junk")},
        {QStringLiteral("archive"), QStringLiteral("Archive")},
    };
    return folders.value(role);
}

QString emailId(const QString &origin)
{
    return origin.startsWith(originPrefix) ? origin.mid(originPrefix.size()) : QString();
}

QStringList sorted(QStringList list)
{
    list.removeDuplicates();
    list.sort();
    return list;
}

/** The keys set to true; keywords are case-insensitive (RFC 8621, section 4.1.1), ids are not */
QStringList setKeys(const QJsonObject &object, Qt::CaseSensitivity sensitivity = Qt::CaseSensitive)
{
    QStringList keys;
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (it.value().toBool())
            keys << (sensitivity == Qt::CaseInsensitive ? it.key().toLower() : it.key());
    }
    return sorted(keys);
}

QJsonObject trueSet(const QStringList &keys)
{
    QJsonObject object;
    for (const QString &key : keys)
        object.insert(key, true);
    return object;
}

/** A JSON pointer segment (RFC 6901) */
QString pointerSegment(QString key)
{
    return key.replace(QLatin1Char('~'), QLatin1String("~0")).replace(QLatin1Char('/'), QLatin1String("~1"));
}

QString parentOf(const QString &folder)
{
    const int slash = folder.lastIndexOf(QLatin1Char('/'));
    return slash < 0 ? QString() : folder.left(slash);
}

QString leafOf(const QString &folder)
{
    return folder.mid(folder.lastIndexOf(QLatin1Char('/')) + 1);
}

QString setErrorText(const QJsonValue &error)
{
    const QJsonObject object = error.toObject();
    const QString description = object.value(QLatin1String("description")).toString();
    return description.isEmpty() ? object.value(QLatin1String("type")).toString() : description;
}

}

SyncJob::SyncJob(const Settings &settings, std::shared_ptr<LocalMail::Store> store, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_store(std::move(store))
    , m_client(new Client(settings, this))
{
    connect(m_client, &Client::certificateRejected, this, &SyncJob::certificateRejected);
    connect(m_client, &Client::failed, this, &SyncJob::fail);
}

SyncJob::~SyncJob()
{
    abort();
}

QString SyncJob::stateFileName()
{
    return QStringLiteral("jmap-state.json");
}

QStringList SyncJob::keywordsToFlags(const QStringList &keywords)
{
    QStringList flags;
    for (const QString &keyword : keywords) {
        const QString lower = keyword.toLower();
        QString flag = keyword;
        for (const auto &known : knownKeywords()) {
            if (known.first == lower)
                flag = known.second;
        }
        flags << flag;
    }
    return flags;
}

QStringList SyncJob::flagsToKeywords(const QStringList &flags)
{
    QStringList keywords;
    for (const QString &flag : flags) {
        QString keyword;
        for (const auto &known : knownKeywords()) {
            if (known.second.compare(flag, Qt::CaseInsensitive) == 0)
                keyword = known.first;
        }
        if (keyword.isEmpty() && !flag.startsWith(QLatin1Char('\\')))
            keyword = flag.toLower();
        if (!keyword.isEmpty())
            keywords << keyword;
    }
    return sorted(keywords);
}

void SyncJob::setTrustCheck(Client::TrustCheck check)
{
    m_client->setTrustCheck(std::move(check));
}

void SyncJob::setTimeout(int seconds)
{
    m_client->setTimeout(seconds);
}

bool SyncJob::isRunning() const
{
    return m_running;
}

int SyncJob::downloaded() const
{
    return m_downloaded;
}

int SyncJob::uploaded() const
{
    return m_uploaded;
}

bool SyncJob::canSubmit() const
{
    return m_canSubmit;
}

QStringList SyncJob::warnings() const
{
    return m_warnings;
}

void SyncJob::start()
{
    if (m_running)
        return;
    if (!m_store) {
        fail(Failure::Storage, tr("The mail on this computer could not be opened."));
        return;
    }
    m_running = true;
    loadState();
    m_client->fetchSession([this]() { onSession(); });
}

void SyncJob::abort()
{
    if (!m_running)
        return;
    m_running = false;
    if (m_client)
        m_client->abort();
}

void SyncJob::fail(Failure failure, const QString &message)
{
    if (!m_running && failure != Failure::Storage)
        return;
    const bool wasRunning = m_running;
    m_running = false;
    if (m_client)
        m_client->abort();
    // What was agreed with the server so far stays agreed
    if (wasRunning && !m_accountId.isEmpty())
        saveState();
    emit failed(failure, message);
}

void SyncJob::storageFailure(const QString &error)
{
    fail(Failure::Storage, error.isEmpty() ? tr("The mail on this computer could not be changed.") : error);
}

void SyncJob::warn(const QString &message)
{
    m_warnings << message;
}

void SyncJob::loadState()
{
    QFile file(m_store->folder() + QLatin1Char('/') + stateFileName());
    const QJsonObject state = file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
    m_accountId = state.value(QLatin1String("accountId")).toString();
    m_emailState = state.value(QLatin1String("emailState")).toString();
    m_mailboxes.clear();
    m_emails.clear();
    m_localOnly.clear();
    const QJsonObject mailboxes = state.value(QLatin1String("mailboxes")).toObject();
    for (auto it = mailboxes.begin(); it != mailboxes.end(); ++it) {
        const QJsonObject value = it.value().toObject();
        MailboxState mailbox;
        mailbox.name = value.value(QLatin1String("name")).toString();
        mailbox.uidValidity = uint(value.value(QLatin1String("uidValidity")).toInteger());
        mailbox.serverName = value.value(QLatin1String("serverName")).toString();
        mailbox.parentId = value.value(QLatin1String("parentId")).toString();
        mailbox.role = value.value(QLatin1String("role")).toString();
        m_mailboxes.insert(it.key(), mailbox);
    }
    for (const QJsonValue &value : state.value(QLatin1String("localOnly")).toArray())
        m_localOnly.insert(uint(value.toInteger()));
    const QJsonObject emails = state.value(QLatin1String("emails")).toObject();
    for (auto it = emails.begin(); it != emails.end(); ++it) {
        const QJsonObject value = it.value().toObject();
        EmailState email;
        for (const QJsonValue &keyword : value.value(QLatin1String("k")).toArray())
            email.keywords << keyword.toString();
        for (const QJsonValue &mailbox : value.value(QLatin1String("m")).toArray())
            email.mailboxIds << mailbox.toString();
        m_emails.insert(it.key(), email);
    }
    m_hadState = !m_accountId.isEmpty() && !m_mailboxes.isEmpty();
}

bool SyncJob::saveState()
{
    QJsonObject mailboxes;
    for (auto it = m_mailboxes.cbegin(); it != m_mailboxes.cend(); ++it) {
        mailboxes.insert(it.key(), QJsonObject{{QStringLiteral("name"), it->name},
                                               {QStringLiteral("uidValidity"), qint64(it->uidValidity)},
                                               {QStringLiteral("serverName"), it->serverName},
                                               {QStringLiteral("parentId"), it->parentId},
                                               {QStringLiteral("role"), it->role}});
    }
    QJsonArray localOnly;
    for (const uint validity : std::as_const(m_localOnly))
        localOnly.append(qint64(validity));
    QJsonObject emails;
    for (auto it = m_emails.cbegin(); it != m_emails.cend(); ++it) {
        emails.insert(it.key(), QJsonObject{{QStringLiteral("k"), QJsonArray::fromStringList(it->keywords)},
                                            {QStringLiteral("m"), QJsonArray::fromStringList(it->mailboxIds)}});
    }
    const QJsonObject state{{QStringLiteral("version"), 1},
                            {QStringLiteral("accountId"), m_accountId},
                            {QStringLiteral("emailState"), m_emailState},
                            {QStringLiteral("mailboxes"), mailboxes},
                            {QStringLiteral("localOnly"), localOnly},
                            {QStringLiteral("emails"), emails}};
    QSaveFile file(m_store->folder() + QLatin1Char('/') + stateFileName());
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    file.write(QJsonDocument(state).toJson(QJsonDocument::Compact));
    return file.commit();
}

QHash<uint, QString> SyncJob::localFolders() const
{
    QHash<uint, QString> folders;
    for (const QString &name : m_store->mailboxNames())
        folders.insert(m_store->mailbox(name).uidValidity, name);
    return folders;
}

QHash<uint, QString> SyncJob::mailboxIdsByValidity() const
{
    QHash<uint, QString> ids;
    for (auto it = m_mailboxes.cbegin(); it != m_mailboxes.cend(); ++it)
        ids.insert(it->uidValidity, it.key());
    return ids;
}

void SyncJob::onSession()
{
    if (!m_running)
        return;
    const Session &session = m_client->session();
    m_canSubmit = session.canSubmit;
    if (session.accountId != m_accountId) {
        // Another account, or the first visit: nothing agreed before applies
        m_accountId = session.accountId;
        m_emailState.clear();
        m_mailboxes.clear();
        m_emails.clear();
        m_localOnly.clear();
        m_hadState = false;
    }
    if (m_hadState && !session.readOnly)
        pushMailboxes();
    else
        pullMailboxes();
}

void SyncJob::pushMailboxes()
{
    const QHash<uint, QString> ids = mailboxIdsByValidity();
    const QHash<uint, QString> folders = localFolders();

    QStringList added;
    for (auto it = folders.cbegin(); it != folders.cend(); ++it) {
        if (!ids.contains(it.key()) && !m_localOnly.contains(it.key()))
            added << it.value();
    }
    std::sort(added.begin(), added.end(), [](const QString &a, const QString &b) {
        const auto depthA = a.count(QLatin1Char('/')), depthB = b.count(QLatin1Char('/'));
        return depthA != depthB ? depthA < depthB : a < b;
    });

    // The parent of a folder: a server mailbox, a mailbox created in the same request, or none for a top folder
    QHash<QString, QString> creationByFolder;
    const auto parentReference = [&](const QString &folder, QJsonValue *reference) {
        const QString parent = parentOf(folder);
        if (parent.isEmpty()) {
            *reference = QJsonValue::Null;
            return true;
        }
        if (creationByFolder.contains(parent)) {
            *reference = QString(QLatin1Char('#') + creationByFolder.value(parent));
            return true;
        }
        const QString id = ids.value(m_store->mailbox(parent).uidValidity);
        if (!m_store->hasMailbox(parent) || id.isEmpty())
            return false;
        *reference = id;
        return true;
    };

    QJsonObject create;
    for (const QString &folder : std::as_const(added)) {
        QJsonValue parent;
        if (!parentReference(folder, &parent))
            continue;
        const QString creationId = QStringLiteral("c%1").arg(creationByFolder.size());
        creationByFolder.insert(folder, creationId);
        create.insert(creationId, QJsonObject{{QStringLiteral("name"), leafOf(folder)}, {QStringLiteral("parentId"), parent}});
    }

    QJsonObject update;
    m_pendingDestroy.clear();
    for (auto it = m_mailboxes.cbegin(); it != m_mailboxes.cend(); ++it) {
        const QString folder = folders.value(it->uidValidity);
        if (!it->role.isEmpty())
            continue;
        if (folder.isEmpty()) {
            m_pendingDestroy << it.key();
            continue;
        }
        if (folder == it->name)
            continue;
        QJsonValue parent;
        if (!parentReference(folder, &parent))
            continue;
        if (leafOf(folder) != it->serverName || parent.toString() != it->parentId)
            update.insert(it.key(), QJsonObject{{QStringLiteral("name"), leafOf(folder)}, {QStringLiteral("parentId"), parent}});
    }

    if (create.isEmpty() && update.isEmpty()) {
        pushEmails();
        return;
    }
    QJsonObject arguments{{QStringLiteral("accountId"), m_accountId}};
    if (!create.isEmpty())
        arguments.insert(QStringLiteral("create"), create);
    if (!update.isEmpty())
        arguments.insert(QStringLiteral("update"), update);
    m_client->call(QJsonArray{Client::method(QStringLiteral("Mailbox/set"), arguments, QStringLiteral("m"))},
                   [this, create, update, creationByFolder, folders](const QJsonArray &responses) {
        if (!m_running)
            return;
        QString error;
        const QJsonObject result = Client::response(responses, QStringLiteral("m"), &error);
        if (!error.isEmpty()) {
            fail(Failure::Protocol, tr("The JMAP server refused to change folders: %1").arg(error));
            return;
        }
        QHash<QString, QString> createdIds;
        const QJsonObject created = result.value(QLatin1String("created")).toObject();
        for (auto it = created.begin(); it != created.end(); ++it)
            createdIds.insert(it.key(), it.value().toObject().value(QLatin1String("id")).toString());
        const auto resolve = [&createdIds](const QJsonValue &parent) {
            const QString text = parent.toString();
            return text.startsWith(QLatin1Char('#')) ? createdIds.value(text.mid(1)) : text;
        };
        for (auto it = creationByFolder.cbegin(); it != creationByFolder.cend(); ++it) {
            const QString id = createdIds.value(it.value());
            if (id.isEmpty()) {
                warn(tr("The JMAP server did not create the folder %1: %2")
                         .arg(it.key(), setErrorText(result.value(QLatin1String("notCreated")).toObject().value(it.value()))));
                continue;
            }
            MailboxState mailbox;
            mailbox.name = it.key();
            mailbox.uidValidity = m_store->mailbox(it.key()).uidValidity;
            mailbox.serverName = leafOf(it.key());
            mailbox.parentId = resolve(create.value(it.value()).toObject().value(QLatin1String("parentId")));
            m_mailboxes.insert(id, mailbox);
        }
        const QJsonObject updated = result.value(QLatin1String("updated")).toObject();
        const QJsonObject notUpdated = result.value(QLatin1String("notUpdated")).toObject();
        for (auto it = update.begin(); it != update.end(); ++it) {
            auto mailbox = m_mailboxes.find(it.key());
            if (mailbox == m_mailboxes.end())
                continue;
            if (!updated.contains(it.key())) {
                warn(tr("The JMAP server did not rename the folder %1: %2")
                         .arg(mailbox->name, setErrorText(notUpdated.value(it.key()))));
                continue;
            }
            mailbox->name = folders.value(mailbox->uidValidity);
            mailbox->serverName = it.value().toObject().value(QLatin1String("name")).toString();
            mailbox->parentId = resolve(it.value().toObject().value(QLatin1String("parentId")));
        }
        saveState();
        pushEmails();
    });
}

void SyncJob::pushEmails()
{
    const QHash<uint, QString> ids = mailboxIdsByValidity();
    QHash<QString, QSet<QString>> localMailboxes;
    QHash<QString, QStringList> localKeywords;
    QSet<QString> undecided;
    m_imports.clear();
    m_importCursor = 0;
    for (const QString &folder : m_store->mailboxNames()) {
        const uint validity = m_store->mailbox(folder).uidValidity;
        const QString mailboxId = ids.value(validity);
        const bool localOnly = m_localOnly.contains(validity);
        for (const LocalMail::MessageInfo &message : m_store->messages(folder)) {
            const QString id = emailId(message.origin);
            const bool known = !id.isEmpty() && m_emails.contains(id);
            if (mailboxId.isEmpty()) {
                // A folder the server refused, or one under a local-only folder: wait until it exists there
                if (known && !localOnly)
                    undecided.insert(id);
                continue;
            }
            const QStringList keywords = flagsToKeywords(message.flags);
            if (!known) {
                m_imports << Import{folder, message.uid, mailboxId, keywords, message.internalDate, QString()};
                continue;
            }
            localMailboxes[id].insert(mailboxId);
            // Copies which disagree: the one changed since the last visit wins
            auto current = localKeywords.find(id);
            if (current == localKeywords.end())
                localKeywords.insert(id, keywords);
            else if (*current == m_emails.value(id).keywords && keywords != *current)
                *current = keywords;
        }
    }

    m_emailUpdates = QJsonObject();
    m_emailDestroys.clear();
    m_afterUpdate.clear();
    for (auto it = m_emails.cbegin(); it != m_emails.cend(); ++it) {
        if (undecided.contains(it.key()))
            continue;
        if (!localMailboxes.contains(it.key())) {
            m_emailDestroys << it.key();
            continue;
        }
        const QStringList mailboxes = sorted(QStringList(localMailboxes.value(it.key()).cbegin(), localMailboxes.value(it.key()).cend()));
        const QStringList keywords = localKeywords.value(it.key());
        QJsonObject patch;
        for (const QString &mailbox : mailboxes) {
            if (!it->mailboxIds.contains(mailbox))
                patch.insert(QLatin1String("mailboxIds/") + pointerSegment(mailbox), true);
        }
        for (const QString &mailbox : it->mailboxIds) {
            if (!mailboxes.contains(mailbox))
                patch.insert(QLatin1String("mailboxIds/") + pointerSegment(mailbox), QJsonValue::Null);
        }
        for (const QString &keyword : keywords) {
            if (!it->keywords.contains(keyword))
                patch.insert(QLatin1String("keywords/") + pointerSegment(keyword), true);
        }
        for (const QString &keyword : it->keywords) {
            if (!keywords.contains(keyword))
                patch.insert(QLatin1String("keywords/") + pointerSegment(keyword), QJsonValue::Null);
        }
        if (patch.isEmpty())
            continue;
        m_emailUpdates.insert(it.key(), patch);
        m_afterUpdate.insert(it.key(), EmailState{keywords, mailboxes});
    }
    runEmailSet();
}

void SyncJob::runEmailSet()
{
    if (m_emailUpdates.isEmpty() && m_emailDestroys.isEmpty()) {
        uploadNext();
        return;
    }
    const int limit = m_client->session().maxObjectsInSet;
    QJsonObject update;
    QJsonArray destroy;
    while (!m_emailUpdates.isEmpty() && update.size() < limit) {
        const QString id = m_emailUpdates.begin().key();
        update.insert(id, m_emailUpdates.take(id));
    }
    while (!m_emailDestroys.isEmpty() && update.size() + destroy.size() < limit)
        destroy.append(m_emailDestroys.takeFirst());
    QJsonObject arguments{{QStringLiteral("accountId"), m_accountId}};
    if (!update.isEmpty())
        arguments.insert(QStringLiteral("update"), update);
    if (!destroy.isEmpty())
        arguments.insert(QStringLiteral("destroy"), destroy);
    m_client->call(QJsonArray{Client::method(QStringLiteral("Email/set"), arguments, QStringLiteral("e"))},
                   [this, update, destroy](const QJsonArray &responses) {
        if (!m_running)
            return;
        QString error;
        const QJsonObject result = Client::response(responses, QStringLiteral("e"), &error);
        if (!error.isEmpty()) {
            fail(Failure::Protocol, tr("The JMAP server refused to change messages: %1").arg(error));
            return;
        }
        const QJsonObject updated = result.value(QLatin1String("updated")).toObject();
        const QJsonObject notUpdated = result.value(QLatin1String("notUpdated")).toObject();
        for (auto it = update.begin(); it != update.end(); ++it) {
            if (updated.contains(it.key()))
                m_emails.insert(it.key(), m_afterUpdate.value(it.key()));
            else
                warn(tr("The JMAP server did not change a message: %1").arg(setErrorText(notUpdated.value(it.key()))));
        }
        const QJsonObject notDestroyed = result.value(QLatin1String("notDestroyed")).toObject();
        for (const QJsonValue &id : result.value(QLatin1String("destroyed")).toArray())
            m_emails.remove(id.toString());
        for (auto it = notDestroyed.begin(); it != notDestroyed.end(); ++it) {
            if (it.value().toObject().value(QLatin1String("type")).toString() == QLatin1String("notFound"))
                m_emails.remove(it.key());
            else
                warn(tr("The JMAP server did not remove a message: %1").arg(setErrorText(it.value())));
        }
        saveState();
        runEmailSet();
    });
}

void SyncJob::uploadNext()
{
    while (m_importCursor < m_imports.size()) {
        Import &import = m_imports[m_importCursor];
        const QByteArray content = m_store->content(import.folder, import.uid);
        if (content.isEmpty()) {
            // Removed while the visit went on
            m_imports.removeAt(m_importCursor);
            continue;
        }
        m_client->upload(content, "message/rfc822", [this](const QString &blobId) {
            if (!m_running)
                return;
            m_imports[m_importCursor].blobId = blobId;
            ++m_importCursor;
            uploadNext();
        });
        return;
    }
    m_importCursor = 0;
    runImports();
}

void SyncJob::runImports()
{
    if (m_importCursor >= m_imports.size()) {
        m_imports.clear();
        destroyMailboxes();
        return;
    }
    const int first = m_importCursor;
    const int count = qMin(int(m_imports.size()) - first, m_client->session().maxObjectsInSet);
    m_importCursor += count;
    QJsonObject emails;
    for (int i = first; i < first + count; ++i) {
        const Import &import = m_imports.at(i);
        QJsonObject email{{QStringLiteral("blobId"), import.blobId},
                          {QStringLiteral("mailboxIds"), trueSet({import.mailboxId})},
                          {QStringLiteral("keywords"), trueSet(import.keywords)}};
        if (import.date.isValid())
            email.insert(QStringLiteral("receivedAt"), import.date.toUTC().toString(Qt::ISODate));
        emails.insert(QStringLiteral("i%1").arg(i), email);
    }
    m_client->call(QJsonArray{Client::method(QStringLiteral("Email/import"),
                                             QJsonObject{{QStringLiteral("accountId"), m_accountId},
                                                         {QStringLiteral("emails"), emails}},
                                             QStringLiteral("i"))},
                   [this, first, count](const QJsonArray &responses) {
        if (!m_running)
            return;
        QString error;
        const QJsonObject result = Client::response(responses, QStringLiteral("i"), &error);
        if (!error.isEmpty()) {
            fail(Failure::Protocol, tr("The JMAP server refused to store messages: %1").arg(error));
            return;
        }
        const QJsonObject created = result.value(QLatin1String("created")).toObject();
        const QJsonObject notCreated = result.value(QLatin1String("notCreated")).toObject();
        for (int i = first; i < first + count; ++i) {
            const Import &import = m_imports.at(i);
            const QString key = QStringLiteral("i%1").arg(i);
            QString id = created.value(key).toObject().value(QLatin1String("id")).toString();
            const QJsonObject refused = notCreated.value(key).toObject();
            if (id.isEmpty() && refused.value(QLatin1String("type")).toString() == QLatin1String("alreadyExists")) {
                // The server has this message already; the next visit adds it to this folder
                id = refused.value(QLatin1String("existingId")).toString();
                if (!id.isEmpty() && !m_emails.contains(id))
                    m_emails.insert(id, EmailState());
            } else if (!id.isEmpty()) {
                EmailState &state = m_emails[id];
                state.keywords = import.keywords;
                state.mailboxIds = sorted(state.mailboxIds + QStringList{import.mailboxId});
            }
            if (id.isEmpty()) {
                warn(tr("The JMAP server did not store a message from %1: %2").arg(import.folder, setErrorText(refused)));
                continue;
            }
            // The local message becomes the copy of the server's
            QStringList flags;
            bool exists = false;
            for (const LocalMail::MessageInfo &message : m_store->messages(import.folder)) {
                if (message.uid == import.uid) {
                    flags = message.flags;
                    exists = true;
                }
            }
            if (!exists)
                continue;
            QString storeError;
            const QByteArray content = m_store->content(import.folder, import.uid);
            if (!m_store->append(import.folder, content, flags, import.date, originPrefix + id, &storeError)
                || !m_store->remove(import.folder, {import.uid}, &storeError)) {
                saveState();
                storageFailure(storeError);
                return;
            }
            ++m_uploaded;
        }
        saveState();
        runImports();
    });
}

void SyncJob::destroyMailboxes()
{
    if (m_pendingDestroy.isEmpty()) {
        pullMailboxes();
        return;
    }
    const QStringList destroy = m_pendingDestroy;
    m_pendingDestroy.clear();
    m_client->call(QJsonArray{Client::method(QStringLiteral("Mailbox/set"),
                                             QJsonObject{{QStringLiteral("accountId"), m_accountId},
                                                         {QStringLiteral("destroy"), QJsonArray::fromStringList(destroy)},
                                                         {QStringLiteral("onDestroyRemoveEmails"), true}},
                                             QStringLiteral("d"))},
                   [this, destroy](const QJsonArray &responses) {
        if (!m_running)
            return;
        QString error;
        const QJsonObject result = Client::response(responses, QStringLiteral("d"), &error);
        if (!error.isEmpty()) {
            fail(Failure::Protocol, tr("The JMAP server refused to remove folders: %1").arg(error));
            return;
        }
        for (const QJsonValue &id : result.value(QLatin1String("destroyed")).toArray())
            m_mailboxes.remove(id.toString());
        const QJsonObject notDestroyed = result.value(QLatin1String("notDestroyed")).toObject();
        for (auto it = notDestroyed.begin(); it != notDestroyed.end(); ++it) {
            if (it.value().toObject().value(QLatin1String("type")).toString() == QLatin1String("notFound"))
                m_mailboxes.remove(it.key());
            else
                warn(tr("The JMAP server did not remove the folder %1: %2")
                         .arg(m_mailboxes.value(it.key()).name, setErrorText(it.value())));
        }
        saveState();
        pullMailboxes();
    });
}

void SyncJob::pullMailboxes()
{
    const QJsonObject arguments{{QStringLiteral("accountId"), m_accountId},
                                {QStringLiteral("ids"), QJsonValue::Null},
                                {QStringLiteral("properties"),
                                 QJsonArray{QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("parentId"),
                                            QStringLiteral("role")}}};
    m_client->call(QJsonArray{Client::method(QStringLiteral("Mailbox/get"), arguments, QStringLiteral("g"))},
                   [this](const QJsonArray &responses) {
        if (!m_running)
            return;
        QString error;
        const QJsonObject result = Client::response(responses, QStringLiteral("g"), &error);
        if (!error.isEmpty()) {
            fail(Failure::Protocol, tr("The JMAP server did not list its folders: %1").arg(error));
            return;
        }
        struct Server {
            QString name;
            QString parentId;
            QString role;
            int depth = 0;
        };
        QHash<QString, Server> server;
        for (const QJsonValue &value : result.value(QLatin1String("list")).toArray()) {
            const QJsonObject mailbox = value.toObject();
            server.insert(mailbox.value(QLatin1String("id")).toString(),
                          Server{mailbox.value(QLatin1String("name")).toString(),
                                 mailbox.value(QLatin1String("parentId")).toString(),
                                 mailbox.value(QLatin1String("role")).toString().toLower(), 0});
        }
        QStringList order = server.keys();
        for (const QString &id : std::as_const(order)) {
            int depth = 0;
            for (QString parent = server.value(id).parentId; server.contains(parent) && depth < 64;
                 parent = server.value(parent).parentId)
                ++depth;
            server[id].depth = depth;
        }
        std::sort(order.begin(), order.end(), [&server](const QString &a, const QString &b) {
            const Server &x = server[a], &y = server[b];
            return x.depth != y.depth ? x.depth < y.depth : x.name < y.name;
        });

        // Local names: special mailboxes by role, the others by their path; clashes get a number
        QHash<QString, QString> wanted;
        QSet<QString> taken;
        for (const QString &id : std::as_const(order)) {
            const QString folder = folderForRole(server.value(id).role);
            if (!folder.isEmpty() && !taken.contains(folder.toLower())) {
                wanted.insert(id, folder);
                taken.insert(folder.toLower());
            }
        }
        for (const QString &id : std::as_const(order)) {
            if (wanted.contains(id))
                continue;
            const Server &mailbox = server.value(id);
            QString leaf = mailbox.name;
            leaf.replace(QLatin1Char('/'), QLatin1Char('_'));
            const QString parent = wanted.value(mailbox.parentId);
            const QString base = parent.isEmpty() ? leaf : parent + QLatin1Char('/') + leaf;
            QString name = base;
            for (int n = 2; taken.contains(name.toLower()); ++n)
                name = QStringLiteral("%1 (%2)").arg(base).arg(n);
            wanted.insert(id, name);
            taken.insert(name.toLower());
        }

        QSet<uint> mapped;
        for (auto it = m_mailboxes.cbegin(); it != m_mailboxes.cend(); ++it) {
            if (server.contains(it.key()))
                mapped.insert(it->uidValidity);
        }
        QString storeError;
        for (const QString &id : std::as_const(order)) {
            const QString target = wanted.value(id);
            const QHash<uint, QString> folders = localFolders();
            const auto known = m_mailboxes.constFind(id);
            QString folder = known != m_mailboxes.cend() ? folders.value(known->uidValidity) : QString();
            if (!folder.isEmpty()) {
                if (folder != target) {
                    if (m_store->hasMailbox(target))
                        warn(tr("The folder %1 could not be renamed to %2, which exists already.").arg(folder, target));
                    else if (m_store->renameMailbox(folder, target, &storeError))
                        folder = target;
                    else
                        warn(storeError);
                }
            } else if (m_store->hasMailbox(target) && !mapped.contains(m_store->mailbox(target).uidValidity)) {
                folder = target;
            } else if (!m_store->hasMailbox(target)) {
                if (!m_store->createMailbox(target, &storeError)) {
                    storageFailure(storeError);
                    return;
                }
                folder = target;
            } else {
                warn(tr("The server's folder %1 clashes with a folder on this computer.").arg(target));
                continue;
            }
            const Server &mailbox = server.value(id);
            const uint validity = m_store->mailbox(folder).uidValidity;
            m_mailboxes.insert(id, MailboxState{folder, validity, mailbox.name, mailbox.parentId, mailbox.role});
            mapped.insert(validity);
            m_localOnly.remove(validity);
        }

        const QHash<uint, QString> folders = localFolders();
        for (auto it = m_mailboxes.begin(); it != m_mailboxes.end();) {
            if (server.contains(it.key())) {
                ++it;
                continue;
            }
            const QString folder = folders.value(it->uidValidity);
            if (!folder.isEmpty() && folder != inbox && !m_store->deleteMailbox(folder, &storeError))
                warn(storeError);
            it = m_mailboxes.erase(it);
        }
        if (!m_hadState) {
            // Folders the server has not, such as a local Trash, stay on this computer
            for (auto it = folders.cbegin(); it != folders.cend(); ++it) {
                if (!mapped.contains(it.key()))
                    m_localOnly.insert(it.key());
            }
        }
        saveState();
        pullEmails();
    });
}

void SyncJob::pullEmails()
{
    m_folderByMailboxId.clear();
    const QHash<uint, QString> folders = localFolders();
    for (auto it = m_mailboxes.cbegin(); it != m_mailboxes.cend(); ++it) {
        const QString folder = folders.value(it->uidValidity);
        if (!folder.isEmpty())
            m_folderByMailboxId.insert(it.key(), folder);
    }
    m_toFetch.clear();
    m_toFetchSet.clear();
    m_destroyed.clear();
    m_serverIds.clear();
    m_newEmailState.clear();
    m_full = false;
    if (m_emailState.isEmpty())
        fullResync();
    else
        requestChanges();
}

void SyncJob::requestChanges()
{
    const QString since = m_newEmailState.isEmpty() ? m_emailState : m_newEmailState;
    m_client->call(QJsonArray{Client::method(QStringLiteral("Email/changes"),
                                             QJsonObject{{QStringLiteral("accountId"), m_accountId},
                                                         {QStringLiteral("sinceState"), since},
                                                         {QStringLiteral("maxChanges"), queryPageSize}},
                                             QStringLiteral("c"))},
                   [this](const QJsonArray &responses) {
        if (!m_running)
            return;
        QString error;
        const QJsonObject result = Client::response(responses, QStringLiteral("c"), &error);
        if (error == QLatin1String("cannotCalculateChanges")) {
            fullResync();
            return;
        }
        if (!error.isEmpty()) {
            fail(Failure::Protocol, tr("The JMAP server did not tell what changed: %1").arg(error));
            return;
        }
        for (const QString &key : {QStringLiteral("created"), QStringLiteral("updated")}) {
            for (const QJsonValue &id : result.value(key).toArray()) {
                if (!m_toFetchSet.contains(id.toString())) {
                    m_toFetchSet.insert(id.toString());
                    m_toFetch << id.toString();
                }
            }
        }
        for (const QJsonValue &id : result.value(QLatin1String("destroyed")).toArray())
            m_destroyed << id.toString();
        m_newEmailState = result.value(QLatin1String("newState")).toString();
        if (result.value(QLatin1String("hasMoreChanges")).toBool()) {
            requestChanges();
            return;
        }
        removeDestroyed();
    });
}

void SyncJob::fullResync()
{
    m_full = true;
    m_toFetch.clear();
    m_toFetchSet.clear();
    m_destroyed.clear();
    // The state comes first, so that changes made while the list is read are caught next time
    m_client->call(QJsonArray{Client::method(QStringLiteral("Email/get"),
                                             QJsonObject{{QStringLiteral("accountId"), m_accountId},
                                                         {QStringLiteral("ids"), QJsonArray()}},
                                             QStringLiteral("s"))},
                   [this](const QJsonArray &responses) {
        if (!m_running)
            return;
        QString error;
        const QJsonObject result = Client::response(responses, QStringLiteral("s"), &error);
        if (!error.isEmpty()) {
            fail(Failure::Protocol, tr("The JMAP server did not list its messages: %1").arg(error));
            return;
        }
        m_newEmailState = result.value(QLatin1String("state")).toString();
        queryPage(0);
    });
}

void SyncJob::queryPage(int position)
{
    m_client->call(QJsonArray{Client::method(QStringLiteral("Email/query"),
                                             QJsonObject{{QStringLiteral("accountId"), m_accountId},
                                                         {QStringLiteral("position"), position},
                                                         {QStringLiteral("limit"), queryPageSize},
                                                         {QStringLiteral("calculateTotal"), true}},
                                             QStringLiteral("q"))},
                   [this, position](const QJsonArray &responses) {
        if (!m_running)
            return;
        QString error;
        const QJsonObject result = Client::response(responses, QStringLiteral("q"), &error);
        if (!error.isEmpty()) {
            fail(Failure::Protocol, tr("The JMAP server did not list its messages: %1").arg(error));
            return;
        }
        const QJsonArray ids = result.value(QLatin1String("ids")).toArray();
        for (const QJsonValue &id : ids) {
            if (!m_toFetchSet.contains(id.toString())) {
                m_toFetchSet.insert(id.toString());
                m_toFetch << id.toString();
            }
        }
        const int next = position + int(ids.size());
        const qint64 total = result.value(QLatin1String("total")).toInteger(-1);
        if (!ids.isEmpty() && (total < 0 || next < total)) {
            queryPage(next);
            return;
        }
        m_serverIds = m_toFetchSet;
        removeDestroyed();
    });
}

void SyncJob::indexCopies()
{
    m_copies.clear();
    for (const QString &folder : m_store->mailboxNames()) {
        for (const LocalMail::MessageInfo &message : m_store->messages(folder)) {
            const QString id = emailId(message.origin);
            if (!id.isEmpty())
                m_copies[id].append(Copy{folder, message.uid, message.flags});
        }
    }
}

void SyncJob::removeDestroyed()
{
    indexCopies();
    QSet<QString> mappedFolders;
    for (const QString &folder : std::as_const(m_folderByMailboxId))
        mappedFolders.insert(folder);
    QStringList gone = m_destroyed;
    if (m_full) {
        for (auto it = m_copies.cbegin(); it != m_copies.cend(); ++it) {
            if (!m_serverIds.contains(it.key()))
                gone << it.key();
        }
        for (auto it = m_emails.cbegin(); it != m_emails.cend(); ++it) {
            if (!m_serverIds.contains(it.key()))
                gone << it.key();
        }
    }
    QString error;
    for (const QString &id : std::as_const(gone)) {
        m_emails.remove(id);
        QList<Copy> &copies = m_copies[id];
        for (auto copy = copies.begin(); copy != copies.end();) {
            if (!mappedFolders.contains(copy->folder)) {
                ++copy;
                continue;
            }
            if (!m_store->remove(copy->folder, {copy->uid}, &error)) {
                storageFailure(error);
                return;
            }
            copy = copies.erase(copy);
        }
    }
    m_fetchCursor = 0;
    fetchNextChunk();
}

void SyncJob::fetchNextChunk()
{
    if (m_fetchCursor >= m_toFetch.size()) {
        finish();
        return;
    }
    const QStringList chunk = m_toFetch.mid(m_fetchCursor, m_client->session().maxObjectsInGet);
    m_fetchCursor += int(chunk.size());
    const QJsonObject arguments{{QStringLiteral("accountId"), m_accountId},
                                {QStringLiteral("ids"), QJsonArray::fromStringList(chunk)},
                                {QStringLiteral("properties"),
                                 QJsonArray{QStringLiteral("id"), QStringLiteral("blobId"), QStringLiteral("mailboxIds"),
                                            QStringLiteral("keywords"), QStringLiteral("receivedAt")}}};
    m_client->call(QJsonArray{Client::method(QStringLiteral("Email/get"), arguments, QStringLiteral("f"))},
                   [this](const QJsonArray &responses) {
        if (!m_running)
            return;
        QString error;
        const QJsonObject result = Client::response(responses, QStringLiteral("f"), &error);
        if (!error.isEmpty()) {
            fail(Failure::Protocol, tr("The JMAP server did not send its messages: %1").arg(error));
            return;
        }
        for (const QJsonValue &email : result.value(QLatin1String("list")).toArray())
            m_applyQueue << email.toObject();
        applyNext();
    });
}

void SyncJob::applyNext()
{
    while (!m_applyQueue.isEmpty()) {
        const QJsonObject email = m_applyQueue.constFirst();
        const QString id = email.value(QLatin1String("id")).toString();
        const bool inMappedFolder = [&]() {
            const QJsonObject mailboxIds = email.value(QLatin1String("mailboxIds")).toObject();
            for (auto it = mailboxIds.begin(); it != mailboxIds.end(); ++it) {
                if (it.value().toBool() && m_folderByMailboxId.contains(it.key()))
                    return true;
            }
            return false;
        }();
        if (inMappedFolder && m_copies.value(id).isEmpty()) {
            m_client->download(email.value(QLatin1String("blobId")).toString(), [this, email](const QByteArray &content) {
                if (!m_running)
                    return;
                if (!applyEmail(email, content))
                    return;
                m_applyQueue.removeFirst();
                applyNext();
            });
            return;
        }
        if (!applyEmail(email, QByteArray()))
            return;
        m_applyQueue.removeFirst();
    }
    fetchNextChunk();
}

bool SyncJob::applyEmail(const QJsonObject &email, const QByteArray &content)
{
    const QString id = email.value(QLatin1String("id")).toString();
    const QStringList mailboxIds = setKeys(email.value(QLatin1String("mailboxIds")).toObject());
    const QStringList keywords = setKeys(email.value(QLatin1String("keywords")).toObject(), Qt::CaseInsensitive);
    const QDateTime received = QDateTime::fromString(email.value(QLatin1String("receivedAt")).toString(), Qt::ISODate);
    QStringList targets;
    for (const QString &mailbox : mailboxIds) {
        const QString folder = m_folderByMailboxId.value(mailbox);
        if (!folder.isEmpty())
            targets << folder;
    }
    QSet<QString> mappedFolders;
    for (const QString &folder : std::as_const(m_folderByMailboxId))
        mappedFolders.insert(folder);

    QString error;
    QList<Copy> &copies = m_copies[id];
    for (auto copy = copies.begin(); copy != copies.end();) {
        if (mappedFolders.contains(copy->folder) && !targets.contains(copy->folder)) {
            if (!m_store->remove(copy->folder, {copy->uid}, &error)) {
                storageFailure(error);
                return false;
            }
            copy = copies.erase(copy);
        } else {
            ++copy;
        }
    }
    const QStringList flags = keywordsToFlags(keywords);
    for (const QString &folder : std::as_const(targets)) {
        const bool present = std::any_of(copies.cbegin(), copies.cend(), [&folder](const Copy &copy) { return copy.folder == folder; });
        if (present)
            continue;
        if (!copies.isEmpty()) {
            const Copy source = copies.constFirst();
            const QList<uint> uids = m_store->copy(source.folder, {source.uid}, folder, &error);
            if (uids.isEmpty()) {
                storageFailure(error);
                return false;
            }
            copies.append(Copy{folder, uids.constFirst(), source.flags});
        } else {
            const uint uid = m_store->append(folder, content, flags, received, originPrefix + id, &error);
            if (!uid) {
                storageFailure(error);
                return false;
            }
            copies.append(Copy{folder, uid, flags});
            ++m_downloaded;
        }
    }
    for (Copy &copy : copies) {
        if (!targets.contains(copy.folder) || flagsToKeywords(copy.flags) == keywords)
            continue;
        QStringList wanted = flags;
        // Marked for removal here and not yet removed: that stays a local decision
        for (const QString &flag : std::as_const(copy.flags)) {
            if (flag.compare(QLatin1String("\\Deleted"), Qt::CaseInsensitive) == 0)
                wanted << flag;
        }
        m_store->setFlags(copy.folder, copy.uid, wanted);
        copy.flags = wanted;
    }
    QStringList agreed;
    for (const QString &mailbox : mailboxIds) {
        if (m_folderByMailboxId.contains(mailbox))
            agreed << mailbox;
    }
    // A message only in mailboxes without a local folder is left alone, rather than removed on the next visit
    if (agreed.isEmpty())
        m_emails.remove(id);
    else
        m_emails.insert(id, EmailState{keywords, agreed});
    return true;
}

void SyncJob::finish()
{
    m_emailState = m_newEmailState;
    m_newEmailState.clear();
    m_copies.clear();
    m_running = false;
    if (!saveState()) {
        emit failed(Failure::Storage, tr("The synchronization state could not be saved."));
        return;
    }
    emit succeeded(m_downloaded);
}

}
