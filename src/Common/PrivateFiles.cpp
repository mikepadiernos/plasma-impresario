/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "PrivateFiles.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimeZone>
#include "Paths.h"

namespace Common {
namespace PrivateFiles {

namespace {

constexpr QFile::Permissions folderPermissions = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
constexpr QFile::Permissions filePermissions = QFile::ReadOwner | QFile::WriteOwner;
// The bits which QFileInfo reports for the user beside the owner's are the same as the owner's; compare these only
constexpr QFile::Permissions comparedBits = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
        | QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup | QFile::ReadOther | QFile::WriteOther | QFile::ExeOther;
// Most file systems allow 255 bytes in a name
constexpr int maxNameBytes = 255;
constexpr int maxSuffixLength = 16;

void setError(QString *error, const QString &text)
{
    if (error)
        *error = text;
}

bool setPermissions(const QFileInfo &info, QFile::Permissions wanted)
{
    if ((info.permissions() & comparedBits) == wanted)
        return false;
    return QFile::setPermissions(info.absoluteFilePath(), wanted);
}

bool isHiddenFormatting(char32_t c)
{
    // Bidirectional embeddings, overrides and isolates can make "evil\u202Egpj.exe" read as "evilexe.jpg"
    return (c >= 0x202A && c <= 0x202E) || (c >= 0x2066 && c <= 0x2069) || c == 0x200E || c == 0x200F
            || c == 0x061C || c == 0xFEFF;
}

QString shortened(const QString &name)
{
    if (name.toUtf8().size() <= maxNameBytes)
        return name;
    QString suffix;
    const auto dot = name.lastIndexOf(QLatin1Char('.'));
    if (dot > 0 && name.size() - dot <= maxSuffixLength)
        suffix = name.mid(dot);
    QString base = name.left(name.size() - suffix.size());
    while (!base.isEmpty() && (base + suffix).toUtf8().size() > maxNameBytes) {
        base.chop(1);
        // Do not leave half of a surrogate pair behind
        if (!base.isEmpty() && base.back().isHighSurrogate())
            base.chop(1);
    }
    return base + suffix;
}

QDateTime newestChange(const QFileInfo &entry)
{
    QDateTime newest = entry.lastModified(QTimeZone::UTC);
    if (entry.isDir() && !entry.isSymLink()) {
        QDirIterator it(entry.absoluteFilePath(), QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            newest = std::max(newest, it.fileInfo().lastModified(QTimeZone::UTC));
        }
    }
    return newest;
}

bool removeEntry(const QFileInfo &entry)
{
    if (entry.isDir() && !entry.isSymLink())
        return QDir(entry.absoluteFilePath()).removeRecursively();
    return QFile::remove(entry.absoluteFilePath());
}

}

bool makeFolder(const QString &path, QString *error)
{
    const QString target = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    QStringList missing;
    for (QString current = target; !QFileInfo::exists(current);) {
        missing.prepend(current);
        const QString parent = QFileInfo(current).path();
        if (parent == current)
            break;
        current = parent;
    }
    for (const QString &folder : std::as_const(missing)) {
        // Created with the permissions already in place, so that the folder is never open to others
        if (!QDir().mkdir(folder, folderPermissions) && !QFileInfo(folder).isDir()) {
            setError(error, QStringLiteral("Cannot create the folder %1").arg(folder));
            return false;
        }
    }
    const QFileInfo info(target);
    if (!info.isDir()) {
        setError(error, QStringLiteral("%1 is not a folder").arg(target));
        return false;
    }
    if ((info.permissions() & comparedBits) != folderPermissions && !QFile::setPermissions(target, folderPermissions)) {
        setError(error, QStringLiteral("Cannot make the folder %1 private").arg(target));
        return false;
    }
    return true;
}

int tighten(const QString &root)
{
    const QFileInfo rootInfo(root);
    if (!rootInfo.isDir() || rootInfo.isSymLink())
        return 0;
    int changed = setPermissions(rootInfo, folderPermissions) ? 1 : 0;
    // QDirIterator does not descend into linked folders unless asked to
    QDirIterator it(root, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        if (info.isSymLink())
            continue;
        if (info.isDir()) {
            changed += setPermissions(info, folderPermissions) ? 1 : 0;
        } else if (info.isFile()) {
            // Read-only copies stay read-only; anything else becomes 0600
            const QFile::Permissions wanted = (info.permissions() & QFile::WriteOwner) ? filePermissions : QFile::ReadOwner;
            changed += setPermissions(info, wanted) ? 1 : 0;
        }
    }
    return changed;
}

QString safeFileName(const QString &name)
{
    QString base = name;
    const auto slash = std::max(base.lastIndexOf(QLatin1Char('/')), base.lastIndexOf(QLatin1Char('\\')));
    if (slash >= 0)
        base = base.mid(slash + 1);

    QString cleaned;
    cleaned.reserve(base.size());
    for (const char32_t c : base.toUcs4()) {
        if (c < 0x20 || (c >= 0x7F && c < 0xA0) || isHiddenFormatting(c))
            continue;
        cleaned.append(QString::fromUcs4(&c, 1));
    }

    cleaned = cleaned.trimmed();
    // A leading dot hides the file and ".." names the parent; trailing dots and spaces confuse other systems
    while (cleaned.startsWith(QLatin1Char('.')))
        cleaned.remove(0, 1);
    while (cleaned.endsWith(QLatin1Char('.')) || cleaned.endsWith(QLatin1Char(' ')))
        cleaned.chop(1);
    cleaned = cleaned.trimmed();
    if (cleaned.isEmpty())
        return QStringLiteral("attachment");
    return shortened(cleaned);
}

QString openedAttachmentsFolder()
{
    return writablePath(LOCATION_CACHE) + QLatin1String("Opened Attachments");
}

QString createOpenedAttachment(const QString &folder, const QString &name, QString *error)
{
    if (!makeFolder(folder, error))
        return {};
    // A folder for each copy keeps the attachment's own name, so the other application shows the name the sender used
    QTemporaryDir copyFolder(QDir(folder).filePath(QStringLiteral("XXXXXX")));
    if (!copyFolder.isValid()) {
        setError(error, copyFolder.errorString());
        return {};
    }
    copyFolder.setAutoRemove(false);
    QFile::setPermissions(copyFolder.path(), folderPermissions);
    QFile file(QDir(copyFolder.path()).filePath(safeFileName(name)));
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly, filePermissions)) {
        setError(error, file.errorString());
        QDir(copyFolder.path()).removeRecursively();
        return {};
    }
    return file.fileName();
}

void removeOpenedAttachment(const QString &folder, const QString &path)
{
    if (path.isEmpty())
        return;
    const QFileInfo file(path);
    const QString copyFolder = file.absolutePath();
    QFile::remove(path);
    // Only a folder directly inside the attachments folder is removed, never anything outside it
    if (QDir::cleanPath(QFileInfo(copyFolder).path()) == QDir::cleanPath(QFileInfo(folder).absoluteFilePath()))
        QDir(copyFolder).removeRecursively();
}

int removeOldOpenedAttachments(const QString &folder, const QDateTime &now)
{
    const QFileInfo folderInfo(folder);
    if (!folderInfo.isDir() || folderInfo.isSymLink())
        return 0;
    int removed = 0;
    const auto entries = QDir(folder).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        if (newestChange(entry).secsTo(now) > openedAttachmentLifetime && removeEntry(entry))
            ++removed;
    }
    return removed;
}

QString fetchedAttachmentsFolder()
{
    return writablePath(LOCATION_CACHE) + QLatin1String("Fetched Attachments");
}

int removeCopiesChangedBefore(const QString &folder, const QDateTime &time)
{
    const QFileInfo folderInfo(folder);
    if (!folderInfo.isDir() || folderInfo.isSymLink())
        return 0;
    int removed = 0;
    const auto entries = QDir(folder).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        if (newestChange(entry) < time && removeEntry(entry))
            ++removed;
    }
    return removed;
}

}
}
