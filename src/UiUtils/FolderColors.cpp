/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/FolderColors.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <limits>

namespace UiUtils {

namespace FolderColors {

namespace {

const QString AccountKey = QStringLiteral("account");
const QString MatchKey = QStringLiteral("match");
const QString PatternKey = QStringLiteral("pattern");
const QString ColorKey = QStringLiteral("color");
const QString SubfoldersKey = QStringLiteral("subfolders");

QRegularExpression expression(const Rule &rule)
{
    return QRegularExpression::fromWildcard(rule.pattern, Qt::CaseInsensitive,
                                            rule.match == Match::Path ? QRegularExpression::DefaultWildcardConversion
                                                                      : QRegularExpression::NonPathWildcardConversion);
}

bool matches(const Rule &rule, const QRegularExpression &re, const QString &path)
{
    const QString subject = rule.match == Match::Path ? path : path.section(QLatin1Char('/'), -1);
    return re.match(subject).hasMatch();
}

} // namespace

std::optional<Rule> parse(const QString &entry)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(entry.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;
    const QJsonObject object = document.object();
    Rule rule;
    const QString match = object.value(MatchKey).toString();
    if (match == QLatin1String("path"))
        rule.match = Match::Path;
    else if (match != QLatin1String("name"))
        return std::nullopt;
    rule.account = object.value(AccountKey).toString().trimmed();
    rule.pattern = object.value(PatternKey).toString().trimmed();
    rule.color = QColor::fromString(object.value(ColorKey).toString());
    rule.subfolders = object.value(SubfoldersKey).toBool();
    if (rule.pattern.isEmpty() || rule.pattern.size() > 1024 || !rule.color.isValid())
        return std::nullopt;
    if (rule.match == Match::Name && rule.pattern.contains(QLatin1Char('/')))
        return std::nullopt;
    if (rule.match == Match::Path)
        rule.pattern.remove(QRegularExpression(QStringLiteral("^/+|/+$")));
    if (rule.pattern.isEmpty() || !expression(rule).isValid())
        return std::nullopt;
    rule.color = QColor(rule.color.rgb());
    return rule;
}

QString entry(const Rule &rule)
{
    QJsonObject object{{MatchKey, rule.match == Match::Path ? QStringLiteral("path") : QStringLiteral("name")},
                       {PatternKey, rule.pattern},
                       {ColorKey, rule.color.isValid() ? rule.color.name(QColor::HexRgb) : QString()},
                       {SubfoldersKey, rule.subfolders}};
    if (!rule.account.isEmpty())
        object.insert(AccountKey, rule.account);
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QList<Rule> rules(const QStringList &entries)
{
    QList<Rule> result;
    for (const QString &stored : entries) {
        const std::optional<Rule> rule = parse(stored);
        if (rule && !result.contains(*rule))
            result.append(*rule);
        if (result.size() == MaximumRules)
            break;
    }
    return result;
}

QStringList entries(const QList<Rule> &rules)
{
    QStringList result;
    for (const Rule &rule : rules)
        result.append(entry(rule));
    return result;
}

QString normalizedPath(const QString &folder, QChar separator)
{
    QString path = folder;
    if (!separator.isNull() && separator != QLatin1Char('/'))
        path.replace(separator, QLatin1Char('/'));
    return path;
}

QColor ruleColor(const QList<Rule> &rules, const QString &accountId, const QString &folder, QChar separator)
{
    if (folder.isEmpty())
        return {};
    const QString path = normalizedPath(folder, separator);
    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const Rule &rule : rules) {
        if (!rule.account.isEmpty() && rule.account != accountId)
            continue;
        const QRegularExpression re = expression(rule);
        if (matches(rule, re, path))
            return rule.color;
        if (!rule.subfolders)
            continue;
        for (qsizetype length = parts.size() - 1; length > 0; --length) {
            if (matches(rule, re, parts.first(length).join(QLatin1Char('/'))))
                return rule.color;
        }
    }
    return {};
}

QColor accountColor(const Colors &colors, const QString &accountId)
{
    return colors.accounts.value(accountId);
}

QColor folderColor(const Colors &colors, const QString &accountId, const QString &folder, QChar separator)
{
    const QColor own = colors.folders.value(accountId).value(folder);
    if (own.isValid())
        return own;
    return ruleColor(colors.rules, accountId, folder, separator);
}

QStringList iconColors()
{
    return {QStringLiteral("red"),   QStringLiteral("orange"), QStringLiteral("yellow"), QStringLiteral("green"),
            QStringLiteral("cyan"),  QStringLiteral("blue"),   QStringLiteral("violet"), QStringLiteral("magenta"),
            QStringLiteral("brown"), QStringLiteral("grey"),   QStringLiteral("black"),  QStringLiteral("white")};
}

QColor iconColor(const QString &name)
{
    // Close to the colours of Breeze's coloured folder icons, in the order of iconColors()
    static const QColor references[] = {
        QColor(218, 68, 83),  QColor(246, 116, 0),   QColor(253, 188, 75), QColor(39, 174, 96),
        QColor(26, 188, 156), QColor(61, 174, 233),  QColor(155, 89, 182), QColor(233, 58, 154),
        QColor(161, 118, 82), QColor(127, 140, 141), QColor(35, 38, 41),   QColor(239, 240, 241),
    };
    const qsizetype index = iconColors().indexOf(name);
    return index < 0 ? QColor() : references[index];
}

QString iconName(const QColor &color)
{
    if (!color.isValid())
        return {};
    const QStringList names = iconColors();
    qsizetype best = 0;
    double bestDistance = std::numeric_limits<double>::max();
    for (qsizetype i = 0; i < names.size(); ++i) {
        const QColor reference = iconColor(names.at(i));
        // A weighted distance, closer to how different colours look than plain RGB
        const double meanRed = (color.red() + reference.red()) / 2.0;
        const double dr = color.red() - reference.red();
        const double dg = color.green() - reference.green();
        const double db = color.blue() - reference.blue();
        const double distance = (2 + meanRed / 256) * dr * dr + 4 * dg * dg + (2 + (255 - meanRed) / 256) * db * db;
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    return QStringLiteral("folder-") + names.at(best);
}

} // namespace FolderColors

} // namespace UiUtils
