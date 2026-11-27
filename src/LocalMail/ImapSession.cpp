/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "LocalMail/ImapSession.h"

#include <algorithm>
#include <cctype>
#include <functional>

#include <QLocale>
#include <QRegularExpression>
#include <QSet>
#include <QTimeZone>

#include "Imap/Encoders.h"

namespace LocalMail {

namespace {

using Token = ImapSession::Token;

/** A command which cannot be understood; the session answers BAD */
struct BadCommand {
    QByteArray reason;
};

const int cachedMessages = 64;

bool isAtomEnd(char c)
{
    return c == ' ' || c == '(' || c == ')' || c == '\r' || c == '\n';
}

bool tokenizeList(const QByteArray &data, int &pos, QList<Token> &out, char close)
{
    while (pos < data.size()) {
        const char c = data.at(pos);
        if (c == ' ' || c == '\r' || c == '\n') {
            ++pos;
            continue;
        }
        if (close && c == close) {
            ++pos;
            return true;
        }
        if (c == ')')
            return false;
        Token token;
        if (c == '(') {
            ++pos;
            token.kind = Token::Kind::List;
            if (!tokenizeList(data, pos, token.items, ')'))
                return false;
        } else if (c == '"') {
            token.kind = Token::Kind::String;
            ++pos;
            bool closed = false;
            while (pos < data.size()) {
                const char ch = data.at(pos++);
                if (ch == '\\' && pos < data.size()) {
                    token.text += data.at(pos++);
                } else if (ch == '"') {
                    closed = true;
                    break;
                } else {
                    token.text += ch;
                }
            }
            if (!closed)
                return false;
        } else if (c == '{') {
            const int end = data.indexOf('}', pos);
            if (end < 0)
                return false;
            QByteArray digits = data.mid(pos + 1, end - pos - 1);
            if (digits.endsWith('+'))
                digits.chop(1);
            bool ok = false;
            const int size = digits.toInt(&ok);
            if (!ok || size < 0)
                return false;
            pos = end + 1;
            if (pos < data.size() && data.at(pos) == '\r')
                ++pos;
            if (pos >= data.size() || data.at(pos) != '\n')
                return false;
            ++pos;
            if (pos + size > data.size())
                return false;
            token.kind = Token::Kind::String;
            token.text = data.mid(pos, size);
            pos += size;
        } else {
            const int start = pos;
            while (pos < data.size() && !isAtomEnd(data.at(pos))) {
                if (data.at(pos) == '[') {
                    const int end = data.indexOf(']', pos);
                    if (end < 0)
                        return false;
                    pos = end + 1;
                    if (pos < data.size() && data.at(pos) == '<') {
                        const int rangeEnd = data.indexOf('>', pos);
                        if (rangeEnd < 0)
                            return false;
                        pos = rangeEnd + 1;
                    }
                    continue;
                }
                ++pos;
            }
            token.text = data.mid(start, pos - start);
        }
        out.append(token);
    }
    return close == 0;
}

QString mailboxName(const Token &token)
{
    if (token.kind == Token::Kind::List)
        throw BadCommand{"Expected a mailbox name"};
    QString name = Imap::decodeImapFolderName(token.text);
    while (name.size() > 1 && name.endsWith(QLatin1Char('/')))
        name.chop(1);
    if (name.compare(QLatin1String("INBOX"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("INBOX");
    if (name.startsWith(QLatin1String("INBOX/"), Qt::CaseInsensitive))
        return QStringLiteral("INBOX") + name.mid(5);
    return name;
}

QByteArray quotedName(const QString &name)
{
    QByteArray encoded = Imap::encodeImapFolderName(name);
    encoded.replace('\\', "\\\\");
    encoded.replace('"', "\\\"");
    return '"' + encoded + '"';
}

QByteArray imapDateTime(const QDateTime &when)
{
    const QDateTime utc = (when.isValid() ? when : QDateTime::currentDateTimeUtc()).toUTC();
    return '"' + QLocale::c().toString(utc, QStringLiteral("dd-MMM-yyyy HH:mm:ss")).toLatin1() + " +0000\"";
}

int monthNumber(const QByteArray &name)
{
    static const QList<QByteArray> months = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                             "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    return months.indexOf(name.toUpper()) + 1;
}

QDate searchDate(const QByteArray &text)
{
    const QList<QByteArray> parts = text.trimmed().split('-');
    if (parts.size() != 3)
        throw BadCommand{"Cannot read the date " + text};
    const QDate date(parts[2].toInt(), monthNumber(parts[1]), parts[0].toInt());
    if (!date.isValid())
        throw BadCommand{"Cannot read the date " + text};
    return date;
}

QDateTime appendDateTime(const QByteArray &text)
{
    static const QRegularExpression format(QStringLiteral("^\\s*(\\d{1,2})-([A-Za-z]{3})-(\\d{4}) (\\d{2}):(\\d{2}):(\\d{2}) ([+-])(\\d{2})(\\d{2})$"));
    const auto match = format.match(QString::fromLatin1(text));
    if (!match.hasMatch())
        return {};
    const QDate date(match.captured(3).toInt(), monthNumber(match.captured(2).toLatin1()), match.captured(1).toInt());
    const QTime time(match.captured(4).toInt(), match.captured(5).toInt(), match.captured(6).toInt());
    int offset = (match.captured(8).toInt() * 60 + match.captured(9).toInt()) * 60;
    if (match.captured(7) == QLatin1String("-"))
        offset = -offset;
    const QDateTime result(date, time, QTimeZone::fromSecondsAheadOfUtc(offset));
    return result.isValid() ? result.toUTC() : QDateTime();
}

QStringList flagsOf(const Token &token, const QList<Token> &rest = {})
{
    QStringList flags;
    const QList<Token> items = token.kind == Token::Kind::List ? token.items : QList<Token>{token} + rest;
    for (const Token &item : items) {
        if (item.kind == Token::Kind::List)
            throw BadCommand{"Flags cannot be nested"};
        flags << QString::fromUtf8(item.text);
    }
    return flags;
}

bool hasFlag(const QStringList &flags, const QString &flag)
{
    for (const QString &existing : flags) {
        if (existing.compare(flag, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

bool sameFlags(const QStringList &a, const QStringList &b)
{
    if (a.size() != b.size())
        return false;
    for (const QString &flag : a) {
        if (!hasFlag(b, flag))
            return false;
    }
    return true;
}

QByteArray uidSet(const QList<uint> &uids)
{
    QByteArray result;
    for (int i = 0; i < uids.size();) {
        int j = i;
        while (j + 1 < uids.size() && uids[j + 1] == uids[j] + 1)
            ++j;
        if (!result.isEmpty())
            result += ',';
        result += QByteArray::number(uids[i]);
        if (j > i)
            result += ':' + QByteArray::number(uids[j]);
        i = j + 1;
    }
    return result;
}

QString decodedText(const Mime::Part &part)
{
    if (part.isMultipart() || part.isMessage()) {
        QString text;
        for (const Mime::Part &child : part.children)
            text += decodedText(child) + QLatin1Char('\n');
        return text;
    }
    if (part.type != "text")
        return {};
    const QByteArray encoding = part.header.value("content-transfer-encoding").trimmed().toLower();
    QByteArray data = part.body;
    if (encoding == "base64")
        data = QByteArray::fromBase64(data);
    else if (encoding == "quoted-printable")
        data = Imap::quotedPrintableDecode(data);
    QByteArray charset;
    for (const auto &parameter : part.parameters) {
        if (parameter.first == "charset")
            charset = parameter.second;
    }
    return Imap::decodeByteArray(data, charset);
}

/** What one SEARCH evaluates a message with */
struct SearchSubject {
    int index = 0;
    MessageInfo info;
    std::function<ImapSession::CachedMessage()> load;
    bool loaded = false;
    ImapSession::CachedMessage message;

    const Mime::Part &part()
    {
        if (!loaded) {
            message = load();
            loaded = true;
        }
        return message.part;
    }
};

}

ImapSession::ImapSession(std::shared_ptr<Store> store, QObject *parent)
    : QObject(parent)
    , m_store(std::move(store))
{
    connect(m_store.get(), &Store::changed, this, &ImapSession::storeChanged);
}

ImapSession::~ImapSession() = default;

QByteArray ImapSession::capabilities()
{
    return "IMAP4rev1 LITERAL+ UIDPLUS UNSELECT IDLE MOVE CHILDREN";
}

void ImapSession::start()
{
    send("* PREAUTH [CAPABILITY " + capabilities() + "] Mail kept on this computer");
    flush();
}

void ImapSession::receive(const QByteArray &data)
{
    if (m_finished)
        return;
    m_input += data;
    processInput();
    flush();
}

void ImapSession::send(const QByteArray &line)
{
    m_output += line + "\r\n";
}

void ImapSession::flush()
{
    if (m_output.isEmpty())
        return;
    const QByteArray data = m_output;
    m_output.clear();
    emit output(data);
}

void ImapSession::processInput()
{
    while (!m_finished) {
        const int lineEnd = m_input.indexOf('\n', m_scanPosition);
        if (lineEnd < 0)
            return;
        QByteArray line = m_input.mid(m_scanPosition, lineEnd - m_scanPosition);
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.endsWith('}') && !m_idling) {
            const int open = line.lastIndexOf('{');
            QByteArray digits = open >= 0 ? line.mid(open + 1, line.size() - open - 2) : QByteArray();
            const bool nonSynchronizing = digits.endsWith('+');
            if (nonSynchronizing)
                digits.chop(1);
            bool ok = false;
            const int size = digits.toInt(&ok);
            if (ok && size >= 0) {
                const int literalStart = lineEnd + 1;
                if (m_input.size() < literalStart + size) {
                    if (!nonSynchronizing && !m_continuationSent) {
                        send("+ Ready for literal data");
                        m_continuationSent = true;
                    }
                    return;
                }
                m_continuationSent = false;
                m_scanPosition = literalStart + size;
                continue;
            }
        }
        const QByteArray command = m_input.left(lineEnd + 1);
        m_input.remove(0, lineEnd + 1);
        m_scanPosition = 0;
        if (m_idling) {
            if (line.trimmed().toUpper() == "DONE") {
                m_idling = false;
                send(m_idleTag + " OK IDLE terminated");
            } else {
                send("* BAD Expected DONE");
            }
            continue;
        }
        handle(command);
    }
}

void ImapSession::handle(const QByteArray &command)
{
    QList<Token> tokens;
    int pos = 0;
    if (!tokenizeList(command, pos, tokens, 0) || tokens.isEmpty()) {
        send("* BAD Cannot read the command");
        return;
    }
    const QByteArray tag = tokens.first().text;
    if (tokens.size() < 2 || tokens[0].kind != Token::Kind::Atom || tokens[1].kind != Token::Kind::Atom) {
        send(tag + " BAD Missing command");
        return;
    }
    QByteArray name = tokens[1].text.toUpper();
    QList<Token> args = tokens.mid(2);
    bool uid = false;
    if (name == "UID") {
        if (args.isEmpty() || args.first().kind != Token::Kind::Atom) {
            send(tag + " BAD Missing UID command");
            return;
        }
        uid = true;
        name = args.takeFirst().text.toUpper();
    }

    const bool needsSelection = name == "FETCH" || name == "STORE" || name == "COPY" || name == "MOVE"
        || name == "SEARCH" || name == "EXPUNGE" || name == "CLOSE" || name == "UNSELECT" || name == "CHECK";
    if (needsSelection && m_selected.isEmpty()) {
        send(tag + " BAD No folder is selected");
        return;
    }
    if (!m_selected.isEmpty()) {
        const bool expungesAllowed = uid || (name != "FETCH" && name != "STORE" && name != "SEARCH");
        synchronize(expungesAllowed);
    }

    try {
        if (name == "CAPABILITY") {
            send("* CAPABILITY " + capabilities());
            send(tag + " OK CAPABILITY completed");
        } else if (name == "NOOP" || name == "CHECK") {
            send(tag + " OK " + name + " completed");
        } else if (name == "LOGOUT") {
            send("* BYE Logging out");
            send(tag + " OK LOGOUT completed");
            m_finished = true;
            flush();
            emit finished();
        } else if (name == "LIST" || name == "LSUB") {
            list(tag, args, name == "LSUB");
        } else if (name == "STATUS") {
            status(tag, args);
        } else if (name == "SELECT" || name == "EXAMINE") {
            select(tag, args, name == "EXAMINE");
        } else if (name == "CREATE" || name == "DELETE" || name == "SUBSCRIBE" || name == "UNSUBSCRIBE") {
            if (args.isEmpty())
                throw BadCommand{"Missing folder name"};
            const QString mailbox = mailboxName(args.first());
            QString error;
            bool ok = false;
            if (name == "CREATE") {
                if (m_store->hasMailbox(mailbox)) {
                    send(tag + " NO [ALREADYEXISTS] The folder exists");
                    return;
                }
                ok = m_store->createMailbox(mailbox, &error);
            } else if (name == "DELETE") {
                ok = m_store->deleteMailbox(mailbox, &error);
                if (ok && mailbox == m_selected)
                    deselect();
            } else {
                ok = m_store->setSubscribed(mailbox, name == "SUBSCRIBE");
            }
            if (ok)
                send(tag + " OK " + name + " completed");
            else
                send(tag + " NO " + (error.isEmpty() ? QByteArray("Cannot change the folder") : error.toUtf8()));
        } else if (name == "RENAME") {
            if (args.size() < 2)
                throw BadCommand{"RENAME needs two folder names"};
            QString error;
            const QString from = mailboxName(args[0]);
            if (m_store->renameMailbox(from, mailboxName(args[1]), &error)) {
                if (from == m_selected)
                    deselect();
                send(tag + " OK RENAME completed");
            } else {
                send(tag + " NO " + error.toUtf8());
            }
        } else if (name == "APPEND") {
            append(tag, args);
        } else if (name == "CLOSE" || name == "UNSELECT") {
            if (name == "CLOSE" && !m_readOnly) {
                QList<int> deleted;
                for (int i = 0; i < m_uids.size(); ++i) {
                    if (hasFlag(m_flags.value(m_uids[i]), QStringLiteral("\\Deleted")))
                        deleted << i;
                }
                removeIndexes(deleted, true);
            }
            deselect();
            send(tag + " OK " + name + " completed");
        } else if (name == "EXPUNGE") {
            expunge(tag, args, uid);
        } else if (name == "FETCH") {
            fetch(tag, args, uid);
        } else if (name == "STORE") {
            storeFlags(tag, args, uid);
        } else if (name == "COPY" || name == "MOVE") {
            copy(tag, args, uid, name == "MOVE");
        } else if (name == "SEARCH") {
            search(tag, args, uid);
        } else if (name == "IDLE" && !uid) {
            m_idling = true;
            m_idleTag = tag;
            send("+ idling");
        } else {
            send(tag + " BAD Unknown command " + name);
        }
    } catch (const BadCommand &bad) {
        send(tag + " BAD " + bad.reason);
    }
}

void ImapSession::storeChanged(const QString &mailbox)
{
    if (mailbox == m_cacheMailbox && (!m_store->hasMailbox(mailbox) || m_store->mailbox(mailbox).uidValidity != m_cacheValidity)) {
        m_cache.clear();
        m_cacheOrder.clear();
        m_cacheMailbox.clear();
    }
    if (m_idling && mailbox == m_selected) {
        synchronize(true);
        flush();
    }
}

QHash<uint, MessageInfo> ImapSession::currentMessages() const
{
    QHash<uint, MessageInfo> result;
    const QList<MessageInfo> messages = m_store->messages(m_selected);
    for (const MessageInfo &info : messages)
        result.insert(info.uid, info);
    return result;
}

void ImapSession::synchronize(bool expunges)
{
    if (m_selected.isEmpty())
        return;
    const QList<MessageInfo> messages = m_store->hasMailbox(m_selected) ? m_store->messages(m_selected) : QList<MessageInfo>();
    QHash<uint, QStringList> current;
    for (const MessageInfo &info : messages)
        current.insert(info.uid, info.flags);

    if (expunges) {
        for (int i = m_uids.size() - 1; i >= 0; --i) {
            if (!current.contains(m_uids[i])) {
                send("* " + QByteArray::number(i + 1) + " EXPUNGE");
                m_flags.remove(m_uids[i]);
                m_uids.removeAt(i);
            }
        }
    }

    const uint highest = m_uids.isEmpty() ? 0 : m_uids.last();
    bool added = false;
    for (const MessageInfo &info : messages) {
        if (info.uid > highest) {
            m_uids << info.uid;
            m_flags.insert(info.uid, info.flags);
            added = true;
        }
    }
    if (added)
        send("* " + QByteArray::number(m_uids.size()) + " EXISTS");

    for (int i = 0; i < m_uids.size(); ++i) {
        const auto it = current.constFind(m_uids[i]);
        if (it == current.constEnd() || sameFlags(*it, m_flags.value(m_uids[i])))
            continue;
        m_flags[m_uids[i]] = *it;
        send("* " + QByteArray::number(i + 1) + " FETCH (UID " + QByteArray::number(m_uids[i]) + " FLAGS "
             + flagList(*it) + ')');
    }
}

void ImapSession::removeIndexes(QList<int> indexes, bool silently)
{
    std::sort(indexes.begin(), indexes.end());
    QList<uint> uids;
    for (int index : std::as_const(indexes))
        uids << m_uids[index];
    if (!uids.isEmpty())
        m_store->remove(m_selected, uids);
    for (int i = indexes.size() - 1; i >= 0; --i) {
        if (!silently)
            send("* " + QByteArray::number(indexes[i] + 1) + " EXPUNGE");
        m_flags.remove(m_uids[indexes[i]]);
        m_uids.removeAt(indexes[i]);
    }
}

void ImapSession::deselect()
{
    m_selected.clear();
    m_readOnly = false;
    m_uids.clear();
    m_flags.clear();
}

bool ImapSession::resolve(const QByteArray &set, bool uid, QList<int> *indexes) const
{
    indexes->clear();
    if (set.isEmpty())
        return false;
    QSet<int> chosen;
    const uint highest = uid ? (m_uids.isEmpty() ? 0 : m_uids.last()) : uint(m_uids.size());
    for (const QByteArray &range : set.split(',')) {
        const QList<QByteArray> ends = range.split(':');
        if (ends.isEmpty() || ends.size() > 2)
            return false;
        uint bounds[2] = {0, 0};
        for (int i = 0; i < ends.size(); ++i) {
            if (ends[i] == "*") {
                bounds[i] = highest;
            } else {
                bool ok = false;
                bounds[i] = ends[i].toUInt(&ok);
                if (!ok || bounds[i] == 0)
                    return false;
            }
        }
        if (ends.size() == 1)
            bounds[1] = bounds[0];
        const uint low = std::min(bounds[0], bounds[1]);
        const uint high = std::max(bounds[0], bounds[1]);
        if (uid) {
            for (int i = 0; i < m_uids.size(); ++i) {
                if (m_uids[i] >= low && m_uids[i] <= high)
                    chosen.insert(i);
            }
        } else {
            if (high > uint(m_uids.size()))
                return false;
            for (uint i = low; i <= high; ++i)
                chosen.insert(int(i) - 1);
        }
    }
    *indexes = QList<int>(chosen.begin(), chosen.end());
    std::sort(indexes->begin(), indexes->end());
    return true;
}

ImapSession::CachedMessage ImapSession::parsed(uint uid)
{
    const uint validity = m_store->mailbox(m_selected).uidValidity;
    if (m_cacheMailbox != m_selected || m_cacheValidity != validity) {
        m_cache.clear();
        m_cacheOrder.clear();
        m_cacheMailbox = m_selected;
        m_cacheValidity = validity;
    }
    const auto it = m_cache.constFind(uid);
    if (it != m_cache.constEnd()) {
        m_cacheOrder.removeOne(uid);
        m_cacheOrder << uid;
        return *it;
    }
    CachedMessage message;
    message.raw = m_store->content(m_selected, uid);
    message.part = Mime::parse(message.raw);
    m_cache.insert(uid, message);
    m_cacheOrder << uid;
    while (m_cacheOrder.size() > cachedMessages)
        m_cache.remove(m_cacheOrder.takeFirst());
    return message;
}

QByteArray ImapSession::flagList(const QStringList &flags) const
{
    QByteArray result = "(";
    for (int i = 0; i < flags.size(); ++i) {
        if (i)
            result += ' ';
        result += flags[i].toUtf8();
    }
    return result + ')';
}

void ImapSession::list(const QByteArray &tag, const QList<Token> &args, bool subscribedOnly)
{
    if (args.size() < 2 || args[0].kind == Token::Kind::List || args[1].kind == Token::Kind::List)
        throw BadCommand{"LIST needs a reference and a pattern"};
    const QByteArray command = subscribedOnly ? "LSUB" : "LIST";
    const QString pattern = Imap::decodeImapFolderName(args[0].text) + Imap::decodeImapFolderName(args[1].text);
    if (pattern.isEmpty()) {
        send("* " + command + " (\\Noselect) \"/\" \"\"");
        send(tag + " OK " + command + " completed");
        return;
    }

    QString expression;
    for (const QChar c : pattern) {
        if (c == QLatin1Char('*'))
            expression += QLatin1String(".*");
        else if (c == QLatin1Char('%'))
            expression += QLatin1String("[^/]*");
        else
            expression += QRegularExpression::escape(QString(c));
    }
    const bool patternStartsWithInbox = pattern.startsWith(QLatin1String("INBOX"), Qt::CaseInsensitive);
    const QRegularExpression matcher(QLatin1Char('^') + expression + QLatin1Char('$'));
    const QRegularExpression inboxMatcher(QLatin1Char('^') + expression + QLatin1Char('$'),
                                          QRegularExpression::CaseInsensitiveOption);

    const QStringList names = m_store->mailboxNames();
    QSet<QString> existing(names.begin(), names.end());
    QSet<QString> subscribed;
    for (const QString &name : names) {
        if (m_store->mailbox(name).subscribed)
            subscribed.insert(name);
    }
    // Folders implied by deeper ones, such as "Work" for "Work/Projects"
    QStringList all = names;
    QSet<QString> seen = existing;
    for (const QString &name : names) {
        const QStringList parts = name.split(QLatin1Char('/'));
        for (int i = 1; i < parts.size(); ++i) {
            const QString parent = parts.mid(0, i).join(QLatin1Char('/'));
            if (!seen.contains(parent)) {
                seen.insert(parent);
                all << parent;
            }
        }
    }

    for (const QString &name : std::as_const(all)) {
        const bool isInbox = name == QLatin1String("INBOX") || name.startsWith(QLatin1String("INBOX/"));
        const bool matched = (isInbox && patternStartsWithInbox) ? inboxMatcher.match(name).hasMatch()
                                                                 : matcher.match(name).hasMatch();
        if (!matched)
            continue;
        bool hasChildren = false;
        bool hasSubscribedChild = false;
        const QString prefix = name + QLatin1Char('/');
        for (const QString &other : std::as_const(all)) {
            if (other.startsWith(prefix)) {
                hasChildren = true;
                hasSubscribedChild = hasSubscribedChild || subscribed.contains(other);
            }
        }
        QByteArrayList attributes;
        bool selectable = existing.contains(name);
        if (subscribedOnly) {
            if (!subscribed.contains(name)) {
                if (!hasSubscribedChild)
                    continue;
                selectable = false;
            }
        }
        if (!selectable)
            attributes << "\\Noselect";
        attributes << (hasChildren ? "\\HasChildren" : "\\HasNoChildren");
        send("* " + command + " (" + attributes.join(' ') + ") \"/\" " + quotedName(name));
    }
    send(tag + " OK " + command + " completed");
}

void ImapSession::status(const QByteArray &tag, const QList<Token> &args)
{
    if (args.size() < 2 || args[1].kind != Token::Kind::List)
        throw BadCommand{"STATUS needs a folder and a list of items"};
    const QString name = mailboxName(args[0]);
    if (!m_store->hasMailbox(name)) {
        send(tag + " NO [NONEXISTENT] No such folder");
        return;
    }
    const MailboxInfo info = m_store->mailbox(name);
    const QList<MessageInfo> messages = m_store->messages(name);
    QByteArrayList items;
    for (const Token &item : args[1].items) {
        const QByteArray key = item.text.toUpper();
        if (key == "MESSAGES") {
            items << "MESSAGES " + QByteArray::number(messages.size());
        } else if (key == "RECENT") {
            items << "RECENT 0";
        } else if (key == "UIDNEXT") {
            items << "UIDNEXT " + QByteArray::number(info.uidNext);
        } else if (key == "UIDVALIDITY") {
            items << "UIDVALIDITY " + QByteArray::number(info.uidValidity);
        } else if (key == "UNSEEN") {
            int unseen = 0;
            for (const MessageInfo &message : messages)
                unseen += hasFlag(message.flags, QStringLiteral("\\Seen")) ? 0 : 1;
            items << "UNSEEN " + QByteArray::number(unseen);
        } else {
            throw BadCommand{"Unknown STATUS item " + key};
        }
    }
    send("* STATUS " + quotedName(name) + " (" + items.join(' ') + ')');
    send(tag + " OK STATUS completed");
}

void ImapSession::select(const QByteArray &tag, const QList<Token> &args, bool readOnly)
{
    if (args.isEmpty())
        throw BadCommand{"Missing folder name"};
    const QString name = mailboxName(args.first());
    deselect();
    if (!m_store->hasMailbox(name)) {
        send(tag + " NO [NONEXISTENT] No such folder");
        return;
    }
    const MailboxInfo info = m_store->mailbox(name);
    m_selected = name;
    m_readOnly = readOnly;
    QStringList keywords = {QStringLiteral("\\Answered"), QStringLiteral("\\Flagged"), QStringLiteral("\\Deleted"),
                            QStringLiteral("\\Seen"), QStringLiteral("\\Draft")};
    for (const MessageInfo &message : m_store->messages(name)) {
        m_uids << message.uid;
        m_flags.insert(message.uid, message.flags);
        for (const QString &flag : message.flags) {
            if (!hasFlag(keywords, flag))
                keywords << flag;
        }
    }
    send("* FLAGS " + flagList(keywords));
    send("* OK [PERMANENTFLAGS " + flagList(keywords + QStringList{QStringLiteral("\\*")}) + "] Flags are kept");
    send("* " + QByteArray::number(m_uids.size()) + " EXISTS");
    send("* 0 RECENT");
    send("* OK [UIDVALIDITY " + QByteArray::number(info.uidValidity) + "] UIDs are valid");
    send("* OK [UIDNEXT " + QByteArray::number(info.uidNext) + "] Predicted next UID");
    send(tag + " OK " + (readOnly ? "[READ-ONLY] EXAMINE" : "[READ-WRITE] SELECT") + " completed");
}

void ImapSession::append(const QByteArray &tag, const QList<Token> &args)
{
    if (args.size() < 2)
        throw BadCommand{"APPEND needs a folder and a message"};
    const QString name = mailboxName(args.first());
    QStringList flags;
    QDateTime date;
    int pos = 1;
    if (pos < args.size() - 1 && args[pos].kind == Token::Kind::List)
        flags = flagsOf(args[pos++]);
    if (pos < args.size() - 1 && args[pos].kind == Token::Kind::String)
        date = appendDateTime(args[pos++].text);
    if (pos != args.size() - 1 || args[pos].kind != Token::Kind::String)
        throw BadCommand{"Cannot read the APPEND arguments"};
    if (!m_store->hasMailbox(name)) {
        send(tag + " NO [TRYCREATE] No such folder");
        return;
    }
    QString error;
    const uint uid = m_store->append(name, args[pos].text, flags, date, QString(), &error);
    if (!uid) {
        send(tag + " NO " + error.toUtf8());
        return;
    }
    if (name == m_selected)
        synchronize(true);
    send(tag + " OK [APPENDUID " + QByteArray::number(m_store->mailbox(name).uidValidity) + ' '
         + QByteArray::number(uid) + "] APPEND completed");
}

void ImapSession::fetch(const QByteArray &tag, const QList<Token> &args, bool uid)
{
    if (args.size() < 2 || args[0].kind != Token::Kind::Atom)
        throw BadCommand{"FETCH needs a message set and items"};
    QList<int> indexes;
    if (!resolve(args[0].text, uid, &indexes))
        throw BadCommand{"Cannot read the message set"};

    QByteArrayList items;
    const QList<Token> requested = args[1].kind == Token::Kind::List ? args[1].items : args.mid(1);
    for (const Token &token : requested) {
        if (token.kind == Token::Kind::List)
            throw BadCommand{"Unexpected list among FETCH items"};
        const QByteArray item = token.text.toUpper();
        if (item == "ALL")
            items << "FLAGS" << "INTERNALDATE" << "RFC822.SIZE" << "ENVELOPE";
        else if (item == "FAST")
            items << "FLAGS" << "INTERNALDATE" << "RFC822.SIZE";
        else if (item == "FULL")
            items << "FLAGS" << "INTERNALDATE" << "RFC822.SIZE" << "ENVELOPE" << "BODY";
        else
            items << item;
    }
    if (uid && !items.contains("UID"))
        items.prepend("UID");

    const QHash<uint, MessageInfo> messages = currentMessages();
    for (int index : std::as_const(indexes)) {
        const uint messageUid = m_uids[index];
        const auto infoIt = messages.constFind(messageUid);
        if (infoIt == messages.constEnd())
            continue;
        const MessageInfo &info = *infoIt;
        QByteArrayList parts;
        bool markSeen = false;
        bool flagsRequested = false;
        for (const QByteArray &item : std::as_const(items)) {
            if (item == "UID") {
                parts << "UID " + QByteArray::number(messageUid);
            } else if (item == "FLAGS") {
                flagsRequested = true;
            } else if (item == "INTERNALDATE") {
                parts << "INTERNALDATE " + imapDateTime(info.internalDate);
            } else if (item == "RFC822.SIZE") {
                parts << "RFC822.SIZE " + QByteArray::number(info.size);
            } else if (item == "ENVELOPE") {
                parts << "ENVELOPE " + Mime::envelope(parsed(messageUid).part.header);
            } else if (item == "BODYSTRUCTURE") {
                parts << "BODYSTRUCTURE " + Mime::bodyStructure(parsed(messageUid).part, true);
            } else if (item == "BODY") {
                parts << "BODY " + Mime::bodyStructure(parsed(messageUid).part, false);
            } else if (item == "RFC822" || item == "RFC822.HEADER" || item == "RFC822.TEXT") {
                const CachedMessage message = parsed(messageUid);
                QByteArray data;
                Mime::section(message.part, message.raw, item == "RFC822" ? QByteArray() : item.mid(7), &data);
                parts << item + ' ' + Mime::nstring(data, false);
                markSeen = markSeen || item != "RFC822.HEADER";
            } else if (item.startsWith("BODY[") || item.startsWith("BODY.PEEK[")) {
                const bool peek = item.startsWith("BODY.PEEK[");
                const int open = item.indexOf('[');
                const int close = item.lastIndexOf(']');
                if (close < open)
                    throw BadCommand{"Cannot read " + item};
                const QByteArray spec = item.mid(open + 1, close - open - 1);
                const QByteArray partial = item.mid(close + 1);
                const CachedMessage message = parsed(messageUid);
                QByteArray data;
                if (!Mime::section(message.part, message.raw, spec, &data))
                    data.clear();
                QByteArray name = "BODY[" + spec + ']';
                if (!partial.isEmpty()) {
                    if (!partial.startsWith('<') || !partial.endsWith('>'))
                        throw BadCommand{"Cannot read the range of " + item};
                    const QList<QByteArray> range = partial.mid(1, partial.size() - 2).split('.');
                    const int offset = range.value(0).toInt();
                    const int length = range.size() > 1 ? range[1].toInt() : -1;
                    data = offset >= data.size() ? QByteArray() : data.mid(offset, length);
                    name += '<' + QByteArray::number(offset) + '>';
                }
                parts << name + ' ' + Mime::nstring(data, false);
                markSeen = markSeen || !peek;
            } else {
                throw BadCommand{"Unknown FETCH item " + item};
            }
        }
        QStringList flags = m_flags.value(messageUid);
        if (markSeen && !m_readOnly && !hasFlag(flags, QStringLiteral("\\Seen"))) {
            flags << QStringLiteral("\\Seen");
            m_store->setFlags(m_selected, messageUid, flags);
            m_flags[messageUid] = flags;
            flagsRequested = true;
        }
        if (flagsRequested)
            parts << "FLAGS " + flagList(flags);
        send("* " + QByteArray::number(index + 1) + " FETCH (" + parts.join(' ') + ')');
    }
    send(tag + " OK FETCH completed");
}

void ImapSession::storeFlags(const QByteArray &tag, const QList<Token> &args, bool uid)
{
    if (args.size() < 3 || args[0].kind != Token::Kind::Atom || args[1].kind != Token::Kind::Atom)
        throw BadCommand{"STORE needs a message set, an operation and flags"};
    QList<int> indexes;
    if (!resolve(args[0].text, uid, &indexes))
        throw BadCommand{"Cannot read the message set"};
    QByteArray operation = args[1].text.toUpper();
    const bool silent = operation.endsWith(".SILENT");
    if (silent)
        operation.chop(7);
    if (operation != "FLAGS" && operation != "+FLAGS" && operation != "-FLAGS")
        throw BadCommand{"Unknown STORE operation " + operation};
    const QStringList given = flagsOf(args[2], args.mid(3));
    if (m_readOnly) {
        send(tag + " NO The folder is read-only");
        return;
    }
    for (int index : std::as_const(indexes)) {
        const uint messageUid = m_uids[index];
        const QStringList old = m_flags.value(messageUid);
        QStringList flags;
        if (operation == "FLAGS") {
            flags = given;
        } else if (operation == "+FLAGS") {
            flags = old;
            for (const QString &flag : given) {
                if (!hasFlag(flags, flag))
                    flags << flag;
            }
        } else {
            for (const QString &flag : old) {
                if (!hasFlag(given, flag))
                    flags << flag;
            }
        }
        if (!sameFlags(old, flags)) {
            m_store->setFlags(m_selected, messageUid, flags);
            m_flags[messageUid] = flags;
        }
        if (!silent) {
            send("* " + QByteArray::number(index + 1) + " FETCH ("
                 + (uid ? "UID " + QByteArray::number(messageUid) + ' ' : QByteArray()) + "FLAGS " + flagList(flags) + ')');
        }
    }
    send(tag + " OK STORE completed");
}

void ImapSession::copy(const QByteArray &tag, const QList<Token> &args, bool uid, bool move)
{
    if (args.size() < 2 || args[0].kind != Token::Kind::Atom)
        throw BadCommand{"COPY needs a message set and a folder"};
    QList<int> indexes;
    if (!resolve(args[0].text, uid, &indexes))
        throw BadCommand{"Cannot read the message set"};
    const QString target = mailboxName(args[1]);
    const QByteArray name = move ? "MOVE" : "COPY";
    if (!m_store->hasMailbox(target)) {
        send(tag + " NO [TRYCREATE] No such folder");
        return;
    }
    if (move && m_readOnly) {
        send(tag + " NO The folder is read-only");
        return;
    }
    if (indexes.isEmpty()) {
        send(tag + " OK " + name + " completed");
        return;
    }
    QList<uint> uids;
    for (int index : std::as_const(indexes))
        uids << m_uids[index];
    QString error;
    const QList<uint> copied = m_store->copy(m_selected, uids, target, &error);
    if (copied.size() != uids.size()) {
        send(tag + " NO " + (error.isEmpty() ? QByteArray("Cannot copy the messages") : error.toUtf8()));
        return;
    }
    const QByteArray copyUid = "[COPYUID " + QByteArray::number(m_store->mailbox(target).uidValidity) + ' '
        + uidSet(uids) + ' ' + uidSet(copied) + ']';
    if (move) {
        send("* OK " + copyUid + " Moved");
        removeIndexes(indexes, false);
        if (target == m_selected)
            synchronize(true);
        send(tag + " OK MOVE completed");
    } else {
        if (target == m_selected)
            synchronize(true);
        send(tag + " OK " + copyUid + " COPY completed");
    }
}

void ImapSession::expunge(const QByteArray &tag, const QList<Token> &args, bool uid)
{
    QList<int> candidates;
    if (uid) {
        if (args.isEmpty() || args[0].kind != Token::Kind::Atom || !resolve(args[0].text, true, &candidates))
            throw BadCommand{"UID EXPUNGE needs a UID set"};
    } else {
        for (int i = 0; i < m_uids.size(); ++i)
            candidates << i;
    }
    if (m_readOnly) {
        send(tag + " NO The folder is read-only");
        return;
    }
    QList<int> deleted;
    for (int index : std::as_const(candidates)) {
        if (hasFlag(m_flags.value(m_uids[index]), QStringLiteral("\\Deleted")))
            deleted << index;
    }
    removeIndexes(deleted, false);
    send(tag + " OK EXPUNGE completed");
}

namespace {

QString searchString(const QList<Token> &keys, int &pos)
{
    if (pos >= keys.size() || keys[pos].kind == Token::Kind::List)
        throw BadCommand{"Missing search text"};
    return QString::fromUtf8(keys[pos++].text);
}

QByteArray searchAtom(const QList<Token> &keys, int &pos)
{
    if (pos >= keys.size() || keys[pos].kind == Token::Kind::List)
        throw BadCommand{"Missing search argument"};
    return keys[pos++].text;
}

bool headerContains(const Mime::Header &header, const QByteArray &field, const QString &text)
{
    if (!header.contains(field))
        return false;
    if (text.isEmpty())
        return true;
    return Imap::decodeRFC2047String(header.value(field)).contains(text, Qt::CaseInsensitive);
}

using SetTest = std::function<bool(const QByteArray &, bool, int)>;

bool matchKey(const QList<Token> &keys, int &pos, SearchSubject &subject, const SetTest &inSet)
{
    const Token &token = keys[pos++];
    if (token.kind == Token::Kind::List) {
        int inner = 0;
        bool result = true;
        while (inner < token.items.size())
            result = matchKey(token.items, inner, subject, inSet) && result;
        return result;
    }
    const QByteArray key = token.text.toUpper();
    const QStringList &flags = subject.info.flags;
    auto flag = [&flags](const char *name) { return hasFlag(flags, QLatin1String(name)); };

    if (!key.isEmpty() && (std::isdigit(static_cast<unsigned char>(key.at(0))) || key.at(0) == '*'))
        return inSet(key, false, subject.index);
    if (key == "ALL" || key == "OLD")
        return true;
    if (key == "NEW" || key == "RECENT")
        return false;
    if (key == "ANSWERED")
        return flag("\\Answered");
    if (key == "DELETED")
        return flag("\\Deleted");
    if (key == "DRAFT")
        return flag("\\Draft");
    if (key == "FLAGGED")
        return flag("\\Flagged");
    if (key == "SEEN")
        return flag("\\Seen");
    if (key == "UNANSWERED")
        return !flag("\\Answered");
    if (key == "UNDELETED")
        return !flag("\\Deleted");
    if (key == "UNDRAFT")
        return !flag("\\Draft");
    if (key == "UNFLAGGED")
        return !flag("\\Flagged");
    if (key == "UNSEEN")
        return !flag("\\Seen");
    if (key == "KEYWORD")
        return hasFlag(flags, QString::fromUtf8(searchAtom(keys, pos)));
    if (key == "UNKEYWORD")
        return !hasFlag(flags, QString::fromUtf8(searchAtom(keys, pos)));
    if (key == "UID")
        return inSet(searchAtom(keys, pos), true, subject.index);
    if (key == "NOT")
        return !matchKey(keys, pos, subject, inSet);
    if (key == "OR") {
        const bool first = matchKey(keys, pos, subject, inSet);
        const bool second = matchKey(keys, pos, subject, inSet);
        return first || second;
    }
    if (key == "LARGER")
        return subject.info.size > searchAtom(keys, pos).toLongLong();
    if (key == "SMALLER")
        return subject.info.size < searchAtom(keys, pos).toLongLong();
    if (key == "FROM" || key == "TO" || key == "CC" || key == "BCC" || key == "SUBJECT") {
        const QString text = searchString(keys, pos);
        return headerContains(subject.part().header, key.toLower(), text);
    }
    if (key == "HEADER") {
        const QByteArray field = searchAtom(keys, pos).toLower();
        const QString text = searchString(keys, pos);
        return headerContains(subject.part().header, field, text);
    }
    if (key == "BODY" || key == "TEXT") {
        const QString text = searchString(keys, pos);
        const Mime::Part &part = subject.part();
        if (key == "TEXT" && Imap::decodeRFC2047String(part.header.raw()).contains(text, Qt::CaseInsensitive))
            return true;
        return decodedText(part).contains(text, Qt::CaseInsensitive);
    }
    if (key == "SINCE" || key == "BEFORE" || key == "ON" || key == "SENTSINCE" || key == "SENTBEFORE" || key == "SENTON") {
        const QDate wanted = searchDate(searchAtom(keys, pos));
        const bool sent = key.startsWith("SENT");
        const QDateTime when = sent ? Mime::sentDate(subject.part().header) : subject.info.internalDate;
        if (!when.isValid())
            return false;
        const QDate date = sent ? when.date() : when.toUTC().date();
        const QByteArray comparison = sent ? key.mid(4) : key;
        if (comparison == "SINCE")
            return date >= wanted;
        if (comparison == "BEFORE")
            return date < wanted;
        return date == wanted;
    }
    throw BadCommand{"Unknown search key " + key};
}

}

void ImapSession::search(const QByteArray &tag, const QList<Token> &args, bool uid)
{
    QList<Token> keys = args;
    if (keys.size() >= 2 && keys[0].kind == Token::Kind::Atom && keys[0].text.toUpper() == "CHARSET") {
        const QByteArray charset = keys[1].text.toUpper();
        if (charset != "UTF-8" && charset != "US-ASCII") {
            send(tag + " NO [BADCHARSET (UTF-8 US-ASCII)] Unknown charset");
            return;
        }
        keys = keys.mid(2);
    }
    if (keys.isEmpty())
        throw BadCommand{"Missing search criteria"};

    QHash<QByteArray, QList<int>> sets;
    const auto inSet = [this, &sets](const QByteArray &set, bool byUid, int index) {
        const QByteArray key = (byUid ? "U" : "S") + set;
        auto it = sets.find(key);
        if (it == sets.end()) {
            QList<int> indexes;
            if (!resolve(set, byUid, &indexes))
                throw BadCommand{"Cannot read the message set " + set};
            it = sets.insert(key, indexes);
        }
        return std::binary_search(it->constBegin(), it->constEnd(), index);
    };

    const QHash<uint, MessageInfo> messages = currentMessages();
    QByteArrayList found;
    for (int i = 0; i < m_uids.size(); ++i) {
        const auto infoIt = messages.constFind(m_uids[i]);
        if (infoIt == messages.constEnd())
            continue;
        SearchSubject subject;
        subject.index = i;
        subject.info = *infoIt;
        const uint messageUid = m_uids[i];
        subject.load = [this, messageUid]() { return parsed(messageUid); };
        int pos = 0;
        bool result = true;
        while (pos < keys.size())
            result = matchKey(keys, pos, subject, inSet) && result;
        if (result)
            found << QByteArray::number(uid ? messageUid : uint(i + 1));
    }
    send("* SEARCH" + (found.isEmpty() ? QByteArray() : ' ' + found.join(' ')));
    send(tag + " OK SEARCH completed");
}

}
