/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QCoreApplication>
#include <QDate>
#include <QString>
#include <QStringList>
#include <vector>

#include "SearchScope.h"

/** @short The quick search's query syntax

A query combines terms with AND, OR, NOT and parentheses. Words without a field search the fields chosen in the search
options; a field such as from:, subject: or tag: searches that field only. Terms next to each other must all match,
and AND binds more tightly than OR. Operators are recognized only in upper case, and a minus sign in front of a term
negates it; quote a value to search for an operator, a phrase or text with parentheses. Inside quotes, \" stands for
a quotation mark and \\ for a backslash.

The syntax follows the advanced search of Plasma Opus where it fits mail: field names before a colon, the comparisons
:, =, <, <=, > and >=, and the dates YYYY-MM-DD, YYYY-MM, YYYY, today and yesterday. Field names are not translated.

A query is parsed and checked completely before anything is sent, so an invalid query names the problem and the text
it is in instead of reaching the server. A valid query is translated to IMAP SEARCH keys, which every IMAP4rev1
server supports except FUZZY.
*/
namespace UiUtils {
namespace SearchQuery {

enum class Field {
    /** Words without a field: the fields chosen in the search options */
    Text,
    From,
    To,
    Cc,
    Bcc,
    Subject,
    Body,
    /** The whole message, headers and body */
    Message,
    /** A mailing list: its List-Id or the address of its List-Post header */
    List,
    Has,
    Is,
    Tag,
    Date,
    Size,
};

enum class Comparison {
    Contains,
    Equal,
    Less,
    LessOrEqual,
    Greater,
    GreaterOrEqual,
};

struct Term {
    Field field = Field::Text;
    Comparison comparison = Comparison::Contains;
    /** The value as written, without quotes; for has: and is: the value in lower case */
    QString value;
    bool quoted = false;
    /** First day and the day after the last of a date term */
    QDate from;
    QDate to;
    /** Bytes of a size term */
    qint64 size = 0;
    /** Where the term is in the query text */
    int position = 0;
    int length = 0;
};

struct Node {
    enum class Kind {
        Term,
        And,
        Or,
        Not,
    };
    Kind kind = Kind::Term;
    Term term;
    std::vector<Node> children;
};

/** @short Why a query cannot be used, and the text it is about */
struct Error {
    QString message;
    /** Start and length of the text in the query, or -1 when the problem is not in one place */
    int position = -1;
    int length = 0;

    bool isEmpty() const { return message.isEmpty(); }
};

class Query
{
    Q_DECLARE_TR_FUNCTIONS(UiUtils::SearchQuery::Query)
public:
    /** @short Parse @arg text; @arg today resolves the dates today and yesterday */
    static Query parse(const QString &text, const QDate &today);

    /** @short Whether the text has no terms at all */
    bool isEmpty() const;
    bool isValid() const;
    Error error() const;
    const Node &root() const;
    /** @short Whether a date term depends on the day, so the search has to be applied again when the day changes */
    bool usesRelativeDates() const;

    /** @short The query in a normal form, with explicit operators and parentheses, such as
    "from:ann AND (subject:\"release plan\" OR NOT is:read)" */
    QString toString() const;

    /** @short IMAP SEARCH keys for the query, one item per key or value

    Words without a field search @arg textFields, with FUZZY when @arg fuzzy is set. IMAP needs parentheses to group
    keys inside OR and NOT, which a list of separately quoted items cannot carry, so a group is written with OR and NOT
    alone: "a AND b" becomes NOT OR NOT a NOT b. When the query cannot be translated, the result is empty and
    @arg problem says why.
    */
    QStringList imapConditions(SearchScope::Fields textFields, bool fuzzy, Error *problem = nullptr) const;

private:
    Node m_root;
    Error m_error;
    bool m_empty = true;
    bool m_relativeDates = false;
};

/** @short @arg value as the query syntax needs it: quoted and escaped if it would otherwise not read as one value */
QString quotedValue(const QString &value);

/** @short The name of @arg field before the colon, such as "from"; empty for Field::Text */
QString fieldName(Field field);

}
}
