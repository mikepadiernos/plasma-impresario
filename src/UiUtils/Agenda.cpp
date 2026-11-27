/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Agenda.h"

#include <algorithm>
#include <QHash>
#include <QRegularExpression>
#include <QTimeZone>

#include "Imap/Model/SearchIndex.h"

namespace UiUtils {
namespace Agenda {

namespace {

/** Position of an item's group in the list */
int group(const Item &item, const QDate &day)
{
    switch (item.kind) {
    case Kind::Event:
        return item.allDay ? 0 : 1;
    case Kind::Task:
        return isOverdue(item, day) ? 2 : 3;
    case Kind::Message:
        return 4;
    }
    return 4;
}

QString messageKey(const QString &accountId, const QString &mailbox, uint uid)
{
    return accountId + QLatin1Char('\n') + mailbox + QLatin1Char('\n') + QString::number(uid);
}

}

bool isOverdue(const Item &item, const QDate &day)
{
    return item.kind == Kind::Task && item.start.isValid() && item.start.date() < day;
}

QList<Item> sorted(QList<Item> items, const QDate &day)
{
    std::stable_sort(items.begin(), items.end(), [&day](const Item &a, const Item &b) {
        const int groupA = group(a, day);
        const int groupB = group(b, day);
        if (groupA != groupB)
            return groupA < groupB;
        if (a.kind == Kind::Message)
            return a.sent.isValid() && (!b.sent.isValid() || a.sent > b.sent);
        // A task due on a day without a time comes after those due at a time of that day
        if (a.start.date() != b.start.date())
            return a.start.date() < b.start.date();
        if (a.kind == Kind::Task && a.allDay != b.allDay)
            return !a.allDay;
        return a.start < b.start;
    });
    return items;
}

int nextEvent(const QList<Item> &items, const QDateTime &now)
{
    int found = -1;
    for (int i = 0; i < items.size(); ++i) {
        const Item &item = items.at(i);
        if (item.kind != Kind::Event || item.allDay || !item.start.isValid())
            continue;
        const QDateTime end = item.end.isValid() ? item.end : item.start;
        if (end <= now && !(item.start == end && item.start >= now))
            continue;
        if (found < 0 || item.start < items.at(found).start)
            found = i;
    }
    return found;
}

int dueTasks(const QList<Item> &items, const QDate &day)
{
    return std::count_if(items.cbegin(), items.cend(), [&day](const Item &item) {
        return item.kind == Kind::Task && item.start.isValid() && item.start.date() <= day;
    });
}

int messages(const QList<Item> &items)
{
    return std::count_if(items.cbegin(), items.cend(), [](const Item &item) { return item.kind == Kind::Message; });
}

qint64 msecsUntilNextDay(const QDateTime &now)
{
    const QDateTime next(now.date().addDays(1), QTime(0, 0), now.timeZone());
    return std::max<qint64>(1, now.msecsTo(next));
}

QUrl findMeetingUrl(const QString &text)
{
    static const QRegularExpression link(QStringLiteral("\\bhttps?://[^\\s<>\"]+"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = link.match(text);
    if (!match.hasMatch())
        return QUrl();
    QString found = match.captured();
    // Punctuation which ends a sentence is not part of the link
    while (!found.isEmpty() && QStringLiteral(".,;:!?)]}'").contains(found.back()))
        found.chop(1);
    const QUrl url(found, QUrl::StrictMode);
    return url.isValid() && !url.host().isEmpty() ? url : QUrl();
}

QList<Item> followUps(const QList<MessageSource> &sources, const ShownFolder &shown, int limit)
{
    QList<Item> result;
    QSet<QString> seen;
    const auto add = [&](const Item &item) {
        const QString key = messageKey(item.accountId, item.mailbox, item.uid);
        if (seen.contains(key))
            return;
        seen.insert(key);
        result << item;
    };
    for (const Item &item : shown.followUps)
        add(item);
    for (const MessageSource &source : sources) {
        if (!source.index)
            continue;
        const auto summaries = source.index->followUps(limit);
        for (const auto &summary : summaries) {
            if (source.accountId == shown.accountId && summary.location.mailbox == shown.mailbox) {
                if (shown.loaded.contains(summary.location.uid))
                    continue;
                if (shown.uidValidity && summary.location.uidValidity && summary.location.uidValidity != shown.uidValidity)
                    continue;
            }
            Item item;
            item.kind = Kind::Message;
            item.title = summary.subject;
            item.accountId = source.accountId;
            item.mailbox = summary.location.mailbox;
            item.uidValidity = summary.location.uidValidity;
            item.uid = summary.location.uid;
            item.messageId = summary.messageId;
            item.sender = summary.sender;
            item.sent = summary.sent;
            item.toDo = summary.toDo;
            add(item);
        }
    }
    std::stable_sort(result.begin(), result.end(), [](const Item &a, const Item &b) {
        return a.sent.isValid() && (!b.sent.isValid() || a.sent > b.sent);
    });
    if (result.size() > limit)
        result.erase(result.begin() + limit, result.end());
    return result;
}

}
}
