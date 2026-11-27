/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Composer/AttachmentRequests.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QUrl>
#include <QUrlQuery>

namespace Composer {
namespace AttachmentRequests {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("Composer::AttachmentRequests", text);
}

bool isAttachmentKey(const QString &key)
{
    return key.compare(QLatin1String("attach"), Qt::CaseInsensitive) == 0
            || key.compare(QLatin1String("attachment"), Qt::CaseInsensitive) == 0;
}

/** Whether a folder or file name in @arg path starts with a dot */
bool hasHiddenPart(const QString &path)
{
    const QStringList parts = QDir::fromNativeSeparators(path).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part.startsWith(QLatin1Char('.')))
            return true;
    }
    return false;
}

/** Whether @arg path is inside a folder of the system rather than of the user */
bool isSystemPath(const QString &path)
{
    static const QStringList folders{QStringLiteral("/proc"), QStringLiteral("/sys"), QStringLiteral("/dev"),
                                     QStringLiteral("/etc"), QStringLiteral("/boot"), QStringLiteral("/root")};
    for (const QString &folder : folders) {
        if (path == folder || path.startsWith(folder + QLatin1Char('/')))
            return true;
    }
    return false;
}

}

QStringList fromMailto(const QUrl &url)
{
    QStringList result;
    if (url.scheme().compare(QLatin1String("mailto"), Qt::CaseInsensitive) != 0)
        return result;
    const auto items = QUrlQuery(url).queryItems(QUrl::FullyEncoded);
    for (const auto &item : items) {
        if (!isAttachmentKey(item.first))
            continue;
        const QString value = QUrl::fromPercentEncoding(item.second.toUtf8());
        if (!value.isEmpty())
            result << value;
    }
    return result;
}

bool hasRequests(const QUrl &url)
{
    return !fromMailto(url).isEmpty();
}

Request resolve(const QString &requested, const Source source)
{
    Request request;
    request.requested = requested;

    QString path;
    const QUrl url(requested, QUrl::TolerantMode);
    if (requested.startsWith(QLatin1Char('/'))) {
        path = requested;
    } else if (url.isLocalFile()) {
        path = url.toLocalFile();
    } else if (url.isValid() && url.scheme().size() > 1) {
        // A single letter would be a Windows drive rather than a scheme
        request.problem = tr("It is not a local file; files on other computers cannot be attached this way.");
        return request;
    }
    if (path.isEmpty() || !QDir::isAbsolutePath(path)) {
        request.problem = tr("It is not an absolute path.");
        return request;
    }

    const QFileInfo info(QDir::cleanPath(path));
    if (!info.exists()) {
        request.problem = info.isSymLink() ? tr("It is a link to a file that does not exist.") : tr("It does not exist.");
        return request;
    }
    if (info.isDir()) {
        request.problem = tr("It is a folder; only files can be attached.");
        return request;
    }

    const QString canonical = info.canonicalFilePath();
    if (source == Source::MailtoUrl) {
        if (hasHiddenPart(info.absoluteFilePath()) || hasHiddenPart(canonical)) {
            request.problem = tr("It is hidden or inside a hidden folder, which a mailto: link may not attach.");
            return request;
        }
        if (isSystemPath(info.absoluteFilePath()) || isSystemPath(canonical)) {
            request.problem = tr("It is a system file, which a mailto: link may not attach.");
            return request;
        }
    }
    if (!info.isFile()) {
        request.problem = tr("It is not a regular file.");
        return request;
    }
    if (!info.isReadable()) {
        request.problem = tr("It cannot be read.");
        return request;
    }
    request.path = info.absoluteFilePath();
    return request;
}

QList<Request> resolveAll(const QStringList &requested, const Source source)
{
    QList<Request> result;
    QSet<QString> seen;
    for (const QString &one : requested) {
        Request request = resolve(one, source);
        if (request.accepted()) {
            const QString key = QFileInfo(request.path).canonicalFilePath();
            if (seen.contains(key))
                continue;
            seen.insert(key);
        }
        result << request;
    }
    return result;
}

}
}
