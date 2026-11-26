/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "RelatedSearch.h"

#include <algorithm>
#include <QDateTime>
#include <QSet>

#include "Imap/Model/FolderSearch.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/SearchResultsModel.h"
#include "Imap/Model/Utils.h"
#include "Conversations.h"

namespace UiUtils {

namespace {

QByteArray bareId(QByteArray id)
{
    id = id.trimmed();
    if (id.startsWith('<') && id.endsWith('>'))
        id = id.mid(1, id.size() - 2);
    return id;
}

/** The folder of a message of the IMAP model */
QString mailboxOf(const QModelIndex &message)
{
    return message.parent().parent().data(Imap::Mailbox::RoleMailboxName).toString();
}

void newestFirst(QList<QPersistentModelIndex> &messages)
{
    std::stable_sort(messages.begin(), messages.end(), [](const QPersistentModelIndex &a, const QPersistentModelIndex &b) {
        return a.data(Imap::Mailbox::RoleMessageDate).toDateTime() > b.data(Imap::Mailbox::RoleMessageDate).toDateTime();
    });
}

void limit(QList<QPersistentModelIndex> &messages, int maximum, bool &truncated)
{
    if (messages.size() > maximum) {
        messages.resize(maximum);
        truncated = true;
    }
}

}

QStringList relatedSearchConditions(const QModelIndex &message, int maximumReferences)
{
    if (!message.isValid())
        return {};
    const QByteArray own = bareId(message.data(Imap::Mailbox::RoleMessageMessageId).toByteArray());
    QList<QByteArray> ancestors;
    const auto collect = [&](const QList<QByteArray> &ids) {
        for (const QByteArray &raw : ids) {
            const QByteArray id = bareId(raw);
            if (!id.isEmpty() && id != own && !ancestors.contains(id))
                ancestors << id;
        }
    };
    // References go from the first message of the thread to the parent; In-Reply-To names the parent
    collect(message.data(Imap::Mailbox::RoleMessageHeaderReferences).value<QList<QByteArray>>());
    collect(message.data(Imap::Mailbox::RoleMessageInReplyTo).value<QList<QByteArray>>());
    const QByteArray root = ancestors.isEmpty() ? QByteArray() : ancestors.constFirst();
    if (ancestors.size() > maximumReferences)
        ancestors = ancestors.mid(ancestors.size() - std::max(0, maximumReferences));

    QList<QStringList> terms;
    const auto add = [&terms](const QStringList &term) {
        if (!terms.contains(term))
            terms << term;
    };
    for (const QByteArray &id : std::as_const(ancestors))
        add({QStringLiteral("HEADER"), QStringLiteral("Message-ID"), QString::fromUtf8(id)});
    if (!root.isEmpty())
        add({QStringLiteral("HEADER"), QStringLiteral("References"), QString::fromUtf8(root)});
    if (!own.isEmpty()) {
        add({QStringLiteral("HEADER"), QStringLiteral("In-Reply-To"), QString::fromUtf8(own)});
        add({QStringLiteral("HEADER"), QStringLiteral("References"), QString::fromUtf8(own)});
    }
    if (const QString sender = firstAddress(message.data(Imap::Mailbox::RoleMessageFrom)); !sender.isEmpty())
        add({QStringLiteral("FROM"), sender});

    QStringList result;
    // OR takes two operands in prefix notation, so n alternatives need n - 1 of them in front
    for (qsizetype i = 1; i < terms.size(); ++i)
        result << QStringLiteral("OR");
    for (const QStringList &term : std::as_const(terms))
        result << term;
    return result;
}

RelatedMessages classifyRelated(const QModelIndex &current, const QList<QPersistentModelIndex> &found, int maximumPerGroup)
{
    RelatedMessages result;
    if (!current.isValid() || !current.data(Imap::Mailbox::RoleIsFetched).toBool())
        return result;
    const QModelIndex self = Imap::deproxifiedIndex(current);
    QList<QPersistentModelIndex> rows;
    QList<ConversationMessage> messages;
    QSet<QPersistentModelIndex> seen;
    for (const QPersistentModelIndex &index : found) {
        const QModelIndex message = Imap::deproxifiedIndex(index);
        // Asking for envelope data of an unfetched message would download it
        if (!message.isValid() || message == self || !message.data(Imap::Mailbox::RoleIsFetched).toBool())
            continue;
        const QPersistentModelIndex persistent(message);
        if (seen.contains(persistent))
            continue;
        seen.insert(persistent);
        rows << persistent;
        messages << conversationMessage(message);
    }
    messages << conversationMessage(current);
    const QList<qsizetype> groups = groupConversations(messages);
    const qsizetype own = groups.constLast();
    const QString sender = firstAddress(current.data(Imap::Mailbox::RoleMessageFrom));
    for (qsizetype at = 0; at < rows.size(); ++at) {
        if (groups.at(at) == own)
            result.conversation << rows.at(at);
        else if (!sender.isEmpty() && firstAddress(rows.at(at).data(Imap::Mailbox::RoleMessageFrom)) == sender)
            result.sameSender << rows.at(at);
    }
    newestFirst(result.conversation);
    newestFirst(result.sameSender);
    limit(result.conversation, maximumPerGroup, result.truncated);
    limit(result.sameSender, maximumPerGroup, result.truncated);
    return result;
}

RelatedSearch::RelatedSearch(QObject *parent)
    : QObject(parent)
{
}

RelatedSearch::~RelatedSearch()
{
    stop(false);
}

void RelatedSearch::setTimeouts(int listingMs, int searchMs, int loadingMs)
{
    m_listingMs = listingMs;
    m_searchMs = searchMs;
    m_loadingMs = loadingMs;
}

bool RelatedSearch::start(const QModelIndex &message, Scope scope, const QStringList &folders)
{
    stop(true);
    m_problem.clear();
    const QModelIndex own = Imap::deproxifiedIndex(message);
    m_message = own;
    auto *model = qobject_cast<Imap::Mailbox::Model *>(const_cast<QAbstractItemModel *>(own.model()));
    if (!model || !own.data(Imap::Mailbox::RoleIsFetched).toBool()) {
        m_problem = tr("The message is not loaded yet");
        emit changed();
        return false;
    }
    const QStringList conditions = relatedSearchConditions(own);
    if (conditions.isEmpty()) {
        m_problem = tr("The message has no Message-ID, references or sender to search for");
        emit changed();
        return false;
    }
    m_mailbox = mailboxOf(own);
    QStringList mailboxes;
    switch (scope) {
    case Scope::MessageFolder:
        mailboxes << m_mailbox;
        break;
    case Scope::AllFolders:
        break;
    case Scope::ChosenFolders:
        mailboxes = folders;
        mailboxes.removeAll(QString());
        mailboxes.removeDuplicates();
        if (mailboxes.isEmpty()) {
            m_problem = tr("No folders are chosen for the search");
            emit changed();
            return false;
        }
        break;
    }
    m_search = new Imap::Mailbox::FolderSearch(model, this);
    m_search->setLimits(folderLimit(), resultLimit());
    m_search->setTimeouts(m_listingMs, m_searchMs, m_loadingMs);
    connect(m_search, &Imap::Mailbox::FolderSearch::progressChanged, this, &RelatedSearch::changed);
    connect(m_search, &Imap::Mailbox::FolderSearch::finished, this, &RelatedSearch::searchFinished);
    m_search->start(mailboxes, scope == Scope::AllFolders, conditions);
    emit changed();
    return true;
}

void RelatedSearch::cancel()
{
    stop(true);
    emit changed();
}

void RelatedSearch::stop(bool notify)
{
    if (!m_search)
        return;
    const bool running = m_search->isRunning();
    QPointer<Imap::Mailbox::Model> model = m_search->model();
    m_search->cancel();
    m_search->deleteLater();
    m_search.clear();
    if (notify && running && model)
        emit selectionChanged(model);
}

void RelatedSearch::searchFinished()
{
    if (m_search && m_search->model())
        emit selectionChanged(m_search->model());
    emit changed();
    emit finished();
}

bool RelatedSearch::isRunning() const
{
    return m_search && m_search->isRunning();
}

QModelIndex RelatedSearch::message() const
{
    return m_message;
}

QString RelatedSearch::problem() const
{
    return m_problem;
}

RelatedMessages RelatedSearch::results(int maximumPerGroup) const
{
    if (!m_search || !m_message.isValid())
        return {};
    QList<QPersistentModelIndex> found;
    const QModelIndexList messages = m_search->results()->messages();
    for (const QModelIndex &message : messages)
        found << QPersistentModelIndex(message);
    return classifyRelated(m_message, found, maximumPerGroup);
}

QStringList RelatedSearch::failures() const
{
    QStringList out;
    if (!m_search)
        return out;
    const auto failures = m_search->failures();
    for (const auto &failure : failures)
        out << (failure.mailbox.isEmpty() ? failure.reason : tr("%1: %2").arg(failure.mailbox, failure.reason));
    return out;
}

int RelatedSearch::folderCount() const
{
    return m_search ? m_search->folderCount() : 0;
}

int RelatedSearch::searchedCount() const
{
    return m_search ? m_search->searchedCount() : 0;
}

bool RelatedSearch::isTruncated() const
{
    return m_search && m_search->isTruncated();
}

}
