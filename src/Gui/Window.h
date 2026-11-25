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
#include <QMainWindow>
#include <QModelIndex>
#include <QPointer>
#include <QSet>
#include <QSystemTrayIcon>

#include "configure.cmake.h"
#include "Composer/Recipients.h"
#include "Imap/ConnectionState.h"
#include "Imap/Model/Cache.h"
#include "MessageActionHandler.h"

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

class Model;
class PrettyMailboxModel;
class ThreadingMsgListModel;
class PrettyMsgListModel;
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
class ContactPanel;
class EmbeddedPanel;
class MessageDetailsPanel;
class RelatedMessagesPanel;
class CompleteMessageWidget;
class ComposeWidget;
class MailBoxTreeView;
class MessageListWidget;
class ProtocolLoggerWidget;
class TaskProgressIndicator;

class MainWindow: public QMainWindow
{
    Q_OBJECT
    typedef QList<QPair<Composer::RecipientKind,QString> > RecipientsType;

    typedef enum { LAYOUT_COMPACT, LAYOUT_WIDE, LAYOUT_ONE_AT_TIME } LayoutMode;
public:
    MainWindow(QSettings *settings);
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

    void enableLoggingToDisk();

    static QWidget *messageSourceWidget(const QModelIndex &message);

    void registerComposeWindow(ComposeWidget* widget);

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
    void handleMarkAsDeleted(bool);
    void handleMarkAsFlagged(const bool);
    void handleMarkAsJunk(const bool);
    void handleMarkAsNotJunk(const bool);
    void slotMoveToArchiveFailed(const QString &error);
    void handleMoveToArchive();
    void slotNextUnread();
    void slotPreviousUnread();
    void msgListClicked(const QModelIndex &);
    void saveMessageSelection(const QModelIndex &);
    void openCompleteMessageWidget();
    void slotCreateMailboxBelowCurrent();
    void slotMarkCurrentMailboxRead();
    void slotCreateTopMailbox();
    void slotDeleteCurrentMailbox();
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
    void slotSortingPreferenceChanged();
    void slotSortingConfirmed(int column, Qt::SortOrder order);
    void slotSearchRequested(const QStringList &searchConditions);
    void slotCapabilitiesUpdated(const QStringList &capabilities);

    void slotMailboxDeleteFailed(const QString &mailbox, const QString &msg);
    void slotMailboxCreateFailed(const QString &mailbox, const QString &msg);
    void slotMailboxSyncFailed(const QString &mailbox, const QString &msg);
    void slotMailboxChanged(const QModelIndex &mailbox);

    void slotDownloadTransferError(const QString &errorString);
    void slotDownloadMessageFileNameRequested(QString *fileName);
    void slotScrollToUnseenMessage();
    void slotScrollToCurrent();
    void slotUpdateWindowTitle();

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

    QModelIndexList translatedSelection() const;

    Common::AccountRegistry *m_accounts = nullptr;
    Common::AccountStatusTracker *m_accountStatus = nullptr;
    QHash<QString, Imap::ImapAccess *> m_accountAccess;
    QComboBox *m_accountSwitcher = nullptr;
    Imap::ImapAccess *m_imapAccess;

    Imap::Mailbox::PrettyMailboxModel *prettyMboxModel;
    Imap::Mailbox::PrettyMsgListModel *prettyMsgListModel;
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
    QAction *m_actionLayoutCompact;
    QAction *m_actionLayoutWide;
    QAction *m_actionLayoutOneAtTime;
    QAction *m_actionMarkMailboxAsRead;

    QAction *m_actionSubscribeMailbox;
    QAction *m_actionShowOnlySubscribed;

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

    MainWindow(const MainWindow &); // don't implement
    MainWindow &operator=(const MainWindow &); // don't implement

    QSystemTrayIcon *m_trayIcon;
    QPoint m_headerDragStart;
};

}

#endif
