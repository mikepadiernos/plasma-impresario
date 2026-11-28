/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Nextcloud/Shares.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>

namespace Nextcloud {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("Nextcloud::Shares", text);
}

QString text(const QJsonValue &value)
{
    if (value.isString())
        return value.toString();
    if (value.isDouble())
        return QString::number(value.toInteger());
    return {};
}

int number(const QJsonValue &value, int fallback = 0)
{
    if (value.isDouble())
        return value.toInt();
    bool ok = false;
    const int parsed = value.toString().toInt(&ok);
    return ok ? parsed : fallback;
}

bool flag(const QJsonValue &value)
{
    if (value.isBool())
        return value.toBool();
    return number(value) != 0;
}

/** The token after /s/ in @arg path, which may end with /download or /preview
 */
QString tokenInPath(const QString &path)
{
    static const QRegularExpression pattern(
        QStringLiteral("^(?:/index\\.php)?/s/([A-Za-z0-9]{8,64})(?:/(?:download|preview)?)?$"));
    const QRegularExpressionMatch match = pattern.match(path);
    return match.hasMatch() ? match.captured(1) : QString();
}

} // namespace

bool Share::isLink() const
{
    return type == PublicLink || type == Email;
}

bool parseShares(const QByteArray &json, QList<Share> *shares, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    const QJsonObject ocs = document.object().value(QLatin1String("ocs")).toObject();
    const QJsonObject meta = ocs.value(QLatin1String("meta")).toObject();
    if (!document.isObject() || ocs.isEmpty() || meta.isEmpty()) {
        *error = tr("The server's list of shares could not be read.");
        return false;
    }
    const int status = number(meta.value(QLatin1String("statuscode")));
    // OCS v1 reports success as 100, v2 as 200
    if (status != 100 && status != 200) {
        const QString message = meta.value(QLatin1String("message")).toString();
        *error = message.isEmpty() ? tr("The server refused to list the shares (status %1).").arg(status) : message;
        return false;
    }
    QList<Share> parsed;
    const QJsonValue data = ocs.value(QLatin1String("data"));
    const QJsonArray items = data.isArray() ? data.toArray() : QJsonArray{data};
    for (const QJsonValue &value : items) {
        const QJsonObject item = value.toObject();
        if (item.isEmpty())
            continue;
        Share share;
        share.id = text(item.value(QLatin1String("id")));
        share.type = number(item.value(QLatin1String("share_type")), -1);
        share.path = item.value(QLatin1String("path")).toString();
        share.folder = item.value(QLatin1String("item_type")).toString() == QLatin1String("folder");
        share.url = QUrl(item.value(QLatin1String("url")).toString());
        share.token = item.value(QLatin1String("token")).toString();
        share.permissions = number(item.value(QLatin1String("permissions")), Share::Read);
        const QString expiration = item.value(QLatin1String("expiration")).toString();
        share.expiration = QDate::fromString(expiration.left(10), Qt::ISODate);
        // Link shares report a password as a hash or a placeholder, and none as
        // null or empty; it is never shown
        const QJsonValue password = item.value(QLatin1String("password"));
        share.hasPassword = password.isString() && !password.toString().isEmpty();
        share.hideDownload = flag(item.value(QLatin1String("hide_download")));
        share.label = item.value(QLatin1String("label")).toString();
        share.owner = item.value(QLatin1String("uid_owner")).toString();
        share.shareWith = text(item.value(QLatin1String("share_with")));
        share.shareWithName = item.value(QLatin1String("share_with_displayname")).toString();
        if (share.type == Share::PublicLink && share.url.isEmpty())
            continue;
        parsed.append(share);
    }
    *shares = parsed;
    return true;
}

QString shareToken(const QUrl &link, const QUrl &server)
{
    if (link.scheme() != server.scheme() || link.host().compare(server.host(), Qt::CaseInsensitive) != 0
        || link.port(link.scheme() == QLatin1String("https") ? 443 : 80)
            != server.port(server.scheme() == QLatin1String("https") ? 443 : 80))
        return {};
    QString base = server.path(QUrl::FullyDecoded);
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    const QString path = link.path(QUrl::FullyDecoded);
    if (!path.startsWith(base + QLatin1Char('/')))
        return {};
    return tokenInPath(path.mid(base.size()));
}

QList<QUrl> findShareLinks(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral("https?://[^\\s<>\"']+?/s/[A-Za-z0-9]{8,64}(?:/"
                                                           "(?:download|preview))?(?=[\\s<>\"'.,;:!?)\\]]|$)"));
    QList<QUrl> links;
    auto it = pattern.globalMatch(text);
    while (it.hasNext()) {
        const QUrl url(it.next().captured(0), QUrl::StrictMode);
        if (url.isValid() && !url.host().isEmpty() && url.userInfo().isEmpty() && !links.contains(url))
            links.append(url);
    }
    return links;
}

const Share *findLinkShare(const QList<Share> &shares, const QString &token)
{
    if (token.isEmpty())
        return nullptr;
    for (const Share &share : shares) {
        if (share.isLink() && share.token == token)
            return &share;
    }
    return nullptr;
}

Visibility visibility(const Share *share)
{
    if (!share)
        return Visibility::Unknown;
    return share->hasPassword ? Visibility::PasswordProtected : Visibility::Public;
}

bool allowsChanges(const Share &share)
{
    return share.permissions & (Share::Update | Share::Create | Share::Delete);
}

QStringList describe(const Share *share)
{
    if (!share) {
        return {tr("This link is not one of your shares, so who can open it and "
                   "what they can do is unknown."),
            tr("Check the link in Nextcloud before sending it.")};
    }
    QStringList lines;
    const bool password = share->hasPassword;
    if (share->folder) {
        const int p = share->permissions;
        if ((p & Share::Create) && !(p & Share::Read))
            lines << (password ? tr("Anyone with the link and its password can upload "
                                    "files to this folder, without seeing its contents.")
                               : tr("Anyone with the link can upload files to this "
                                    "folder, without seeing its contents."));
        else if (allowsChanges(*share))
            lines << (password ? tr("Anyone with the link and its password can view, "
                                    "upload, change and delete files in this folder.")
                               : tr("Anyone with the link can view, upload, change "
                                    "and delete files in this folder."));
        else
            lines << (password ? tr("Anyone with the link and its password can view "
                                    "the files in this folder.")
                               : tr("Anyone with the link can view the files in this "
                                    "folder."));
    } else if (allowsChanges(*share)) {
        lines << (password ? tr("Anyone with the link and its password can view and "
                                "edit this file.")
                           : tr("Anyone with the link can view and edit this file."));
    } else {
        lines << (password ? tr("Anyone with the link and its password can view "
                                "this file.")
                           : tr("Anyone with the link can view this file."));
    }
    if (share->type == Share::Email && !share->shareWith.isEmpty())
        lines << tr("The link was shared by email with %1.").arg(share->shareWith);
    lines << (password ? tr("The link is protected by a password, which is not "
                            "included in the message.")
                       : tr("The link has no password."));
    if (share->expiration.isValid())
        lines << tr("The link expires on %1.").arg(QLocale().toString(share->expiration, QLocale::LongFormat));
    else
        lines << tr("The link does not expire.");
    if (share->hideDownload)
        lines << tr("The download button is hidden on the share's page.");
    return lines;
}

} // namespace Nextcloud
