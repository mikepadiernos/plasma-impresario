/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

#include "UiUtils/RecentFolders.h"

class QSettings;

namespace UiUtils {

/** @short How the message list of a folder is arranged */
struct MessageListView {
    /** @short Column of Imap::Mailbox::MsgListModel, or -1 for the order of the mailbox or of its threads */
    int sortColumn = -1;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;
    bool threading = false;
    /** @short Name of a grouping as in GroupingMsgListModel::groupingName(); empty when the list is not grouped */
    QString groupBy;

    bool operator==(const MessageListView &other) const = default;
};

/** @short Where the message list of a folder was left: its current message and the row at the top of the view */
struct FolderPosition {
    uint uidValidity = 0;
    /** @short UID of the current message, 0 for none */
    uint uid = 0;
    /** @short UID of the message shown at the top of the view, 0 for none */
    uint anchorUid = 0;
    /** @short UIDs of the selected messages besides the current one, at most MaxSelectedUids of them */
    QList<uint> selectedUids;

    static constexpr int MaxSelectedUids = 100;

    bool isValid() const { return uidValidity && (uid || anchorUid); }
    bool operator==(const FolderPosition &other) const = default;
};

/** @short Entries keyed by folder name, most recently used first, keeping at most a fixed number of folders */
template <typename T>
class FolderLru
{
public:
    explicit FolderLru(int limit) : m_limit(qMax(1, limit)) {}

    std::optional<T> value(const QString &folder) const
    {
        for (const auto &entry : m_entries) {
            if (entry.first == folder)
                return entry.second;
        }
        return std::nullopt;
    }

    /** @short Store @arg value as the most recent entry, dropping the least recently used ones beyond the limit */
    void insert(const QString &folder, const T &item)
    {
        if (folder.isEmpty())
            return;
        remove(folder);
        m_entries.prepend({folder, item});
        while (m_entries.size() > m_limit)
            m_entries.removeLast();
    }

    /** @short Append an entry which is older than all present ones, as when loading them in order */
    void appendOlder(const QString &folder, const T &item)
    {
        if (folder.isEmpty() || m_entries.size() >= m_limit || value(folder))
            return;
        m_entries.append({folder, item});
    }

    bool remove(const QString &folder)
    {
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            if (it->first == folder) {
                m_entries.erase(it);
                return true;
            }
        }
        return false;
    }

    /** @short Rename @arg oldName and the folders below it, keeping their age; returns true if anything changed */
    bool rename(const QString &oldName, const QString &newName, const QString &separator)
    {
        if (oldName.isEmpty() || newName.isEmpty() || oldName == newName)
            return false;
        const QString prefix = separator.isEmpty() ? QString() : oldName + separator;
        bool changed = false;
        QList<std::pair<QString, T>> renamed;
        renamed.reserve(m_entries.size());
        for (const auto &entry : std::as_const(m_entries)) {
            QString name = entry.first;
            if (name == oldName) {
                name = newName;
                changed = true;
            } else if (!prefix.isEmpty() && name.startsWith(prefix)) {
                name = newName + separator + name.mid(prefix.size());
                changed = true;
            }
            renamed.append({name, entry.second});
        }
        if (!changed)
            return false;
        // An entry which was already stored under a new name is older than the renamed one, or a stale leftover
        m_entries.clear();
        for (const auto &entry : std::as_const(renamed)) {
            if (!value(entry.first))
                m_entries.append(entry);
        }
        return true;
    }

    /** @short Forget the folders which @arg state reports as gone; returns true if anything was forgotten */
    bool prune(const RecentFolders::StateLookup &state)
    {
        const auto removed = m_entries.removeIf([&state](const std::pair<QString, T> &entry) {
            return state(entry.first) == RecentFolders::State::Gone;
        });
        return removed > 0;
    }

    void clear() { m_entries.clear(); }
    QStringList folders() const
    {
        QStringList names;
        names.reserve(m_entries.size());
        for (const auto &entry : m_entries)
            names << entry.first;
        return names;
    }
    const QList<std::pair<QString, T>> &entries() const { return m_entries; }
    int limit() const { return m_limit; }

private:
    QList<std::pair<QString, T>> m_entries;
    int m_limit;
};

/** @short Arrangement of the message list of one account: per folder, as the account default, and the global default

A folder without its own arrangement uses the account default, and an account without a default uses the global one.
Grouping chosen before folders had their own arrangement is kept as part of the account default.
*/
class ViewStateStore
{
public:
    static constexpr int DefaultLimit = 200;

    /** @short Which level decides the arrangement of a folder */
    enum class Scope {
        Folder,
        Account,
        Global,
    };

    explicit ViewStateStore(int limit = DefaultLimit);

    /** @short Settings keys within the account settings */
    static const QString folderViewsKey;
    static const QString defaultViewKey;
    /** @short Grouping of the account, as stored before folders had their own arrangement */
    static const QString legacyGroupByKey;

    void load(const QSettings &settings);
    void save(QSettings &settings) const;

    void setGlobalDefault(const MessageListView &view);
    MessageListView globalDefault() const;

    std::optional<MessageListView> accountDefault() const;
    /** @short Default of the account, which is the global default with the grouping of the account, if any */
    MessageListView effectiveAccountDefault() const;
    void setAccountDefault(const MessageListView &view);

    std::optional<MessageListView> folderView(const QString &mailbox) const;
    MessageListView effectiveView(const QString &mailbox) const;
    Scope scope(const QString &mailbox) const;

    /** @short Remember @arg view for @arg mailbox as its most recently used arrangement */
    void setFolderView(const QString &mailbox, const MessageListView &view);
    /** @short Let @arg mailbox follow the account default again; returns true if it had its own arrangement */
    bool resetFolder(const QString &mailbox);
    /** @short Make @arg view the account default and let all folders follow it */
    void applyToAllFolders(const MessageListView &view);

    bool renameFolder(const QString &oldName, const QString &newName, const QString &separator);
    bool prune(const RecentFolders::StateLookup &state);

    QStringList folders() const;
    int limit() const;

    static QString toJson(const MessageListView &view);
    static std::optional<MessageListView> fromJson(const QString &json);

private:
    MessageListView m_global;
    std::optional<MessageListView> m_accountDefault;
    QString m_legacyGroupBy;
    FolderLru<MessageListView> m_folders;
};

/** @short Positions of the message lists of the folders of one account, kept for a bounded number of folders */
class FolderPositionStore
{
public:
    static constexpr int DefaultLimit = 200;

    explicit FolderPositionStore(int limit = DefaultLimit);

    std::optional<FolderPosition> position(const QString &mailbox) const;
    /** @short Remember @arg position; an invalid position forgets the folder */
    void setPosition(const QString &mailbox, const FolderPosition &position);
    bool remove(const QString &mailbox);
    bool renameFolder(const QString &oldName, const QString &newName, const QString &separator);
    bool prune(const RecentFolders::StateLookup &state);
    QStringList folders() const;

    QString toJson() const;
    void loadJson(const QString &json);

private:
    FolderLru<FolderPosition> m_folders;
};

}
