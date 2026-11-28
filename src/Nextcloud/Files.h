/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

namespace Nextcloud {

/** @short What the user may do with a file or folder, from the letters of the
oc:permissions property

A file or folder shared with the user may lack letters its owner has; the server
enforces them, and the client uses them to offer only what will work.
*/
struct Permissions {
    QString letters;

    /** S: shared with the user by someone else */
    bool isShared() const;
    /** M: on external storage */
    bool isMounted() const;
    /** R: may be shared again */
    bool canShare() const;
    /** W: the file's content may be changed */
    bool canWrite() const;
    /** C: files may be created in the folder */
    bool canCreateFile() const;
    /** K: folders may be created in the folder */
    bool canCreateFolder() const;
    bool canDelete() const;
    bool canRename() const;
    /** Whether the letters were reported at all; old servers and some storages do
     * not */
    bool isKnown() const;

    bool operator==(const Permissions &other) const = default;
};

/** @short A file or folder in a WebDAV listing */
struct Entry {
    /** The path in the user's files, starting with a slash */
    QString path;
    QString name;
    bool folder = false;
    qint64 size = 0;
    QString contentType;
    QDateTime modified;
    QString etag;
    qint64 fileId = 0;
    Permissions permissions;
    /** The oc:share-types of shares on the entry, which only its owner sees; 3 is
     * a public link */
    QList<int> shareTypes;

    bool hasLinkShare() const;
    bool operator==(const Entry &other) const = default;
};

/** @short A folder and what it contains, folders first, then by name */
struct Listing {
    Entry folder;
    QList<Entry> children;
};

/** @short The body of a PROPFIND asking for the properties of Entry */
QByteArray propfindBody();

/** @short The listing in a WebDAV multistatus answer @arg xml

@arg davPath is the decoded path of the user's files on the server, as
Account::davPath(), and @arg requested the path asked for. Entries outside the
user's files are skipped. Returns false and sets @arg error when the answer is
no multistatus or does not contain the requested folder.
*/
bool parseListing(
    const QByteArray &xml, const QString &davPath, const QString &requested, Listing *listing, QString *error);

} // namespace Nextcloud
