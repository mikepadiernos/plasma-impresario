/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace Nextcloud {

/** @short A share of a file or folder, from the OCS Share API

Plasma Impresario only reads shares. It never creates, broadens or revokes one;
the user does that in Nextcloud.
*/
struct Share {
    /** The share types of the OCS Share API */
    enum Type {
        User = 0,
        Group = 1,
        PublicLink = 3,
        Email = 4,
        FederatedCloud = 6,
        Team = 7,
        TalkRoom = 10,
    };
    /** The permission bits of the OCS Share API */
    enum Permission {
        Read = 1,
        Update = 2,
        Create = 4,
        Delete = 8,
        Reshare = 16,
    };

    QString id;
    int type = PublicLink;
    /** The path of the shared file or folder in the owner's files */
    QString path;
    bool folder = false;
    QUrl url;
    QString token;
    int permissions = Read;
    /** Invalid when the share does not expire */
    QDate expiration;
    bool hasPassword = false;
    bool hideDownload = false;
    QString label;
    QString owner;
    QString shareWith;
    QString shareWithName;

    bool isLink() const;
    bool operator==(const Share &other) const = default;
};

/** @short Who can open a link, and what they can do there */
enum class Visibility {
    /** Anyone who has the link */
    Public,
    /** Anyone who has the link and its password */
    PasswordProtected,
    /** The link is not one of the user's shares, so who can open it is not known
     */
    Unknown,
};

/** @short The shares in an OCS Share API answer in JSON; false and @arg error
 * when it is not one */
bool parseShares(const QByteArray &json, QList<Share> *shares, QString *error);

/** @short The token of a public share link of @arg server, such as
https://cloud.example.com/s/TOKEN; empty otherwise

Links through index.php and to the download page of a share are recognized too.
*/
QString shareToken(const QUrl &link, const QUrl &server);

/** @short The links in @arg text which look like public share links of any
 * Nextcloud, in order and without repeats */
QList<QUrl> findShareLinks(const QString &text);

/** @short The link share with @arg token among @arg shares, or nullptr */
const Share *findLinkShare(const QList<Share> &shares, const QString &token);

Visibility visibility(const Share *share);

/** @short One sentence each about who can open @arg share, what they can do,
its password and its expiry

@arg share may be nullptr for a link which is not one of the user's shares.
*/
QStringList describe(const Share *share);

/** @short Whether recipients can change, add or delete files through @arg share
 */
bool allowsChanges(const Share &share);

} // namespace Nextcloud
