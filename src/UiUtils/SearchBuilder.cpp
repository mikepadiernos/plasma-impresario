/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "SearchBuilder.h"

namespace UiUtils {
namespace SearchBuilder {

using SearchQuery::Comparison;
using SearchQuery::Field;
using SearchQuery::Node;

namespace {

QString tr(const char *text)
{
    return Messages::tr(text);
}

bool isOrdered(const Field field)
{
    return field == Field::Date || field == Field::Size;
}

/** @short The comparison which matches exactly the messages that @arg comparison does not match */
Comparison opposite(const Comparison comparison)
{
    switch (comparison) {
    case Comparison::Less:
        return Comparison::GreaterOrEqual;
    case Comparison::LessOrEqual:
        return Comparison::Greater;
    case Comparison::Greater:
        return Comparison::LessOrEqual;
    case Comparison::GreaterOrEqual:
        return Comparison::Less;
    case Comparison::Contains:
    case Comparison::Equal:
        break;
    }
    return comparison;
}

bool offered(const Condition &condition)
{
    for (const Operator &op : operators(condition.field)) {
        if (op.comparison == condition.comparison && op.negated == condition.negated)
            return true;
    }
    return false;
}

/** @short The condition of @arg node, a term or a negated term; false for anything else */
bool conditionOf(const Node &node, Condition &condition)
{
    bool negated = false;
    const Node *term = &node;
    if (node.kind == Node::Kind::Not) {
        negated = true;
        term = &node.children.front();
    }
    if (term->kind != Node::Kind::Term)
        return false;
    condition.field = term->term.field;
    condition.comparison = term->term.comparison;
    condition.negated = negated;
    condition.value = term->term.value;
    if (condition.field == Field::Date && condition.comparison == Comparison::Equal)
        condition.comparison = Comparison::Contains;
    if (isOrdered(condition.field) && condition.negated && condition.comparison != Comparison::Contains) {
        condition.comparison = opposite(condition.comparison);
        condition.negated = false;
    }
    return offered(condition);
}

/** @short Text written for the comparison of a date or size term */
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

}

bool Condition::operator==(const Condition &other) const
{
    return field == other.field && comparison == other.comparison && negated == other.negated && value == other.value;
}

QList<Operator> operators(const Field field)
{
    switch (field) {
    case Field::Date:
        return {
            {Comparison::Contains, false},
            {Comparison::Contains, true},
            {Comparison::Less, false},
            {Comparison::LessOrEqual, false},
            {Comparison::Greater, false},
            {Comparison::GreaterOrEqual, false},
        };
    case Field::Size:
        return {
            {Comparison::Greater, false},
            {Comparison::GreaterOrEqual, false},
            {Comparison::Less, false},
            {Comparison::LessOrEqual, false},
        };
    default:
        return {
            {Comparison::Contains, false},
            {Comparison::Contains, true},
        };
    }
}

bool needsValue(const Field field)
{
    return field != Field::Has;
}

QString conditionText(const Condition &condition)
{
    QString value = needsValue(condition.field) ? condition.value.trimmed() : QStringLiteral("attachment");
    if (value.isEmpty())
        return QString();
    QString text;
    if (condition.field == Field::Text) {
        text = SearchQuery::quotedValue(value);
    } else if (isOrdered(condition.field)) {
        // Dates and sizes are single words; quoting would hide a typo such as "2025 06" behind a parse error
        text = SearchQuery::fieldName(condition.field) + comparisonText(condition.comparison) + value;
    } else {
        text = SearchQuery::fieldName(condition.field) + QLatin1Char(':') + SearchQuery::quotedValue(value);
    }
    return condition.negated ? QLatin1Char('-') + text : text;
}

QString queryText(const Conditions &conditions)
{
    QStringList parts;
    for (const Condition &condition : conditions.conditions) {
        const QString text = conditionText(condition);
        if (!text.isEmpty())
            parts << text;
    }
    return parts.join(conditions.match == Match::Any ? QStringLiteral(" OR ") : QStringLiteral(" "));
}

Result fromQuery(const QString &text, const QDate &today)
{
    Result result;
    const SearchQuery::Query query = SearchQuery::Query::parse(text, today);
    if (!query.isValid()) {
        result.problem = tr("The query cannot be opened in the builder because it is not valid: %1").arg(query.error().message);
        return result;
    }
    if (query.isEmpty())
        return result;

    const Node &root = query.root();
    QList<const Node *> nodes;
    if (root.kind == Node::Kind::And || root.kind == Node::Kind::Or) {
        result.conditions.match = root.kind == Node::Kind::Or ? Match::Any : Match::All;
        for (const Node &child : root.children)
            nodes << &child;
    } else {
        nodes << &root;
    }
    for (const Node *node : nodes) {
        Condition condition;
        if (!conditionOf(*node, condition)) {
            result.conditions = {};
            result.problem = tr("The builder lists conditions of which all or any must match, so it cannot show groups "
                                "inside groups or a negated group, as in %1. Edit the query text instead, or build a new "
                                "query.")
                                 .arg(query.toString());
            return result;
        }
        result.conditions.conditions << condition;
    }
    return result;
}

}
}
