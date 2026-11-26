/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "SearchIndex.h"

#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

namespace Imap
{
namespace Mailbox
{

namespace
{

const int indexVersion = 2;

/** Text kept of one message; longer text is cut, as it rarely adds anything to a search */
const int maximumBody = 512 * 1024;

const QString textColumns[] = {
    QStringLiteral("subject"), QStringLiteral("sender"), QStringLiteral("recipient_to"), QStringLiteral("recipient_cc"),
    QStringLiteral("recipient_bcc"), QStringLiteral("body"), QStringLiteral("attachments"),
};

/** @short Groups writes; inside the cache's own delayed transaction it only nests, otherwise it commits */
class Savepoint
{
public:
    explicit Savepoint(QSqlDatabase db)
        : m_db(db)
    {
        QSqlQuery q(m_db);
        m_active = q.exec(QStringLiteral("SAVEPOINT search_index"));
    }
    ~Savepoint()
    {
        if (!m_active)
            return;
        QSqlQuery q(m_db);
        q.exec(QStringLiteral("ROLLBACK TO search_index"));
        q.exec(QStringLiteral("RELEASE search_index"));
    }
    bool isActive() const { return m_active; }
    bool release()
    {
        QSqlQuery q(m_db);
        m_active = false;
        return q.exec(QStringLiteral("RELEASE search_index"));
    }

private:
    QSqlDatabase m_db;
    bool m_active = false;
};

/** @short Flags as stored: lower case, each with a space on both sides, so that one can be found with instr() */
QString flagText(const QStringList &flags)
{
    QStringList lower;
    for (const QString &flag : flags)
        lower << flag.toLower();
    lower.sort();
    lower.removeDuplicates();
    return lower.isEmpty() ? QStringLiteral(" ") : QLatin1Char(' ') + lower.join(QLatin1Char(' ')) + QLatin1Char(' ');
}

/** @short A stored update time; 0 means the folder was never indexed */
QDateTime updatedAt(const QVariant &value)
{
    const qint64 seconds = value.toLongLong();
    return seconds > 0 ? QDateTime::fromSecsSinceEpoch(seconds) : QDateTime();
}

QStringList flagList(const QString &text)
{
    return text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

QVariant day(const QDate &date)
{
    return date.isValid() ? QVariant(date.toJulianDay()) : QVariant(QMetaType(QMetaType::LongLong));
}

/** @short A date of an IMAP SEARCH key, such as 1-Feb-2026 */
QDate imapDate(const QString &text)
{
    static const QStringList months = {
        QStringLiteral("jan"), QStringLiteral("feb"), QStringLiteral("mar"), QStringLiteral("apr"),
        QStringLiteral("may"), QStringLiteral("jun"), QStringLiteral("jul"), QStringLiteral("aug"),
        QStringLiteral("sep"), QStringLiteral("oct"), QStringLiteral("nov"), QStringLiteral("dec"),
    };
    const QStringList parts = text.split(QLatin1Char('-'));
    if (parts.size() != 3)
        return QDate();
    bool dayOk = false;
    bool yearOk = false;
    const int dayNumber = parts[0].toInt(&dayOk);
    const int month = int(months.indexOf(parts[1].toLower())) + 1;
    const int year = parts[2].toInt(&yearOk);
    if (!dayOk || !yearOk || month < 1)
        return QDate();
    return QDate(year, month, dayNumber);
}

/** @short Split a search typed as raw IMAP into keys and values */
QStringList tokenize(const QString &text)
{
    QStringList tokens;
    int i = 0;
    while (i < text.size()) {
        const QChar c = text[i];
        if (c.isSpace()) {
            ++i;
        } else if (c == QLatin1Char('(') || c == QLatin1Char(')')) {
            tokens << QString(c);
            ++i;
        } else if (c == QLatin1Char('"')) {
            QString value;
            ++i;
            while (i < text.size() && text[i] != QLatin1Char('"')) {
                if (text[i] == QLatin1Char('\\') && i + 1 < text.size())
                    ++i;
                value += text[i++];
            }
            ++i;
            tokens << value;
        } else {
            const int start = i;
            while (i < text.size() && !text[i].isSpace() && text[i] != QLatin1Char('(') && text[i] != QLatin1Char(')'))
                ++i;
            tokens << text.mid(start, i - start);
        }
    }
    return tokens;
}

struct Translator {
    QStringList tokens;
    int position = 0;
    QVariantList values;
    QString problem;
    bool usesText = false;

    bool argument(const QString &key, QString *value)
    {
        if (position >= tokens.size()) {
            problem = SearchIndex::tr("The search key %1 has no value").arg(key);
            return false;
        }
        *value = tokens.at(position++);
        return true;
    }

    QString like(const QString &column, const QString &value)
    {
        QString escaped = value;
        escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
        escaped.replace(QLatin1Char('%'), QLatin1String("\\%"));
        escaped.replace(QLatin1Char('_'), QLatin1String("\\_"));
        values << QVariant(QLatin1Char('%') + escaped + QLatin1Char('%'));
        return QStringLiteral("coalesce(%1, '') LIKE ? ESCAPE '\\'").arg(column);
    }

    /** @short Messages with @arg value anywhere in one of @arg columns of the text, regardless of case */
    QString text(const QStringList &columns, const QString &value)
    {
        if (value.isEmpty())
            return QStringLiteral("1");
        if (value.toUcs4().size() >= 3) {
            // The trigram tokenizer finds any part of the text which is at least three characters long
            QString phrase = value;
            phrase.replace(QLatin1Char('"'), QLatin1String("\"\""));
            const QString filter = columns.size() == 1 ? columns.constFirst()
                                                       : QLatin1Char('{') + columns.join(QLatin1Char(' ')) + QLatin1Char('}');
            values << QVariant(filter + QLatin1String(" : \"") + phrase + QLatin1Char('"'));
            return QStringLiteral("id IN (SELECT rowid FROM search_text WHERE search_text MATCH ?)");
        }
        // Shorter text has no trigram, so the table is read through
        QStringList alternatives;
        for (const QString &column : columns)
            alternatives << like(column, value);
        return QStringLiteral("id IN (SELECT rowid FROM search_text WHERE %1)").arg(alternatives.join(QLatin1String(" OR ")));
    }

    QString flag(const QString &name, bool present)
    {
        values << QVariant(QLatin1Char(' ') + name.toLower() + QLatin1Char(' '));
        return present ? QStringLiteral("instr(flags, ?) > 0") : QStringLiteral("instr(flags, ?) = 0");
    }

    QString date(const QString &key, const QString &column, const QString &comparison)
    {
        QString value;
        if (!argument(key, &value))
            return QString();
        const QDate parsed = imapDate(value);
        if (!parsed.isValid()) {
            problem = SearchIndex::tr("\u201C%1\u201D is not a date").arg(value);
            return QString();
        }
        values << QVariant(parsed.toJulianDay());
        return column + QLatin1Char(' ') + comparison + QLatin1String(" ?");
    }

    QString size(const QString &key, const QString &comparison)
    {
        QString value;
        if (!argument(key, &value))
            return QString();
        bool ok = false;
        const qulonglong bytes = value.toULongLong(&ok);
        if (!ok) {
            problem = SearchIndex::tr("\u201C%1\u201D is not a size").arg(value);
            return QString();
        }
        values << QVariant(bytes);
        return QLatin1String("size ") + comparison + QLatin1String(" ?");
    }

    QString header(const QString &name, const QString &value)
    {
        const QString lower = name.toLower();
        if (lower == QLatin1String("subject"))
            return text({QStringLiteral("subject")}, value);
        if (lower == QLatin1String("from"))
            return text({QStringLiteral("sender")}, value);
        if (lower == QLatin1String("to"))
            return text({QStringLiteral("recipient_to")}, value);
        if (lower == QLatin1String("cc"))
            return text({QStringLiteral("recipient_cc")}, value);
        if (lower == QLatin1String("bcc"))
            return text({QStringLiteral("recipient_bcc")}, value);
        static const QHash<QString, QString> columns = {
            {QStringLiteral("list-id"), QStringLiteral("list_id")},
            {QStringLiteral("list-post"), QStringLiteral("list_post")},
            {QStringLiteral("message-id"), QStringLiteral("message_id")},
            {QStringLiteral("in-reply-to"), QStringLiteral("in_reply_to")},
            {QStringLiteral("references"), QStringLiteral("refs")},
            {QStringLiteral("content-type"), QStringLiteral("content_type")},
        };
        const auto column = columns.constFind(lower);
        if (column == columns.constEnd()) {
            problem = SearchIndex::tr("The local index does not keep the %1 header").arg(name);
            return QString();
        }
        if (value.isEmpty()) {
            // An empty value asks whether the header is there at all
            return QStringLiteral("coalesce(%1, '') <> ''").arg(*column);
        }
        return like(*column, value);
    }

    /** @short The condition of the key at the current position */
    QString key()
    {
        if (position >= tokens.size()) {
            problem = SearchIndex::tr("The search ends too early");
            return QString();
        }
        const QString name = tokens.at(position++);
        const QString key = name.toUpper();
        QString value;
        if (key == QLatin1String("ALL"))
            return QStringLiteral("1");
        if (key == QLatin1String("FUZZY"))
            return this->key();
        if (key == QLatin1String("NOT")) {
            const QString inner = this->key();
            return inner.isEmpty() ? QString() : QLatin1String("NOT (") + inner + QLatin1Char(')');
        }
        if (key == QLatin1String("OR")) {
            const QString first = this->key();
            if (first.isEmpty())
                return QString();
            const QString second = this->key();
            if (second.isEmpty())
                return QString();
            return QLatin1String("((") + first + QLatin1String(") OR (") + second + QLatin1String("))");
        }
        if (key == QLatin1String("(")) {
            QStringList all;
            while (position < tokens.size() && tokens.at(position) != QLatin1String(")")) {
                const QString inner = this->key();
                if (inner.isEmpty())
                    return QString();
                all << QLatin1Char('(') + inner + QLatin1Char(')');
            }
            if (position >= tokens.size()) {
                problem = SearchIndex::tr("A parenthesis is not closed");
                return QString();
            }
            ++position;
            return all.isEmpty() ? QStringLiteral("1") : all.join(QLatin1String(" AND "));
        }
        static const QHash<QString, QStringList> textKeys = {
            {QStringLiteral("SUBJECT"), {QStringLiteral("subject")}},
            {QStringLiteral("FROM"), {QStringLiteral("sender")}},
            {QStringLiteral("TO"), {QStringLiteral("recipient_to")}},
            {QStringLiteral("CC"), {QStringLiteral("recipient_cc")}},
            {QStringLiteral("BCC"), {QStringLiteral("recipient_bcc")}},
            {QStringLiteral("BODY"), {QStringLiteral("body")}},
        };
        if (const auto columns = textKeys.constFind(key); columns != textKeys.constEnd()) {
            if (!argument(key, &value))
                return QString();
            if (key == QLatin1String("BODY"))
                usesText = true;
            return text(*columns, value);
        }
        if (key == QLatin1String("TEXT")) {
            if (!argument(key, &value))
                return QString();
            usesText = true;
            QStringList all;
            for (const QString &column : textColumns)
                all << column;
            return text(all, value);
        }
        if (key == QLatin1String("HEADER")) {
            QString headerName;
            if (!argument(key, &headerName) || !argument(key, &value))
                return QString();
            return header(headerName, value);
        }
        static const QHash<QString, QPair<QString, bool>> flagKeys = {
            {QStringLiteral("SEEN"), {QStringLiteral("\\Seen"), true}},
            {QStringLiteral("UNSEEN"), {QStringLiteral("\\Seen"), false}},
            {QStringLiteral("FLAGGED"), {QStringLiteral("\\Flagged"), true}},
            {QStringLiteral("UNFLAGGED"), {QStringLiteral("\\Flagged"), false}},
            {QStringLiteral("ANSWERED"), {QStringLiteral("\\Answered"), true}},
            {QStringLiteral("UNANSWERED"), {QStringLiteral("\\Answered"), false}},
            {QStringLiteral("DELETED"), {QStringLiteral("\\Deleted"), true}},
            {QStringLiteral("UNDELETED"), {QStringLiteral("\\Deleted"), false}},
            {QStringLiteral("DRAFT"), {QStringLiteral("\\Draft"), true}},
            {QStringLiteral("UNDRAFT"), {QStringLiteral("\\Draft"), false}},
        };
        if (const auto flag = flagKeys.constFind(key); flag != flagKeys.constEnd())
            return this->flag(flag->first, flag->second);
        if (key == QLatin1String("KEYWORD") || key == QLatin1String("UNKEYWORD")) {
            if (!argument(key, &value))
                return QString();
            return flag(value, key == QLatin1String("KEYWORD"));
        }
        // Without a Date header, the time the message arrived stands in, as on most servers
        const QString sent = QStringLiteral("coalesce(sent, received)");
        if (key == QLatin1String("SENTSINCE"))
            return date(key, sent, QStringLiteral(">="));
        if (key == QLatin1String("SENTBEFORE"))
            return date(key, sent, QStringLiteral("<"));
        if (key == QLatin1String("SENTON"))
            return date(key, sent, QStringLiteral("="));
        if (key == QLatin1String("SINCE"))
            return date(key, QStringLiteral("received"), QStringLiteral(">="));
        if (key == QLatin1String("BEFORE"))
            return date(key, QStringLiteral("received"), QStringLiteral("<"));
        if (key == QLatin1String("ON"))
            return date(key, QStringLiteral("received"), QStringLiteral("="));
        if (key == QLatin1String("LARGER"))
            return size(key, QStringLiteral(">"));
        if (key == QLatin1String("SMALLER"))
            return size(key, QStringLiteral("<"));
        problem = SearchIndex::tr("The local index cannot search for %1").arg(name);
        return QString();
    }
};

}

SearchIndex::SearchIndex(const QString &connectionName)
    : m_connection(connectionName)
{
}

QSqlDatabase SearchIndex::database() const
{
    return QSqlDatabase::database(m_connection, false);
}

bool SearchIndex::fail(const QString &what, const QString &detail) const
{
    // The detail names the failing statement, never the indexed text, which is only ever bound
    m_error = detail.isEmpty() ? what : tr("%1: %2").arg(what, detail);
    return false;
}

bool SearchIndex::supportsFullText(const QSqlDatabase &db, QString *problem)
{
    if (!db.isValid() || !db.isOpen()) {
        if (problem)
            *problem = tr("The offline cache is not open");
        return false;
    }
    QSqlQuery q(db);
    // A temporary table disappears with the connection and is never written to the cache file
    const bool ok = q.exec(QStringLiteral("CREATE VIRTUAL TABLE temp.search_index_probe USING fts5(t, tokenize='trigram')"));
    if (ok) {
        q.exec(QStringLiteral("DROP TABLE temp.search_index_probe"));
        return true;
    }
    if (problem)
        *problem = tr("The SQLite library does not offer full-text search with FTS5 and its trigram tokenizer (%1)")
                       .arg(q.lastError().text());
    return false;
}

bool SearchIndex::open()
{
    m_open = false;
    QSqlDatabase db = database();
    QString problem;
    if (!supportsFullText(db, &problem))
        return fail(problem, QString());
    Savepoint savepoint(db);
    QSqlQuery q(db);
    const QStringList statements = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS search_meta (key TEXT PRIMARY KEY, value TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS search_folders (mailbox TEXT PRIMARY KEY, uidvalidity INTEGER, "
                       "messages INTEGER, indexed INTEGER, with_text INTEGER, updated INTEGER)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS search_docs (id INTEGER PRIMARY KEY, mailbox TEXT NOT NULL, "
                       "uid INTEGER NOT NULL, sent INTEGER, received INTEGER, size INTEGER, flags TEXT, "
                       "content_type TEXT, has_attachment INTEGER, message_id TEXT, in_reply_to TEXT, refs TEXT, "
                       "list_id TEXT, list_post TEXT, has_text INTEGER, text_parts TEXT, "
                       "decrypted INTEGER NOT NULL DEFAULT 0, UNIQUE (mailbox, uid))"),
        QStringLiteral("CREATE VIRTUAL TABLE IF NOT EXISTS search_text USING fts5(subject, sender, recipient_to, "
                       "recipient_cc, recipient_bcc, body, attachments, tokenize='trigram')"),
    };
    for (const QString &statement : statements) {
        if (!q.exec(statement))
            return fail(tr("Cannot create the search index"), q.lastError().text());
    }
    q.prepare(QStringLiteral("SELECT value FROM search_meta WHERE key = 'version'"));
    if (!q.exec())
        return fail(tr("Cannot read the search index"), q.lastError().text());
    if (!q.next()) {
        q.prepare(QStringLiteral("INSERT INTO search_meta (key, value) VALUES ('version', ?)"));
        q.addBindValue(indexVersion);
        if (!q.exec())
            return fail(tr("Cannot create the search index"), q.lastError().text());
    } else if (q.value(0).toInt() != indexVersion) {
        // A different layout is rebuilt rather than read wrongly
        savepoint.release();
        if (!drop(m_connection, &problem))
            return fail(problem, QString());
        return open();
    }
    if (!savepoint.release())
        return fail(tr("Cannot create the search index"), QString());
    m_open = true;
    m_error.clear();
    return true;
}

bool SearchIndex::isOpen() const
{
    return m_open;
}

QString SearchIndex::errorString() const
{
    return m_error;
}

bool SearchIndex::drop(const QString &connectionName, QString *problem)
{
    QSqlDatabase db = QSqlDatabase::database(connectionName, false);
    if (!db.isValid() || !db.isOpen()) {
        if (problem)
            *problem = tr("The offline cache is not open");
        return false;
    }
    const QStringList tables = {QStringLiteral("search_text"), QStringLiteral("search_docs"),
                                QStringLiteral("search_folders"), QStringLiteral("search_meta")};
    const QStringList existing = db.tables();
    {
        // Emptying the tables always works, and removes the indexed text right away
        Savepoint savepoint(db);
        QSqlQuery q(db);
        for (const QString &table : tables) {
            if (existing.contains(table) && !q.exec(QStringLiteral("DELETE FROM %1").arg(table))) {
                if (problem)
                    *problem = tr("Cannot remove the search index: %1").arg(q.lastError().text());
                return false;
            }
        }
        if (!savepoint.release()) {
            if (problem)
                *problem = tr("Cannot remove the search index: %1").arg(db.lastError().text());
            return false;
        }
    }
    // SQLite refuses to drop a table while a statement on the connection is still reading, so callers finish the
    // cache's queries first; should a drop still fail, the empty tables stay until the next time, which is harmless
    QSqlQuery q(db);
    for (const QString &table : tables) {
        if (existing.contains(table) && !q.exec(QStringLiteral("DROP TABLE IF EXISTS %1").arg(table)))
            qWarning() << "Search index: cannot drop" << table << q.lastError().text();
    }
    return true;
}

bool SearchIndex::clear()
{
    QSqlDatabase db = database();
    Savepoint savepoint(db);
    QSqlQuery q(db);
    for (const QString &table : {QStringLiteral("search_text"), QStringLiteral("search_docs"), QStringLiteral("search_folders")}) {
        if (!q.exec(QStringLiteral("DELETE FROM %1").arg(table)))
            return fail(tr("Cannot empty the search index"), q.lastError().text());
    }
    if (!savepoint.release())
        return fail(tr("Cannot empty the search index"), db.lastError().text());
    return true;
}

QList<SearchIndex::Folder> SearchIndex::folders() const
{
    QList<Folder> result;
    QSqlQuery q(database());
    if (!q.exec(QStringLiteral("SELECT mailbox, uidvalidity, messages, indexed, with_text, updated FROM search_folders "
                               "ORDER BY mailbox"))) {
        fail(tr("Cannot read the search index"), q.lastError().text());
        return result;
    }
    while (q.next()) {
        Folder folder;
        folder.mailbox = q.value(0).toString();
        folder.uidValidity = q.value(1).toUInt();
        folder.messages = q.value(2).toInt();
        folder.indexed = q.value(3).toInt();
        folder.withText = q.value(4).toInt();
        folder.updated = updatedAt(q.value(5));
        result << folder;
    }
    return result;
}

SearchIndex::Folder SearchIndex::folder(const QString &mailbox) const
{
    QSqlQuery q(database());
    q.prepare(QStringLiteral("SELECT uidvalidity, messages, indexed, with_text, updated FROM search_folders "
                             "WHERE mailbox = ?"));
    q.addBindValue(mailbox);
    Folder folder;
    folder.mailbox = mailbox;
    if (!q.exec()) {
        fail(tr("Cannot read the search index"), q.lastError().text());
        return folder;
    }
    if (q.next()) {
        folder.uidValidity = q.value(0).toUInt();
        folder.messages = q.value(1).toInt();
        folder.indexed = q.value(2).toInt();
        folder.withText = q.value(3).toInt();
        folder.updated = updatedAt(q.value(4));
    }
    return folder;
}

bool SearchIndex::setFolder(const Folder &folder)
{
    QSqlQuery q(database());
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO search_folders (mailbox, uidvalidity, messages, indexed, with_text, "
                             "updated) VALUES (?, ?, ?, ?, ?, ?)"));
    q.addBindValue(folder.mailbox);
    q.addBindValue(folder.uidValidity);
    q.addBindValue(folder.messages);
    q.addBindValue(folder.indexed);
    q.addBindValue(folder.withText);
    q.addBindValue(folder.updated.isValid() ? folder.updated.toSecsSinceEpoch() : 0);
    if (!q.exec())
        return fail(tr("Cannot update the search index"), q.lastError().text());
    return true;
}

bool SearchIndex::removeFolder(const QString &mailbox)
{
    QSqlDatabase db = database();
    Savepoint savepoint(db);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM search_text WHERE rowid IN (SELECT id FROM search_docs WHERE mailbox = ?)"));
    q.addBindValue(mailbox);
    if (!q.exec())
        return fail(tr("Cannot update the search index"), q.lastError().text());
    for (const QString &table : {QStringLiteral("search_docs"), QStringLiteral("search_folders")}) {
        q.prepare(QStringLiteral("DELETE FROM %1 WHERE mailbox = ?").arg(table));
        q.addBindValue(mailbox);
        if (!q.exec())
            return fail(tr("Cannot update the search index"), q.lastError().text());
    }
    return savepoint.release();
}

QHash<uint, SearchIndex::Entry> SearchIndex::entries(const QString &mailbox) const
{
    QHash<uint, Entry> result;
    QSqlQuery q(database());
    q.setForwardOnly(true);
    q.prepare(QStringLiteral("SELECT uid, has_text, flags, text_parts FROM search_docs WHERE mailbox = ?"));
    q.addBindValue(mailbox);
    if (!q.exec()) {
        fail(tr("Cannot read the search index"), q.lastError().text());
        return result;
    }
    while (q.next()) {
        Entry entry;
        entry.hasText = q.value(1).toBool();
        entry.flags = flagList(q.value(2).toString());
        entry.textParts = q.value(3).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        result.insert(q.value(0).toUInt(), entry);
    }
    return result;
}

bool SearchIndex::store(const QList<Document> &documents)
{
    QSqlDatabase db = database();
    Savepoint savepoint(db);
    if (!savepoint.isActive())
        return fail(tr("Cannot update the search index"), db.lastError().text());
    QSqlQuery find(db);
    find.prepare(QStringLiteral("SELECT id FROM search_docs WHERE mailbox = ? AND uid = ?"));
    QSqlQuery removeText(db);
    removeText.prepare(QStringLiteral("DELETE FROM search_text WHERE rowid = ?"));
    QSqlQuery update(db);
    update.prepare(QStringLiteral("UPDATE search_docs SET sent = ?, received = ?, size = ?, flags = ?, content_type = ?, "
                                  "has_attachment = ?, message_id = ?, in_reply_to = ?, refs = ?, list_id = ?, "
                                  "list_post = ?, has_text = ?, text_parts = ?, decrypted = 0 WHERE id = ?"));
    QSqlQuery insert(db);
    insert.prepare(QStringLiteral("INSERT INTO search_docs (sent, received, size, flags, content_type, has_attachment, "
                                  "message_id, in_reply_to, refs, list_id, list_post, has_text, text_parts, mailbox, uid) "
                                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    QSqlQuery insertText(db);
    insertText.prepare(QStringLiteral("INSERT INTO search_text (rowid, subject, sender, recipient_to, recipient_cc, "
                                      "recipient_bcc, body, attachments) VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    for (const Document &document : documents) {
        find.addBindValue(document.mailbox);
        find.addBindValue(document.uid);
        if (!find.exec())
            return fail(tr("Cannot update the search index"), find.lastError().text());
        const bool exists = find.next();
        const qlonglong existing = exists ? find.value(0).toLongLong() : 0;
        find.finish();
        QSqlQuery &row = exists ? update : insert;
        row.addBindValue(day(document.sent));
        row.addBindValue(day(document.received));
        row.addBindValue(QVariant::fromValue<qulonglong>(document.size));
        row.addBindValue(flagText(document.flags));
        row.addBindValue(document.contentType);
        row.addBindValue(document.hasAttachment);
        row.addBindValue(document.messageId);
        row.addBindValue(document.inReplyTo);
        row.addBindValue(document.references);
        row.addBindValue(document.listId);
        row.addBindValue(document.listPost);
        row.addBindValue(document.hasText);
        row.addBindValue(document.textParts.join(QLatin1Char(' ')));
        if (exists) {
            row.addBindValue(existing);
        } else {
            row.addBindValue(document.mailbox);
            row.addBindValue(document.uid);
        }
        if (!row.exec())
            return fail(tr("Cannot update the search index"), row.lastError().text());
        qlonglong id = existing;
        if (exists) {
            removeText.addBindValue(id);
            if (!removeText.exec())
                return fail(tr("Cannot update the search index"), removeText.lastError().text());
        } else {
            id = row.lastInsertId().toLongLong();
        }
        insertText.addBindValue(id);
        for (const QString &text : {document.subject, document.from, document.to, document.cc, document.bcc,
                                    document.body, document.attachments})
            insertText.addBindValue(text);
        if (!insertText.exec())
            return fail(tr("Cannot update the search index"), insertText.lastError().text());
    }
    if (!savepoint.release())
        return fail(tr("Cannot update the search index"), db.lastError().text());
    return true;
}

bool SearchIndex::hasMessage(const QString &mailbox, uint uid) const
{
    QSqlQuery q(database());
    q.prepare(QStringLiteral("SELECT 1 FROM search_docs WHERE mailbox = ? AND uid = ?"));
    q.addBindValue(mailbox);
    q.addBindValue(uid);
    if (!q.exec()) {
        fail(tr("Cannot read the search index"), q.lastError().text());
        return false;
    }
    return q.next();
}

bool SearchIndex::addDecryptedText(const QString &mailbox, uint uid, const QString &text)
{
    QSqlDatabase db = database();
    Savepoint savepoint(db);
    if (!savepoint.isActive())
        return fail(tr("Cannot update the search index"), db.lastError().text());
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT search_docs.id, search_text.body FROM search_docs JOIN search_text "
                             "ON search_text.rowid = search_docs.id WHERE search_docs.mailbox = ? AND search_docs.uid = ?"));
    q.addBindValue(mailbox);
    q.addBindValue(uid);
    if (!q.exec())
        return fail(tr("Cannot read the search index"), q.lastError().text());
    if (!q.next())
        return fail(tr("The message is not in the search index"), QString());
    const qlonglong id = q.value(0).toLongLong();
    QString body = q.value(1).toString();
    q.finish();
    if (!body.contains(text)) {
        if (!body.isEmpty())
            body += QLatin1Char('\n');
        body += text;
        if (body.size() > maximumBody)
            body.truncate(maximumBody);
        q.prepare(QStringLiteral("UPDATE search_text SET body = ? WHERE rowid = ?"));
        q.addBindValue(body);
        q.addBindValue(id);
        if (!q.exec())
            return fail(tr("Cannot update the search index"), q.lastError().text());
    }
    q.prepare(QStringLiteral("UPDATE search_docs SET decrypted = 1 WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec())
        return fail(tr("Cannot update the search index"), q.lastError().text());
    if (!savepoint.release())
        return fail(tr("Cannot update the search index"), db.lastError().text());
    return true;
}

int SearchIndex::decryptedMessages() const
{
    QSqlQuery q(database());
    if (!q.exec(QStringLiteral("SELECT count(*) FROM search_docs WHERE decrypted = 1")) || !q.next()) {
        fail(tr("Cannot read the search index"), q.lastError().text());
        return 0;
    }
    return q.value(0).toInt();
}

bool SearchIndex::removeDecrypted()
{
    QSqlDatabase db = database();
    Savepoint savepoint(db);
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("DELETE FROM search_text WHERE rowid IN (SELECT id FROM search_docs WHERE decrypted = 1)"))
        || !q.exec(QStringLiteral("DELETE FROM search_docs WHERE decrypted = 1")))
        return fail(tr("Cannot update the search index"), q.lastError().text());
    if (!savepoint.release())
        return fail(tr("Cannot update the search index"), db.lastError().text());
    return true;
}

bool SearchIndex::setFlags(const QString &mailbox, const QHash<uint, QStringList> &flags)
{
    QSqlDatabase db = database();
    Savepoint savepoint(db);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE search_docs SET flags = ? WHERE mailbox = ? AND uid = ?"));
    for (auto it = flags.constBegin(); it != flags.constEnd(); ++it) {
        q.addBindValue(flagText(it.value()));
        q.addBindValue(mailbox);
        q.addBindValue(it.key());
        if (!q.exec())
            return fail(tr("Cannot update the search index"), q.lastError().text());
    }
    return savepoint.release();
}

bool SearchIndex::remove(const QString &mailbox, const QList<uint> &uids)
{
    QSqlDatabase db = database();
    Savepoint savepoint(db);
    QSqlQuery text(db);
    text.prepare(QStringLiteral("DELETE FROM search_text WHERE rowid IN (SELECT id FROM search_docs WHERE mailbox = ? "
                                "AND uid = ?)"));
    QSqlQuery docs(db);
    docs.prepare(QStringLiteral("DELETE FROM search_docs WHERE mailbox = ? AND uid = ?"));
    for (const uint uid : uids) {
        for (QSqlQuery *q : {&text, &docs}) {
            q->addBindValue(mailbox);
            q->addBindValue(uid);
            if (!q->exec())
                return fail(tr("Cannot update the search index"), q->lastError().text());
        }
    }
    return savepoint.release();
}

bool SearchIndex::translate(const QStringList &conditions, QString *sql, QVariantList *values, QString *problem,
                            bool *usesText)
{
    Translator translator;
    // A search typed as raw IMAP arrives as one item
    translator.tokens = conditions.size() == 1 ? tokenize(conditions.constFirst()) : conditions;
    QStringList all;
    while (translator.position < translator.tokens.size()) {
        const QString condition = translator.key();
        if (condition.isEmpty()) {
            if (problem)
                *problem = translator.problem;
            return false;
        }
        all << QLatin1Char('(') + condition + QLatin1Char(')');
    }
    if (sql)
        *sql = all.isEmpty() ? QStringLiteral("1") : all.join(QLatin1String(" AND "));
    if (values)
        *values = translator.values;
    if (usesText)
        *usesText = translator.usesText;
    return true;
}

SearchIndex::Result SearchIndex::search(const QStringList &conditions, const QStringList &mailboxes) const
{
    Result result;
    if (!m_open) {
        result.problem = m_error.isEmpty() ? tr("The search index is not open") : m_error;
        return result;
    }
    QString condition;
    QVariantList values;
    if (!translate(conditions, &condition, &values, &result.problem, &result.usesText))
        return result;
    QString places;
    if (!mailboxes.isEmpty()) {
        QStringList marks;
        for (int i = 0; i < mailboxes.size(); ++i)
            marks << QStringLiteral("?");
        places = QStringLiteral("mailbox IN (%1) AND ").arg(marks.join(QLatin1String(", ")));
    }
    QSqlQuery q(database());
    q.setForwardOnly(true);
    q.prepare(QStringLiteral("SELECT mailbox, uid FROM search_docs WHERE %1(%2) ORDER BY mailbox, uid").arg(places, condition));
    for (const QString &mailbox : mailboxes)
        q.addBindValue(mailbox);
    for (const QVariant &value : std::as_const(values))
        q.addBindValue(value);
    if (!q.exec()) {
        result.problem = tr("The local index could not be searched: %1").arg(q.lastError().text());
        return result;
    }
    while (q.next())
        result.matches[q.value(0).toString()].append(q.value(1).toUInt());
    if (result.usesText) {
        QSqlQuery missing(database());
        missing.prepare(QStringLiteral("SELECT count(*) FROM search_docs WHERE %1has_text = 0").arg(places));
        for (const QString &mailbox : mailboxes)
            missing.addBindValue(mailbox);
        if (missing.exec() && missing.next())
            result.withoutText = missing.value(0).toInt();
    }
    return result;
}

QString SearchIndex::plainTextOfHtml(const QString &html)
{
    static const QRegularExpression hidden(QStringLiteral("<(script|style|head)\\b.*?</\\1\\s*>"),
                                           QRegularExpression::CaseInsensitiveOption
                                               | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression comments(QStringLiteral("<!--.*?-->"), QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression breaks(QStringLiteral("<(br|/p|/div|/li|/tr|/h[1-6])\\b[^>]*>"),
                                          QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression tags(QStringLiteral("<[^>]*>"));
    static const QRegularExpression numeric(QStringLiteral("&#(x?)([0-9a-fA-F]+);"));
    static const QRegularExpression spaces(QStringLiteral("[ \\t\\r\\f]+"));
    static const QRegularExpression blankLines(QStringLiteral("\\n\\s*\\n+"));
    static const QRegularExpression lineEdges(QStringLiteral("[ \\t]*\\n[ \\t]*"));
    QString text = html;
    text.remove(hidden);
    text.remove(comments);
    text.replace(breaks, QStringLiteral("\n"));
    text.replace(tags, QStringLiteral(" "));
    QString decoded;
    decoded.reserve(text.size());
    qsizetype last = 0;
    auto it = numeric.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        decoded += QStringView(text).mid(last, match.capturedStart() - last);
        bool ok = false;
        const uint code = match.captured(2).toUInt(&ok, match.captured(1).isEmpty() ? 10 : 16);
        if (ok && code > 0 && code <= 0x10FFFF)
            decoded += QString::fromUcs4(reinterpret_cast<const char32_t *>(&code), 1);
        last = match.capturedEnd();
    }
    decoded += QStringView(text).mid(last);
    static const QList<QPair<QString, QString>> entities = {
        {QStringLiteral("&nbsp;"), QStringLiteral(" ")}, {QStringLiteral("&lt;"), QStringLiteral("<")},
        {QStringLiteral("&gt;"), QStringLiteral(">")},   {QStringLiteral("&quot;"), QStringLiteral("\"")},
        {QStringLiteral("&apos;"), QStringLiteral("'")}, {QStringLiteral("&amp;"), QStringLiteral("&")},
    };
    for (const auto &entity : entities)
        decoded.replace(entity.first, entity.second, Qt::CaseInsensitive);
    decoded.replace(spaces, QStringLiteral(" "));
    decoded.replace(lineEdges, QStringLiteral("\n"));
    decoded.replace(blankLines, QStringLiteral("\n"));
    return decoded.trimmed();
}

}
}
