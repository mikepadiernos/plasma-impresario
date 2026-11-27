/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "SearchSnippets.h"

#include <algorithm>
#include <QModelIndex>

#include "Imap/Model/Cache.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/SearchIndexer.h"
#include "Imap/Model/Utils.h"

namespace Imap
{
namespace Mailbox
{

SearchSnippets::SearchSnippets(QObject *parent)
    : QObject(parent)
{
}

void SearchSnippets::setTerms(const QStringList &terms)
{
    if (terms == m_terms)
        return;
    m_terms = terms;
    m_cache.clear();
    emit changed();
}

QStringList SearchSnippets::terms() const
{
    return m_terms;
}

void SearchSnippets::setTextSource(const TextSource &source)
{
    disconnect(m_watch);
    m_model = nullptr;
    m_source = source;
    m_cache.clear();
    emit changed();
}

void SearchSnippets::setModel(Model *model)
{
    if (model && model == m_model)
        return;
    TextSource source;
    if (model) {
        QPointer<Model> guarded(model);
        source = [guarded](const QString &mailbox, uint uid) {
            const std::shared_ptr<AbstractCache> cache = guarded ? guarded->cache() : nullptr;
            return cache ? SearchIndexer::downloadedText(*cache, mailbox, uid) : QString();
        };
    }
    setTextSource(source);
    m_model = model;
    if (model)
        m_watch = connect(model, &QAbstractItemModel::dataChanged, this, &SearchSnippets::partChanged);
}

bool SearchSnippets::isActive() const
{
    return !m_terms.isEmpty() && m_source;
}

QVariant SearchSnippets::snippet(const QModelIndex &index) const
{
    if (!isActive() || !index.isValid())
        return QVariant();
    const QModelIndex message = Imap::deproxifiedIndex(index);
    const uint uid = message.data(RoleMessageUid).toUInt();
    const QString mailbox = message.parent().parent().data(RoleMailboxName).toString();
    if (!uid || mailbox.isEmpty())
        return QVariant();
    const std::pair<QString, uint> key{mailbox, uid};
    auto it = m_cache.constFind(key);
    if (it == m_cache.cend())
        it = m_cache.insert(key, make(m_source(mailbox, uid), m_terms));
    return QVariant::fromValue(*it);
}

void SearchSnippets::partChanged(const QModelIndex &topLeft)
{
    if (m_cache.isEmpty() || !m_model || topLeft.model() != m_model)
        return;
    // Only a downloaded part brings new text
    const auto *part = dynamic_cast<const TreeItemPart *>(static_cast<TreeItem *>(topLeft.internalPointer()));
    const TreeItemMessage *message = part ? part->message() : nullptr;
    const auto *mailbox = message && message->parent()
        ? dynamic_cast<const TreeItemMailbox *>(message->parent()->parent()) : nullptr;
    if (!mailbox || !m_cache.remove({mailbox->mailbox(), message->uid()}))
        return;
    emit changed();
}

SearchSnippet SearchSnippets::make(const QString &text, const QStringList &terms, int length)
{
    SearchSnippet snippet;
    const QString plain = text.simplified();
    if (plain.isEmpty())
        return snippet;

    // A phrase which the text lacks as a whole may still have its words in it
    QStringList needles;
    for (const QString &term : terms) {
        if (term.isEmpty())
            continue;
        const QStringList parts = plain.contains(term, Qt::CaseInsensitive)
            ? QStringList{term} : term.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            if (!needles.contains(part))
                needles << part;
        }
    }
    qsizetype first = -1;
    for (const QString &needle : std::as_const(needles)) {
        const qsizetype at = plain.indexOf(needle, 0, Qt::CaseInsensitive);
        if (at >= 0 && (first < 0 || at < first))
            first = at;
    }

    // The first match comes a third into the snippet, which starts and ends with whole words where it can
    qsizetype start = 0;
    if (first > length / 3) {
        start = first - length / 3;
        const qsizetype space = plain.indexOf(QLatin1Char(' '), start);
        if (space >= 0 && space < first)
            start = space + 1;
    }
    qsizetype end = std::min<qsizetype>(plain.size(), start + length);
    if (end < plain.size()) {
        const qsizetype space = plain.lastIndexOf(QLatin1Char(' '), end);
        if (space > start + length / 2)
            end = space;
    }
    const QString ellipsis(QChar(0x2026));
    const QString piece = plain.mid(start, end - start);
    const qsizetype offset = start > 0 ? ellipsis.size() : 0;
    snippet.text = (start > 0 ? ellipsis : QString()) + piece + (end < plain.size() ? ellipsis : QString());

    QList<std::pair<int, int>> found;
    for (const QString &needle : std::as_const(needles)) {
        for (qsizetype at = piece.indexOf(needle, 0, Qt::CaseInsensitive); at >= 0;
             at = piece.indexOf(needle, at + needle.size(), Qt::CaseInsensitive))
            found.append({int(at + offset), int(needle.size())});
    }
    std::sort(found.begin(), found.end());
    for (const auto &match : std::as_const(found)) {
        if (!snippet.matches.isEmpty()
            && match.first <= snippet.matches.last().first + snippet.matches.last().second) {
            auto &last = snippet.matches.last();
            last.second = std::max(last.second, match.first + match.second - last.first);
        } else {
            snippet.matches.append(match);
        }
    }
    return snippet;
}

}
}
