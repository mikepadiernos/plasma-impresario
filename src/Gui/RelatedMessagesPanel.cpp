/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "RelatedMessagesPanel.h"

#include <algorithm>
#include <KConfigGroup>
#include <KLocalizedString>
#include <QAbstractItemModel>
#include <QActionGroup>
#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QSet>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Utils.h"
#include "Imap/Parser/MailAddress.h"
#include "UiUtils/RelatedMessages.h"
#include "UiUtils/SearchScope.h"

namespace Gui {

namespace {

constexpr int MessageRole = Qt::UserRole;
/** The message list changes often while a mailbox syncs, so coalesce its signals */
constexpr int ListRefreshDelay = 250;
/** Stepping through messages should not start a search on the server for each of them */
constexpr int ServerSearchDelay = 500;

using Scope = UiUtils::RelatedSearch::Scope;

QString scopeName(Scope scope)
{
    switch (scope) {
    case Scope::MessageFolder:
        return QStringLiteral("folder");
    case Scope::AllFolders:
        return QStringLiteral("all");
    case Scope::ChosenFolders:
        return QStringLiteral("chosen");
    }
    return QString();
}

Scope scopeFromName(const QString &name)
{
    if (name == QLatin1String("all"))
        return Scope::AllFolders;
    if (name == QLatin1String("chosen"))
        return Scope::ChosenFolders;
    return Scope::MessageFolder;
}

/** The folder of a message of the IMAP model or of a list above it */
QString mailboxOf(const QModelIndex &message)
{
    return Imap::deproxifiedIndex(message).parent().parent().data(Imap::Mailbox::RoleMailboxName).toString();
}

/** Add the messages of @arg groups which @arg seen does not hold yet, newest first and at most @arg maximum */
QList<QPersistentModelIndex> merged(const QList<QList<QPersistentModelIndex>> &groups, QSet<QPersistentModelIndex> &seen, int maximum,
                                    bool &truncated)
{
    QList<QPersistentModelIndex> out;
    for (const auto &group : groups) {
        for (const QPersistentModelIndex &message : group) {
            const QPersistentModelIndex own(Imap::deproxifiedIndex(message));
            if (!own.isValid() || seen.contains(own))
                continue;
            seen.insert(own);
            out << message;
        }
    }
    std::stable_sort(out.begin(), out.end(), [](const QPersistentModelIndex &a, const QPersistentModelIndex &b) {
        return a.data(Imap::Mailbox::RoleMessageDate).toDateTime() > b.data(Imap::Mailbox::RoleMessageDate).toDateTime();
    });
    if (out.size() > maximum) {
        out.resize(maximum);
        truncated = true;
    }
    return out;
}

}

EmbeddedPanel::Descriptor RelatedMessagesPanel::panelDescriptor()
{
    Descriptor descriptor;
    descriptor.id = QStringLiteral("RelatedMessages");
    descriptor.configKey = QStringLiteral("RelatedMessagesPanel");
    descriptor.title = i18nc("@title embedded panel", "Related Messages");
    descriptor.autoHideToolTip = i18nc("@info:tooltip", "Show the related messages panel only while it has messages to list");
    descriptor.defaultEnabled = false;
    descriptor.defaultPlacement = Placement::RightColumn;
    return descriptor;
}

RelatedMessagesPanel::RelatedMessagesPanel(QWidget *parent)
    : MessageContextPanel(panelDescriptor(), QStringLiteral("relatedMessagesPanel"), parent)
    , m_body(new QWidget(this))
    , m_tree(new QTreeWidget(m_body))
    , m_serverStatusLabel(new QLabel(m_body))
    , m_listTimer(new QTimer(this))
    , m_serverTimer(new QTimer(this))
    , m_serverSearch(new UiUtils::RelatedSearch(this))
{
    m_tree->setObjectName(QStringLiteral("relatedMessagesPanelList"));
    m_tree->setAccessibleName(i18nc("@label", "Messages related to the current message"));
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setTextElideMode(Qt::ElideRight);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_tree, &QTreeWidget::itemActivated, this, &RelatedMessagesPanel::activateCurrent);

    m_serverStatusLabel->setObjectName(QStringLiteral("relatedMessagesPanelServerStatus"));
    m_serverStatusLabel->setWordWrap(true);
    m_serverStatusLabel->setTextFormat(Qt::PlainText);
    m_serverStatusLabel->setForegroundRole(QPalette::PlaceholderText);
    m_serverStatusLabel->hide();
    auto *layout = new QVBoxLayout(m_body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(m_tree, 1);
    layout->addWidget(m_serverStatusLabel);
    setContentWidget(m_body);
    setPreferredRows(5);

    QToolButton *serverButton = addHeaderButton(QStringLiteral("ServerSearch"), QStringLiteral("edit-find"),
                                                i18nc("@action:button", "Search on the Server"));
    auto *menu = new QMenu(serverButton);
    m_searchServerAction = menu->addAction(i18nc("@option:check", "Search on the Server"));
    m_searchServerAction->setObjectName(QStringLiteral("relatedMessagesPanelSearchServer"));
    m_searchServerAction->setCheckable(true);
    m_searchServerAction->setToolTip(i18nc("@info:tooltip", "Also ask the server for the messages of the conversation and of the sender"));
    connect(m_searchServerAction, &QAction::toggled, this, &RelatedMessagesPanel::setSearchesServer);
    menu->addSection(i18nc("@title:menu related messages on the server", "Search In"));
    m_scopeGroup = new QActionGroup(menu);
    m_scopeGroup->setExclusive(true);
    const auto addScope = [&](Scope scope, const QString &name, const QString &text) {
        QAction *action = menu->addAction(text);
        action->setObjectName(name);
        action->setCheckable(true);
        action->setData(int(scope));
        m_scopeGroup->addAction(action);
    };
    addScope(Scope::MessageFolder, QStringLiteral("relatedMessagesPanelScopeFolder"), i18nc("@option:radio", "This Message's Folder"));
    addScope(Scope::AllFolders, QStringLiteral("relatedMessagesPanelScopeAll"),
             i18nc("@option:radio", "All Folders (up to %1)", UiUtils::RelatedSearch::folderLimit()));
    addScope(Scope::ChosenFolders, QStringLiteral("relatedMessagesPanelScopeChosen"), i18nc("@option:radio", "Chosen Folders"));
    connect(m_scopeGroup, &QActionGroup::triggered, this, [this](QAction *action) {
        const auto scope = Scope(action->data().toInt());
        setServerScope(scope);
        const QString accountId = m_accountResolver ? m_accountResolver(message()) : QString();
        const bool chosen = std::any_of(m_chosenFolders.cbegin(), m_chosenFolders.cend(), [&](const QString &key) {
            return accountId.isEmpty() || UiUtils::SearchScope::folderFromKey(key).accountId == accountId;
        });
        if (scope == Scope::ChosenFolders && !chosen)
            emit chooseFoldersRequested(accountId);
    });
    menu->addSeparator();
    m_chooseFoldersAction = menu->addAction(QIcon::fromTheme(QStringLiteral("folder-open")), i18nc("@action:inmenu", "Choose Folders…"));
    m_chooseFoldersAction->setObjectName(QStringLiteral("relatedMessagesPanelChooseFolders"));
    connect(m_chooseFoldersAction, &QAction::triggered, this, [this]() {
        emit chooseFoldersRequested(m_accountResolver ? m_accountResolver(message()) : QString());
    });
    serverButton->setMenu(menu);
    serverButton->setPopupMode(QToolButton::InstantPopup);

    m_listTimer->setSingleShot(true);
    m_listTimer->setInterval(ListRefreshDelay);
    connect(m_listTimer, &QTimer::timeout, this, &RelatedMessagesPanel::refresh);
    m_serverTimer->setSingleShot(true);
    m_serverTimer->setInterval(ServerSearchDelay);
    connect(m_serverTimer, &QTimer::timeout, this, &RelatedMessagesPanel::startServerSearch);
    connect(m_serverSearch, &UiUtils::RelatedSearch::changed, this, [this]() {
        if (m_serverStarted)
            scheduleListRefresh();
    });

    syncMenu();
    refresh();
}

void RelatedMessagesPanel::setMessageList(QAbstractItemModel *messages)
{
    for (const auto &connection : std::as_const(m_listConnections))
        disconnect(connection);
    m_listConnections.clear();
    m_messages = messages;
    if (messages) {
        m_listConnections << connect(messages, &QAbstractItemModel::rowsInserted, this, &RelatedMessagesPanel::scheduleListRefresh)
                          << connect(messages, &QAbstractItemModel::rowsRemoved, this, &RelatedMessagesPanel::scheduleListRefresh)
                          << connect(messages, &QAbstractItemModel::modelReset, this, &RelatedMessagesPanel::scheduleListRefresh)
                          << connect(messages, &QAbstractItemModel::layoutChanged, this, &RelatedMessagesPanel::scheduleListRefresh)
                          << connect(messages, &QAbstractItemModel::dataChanged, this, &RelatedMessagesPanel::scheduleListRefresh);
    }
    refresh();
}

void RelatedMessagesPanel::setConfig(KSharedConfigPtr config)
{
    m_config = config;
    if (m_config) {
        const KConfigGroup appearance(m_config, QStringLiteral("Appearance"));
        m_searchesServer = appearance.readEntry("RelatedMessagesPanelServerSearch", false);
        m_scope = scopeFromName(appearance.readEntry("RelatedMessagesPanelServerScope", scopeName(Scope::MessageFolder)));
        m_chosenFolders = appearance.readEntry("RelatedMessagesPanelFolders", QStringList());
    }
    syncMenu();
    restartServerSearch();
}

void RelatedMessagesPanel::saveConfig()
{
    if (!m_config)
        return;
    KConfigGroup appearance(m_config, QStringLiteral("Appearance"));
    appearance.writeEntry("RelatedMessagesPanelServerSearch", m_searchesServer);
    appearance.writeEntry("RelatedMessagesPanelServerScope", scopeName(m_scope));
    appearance.writeEntry("RelatedMessagesPanelFolders", m_chosenFolders);
    appearance.sync();
}

void RelatedMessagesPanel::syncMenu()
{
    const QSignalBlocker blocker(m_searchServerAction);
    m_searchServerAction->setChecked(m_searchesServer);
    const QList<QAction *> scopes = m_scopeGroup->actions();
    for (QAction *action : scopes) {
        action->setChecked(Scope(action->data().toInt()) == m_scope);
        action->setEnabled(m_searchesServer);
    }
    m_chooseFoldersAction->setEnabled(m_searchesServer);
}

void RelatedMessagesPanel::setAccountResolver(const std::function<QString(const QModelIndex &)> &resolver)
{
    m_accountResolver = resolver;
}

void RelatedMessagesPanel::setServerSearchGuard(const std::function<QString(const QModelIndex &)> &guard)
{
    m_guard = guard;
}

void RelatedMessagesPanel::setServerSearchDelay(int ms)
{
    m_serverTimer->setInterval(ms);
}

bool RelatedMessagesPanel::searchesServer() const
{
    return m_searchesServer;
}

UiUtils::RelatedSearch::Scope RelatedMessagesPanel::serverScope() const
{
    return m_scope;
}

QStringList RelatedMessagesPanel::chosenFolders() const
{
    return m_chosenFolders;
}

UiUtils::RelatedSearch *RelatedMessagesPanel::serverSearch() const
{
    return m_serverSearch;
}

QString RelatedMessagesPanel::serverStatus() const
{
    return m_serverStatus;
}

bool RelatedMessagesPanel::isServerSearchBlocked() const
{
    return m_serverStarted && !m_serverBlocked.isEmpty();
}

void RelatedMessagesPanel::setSearchesServer(bool searches)
{
    if (searches == m_searchesServer)
        return;
    m_searchesServer = searches;
    syncMenu();
    saveConfig();
    restartServerSearch();
}

void RelatedMessagesPanel::setServerScope(UiUtils::RelatedSearch::Scope scope)
{
    if (scope == m_scope)
        return;
    m_scope = scope;
    syncMenu();
    saveConfig();
    restartServerSearch();
}

void RelatedMessagesPanel::setChosenFolders(const QStringList &keys)
{
    m_chosenFolders = keys;
    saveConfig();
    if (m_scope == Scope::ChosenFolders)
        restartServerSearch();
}

void RelatedMessagesPanel::restartServerSearch()
{
    m_serverTimer->stop();
    const bool started = std::exchange(m_serverStarted, false);
    m_serverBlocked.clear();
    if (started || m_serverSearch->isRunning())
        m_serverSearch->cancel();
    refresh();
}

void RelatedMessagesPanel::panelEnabledChanged()
{
    refresh();
}

void RelatedMessagesPanel::scheduleListRefresh()
{
    if (isPanelEnabled() && !m_listTimer->isActive())
        m_listTimer->start();
}

bool RelatedMessagesPanel::hasContent() const
{
    return !m_conversation.isEmpty() || !m_sameSender.isEmpty();
}

QWidget *RelatedMessagesPanel::focusTarget() const
{
    return m_tree;
}

QList<QPersistentModelIndex> RelatedMessagesPanel::conversation() const
{
    return m_conversation;
}

QList<QPersistentModelIndex> RelatedMessagesPanel::sameSender() const
{
    return m_sameSender;
}

bool RelatedMessagesPanel::serverSearchWanted() const
{
    return m_searchesServer && isPanelEnabled();
}

void RelatedMessagesPanel::followMessage(const QModelIndex &current)
{
    const QModelIndex own = Imap::deproxifiedIndex(current);
    const bool changed = QModelIndex(m_serverMessage) != own;
    if (changed || !serverSearchWanted()) {
        // Results of the previous message must never show up for this one
        m_serverMessage = own;
        m_serverTimer->stop();
        m_serverBlocked.clear();
        if (std::exchange(m_serverStarted, false) || m_serverSearch->isRunning())
            m_serverSearch->cancel();
    }
    if (serverSearchWanted() && own.isValid() && messageIsFetched() && !m_serverStarted && !m_serverTimer->isActive())
        m_serverTimer->start();
}

void RelatedMessagesPanel::startServerSearch()
{
    m_serverTimer->stop();
    const QModelIndex own = Imap::deproxifiedIndex(message());
    if (!serverSearchWanted() || !own.isValid() || own != QModelIndex(m_serverMessage) || m_serverStarted)
        return;
    m_serverStarted = true;
    m_serverBlocked = m_guard ? m_guard(own) : QString();
    if (m_serverBlocked.isEmpty()) {
        QStringList folders;
        if (m_scope == Scope::ChosenFolders) {
            const QString accountId = m_accountResolver ? m_accountResolver(own) : QString();
            for (const QString &key : std::as_const(m_chosenFolders)) {
                const UiUtils::SearchScope::Folder folder = UiUtils::SearchScope::folderFromKey(key);
                if (accountId.isEmpty() || folder.accountId == accountId)
                    folders << folder.mailbox;
            }
        }
        m_serverSearch->start(own, m_scope, folders);
    }
    refresh();
}

void RelatedMessagesPanel::updateServerStatus()
{
    QStringList lines;
    QString toolTip;
    if (serverSearchWanted() && m_serverStarted) {
        if (!m_serverBlocked.isEmpty()) {
            lines << m_serverBlocked;
        } else if (const QString problem = m_serverSearch->problem(); !problem.isEmpty()) {
            lines << i18nc("@info reason", "Could not search the server: %1", problem);
        } else {
            const int folders = m_serverSearch->folderCount();
            if (m_serverSearch->isRunning()) {
                lines << (folders > 0 ? i18nc("@info folders done, all folders", "Searching the server: %1 of %2 folders…",
                                              std::min(m_serverSearch->searchedCount() + 1, folders), folders)
                                      : i18nc("@info", "Searching the server…"));
            }
            const QStringList failures = m_serverSearch->failures();
            if (failures.size() == 1)
                lines << i18nc("@info place and reason", "Not searched: %1", failures.constFirst());
            else if (!failures.isEmpty())
                lines << i18ncp("@info", "%1 place could not be searched", "%1 places could not be searched", int(failures.size()));
            if (!failures.isEmpty())
                toolTip = failures.join(QLatin1Char('\n'));
            if (m_serverSearch->isTruncated() && !m_serverSearch->isRunning()) {
                lines << i18nc("@info", "Only the first %1 folders and the newest %2 matches of each folder were searched",
                               UiUtils::RelatedSearch::folderLimit(), UiUtils::RelatedSearch::resultLimit());
            }
        }
    }
    m_serverStatus = lines.join(QLatin1Char('\n'));
    m_serverStatusLabel->setText(m_serverStatus);
    m_serverStatusLabel->setToolTip(toolTip);
    m_serverStatusLabel->setVisible(!m_serverStatus.isEmpty());
}

void RelatedMessagesPanel::refresh()
{
    m_listTimer->stop();
    const QModelIndex current = message();
    followMessage(current);
    // Scanning the list is the expensive part, so a turned-off panel does not do it.
    UiUtils::RelatedMessages related;
    if (isPanelEnabled()) {
        related = UiUtils::collectRelatedMessages(m_messages.data(), current, maximumPerGroup());
        if (m_serverStarted && m_serverMessage.isValid() && m_serverSearch->message() == QModelIndex(m_serverMessage)) {
            const UiUtils::RelatedMessages server = m_serverSearch->results(maximumPerGroup());
            QSet<QPersistentModelIndex> seen;
            bool truncated = related.truncated || server.truncated;
            related.conversation = merged({related.conversation, server.conversation}, seen, maximumPerGroup(), truncated);
            related.sameSender = merged({related.sameSender, server.sameSender}, seen, maximumPerGroup(), truncated);
            related.truncated = truncated;
        }
    }
    m_conversation = related.conversation;
    m_sameSender = related.sameSender;
    updateServerStatus();

    const QPersistentModelIndex selected =
        m_tree->currentItem() ? m_tree->currentItem()->data(0, MessageRole).toPersistentModelIndex() : QPersistentModelIndex();
    const QString shownMailbox = mailboxOf(current);
    m_tree->clear();
    if (!m_conversation.isEmpty()) {
        auto *group = new QTreeWidgetItem(m_tree, {i18ncp("@title:group related messages", "Same Conversation (%1)",
                                                          "Same Conversation (%1)", m_conversation.size())});
        fillGroup(group, m_conversation, shownMailbox);
    }
    if (!m_sameSender.isEmpty()) {
        auto *group = new QTreeWidgetItem(m_tree, {i18ncp("@title:group related messages", "From the Same Sender (%1)",
                                                          "From the Same Sender (%1)", m_sameSender.size())});
        fillGroup(group, m_sameSender, shownMailbox);
    }
    m_tree->expandAll();
    if (selected.isValid()) {
        for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
            if ((*it)->data(0, MessageRole).toPersistentModelIndex() == selected) {
                m_tree->setCurrentItem(*it);
                break;
            }
        }
    }

    const bool searchedServer = serverSearchWanted() && m_serverStarted;
    if (hasContent())
        showContent();
    else if (!current.isValid())
        showPlaceholder(i18nc("@info", "No message selected"));
    else if (!messageIsFetched())
        showPlaceholder(i18nc("@info", "Loading message…"));
    else if (searchedServer && m_serverSearch->isRunning())
        showPlaceholder(m_serverStatus);
    else if (searchedServer && !m_serverStatus.isEmpty())
        showPlaceholder(i18nc("@info followed by what went wrong", "No related messages found\n%1", m_serverStatus));
    else if (searchedServer)
        showPlaceholder(i18nc("@info", "No related messages found"));
    else if (serverSearchWanted())
        showPlaceholder(i18nc("@info", "No related messages in this folder; searching the server soon…"));
    else
        showPlaceholder(i18nc("@info", "No related messages in this folder"));
    updateVisibility();
}

void RelatedMessagesPanel::fillGroup(QTreeWidgetItem *group, const QList<QPersistentModelIndex> &messages, const QString &shownMailbox)
{
    QFont font = group->font(0);
    font.setBold(true);
    group->setFont(0, font);
    group->setFlags(Qt::ItemIsEnabled);
    for (const auto &message : messages) {
        QString subject = message.data(Imap::Mailbox::RoleMessageSubject).toString();
        if (subject.isEmpty())
            subject = i18nc("@info message without a subject", "(no subject)");
        const QString from = Imap::Message::MailAddress::prettyList(message.data(Imap::Mailbox::RoleMessageFrom).toList(),
                                                                    Imap::Message::MailAddress::FORMAT_JUST_NAME);
        const QDateTime date = message.data(Imap::Mailbox::RoleMessageDate).toDateTime();
        const QString when = date.isValid() ? QLocale().toString(date.toLocalTime().date(), QLocale::ShortFormat) : QString();
        QString text = when.isEmpty() ? subject : i18nc("@item subject and date", "%1 — %2", subject, when);
        QString toolTip = from.isEmpty() ? subject : i18nc("@info:tooltip subject and sender", "%1\nFrom: %2", subject, from);
        const QString mailbox = mailboxOf(message);
        if (!mailbox.isEmpty() && mailbox != shownMailbox) {
            // First, so that eliding a long subject keeps it
            text = i18nc("@item the folder which holds the message, then the subject and date", "%1 › %2", mailbox, text);
            toolTip = i18nc("@info:tooltip message details and the folder which holds it", "%1\nFolder: %2", toolTip, mailbox);
        }
        auto *item = new QTreeWidgetItem(group, {text});
        item->setData(0, MessageRole, QVariant::fromValue(message));
        item->setToolTip(0, toolTip);
        if (!message.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool()) {
            QFont unread = item->font(0);
            unread.setBold(true);
            item->setFont(0, unread);
        }
    }
}

void RelatedMessagesPanel::activateCurrent()
{
    const auto *item = m_tree->currentItem();
    if (!item)
        return;
    const QPersistentModelIndex message = item->data(0, MessageRole).toPersistentModelIndex();
    if (message.isValid())
        emit messageActivated(message);
}

}
