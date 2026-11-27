/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDateTime>
#include <QString>

namespace Common {

/** @short Keep the offline cache and opened attachments readable only by the user

Folders are created and kept at 0700 and files at 0600, never executable. An attachment opened in another
application is copied into a folder of its own inside the cache, under a name that cannot leave that folder, and
copies older than a day are removed.
*/
namespace PrivateFiles {

/** How long a copy of an opened attachment is kept, in seconds */
constexpr qint64 openedAttachmentLifetime = 24 * 60 * 60;

/** Create @arg path and any missing parent folders, each readable only by the user

An existing @arg path is made user-only as well; existing parents are left as they are. Returns false and sets
@arg error when a folder cannot be created or its permissions cannot be set.
*/
bool makeFolder(const QString &path, QString *error = nullptr);

/** Make @arg root and everything below it user-only: folders 0700, files 0600

Symbolic links are neither followed nor changed. Returns the number of entries whose permissions were changed.
*/
int tighten(const QString &root);

/** A file name taken from a message, made safe to create inside a folder

Folder parts, control characters, bidirectional overrides and leading dots are dropped, and the name is shortened to
fit a file system's limit while keeping its extension. An empty result becomes "attachment".
*/
QString safeFileName(const QString &name);

/** The folder which holds copies of opened attachments, inside the cache of the current profile */
QString openedAttachmentsFolder();

/** Create an empty user-only file for an opened attachment called @arg name, in a new folder inside @arg folder

Returns the file's path, or an empty string and @arg error.
*/
QString createOpenedAttachment(const QString &folder, const QString &name, QString *error = nullptr);

/** Remove the copy of an opened attachment at @arg path together with its folder */
void removeOpenedAttachment(const QString &folder, const QString &path);

/** Remove the copies in @arg folder which were last changed more than a day before @arg now

Returns the number of copies removed.
*/
int removeOldOpenedAttachments(const QString &folder, const QDateTime &now = QDateTime::currentDateTimeUtc());

/** The folder which holds private copies of files fetched from other computers to be attached, inside the cache of
the current profile; each is created with createOpenedAttachment() and removed with removeOpenedAttachment() */
QString fetchedAttachmentsFolder();

/** Remove the copies in @arg folder which were last changed before @arg time, such as those left by a message being
composed when the application ended unexpectedly

Returns the number of copies removed.
*/
int removeCopiesChangedBefore(const QString &folder, const QDateTime &time);

}

}
