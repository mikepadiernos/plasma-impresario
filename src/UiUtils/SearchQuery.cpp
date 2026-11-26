/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "SearchQuery.h"

#include <QHash>
#include <QRegularExpression>
#include <cmath>
#include <optional>

#include "MailingLists.h"

namespace UiUtils {
namespace SearchQuery {

namespace {

struct Token {
    enum class Kind {
        Word,
        Open,
        Close,
        And,
        Or,
        Not,
    };
    Kind kind = Kind::Word;
    /** The field name as written, empty for a plain word */
    QString field;
    /** Comparison operator characters after the field, without the colon */
    QString op;
    bool colon = false;
    QString value;
    bool quoted = false;
    int position = 0;
    int length = 0;
};

QString tr(const char *text)
{
    return Query::tr(text);
}

bool isBreak(const QChar c)
{
    return c.isSpace() || c == QLatin1Char('(') || c == QLatin1Char(')');
}

bool isOperatorChar(const QChar c)
{
    return c == QLatin1Char('<') || c == QLatin1Char('>') || c == QLatin1Char('=');
}

Error errorAt(const QString &message, const int position, const int length)
{
    return Error{message, position, length};
}

Error tokenize(const QString &text, QList<Token> &tokens)
{
    const int length = text.size();
    int i = 0;
    while (i < length) {
        const QChar c = text.at(i);
        if (c.isSpace()) {
            ++i;
            continue;
        }
        if (c == QLatin1Char('(') || c == QLatin1Char(')')) {
            Token token;
            token.kind = c == QLatin1Char('(') ? Token::Kind::Open : Token::Kind::Close;
            token.value = c;
            token.position = i;
            token.length = 1;
            tokens.append(token);
            ++i;
            continue;
        }
        // A minus sign negates the term or group it is attached to
        if (c == QLatin1Char('-') && i + 1 < length && !text.at(i + 1).isSpace() && text.at(i + 1) != QLatin1Char(')')) {
            Token token;
            token.kind = Token::Kind::Not;
            token.value = c;
            token.position = i;
            token.length = 1;
            tokens.append(token);
            ++i;
            continue;
        }

        const int start = i;
        Token token;
        token.position = start;
        // A field is a run of letters followed by a colon or a comparison
        int end = i;
        while (end < length && text.at(end).isLetter())
            ++end;
        if (end > i && end < length && (text.at(end) == QLatin1Char(':') || isOperatorChar(text.at(end)))) {
            token.field = text.mid(i, end - i);
            i = end;
            if (text.at(i) == QLatin1Char(':')) {
                token.colon = true;
                ++i;
            }
            const int opStart = i;
            while (i < length && isOperatorChar(text.at(i)))
                ++i;
            token.op = text.mid(opStart, i - opStart);
        }
        if (i < length && text.at(i) == QLatin1Char('"')) {
            QString value;
            int j = i + 1;
            bool closed = false;
            while (j < length) {
                const QChar d = text.at(j);
                if (d == QLatin1Char('\\') && j + 1 < length
                    && (text.at(j + 1) == QLatin1Char('"') || text.at(j + 1) == QLatin1Char('\\'))) {
                    value += text.at(j + 1);
                    j += 2;
                    continue;
                }
                if (d == QLatin1Char('"')) {
                    closed = true;
                    break;
                }
                value += d;
                ++j;
            }
            if (!closed)
                return errorAt(tr("A quotation mark is not closed: %1").arg(text.mid(start)), start, length - start);
            token.value = value;
            token.quoted = true;
            i = j + 1;
            if (i < length && !isBreak(text.at(i))) {
                return errorAt(tr("Separate a quoted value from the following text with a space: %1").arg(text.mid(start, i - start + 1)),
                               start, i - start + 1);
            }
        } else {
            const int valueStart = i;
            while (i < length && !isBreak(text.at(i))) {
                if (text.at(i) == QLatin1Char('"')) {
                    return errorAt(tr("Quote a whole value, not part of a word: %1").arg(text.mid(start, i - start + 1)),
                                   start, i - start + 1);
                }
                ++i;
            }
            token.value = text.mid(valueStart, i - valueStart);
        }
        token.length = i - start;
        if (token.field.isEmpty() && !token.quoted) {
            if (token.value == QLatin1String("AND"))
                token.kind = Token::Kind::And;
            else if (token.value == QLatin1String("OR"))
                token.kind = Token::Kind::Or;
            else if (token.value == QLatin1String("NOT"))
                token.kind = Token::Kind::Not;
        }
        tokens.append(token);
    }
    return {};
}

const QHash<QString, Field> &fieldNames()
{
    static const QHash<QString, Field> names = {
        {QStringLiteral("from"), Field::From},
        {QStringLiteral("to"), Field::To},
        {QStringLiteral("cc"), Field::Cc},
        {QStringLiteral("bcc"), Field::Bcc},
        {QStringLiteral("subject"), Field::Subject},
        {QStringLiteral("body"), Field::Body},
        {QStringLiteral("text"), Field::Message},
        {QStringLiteral("list"), Field::List},
        {QStringLiteral("has"), Field::Has},
        {QStringLiteral("is"), Field::Is},
        {QStringLiteral("tag"), Field::Tag},
        {QStringLiteral("date"), Field::Date},
        {QStringLiteral("size"), Field::Size},
    };
    return names;
}

std::optional<Comparison> comparisonFor(const Token &token)
{
    const QString &op = token.op;
    if (op.isEmpty())
        return token.colon ? std::optional(Comparison::Contains) : std::nullopt;
    if (op == QLatin1String("="))
        return Comparison::Equal;
    if (op == QLatin1String("<"))
        return Comparison::Less;
    if (op == QLatin1String("<="))
        return Comparison::LessOrEqual;
    if (op == QLatin1String(">"))
        return Comparison::Greater;
    if (op == QLatin1String(">="))
        return Comparison::GreaterOrEqual;
    return std::nullopt;
}

QString comparisonText(const Comparison comparison)
{
    switch (comparison) {
    case Comparison::Contains:
        return QStringLiteral(":");
    case Comparison::Equal:
        return QStringLiteral("=");
    case Comparison::Less:
        return QStringLiteral("<");
    case Comparison::LessOrEqual:
        return QStringLiteral("<=");
    case Comparison::Greater:
        return QStringLiteral(">");
    case Comparison::GreaterOrEqual:
        return QStringLiteral(">=");
    }
    return QString();
}

std::optional<qint64> parseSize(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral("^(\\d+(?:\\.\\d+)?)([kmg]i?b?|b)?$"),
                                            QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = pattern.match(text);
    if (!match.hasMatch())
        return std::nullopt;
    double value = match.captured(1).toDouble();
    const QString unit = match.captured(2).toLower();
    if (!unit.isEmpty() && unit != QLatin1String("b"))
        value *= std::pow(1024.0, QStringLiteral("kmg").indexOf(unit.at(0)) + 1);
    // IMAP numbers are 32 bits wide
    if (value > 4294967295.0)
        return std::nullopt;
    return static_cast<qint64>(std::llround(value));
}

bool parsePeriod(const QString &text, const QDate &today, QDate &from, QDate &to, bool &relative)
{
    static const QRegularExpression day(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}$"));
    static const QRegularExpression month(QStringLiteral("^\\d{4}-\\d{2}$"));
    static const QRegularExpression year(QStringLiteral("^\\d{4}$"));
    const QString key = text.toLower();
    if (key == QLatin1String("today") || key == QLatin1String("yesterday")) {
        from = today.addDays(key == QLatin1String("today") ? 0 : -1);
        to = from.addDays(1);
        relative = true;
    } else if (day.match(text).hasMatch()) {
        from = QDate::fromString(text, Qt::ISODate);
        to = from.addDays(1);
    } else if (month.match(text).hasMatch()) {
        from = QDate(text.left(4).toInt(), text.mid(5, 2).toInt(), 1);
        to = from.addMonths(1);
    } else if (year.match(text).hasMatch()) {
        from = QDate(text.toInt(), 1, 1);
        to = from.addYears(1);
    }
    return from.isValid() && to.isValid();
}

/** @short Whether @arg value can be sent as an IMAP flag keyword, which has to be an atom */
bool isKeyword(const QString &value)
{
    if (value.isEmpty())
        return false;
    for (const QChar c : value) {
        if (c.unicode() <= 0x20 || c.unicode() >= 0x7f)
            return false;
        if (QStringView(u"(){%*\"\\]").contains(c))
            return false;
    }
    return true;
}

const QHash<QString, QString> &isValues()
{
    static const QHash<QString, QString> values = {
        {QStringLiteral("unread"), QStringLiteral("unread")},
        {QStringLiteral("unseen"), QStringLiteral("unread")},
        {QStringLiteral("read"), QStringLiteral("read")},
        {QStringLiteral("seen"), QStringLiteral("read")},
        {QStringLiteral("flagged"), QStringLiteral("flagged")},
        {QStringLiteral("starred"), QStringLiteral("flagged")},
        {QStringLiteral("unflagged"), QStringLiteral("unflagged")},
        {QStringLiteral("answered"), QStringLiteral("answered")},
        {QStringLiteral("replied"), QStringLiteral("answered")},
        {QStringLiteral("unanswered"), QStringLiteral("unanswered")},
        {QStringLiteral("draft"), QStringLiteral("draft")},
        {QStringLiteral("deleted"), QStringLiteral("deleted")},
        {QStringLiteral("forwarded"), QStringLiteral("forwarded")},
    };
    return values;
}

Error buildTerm(const Token &token, const QDate &today, Term &term, bool &relative)
{
    term.value = token.value;
    term.quoted = token.quoted;
    term.position = token.position;
    term.length = token.length;
    const QString source = token.field + (token.colon ? QStringLiteral(":") : QString()) + token.op;

    if (token.field.isEmpty()) {
        if (token.value.isEmpty())
            return errorAt(tr("Enter text inside the quotation marks."), token.position, token.length);
        term.field = Field::Text;
        return {};
    }

    const auto field = fieldNames().constFind(token.field.toLower());
    if (field == fieldNames().constEnd()) {
        // "Re:" or "Note:" followed by a space is text, not a field
        if (token.colon && token.op.isEmpty() && token.value.isEmpty() && !token.quoted) {
            term.field = Field::Text;
            term.value = token.field + QLatin1Char(':');
            return {};
        }
        return errorAt(tr("Unknown field \u201C%1\u201D. Use from, to, cc, bcc, subject, body, text, list, has, is, tag, "
                          "date or size, or put the text in quotation marks to search for it.")
                           .arg(token.field),
                       token.position, token.length);
    }
    term.field = *field;

    const std::optional<Comparison> comparison = comparisonFor(token);
    if (!comparison) {
        return errorAt(tr("Unknown comparison \u201C%1\u201D in %2. Use :, =, <, <=, > or >=.").arg(token.op, source),
                       token.position, token.length);
    }
    term.comparison = *comparison;

    if (token.value.isEmpty() && !token.quoted)
        return errorAt(tr("Enter a value after %1").arg(source), token.position, token.length);
    if (token.value.isEmpty())
        return errorAt(tr("Enter text inside the quotation marks."), token.position, token.length);

    const bool ordered = term.comparison != Comparison::Contains && term.comparison != Comparison::Equal;
    switch (term.field) {
    case Field::Date:
        if (!parsePeriod(token.value, today, term.from, term.to, relative)) {
            return errorAt(tr("\u201C%1\u201D is not a valid date. Use YYYY-MM-DD, YYYY-MM, YYYY, today or yesterday.").arg(token.value),
                           token.position, token.length);
        }
        return {};
    case Field::Size: {
        if (!ordered) {
            return errorAt(tr("Size criteria compare with <, <=, > or >=, such as size>2M: %1").arg(token.value),
                           token.position, token.length);
        }
        const std::optional<qint64> size = parseSize(token.value);
        if (!size) {
            return errorAt(tr("\u201C%1\u201D is not a valid size. Use a number with an optional unit, such as 500K, 10M or 2G.").arg(token.value),
                           token.position, token.length);
        }
        term.size = *size;
        return {};
    }
    default:
        break;
    }

    if (term.comparison != Comparison::Contains) {
        return errorAt(tr("Only date and size criteria can use =, <, <=, > or >=: %1").arg(source + token.value),
                       token.position, token.length);
    }
    switch (term.field) {
    case Field::Has:
        if (token.value.compare(QLatin1String("attachment"), Qt::CaseInsensitive) != 0
            && token.value.compare(QLatin1String("attachments"), Qt::CaseInsensitive) != 0) {
            return errorAt(tr("\u201C%1\u201D is not a valid has: value. Use has:attachment.").arg(token.value), token.position,
                           token.length);
        }
        term.value = QStringLiteral("attachment");
        return {};
    case Field::Is: {
        const auto value = isValues().constFind(token.value.toLower());
        if (value == isValues().constEnd()) {
            return errorAt(tr("\u201C%1\u201D is not a valid is: value. Use unread, read, flagged, unflagged, answered, "
                              "unanswered, draft, deleted or forwarded.")
                               .arg(token.value),
                           token.position, token.length);
        }
        term.value = *value;
        return {};
    }
    case Field::Tag:
        if (!isKeyword(token.value)) {
            return errorAt(tr("\u201C%1\u201D is not a valid tag name. Tag names are plain ASCII without spaces or any of "
                              "( ) { %  * \" \\ ].")
                               .arg(token.value),
                           token.position, token.length);
        }
        return {};
    default:
        return {};
    }
}

class Parser
{
public:
    Parser(const QList<Token> &tokens, const QDate &today)
        : m_tokens(tokens)
        , m_today(today)
    {
    }

    Error parse(Node &root)
    {
        if (Error error = parseOr(root); !error.isEmpty())
            return error;
        if (const Token *token = peek())
            return errorAt(tr("A closing parenthesis has no matching opening parenthesis."), token->position, token->length);
        return {};
    }

    bool relative = false;

private:
    const Token *peek() const
    {
        return m_position < m_tokens.size() ? &m_tokens.at(m_position) : nullptr;
    }

    static void append(Node &parent, Node &&child)
    {
        if (child.kind == parent.kind) {
            for (Node &grandchild : child.children)
                parent.children.push_back(std::move(grandchild));
        } else {
            parent.children.push_back(std::move(child));
        }
    }

    static void combine(Node &node, const Node::Kind kind, Node &&right)
    {
        if (node.kind != kind) {
            Node left = std::move(node);
            node = Node{kind, {}, {}};
            append(node, std::move(left));
        }
        append(node, std::move(right));
    }

    Error parseOr(Node &node)
    {
        if (Error error = parseAnd(node, nullptr); !error.isEmpty())
            return error;
        while (peek() && peek()->kind == Token::Kind::Or) {
            const Token *op = peek();
            ++m_position;
            Node right;
            if (Error error = parseAnd(right, op); !error.isEmpty())
                return error;
            combine(node, Node::Kind::Or, std::move(right));
        }
        return {};
    }

    Error parseAnd(Node &node, const Token *after)
    {
        if (Error error = parseUnary(node, after); !error.isEmpty())
            return error;
        while (const Token *token = peek()) {
            if (token->kind == Token::Kind::Or || token->kind == Token::Kind::Close)
                break;
            const Token *op = nullptr;
            if (token->kind == Token::Kind::And) {
                op = token;
                ++m_position;
            }
            Node right;
            if (Error error = parseUnary(right, op); !error.isEmpty())
                return error;
            combine(node, Node::Kind::And, std::move(right));
        }
        return {};
    }

    Error parseUnary(Node &node, const Token *after)
    {
        const Token *token = peek();
        if (token && token->kind == Token::Kind::Not) {
            ++m_position;
            Node child;
            if (Error error = parseUnary(child, token); !error.isEmpty())
                return error;
            if (child.kind == Node::Kind::Not) {
                node = std::move(child.children.front());
            } else {
                node = Node{Node::Kind::Not, {}, {}};
                node.children.push_back(std::move(child));
            }
            return {};
        }
        return parsePrimary(node, after);
    }

    Error missingAfter(const Token *after) const
    {
        if (!after)
            return errorAt(tr("Enter text to search for."), -1, 0);
        return errorAt(tr("Add a search term after %1").arg(after->value), after->position, after->length);
    }

    Error parsePrimary(Node &node, const Token *after)
    {
        const Token *token = peek();
        if (!token)
            return missingAfter(after);
        switch (token->kind) {
        case Token::Kind::Open: {
            ++m_position;
            if (peek() && peek()->kind == Token::Kind::Close)
                return errorAt(tr("Parentheses must contain a search term."), token->position, peek()->position - token->position + 1);
            if (Error error = parseOr(node); !error.isEmpty())
                return error;
            if (!peek() || peek()->kind != Token::Kind::Close)
                return errorAt(tr("An opening parenthesis is not closed."), token->position, token->length);
            ++m_position;
            return {};
        }
        case Token::Kind::Close:
            if (after)
                return missingAfter(after);
            return errorAt(tr("A closing parenthesis has no matching opening parenthesis."), token->position, token->length);
        case Token::Kind::And:
        case Token::Kind::Or:
            return errorAt(tr("%1 must be placed between two search terms. Quote it to search for the word.").arg(token->value),
                           token->position, token->length);
        case Token::Kind::Not:
        case Token::Kind::Word:
            break;
        }
        ++m_position;
        node = Node{};
        return buildTerm(*token, m_today, node.term, relative);
    }

    const QList<Token> &m_tokens;
    const QDate m_today;
    qsizetype m_position = 0;
};

QString termText(const Term &term)
{
    if (term.field == Field::Text)
        return quotedValue(term.value);
    const QString name = fieldName(term.field);
    switch (term.field) {
    case Field::Date:
    case Field::Size:
        return name + comparisonText(term.comparison) + term.value;
    default:
        return name + QLatin1Char(':') + quotedValue(term.value);
    }
}

QString nodeText(const Node &node)
{
    switch (node.kind) {
    case Node::Kind::Term:
        return termText(node.term);
    case Node::Kind::Not: {
        const Node &child = node.children.front();
        const QString text = nodeText(child);
        return QStringLiteral("NOT ") + (child.kind == Node::Kind::Term || child.kind == Node::Kind::Not ? text : QLatin1Char('(') + text + QLatin1Char(')'));
    }
    case Node::Kind::And:
    case Node::Kind::Or: {
        QStringList parts;
        for (const Node &child : node.children) {
            const QString text = nodeText(child);
            // AND binds more tightly than OR, so only an OR inside an AND needs parentheses
            parts << (node.kind == Node::Kind::And && child.kind == Node::Kind::Or ? QLatin1Char('(') + text + QLatin1Char(')') : text);
        }
        return parts.join(node.kind == Node::Kind::And ? QStringLiteral(" AND ") : QStringLiteral(" OR "));
    }
    }
    return QString();
}

/** Keys which must all match; each key is one IMAP search-key, as items */
using Keys = QList<QStringList>;

QStringList negated(const QStringList &key)
{
    // Every NOT made here is followed by exactly one key
    if (!key.isEmpty() && key.front() == QLatin1String("NOT"))
        return key.mid(1);
    return QStringList{QStringLiteral("NOT")} + key;
}

QStringList anyOf(const QList<QStringList> &alternatives)
{
    if (alternatives.size() == 1)
        return alternatives.front();
    QStringList result;
    // OR takes two operands in prefix notation, so n alternatives need n - 1 of them in front
    for (qsizetype i = 1; i < alternatives.size(); ++i)
        result << QStringLiteral("OR");
    for (const QStringList &alternative : alternatives)
        result += alternative;
    return result;
}

/** @short One key which matches when all @arg keys match: NOT OR NOT a NOT b */
QStringList allOf(const Keys &keys)
{
    if (keys.size() == 1)
        return keys.front();
    QList<QStringList> alternatives;
    for (const QStringList &key : keys)
        alternatives << negated(key);
    return negated(anyOf(alternatives));
}

struct Translator {
    SearchScope::Fields textFields;
    bool fuzzy = false;
    Error problem;

    Keys termKeys(const Term &term)
    {
        const auto key = [&](const QString &name) { return Keys{QStringList{name, term.value}}; };
        switch (term.field) {
        case Field::Text: {
            const QStringList keys = SearchScope::textConditions(term.value, textFields, fuzzy);
            if (keys.isEmpty()) {
                problem = errorAt(Query::tr("Choose a field to search in the search options, or name one, such as subject:%1")
                                      .arg(quotedValue(term.value)),
                                  term.position, term.length);
                return {};
            }
            return Keys{keys};
        }
        case Field::From:
            return key(QStringLiteral("FROM"));
        case Field::To:
            return key(QStringLiteral("TO"));
        case Field::Cc:
            return key(QStringLiteral("CC"));
        case Field::Bcc:
            return key(QStringLiteral("BCC"));
        case Field::Subject:
            return key(QStringLiteral("SUBJECT"));
        case Field::Body:
            return key(QStringLiteral("BODY"));
        case Field::Message:
            return key(QStringLiteral("TEXT"));
        case Field::List:
            return Keys{MailingLists::searchKeys(term.value)};
        case Field::Has:
            // IMAP cannot ask for attachments; most messages with one are multipart/mixed
            return Keys{QStringList{QStringLiteral("HEADER"), QStringLiteral("Content-Type"), QStringLiteral("multipart/mixed")}};
        case Field::Is: {
            static const QHash<QString, QStringList> flags = {
                {QStringLiteral("unread"), {QStringLiteral("UNSEEN")}},
                {QStringLiteral("read"), {QStringLiteral("SEEN")}},
                {QStringLiteral("flagged"), {QStringLiteral("FLAGGED")}},
                {QStringLiteral("unflagged"), {QStringLiteral("UNFLAGGED")}},
                {QStringLiteral("answered"), {QStringLiteral("ANSWERED")}},
                {QStringLiteral("unanswered"), {QStringLiteral("UNANSWERED")}},
                {QStringLiteral("draft"), {QStringLiteral("DRAFT")}},
                {QStringLiteral("deleted"), {QStringLiteral("DELETED")}},
                {QStringLiteral("forwarded"), {QStringLiteral("KEYWORD"), QStringLiteral("$Forwarded")}},
            };
            return Keys{flags.value(term.value)};
        }
        case Field::Tag:
            return key(QStringLiteral("KEYWORD"));
        case Field::Date: {
            const QString since = QStringLiteral("SENTSINCE");
            const QString before = QStringLiteral("SENTBEFORE");
            const QString first = SearchScope::imapDate(term.from);
            const QString after = SearchScope::imapDate(term.to);
            switch (term.comparison) {
            case Comparison::Contains:
            case Comparison::Equal:
                return Keys{{since, first}, {before, after}};
            case Comparison::Less:
                return Keys{{before, first}};
            case Comparison::LessOrEqual:
                return Keys{{before, after}};
            case Comparison::Greater:
                return Keys{{since, after}};
            case Comparison::GreaterOrEqual:
                return Keys{{since, first}};
            }
            return {};
        }
        case Field::Size: {
            const QString larger = QStringLiteral("LARGER");
            const QString smaller = QStringLiteral("SMALLER");
            switch (term.comparison) {
            case Comparison::Greater:
                return Keys{{larger, QString::number(term.size)}};
            case Comparison::GreaterOrEqual:
                return term.size > 0 ? Keys{{larger, QString::number(term.size - 1)}} : Keys{{QStringLiteral("ALL")}};
            case Comparison::Less:
                return term.size > 0 ? Keys{{smaller, QString::number(term.size)}} : Keys{{QStringLiteral("NOT"), QStringLiteral("ALL")}};
            case Comparison::LessOrEqual:
                return term.size < 4294967295LL ? Keys{{smaller, QString::number(term.size + 1)}} : Keys{{QStringLiteral("ALL")}};
            default:
                return {};
            }
        }
        }
        return {};
    }

    Keys keys(const Node &node)
    {
        switch (node.kind) {
        case Node::Kind::Term:
            return termKeys(node.term);
        case Node::Kind::And: {
            Keys result;
            for (const Node &child : node.children) {
                result += keys(child);
                if (!problem.isEmpty())
                    return {};
            }
            return result;
        }
        case Node::Kind::Or: {
            QList<QStringList> alternatives;
            for (const Node &child : node.children) {
                const Keys childKeys = keys(child);
                if (!problem.isEmpty())
                    return {};
                alternatives << allOf(childKeys);
            }
            return Keys{anyOf(alternatives)};
        }
        case Node::Kind::Not: {
            const Keys childKeys = keys(node.children.front());
            if (!problem.isEmpty())
                return {};
            return Keys{negated(allOf(childKeys))};
        }
        }
        return {};
    }
};

}

Query Query::parse(const QString &text, const QDate &today)
{
    Query query;
    QList<Token> tokens;
    query.m_error = tokenize(text, tokens);
    if (!query.m_error.isEmpty()) {
        query.m_empty = false;
        return query;
    }
    if (tokens.isEmpty())
        return query;
    query.m_empty = false;
    Parser parser(tokens, today);
    query.m_error = parser.parse(query.m_root);
    query.m_relativeDates = parser.relative;
    if (!query.m_error.isEmpty())
        query.m_root = Node{};
    return query;
}

bool Query::isEmpty() const
{
    return m_empty;
}

bool Query::isValid() const
{
    return m_error.isEmpty();
}

Error Query::error() const
{
    return m_error;
}

const Node &Query::root() const
{
    return m_root;
}

bool Query::usesRelativeDates() const
{
    return m_relativeDates;
}

QString Query::toString() const
{
    if (m_empty || !isValid())
        return QString();
    return nodeText(m_root);
}

QStringList Query::imapConditions(const SearchScope::Fields textFields, const bool fuzzy, Error *problem) const
{
    if (problem)
        *problem = m_error;
    if (m_empty || !isValid())
        return QStringList();
    Translator translator{textFields, fuzzy, {}};
    const Keys keys = translator.keys(m_root);
    if (!translator.problem.isEmpty()) {
        if (problem)
            *problem = translator.problem;
        return QStringList();
    }
    QStringList result;
    for (const QStringList &key : keys)
        result += key;
    return result;
}

QString quotedValue(const QString &value)
{
    bool needsQuotes = value.isEmpty() || value.front() == QLatin1Char('-') || value == QLatin1String("AND")
        || value == QLatin1String("OR") || value == QLatin1String("NOT");
    for (const QChar c : value) {
        if (c.isSpace() || c == QLatin1Char('(') || c == QLatin1Char(')') || c == QLatin1Char('"') || c == QLatin1Char('\\')
            || c == QLatin1Char(':') || isOperatorChar(c)) {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes)
        return value;
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    escaped.replace(QLatin1Char('"'), QLatin1String("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QString fieldName(const Field field)
{
    for (auto it = fieldNames().constBegin(); it != fieldNames().constEnd(); ++it) {
        if (it.value() == field)
            return it.key();
    }
    return QString();
}

}
}
