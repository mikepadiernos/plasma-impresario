/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QString>
#include <QUrl>

class KConfig;
class KConfigGroup;

namespace UiUtils {

/** @short Finding files which the user saved from mail, such as attachments and exported messages

The search runs in the default file manager, such as Plasma Opus or Dolphin, through KIO URLs: baloosearch: asks
Baloo's file index, and filenamesearch: walks the folder live. Nothing is sent to Baloo; it only finds what it
indexed from the user's files on its own.
*/
namespace FileSearch {

/** How well Baloo's file index covers a folder */
enum class Coverage {
    /** The baloosearch: KIO worker is not installed */
    NotInstalled,
    /** File indexing is turned off */
    Disabled,
    /** The folder is outside every indexed folder */
    NotIncluded,
    /** The folder, or one of its parents, is excluded */
    Excluded,
    /** The folder is inside a hidden folder, which Baloo skips */
    Hidden,
    /** Only the names and basic properties of files are indexed, not their text */
    NamesOnly,
    Covered,
};

struct Status {
    Coverage coverage = Coverage::NotInstalled;
    /** The folder which decided the coverage: the excluded folder, the hidden folder or the indexed folder; empty
    when none did */
    QString decidingFolder;

    /** Whether a Baloo search can find files in the folder, if only by name */
    bool isSearchable() const;
};

/** The coverage of @arg folder by the Baloo settings in @arg balooConfig, normally baloofilerc

Follows Baloo's own rules: the closest configured folder decides whether a folder is indexed, and hidden folders
below it are skipped unless hidden folders are indexed. Exclude filters on file and folder names are not checked.
*/
Status coverage(const KConfig &balooConfig, const QString &folder, bool installed);

/** A baloosearch: URL finding @arg text in @arg folder and below, titled @arg title in the file manager */
QUrl indexedSearchUrl(const QString &text, const QString &folder, const QString &title);

/** A filenamesearch: URL finding @arg text in the names and contents of files in @arg folder and below */
QUrl liveSearchUrl(const QString &text, const QString &folder, const QString &title);

/** The folder in which the user last saved an attachment or a message, or @arg fallback if it no longer exists */
QString savedFilesFolder(const KConfigGroup &state, const QString &fallback);

/** Remember the folder of @arg filePath, a file which the user saved */
void rememberSavedFile(KConfigGroup &state, const QString &filePath);

}

}
