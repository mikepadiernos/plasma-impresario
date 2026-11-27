/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/PlasmaSearch.h"

#include <QRegularExpression>
#include <QUrl>

#include <KConfigGroup>

#include <algorithm>

#include "Imap/Model/SearchIndex.h"

namespace UiUtils {

namespace PlasmaSearch {

namespace {

const char groupName[] = "PlasmaSearch";
const char enabledKey[] = "ShowMail";
const QLatin1String idPrefix("message");
/** Words beyond these are left out, keeping a pasted paragraph from becoming a long query */
constexpr int maximumWords = 8;

QString encoded(const QString &part)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(part));
}

QString decoded(const QString &part)
{
    return QUrl::fromPercentEncoding(part.toLatin1());
}

}

bool isEnabled(const KConfigBase &config)
{
    return config.group(QLatin1String(groupName)).readEntry(enabledKey, false);
}

void setEnabled(KConfigBase &config, bool enabled)
{
    KConfigGroup group = config.group(QLatin1String(groupName));
    group.writeEntry(enabledKey, enabled);
    group.sync();
}

QStringList words(const QString &query)
{
    static const QRegularExpression separators(QStringLiteral("\\s+"));
    QStringList result;
    const QStringList parts = query.split(separators, Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part.toUcs4().size() < minimumWordLength)
            continue;
        const bool known = std::any_of(result.cbegin(), result.cend(), [&part](const QString &word) {
            return word.compare(part, Qt::CaseInsensitive) == 0;
        });
        if (known)
            continue;
        result << part;
        if (result.size() == maximumWords)
            break;
    }
    return result;
}

QString matchId(const Target &target)
{
    return QStringList{idPrefix, encoded(target.accountId), encoded(target.mailbox),
                       QString::number(target.uidValidity), QString::number(target.uid)}
        .join(QLatin1Char('/'));
}

bool parseMatchId(const QString &id, Target *target)
{
    const QStringList parts = id.split(QLatin1Char('/'));
    if (parts.size() != 5 || parts.at(0) != idPrefix)
        return false;
    // Only what matchId() writes is accepted, as the ID comes from another process
    for (int i = 1; i < 3; ++i) {
        if (parts.at(i).isEmpty() || encoded(decoded(parts.at(i))) != parts.at(i))
            return false;
    }
    bool validityOk = false;
    bool uidOk = false;
    Target parsed;
    parsed.accountId = decoded(parts.at(1));
    parsed.mailbox = decoded(parts.at(2));
    parsed.uidValidity = parts.at(3).toUInt(&validityOk);
    parsed.uid = parts.at(4).toUInt(&uidOk);
    if (!validityOk || !uidOk || !parsed.uidValidity || !parsed.uid
        || parts.at(3) != QString::number(parsed.uidValidity) || parts.at(4) != QString::number(parsed.uid))
        return false;
    if (target)
        *target = parsed;
    return true;
}

QList<Match> matches(const QString &query, const QList<Source> &sources, int limit)
{
    QList<Match> result;
    const QStringList searched = words(query);
    if (searched.isEmpty() || limit <= 0)
        return result;
    for (const Source &source : sources) {
        if (!source.index || !source.index->isOpen())
            continue;
        const auto found = source.index->quickMatches(searched, limit);
        for (const auto &summary : found) {
            // Without a UIDVALIDITY the message could not be told apart from another one with the same UID
            if (!summary.location.uidValidity)
                continue;
            Match match;
            match.id = matchId({source.accountId, summary.location.mailbox, summary.location.uidValidity,
                                summary.location.uid});
            match.subject = summary.subject;
            match.sender = summary.sender;
            match.sent = summary.sent;
            result << match;
        }
    }
    // Each account gave its newest matches; of them all, the newest are kept
    std::stable_sort(result.begin(), result.end(), [](const Match &a, const Match &b) {
        if (a.sent.isValid() != b.sent.isValid())
            return a.sent.isValid();
        return a.sent > b.sent;
    });
    if (result.size() > limit)
        result.resize(limit);
    for (int i = 0; i < result.size(); ++i)
        result[i].relevance = qreal(result.size() - i) / qreal(result.size() + 1);
    return result;
}

}

}
