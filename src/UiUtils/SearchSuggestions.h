/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

class QAbstractItemModel;

namespace UiUtils {

/** @short Queries which the user ran from the quick search, most recent first */
class SearchHistory
{
public:
    static constexpr int DefaultLimit = 20;

    /** @short Start from @arg entries, dropping empty and repeated queries and anything beyond @arg limit */
    explicit SearchHistory(const QStringList &entries = QStringList(), int limit = DefaultLimit);

    /** @short Put @arg query first, dropping an older copy and anything beyond the limit; returns true on a change

    Surrounding white space is ignored, and an empty query is not remembered.
    */
    bool add(const QString &query);
    void clear();

    QStringList entries() const;
    int limit() const;

    /** @short The config key of the history of @arg profileName, the value of --profile; "default" without one */
    static QString profileKey(const QString &profileName);

private:
    QStringList m_entries;
    int m_limit;
};

namespace SearchSuggestions {

enum class Kind {
    /** A whole query from the search history */
    RecentSearch,
    /** A from:, to:, cc: or bcc: term for an address of the address book */
    Contact,
    /** A tag: term */
    Tag,
    /** Searching a folder instead of the word typed for it */
    Folder,
};

/** @short A person of the address book */
struct Contact {
    QString name;
    QString email;
};

/** @short A folder which a search can look in */
struct FolderCandidate {
    /** As made by SearchScope::folderKey() */
    QString key;
    /** What the user sees, such as "Archive / 2024" */
    QString path;

    bool operator==(const FolderCandidate &other) const = default;
};

/** @short One entry offered below the quick search */
struct Suggestion {
    Kind kind = Kind::RecentSearch;
    /** The query, the address, the tag or the folder path */
    QString value;
    /** For contacts, the name in the address book */
    QString detail;
    /** The text of the quick search once the suggestion is chosen */
    QString text;
    /** For folders, the folder to search, as made by SearchScope::folderKey() */
    QString folderKey;

    bool operator==(const Suggestion &other) const = default;
};

/** @short The word being typed at the end of a query, which suggestions complete */
struct Token {
    /** Where the word starts in the query; -1 when there is nothing to complete */
    int start = -1;
    /** Characters such as "-" or "(" in front of the word, which a completed term keeps */
    QString prefix;
    /** The field name in lower case, such as "from" or "tag"; empty for a plain word */
    QString field;
    QString value;

    bool isValid() const { return start >= 0; }
};

/** @short The word at the end of @arg text; invalid after white space, inside quotes and in raw IMAP queries

Only plain words and the fields which suggestions complete, from:, to:, cc:, bcc: and tag:, are words to complete.
*/
Token currentToken(const QString &text);

/** @short Suggestions for @arg text, recent searches first, then contacts, tags and folders

Recent searches match the whole text; the others complete the word at its end, and plain words need at least two
characters. A plain word offers from: terms for contacts. At most @arg maxPerKind entries of each kind are offered.
*/
QList<Suggestion> suggest(const QString &text, const QStringList &history, const QList<Contact> &contacts,
                          const QStringList &tags, const QList<FolderCandidate> &folders, int maxPerKind = 5);

/** @short The word of @arg text which the address book should be asked to complete; empty when none */
QString contactQuery(const QString &text);

/** @short The selectable folders of @arg mailboxes, a mailbox tree which uses the Imap::Mailbox roles

Only branches whose children are already known are walked, so this never makes an IMAP model contact the server.
*/
QList<FolderCandidate> knownFolders(QAbstractItemModel *mailboxes, const QString &accountId);

}

}
