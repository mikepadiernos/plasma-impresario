/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QColor>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

namespace UiUtils {

/** @short Colours of accounts and folders, and the rules which colour folders

Colours are local: they are kept in the application's configuration and never
change folders, messages or anything on the server. A folder's own colour wins
over the rules; the rules are checked in order and the first match wins. Colour
is never the only sign of an account, which is always named too.
*/
namespace FolderColors {

inline constexpr int MaximumRules = 50;

enum class Match {
    /** A wildcard pattern for the folder's path, with "/" between its parts, such
       as "Lists/*" */
    Path,
    /** A case-insensitive wildcard pattern for the last part of the folder's
       name, such as "Archive*" */
    Name,
};

struct Rule {
    /** The account the rule is for, or empty for every account */
    QString account;
    Match match = Match::Name;
    QString pattern;
    QColor color;
    /** Whether folders below a matching folder take its colour too */
    bool subfolders = false;

    bool operator==(const Rule &other) const = default;
};

/** @short A stored rule, a JSON object as in Plasma Opus with the account
 * added; std::nullopt when invalid */
std::optional<Rule> parse(const QString &entry);
QString entry(const Rule &rule);
/** @short The valid rules of @arg entries, without duplicates and at most
 * MaximumRules */
QList<Rule> rules(const QStringList &entries);
QStringList entries(const QList<Rule> &rules);

/** @short @arg folder of an account as a path with "/" between its parts */
QString normalizedPath(const QString &folder, QChar separator);

/** @short The colour of the first rule matching @arg folder of @arg accountId,
 * or an invalid colour */
QColor ruleColor(const QList<Rule> &rules, const QString &accountId, const QString &folder, QChar separator);

struct Colors {
    /** By account id */
    QHash<QString, QColor> accounts;
    /** By account id, then by folder name as the server names it */
    QHash<QString, QHash<QString, QColor>> folders;
    QList<Rule> rules;
};

/** @short The colour of @arg accountId, or an invalid colour */
QColor accountColor(const Colors &colors, const QString &accountId);
/** @short The folder's own colour, else the first matching rule's, else an
 * invalid colour */
QColor folderColor(const Colors &colors, const QString &accountId, const QString &folder, QChar separator);

/** @short The colours an icon theme offers coloured folder icons in, such as
 * Breeze's "folder-red" */
QStringList iconColors();
/** @short The coloured folder icon closest to @arg color, such as "folder-red",
 * or empty for an invalid colour */
QString iconName(const QColor &color);

} // namespace FolderColors

} // namespace UiUtils
