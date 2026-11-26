/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <functional>
#include <KSharedConfig>

#include "EmbeddedPanel.h"
#include "UiUtils/RelatedSearch.h"

class QActionGroup;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

namespace Gui {

/** @short Embedded panel with messages related to the one shown in the reader

Groups messages from the current message list that belong to the same conversation
and other messages from the same sender. Only messages which the list has already
loaded are considered, so the panel never makes the IMAP model download envelopes.

Searching the server is an option of the panel, off by default. It looks for the
messages of the thread and of the sender in the message's folder, in every folder of
its account or in chosen folders, and adds them to the local ones with their folder.
*/
class RelatedMessagesPanel : public MessageContextPanel
{
    Q_OBJECT
public:
    explicit RelatedMessagesPanel(QWidget *parent = nullptr);

    static Descriptor panelDescriptor();
    /** At most this many messages in each group */
    static constexpr int maximumPerGroup() { return 25; }

    /** Search the rows of a flat message list such as Imap::Mailbox::MsgListModel */
    void setMessageList(QAbstractItemModel *messages);
    /** Read and keep the server search options in the Appearance group of @arg config */
    void setConfig(KSharedConfigPtr config);
    /** Tell the account id of a message, which picks its chosen folders */
    void setAccountResolver(const std::function<QString(const QModelIndex &)> &resolver);
    /** Tell why the server cannot be searched for a message now, or an empty string when it can */
    void setServerSearchGuard(const std::function<QString(const QModelIndex &)> &guard);
    /** Wait this long after the message changed before searching the server (default: 500 ms) */
    void setServerSearchDelay(int ms);

    bool hasContent() const override;
    QWidget *focusTarget() const override;

    QList<QPersistentModelIndex> conversation() const;
    QList<QPersistentModelIndex> sameSender() const;

    bool searchesServer() const;
    UiUtils::RelatedSearch::Scope serverScope() const;
    /** Folders for UiUtils::RelatedSearch::Scope::ChosenFolders, as UiUtils::SearchScope::folderKey() */
    QStringList chosenFolders() const;
    UiUtils::RelatedSearch *serverSearch() const;
    /** What the server search is doing or why it did not find everything, or empty */
    QString serverStatus() const;
    /** Whether the guard kept the server from being searched for the current message */
    bool isServerSearchBlocked() const;

public slots:
    void activateCurrent();
    void setSearchesServer(bool searches);
    void setServerScope(UiUtils::RelatedSearch::Scope scope);
    void setChosenFolders(const QStringList &keys);
    /** Search the server again for the current message, for example after the guard stopped allowing it */
    void restartServerSearch();

signals:
    /** The user picked a message; @arg message is a row of the message list or a message of the IMAP model */
    void messageActivated(const QModelIndex &message);
    /** The user wants to choose the folders to search for the messages of @arg accountId */
    void chooseFoldersRequested(const QString &accountId);

protected:
    void refresh() override;
    void panelEnabledChanged() override;

private:
    void scheduleListRefresh();
    void fillGroup(QTreeWidgetItem *group, const QList<QPersistentModelIndex> &messages, const QString &shownMailbox);
    void followMessage(const QModelIndex &current);
    void startServerSearch();
    void updateServerStatus();
    void syncMenu();
    void saveConfig();
    bool serverSearchWanted() const;

    QPointer<QAbstractItemModel> m_messages;
    QList<QMetaObject::Connection> m_listConnections;
    QList<QPersistentModelIndex> m_conversation;
    QList<QPersistentModelIndex> m_sameSender;
    QWidget *m_body;
    QTreeWidget *m_tree;
    QLabel *m_serverStatusLabel;
    QTimer *m_listTimer;
    QTimer *m_serverTimer;
    QAction *m_searchServerAction;
    QAction *m_chooseFoldersAction;
    QActionGroup *m_scopeGroup;
    UiUtils::RelatedSearch *m_serverSearch;
    KSharedConfigPtr m_config;
    std::function<QString(const QModelIndex &)> m_accountResolver;
    std::function<QString(const QModelIndex &)> m_guard;
    bool m_searchesServer = false;
    UiUtils::RelatedSearch::Scope m_scope = UiUtils::RelatedSearch::Scope::MessageFolder;
    QStringList m_chosenFolders;
    /** The message which the server results are for, of the IMAP model */
    QPersistentModelIndex m_serverMessage;
    bool m_serverStarted = false;
    QString m_serverBlocked;
    QString m_serverStatus;
};

}
