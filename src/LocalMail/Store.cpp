/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "LocalMail/Store.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#endif

#include "Common/PrivateFiles.h"
#include "LocalMail/Mime.h"

namespace LocalMail {

namespace {

QHash<QString, std::weak_ptr<Store>> &openStores()
{
    static QHash<QString, std::weak_ptr<Store>> stores;
    return stores;
}

QStringList normalizedFlags(const QStringList &flags)
{
    QStringList result;
    for (const QString &flag : flags) {
        if (flag.isEmpty() || flag.compare(QLatin1String("\\Recent"), Qt::CaseInsensitive) == 0)
            continue;
        bool known = false;
        for (const QString &existing : std::as_const(result))
            known = known || existing.compare(flag, Qt::CaseInsensitive) == 0;
        if (!known)
            result << flag;
    }
    return result;
}

void syncFolder(const QString &path)
{
#ifdef Q_OS_UNIX
    const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY);
    if (fd >= 0) {
        ::fsync(fd);
        ::close(fd);
    }
#else
    Q_UNUSED(path);
#endif
}

}

QStringList Store::defaultMailboxes()
{
    return {QStringLiteral("INBOX"), QStringLiteral("Sent"), QStringLiteral("Drafts"), QStringLiteral("Trash")};
}

std::shared_ptr<Store> Store::open(const QString &folder, QString *error)
{
    const QString key = QDir::cleanPath(QDir(folder).absolutePath());
    if (auto existing = openStores().value(key).lock())
        return existing;
    std::shared_ptr<Store> store(new Store(key));
    if (!store->initialize(error))
        return {};
    openStores().insert(key, store);
    return store;
}

Store::Store(const QString &folder)
    : m_folder(folder)
    , m_connection(QStringLiteral("LocalMail-") + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

Store::~Store()
{
    openStores().remove(m_folder);
    {
        QSqlDatabase db = QSqlDatabase::database(m_connection, false);
        if (db.isValid())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_connection);
}

QString Store::folder() const
{
    return m_folder;
}

bool Store::exec(const QString &statement, const QVariantList &values, QString *error)
{
    QSqlQuery query(QSqlDatabase::database(m_connection));
    if (!query.prepare(statement)) {
        if (error)
            *error = query.lastError().text();
        return false;
    }
    for (const QVariant &value : values)
        query.addBindValue(value);
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        return false;
    }
    return true;
}

bool Store::initialize(QString *error)
{
    if (!Common::PrivateFiles::makeFolder(m_folder, error) || !Common::PrivateFiles::makeFolder(messageFilePath(QString()), error))
        return false;
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
    const QString path = m_folder + QLatin1String("/mail.sqlite");
    db.setDatabaseName(path);
    if (!db.open()) {
        if (error)
            *error = db.lastError().text();
        return false;
    }
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
    const QStringList schema = {
        QStringLiteral("PRAGMA synchronous = FULL"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS mailboxes (name TEXT PRIMARY KEY, uidvalidity INTEGER NOT NULL, "
                       "uidnext INTEGER NOT NULL, subscribed INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS messages (mailbox TEXT NOT NULL, uid INTEGER NOT NULL, flags TEXT, "
                       "internaldate INTEGER, size INTEGER, origin TEXT, file TEXT NOT NULL, PRIMARY KEY (mailbox, uid))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS origins (origin TEXT PRIMARY KEY, stored INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT)"),
    };
    for (const QString &statement : schema) {
        if (!exec(statement, {}, error))
            return false;
    }

    QSqlQuery query(db);
    query.exec(QStringLiteral("SELECT name, uidvalidity, uidnext, subscribed FROM mailboxes"));
    while (query.next()) {
        MailboxInfo info;
        info.name = query.value(0).toString();
        info.uidValidity = query.value(1).toUInt();
        info.uidNext = query.value(2).toUInt();
        info.subscribed = query.value(3).toBool();
        m_mailboxes.insert(info.name, info);
        m_messages.insert(info.name, {});
    }
    query.exec(QStringLiteral("SELECT mailbox, uid, flags, internaldate, size, origin, file FROM messages ORDER BY mailbox, uid"));
    while (query.next()) {
        MessageInfo message;
        message.uid = query.value(1).toUInt();
        message.flags = query.value(2).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        message.internalDate = QDateTime::fromMSecsSinceEpoch(query.value(3).toLongLong(), QTimeZone::UTC);
        message.size = query.value(4).toLongLong();
        message.origin = query.value(5).toString();
        message.file = query.value(6).toString();
        m_messages[query.value(0).toString()].append(message);
    }
    query.exec(QStringLiteral("SELECT origin, stored FROM origins"));
    while (query.next())
        m_origins.insert(query.value(0).toString(), QDateTime::fromMSecsSinceEpoch(query.value(1).toLongLong(), QTimeZone::UTC));

    for (const QString &name : defaultMailboxes()) {
        if (!m_mailboxes.contains(name) && !createMailbox(name, error))
            return false;
    }
    return true;
}

QStringList Store::mailboxNames() const
{
    QStringList names = m_mailboxes.keys();
    names.removeAll(QStringLiteral("INBOX"));
    names.sort(Qt::CaseInsensitive);
    if (m_mailboxes.contains(QStringLiteral("INBOX")))
        names.prepend(QStringLiteral("INBOX"));
    return names;
}

bool Store::hasMailbox(const QString &name) const
{
    return m_mailboxes.contains(name);
}

MailboxInfo Store::mailbox(const QString &name) const
{
    return m_mailboxes.value(name);
}

bool Store::createMailbox(const QString &name, QString *error)
{
    if (name.isEmpty() || m_mailboxes.contains(name)) {
        if (error)
            *error = tr("The folder already exists.");
        return false;
    }
    // Each new folder gets a UIDVALIDITY never used before, even within one second
    QSqlQuery query(QSqlDatabase::database(m_connection));
    query.exec(QStringLiteral("SELECT value FROM meta WHERE key = 'lastuidvalidity'"));
    const uint last = query.next() ? query.value(0).toUInt() : 0;
    const uint uidValidity = qMax(static_cast<uint>(QDateTime::currentSecsSinceEpoch()), last + 1);
    if (!exec(QStringLiteral("INSERT OR REPLACE INTO meta (key, value) VALUES ('lastuidvalidity', ?)"), {uidValidity}, error)
        || !exec(QStringLiteral("INSERT INTO mailboxes (name, uidvalidity, uidnext, subscribed) VALUES (?, ?, 1, 1)"),
                 {name, uidValidity}, error))
        return false;
    m_mailboxes.insert(name, MailboxInfo{name, uidValidity, 1, true});
    m_messages.insert(name, {});
    emit changed(name);
    return true;
}

bool Store::deleteMailbox(const QString &name, QString *error)
{
    if (name == QLatin1String("INBOX") || !m_mailboxes.contains(name)) {
        if (error)
            *error = name == QLatin1String("INBOX") ? tr("INBOX cannot be removed.") : tr("There is no such folder.");
        return false;
    }
    QList<uint> uids;
    for (const MessageInfo &message : m_messages.value(name))
        uids << message.uid;
    if (!remove(name, uids, error) || !exec(QStringLiteral("DELETE FROM mailboxes WHERE name = ?"), {name}, error))
        return false;
    m_mailboxes.remove(name);
    m_messages.remove(name);
    emit changed(name);
    return true;
}

bool Store::renameMailbox(const QString &from, const QString &to, QString *error)
{
    if (!m_mailboxes.contains(from) || m_mailboxes.contains(to) || to.isEmpty() || from == QLatin1String("INBOX")) {
        if (error)
            *error = tr("The folder cannot be renamed to that name.");
        return false;
    }
    QStringList affected;
    for (const QString &name : m_mailboxes.keys()) {
        if (name == from || name.startsWith(from + QLatin1Char('/')))
            affected << name;
    }
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    db.transaction();
    for (const QString &name : std::as_const(affected)) {
        const QString renamed = to + name.mid(from.size());
        if (!exec(QStringLiteral("UPDATE mailboxes SET name = ? WHERE name = ?"), {renamed, name}, error)
            || !exec(QStringLiteral("UPDATE messages SET mailbox = ? WHERE mailbox = ?"), {renamed, name}, error)) {
            db.rollback();
            return false;
        }
    }
    db.commit();
    for (const QString &name : std::as_const(affected)) {
        const QString renamed = to + name.mid(from.size());
        MailboxInfo info = m_mailboxes.take(name);
        info.name = renamed;
        m_mailboxes.insert(renamed, info);
        m_messages.insert(renamed, m_messages.take(name));
        emit changed(name);
        emit changed(renamed);
    }
    return true;
}

bool Store::setSubscribed(const QString &name, bool subscribed)
{
    if (!m_mailboxes.contains(name)
        || !exec(QStringLiteral("UPDATE mailboxes SET subscribed = ? WHERE name = ?"), {subscribed ? 1 : 0, name}))
        return false;
    m_mailboxes[name].subscribed = subscribed;
    return true;
}

QList<MessageInfo> Store::messages(const QString &name) const
{
    return m_messages.value(name);
}

QString Store::messageFilePath(const QString &file) const
{
    return m_folder + QLatin1String("/messages/") + file;
}

QByteArray Store::content(const QString &mailbox, uint uid) const
{
    for (const MessageInfo &message : m_messages.value(mailbox)) {
        if (message.uid == uid) {
            QFile file(messageFilePath(message.file));
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        }
    }
    return {};
}

QString Store::writeMessage(const QByteArray &content, QString *error)
{
    const QString name = QUuid::createUuid().toString(QUuid::WithoutBraces) + QLatin1String(".eml");
    QSaveFile file(messageFilePath(name));
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return {};
    }
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    if (file.write(content) != content.size() || !file.flush()) {
        if (error)
            *error = file.errorString();
        file.cancelWriting();
        return {};
    }
#ifdef Q_OS_UNIX
    ::fsync(file.handle());
#endif
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return {};
    }
    syncFolder(messageFilePath(QString()));
    return name;
}

uint Store::append(const QString &mailbox, const QByteArray &raw, const QStringList &flags, const QDateTime &internalDate,
                   const QString &origin, QString *error)
{
    if (!m_mailboxes.contains(mailbox)) {
        if (error)
            *error = tr("There is no such folder.");
        return 0;
    }
    const QByteArray content = Mime::canonicalLineEnds(raw);
    const QString file = writeMessage(content, error);
    if (file.isEmpty())
        return 0;

    MailboxInfo &info = m_mailboxes[mailbox];
    MessageInfo message;
    message.uid = info.uidNext;
    message.flags = normalizedFlags(flags);
    message.internalDate = (internalDate.isValid() ? internalDate : QDateTime::currentDateTimeUtc()).toUTC();
    message.size = content.size();
    message.origin = origin;
    message.file = file;
    const QDateTime now = QDateTime::currentDateTimeUtc();

    QSqlDatabase db = QSqlDatabase::database(m_connection);
    db.transaction();
    bool ok = exec(QStringLiteral("INSERT INTO messages (mailbox, uid, flags, internaldate, size, origin, file) "
                                  "VALUES (?, ?, ?, ?, ?, ?, ?)"),
                   {mailbox, message.uid, message.flags.join(QLatin1Char(' ')), message.internalDate.toMSecsSinceEpoch(),
                    message.size, origin, file},
                   error)
        && exec(QStringLiteral("UPDATE mailboxes SET uidnext = ? WHERE name = ?"), {message.uid + 1, mailbox}, error);
    if (ok && !origin.isEmpty())
        ok = exec(QStringLiteral("INSERT OR IGNORE INTO origins (origin, stored) VALUES (?, ?)"),
                  {origin, now.toMSecsSinceEpoch()}, error);
    if (!ok || !db.commit()) {
        db.rollback();
        QFile::remove(messageFilePath(file));
        return 0;
    }
    info.uidNext = message.uid + 1;
    m_messages[mailbox].append(message);
    if (!origin.isEmpty() && !m_origins.contains(origin))
        m_origins.insert(origin, now);
    emit changed(mailbox);
    return message.uid;
}

bool Store::setFlags(const QString &mailbox, uint uid, const QStringList &flags)
{
    auto it = m_messages.find(mailbox);
    if (it == m_messages.end())
        return false;
    for (MessageInfo &message : *it) {
        if (message.uid != uid)
            continue;
        const QStringList normalized = normalizedFlags(flags);
        if (normalized == message.flags)
            return true;
        if (!exec(QStringLiteral("UPDATE messages SET flags = ? WHERE mailbox = ? AND uid = ?"),
                  {normalized.join(QLatin1Char(' ')), mailbox, uid}))
            return false;
        message.flags = normalized;
        emit changed(mailbox);
        return true;
    }
    return false;
}

bool Store::remove(const QString &mailbox, const QList<uint> &uids, QString *error)
{
    auto it = m_messages.find(mailbox);
    if (it == m_messages.end())
        return false;
    QStringList files;
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    db.transaction();
    for (const MessageInfo &message : std::as_const(*it)) {
        if (!uids.contains(message.uid))
            continue;
        if (!exec(QStringLiteral("DELETE FROM messages WHERE mailbox = ? AND uid = ?"), {mailbox, message.uid}, error)) {
            db.rollback();
            return false;
        }
        files << message.file;
    }
    if (!db.commit()) {
        db.rollback();
        return false;
    }
    it->erase(std::remove_if(it->begin(), it->end(), [&uids](const MessageInfo &message) { return uids.contains(message.uid); }),
              it->end());
    for (const QString &file : std::as_const(files))
        QFile::remove(messageFilePath(file));
    if (!files.isEmpty())
        emit changed(mailbox);
    return true;
}

QList<uint> Store::copy(const QString &from, const QList<uint> &uids, const QString &to, QString *error)
{
    if (!m_mailboxes.contains(from) || !m_mailboxes.contains(to)) {
        if (error)
            *error = tr("There is no such folder.");
        return {};
    }
    QList<uint> result;
    const QList<MessageInfo> source = m_messages.value(from);
    for (const uint uid : uids) {
        for (const MessageInfo &message : source) {
            if (message.uid != uid)
                continue;
            const uint copied = append(to, content(from, uid), message.flags, message.internalDate, message.origin, error);
            if (!copied) {
                remove(to, result);
                return {};
            }
            result << copied;
        }
    }
    return result;
}

bool Store::knowsOrigin(const QString &origin) const
{
    return m_origins.contains(origin);
}

QDateTime Store::originStored(const QString &origin) const
{
    return m_origins.value(origin);
}

QStringList Store::origins() const
{
    return m_origins.keys();
}

void Store::keepOnlyOrigins(const QStringList &kept)
{
    const QSet<QString> keep(kept.begin(), kept.end());
    const QStringList known = m_origins.keys();
    for (const QString &origin : known) {
        if (keep.contains(origin))
            continue;
        if (exec(QStringLiteral("DELETE FROM origins WHERE origin = ?"), {origin}))
            m_origins.remove(origin);
    }
}

}
