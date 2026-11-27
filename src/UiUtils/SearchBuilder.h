/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef UIUTILS_SEARCHBUILDER_H
#define UIUTILS_SEARCHBUILDER_H

#include <QCoreApplication>
#include <QDate>
#include <QList>
#include <QString>

#include "UiUtils/SearchQuery.h"

/** @short Conditions of the search builder, and their translation from and to the query syntax

The builder lists conditions of which either all or any must match, like the search dialogs of Thunderbird and KMail.
Each condition is one term of the query syntax, optionally negated, so a builder always has an equivalent query text.
A query can be opened in the builder when it is a single term, or terms which are all joined by AND or all joined by
OR, each of them optionally negated. Groups inside groups, such as "a OR (b c)", have no builder form.
*/
namespace UiUtils {
namespace SearchBuilder {

enum class Match {
    /** Every condition must match */
    All,
    /** At least one condition must match */
    Any,
};

struct Condition {
    SearchQuery::Field field = SearchQuery::Field::Text;
    SearchQuery::Comparison comparison = SearchQuery::Comparison::Contains;
    /** The condition must not match, such as "does not contain" */
    bool negated = false;
    /** The value as it is written in the query, without quotes; for has: always "attachment" */
    QString value;

    bool operator==(const Condition &other) const;
};

struct Conditions {
    Match match = Match::All;
    QList<Condition> conditions;
};

/** @short A comparison which the builder offers for a field */
struct Operator {
    SearchQuery::Comparison comparison;
    bool negated;
};

/** @short The comparisons offered for @arg field, in the order in which they are listed */
QList<Operator> operators(SearchQuery::Field field);

/** @short Whether @arg field takes a value; has: does not, because attachment is its only value */
bool needsValue(SearchQuery::Field field);

/** @short The query text of @arg condition, such as "-from:ann" or "size>2M"; empty when it still needs a value */
QString conditionText(const Condition &condition);

/** @short The query text of @arg conditions: separated by spaces when all must match, by OR when any must match.
Conditions which still need a value are left out. */
QString queryText(const Conditions &conditions);

/** @short The conditions of a query, or why the query cannot be shown as conditions */
struct Result {
    Conditions conditions;
    /** Why the query has no builder form; empty when it has one */
    QString problem;

    bool isValid() const { return problem.isEmpty(); }
};

/** @short Open the query @arg text in the builder; @arg today resolves the dates today and yesterday

A negated date or size comparison is turned around, so "-size>2M" becomes "size<=2M", and date= becomes date:. An empty
text gives no conditions.
*/
Result fromQuery(const QString &text, const QDate &today);

/** @short Translations for the messages of this namespace */
class Messages
{
    Q_DECLARE_TR_FUNCTIONS(UiUtils::SearchBuilder)
};

}
}

#endif
