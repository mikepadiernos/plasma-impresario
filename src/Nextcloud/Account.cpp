/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Nextcloud/Account.h"

#include <QCoreApplication>
#include <QHostAddress>

namespace Nextcloud {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("Nextcloud::Account", text);
}

QString serverPath(const QUrl &server)
{
    QString path = server.path(QUrl::FullyDecoded);
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    return path;
}

} // namespace

bool Account::isValid() const
{
    return !id.isEmpty() && server.isValid() && !server.host().isEmpty() && !userId.isEmpty();
}

QString Account::title() const
{
    const QString user = displayName.isEmpty() ? loginName : displayName;
    return user.isEmpty() ? server.host() : tr("%1 on %2").arg(user, server.host());
}

QString Account::davPath() const
{
    return serverPath(server) + QLatin1String("/remote.php/dav/files/") + userId;
}

QUrl Account::fileUrl(const QString &path) const
{
    QUrl url = server;
    const QString clean = cleanPath(path);
    url.setPath(davPath() + (clean == QLatin1String("/") ? QStringLiteral("/") : clean), QUrl::DecodedMode);
    url.setQuery(QString());
    url.setFragment(QString());
    url.setUserInfo(QString());
    return url;
}

QUrl Account::webFolderUrl(const QString &folder) const
{
    QUrl url = server;
    url.setPath(serverPath(server) + QLatin1String("/index.php/apps/files/"), QUrl::DecodedMode);
    url.setQuery(QLatin1String("dir=") + QString::fromLatin1(QUrl::toPercentEncoding(cleanPath(folder), "/")));
    url.setUserInfo(QString());
    return url;
}

bool isAcceptable(const QUrl &url)
{
    if (url.scheme() == QLatin1String("https"))
        return true;
    if (url.scheme() != QLatin1String("http"))
        return false;
    const QString host = url.host();
    return host == QLatin1String("localhost") || QHostAddress(host).isLoopback();
}

QUrl normalizedServer(const QString &text, QString *error)
{
    QString typed = text.trimmed();
    if (typed.isEmpty()) {
        *error = tr("Enter the address of the Nextcloud server.");
        return {};
    }
    if (!typed.contains(QLatin1String("://")))
        typed.prepend(QLatin1String("https://"));
    QUrl url(typed, QUrl::StrictMode);
    if (!url.isValid() || url.host().isEmpty()) {
        *error = tr("The address of the Nextcloud server is not valid.");
        return {};
    }
    if (url.scheme() != QLatin1String("https") && url.scheme() != QLatin1String("http")) {
        *error = tr("The address of the Nextcloud server must start with https://.");
        return {};
    }
    if (!url.userInfo().isEmpty()) {
        *error = tr("Do not put a user name or password in the address; you sign "
                    "in after connecting.");
        return {};
    }
    if (!isAcceptable(url)) {
        *error = tr("The server address is not protected by TLS. Use an https:// address.");
        return {};
    }
    QString path = url.path(QUrl::FullyDecoded);
    for (const QLatin1String marker : {QLatin1String("/index.php"), QLatin1String("/apps/"),
             QLatin1String("/remote.php"), QLatin1String("/ocs/"), QLatin1String("/login")}) {
        const qsizetype at = path.indexOf(marker);
        if (at >= 0)
            path.truncate(at);
    }
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    url.setPath(path, QUrl::DecodedMode);
    url.setQuery(QString());
    url.setFragment(QString());
    return url;
}

QString cleanPath(const QString &path)
{
    QStringList parts;
    const QStringList pieces = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &piece : pieces) {
        if (piece == QLatin1String("."))
            continue;
        if (piece == QLatin1String("..")) {
            if (!parts.isEmpty())
                parts.removeLast();
            continue;
        }
        parts.append(piece);
    }
    return QLatin1Char('/') + parts.join(QLatin1Char('/'));
}

QString parentPath(const QString &path)
{
    const QString clean = cleanPath(path);
    const qsizetype slash = clean.lastIndexOf(QLatin1Char('/'));
    return slash <= 0 ? QStringLiteral("/") : clean.left(slash);
}

QString childPath(const QString &folder, const QString &name)
{
    return cleanPath(folder + QLatin1Char('/') + name);
}

QString safeFileName(const QString &name)
{
    QString safe;
    safe.reserve(name.size());
    for (const QChar c : name)
        safe.append(c == QLatin1Char('/') || c == QLatin1Char('\\') || c.category() == QChar::Other_Control
                ? QLatin1Char('_')
                : c);
    safe = safe.trimmed();
    if (safe.isEmpty() || safe == QLatin1String(".") || safe == QLatin1String(".."))
        return tr("Attachment");
    return safe;
}

QString numberedName(const QString &name, int number)
{
    const qsizetype dot = name.lastIndexOf(QLatin1Char('.'));
    const bool hasExtension = dot > 0 && dot < name.size() - 1;
    const QString base = hasExtension ? name.left(dot) : name;
    const QString extension = hasExtension ? name.mid(dot) : QString();
    return QStringLiteral("%1 (%2)%3").arg(base).arg(number).arg(extension);
}

} // namespace Nextcloud
