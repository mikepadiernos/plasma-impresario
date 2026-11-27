/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "SearchSuggestions.h"

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QSet>

#include "Imap/Model/ItemRoles.h"
#include "UiUtils/SearchQuery.h"
#include "UiUtils/SearchScope.h"

namespace UiUtils {

SearchHistory::SearchHistory(const QStringList &entries, const int limit)
    : m_limit(qMax(1, limit))
{
    for (const QString &entry : entries) {
        if (m_entries.size() >= m_limit)
            break;
        const QString query = entry.trimmed();
        if (!query.isEmpty() && !m_entries.contains(query))
            m_entries << query;
    }
}

bool SearchHistory::add(const QString &query)
{
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty() || (!m_entries.isEmpty() && m_entries.first() == trimmed))
        return false;
    m_entries.removeAll(trimmed);
    m_entries.prepend(trimmed);
    while (m_entries.size() > m_limit)
        m_entries.removeLast();
    return true;
}

void SearchHistory::clear()
{
    m_entries.clear();
}

QStringList SearchHistory::entries() const
{
    return m_entries;
}

int SearchHistory::limit() const
{
    return m_limit;
}

QString SearchHistory::profileKey(const QString &profileName)
{
    return profileName.isEmpty() ? QStringLiteral("default") : profileName;
}

namespace SearchSuggestions {

namespace {

const QStringList contactFields = {
    QStringLiteral("from"), QStringLiteral("to"), QStringLiteral("cc"), QStringLiteral("bcc"),
};

bool insideQuotes(const QString &text)
{
    bool quoted = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (quoted && c == QLatin1Char('\\'))
            ++i;
        else if (c == QLatin1Char('"'))
            quoted = !quoted;
    }
    return quoted;
}

}

Token currentToken(const QString &text)
{
    Token token;
    if (text.isEmpty() || text.startsWith(QLatin1String(":=")) || text.back().isSpace() || insideQuotes(text))
        return token;
    int start = text.size();
    while (start > 0 && !text.at(start - 1).isSpace())
        --start;
    const QString word = text.mid(start);
    int prefixLength = 0;
    while (prefixLength < word.size() && (word.at(prefixLength) == QLatin1Char('-') || word.at(prefixLength) == QLatin1Char('(')))
        ++prefixLength;
    const QString rest = word.mid(prefixLength);
    if (rest.isEmpty() || rest.contains(QLatin1Char('"')) || rest.contains(QLatin1Char(')')))
        return token;
    const int colon = rest.indexOf(QLatin1Char(':'));
    if (colon >= 0) {
        const QString field = rest.left(colon).toLower();
        if (!contactFields.contains(field) && field != QLatin1String("tag"))
            return token;
        token.field = field;
        token.value = rest.mid(colon + 1);
    } else {
        token.value = rest;
    }
    token.start = start;
    token.prefix = word.left(prefixLength);
    return token;
}

QString contactQuery(const QString &text)
{
    const Token token = currentToken(text);
    if (!token.isValid() || (!token.field.isEmpty() && !contactFields.contains(token.field)))
        return QString();
    if (token.value.size() < (token.field.isEmpty() ? 2 : 1))
        return QString();
    return token.value;
}

QList<Suggestion> suggest(const QString &text, const QStringList &history, const QList<Contact> &contacts,
                          const QStringList &tags, const QList<FolderCandidate> &folders, const int maxPerKind)
{
    QList<Suggestion> result;
    const QString typed = text.trimmed();

    int count = 0;
    for (const QString &entry : history) {
        if (count >= maxPerKind)
            break;
        if (entry == typed || (!typed.isEmpty() && !entry.contains(typed, Qt::CaseInsensitive)))
            continue;
        result.append({Kind::RecentSearch, entry, QString(), entry, QString()});
        ++count;
    }

    const Token token = currentToken(text);
    if (!token.isValid())
        return result;
    const bool plain = token.field.isEmpty();
    // A single letter would match nearly everything
    if (plain && token.value.size() < 2)
        return result;
    const QString head = text.left(token.start) + token.prefix;
    const QString typedTerm = text.mid(token.start + token.prefix.size());

    if (plain || contactFields.contains(token.field)) {
        const QString field = plain ? QStringLiteral("from") : token.field;
        QSet<QString> seen;
        count = 0;
        for (const Contact &contact : contacts) {
            if (count >= maxPerKind)
                break;
            const QString email = contact.email.trimmed();
            if (email.isEmpty() || seen.contains(email.toLower()))
                continue;
            // Answers of the address book may be about a word typed earlier
            if (!token.value.isEmpty() && !email.contains(token.value, Qt::CaseInsensitive)
                && !contact.name.contains(token.value, Qt::CaseInsensitive))
                continue;
            const QString term = field + QLatin1Char(':') + SearchQuery::quotedValue(email);
            if (term.compare(typedTerm, Qt::CaseInsensitive) == 0)
                continue;
            seen.insert(email.toLower());
            result.append({Kind::Contact, email, contact.name.trimmed(), head + term, QString()});
            ++count;
        }
    }

    if (plain || token.field == QLatin1String("tag")) {
        count = 0;
        for (const QString &tag : tags) {
            if (count >= maxPerKind)
                break;
            if (tag.isEmpty() || !tag.contains(token.value, Qt::CaseInsensitive))
                continue;
            const QString term = QLatin1String("tag:") + SearchQuery::quotedValue(tag);
            if (term == typedTerm)
                continue;
            result.append({Kind::Tag, tag, QString(), head + term, QString()});
            ++count;
        }
    }

    // A folder is a place to search rather than a term, so "-" or "(" in front of the word rules it out
    if (plain && token.prefix.isEmpty()) {
        QString rest = text.left(token.start);
        while (!rest.isEmpty() && rest.back().isSpace())
            rest.chop(1);
        count = 0;
        for (const FolderCandidate &folder : folders) {
            if (count >= maxPerKind)
                break;
            if (folder.key.isEmpty() || !folder.path.contains(token.value, Qt::CaseInsensitive))
                continue;
            result.append({Kind::Folder, folder.path, QString(), rest, folder.key});
            ++count;
        }
    }
    return result;
}

namespace {

void collectFolders(QAbstractItemModel *model, const QModelIndex &parent, const QString &accountId, const QString &parentPath,
                    QList<FolderCandidate> &folders)
{
    using namespace Imap::Mailbox;
    const int rows = model->rowCount(parent);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        const QString name = index.data(RoleMailboxName).toString();
        QString shortName = index.data(RoleShortMailboxName).toString();
        if (shortName.isEmpty())
            shortName = name;
        const QString path = parentPath.isEmpty()
            ? shortName
            : QCoreApplication::translate("UiUtils::SearchSuggestions", "%1 / %2", "folder path, %1 parent, %2 child")
                  .arg(parentPath, shortName);
        if (!name.isEmpty() && index.data(RoleMailboxIsSelectable).toBool())
            folders.append({SearchScope::folderKey({accountId, name}), path});
        // Asking an unlisted branch for its children would send a LIST command
        const QVariant fetched = index.data(RoleIsFetched);
        if ((!fetched.isValid() || fetched.toBool()) && model->hasChildren(index))
            collectFolders(model, index, accountId, path, folders);
    }
}

}

QList<FolderCandidate> knownFolders(QAbstractItemModel *mailboxes, const QString &accountId)
{
    QList<FolderCandidate> folders;
    if (mailboxes && !accountId.isEmpty())
        collectFolders(mailboxes, QModelIndex(), accountId, QString(), folders);
    return folders;
}

}

}
