/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QString>
#include <QUrl>

namespace Nextcloud {

/** @short A Nextcloud account the user connected, without its app password

The app password is kept in encrypted storage by CredentialStore, or by KDE
Online Accounts for accounts which come from there; it is never part of the
account, its settings or a URL.
*/
struct Account {
    enum class Source {
        /** Connected in the settings, through Login Flow v2 or an app password */
        Manual,
        /** Discovered through KDE Online Accounts, which keeps the credentials */
        OnlineAccounts,
    };

    QString id;
    /** The address of the server, such as https://cloud.example.com or
     * https://example.com/nextcloud */
    QUrl server;
    /** What the user signs in with, which may be an email address */
    QString loginName;
    /** The user ID in WebDAV paths, which the server reports */
    QString userId;
    QString displayName;
    Source source = Source::Manual;
    /** The KDE Online Accounts ID of an OnlineAccounts account */
    quint32 onlineAccountId = 0;

    bool isValid() const;
    /** What the account is called in lists, such as "alice on cloud.example.com"
     */
    QString title() const;
    /** The WebDAV URL of @arg path, a path in the user's files starting with a
     * slash */
    QUrl fileUrl(const QString &path) const;
    /** The path of the user's files on the server, as in the WebDAV answers */
    QString davPath() const;
    /** The page of @arg folder in the Files app of the web interface */
    QUrl webFolderUrl(const QString &folder) const;

    bool operator==(const Account &other) const = default;
};

/** @short Whether requests with a password may go to @arg url: over TLS, or to
 * this computer */
bool isAcceptable(const QUrl &url);

/** @short The server address typed by the user, made usable; invalid and @arg
error when it cannot be

An address without a scheme gets https. A trailing slash, index.php and the
paths of the web interface are removed, so that a copied page address works too.
*/
QUrl normalizedServer(const QString &text, QString *error);

/** @short @arg path with one slash at the start, none at the end and no empty,
 * "." or ".." parts; "/" for the root */
QString cleanPath(const QString &path);
/** @short The folder which contains @arg path, or "/" */
QString parentPath(const QString &path);
/** @short @arg folder and @arg name joined with a slash */
QString childPath(const QString &folder, const QString &name);
/** @short @arg name, such as an attachment's, usable as one file name: no
 * slashes or control characters, never "." or ".." */
QString safeFileName(const QString &name);
/** @short "name (2).ext" for @arg number 2, keeping the extension of @arg name
 */
QString numberedName(const QString &name, int number);

} // namespace Nextcloud
