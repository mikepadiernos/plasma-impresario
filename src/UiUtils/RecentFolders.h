/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QStringList>
#include <functional>

class QAbstractItemModel;
class QSettings;

namespace UiUtils {

/** @short Folders of one account which messages were recently moved or copied to, most recent first */
class RecentFolders
{
public:
    /** @short What is known about a remembered folder without asking the server */
    enum class State {
        /** Listed and able to hold messages */
        Available,
        /** Listed but unable to hold messages, or missing from a folder list which is complete */
        Gone,
        /** Not listed yet, for example because the folder above it has not been expanded */
        Unknown,
    };
    using StateLookup = std::function<State(const QString &mailbox)>;

    static constexpr int DefaultLimit = 5;

    /** @short Start from @arg folders, dropping empty names, repeated names and anything beyond @arg limit */
    explicit RecentFolders(const QStringList &folders = QStringList(), int limit = DefaultLimit);

    static RecentFolders load(const QSettings &settings, const QString &key, int limit = DefaultLimit);
    /** @short Store the list under @arg key, removing the key when the list is empty */
    void save(QSettings &settings, const QString &key) const;

    /** @short Put @arg mailbox first, dropping an older entry of the same name and anything beyond the limit */
    void add(const QString &mailbox);

    /** @short Forget the folders which @arg state reports as gone; returns true if anything was forgotten */
    bool prune(const StateLookup &state);

    /** @short Folders worth offering: all except the gone ones and those in @arg excluded */
    QStringList offered(const StateLookup &state, const QStringList &excluded = QStringList()) const;

    QStringList folders() const;
    int limit() const;

    /** @short Look up folder states in a mailbox tree model which uses the Imap::Mailbox roles

    The model is walked once, when this function is called. Only branches whose children are already known
    are visited, so the walk never makes an IMAP model contact the server. A folder is gone only when it is
    listed as not selectable, or when the folder above it (or the top level) has been listed completely
    without it.
    */
    static StateLookup stateFromModel(QAbstractItemModel *mailboxes);

private:
    QStringList m_folders;
    int m_limit;
};

}
