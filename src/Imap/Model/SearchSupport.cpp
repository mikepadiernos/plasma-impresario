/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "SearchSupport.h"
#include <QHash>

namespace Imap {
namespace Mailbox {

namespace {

struct Token {
    QString text;
    bool atom = true;
};

/** @short Split a raw query into atoms, parentheses and strings */
QList<Token> tokenize(const QString &query)
{
    QList<Token> tokens;
    qsizetype i = 0;
    const qsizetype size = query.size();
    while (i < size) {
        const QChar c = query.at(i);
        if (c.isSpace()) {
            ++i;
        } else if (c == QLatin1Char('(') || c == QLatin1Char(')')) {
            tokens.append({QString(c), true});
            ++i;
        } else if (c == QLatin1Char('"')) {
            QString text;
            ++i;
            while (i < size && query.at(i) != QLatin1Char('"')) {
                if (query.at(i) == QLatin1Char('\\') && i + 1 < size)
                    ++i;
                text += query.at(i++);
            }
            ++i;
            tokens.append({text, false});
        } else if (c == QLatin1Char('{')) {
            // A literal: {n} or {n+}, a line break, then n characters
            const qsizetype close = query.indexOf(QLatin1Char('}'), i);
            if (close < 0)
                break;
            const qsizetype length = QStringView(query).mid(i + 1, close - i - 1).toString().remove(QLatin1Char('+')).toLongLong();
            i = close + 1;
            while (i < size && (query.at(i) == QLatin1Char('\r') || query.at(i) == QLatin1Char('\n')))
                ++i;
            tokens.append({query.mid(i, length), false});
            i += length;
        } else {
            const qsizetype start = i;
            while (i < size && !query.at(i).isSpace() && query.at(i) != QLatin1Char('(') && query.at(i) != QLatin1Char(')')
                   && query.at(i) != QLatin1Char('"'))
                ++i;
            tokens.append({query.mid(start, i - start), true});
        }
    }
    return tokens;
}

bool isSequenceSet(const QString &atom)
{
    for (const QChar c : atom) {
        if (!c.isDigit() && c != QLatin1Char('*') && c != QLatin1Char(':') && c != QLatin1Char(',') && c != QLatin1Char('$'))
            return false;
    }
    return !atom.isEmpty();
}

/** @short How many arguments a key takes; keys missing here are not known */
const QHash<QString, int> &keyArguments()
{
    static const QHash<QString, int> arguments = {
        {QStringLiteral("ALL"), 0}, {QStringLiteral("ANSWERED"), 0}, {QStringLiteral("DELETED"), 0},
        {QStringLiteral("DRAFT"), 0}, {QStringLiteral("FLAGGED"), 0}, {QStringLiteral("NEW"), 0},
        {QStringLiteral("OLD"), 0}, {QStringLiteral("RECENT"), 0}, {QStringLiteral("SEEN"), 0},
        {QStringLiteral("UNANSWERED"), 0}, {QStringLiteral("UNDELETED"), 0}, {QStringLiteral("UNDRAFT"), 0},
        {QStringLiteral("UNFLAGGED"), 0}, {QStringLiteral("UNSEEN"), 0},
        {QStringLiteral("BCC"), 1}, {QStringLiteral("BEFORE"), 1}, {QStringLiteral("BODY"), 1},
        {QStringLiteral("CC"), 1}, {QStringLiteral("FROM"), 1}, {QStringLiteral("KEYWORD"), 1},
        {QStringLiteral("LARGER"), 1}, {QStringLiteral("ON"), 1}, {QStringLiteral("SENTBEFORE"), 1},
        {QStringLiteral("SENTON"), 1}, {QStringLiteral("SENTSINCE"), 1}, {QStringLiteral("SINCE"), 1},
        {QStringLiteral("SMALLER"), 1}, {QStringLiteral("SUBJECT"), 1}, {QStringLiteral("TEXT"), 1},
        {QStringLiteral("TO"), 1}, {QStringLiteral("UID"), 1}, {QStringLiteral("UNKEYWORD"), 1},
        {QStringLiteral("HEADER"), 2},
        {QStringLiteral("OLDER"), 1}, {QStringLiteral("YOUNGER"), 1},
        {QStringLiteral("SAVEDBEFORE"), 1}, {QStringLiteral("SAVEDON"), 1}, {QStringLiteral("SAVEDSINCE"), 1},
        {QStringLiteral("SAVEDATESUPPORTED"), 0},
        {QStringLiteral("EMAILID"), 1}, {QStringLiteral("THREADID"), 1},
        {QStringLiteral("FILTER"), 1}, {QStringLiteral("ANNOTATION"), 3},
        {QStringLiteral("X-GM-RAW"), 1}, {QStringLiteral("X-GM-MSGID"), 1}, {QStringLiteral("X-GM-THRID"), 1},
        {QStringLiteral("X-GM-LABELS"), 1},
    };
    return arguments;
}

/** @short The capability which a key needs, for keys outside IMAP4rev1 */
const QHash<QString, QString> &keyCapabilities()
{
    static const QHash<QString, QString> capabilities = {
        {QStringLiteral("FUZZY"), QStringLiteral("SEARCH=FUZZY")},
        {QStringLiteral("OLDER"), QStringLiteral("WITHIN")},
        {QStringLiteral("YOUNGER"), QStringLiteral("WITHIN")},
        {QStringLiteral("MODSEQ"), QStringLiteral("CONDSTORE")},
        {QStringLiteral("SAVEDBEFORE"), QStringLiteral("SAVEDATE")},
        {QStringLiteral("SAVEDON"), QStringLiteral("SAVEDATE")},
        {QStringLiteral("SAVEDSINCE"), QStringLiteral("SAVEDATE")},
        {QStringLiteral("SAVEDATESUPPORTED"), QStringLiteral("SAVEDATE")},
        {QStringLiteral("EMAILID"), QStringLiteral("OBJECTID")},
        {QStringLiteral("THREADID"), QStringLiteral("OBJECTID")},
        {QStringLiteral("FILTER"), QStringLiteral("FILTERS")},
        {QStringLiteral("ANNOTATION"), QStringLiteral("ANNOTATE-EXPERIMENT-1")},
        {QStringLiteral("X-GM-RAW"), QStringLiteral("X-GM-EXT-1")},
        {QStringLiteral("X-GM-MSGID"), QStringLiteral("X-GM-EXT-1")},
        {QStringLiteral("X-GM-THRID"), QStringLiteral("X-GM-EXT-1")},
        {QStringLiteral("X-GM-LABELS"), QStringLiteral("X-GM-EXT-1")},
    };
    return capabilities;
}

bool advertises(const QStringList &capabilities, const QString &capability)
{
    if (capabilities.contains(capability, Qt::CaseInsensitive))
        return true;
    // QRESYNC implies CONDSTORE (RFC 7162)
    return capability == QLatin1String("CONDSTORE") && capabilities.contains(QStringLiteral("QRESYNC"), Qt::CaseInsensitive);
}

QString withDetail(const QString &sentence, const QString &serverText)
{
    const QString detail = serverText.trimmed();
    if (detail.isEmpty())
        return sentence;
    return SearchSupport::tr("%1 The server said: %2").arg(sentence, detail);
}

}

QList<SearchSupport::MissingKey> SearchSupport::unsupportedKeys(const QStringList &conditions, const QStringList &capabilities)
{
    QList<Token> tokens;
    if (conditions.size() == 1) {
        tokens = tokenize(conditions.front());
    } else {
        for (const QString &condition : conditions)
            tokens.append({condition, true});
    }

    QList<MissingKey> missing;
    const auto &arguments = keyArguments();
    const auto &needs = keyCapabilities();
    qsizetype i = 0;
    while (i < tokens.size()) {
        const Token &token = tokens.at(i++);
        if (!token.atom)
            break;
        const QString key = token.text.toUpper();
        if (key == QLatin1String("(") || key == QLatin1String(")"))
            continue;
        const auto need = needs.constFind(key);
        if (need != needs.constEnd() && !advertises(capabilities, *need)) {
            const MissingKey entry{key, *need};
            if (!missing.contains(entry))
                missing.append(entry);
        }
        if (key == QLatin1String("NOT") || key == QLatin1String("OR") || key == QLatin1String("FUZZY") || isSequenceSet(key))
            continue;
        if (key == QLatin1String("MODSEQ")) {
            // MODSEQ [entry-name entry-type-req] mod-sequence-valzer
            i += (i < tokens.size() && !tokens.at(i).atom) ? 3 : 1;
            continue;
        }
        const auto count = arguments.constFind(key);
        if (count == arguments.constEnd())
            break;
        i += *count;
    }
    return missing;
}

bool SearchSupport::needsUnicode(const QStringList &conditions)
{
    for (const QString &condition : conditions) {
        for (const QChar c : condition) {
            if (c.unicode() > 0x7f)
                return true;
        }
    }
    return false;
}

QString SearchSupport::explainUnsupported(const QList<MissingKey> &missing)
{
    QStringList parts;
    for (const MissingKey &entry : missing) {
        if (entry.key == QLatin1String("FUZZY"))
            parts << tr("fuzzy matching (FUZZY needs %1)").arg(entry.capability);
        else
            parts << tr("%1 (needs %2)").arg(entry.key, entry.capability);
    }
    return tr("The server does not support part of this search: %1. Leave it out, or search in another way.")
        .arg(parts.join(tr(", ")));
}

QString SearchSupport::explainAsciiOnly()
{
    return tr("This server can only search for plain ASCII text. Search for words without accents or other "
              "non-ASCII letters.");
}

QString SearchSupport::explainRejection(const Responses::Kind kind, const Responses::Code code, const QStringList &charsets,
                                        const QString &serverText, const bool rawQuery)
{
    switch (code) {
    case Responses::BADCHARSET:
        if (charsets.isEmpty())
            return tr("The server cannot search for text in UTF-8. Search for words in plain ASCII instead.");
        return tr("The server cannot search for text in UTF-8; it accepts %1. Search for words in plain ASCII instead.")
            .arg(charsets.join(tr(", ")));
    case Responses::CANNOT:
        return withDetail(tr("The server cannot run this search."), serverText);
    case Responses::LIMIT:
        return withDetail(tr("The search goes beyond a limit of the server."), serverText);
    case Responses::UNAVAILABLE:
        return withDetail(tr("The server cannot search at the moment; try again later."), serverText);
    default:
        break;
    }
    if (kind == Responses::BAD) {
        return withDetail(rawQuery ? tr("The server did not understand the IMAP search query. Check its keys and quoting.")
                                   : tr("The server did not understand the search."),
                          serverText);
    }
    return withDetail(tr("The server refused the search."), serverText);
}

}
}
