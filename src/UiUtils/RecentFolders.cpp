/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "RecentFolders.h"

#include <QAbstractItemModel>
#include <QHash>
#include <QSettings>

#include "Imap/Model/ItemRoles.h"

namespace UiUtils {

namespace {

struct KnownMailbox {
    QString separator;
    bool selectable = false;
    bool childrenKnown = false;
};

struct Snapshot {
    QHash<QString, KnownMailbox> mailboxes;
    bool topLevelKnown = false;
};

void collect(QAbstractItemModel *model, const QModelIndex &parent, Snapshot &snapshot)
{
    using namespace Imap::Mailbox;
    const int rows = model->rowCount(parent);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        const QString name = index.data(RoleMailboxName).toString();
        if (name.isEmpty())
            continue;
        const QVariant fetched = index.data(RoleIsFetched);
        KnownMailbox mailbox;
        mailbox.separator = index.data(RoleMailboxSeparator).toString();
        mailbox.selectable = index.data(RoleMailboxIsSelectable).toBool();
        mailbox.childrenKnown = fetched.isValid() && fetched.toBool();
        snapshot.mailboxes.insert(name, mailbox);
        // Asking an unlisted branch for its children would send a LIST command
        if (mailbox.childrenKnown)
            collect(model, index, snapshot);
    }
}

RecentFolders::State lookup(const Snapshot &snapshot, const QString &mailbox)
{
    using State = RecentFolders::State;
    const auto known = snapshot.mailboxes.constFind(mailbox);
    if (known != snapshot.mailboxes.constEnd())
        return known->selectable ? State::Available : State::Gone;

    // The closest listed folder above it decides whether its absence means anything
    int ancestorLength = -1;
    bool ancestorChildrenKnown = false;
    for (auto it = snapshot.mailboxes.constBegin(); it != snapshot.mailboxes.constEnd(); ++it) {
        if (it->separator.isEmpty() || it.key().size() <= ancestorLength)
            continue;
        if (mailbox.startsWith(it.key() + it->separator)) {
            ancestorLength = it.key().size();
            ancestorChildrenKnown = it->childrenKnown;
        }
    }
    if (ancestorLength >= 0)
        return ancestorChildrenKnown ? State::Gone : State::Unknown;
    return snapshot.topLevelKnown ? State::Gone : State::Unknown;
}

}

RecentFolders::RecentFolders(const QStringList &folders, const int limit)
    : m_limit(qMax(1, limit))
{
    for (const QString &folder : folders) {
        if (m_folders.size() >= m_limit)
            break;
        if (!folder.isEmpty() && !m_folders.contains(folder))
            m_folders << folder;
    }
}

RecentFolders RecentFolders::load(const QSettings &settings, const QString &key, const int limit)
{
    return RecentFolders(settings.value(key).toStringList(), limit);
}

void RecentFolders::save(QSettings &settings, const QString &key) const
{
    if (m_folders.isEmpty())
        settings.remove(key);
    else
        settings.setValue(key, m_folders);
}

void RecentFolders::add(const QString &mailbox)
{
    if (mailbox.isEmpty())
        return;
    m_folders.removeAll(mailbox);
    m_folders.prepend(mailbox);
    while (m_folders.size() > m_limit)
        m_folders.removeLast();
}

bool RecentFolders::prune(const StateLookup &state)
{
    if (!state)
        return false;
    const qsizetype removed = m_folders.removeIf([&state](const QString &folder) {
        return state(folder) == State::Gone;
    });
    return removed > 0;
}

QStringList RecentFolders::offered(const StateLookup &state, const QStringList &excluded) const
{
    QStringList res;
    for (const QString &folder : m_folders) {
        if (excluded.contains(folder) || (state && state(folder) == State::Gone))
            continue;
        res << folder;
    }
    return res;
}

QStringList RecentFolders::folders() const
{
    return m_folders;
}

int RecentFolders::limit() const
{
    return m_limit;
}

RecentFolders::StateLookup RecentFolders::stateFromModel(QAbstractItemModel *mailboxes)
{
    Snapshot snapshot;
    if (mailboxes) {
        collect(mailboxes, QModelIndex(), snapshot);
        // An account always has an INBOX, so an empty top level has simply not been listed yet
        snapshot.topLevelKnown = !snapshot.mailboxes.isEmpty();
    }
    return [snapshot](const QString &mailbox) {
        return lookup(snapshot, mailbox);
    };
}

}
