/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QKeySequence>
#include <QList>
#include <QString>
#include <QStringList>

namespace UiUtils {

/** @short Data behind the read-only overview of keyboard shortcuts */
namespace ShortcutOverview {

struct Entry {
    /** Object name of the action, such as "action_archive" */
    QString id;
    /** Menu or part of the window the action belongs to */
    QString area;
    /** Action text as shown in menus; see plainText() */
    QString text;
    QList<QKeySequence> shortcuts;
};

struct Group {
    QString area;
    QList<Entry> entries;
};

/** @short @arg text without mnemonic markers and a trailing ellipsis, "&Move to Folder…" becoming "Move to Folder" */
QString plainText(const QString &text);

/** @short The shortcuts of @arg entry for display, in the native format of the platform */
QString shortcutText(const Entry &entry);

/** @short Group @arg entries by area

Areas listed in @arg areaOrder come first and in that order, the others follow alphabetically. Within a group
the entries are sorted by their plain text.
*/
QList<Group> group(const QList<Entry> &entries, const QStringList &areaOrder = QStringList());

/** @short Keep the entries whose text, shortcut or area contains @arg query, ignoring case; empty groups are dropped */
QList<Group> filter(const QList<Group> &groups, const QString &query);

/** @short Shortcuts which are used by more than one entry, in portable text form, sorted */
QStringList duplicateShortcuts(const QList<Entry> &entries);

}

}
