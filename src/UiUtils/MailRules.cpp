/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/MailRules.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QUuid>

#include "UiUtils/SearchQuery.h"

namespace UiUtils {

namespace MailRules {

namespace {

const QString inbox = QStringLiteral("INBOX");

QString tr(const char *text, const char *comment = nullptr)
{
    return QCoreApplication::translate("UiUtils::MailRules", text, comment);
}

bool isMoving(ActionKind kind)
{
    return kind == ActionKind::Move || kind == ActionKind::Archive || kind == ActionKind::Delete;
}

bool sameFolder(const QString &a, const QString &b)
{
    // The INBOX is case-insensitive in IMAP; other names are compared as they are
    if (a.compare(inbox, Qt::CaseInsensitive) == 0)
        return b.compare(inbox, Qt::CaseInsensitive) == 0;
    return a == b;
}

/** The address inside "Name <address>", or the whole text */
QString addressOf(const QString &entry)
{
    const qsizetype open = entry.lastIndexOf(QLatin1Char('<'));
    const qsizetype close = entry.lastIndexOf(QLatin1Char('>'));
    if (open >= 0 && close > open)
        return entry.mid(open + 1, close - open - 1).trimmed();
    return entry.trimmed();
}

/** The name in front of "<address>", without quotes; empty when there is none */
QString nameOf(const QString &entry)
{
    const qsizetype open = entry.lastIndexOf(QLatin1Char('<'));
    if (open <= 0)
        return QString();
    QString name = entry.left(open).trimmed();
    if (name.size() >= 2 && name.startsWith(QLatin1Char('"')) && name.endsWith(QLatin1Char('"')))
        name = name.mid(1, name.size() - 2);
    return name;
}

bool anyContains(const QStringList &values, const QString &needle)
{
    for (const QString &value : values) {
        if (value.contains(needle, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

bool anyAddressIs(const QStringList &entries, const QString &value)
{
    const QString wanted = value.trimmed();
    for (const QString &entry : entries) {
        if (addressOf(entry).compare(wanted, Qt::CaseInsensitive) == 0
            || (!nameOf(entry).isEmpty() && nameOf(entry).compare(wanted, Qt::CaseInsensitive) == 0))
            return true;
    }
    return false;
}

bool anyTagIs(const QStringList &flags, const QString &value)
{
    for (const QString &flag : flags) {
        if (flag.compare(value.trimmed(), Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

/** The tags among @arg flags: keywords, without the system flags which start with a backslash */
QStringList tagsOf(const QStringList &flags)
{
    QStringList tags;
    for (const QString &flag : flags) {
        if (!flag.startsWith(QLatin1Char('\\')))
            tags.append(flag);
    }
    return tags;
}

/** Whether @arg text, as a positive comparison, holds; the negative comparisons are its opposite */
bool positive(Comparison comparison)
{
    return comparison == Comparison::Contains || comparison == Comparison::Is;
}

QString quoted(const QString &value)
{
    return tr("“%1”", "a value quoted in a rule, such as a subject or a folder").arg(value);
}

}

bool Rule::movesMessages() const
{
    for (const Action &action : actions) {
        if (isMoving(action.kind))
            return true;
    }
    return false;
}

QString Rule::fingerprint() const
{
    QStringList parts;
    parts << accountId << checkedFolders().join(QLatin1Char('\x1f')) << QString::number(static_cast<int>(match))
          << QString::number(stop ? 1 : 0);
    for (const Condition &condition : conditions)
        parts << QStringLiteral("c%1:%2:%3").arg(static_cast<int>(condition.field)).arg(static_cast<int>(condition.comparison)).arg(condition.value);
    for (const Action &action : actions)
        parts << QStringLiteral("a%1:%2").arg(static_cast<int>(action.kind)).arg(action.value);
    const QByteArray hash = QCryptographicHash::hash(parts.join(QLatin1Char('\x1e')).toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex().left(16));
}

bool Rule::isReviewed() const
{
    return !reviewedFingerprint.isEmpty() && reviewedFingerprint == fingerprint();
}

bool Rule::needsReview() const
{
    return enabled && movesMessages() && !isReviewed();
}

bool Rule::runsAutomatically() const
{
    return enabled && !needsReview();
}

void Rule::markReviewed()
{
    reviewedFingerprint = fingerprint();
}

QStringList Rule::checkedFolders() const
{
    QStringList result;
    for (const QString &folder : folders) {
        if (!folder.trimmed().isEmpty() && !result.contains(folder.trimmed()))
            result.append(folder.trimmed());
    }
    if (result.isEmpty())
        result.append(inbox);
    return result;
}

bool appliesTo(const Rule &rule, const QString &accountId, const QString &folder)
{
    if (!rule.accountId.isEmpty() && rule.accountId != accountId)
        return false;
    for (const QString &checked : rule.checkedFolders()) {
        if (sameFolder(checked, folder))
            return true;
    }
    return false;
}

bool isJunk(const QStringList &flags)
{
    bool junk = false;
    for (const QString &flag : flags) {
        if (flag.compare(QLatin1String("$NotJunk"), Qt::CaseInsensitive) == 0
            || flag.compare(QLatin1String("NotJunk"), Qt::CaseInsensitive) == 0)
            return false;
        if (flag.compare(QLatin1String("$Junk"), Qt::CaseInsensitive) == 0 || flag.compare(QLatin1String("Junk"), Qt::CaseInsensitive) == 0)
            junk = true;
    }
    return junk;
}

bool matches(const Condition &condition, const Message &message)
{
    bool holds = false;
    const QString value = condition.value.trimmed();
    const bool exact = condition.comparison == Comparison::Is || condition.comparison == Comparison::IsNot;
    switch (condition.field) {
    case Field::Sender:
        holds = exact ? anyAddressIs(message.senders, value) : anyContains(message.senders, value);
        break;
    case Field::Recipients:
        holds = exact ? anyAddressIs(message.recipients, value) : anyContains(message.recipients, value);
        break;
    case Field::Subject:
        holds = exact ? message.subject.trimmed().compare(value, Qt::CaseInsensitive) == 0
                      : message.subject.contains(value, Qt::CaseInsensitive);
        break;
    case Field::Attachment:
        holds = message.hasAttachment;
        break;
    case Field::Folder:
        holds = exact ? sameFolder(message.folder, value) : message.folder.contains(value, Qt::CaseInsensitive);
        break;
    case Field::Tag:
        holds = exact ? anyTagIs(tagsOf(message.flags), value) : anyContains(tagsOf(message.flags), value);
        break;
    case Field::Junk:
        holds = isJunk(message.flags);
        break;
    }
    return positive(condition.comparison) ? holds : !holds;
}

bool matches(const Rule &rule, const Message &message)
{
    if (rule.conditions.isEmpty())
        return true;
    for (const Condition &condition : rule.conditions) {
        const bool hit = matches(condition, message);
        if (rule.match == Match::Any && hit)
            return true;
        if (rule.match == Match::All && !hit)
            return false;
    }
    return rule.match == Match::All;
}

QStringList problems(const Rule &rule)
{
    QStringList result;
    if (rule.name.trimmed().isEmpty())
        result.append(tr("The rule needs a name."));
    if (rule.actions.isEmpty())
        result.append(tr("The rule needs at least one action."));
    for (const Condition &condition : rule.conditions) {
        if (takesValue(condition.field) && condition.value.trimmed().isEmpty()) {
            result.append(tr("The condition on %1 needs a value.").arg(fieldName(condition.field)));
        } else if (!comparisonsFor(condition.field).contains(condition.comparison)) {
            result.append(tr("%1 cannot be compared that way.").arg(fieldName(condition.field)));
        }
    }
    int moving = 0;
    for (const Action &action : rule.actions) {
        if (isMoving(action.kind))
            ++moving;
        if (action.kind == ActionKind::Move && action.value.trimmed().isEmpty())
            result.append(tr("Moving needs a folder."));
        if (action.kind == ActionKind::AddTag && action.value.trimmed().isEmpty())
            result.append(tr("Adding a tag needs a tag."));
        if (action.kind == ActionKind::AddTag && action.value.trimmed().contains(QLatin1Char(' ')))
            result.append(tr("A tag cannot contain spaces."));
        if (action.kind == ActionKind::Move) {
            for (const QString &folder : rule.checkedFolders()) {
                if (sameFolder(folder, action.value.trimmed()))
                    result.append(tr("The rule would move messages to %1, which it checks itself.").arg(quoted(folder)));
            }
        }
    }
    if (moving > 1)
        result.append(tr("A message can be moved only once; keep one of moving, archiving and moving to the trash."));
    return result;
}

QStringList warnings(const Rule &rule)
{
    QStringList result;
    if (rule.conditions.isEmpty())
        result.append(tr("Without conditions, the rule matches every new message."));
    if (rule.movesMessages() && !rule.isReviewed())
        result.append(tr("The rule moves messages, so it runs on its own only after you preview its matches."));
    return result;
}

Outcome evaluate(const QList<Rule> &rules, const Message &message, Mode mode)
{
    Outcome outcome;
    for (const Rule &rule : rules) {
        if (!rule.enabled || !appliesTo(rule, message.accountId, message.folder) || !matches(rule, message))
            continue;
        if (mode == Mode::Automatic && rule.needsReview()) {
            outcome.waiting.append(rule.id);
            continue;
        }
        outcome.ruleIds.append(rule.id);
        outcome.ruleNames.append(rule.name);
        bool moved = false;
        for (const Action &action : rule.actions) {
            switch (action.kind) {
            case ActionKind::Move:
                outcome.destination = Outcome::Destination::Move;
                outcome.folder = action.value.trimmed();
                moved = true;
                break;
            case ActionKind::Archive:
                outcome.destination = Outcome::Destination::Archive;
                moved = true;
                break;
            case ActionKind::Delete:
                outcome.destination = Outcome::Destination::Trash;
                moved = true;
                break;
            case ActionKind::Flag:
                outcome.flag = true;
                break;
            case ActionKind::MarkRead:
                outcome.markRead = true;
                break;
            case ActionKind::AddTag:
                if (!outcome.tags.contains(action.value.trimmed()))
                    outcome.tags.append(action.value.trimmed());
                break;
            case ActionKind::Notify:
                outcome.notify = true;
                break;
            }
            if (moved && outcome.destinationRuleId.isEmpty())
                outcome.destinationRuleId = rule.id;
        }
        if (moved || rule.stop)
            break;
    }
    return outcome;
}

QString fieldName(Field field)
{
    switch (field) {
    case Field::Sender:
        return tr("Sender");
    case Field::Recipients:
        return tr("Recipients");
    case Field::Subject:
        return tr("Subject");
    case Field::Attachment:
        return tr("Attachments");
    case Field::Folder:
        return tr("Folder");
    case Field::Tag:
        return tr("Tag");
    case Field::Junk:
        return tr("Junk mark");
    }
    return QString();
}

QString comparisonName(Field field, Comparison comparison)
{
    if (!takesValue(field)) {
        if (field == Field::Attachment)
            return positive(comparison) ? tr("are present") : tr("are absent");
        return positive(comparison) ? tr("is set") : tr("is not set");
    }
    switch (comparison) {
    case Comparison::Contains:
        return tr("contains");
    case Comparison::DoesNotContain:
        return tr("does not contain");
    case Comparison::Is:
        return tr("is");
    case Comparison::IsNot:
        return tr("is not");
    }
    return QString();
}

QList<Comparison> comparisonsFor(Field field)
{
    if (!takesValue(field))
        return {Comparison::Is, Comparison::IsNot};
    return {Comparison::Contains, Comparison::DoesNotContain, Comparison::Is, Comparison::IsNot};
}

bool takesValue(Field field)
{
    return field != Field::Attachment && field != Field::Junk;
}

QString actionName(ActionKind kind)
{
    switch (kind) {
    case ActionKind::Move:
        return tr("Move to folder");
    case ActionKind::Archive:
        return tr("Archive");
    case ActionKind::Delete:
        return tr("Move to trash");
    case ActionKind::Flag:
        return tr("Flag");
    case ActionKind::MarkRead:
        return tr("Mark as read");
    case ActionKind::AddTag:
        return tr("Add tag");
    case ActionKind::Notify:
        return tr("Notify");
    }
    return QString();
}

bool takesValue(ActionKind kind)
{
    return kind == ActionKind::Move || kind == ActionKind::AddTag;
}

QString describe(const Condition &condition)
{
    if (!takesValue(condition.field))
        return tr("%1 %2", "a condition without a value: %1 field, %2 comparison, such as Attachments are present")
            .arg(fieldName(condition.field), comparisonName(condition.field, condition.comparison));
    return tr("%1 %2 %3", "a condition: %1 field, %2 comparison, %3 quoted value, such as Subject contains “invoice”")
        .arg(fieldName(condition.field), comparisonName(condition.field, condition.comparison), quoted(condition.value.trimmed()));
}

QString describe(const Action &action)
{
    switch (action.kind) {
    case ActionKind::Move:
        return tr("move to %1").arg(quoted(action.value.trimmed()));
    case ActionKind::Archive:
        return tr("archive");
    case ActionKind::Delete:
        return tr("move to the trash");
    case ActionKind::Flag:
        return tr("flag");
    case ActionKind::MarkRead:
        return tr("mark as read");
    case ActionKind::AddTag:
        return tr("tag with %1").arg(quoted(action.value.trimmed()));
    case ActionKind::Notify:
        return tr("notify");
    }
    return QString();
}

QString summary(const Rule &rule)
{
    QStringList conditions;
    for (const Condition &condition : rule.conditions)
        conditions.append(describe(condition));
    QStringList actions;
    for (const Action &action : rule.actions)
        actions.append(describe(action));
    const QString what = actions.join(tr(", ", "separator between actions"));
    if (conditions.isEmpty())
        return tr("Every new message: %1", "%1 the actions").arg(what);
    const QString when = conditions.join(rule.match == Match::All ? tr(" and ", "conditions which must all match")
                                                                    : tr(" or ", "conditions of which one must match"));
    return tr("If %1: %2", "%1 the conditions, %2 the actions").arg(when, what);
}

namespace {

using SearchQuery::Node;
using SearchQuery::Term;

/** Convert one term; returns false and fills @arg why when rules cannot check it */
bool convertTerm(const Term &term, bool negated, Condition *condition, QString *why, QStringList *notes)
{
    Comparison comparison = negated ? Comparison::DoesNotContain : Comparison::Contains;
    if (term.comparison == SearchQuery::Comparison::Equal)
        comparison = negated ? Comparison::IsNot : Comparison::Is;
    const QString written = term.value;
    switch (term.field) {
    case SearchQuery::Field::Text:
        condition->field = Field::Subject;
        if (notes)
            notes->append(tr("Words without a field are checked in the subject: %1.").arg(quoted(written)));
        break;
    case SearchQuery::Field::From:
        condition->field = Field::Sender;
        break;
    case SearchQuery::Field::To:
    case SearchQuery::Field::Cc:
    case SearchQuery::Field::Bcc:
        condition->field = Field::Recipients;
        break;
    case SearchQuery::Field::Subject:
        condition->field = Field::Subject;
        break;
    case SearchQuery::Field::Tag:
        condition->field = Field::Tag;
        break;
    case SearchQuery::Field::Has:
        condition->field = Field::Attachment;
        condition->comparison = negated ? Comparison::IsNot : Comparison::Is;
        return true;
    case SearchQuery::Field::Body:
    case SearchQuery::Field::Message:
        *why = tr("Rules cannot check message text: %1.").arg(quoted(SearchQuery::fieldName(term.field) + QLatin1Char(':') + written));
        return false;
    case SearchQuery::Field::List:
        *why = tr("Rules cannot check mailing lists yet: %1.").arg(quoted(QStringLiteral("list:") + written));
        return false;
    case SearchQuery::Field::Is:
        *why = tr("Rules check new mail, so they cannot check message states: %1.").arg(quoted(QStringLiteral("is:") + written));
        return false;
    case SearchQuery::Field::Date:
    case SearchQuery::Field::Size:
        *why = tr("Rules cannot check dates or sizes: %1.").arg(quoted(SearchQuery::fieldName(term.field) + QLatin1Char(':') + written));
        return false;
    }
    if (term.comparison != SearchQuery::Comparison::Contains && term.comparison != SearchQuery::Comparison::Equal) {
        *why = tr("Rules cannot compare %1 that way.").arg(fieldName(condition->field));
        return false;
    }
    condition->comparison = comparison;
    condition->value = written;
    return true;
}

/** Convert a term or a negated term; nested groups are not supported */
bool convertLeaf(const Node &node, Condition *condition, QString *why, QStringList *notes)
{
    if (node.kind == Node::Kind::Term)
        return convertTerm(node.term, false, condition, why, notes);
    if (node.kind == Node::Kind::Not && node.children.size() == 1 && node.children.front().kind == Node::Kind::Term)
        return convertTerm(node.children.front().term, true, condition, why, notes);
    *why = tr("Rules cannot combine AND and OR; keep one of them.");
    return false;
}

}

Conversion fromQuery(const SearchQuery::Query &query)
{
    Conversion result;
    if (!query.isValid()) {
        result.unsupported.append(query.error().message);
        return result;
    }
    if (query.isEmpty()) {
        result.unsupported.append(tr("The search is empty."));
        return result;
    }
    const Node &root = query.root();
    std::vector<const Node *> leaves;
    if (root.kind == Node::Kind::And || root.kind == Node::Kind::Or) {
        result.rule.match = root.kind == Node::Kind::And ? Match::All : Match::Any;
        for (const Node &child : root.children)
            leaves.push_back(&child);
    } else {
        leaves.push_back(&root);
    }
    for (const Node *leaf : leaves) {
        Condition condition;
        QString why;
        if (convertLeaf(*leaf, &condition, &why, &result.notes)) {
            if (!result.rule.conditions.contains(condition))
                result.rule.conditions.append(condition);
        } else if (!result.unsupported.contains(why)) {
            result.unsupported.append(why);
        }
    }
    result.rule.name = query.toString();
    return result;
}

QString newId(const QStringList &taken)
{
    QString id;
    do {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    } while (taken.contains(id));
    return id;
}

}

}
