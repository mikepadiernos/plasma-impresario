/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Nextcloud/Files.h"

#include <QCollator>
#include <QCoreApplication>
#include <QLocale>
#include <QTimeZone>
#include <QUrl>
#include <QXmlStreamReader>

#include <algorithm>

#include "Nextcloud/Account.h"

namespace Nextcloud {

namespace {

const QString dav = QStringLiteral("DAV:");
const QString owncloud = QStringLiteral("http://owncloud.org/ns");

QString tr(const char *text)
{
    return QCoreApplication::translate("Nextcloud::Files", text);
}

/** An HTTP date such as "Fri, 27 Nov 2026 10:00:00 GMT", which is always in UTC */
QDateTime httpDate(const QString &text)
{
    QDateTime date = QLocale::c().toDateTime(text.trimmed(), QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'"));
    if (date.isValid()) {
        date.setTimeZone(QTimeZone::UTC);
        return date;
    }
    return QDateTime::fromString(text.trimmed(), Qt::RFC2822Date);
}

struct Response {
    QString href;
    bool ok = false;
    Entry entry;
};

/** Read one <d:propstat>, keeping its properties only when its status is 200 */
void readPropstat(QXmlStreamReader &xml, Response *response)
{
    Entry entry = response->entry;
    QString status;
    bool collection = false;
    QList<int> shareTypes;
    while (xml.readNextStartElement()) {
        if (xml.namespaceUri() == dav && xml.name() == QLatin1String("status")) {
            status = xml.readElementText();
        } else if (xml.namespaceUri() == dav && xml.name() == QLatin1String("prop")) {
            while (xml.readNextStartElement()) {
                const auto name = xml.name();
                const auto ns = xml.namespaceUri();
                if (ns == dav && name == QLatin1String("resourcetype")) {
                    while (xml.readNextStartElement()) {
                        if (xml.namespaceUri() == dav && xml.name() == QLatin1String("collection"))
                            collection = true;
                        xml.skipCurrentElement();
                    }
                } else if (ns == dav && name == QLatin1String("getcontentlength")) {
                    entry.size = xml.readElementText().toLongLong();
                } else if (ns == owncloud && name == QLatin1String("size")) {
                    const qint64 size = xml.readElementText().toLongLong();
                    if (size > 0)
                        entry.size = size;
                } else if (ns == dav && name == QLatin1String("getcontenttype")) {
                    entry.contentType = xml.readElementText();
                } else if (ns == dav && name == QLatin1String("getlastmodified")) {
                    entry.modified = httpDate(xml.readElementText());
                } else if (ns == dav && name == QLatin1String("getetag")) {
                    entry.etag = xml.readElementText();
                } else if (ns == owncloud && name == QLatin1String("fileid")) {
                    entry.fileId = xml.readElementText().toLongLong();
                } else if (ns == owncloud && name == QLatin1String("permissions")) {
                    entry.permissions.letters = xml.readElementText();
                } else if (ns == owncloud && name == QLatin1String("share-types")) {
                    while (xml.readNextStartElement()) {
                        if (xml.name() == QLatin1String("share-type"))
                            shareTypes.append(xml.readElementText().toInt());
                        else
                            xml.skipCurrentElement();
                    }
                } else {
                    xml.skipCurrentElement();
                }
            }
        } else {
            xml.skipCurrentElement();
        }
    }
    if (status.contains(QLatin1String(" 200"))) {
        entry.folder = entry.folder || collection;
        entry.shareTypes += shareTypes;
        response->entry = entry;
        response->ok = true;
    }
}

} // namespace

bool Permissions::isShared() const
{
    return letters.contains(QLatin1Char('S'));
}

bool Permissions::isMounted() const
{
    return letters.contains(QLatin1Char('M'));
}

bool Permissions::canShare() const
{
    return letters.contains(QLatin1Char('R'));
}

bool Permissions::canWrite() const
{
    return letters.contains(QLatin1Char('W'));
}

bool Permissions::canCreateFile() const
{
    return letters.contains(QLatin1Char('C'));
}

bool Permissions::canCreateFolder() const
{
    return letters.contains(QLatin1Char('K'));
}

bool Permissions::canDelete() const
{
    return letters.contains(QLatin1Char('D'));
}

bool Permissions::canRename() const
{
    return letters.contains(QLatin1Char('N'));
}

bool Permissions::isKnown() const
{
    return !letters.isEmpty();
}

bool Entry::hasLinkShare() const
{
    return shareTypes.contains(3);
}

QByteArray propfindBody()
{
    return QByteArrayLiteral(R"(<?xml version="1.0" encoding="UTF-8"?>
<d:propfind xmlns:d="DAV:" xmlns:oc="http://owncloud.org/ns" xmlns:nc="http://nextcloud.org/ns">
  <d:prop>
    <d:resourcetype/>
    <d:getcontentlength/>
    <d:getcontenttype/>
    <d:getlastmodified/>
    <d:getetag/>
    <oc:fileid/>
    <oc:size/>
    <oc:permissions/>
    <oc:share-types/>
  </d:prop>
</d:propfind>
)");
}

bool parseListing(
    const QByteArray &body, const QString &davPath, const QString &requested, Listing *listing, QString *error)
{
    QXmlStreamReader xml(body);
    QList<Response> responses;
    bool multistatus = false;
    if (xml.readNextStartElement() && xml.namespaceUri() == dav && xml.name() == QLatin1String("multistatus")) {
        multistatus = true;
        while (xml.readNextStartElement()) {
            if (xml.namespaceUri() != dav || xml.name() != QLatin1String("response")) {
                xml.skipCurrentElement();
                continue;
            }
            Response response;
            while (xml.readNextStartElement()) {
                if (xml.namespaceUri() == dav && xml.name() == QLatin1String("href"))
                    response.href = xml.readElementText();
                else if (xml.namespaceUri() == dav && xml.name() == QLatin1String("propstat"))
                    readPropstat(xml, &response);
                else
                    xml.skipCurrentElement();
            }
            responses.append(response);
        }
    }
    if (!multistatus || xml.hasError()) {
        *error = tr("The server's list of files could not be read.");
        return false;
    }

    const QString wanted = cleanPath(requested);
    const QString root = davPath.endsWith(QLatin1Char('/')) ? davPath.chopped(1) : davPath;
    Listing parsed;
    bool foundFolder = false;
    for (Response &response : responses) {
        if (!response.ok)
            continue;
        // A href is a path or a full URL; either way its path is percent-encoded
        const QString decoded = QUrl::fromPercentEncoding(QUrl(response.href).path(QUrl::FullyEncoded).toUtf8());
        if (decoded != root && !decoded.startsWith(root + QLatin1Char('/')))
            continue;
        const QString path = cleanPath(decoded.mid(root.size()));
        response.entry.path = path;
        response.entry.name = path == QLatin1String("/") ? QString() : path.section(QLatin1Char('/'), -1);
        if (decoded.endsWith(QLatin1Char('/')))
            response.entry.folder = true;
        if (path == wanted) {
            parsed.folder = response.entry;
            foundFolder = true;
        } else if (parentPath(path) == wanted) {
            parsed.children.append(response.entry);
        }
    }
    if (!foundFolder) {
        *error = tr("The server did not list the folder.");
        return false;
    }
    // The C locale compares code points only, which would put 10.txt before 9.txt
    QLocale locale;
    if (locale.language() == QLocale::C)
        locale = QLocale(QLocale::English, QLocale::UnitedStates);
    QCollator collator(locale);
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::stable_sort(parsed.children.begin(), parsed.children.end(), [&collator](const Entry &a, const Entry &b) {
        if (a.folder != b.folder)
            return a.folder;
        return collator.compare(a.name, b.name) < 0;
    });
    *listing = parsed;
    return true;
}

} // namespace Nextcloud
