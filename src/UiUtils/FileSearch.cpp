/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "FileSearch.h"

#include <KConfig>
#include <KConfigGroup>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

namespace UiUtils {
namespace FileSearch {

namespace {

QString normalized(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

/** Whether @arg path is @arg folder or inside it; both normalized */
bool isWithin(const QString &path, const QString &folder)
{
    if (path == folder)
        return true;
    return path.startsWith(folder.endsWith(QLatin1Char('/')) ? folder : folder + QLatin1Char('/'));
}

}

bool Status::isSearchable() const
{
    return coverage == Coverage::Covered || coverage == Coverage::NamesOnly;
}

Status coverage(const KConfig &balooConfig, const QString &folder, bool installed)
{
    Status status;
    if (!installed)
        return status;

    const KConfigGroup basic = balooConfig.group(QStringLiteral("Basic Settings"));
    if (!basic.readEntry("Indexing-Enabled", true)) {
        status.coverage = Coverage::Disabled;
        return status;
    }

    const KConfigGroup general = balooConfig.group(QStringLiteral("General"));
    const QStringList included = general.readPathEntry(QStringLiteral("folders"), QStringList{QDir::homePath()});
    const QStringList excluded = general.readPathEntry(QStringLiteral("exclude folders"), QStringList());
    const QString path = normalized(folder);

    // Like Baloo, the closest configured folder decides; an exclusion wins over an inclusion of the same folder
    QString deciding;
    bool isIncluded = false;
    for (const QString &entry : included) {
        const QString candidate = normalized(entry);
        if (!entry.isEmpty() && isWithin(path, candidate) && candidate.size() > deciding.size()) {
            deciding = candidate;
            isIncluded = true;
        }
    }
    for (const QString &entry : excluded) {
        const QString candidate = normalized(entry);
        if (!entry.isEmpty() && isWithin(path, candidate) && candidate.size() >= deciding.size()) {
            deciding = candidate;
            isIncluded = false;
        }
    }

    if (deciding.isEmpty()) {
        status.coverage = Coverage::NotIncluded;
        return status;
    }
    status.decidingFolder = deciding;
    if (!isIncluded) {
        status.coverage = Coverage::Excluded;
        return status;
    }

    if (!general.readEntry("index hidden folders", false)) {
        QString walked = deciding;
        const QStringList below = path.mid(deciding.size()).split(QLatin1Char('/'), Qt::SkipEmptyParts);
        for (const QString &component : below) {
            if (!walked.endsWith(QLatin1Char('/')))
                walked += QLatin1Char('/');
            walked += component;
            if (component.startsWith(QLatin1Char('.'))) {
                status.coverage = Coverage::Hidden;
                status.decidingFolder = walked;
                return status;
            }
        }
    }

    status.coverage = general.readEntry("only basic indexing", false) ? Coverage::NamesOnly : Coverage::Covered;
    return status;
}

QUrl indexedSearchUrl(const QString &text, const QString &folder, const QString &title)
{
    // The same URL as Baloo::Query::toSearchUrl(), without linking to Baloo: the JSON is percent-encoded first, so
    // that a "+" in the text is never read as a space
    QJsonObject query;
    if (!text.isEmpty())
        query.insert(QStringLiteral("searchString"), text);
    if (!folder.isEmpty())
        query.insert(QStringLiteral("includeFolder"), normalized(folder));

    QUrlQuery items;
    items.addQueryItem(QStringLiteral("json"),
                       QString::fromLatin1(QUrl::toPercentEncoding(
                           QString::fromUtf8(QJsonDocument(query).toJson(QJsonDocument::Compact)), QByteArrayLiteral(":"))));
    if (!title.isEmpty())
        items.addQueryItem(QStringLiteral("title"), title);
    QUrl url;
    url.setScheme(QStringLiteral("baloosearch"));
    url.setQuery(items);
    return url;
}

QUrl liveSearchUrl(const QString &text, const QString &folder, const QString &title)
{
    // The query items which Dolphin and Plasma Opus use for their own live searches
    QUrlQuery items;
    items.addQueryItem(QStringLiteral("search"), text);
    items.addQueryItem(QStringLiteral("checkContent"), QStringLiteral("yes"));
    items.addQueryItem(QStringLiteral("url"), QUrl::fromLocalFile(normalized(folder)).toString());
    if (!title.isEmpty())
        items.addQueryItem(QStringLiteral("title"), title);
    QUrl url;
    url.setScheme(QStringLiteral("filenamesearch"));
    url.setQuery(items);
    return url;
}

QString savedFilesFolder(const KConfigGroup &state, const QString &fallback)
{
    const QString folder = state.readEntry("Folder", QString());
    if (!folder.isEmpty() && QFileInfo(folder).isDir())
        return folder;
    return fallback;
}

void rememberSavedFile(KConfigGroup &state, const QString &filePath)
{
    if (filePath.isEmpty())
        return;
    state.writeEntry("Folder", QFileInfo(filePath).absolutePath());
}

}
}
