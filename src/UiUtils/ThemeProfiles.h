/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

#include "UiUtils/Theme.h"

namespace UiUtils {

/** @short Named, versioned sets of appearance options which can be saved,
exported and imported

A profile is plain data: importing one validates and stores option values, and
never runs code, loads other files or reaches the network. The format is shared
with Plasma Opus. A file written for Plasma Opus holds only the options both
applications have, with the message list font as its file list font.
*/
namespace ThemeProfiles {

/** @short The "format" of a Plasma Impresario profile, and of a Plasma Opus
 * profile, which is read too */
inline constexpr auto Format = "plasma-impresario-theme";
inline constexpr auto OpusFormat = "plasma-opus-theme";
/** @short The newest format version read, and the one written */
inline constexpr int Version = 1;
/** @short Larger files are refused before they are parsed */
inline constexpr qint64 MaximumFileSize = 512 * 1024;
inline constexpr int MaximumProfiles = 100;
inline constexpr int MaximumNameLength = 100;

struct Profile {
    QString id;
    QString name;
    /** Values of Theme::profileEntries(); entries a file leaves out have their
     * defaults */
    Theme::Values values;

    bool operator==(const Profile &other) const = default;
};

/** @short Who a written profile is for */
enum class Target {
    Impresario,
    Opus,
};

/** @short Why @arg name cannot name a profile, or an empty string: blank, too
 * long or with control characters */
QString nameError(const QString &name);

/** @short @arg profile as a JSON document; @arg withStyleSheet false leaves the
 * stylesheet out */
QByteArray toJson(const Profile &profile, Target target = Target::Impresario, bool withStyleSheet = true);

struct ParseResult {
    /** Without an id; std::nullopt when the file is refused */
    std::optional<Profile> profile;
    QString error;
    /** Skipped unknown options */
    QStringList warnings;
    /** Whether the file came from Plasma Opus */
    bool fromOpus = false;
};
ParseResult fromJson(const QByteArray &json);
/** @short Read the profile file at @arg path, refusing files over
 * MaximumFileSize */
ParseResult importFile(const QString &path);
/** @short Write @arg profile to @arg path; an error, or an empty string */
QString exportFile(const Profile &profile, const QString &path, Target target = Target::Impresario,
                   bool withStyleSheet = true);

/** @short The stored form of @arg profiles, one JSON document each, and back;
 * invalid ones are reported */
QStringList store(const QList<Profile> &profiles);
QList<Profile> restore(const QStringList &stored, QStringList *problems = nullptr);

/** @short Why @arg profiles cannot be saved, or an empty string: an invalid or
 * repeated name, or too many */
QString listError(const QList<Profile> &profiles);
/** @short An id not used by @arg profiles */
QString newId(const QList<Profile> &profiles);
/** @short The index of the profile named @arg name, ignoring case, or -1 */
qsizetype indexOfName(const QList<Profile> &profiles, const QString &name);
const Profile *find(const QList<Profile> &profiles, const QString &id);

/** @short @arg global with the options of @arg profile in its place, as when a profile or a followed file becomes the
global appearance; options the profile does not hold keep their global values */
Theme::Values appliedTo(const Theme::Values &global, const Profile &profile);
/** @short A profile called @arg name holding the profile options of @arg values */
Profile fromValues(const QString &name, const Theme::Values &values);

} // namespace ThemeProfiles

} // namespace UiUtils
