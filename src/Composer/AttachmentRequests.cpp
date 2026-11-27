/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Composer/AttachmentRequests.h"

#include <QCoreApplication>
#include <QDir>
#include <QHostAddress>
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

/** Network protocols through which a mailto: URL may ask for a file on another computer */
bool isNetworkScheme(const QString &scheme)
{
    static const QStringList schemes{QStringLiteral("sftp"), QStringLiteral("fish"), QStringLiteral("smb"),
                                     QStringLiteral("ftp"), QStringLiteral("ftps"), QStringLiteral("webdav"),
                                     QStringLiteral("webdavs"), QStringLiteral("http"), QStringLiteral("https")};
    return schemes.contains(scheme.toLower());
}

/** Whether @arg host names this computer, through which a remote protocol could reach its private files */
bool isThisComputer(const QString &host)
{
    const QString name = host.toLower();
    if (name.isEmpty() || name == QLatin1String("localhost") || name.endsWith(QLatin1String(".localhost"))
        || name == QLatin1String("localhost.localdomain"))
        return true;
    QHostAddress address;
    if (address.setAddress(name))
        return address.isLoopback() || address == QHostAddress::AnyIPv4 || address == QHostAddress::AnyIPv6;
    return false;
}

}

bool isFetchable(const QUrl &url)
{
    if (!url.isValid() || url.isLocalFile() || url.scheme().size() < 2)
        return false;
    static const QStringList notFiles{QStringLiteral("mailto"), QStringLiteral("data"), QStringLiteral("javascript"),
                                      QStringLiteral("about"), QStringLiteral("mid"), QStringLiteral("cid"),
                                      QStringLiteral("news"), QStringLiteral("tel")};
    return !notFiles.contains(url.scheme().toLower()) && !url.path().isEmpty();
}

QString displayUrl(const QUrl &url)
{
    return url.toDisplayString(QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::PreferLocalFile);
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
        if (!isFetchable(url)) {
            request.problem = tr("It does not name a file.");
        } else if (source == Source::MailtoUrl && !isNetworkScheme(url.scheme())) {
            request.problem = tr("A mailto: link may not ask for files through %1:.").arg(url.scheme());
        } else if (source == Source::MailtoUrl && isThisComputer(url.host())) {
            request.problem = tr("It names this computer through the network, which a mailto: link may not do.");
        } else {
            request.remote = url;
        }
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
            const QString key = request.remote.isEmpty() ? QFileInfo(request.path).canonicalFilePath()
                                                         : request.remote.adjusted(QUrl::NormalizePathSegments).toString();
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
