/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ShortcutOverview.h"

#include <QCollator>
#include <QHash>
#include <QMap>

#include <algorithm>

namespace UiUtils {
namespace ShortcutOverview {

QString plainText(const QString &text)
{
    QString result;
    result.reserve(text.size());
    for (int i = 0; i < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('&')) {
            // "&&" stands for a literal ampersand
            if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('&')) {
                result += QLatin1Char('&');
                ++i;
            }
            continue;
        }
        result += text.at(i);
    }
    result = result.trimmed();
    if (result.endsWith(QChar(0x2026)))
        result.chop(1);
    else if (result.endsWith(QLatin1String("...")))
        result.chop(3);
    return result.trimmed();
}

QString shortcutText(const Entry &entry)
{
    QStringList texts;
    for (const QKeySequence &shortcut : entry.shortcuts) {
        const QString text = shortcut.toString(QKeySequence::NativeText);
        if (!text.isEmpty())
            texts << text;
    }
    return texts.join(QLatin1String(", "));
}

QList<Group> group(const QList<Entry> &entries, const QStringList &areaOrder)
{
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    const auto before = [&collator](const QString &a, const QString &b) {
        return collator.compare(a, b) < 0;
    };

    QMap<QString, QList<Entry>> byArea;
    for (const Entry &entry : entries)
        byArea[entry.area] << entry;

    QStringList areas;
    for (const QString &area : areaOrder) {
        if (byArea.contains(area) && !areas.contains(area))
            areas << area;
    }
    QStringList others;
    for (auto it = byArea.constBegin(); it != byArea.constEnd(); ++it) {
        if (!areas.contains(it.key()))
            others << it.key();
    }
    std::sort(others.begin(), others.end(), before);
    areas << others;

    QList<Group> groups;
    for (const QString &area : std::as_const(areas)) {
        Group group{area, byArea.value(area)};
        std::stable_sort(group.entries.begin(), group.entries.end(), [&before](const Entry &a, const Entry &b) {
            return before(plainText(a.text), plainText(b.text));
        });
        groups << group;
    }
    return groups;
}

QList<Group> filter(const QList<Group> &groups, const QString &query)
{
    const QString needle = query.trimmed();
    if (needle.isEmpty())
        return groups;
    QList<Group> result;
    for (const Group &group : groups) {
        const bool areaMatches = plainText(group.area).contains(needle, Qt::CaseInsensitive);
        Group matching{group.area, {}};
        for (const Entry &entry : group.entries) {
            bool matches = areaMatches || plainText(entry.text).contains(needle, Qt::CaseInsensitive);
            for (const QKeySequence &shortcut : entry.shortcuts) {
                matches = matches || shortcut.toString(QKeySequence::NativeText).contains(needle, Qt::CaseInsensitive)
                        || shortcut.toString(QKeySequence::PortableText).contains(needle, Qt::CaseInsensitive);
            }
            if (matches)
                matching.entries << entry;
        }
        if (!matching.entries.isEmpty())
            result << matching;
    }
    return result;
}

QStringList duplicateShortcuts(const QList<Entry> &entries)
{
    QHash<QString, QStringList> owners;
    for (const Entry &entry : entries) {
        for (const QKeySequence &shortcut : entry.shortcuts) {
            const QString key = shortcut.toString(QKeySequence::PortableText);
            if (!key.isEmpty() && !owners[key].contains(entry.id))
                owners[key] << entry.id;
        }
    }
    QStringList duplicates;
    for (auto it = owners.constBegin(); it != owners.constEnd(); ++it) {
        if (it.value().size() > 1)
            duplicates << it.key();
    }
    duplicates.sort();
    return duplicates;
}

}
}
