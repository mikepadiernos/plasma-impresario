/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/MailRulesSettings.h"

#include <KConfigGroup>

namespace UiUtils {

namespace MailRulesSettings {

using namespace MailRules;

namespace {

const char groupName[] = "MailRules";
const char orderKey[] = "Order";

QString ruleGroupName(const QString &id)
{
    return QStringLiteral("Rule ") + id;
}

const QList<QPair<Field, QString>> &fieldKeys()
{
    static const QList<QPair<Field, QString>> keys = {
        {Field::Sender, QStringLiteral("sender")},
        {Field::Recipients, QStringLiteral("recipients")},
        {Field::Subject, QStringLiteral("subject")},
        {Field::Attachment, QStringLiteral("attachment")},
        {Field::Folder, QStringLiteral("folder")},
        {Field::Tag, QStringLiteral("tag")},
        {Field::Junk, QStringLiteral("junk")},
    };
    return keys;
}

const QList<QPair<Comparison, QString>> &comparisonKeys()
{
    static const QList<QPair<Comparison, QString>> keys = {
        {Comparison::Contains, QStringLiteral("contains")},
        {Comparison::DoesNotContain, QStringLiteral("notContains")},
        {Comparison::Is, QStringLiteral("is")},
        {Comparison::IsNot, QStringLiteral("isNot")},
    };
    return keys;
}

const QList<QPair<ActionKind, QString>> &actionKeys()
{
    static const QList<QPair<ActionKind, QString>> keys = {
        {ActionKind::Move, QStringLiteral("move")},
        {ActionKind::Archive, QStringLiteral("archive")},
        {ActionKind::Delete, QStringLiteral("trash")},
        {ActionKind::Flag, QStringLiteral("flag")},
        {ActionKind::MarkRead, QStringLiteral("markRead")},
        {ActionKind::AddTag, QStringLiteral("addTag")},
        {ActionKind::Notify, QStringLiteral("notify")},
    };
    return keys;
}

template<typename T>
QString keyOf(const QList<QPair<T, QString>> &keys, T value)
{
    for (const auto &key : keys) {
        if (key.first == value)
            return key.second;
    }
    return QString();
}

template<typename T>
bool valueOf(const QList<QPair<T, QString>> &keys, const QString &name, T *value)
{
    for (const auto &key : keys) {
        if (key.second == name) {
            *value = key.first;
            return true;
        }
    }
    return false;
}

Rule readRule(const KConfigGroup &group, const QString &id)
{
    Rule rule;
    rule.id = id;
    rule.name = group.readEntry("Name", QString());
    rule.enabled = group.readEntry("Enabled", true);
    rule.accountId = group.readEntry("Account", QString());
    rule.folders = group.readEntry("Folders", QStringList());
    rule.match = group.readEntry("Match", QString()) == QLatin1String("any") ? Match::Any : Match::All;
    rule.stop = group.readEntry("Stop", false);
    rule.reviewedFingerprint = group.readEntry("Reviewed", QString());
    for (const QString &entry : group.readEntry("Conditions", QStringList())) {
        const QStringList parts = entry.split(QLatin1Char('|'));
        Condition condition;
        if (parts.size() < 3 || !valueOf(fieldKeys(), parts.at(0), &condition.field)
            || !valueOf(comparisonKeys(), parts.at(1), &condition.comparison))
            continue;
        condition.value = parts.mid(2).join(QLatin1Char('|'));
        rule.conditions.append(condition);
    }
    for (const QString &entry : group.readEntry("Actions", QStringList())) {
        const QStringList parts = entry.split(QLatin1Char('|'));
        Action action;
        if (!valueOf(actionKeys(), parts.at(0), &action.kind))
            continue;
        action.value = parts.mid(1).join(QLatin1Char('|'));
        rule.actions.append(action);
    }
    return rule;
}

void writeRule(KConfigGroup &group, const Rule &rule)
{
    group.writeEntry("Name", rule.name);
    group.writeEntry("Enabled", rule.enabled);
    group.writeEntry("Account", rule.accountId);
    group.writeEntry("Folders", rule.folders);
    group.writeEntry("Match", rule.match == Match::Any ? QStringLiteral("any") : QStringLiteral("all"));
    group.writeEntry("Stop", rule.stop);
    group.writeEntry("Reviewed", rule.reviewedFingerprint);
    QStringList conditions;
    for (const Condition &condition : rule.conditions)
        conditions.append(keyOf(fieldKeys(), condition.field) + QLatin1Char('|') + keyOf(comparisonKeys(), condition.comparison)
                          + QLatin1Char('|') + condition.value);
    group.writeEntry("Conditions", conditions);
    QStringList actions;
    for (const Action &action : rule.actions)
        actions.append(keyOf(actionKeys(), action.kind) + QLatin1Char('|') + action.value);
    group.writeEntry("Actions", actions);
}

}

QList<Rule> load(const KConfigBase &config)
{
    const KConfigGroup group = config.group(QLatin1String(groupName));
    QList<Rule> rules;
    QStringList seen;
    for (const QString &id : group.readEntry(orderKey, QStringList())) {
        if (id.isEmpty() || seen.contains(id) || !group.hasGroup(ruleGroupName(id)))
            continue;
        seen.append(id);
        rules.append(readRule(group.group(ruleGroupName(id)), id));
    }
    return rules;
}

void save(KConfigBase &config, const QList<Rule> &rules)
{
    KConfigGroup group = config.group(QLatin1String(groupName));
    QStringList order;
    for (const Rule &rule : rules)
        order.append(rule.id);
    for (const QString &name : group.groupList()) {
        if (name.startsWith(QLatin1String("Rule ")) && !order.contains(name.mid(5)))
            group.deleteGroup(name);
    }
    for (const Rule &rule : rules) {
        KConfigGroup ruleGroup = group.group(ruleGroupName(rule.id));
        ruleGroup.deleteGroup();
        ruleGroup = group.group(ruleGroupName(rule.id));
        writeRule(ruleGroup, rule);
    }
    group.writeEntry(orderKey, order);
    group.sync();
}

void removeAccount(KConfigBase &config, const QString &accountId)
{
    if (accountId.isEmpty())
        return;
    QList<Rule> rules = load(config);
    const qsizetype before = rules.size();
    rules.removeIf([&accountId](const Rule &rule) { return rule.accountId == accountId; });
    if (rules.size() != before)
        save(config, rules);
}

bool renameFolder(KConfigBase &config, const QString &accountId, const QString &oldName, const QString &newName)
{
    QList<Rule> rules = load(config);
    bool changed = false;
    for (Rule &rule : rules) {
        // Rules for every account name folders of each of them; renaming one account's folder leaves them alone
        if (rule.accountId != accountId)
            continue;
        const bool reviewed = rule.isReviewed();
        for (QString &folder : rule.folders) {
            if (folder == oldName) {
                folder = newName;
                changed = true;
            }
        }
        for (Action &action : rule.actions) {
            if (action.kind == ActionKind::Move && action.value.trimmed() == oldName) {
                action.value = newName;
                changed = true;
            }
        }
        // Following a rename does not change what the rule does, so a preview stays valid
        if (reviewed)
            rule.markReviewed();
    }
    if (changed)
        save(config, rules);
    return changed;
}

}

}
