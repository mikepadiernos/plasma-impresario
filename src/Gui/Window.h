/* Copyright (C) 2006 - 2015 Jan Kundrát <jkt@kde.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TROJITA_WINDOW_H
#define TROJITA_WINDOW_H

#include <QHash>
#include <QIcon>
#include <QMap>
#include <QMainWindow>
#include <QModelIndex>
#include <QPointer>
#include <QSet>
#include <QSystemTrayIcon>
#include <functional>

#include "configure.cmake.h"
#include "Composer/Recipients.h"
#include "Imap/ConnectionState.h"
#include "Imap/Model/Cache.h"
#include "Imap/Model/CopyMoveOperation.h"
#include "Imap/Model/GroupingMsgListModel.h"
#include "Imap/Model/SpecialFolder.h"
#include "Imap/Parser/Uids.h"
#include "UiUtils/DestructiveActions.h"
#include "UiUtils/QuickFilters.h"
#include "UiUtils/RecentFolders.h"
#include "UiUtils/SearchScope.h"
#include "UiUtils/ShortcutOverview.h"
#include "UiUtils/ViewStateStore.h"
#include "MessageActionHandler.h"

class QActionGroup;
class QAuthenticator;
class QComboBox;
class QCloseEvent;
class QFile;
class QItemSelection;
class QLabel;
class QMessageBox;
class QModelIndex;
class QScrollArea;
class QSettings;
class QSplitter;
class QSslCertificate;
class QSslError;
class QStackedWidget;
class QToolButton;
class QTreeView;

namespace Common
{
class AccountRegistry;
class AccountStatusTracker;
}

namespace Composer
{
class SenderIdentitiesModel;
}

namespace Imap
{

class ImapAccess;

namespace Mailbox
{

class FolderSearch;
class ImapTask;
class Model;
class PrettyMailboxModel;
class ThreadingMsgListModel;
class PrettyMsgListModel;
class PrettyUnifiedMsgListModel;
class UnifiedMsgListModel;
class FavoriteTagsModel;

}
}

namespace Plugins
{
class PluginManager;
}

namespace MSA
{
class MSAFactory;
}
namespace Gui
{

class AccountSidebar;
class CapabilityActionGate;
class ShellHeader;
class AttachmentPanel;
class ReaderActionBar;
class ContactPanel;
class EmbeddedPanel;
class MessageDetailsPanel;
class RelatedMessagesPanel;
class CompleteMessageWidget;
class ComposeWidget;
class DensityMenu;
class MessageRowsMenu;
class EmbeddedPanelsMenu;
class InlineNotificationBar;
class MailBoxTreeView;
class MessageListWidget;
class MessageSelectionRestorer;
class ProtocolLoggerWidget;
class ShortcutOverviewDialog;
class TaskProgressIndicator;

class MainWindow: public QMainWindow
{
    Q_OBJECT
    typedef QList<QPair<Composer::RecipientKind,QString> > RecipientsType;

    typedef enum { LAYOUT_COMPACT, LAYOUT_WIDE, LAYOUT_ONE_AT_TIME } LayoutMode;
public:
    MainWindow(QSettings *settings);
    ~MainWindow() override;
    void showMainWindow();
    QSize sizeHint() const override;

    Imap::Mailbox::Model *imapModel() const;

    /** Return the sender identities the active account sends from: its own, the shared ones, or none. */
    Composer::SenderIdentitiesModel *senderIdentitiesModel();
    /** Return the identities shared by accounts that do not use their own. */
    Composer::SenderIdentitiesModel *sharedIdentitiesModel() { return m_senderIdentities; }
    /** Return the active account's own identities, whether or not it currently uses them. */
    Composer::SenderIdentitiesModel *accountIdentitiesModel();
    Imap::Mailbox::FavoriteTagsModel *favoriteTagsModel() { return m_favoriteTags; }
    Plugins::PluginManager *pluginManager() { return m_pluginManager; }
    QSettings *settings() const { return m_settings; }

    /** Return the configuration of the active account. */
    QSettings *accountSettings() const;
    MSA::MSAFactory *msaFactory();

    // FIXME: this should be changed to some wrapper when support for multiple accounts is available
    Imap::ImapAccess *imapAccess() const;

    /** @short The configured accounts in their order */
    QStringList accountIds() const;
    /** @short The name which the user gave @p accountId */
    QString accountName(const QString &accountId) const;
    /** @short The account which a composer opened now belongs to: the one of the message handled now, or the shown one */
    QString composerAccountId() const;
    /** @short The name of the account which @p message belongs to while several accounts are set up, otherwise empty */
    QString messageAccountName(const QModelIndex &message) const;
    /** @short Run @p fn while accountSettings(), imapAccess(), imapModel(), senderIdentitiesModel() and msaFactory()
    refer to @p accountId

    Returns false without running @p fn when the account is gone or has no connection set up.
    */
    bool withAccount(const QString &accountId, const std::function<void()> &fn);

    void enableLoggingToDisk();

    static QWidget *messageSourceWidget(const QModelIndex &message);

    void registerComposeWindow(ComposeWidget* widget);

    /** @short Answer the confirmation of a permanent operation; receives the kind, the title and the question */
    using ConfirmationPrompt = std::function<UiUtils::DestructiveActions::ConfirmationAnswer(
        UiUtils::DestructiveActions::Confirmation, const QString &, const QString &)>;
    /** @short Replace the confirmation dialog, for example in tests; an empty function restores the dialog */
    void setConfirmationPrompt(const ConfirmationPrompt &prompt);

signals:
    /** @short An account was added, renamed or removed */
    void accountsChanged();

public slots:
    void slotComposeMailUrl(const QUrl &url);
    void slotComposeMail();
    void invokeContactEditor();
protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *o, QEvent *e) override;
private slots:
    void showContextMenuMboxTree(const QPoint &position);
    void showContextMenuMsgListTree(const QPoint &position);
    void slotReloadMboxList();
    void slotResyncMbox();
    void alertReceived(const QString &message);
    void networkPolicyOffline();
    void networkPolicyExpensive();
    void networkPolicyOnline();
    void slotShowSettings();
    void slotShowAccountSettings();
    void slotAddAccount();
    void slotRemoveAccount();
    void slotNextAccount();
    void slotPreviousAccount();
    void slotFocusMessageView();
    void slotFocusNextRegion();
    void slotFocusPreviousRegion();
    void slotReturnToMessageList();
    void removeAccountNow(const QString &accountId);
    /** Authorize an OAuth account in the browser again, then reconnect it */
    void signInAccount(const QString &accountId);
    void slotRenameAccount();
    /** Show the menu of an account's sidebar header, with its settings, renaming and removal */
    void showAccountMenu(const QString &accountId, const QPoint &globalPos);
    void slotShowImapInfo();
    void slotExpunge();
    void imapError(const QString &message);
    void networkError(const QString &message);
    void cacheError(const QString &message);
    void authenticationRequested();
    void authenticationContinue(const QString &pass, const QString &errorMessage = QString());
    void slotManageContact(const QUrl &url);
    void slotEditDraft();
    void slotReplyTo();
    void slotReplyAllButMe();
    void slotReplyAll();
    void slotReplyList();
    void slotReplyGuess();
    void slotForwardAsAttachment();
    void slotResend();
    void slotUpdateMessageActions();
    void handleTag(const bool checked, const int index);
    void handleMarkAsRead(bool);
    void handleMarkAsUnread();
    void handleMarkAsDeleted(bool);
    void handleMarkAsFlagged(const bool);
    void handleMarkAsJunk(const bool);
    void handleMarkAsNotJunk(const bool);
    void slotMoveToArchiveFailed(const QString &error, int count = 0);
    void handleMoveToArchive();
    void slotMoveToFolder();
    void slotCopyToFolder();
    /** @short Put @p mailbox first among the active account's recent Move To and Copy To destinations */
    void rememberRecentFolder(const QString &mailbox);
    void handleDeletePermanently();
    void slotNextUnread();
    void slotPreviousUnread();
    /** @short Mark the selected messages as read, then show the next unread one */
    void slotMarkReadAndNextUnread();
    /** @short Show the next or previous message in the list, read or not */
    void slotNextMessage();
    void slotPreviousMessage();
    void msgListClicked(const QModelIndex &);
    void saveMessageSelection(const QModelIndex &);
    /** @short Show the message which became current because the previous one left the list */
    void slotMessageReplacedAfterRemoval(const QModelIndex &index);
    void slotShowShortcutOverview();
    void openCompleteMessageWidget();
    void slotCreateMailboxBelowCurrent();
    void slotMarkCurrentMailboxRead();
    void slotCreateTopMailbox();
    void slotDeleteCurrentMailbox();
    void slotRenameCurrentMailbox();
    void handleTrayIconChange();
    void slotSubscribeCurrentMailbox();
    void slotShowOnlySubscribed();
    void updateMessageFlags();
    void updateMessageFlagsOf(const QModelIndex &index);
    void scrollMessageUp();
    void slotMessageModelChanged(QAbstractItemModel *model);
    void showConnectionStatus(uint parserId, Imap::ConnectionState state);
    void slotShowLinkTarget(const QString &link);
    void slotShowAboutTrojita();

    void slotSaveCurrentMessageBody();
    void slotViewMsgSource();
    void slotViewMsgHeaders();
    void slotThreadMsgList();
    void slotHideRead();
    /** @short Clear the quick search, its date range and the quick filters, and go back to where the list was before */
    void clearSearchAndFilters();
    void slotSortingPreferenceChanged();
    void slotSortingConfirmed(int column, Qt::SortOrder order);
    void slotSearchRequested(const QStringList &searchConditions);
    void slotCapabilitiesUpdated(const QStringList &capabilities);
    void slotGoToFolder();
    /** Open the next folder of the active account which has unread messages */
    void slotGoToNextUnreadFolder();
    /** Ask every online account for new mail, and resynchronize the open folder */
    void slotCheckMail();
    void updateGoToFolderActions();

    void slotMailboxDeleteFailed(const QString &mailbox, const QString &msg);
    void slotMailboxCreateFailed(const QString &mailbox, const QString &msg);
    void slotMailboxSyncFailed(const QString &mailbox, const QString &msg);
    void slotMailboxRenameSucceeded(const QString &oldName, const QString &newName);
    void slotMailboxRenameFailed(const QString &oldName, const QString &newName, const QString &msg);
    void slotMailboxEmptied(const QString &mailbox);
    void slotMailboxEmptyFailed(const QString &mailbox, const QString &msg);
    void slotMailboxChanged(const QModelIndex &mailbox);

    void slotDownloadTransferError(const QString &errorString);
    void slotDownloadMessageFileNameRequested(QString *fileName);
    void slotScrollToUnseenMessage();
    void slotScrollToCurrent();
    void slotUpdateWindowTitle();
    /** @short Update the counts and the selection size below the message list soon */
    void scheduleMessageListStatus();
    void updateMessageListStatus();
    /** @short Select the whole thread of the current message */
    void slotSelectThread();
    void slotInvertSelection();
    /** @short Enable the Show Related and Select Similar actions which the current message supports */
    void updateRelatedActions();
    /** @short Switch to (or back from) compact rows without the panels which triage does not need */
    void setDenseProcessingLayout(bool enabled);
    /** @short Show the sort column and order on the message list's Sort button */
    void updateSortButton();

    void slotLayoutCompact();
    void slotLayoutWide();
    void slotLayoutOneAtTime();
    void saveSizesAndState();
    void flushUiState();
    void saveRawStateSetting(bool enabled);
    void possiblyLoadMessageOnSplittersChanged();
    void updateNetworkIndication();

    void desktopGeometryChanged();

    void slotIconActivated(const QSystemTrayIcon::ActivationReason reason);
    void slotToggleSysTray();

    void slotResetReconnectState();

    void slotPluginsChanged();

    void showStatusMessage(const QString &message);

    void slotFavoriteTagsChanged();

    void recoverDrafts();

protected:
    void resizeEvent(QResizeEvent *) override;

private:
    void reply(const Composer::ReplyMode mode);
    /** @short Select and open a mailbox of the active account, showing the message list */
    void goToMailbox(const QString &mailbox);
    void goToSpecialFolder(Imap::Mailbox::SpecialUse kind);

    void defineActions();
    void createMenus();
    void createActions();
    void createWidgets();

    /** Load the account list, migrating a single-account profile into the first account. */
    void setupAccounts();

    /** Return the ImapAccess of an account, creating it on first use. */
    Imap::ImapAccess *accountAccess(const QString &accountId);

    /** Make another account the one shown in the window; models of other accounts stay alive. */
    void switchAccount(const QString &accountId);

    /** Ask for a new name of an account. */
    void renameAccount(const QString &accountId);

    /** Ask whether to remove an account, and remove it. */
    void confirmRemoveAccount(const QString &accountId);
    /** Whether another account signs in with the same provider, client and user, so a revocation would reach it */
    bool sharesOAuthGrant(const QString &accountId) const;

    /** Disconnect the window from the models of the active account. */
    void detachAccountModels();

    /** Record connection and authentication status of an account from its IMAP model signals. */
    void monitorAccount(const QString &accountId);

    /** Connect accounts other than the active one so they keep running in the background. */
    void connectBackgroundAccounts();

    /** Update the unread INBOX counts in the account sidebar soon, coalescing repeated requests. */
    void scheduleAccountUnreadRefresh();
    void refreshAccountUnread();

    /** Supply the stored password to an account that is not shown, or mark it as needing one. */
    void requestStoredPassword(const QString &accountId);
    /** Ask whether to trust the TLS connection of @p accountId and answer its access, not the active one */
    void checkSslPolicy(const QString &accountId);

    /** Switch to the account @p step positions after the active one, wrapping around. */
    void cycleAccount(int step);
    /** Move keyboard focus to the next visible shell region in @p step direction */
    void cycleFocusRegion(int step);
    /** Folders expanded in the active account's mailbox tree */
    QStringList expandedMailboxes() const;

    /** Rebuild the account selector from the account list. */
    void refreshAccountSwitcher();

    /** Show the other accounts that need attention in the footer. */
    void updateBackgroundAlerts();

    /** Tell the user that an account other than the shown one needs attention. */
    void notifyBackgroundAccount(const QString &accountId);

    /** @param reconnect connect the account to its server; false when its models already exist */
    void setupModels(bool reconnect = true);
    QStringList copyrightHolders(QFile *file) const;

    void nukeModels();
    void connectModelActions();

    /** @short Apply the active account's quick filters to the message list */
    void applyMessageListFilters();
    /** @short Remember the message list's quick filters for the active account */
    void saveMessageListFilters();
    /** @short Show the state of the quick filters on their actions and buttons */
    void syncMessageListFilterUi();
    /** @short Whether a search or a quick filter hides messages of the list */
    bool messageListNarrowed() const;
    /** @short Note where the folder's list is before a search or a filter narrows it, for clearSearchAndFilters() */
    void rememberUnnarrowedPosition();
    void updateSelectionActions();
    /** @short Limit the quick search to messages sharing @p field with the current message */
    void showRelatedMessages(UiUtils::FieldFilter field);
    /** @short Select the loaded messages of the list which match @p criterion */
    void selectSimilarMessages(UiUtils::SimilarMessages criterion);
    void goToAdjacentMessage(bool forward);
    /** @short Open the next or previous message of the current message's conversation */
    void goToMessageInConversation(bool forward);
    /** @short Whether the message list shows threads, rather than a flat list or groups */
    bool showsThreads() const;
    /** @short The row of the message list for @arg message of the IMAP model, shown even if the quick filters hide it */
    QModelIndex revealMessage(const QModelIndex &message);

    void createMailboxBelow(const QModelIndex &index);

    void updateActionsOnlineOffline(bool online);

    /** Place the mailbox sidebar on the requested side of the current layout. */
    void setMailboxSidebarRight(bool right);

    /** Create the shell: the header above the left, main and right columns, and a footer below them. */
    void createShell();
    /** Put the mailbox tree into the column chosen for it. */
    void placeMailboxSidebar();
    /** Move each embedded panel into the region chosen by its placement preference. */
    void placeEmbeddedPanels();
    /** Remember the size of each shown shell region, so that it comes back at the same size. */
    void rememberShellExtents();
    /** Show the shell regions which have content, hide the others and keep the main column's share. */
    void updateShellRegions(bool rememberCurrent = true, bool force = false);
    bool shellRegionHasContent(const QSplitter *region) const;
    /** Give shown regions their remembered size and the rest to the main column, once a splitter is laid out */
    void applyPendingShellSizes(QSplitter *only = nullptr);
    /** The splitter which holds @p panel at its current placement */
    QSplitter *panelRegion(const EmbeddedPanel *panel) const;
    /** Remember the size of each shown panel in @p region, for example after a divider was moved */
    void rememberPanelExtents(QSplitter *region);
    /** Give the shown panels of each region their remembered size or their size hint */
    void fitPanelRegions();
    /** Give the shown panels of @p region their remembered size or their size hint, once it is laid out */
    void fitPanelRegion(QSplitter *region);
    /** Select a message picked in the related messages panel. */
    void activateRelatedMessage(const QModelIndex &message);
    /** @short Run @p fn while actions work on the message open in the reader, and on its account */
    void actOnOpenMessage(const std::function<void()> &fn);
    /** Bring the reader's action bar in line with the open message and its account */
    void updateReaderActions();
    /** Show the attachments panel, even when it was turned off, and move the focus into it */
    void showAttachmentPanel();

    /** Display the current connection status in the window footer. */
    void updateConnectionStatus(const QString &message);

    /** Resume the selected mailbox when network access returns. */
    void resumeCurrentMailbox();

    void applySizesAndState();
    QString layoutToString(const LayoutMode layout) const;
    void convertSizesAndStateSettings();
    void convertSizesAndStateSetting(const LayoutMode layout);
    QList<int> migrateLegacyPaneSizes();
    QString settingsKeyForLayout(const LayoutMode layout);

    void createSysTray();
    void removeSysTray();

    /** @short The selected messages as indexes of the MsgListModel, without the headers of groups */
    Q_INVOKABLE QModelIndexList translatedSelection() const;

    /** @short Show the message list flat, in conversations or in groups; @p remember stores the choice for the account */
    void setMessageListGrouping(Imap::Mailbox::GroupingMsgListModel::Grouping grouping, bool remember = true);
    /** @short Arrange the message list as remembered for the current folder, or as the account's default */
    void applyMessageListView();
    /** @short Sort the message list as the arrangement of the current folder says, unless it is sorted by relevance */
    void applyMessageListSort();
    /** @short The threading or grouping which the message list shows */
    Imap::Mailbox::GroupingMsgListModel::Grouping shownGrouping() const;
    /** @short Remember the shown threading and grouping as the arrangement of the current folder */
    void rememberMessageListView();
    /** @short Remember a sorting picked by the user as the arrangement of the current folder */
    void rememberMessageListSort(int column, Qt::SortOrder order);
    /** @short Store m_view for the current folder, or as the account's default when no folder is open */
    void storeMessageListView();
    void saveMessageListViews();
    /** @short Make the current folder's arrangement the account's default for all folders, after asking */
    void applyMessageListViewToAllFolders();
    /** @short Let the current folder follow the account's default arrangement again */
    void resetMessageListViewForFolder();
    void updateMessageListViewActions();
    /** @short Forget the arrangements and positions of folders which are gone */
    void pruneFolderStates();
    /** @short The remembered positions in the message lists of the active account's folders */
    UiUtils::FolderPositionStore &folderPositions();
    /** @short Note where the message list of the open folder is, so that coming back can restore it */
    void captureFolderPosition(bool anchorOnly = false);
    /** @short Remember the noted position of the folder which the message list showed */
    void commitFolderPosition();
    /** @short Go back to the remembered position in the message list of the open folder */
    void restoreFolderPosition();
    void saveFolderPositions();
    /** @short Show the account and the folder of the message list */
    void updateMessageListScope();
    void syncGroupByActions();
    /** @short Show @p model in the message list, keeping the current and the selected messages */
    void swapMessageListModel(QAbstractItemModel *model);
    void connectMessageListSelection();
    /** @short Expand the groups except those which the user has collapsed */
    void applyGroupExpansion();
    /** @short Sort by relevance while @p searchConditions look for text, otherwise go back to the previous sorting */
    void updateRelevanceSorting(const QStringList &searchConditions);
    /** @short Map an index of the message list view to the PrettyMsgListModel, an invalid index for a group's header */
    QModelIndex toPrettyIndex(const QModelIndex &viewIndex) const;
    QModelIndex fromPrettyIndex(const QModelIndex &prettyIndex) const;

    /** @short Where a move which can be undone went */
    enum class MoveKind {
        Trash,
        Archive,
        Folder,
    };
    /** @short Name of the mailbox shown in the message list */
    QString currentMailboxName() const;
    /** @short The mailbox holding all of @arg messages, such as a message opened from another folder, else the list's */
    QString sourceMailboxName(const QModelIndexList &messages) const;
    /** @short Ask before a permanent operation unless the user opted out; true means go ahead */
    bool confirmPermanentAction(UiUtils::DestructiveActions::Confirmation kind, const QString &title, const QString &text,
                                const QString &acceptText);
    /** @short The active account's "Move Deleted Messages to Trash" setting */
    bool deleteMovesToTrash() const;
    /** @short The Trash folder which Delete moves the selected messages to, or an empty string to mark them */
    QString deleteTrashTarget() const;
    /** @short Make the Delete action either move to Trash or toggle the \\Deleted mark */
    void updateDeleteActionMode();
    /** @short Report the result of moving @p count messages from @p source to @p target, offering Undo if possible */
    void trackMove(Imap::Mailbox::ImapTask *task, const QString &source, const QString &target, int count, MoveKind kind);
    /** @short Report the result of copying @p count messages to @p target */
    void trackCopy(Imap::Mailbox::ImapTask *task, const QString &target, int count);
    /** @short Show @p busyText while @p task runs for longer than a moment, until its result replaces it */
    void showBusyWhileRunning(Imap::Mailbox::ImapTask *task, const QString &busyText);
    /** @short Report the result of changing flags of @p count messages

    @p doneText is shown on success unless it is empty; @p failedText builds the error from the server's message.
    */
    void trackFlagChange(Imap::Mailbox::ImapTask *task, const QString &busyText, const QString &doneText,
                         const std::function<QString(const QString &)> &failedText);
    /** @short The actions defined in ShortcutHandler, each with the menu or the part of the window it belongs to */
    QList<UiUtils::ShortcutOverview::Entry> shortcutOverviewEntries() const;
    /** @short The folder which Archive moves messages to */
    QString archiveTarget() const;
    /** @short Copy or move the messages to @p mailbox, remembering it as a recent destination */
    void copyOrMoveToFolder(const QModelIndexList &messages, const QString &mailbox, Imap::Mailbox::CopyMoveOperation op);
    /** @short Let the user choose the folder to copy or move the selected messages to */
    void pickFolder(Imap::Mailbox::CopyMoveOperation op);
    /** @short Fill a Move To or Copy To menu with the recent destinations of the active account */
    void populateRecentFoldersMenu(QMenu *menu, Imap::Mailbox::CopyMoveOperation op);
    /** @short Move messages which an earlier move put into @p target back to @p source */
    void undoMove(const QString &source, const QString &target, uint uidValidity, const Imap::Uids &uids);
    /** @short Open the message list's mailbox again after an operation had to select @p usedMailbox */
    void reopenCurrentMailbox(const QString &usedMailbox);
    bool canRenameMailbox(const QModelIndex &index) const;
    void emptySpecialFolder(Imap::Mailbox::SpecialUse kind);

    /** @short The account which actions work on: the shown one, or the one of the messages handled right now */
    Imap::ImapAccess *currentAccess() const;
    QString currentAccountId() const;
    /** @short Whether the message list shows messages of @p model, so that feedback about them makes sense */
    bool showsModel(const Imap::Mailbox::Model *model) const;
    /** @short The ID of the account which @p model belongs to, or an empty string */
    QString accountOfModel(const Imap::Mailbox::Model *model) const;
    /** @short Run @p fn while actions work on @p accountId and its messages in @p selection */
    void actAs(const QString &accountId, const QModelIndexList &selection, const std::function<void()> &fn);
    /** @short Run @p fn while actions work on the account of @p model; false when that account is gone */
    bool actOnModel(const Imap::Mailbox::Model *model, const std::function<void()> &fn);
    /** @short Run @p fn once for each account of the selected messages in the Unified Inbox

    Returns false, doing nothing, when the messages all belong to the shown account or no unified list is shown. With
    @p singleAccount, messages of several accounts are refused with an error. An empty @p selection means the selected
    messages.
    */
    bool routeToMessageAccounts(const std::function<void()> &fn, bool singleAccount = false,
                                const QModelIndexList &selection = QModelIndexList());
    /** @short @p text, prefixed with the account of @p model while the Unified Inbox shows several accounts */
    QString forAccountOf(const Imap::Mailbox::Model *model, const QString &text) const;
    /** @short Show the INBOX of all accounts in one list, or go back to the folder which was shown before */
    void setUnifiedInbox(bool enabled, bool returnToFolder = true);
    void syncUnifiedSources();
    void updateUnifiedStates();
    QStringList unifiedAccountIds() const;
    void populateUnifiedAccountsMenu();
    /** @short Whether the list shows messages of several folders or accounts: the Unified Inbox or search results */
    bool showsUnifiedList() const;
    /** @short Show the unified list instead of the folder's list, or the folder's list again */
    void showUnifiedListModel();
    void showFolderListModel();
    /** @short Offer the actions which only make sense for one folder, or disable them for a unified list */
    void setFolderViewActionsEnabled(bool enabled);
    /** @short Search the places chosen in the quick search, which span several folders or accounts */
    void startSearchAcross(const QStringList &conditions);
    /** @short Stop searching several folders and show the list from before the search */
    void leaveSearchAcross();
    void cancelFolderSearches();
    /** @short Search @p mailboxes, or all folders, of @p accountId for the shown results, or report why it cannot be

    With @p retrying, the places are searched again and the results found before stay; an account which lost its
    connection is asked to connect first.
    */
    void searchAccount(const QString &accountId, const QStringList &mailboxes, bool allFolders, bool retrying);
    void addSearchProblem(const QString &accountId, const QString &mailbox, const QString &text);
    bool isSearchAcrossRunning() const;
    /** @short Stop the running searches and keep the results found so far */
    void stopSearchAcross();
    /** @short Search the places which could not be searched again */
    void retrySearchAcross();
    /** @short Progress of each account, for the tooltip of the status */
    QString searchProgressDetails() const;
    void folderSearchFinished(const QString &accountId);
    /** @short Ask which folders the quick search looks at */
    void chooseSearchFolders();
    /** @short Ask which folders of @arg accountId the related messages panel searches on the server */
    void chooseRelatedFolders(const QString &accountId);
    /** @short Let the related messages panel search the server again once the search across folders is done */
    void resumeRelatedSearch();

    Common::AccountRegistry *m_accounts = nullptr;
    Common::AccountStatusTracker *m_accountStatus = nullptr;
    QHash<QString, Imap::ImapAccess *> m_accountAccess;
    QComboBox *m_accountSwitcher = nullptr;
    Imap::ImapAccess *m_imapAccess;

    Imap::Mailbox::PrettyMailboxModel *prettyMboxModel;
    Imap::Mailbox::PrettyMsgListModel *prettyMsgListModel;
    /** Shown on top of prettyMsgListModel while the messages are grouped by something else than conversations */
    Imap::Mailbox::GroupingMsgListModel *m_groupingMsgListModel = nullptr;
    Imap::Mailbox::GroupingMsgListModel::Grouping m_groupBy = Imap::Mailbox::GroupingMsgListModel::Grouping::None;
    /** Arrangements of the message list of the active account's folders */
    UiUtils::ViewStateStore m_viewStore;
    /** Arrangement which the message list should have for m_viewMailbox */
    UiUtils::MessageListView m_view;
    QString m_viewMailbox;
    /** Nonzero while the message list is arranged from m_view rather than by the user */
    int m_applyingView = 0;
    /** Sorting from the saved header state, the default of all accounts */
    std::optional<std::pair<int, Qt::SortOrder>> m_globalSort;
    /** Positions in the message lists, per account ID */
    QHash<QString, UiUtils::FolderPositionStore> m_folderPositions;
    /** Folder which the message list shows, and where in it the list is */
    QString m_positionMailbox;
    UiUtils::FolderPosition m_livePosition;
    /** Folder whose list was narrowed by a search or a filter, and where in it the list was before */
    QString m_unnarrowedMailbox;
    UiUtils::FolderPosition m_unnarrowedPosition;
    /** Whether the user opened a message while the list was narrowed, which then stays current after clearing */
    bool m_pickedWhileNarrowed = false;
    QPointer<MessageSelectionRestorer> m_messageRestorer;
    /** Keys of the groups which the user has collapsed during this session */
    QSet<QString> m_collapsedGroups;
    Composer::SenderIdentitiesModel *m_senderIdentities;
    QHash<QString, Composer::SenderIdentitiesModel *> m_accountIdentities;
    /** Always empty; used by accounts that send without an identity */
    Composer::SenderIdentitiesModel *m_noIdentities = nullptr;
    Imap::Mailbox::FavoriteTagsModel *m_favoriteTags;

    MailBoxTreeView *mboxTree;
    MessageListWidget *msgListWidget;
    QTreeView *allTree;
    QDockWidget *allDock;
    QTreeView *taskTree;
    QDockWidget *taskDock;
    QTreeView *mailMimeTree;
    QDockWidget *mailMimeDock;

#ifdef TROJITA_HAVE_WEBENGINE
    CompleteMessageWidget *m_messageWidget;
#else
    QLabel *m_messageWidget;
#endif
    /** The reader and the embedded panels which follow its message, stacked vertically */
    QSplitter *m_readerColumn = nullptr;
    AttachmentPanel *m_attachmentPanel = nullptr;
    /** Actions above the reader which work on the open message */
    ReaderActionBar *m_readerActions = nullptr;
    MessageDetailsPanel *m_messageDetailsPanel = nullptr;
    ContactPanel *m_contactPanel = nullptr;
    RelatedMessagesPanel *m_relatedMessagesPanel = nullptr;
    /** All embedded panels, in their canonical order within a region */
    QList<EmbeddedPanel *> m_embeddedPanels;

    /** A collapsible region at the edge of the shell and the size it had when last shown */
    struct ShellRegion {
        QSplitter *widget = nullptr;
        int extent = -1;
        bool shown = false;
    };
    /** The shell: shellRow above the footer */
    QSplitter *m_shellBody = nullptr;
    /** The left column, the main column and the right column */
    QSplitter *m_shellRow = nullptr;
    /** The message list and the reader column side by side or stacked, or one panel at a time */
    QSplitter *m_mainColumn = nullptr;
    ShellRegion m_shellLeft;
    ShellRegion m_shellRight;
    ShellRegion m_shellFooter;
    bool m_shellRegionsInitialized = false;
    /** Whether region sizes still have to be applied once the shell has its real size */
    bool m_shellRowSizesPending = false;
    bool m_shellBodySizesPending = false;
    /** Size of each panel along its region after the user moved a divider; panels without one use their size hint */
    QHash<EmbeddedPanel *, int> m_panelExtents;
    /** Panel regions which still have to be fitted once they are laid out */
    QSet<QSplitter *> m_panelRegionsPending;
    /** The folder tree grouped by account, in the column chosen for the mailbox sidebar */
    AccountSidebar *m_accountSidebar = nullptr;
    /** The header region above the columns, off by default */
    ShellHeader *m_shellHeader = nullptr;
    /** Hides the actions which the active account's protocol does not support */
    CapabilityActionGate *m_capabilityGate = nullptr;

    ProtocolLoggerWidget *protocolLogger;
    QDockWidget *protocolLoggerDock;

    QPointer<QStackedWidget> m_mainStack;
    QLabel *m_connectionStatus;
    /** Footer button listing the other accounts that need attention */
    QToolButton *m_backgroundAlerts = nullptr;
    /** Footer button which signs the shown OAuth account in again after its sign-in failed */
    QToolButton *m_oauthSignIn = nullptr;
    bool m_connectionError = false;

    LayoutMode m_layoutMode;
    bool m_mailboxSidebarOnRight = false;
    bool m_skipSavingOfUI;
    QTimer *m_delayedStateSaving;

    QAction *reloadMboxList;
    QAction *reloadAllMailboxes;
    QAction *resyncMbox;
    QAction *netOffline;
    QAction *netExpensive;
    QAction *netOnline;
    QAction *m_netToolbarDefaultAction;
    QAction *exitAction;
    QAction *showFullView;
    QAction *showTaskView;
    QAction *showMimeView;
    QAction *showProtocolLogger;
    QAction *logPersistent;
    QAction *showImapCapabilities;
    QAction *showMenuBar;
    QAction *showToolBar;
    QAction *configSettings;
    QAction *m_actionAddAccount;
    QAction *m_actionAccountSettings;
    QAction *m_actionRenameAccount;
    QAction *m_actionRemoveAccount;
    QAction *m_oneAtTimeGoBack;
    QAction *composeMail;
    QAction *m_editDraft;
    QAction *m_replyPrivate;
    QAction *m_replyAllButMe;
    QAction *m_replyAll;
    QAction *m_replyList;
    QAction *m_replyGuess;
    QAction *m_forwardAsAttachment;
    QAction *m_resend;
    QAction *expunge;
    QAction *createChildMailbox;
    QAction *createTopMailbox;
    QAction *deleteCurrentMailbox;
    QAction *m_renameMailbox = nullptr;
    QAction *m_emptyTrash = nullptr;
    QAction *m_emptyJunk = nullptr;
    QAction *aboutTrojita;

    QAction *tag1;
    QAction *tag2;
    QAction *tag3;
    QAction *tag4;
    QAction *tag5;
    QAction *tag6;
    QAction *tag7;
    QAction *tag8;
    QAction *tag9;
    QAction *markAsRead;
    QAction *markAsDeleted;
    QAction *markAsFlagged;
    QAction *markAsJunk;
    QAction *markAsNotJunk;
    QAction *moveToArchive;
    QAction *m_markAsUnread = nullptr;
    QAction *m_moveToFolder = nullptr;
    QAction *m_copyToFolder = nullptr;
    QMenu *m_moveToMenu = nullptr;
    QMenu *m_copyToMenu = nullptr;
    QToolButton *m_moveButton = nullptr;
    QAction *m_deletePermanently = nullptr;
    QAction *m_deleteMovesToTrash = nullptr;
    /** @short Text and icon of the Delete action while it toggles the \\Deleted mark */
    QString m_markAsDeletedText;
    QIcon m_markAsDeletedIcon;
    QAction *saveWholeMessage;
    QAction *viewMsgSource;
    QAction *viewMsgHeaders;
    QAction *m_nextMessage;
    QAction *m_previousMessage;

    QAction *actionThreadMsgList;
    QAction *actionHideRead;
    QAction *m_actionSortByArrival;
    QAction *m_actionSortByCc;
    QAction *m_actionSortByDate;
    QAction *m_actionSortByFrom;
    QAction *m_actionSortBySize;
    QAction *m_actionSortBySubject;
    QAction *m_actionSortByTo;
    QAction *m_actionSortThreading;
    QAction *m_actionSortNone;
    QAction *m_actionSortAscending;
    QAction *m_actionSortDescending;
    QAction *m_actionSortByRelevance = nullptr;
    QActionGroup *m_groupByActions = nullptr;
    QMenu *m_groupByMenu = nullptr;
    QAction *m_actionExpandAllGroups = nullptr;
    QAction *m_actionViewForAllFolders = nullptr;
    QAction *m_actionResetFolderView = nullptr;
    QAction *m_actionCollapseAllGroups = nullptr;
    QAction *m_actionLayoutCompact;
    QAction *m_actionLayoutWide;
    QAction *m_actionLayoutOneAtTime;
    QAction *m_actionMarkMailboxAsRead;
    QAction *m_actionFilterUnread = nullptr;
    QAction *m_actionFilterFlagged = nullptr;
    QAction *m_actionFilterAttachments = nullptr;
    QAction *m_actionClearFilters = nullptr;
    QToolButton *m_clearFiltersButton = nullptr;
    QToolButton *m_sortButton = nullptr;
    QAction *m_actionSelectAllMessages = nullptr;
    QAction *m_actionSelectThread = nullptr;
    QAction *m_actionInvertSelection = nullptr;
    QAction *m_actionDeselectMessages = nullptr;
    QMenu *m_showRelatedMenu = nullptr;
    QAction *m_actionRelatedSender = nullptr;
    QAction *m_actionRelatedRecipient = nullptr;
    QAction *m_actionRelatedSubject = nullptr;
    QAction *m_actionRelatedList = nullptr;
    QMenu *m_selectSimilarMenu = nullptr;
    QAction *m_actionSelectSameSender = nullptr;
    QAction *m_actionSelectSameSubject = nullptr;
    QAction *m_actionSelectUnread = nullptr;
    QAction *m_actionSelectFlagged = nullptr;
    QAction *m_markReadNextUnread = nullptr;
    QAction *m_nextAnyMessage = nullptr;
    QAction *m_previousAnyMessage = nullptr;
    QAction *m_nextInConversation = nullptr;
    QAction *m_previousInConversation = nullptr;
    QAction *m_expandAllConversations = nullptr;
    QAction *m_collapseAllConversations = nullptr;
    QAction *m_actionDenseProcessing = nullptr;
    DensityMenu *m_densityMenu = nullptr;
    MessageRowsMenu *m_messageRowsMenu = nullptr;
    EmbeddedPanelsMenu *m_embeddedPanelsMenu = nullptr;
    QTimer *m_listStatusTimer = nullptr;
    QAction *m_actionShortcutOverview = nullptr;
    QPointer<ShortcutOverviewDialog> m_shortcutOverview;

    QAction *m_actionSubscribeMailbox;
    QAction *m_actionShowOnlySubscribed;

    QAction *m_goToFolder;
    /** Opens the search builder of the message list */
    QAction *m_searchBuilder;
    /** Clears the search and the quick filters of the message list at once */
    QAction *m_clearSearchAndFilters = nullptr;
    QAction *m_goToNextUnreadFolder = nullptr;
    QAction *m_checkMail = nullptr;
    QTimer *m_accountUnreadTimer = nullptr;
    QMenu *m_goToFolderMenu;
    QMap<Imap::Mailbox::SpecialUse, QAction *> m_goToSpecialFolderActions;

    QAction *m_actionContactEditor;

    QToolBar *m_mainToolbar;
    QToolButton *m_menuFromToolBar;
    QToolButton *m_replyButton;
    QMenu *m_replyMenu;
    QToolButton *m_composeButton;
    QMenu *m_composeMenu;

    TaskProgressIndicator *busyParsersIndicator;
    QToolButton *networkIndicator;

    bool m_ignoreStoredPassword;

    QSettings *m_settings;
    Plugins::PluginManager *m_pluginManager;
    MessageActionHandler m_msgActionHandler;

    QMessageBox *m_networkErrorMessageBox;

    /** @short Feedback about operations on messages and folders, above the message list */
    InlineNotificationBar *m_notificationBar = nullptr;
    ConfirmationPrompt m_confirmationPrompt;
    /** @short For each folder being renamed, the new name of the open mailbox which the rename affects */
    QHash<QString, QString> m_renamedCurrentMailbox;

    Imap::Mailbox::UnifiedMsgListModel *m_unifiedModel = nullptr;
    Imap::Mailbox::PrettyUnifiedMsgListModel *m_prettyUnifiedModel = nullptr;
    bool m_unifiedInbox = false;
    QAction *m_actionUnifiedInbox = nullptr;
    QMenu *m_unifiedAccountsMenu = nullptr;
    /** The folder to go back to when the Unified Inbox is left */
    QString m_unifiedReturnMailbox;
    /** Whether threading could be chosen before the Unified Inbox disabled it */
    bool m_unifiedThreadingEnabled = true;
    /** While the list shows the results of a search in several folders or accounts */
    bool m_searchAcross = false;
    /** The running or finished searches of the shown results, by account */
    QHash<QString, QPointer<Imap::Mailbox::FolderSearch>> m_folderSearches;
    /** The searched places, for the tooltip of the list */
    QStringList m_searchedPlaces;
    struct SearchProblem {
        QString accountId;
        /** Full mailbox name, or empty when the account could not be searched at all */
        QString mailbox;
        QString text;
    };
    /** Accounts and folders which could not be searched */
    QList<SearchProblem> m_searchProblems;
    /** The conditions and the places of the shown results, for searching places again */
    QStringList m_searchConditions;
    QList<UiUtils::SearchScope::AccountFolders> m_searchPlan;
    /** The user stopped the search before all places were searched */
    bool m_searchStopped = false;
    /** The notification which lists the places which could not be searched */
    QString m_searchProblemsNotice;
    QAction *m_actionStopSearch = nullptr;
    QAction *m_actionRetrySearch = nullptr;
    /** While an action works on messages of another account than the shown one */
    Imap::ImapAccess *m_actingAccess = nullptr;
    QString m_actingAccountId;
    QModelIndexList m_actingSelection;
    bool m_acting = false;

    MainWindow(const MainWindow &); // don't implement
    MainWindow &operator=(const MainWindow &); // don't implement

    QSystemTrayIcon *m_trayIcon;
    QPoint m_headerDragStart;
};

}

#endif
