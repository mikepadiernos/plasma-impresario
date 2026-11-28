/* Copyright (C) 2006 - 2015 Jan Kundrát <jkt@flaska.net>
   Copyright (C) 2013 - 2015 Pali Rohár <pali.rohar@gmail.com>
   Copyright (C) 2025 Espen Sandøy Hustad <espen@ehustad.com>

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

#include <algorithm>
#include <memory>
#include <QAuthenticator>
#include <QActionGroup>
#include <QColorDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QDBusConnection>
#include <QFileInfo>
#include <QInputDialog>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileSystemWatcher>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QAbstractScrollArea>
#include <QStandardPaths>
#include <QUuid>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScopedPointer>
#include <QScreen>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QSslError>
#include <QSslKey>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QThreadPool>
#include <QTimer>
#include <QTextDocument>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <KConfigGroup>
#include <KIO/ApplicationLauncherJob>
#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenFileManagerWindowJob>
#include <KIO/OpenUrlJob>
#include <KLazyLocalizedString>
#include <KLocalizedString>
#include <KApplicationTrader>
#include <KService>
#include <KSharedConfig>
#include <KWindowSystem>

#include "configure.cmake.h"
#ifdef IMPRESARIO_HAVE_KNOTIFICATIONS
#include <KNotification>
#endif
#include "Common/Application.h"
#include "Common/InvokeMethod.h"
#include "Common/Paths.h"
#include "Common/PortNumbers.h"
#include "Common/PrivateFiles.h"
#include "Common/SettingsNames.h"
#include "Composer/AccountIdentities.h"
#include "Composer/Mailto.h"
#include "Composer/Outbox.h"
#include "Composer/SenderIdentitiesModel.h"
#ifdef IMPRESARIO_HAVE_CRYPTO_MESSAGES
#  ifdef IMPRESARIO_HAVE_GPGMEPP
#    include "Cryptography/GpgMe++.h"
#  endif
#endif
#include "Imap/Model/HeaderBackfill.h"
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/ModelWatcher.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/NetworkWatcher.h"
#include "Imap/Model/PrettyMailboxModel.h"
#include "Imap/Model/PrettyMsgListModel.h"
#include "Imap/Model/SpecialFlagNames.h"
#include "Imap/Model/ThreadingMsgListModel.h"
#include "Imap/Model/UnifiedMsgListModel.h"
#include "Imap/Model/FolderSearch.h"
#include "Imap/Model/SearchIndexer.h"
#include "Imap/Model/SearchTextFetcher.h"
#include "Imap/Model/SearchResultsModel.h"
#include "Imap/Model/SearchSnippets.h"
#include "Imap/Model/FavoriteTagsModel.h"
#include "Imap/Model/GroupingMsgListModel.h"
#include "Imap/Model/Utils.h"
#include "Imap/Parser/MailAddress.h"
#include "Imap/Tasks/AppendTask.h"
#include "Imap/Tasks/CopyMoveMessagesTask.h"
#include "Imap/Tasks/ImapTask.h"
#include "Imap/Network/FileDownloadManager.h"
#include "MSA/ImapSubmit.h"
#include "MSA/Sendmail.h"
#include "MSA/SMTP.h"
#include "Plugins/AddressbookPlugin.h"
#include "Plugins/PasswordPlugin.h"
#include "Plugins/PluginManager.h"
#include "CompleteMessageWidget.h"
#include "ComposeWidget.h"
#include "DensityDelegate.h"
#include "Common/AccountRegistry.h"
#include "Common/AccountStatus.h"
#include "AgendaPanel.h"
#include "AttachmentPanel.h"
#include "ContactPanel.h"
#include "DensityMenu.h"
#include "EmbeddedPanelsMenu.h"
#include "InlineNotificationBar.h"
#include "MailBoxTreeView.h"
#include "MailboxPickerDialog.h"
#include "SearchFoldersDialog.h"
#include "SavedFilesSearchDialog.h"
#include "NextcloudBrowserDialog.h"
#include "NextcloudConnectDialog.h"
#include "NextcloudHub.h"
#include "NextcloudTransfers.h"
#include "SearchIndexDialog.h"
#include "MessageDetailsPanel.h"
#include "MessageListWidget.h"
#include "MessageRowsMenu.h"
#include "MessageView.h"
#include "MessageSourceWidget.h"
#include "Gui/AppearanceManager.h"
#include "Gui/AppearanceStyle.h"
#include "Gui/FileChooser.h"
#include "Gui/LinkTarget.h"
#include "Gui/MessageHeadersWidget.h"
#include "MsgListView.h"
#include "MessageSelectionRestorer.h"
#include "OnePanelAtTimeWidget.h"
#include "PasswordDialog.h"
#include "Pop3Fetcher.h"
#include "JmapSyncer.h"
#include "Jmap/Submission.h"
#include "Common/TrustedCertificates.h"
#include "ReaderActionBar.h"
#include "RelatedMessagesPanel.h"
#include "SearchResultsPanel.h"
#include "ProtocolLoggerWidget.h"
#include "SettingsDialog.h"
#include "SettingsGroups.h"
#include "PlasmaActivities.h"
#include "MailTabBar.h"
#include "VimMode.h"
#include "ShortcutOverviewDialog.h"
#ifdef IMPRESARIO_HAVE_WEBENGINE
#include "SimplePartWidget.h"
#endif
#include "Streams/SocketFactory.h"
#include "TaskProgressIndicator.h"
#include "StateColors.h"
#include "Util.h"
#include "Window.h"
#include "ShortcutHandler/ShortcutHandler.h"

#include "ui_CreateMailboxDialog.h"
#include "ui_AboutDialog.h"
#include "ui_ImapInfoDialog.h"

#include "Imap/Model/ModelTest/modeltest.h"
#include "UiUtils/AccountNotices.h"
#include "UiUtils/BackgroundRunning.h"
#include "UiUtils/IconLoader.h"
#include "UiUtils/MailSafetySettings.h"
#include "UiUtils/MessageId.h"
#include "UiUtils/MessageIdLookup.h"
#include "UiUtils/PlasmaSearch.h"
#include "UiUtils/QaimDfsIterator.h"
#include "UiUtils/RelatedMessages.h"
#include "UiUtils/Relevance.h"
#include "UiUtils/SavedSearches.h"
#include "UiUtils/FolderColors.h"
#include "UiUtils/SettingsStore.h"
#include "UiUtils/Keymap.h"
#include "UiUtils/Workspaces.h"
#include "UiUtils/SleepMonitor.h"
#include "UiUtils/SearchSuggestions.h"
#include "UiUtils/ServerDiscovery.h"
#include "UiUtils/UnreadFolders.h"
#ifdef IMPRESARIO_HAVE_AKONADI_CALENDAR
#include "UiUtils/AkonadiAgendaSource.h"
#endif

#include <QVBoxLayout>
#include "Imap/Model/AccountCapabilities.h"
#include "OAuth/OAuth.h"
#include "OAuth/Session.h"
#include "AccountSidebar.h"
#include "AddAccountDialog.h"
#include "CapabilityActionGate.h"
#include "SavedSearchFolders.h"
#include "ActivityDialog.h"
#include "OutboxDialog.h"
#include "MailAutomation.h"
#include "MailSafetyPrompt.h"
#include "NotificationsDialog.h"
#include "RulesDialog.h"
#include "UiUtils/SearchQuery.h"
#include "ShellHeader.h"

/** @short All user-facing widgets and related classes */
namespace Gui
{

    static const char * const netErrorUnseen = "net_error_unseen";

namespace {
/** @short The state and settings group of the Unified Inbox */
QString unifiedInboxGroup()
{
    return QStringLiteral("UnifiedInbox");
}

void reselectShownFolder(Imap::ImapAccess *access);
}

MainWindow::MainWindow(QSettings *settings): QMainWindow(), m_imapAccess(nullptr),
    m_mainStack(nullptr), m_layoutMode(LAYOUT_COMPACT), m_skipSavingOfUI(true), m_delayedStateSaving(nullptr), m_actionSortNone(nullptr),
    m_ignoreStoredPassword(false), m_settings(settings), m_pluginManager(nullptr), m_msgActionHandler(this),m_networkErrorMessageBox(nullptr), m_trayIcon(nullptr)
{
    setAttribute(Qt::WA_AlwaysShowToolTips);
    // m_pluginManager must be created before calling createWidgets
    m_pluginManager = new Plugins::PluginManager(this, m_settings,
                                                 Common::SettingsNames::addressbookPlugin, Common::SettingsNames::passwordPlugin,
                                                 Common::SettingsNames::spellcheckerPlugin);
    connect(m_pluginManager, &Plugins::PluginManager::pluginsChanged, this, &MainWindow::slotPluginsChanged);
    m_nextcloud = new NextcloudHub(KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")), m_settings,
                                   [this]() { return m_pluginManager->password(); }, this);
    m_nextcloudTransfers = new NextcloudTransfers(m_nextcloud, this);
    connect(m_pluginManager, &Plugins::PluginManager::pluginError, this, [this](const QString &errorMessage) {
        Gui::Util::messageBoxWarning(this, tr("Plugin Error"),
                                     //: The %1 placeholder is a full error message as provided by Qt, ready for human consumption.
                                     tr("A plugin failed to load, therefore some functionality might be lost. "
                                            "You might want to update your system or report a bug to your vendor."
                                            "\n\n%1").arg(errorMessage));
    });

    connect(m_pluginManager, &Plugins::PluginManager::pluginCreateError, this, [this](const QString &name, const QString &description, const QString &errorMessage) {
        Gui::Util::messageBoxWarning(this, tr("Plugin Error"),
                                     //: The %1 placeholder is the name of the plugin.
                                     //: The %2 placeholder is the human readable description of the plugin.
                                     //: The %3 placeholder is the detailed error message, ready for human consumption.
                                     tr("The plugin \"%1 (%2)\" failed to load, therefore some functionality might be lost. "
                                            "\n\n%3").arg(name, description, errorMessage));

    });

#ifdef IMPRESARIO_HAVE_CRYPTO_MESSAGES
    Plugins::PluginManager::MimePartReplacers replacers;
#ifdef IMPRESARIO_HAVE_GPGMEPP
    replacers.emplace_back(std::make_shared<Cryptography::GpgMeReplacer>());
#endif
    m_pluginManager->setMimePartReplacers(replacers);
#endif

    // ImapAccess contains a wrapper for retrieving passwords through some plugin.
    // That PasswordWatcher is used by the SettingsDialog's widgets *and* by this class,
    // which means that ImapAccess has to be constructed before we go and open the settings dialog.
    setupAccounts();

    ShortcutHandler *shortcutHandler = new ShortcutHandler(this);
    shortcutHandler->setSettingsObject(m_settings);
    defineActions();
    shortcutHandler->readSettings(); // must happen after defineActions()

    // must be created before calling createWidgets
    m_favoriteTags = new Imap::Mailbox::FavoriteTagsModel(this);
    m_favoriteTags->loadFromSettings(*m_settings);

    m_focusLayout = UiUtils::SettingsStore::value(*KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")),
                                                  m_settings, QStringLiteral("Appearance/FocusLayout")).toBool();
    createWidgets();

    Imap::migrateSettings(m_settings);

    m_senderIdentities = new Composer::SenderIdentitiesModel(this);
    m_senderIdentities->loadFromSettings(*m_settings);
    for (const QString &accountId : m_accounts->accountIds())
        Composer::AccountIdentities::migrateAccount(*m_settings, *m_accounts->settings(accountId));

    // POP3 and JMAP accounts have no IMAP method; each protocol knows what it needs
    if (m_imapAccess ? !m_imapAccess->isConfigured()
                     : !accountSettings()->contains(Common::SettingsNames::imapMethodKey)) {
        QTimer::singleShot(0, this, SLOT(slotShowAccountSettings()));
    }


    setupModels();
    connectBackgroundAccounts();
    createActions();
    createMenus();
    applyMessageListView();
    m_activities = new PlasmaActivities(this);
    connect(m_activities, &PlasmaActivities::currentChanged, this, &MainWindow::followActivity);
    watchSharedKeymap();
    applyTabSettings();
    registerAppearance();
    watchFollowedTheme();
    m_vimPending = new QLabel(statusBar());
    m_vimPending->setObjectName(QStringLiteral("vimPending"));
    m_vimPending->setAccessibleName(i18nc("@label", "Vim keys typed"));
    m_vimPending->hide();
    statusBar()->addPermanentWidget(m_vimPending);
    m_vimMode = new VimMode(this, [this](QWidget *focus) { return vimContext(focus); }, this);
    connect(m_vimMode, &VimMode::commandTriggered, this, &MainWindow::runVimCommand);
    connect(m_vimMode, &VimMode::pendingChanged, this, [this](const QString &keys) {
        m_vimPending->setText(keys.isEmpty() ? i18nc("@info:status Vim mode is on", "Vim") : keys);
    });
    applyVimSettings();
    applyProductivitySettings();
    // Choosing a folder leaves the Unified Inbox and the search results for it
    const auto showChosenFolder = [this]() {
        // A folder shows all of its messages, not what the saved search found
        msgListWidget->endSavedSearchEdit();
        const bool savedSearch = !msgListWidget->activeSavedSearchOwner().isEmpty();
        msgListWidget->forgetActiveSavedSearch();
        m_pendingReveal = PendingReveal();
        m_activeReveal = PendingReveal();
        // The results in the Search Results panel stay while other folders are looked at
        if (searchShownInList()) {
            leaveSearchAcross();
            msgListWidget->clearSearch();
        } else if (savedSearch) {
            msgListWidget->clearSearch();
        }
        setUnifiedInbox(false, false);
    };
    connect(mboxTree, &MailBoxTreeView::clicked, this, showChosenFolder);
    connect(mboxTree, &MailBoxTreeView::activated, this, showChosenFolder);
    connect(msgListWidget, &MessageListWidget::foldersChoiceRequested, this, &MainWindow::chooseSearchFolders);
    connect(msgListWidget, &MessageListWidget::contactSuggestionsRequested, this, [this](const QString &query) {
        Plugins::AddressbookPlugin *addressbook = m_pluginManager->addressbook();
        if (!addressbook || !(addressbook->features() & Plugins::AddressbookPlugin::FeatureCompletion))
            return;
        Plugins::AddressbookCompletionJob *job = addressbook->requestCompletion(query, QStringList(), 5);
        if (!job)
            return;
        connect(job, &Plugins::AddressbookCompletionJob::completionAvailable, msgListWidget,
                [this, query](const Plugins::NameEmailList &completion) {
                    QList<UiUtils::SearchSuggestions::Contact> contacts;
                    for (const Plugins::NameEmail &entry : completion)
                        contacts.append({entry.name, entry.email});
                    msgListWidget->showContactSuggestions(query, contacts);
                });
        job->setAutoDelete(true);
        job->start();
    });
    msgListWidget->setFolderSource([this]() {
        // The shown account, whose folders are listed in the folder tree
        for (const QString &accountId : m_accounts->accountIds()) {
            if (m_accountAccess.value(accountId) == m_imapAccess)
                return UiUtils::SearchSuggestions::knownFolders(prettyMboxModel, accountId);
        }
        return QList<UiUtils::SearchSuggestions::FolderCandidate>();
    });
    msgListWidget->setFacetSource([this]() {
        if (m_searchAcross && m_searchInPanel)
            return m_searchResultsPanel->prettyResults()->facetCandidates();
        if (showsUnifiedList())
            return m_prettyUnifiedModel ? m_prettyUnifiedModel->facetCandidates() : QModelIndexList();
        return prettyMsgListModel ? prettyMsgListModel->facetCandidates() : QModelIndexList();
    });
    if (KConfigGroup(KSharedConfig::openStateConfig(), unifiedInboxGroup()).readEntry("Enabled", false))
        m_actionUnifiedInbox->setChecked(true);
    connect(msgListWidget->tree->header(), &QHeaderView::sectionClicked, this, [this]() {
        if (!currentSearchTerms().isEmpty())
            rememberRelevanceSorting(false);
        // Sorting by relevance is never remembered for a folder
        if (m_actionSortByRelevance && m_actionSortByRelevance->isChecked())
            return;
        const QHeaderView *header = msgListWidget->tree->header();
        // The Unified Inbox sorts by itself and has no folder to remember the sorting for
        if (showsUnifiedList()) {
            slotSortingConfirmed(header->sortIndicatorSection(), header->sortIndicatorOrder());
            return;
        }
        rememberMessageListSort(header->sortIndicatorSection(), header->sortIndicatorOrder());
    });
    connect(msgListWidget->tree->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        captureFolderPosition(true);
    });
    slotToggleSysTray();
    slotPluginsChanged();
    keepPrivateFilesTidy();

    slotFavoriteTagsChanged();
    connect(m_favoriteTags, &QAbstractItemModel::modelReset, this, &MainWindow::slotFavoriteTagsChanged);
    connect(m_favoriteTags, &QAbstractItemModel::layoutChanged, this, &MainWindow::slotFavoriteTagsChanged);
    connect(m_favoriteTags, &QAbstractItemModel::rowsMoved, this, &MainWindow::slotFavoriteTagsChanged);
    connect(m_favoriteTags, &QAbstractItemModel::rowsInserted, this, &MainWindow::slotFavoriteTagsChanged);
    connect(m_favoriteTags, &QAbstractItemModel::rowsRemoved, this, &MainWindow::slotFavoriteTagsChanged);
    connect(m_favoriteTags, &QAbstractItemModel::dataChanged, this, &MainWindow::slotFavoriteTagsChanged);

    // Please note that Qt 4.6.1 really requires passing the method signature this way, *not* using the SLOT() macro
    QDesktopServices::setUrlHandler(QStringLiteral("mailto"), this, "slotComposeMailUrl");
    QDesktopServices::setUrlHandler(QStringLiteral("mid"), this, "openMessageUrl");
    QDesktopServices::setUrlHandler(QStringLiteral("x-trojita-manage-contact"), this, "slotManageContact");

    slotUpdateWindowTitle();

    CALL_LATER_NOARG(this, recoverDrafts);

    if (m_actionLayoutWide->isEnabled() &&
            m_settings->value(Common::SettingsNames::guiMainWindowLayout) == Common::SettingsNames::guiMainWindowLayoutWide) {
        m_actionLayoutWide->trigger();
    } else if (m_settings->value(Common::SettingsNames::guiMainWindowLayout) == Common::SettingsNames::guiMainWindowLayoutOneAtTime) {
        m_actionLayoutOneAtTime->trigger();
    } else {
        m_actionLayoutCompact->trigger();
    }

    connect(qApp, &QGuiApplication::applicationStateChanged, this,
            [&](Qt::ApplicationState state) {
                if (state == Qt::ApplicationActive && m_networkErrorMessageBox && m_networkErrorMessageBox->property(netErrorUnseen).toBool()) {
                    m_networkErrorMessageBox->setProperty(netErrorUnseen, false);
                    m_networkErrorMessageBox->show();
                }
            });

    // Don't listen to QDesktopWidget::resized; that is emitted too early (when it gets fired, the screen size has changed, but
    // the workspace area is still the old one). Instead, listen to workAreaResized which gets emitted at an appropriate time.
    // The delay is still there to guarantee some smoothing; on jkt's box there are typically three events in a rapid sequence
    // (some of them most likely due to the fact that at first, the actual desktop gets resized, the plasma panel reacts
    // to that and only after the panel gets resized, the available size of "the rest" is correct again).
    // Which is why it makes sense to introduce some delay in there. The 0.5s delay is my best guess and "should work" (especially
    // because every change bumps the timer anyway, as Thomas pointed out).
    QTimer *delayedResize = new QTimer(this);
    delayedResize->setSingleShot(true);
    delayedResize->setInterval(500);
    connect(delayedResize, &QTimer::timeout, this, &MainWindow::desktopGeometryChanged);
    connect(QGuiApplication::primaryScreen(), &QScreen::availableGeometryChanged, delayedResize, static_cast<void (QTimer::*)()>(&QTimer::start));
    m_skipSavingOfUI = false;
}

MainWindow::~MainWindow()
{
    // The message lists of the accounts go away with the child widgets, after this destructor; the Unified Inbox would
    // report their rows as removed to a window which is no longer one
    delete m_prettyUnifiedModel;
    m_prettyUnifiedModel = nullptr;
    delete m_unifiedModel;
    m_unifiedModel = nullptr;
    // Without restoring the selected folders, as the accounts go away as well
    for (const QPointer<Imap::Mailbox::FolderSearch> &search : std::as_const(m_folderSearches))
        delete search.data();
    m_folderSearches.clear();
    delete m_agendaSearch.data();
}

void MainWindow::reply(const Composer::ReplyMode mode)
{
#ifdef IMPRESARIO_HAVE_WEBENGINE
    const QModelIndex messageIndex = m_messageWidget->message();
    const bool isCryptoMessage = m_messageWidget->isCryptoMessage();
    QPointer<MainWindow> window(this);

    m_messageWidget->quoteText([window, messageIndex, mode, isCryptoMessage](const QString &text) {
        if (!window || !messageIndex.isValid())
            return;

        auto replyNow = [&]() {
            if (isCryptoMessage)
                window->m_msgActionHandler.reply(messageIndex, mode, text, MessageActionHandler::CryptoMessage);
            else
                window->m_msgActionHandler.reply(messageIndex, mode, text, MessageActionHandler::Message);
        };
        // The reply comes from the account which received the message
        if (!window->actOnModel(qobject_cast<const Imap::Mailbox::Model *>(Imap::deproxifiedIndex(messageIndex).model()), replyNow))
            replyNow();
    });
#endif
}

namespace {

/** @short Icon for an action shown at toolbar size, where Plasma prefers the -symbolic variant */
QIcon quickActionIcon(const QString &name)
{
    return UiUtils::SymbolicIcon::fromTheme(name);
}

/** @short Sort indicator stored in a saved QHeaderView state, or (-1, ascending) when there is none */
std::pair<int, Qt::SortOrder> headerStateSort(const QByteArray &state, int columns)
{
    if (state.isEmpty() || columns <= 0)
        return {-1, Qt::AscendingOrder};
    QStandardItemModel model(0, columns);
    QHeaderView probe(Qt::Horizontal);
    probe.setModel(&model);
    if (!probe.restoreState(state))
        return {-1, Qt::AscendingOrder};
    return {probe.sortIndicatorSection(), probe.sortIndicatorOrder()};
}

/** @short Saved QHeaderView @p state with its sort indicator replaced, so that a folder's sort never leaks into the global default */
QByteArray headerStateWithSort(const QByteArray &state, int columns, int column, Qt::SortOrder order)
{
    if (state.isEmpty() || columns <= 0)
        return state;
    QStandardItemModel model(0, columns);
    QHeaderView probe(Qt::Horizontal);
    probe.setModel(&model);
    if (!probe.restoreState(state))
        return state;
    probe.setSortIndicator(column, order);
    return probe.saveState();
}

/** @short Add the row of @p index and the rows shown below it; selectedTree() adds the rest of collapsed threads */
void selectShownRows(const QTreeView *view, const QModelIndex &index, QItemSelection &selection)
{
    QModelIndexList pending{index};
    while (!pending.isEmpty()) {
        const QModelIndex item = pending.takeLast();
        const QAbstractItemModel *model = item.model();
        // The header of a group is not a message, but its messages are shown below it
        if (model->flags(item) & Qt::ItemIsSelectable)
            selection.select(item, item.siblingAtColumn(model->columnCount(item.parent()) - 1));
        if (!view->isExpanded(item))
            continue;
        for (int row = 0, rows = model->rowCount(item); row < rows; ++row)
            pending.append(model->index(row, 0, item));
    }
}

QString goToSpecialFolderText(Imap::Mailbox::SpecialUse kind)
{
    using Imap::Mailbox::SpecialUse;
    switch (kind) {
    case SpecialUse::Inbox:
        return i18nc("@action:inmenu go to folder", "&Inbox");
    case SpecialUse::Drafts:
        return i18nc("@action:inmenu go to folder", "&Drafts");
    case SpecialUse::Sent:
        return i18nc("@action:inmenu go to folder", "&Sent");
    case SpecialUse::Archive:
        return i18nc("@action:inmenu go to folder", "A&rchive");
    case SpecialUse::Junk:
        return i18nc("@action:inmenu go to folder", "&Junk");
    case SpecialUse::Trash:
        return i18nc("@action:inmenu go to folder", "&Trash");
    case SpecialUse::None:
    case SpecialUse::All:
    case SpecialUse::Flagged:
        break;
    }
    return QString();
}

QString goToSpecialFolderObjectName(Imap::Mailbox::SpecialUse kind)
{
    using Imap::Mailbox::SpecialUse;
    switch (kind) {
    case SpecialUse::Inbox:
        return QStringLiteral("action_go_to_inbox");
    case SpecialUse::Drafts:
        return QStringLiteral("action_go_to_drafts");
    case SpecialUse::Sent:
        return QStringLiteral("action_go_to_sent");
    case SpecialUse::Archive:
        return QStringLiteral("action_go_to_archive");
    case SpecialUse::Junk:
        return QStringLiteral("action_go_to_junk");
    case SpecialUse::Trash:
        return QStringLiteral("action_go_to_trash");
    case SpecialUse::None:
    case SpecialUse::All:
    case SpecialUse::Flagged:
        break;
    }
    return QString();
}

}

void MainWindow::defineActions()
{
    ShortcutHandler *shortcutHandler = ShortcutHandler::instance();
    shortcutHandler->defineAction(QStringLiteral("action_application_exit"), QStringLiteral("application-exit"), tr("E&xit"), QKeySequence::Quit);
    shortcutHandler->defineAction(QStringLiteral("action_compose_mail"), QStringLiteral("document-edit"), tr("&New Message..."), QKeySequence::New);
    shortcutHandler->defineAction(QStringLiteral("action_compose_draft"), QStringLiteral("document-open-recent"), tr("&Edit Draft..."));
    shortcutHandler->defineAction(QStringLiteral("action_show_menubar"), QStringLiteral("view-list-text"), tr("Show Main Menu &Bar"), tr("Ctrl+M"));
    shortcutHandler->defineAction(QStringLiteral("action_expunge"), QStringLiteral("trash-empty"), tr("Exp&unge"), tr("Ctrl+E"));
    shortcutHandler->defineAction(QStringLiteral("action_mark_as_read"), QStringLiteral("mail-mark-read"), tr("Mark as &Read"), QStringLiteral("M"));
    shortcutHandler->defineAction(QStringLiteral("action_go_to_next_unread"), QStringLiteral("arrow-right"), tr("&Next Unread Message"), QStringLiteral("N"));
    shortcutHandler->defineAction(QStringLiteral("action_go_to_previous_unread"), QStringLiteral("arrow-left"), tr("&Previous Unread Message"), QStringLiteral("P"));
    shortcutHandler->defineAction(QStringLiteral("action_mark_read_go_to_next_unread"), QStringLiteral("mail-mark-read"),
                                  i18nc("@action", "Mark as Read and Go to Next U&nread"), QStringLiteral("Shift+N"));
    shortcutHandler->defineAction(QStringLiteral("action_go_to_next_message"), QStringLiteral("go-down"),
                                  i18nc("@action", "Ne&xt Message"), QStringLiteral("F"));
    shortcutHandler->defineAction(QStringLiteral("action_go_to_previous_message"), QStringLiteral("go-up"),
                                  i18nc("@action", "Pre&vious Message"), QStringLiteral("B"));
    shortcutHandler->defineAction(QStringLiteral("action_go_to_next_in_conversation"), QStringLiteral("go-down"),
                                  i18nc("@action", "Next Message in &Conversation"), QStringLiteral("Alt+Down"));
    shortcutHandler->defineAction(QStringLiteral("action_go_to_previous_in_conversation"), QStringLiteral("go-up"),
                                  i18nc("@action", "Previous Message in C&onversation"), QStringLiteral("Alt+Up"));
    shortcutHandler->defineAction(QStringLiteral("action_expand_all_conversations"), QStringLiteral("expand-all"),
                                  i18nc("@action", "E&xpand All Conversations"), QStringLiteral("Ctrl+."));
    shortcutHandler->defineAction(QStringLiteral("action_collapse_all_conversations"), QStringLiteral("collapse-all"),
                                  i18nc("@action", "Co&llapse All Conversations"), QStringLiteral("Ctrl+,"));
    shortcutHandler->defineAction(QStringLiteral("action_unified_inbox"), QStringLiteral("mail-folder-inbox"),
                                  i18nc("@action", "&Unified Inbox"), QStringLiteral("Ctrl+Shift+I"));
    shortcutHandler->defineAction(QStringLiteral("action_dense_processing_layout"), QStringLiteral("view-list-details"),
                                  i18nc("@action", "&Dense Processing Layout"));
    shortcutHandler->defineAction(QStringLiteral("action_mark_as_deleted"), QStringLiteral("list-remove"), tr("Mark as &Deleted"), QKeySequence(Qt::Key_Delete).toString());
    shortcutHandler->defineAction(QStringLiteral("action_delete_permanently"), QStringLiteral("edit-delete"),
                                  i18nc("@action", "Delete &Permanently…"), QKeySequence(Qt::SHIFT | Qt::Key_Delete).toString());
    shortcutHandler->defineAction(QStringLiteral("action_mark_as_flagged"), QStringLiteral("mail-flagged"), tr("Mark as &Flagged"), QStringLiteral("S"));
    shortcutHandler->defineAction(QStringLiteral("action_mark_as_junk"), QStringLiteral("mail-mark-junk"), tr("Mark as &Junk"), QStringLiteral("J"));
    shortcutHandler->defineAction(QStringLiteral("action_mark_as_notjunk"), QStringLiteral("mail-mark-notjunk"), tr("Mark as Not &junk"), QStringLiteral("Shift+J"));
    shortcutHandler->defineAction(QStringLiteral("action_save_message_as"), QStringLiteral("document-save"), tr("&Save Message..."));
    shortcutHandler->defineAction(QStringLiteral("action_view_message_source"), QString(), tr("View Message &Source..."));
    shortcutHandler->defineAction(QStringLiteral("action_view_message_headers"), QString(), tr("View Message &Headers..."), tr("Ctrl+U"));
    shortcutHandler->defineAction(QStringLiteral("action_reply_private"), QStringLiteral("mail-reply-sender"), tr("&Private Reply"), tr("Ctrl+Shift+A"));
    shortcutHandler->defineAction(QStringLiteral("action_reply_all_but_me"), QStringLiteral("mail-reply-all"), tr("Reply to All &but Me"), tr("Ctrl+Shift+R"));
    shortcutHandler->defineAction(QStringLiteral("action_reply_all"), QStringLiteral("mail-reply-all"), tr("Reply to &All"), tr("Ctrl+Alt+Shift+R"));
    shortcutHandler->defineAction(QStringLiteral("action_reply_list"), QStringLiteral("mail-reply-list"), tr("Reply to &Mailing List"), tr("Ctrl+L"));
    shortcutHandler->defineAction(QStringLiteral("action_reply_guess"), QString(), tr("Reply by &Guess"), tr("Ctrl+R"));
    shortcutHandler->defineAction(QStringLiteral("action_forward_attachment"), QStringLiteral("mail-forward"), tr("&Forward"), tr("Ctrl+Shift+F"));
    shortcutHandler->defineAction(QStringLiteral("action_resend"), QStringLiteral("mail-resend"), tr("Resend..."));
    shortcutHandler->defineAction(QStringLiteral("action_archive"), QStringLiteral("mail-move-to-archive"), tr("&Archive"), QStringLiteral("A"));
    shortcutHandler->defineAction(QStringLiteral("action_move_to_folder"), QStringLiteral("edit-move"),
                                  i18nc("@action", "&Move to Folder…"), QStringLiteral("V"));
    shortcutHandler->defineAction(QStringLiteral("action_copy_to_folder"), QStringLiteral("edit-copy"),
                                  i18nc("@action", "&Copy to Folder…"), QStringLiteral("Shift+V"));
    shortcutHandler->defineAction(QStringLiteral("action_mark_as_unread"), QStringLiteral("mail-mark-unread"),
                                  i18nc("@action", "Mark as &Unread"), QStringLiteral("Shift+M"));
    shortcutHandler->defineAction(QStringLiteral("action_contact_editor"), QStringLiteral("contact-unknown"), tr("Address Book..."));
    shortcutHandler->defineAction(QStringLiteral("action_network_offline"), QStringLiteral("network-disconnect"), tr("&Offline"));
    shortcutHandler->defineAction(QStringLiteral("action_network_expensive"), QStringLiteral("network-wireless"), tr("&Expensive Connection"));
    shortcutHandler->defineAction(QStringLiteral("action_network_online"), QStringLiteral("network-connect"), tr("&Free Access"));
    shortcutHandler->defineAction(QStringLiteral("action_messagewindow_close"), QStringLiteral("window-close"), tr("Close Standalone Message Window"));
    shortcutHandler->defineAction(QStringLiteral("action_open_messagewindow"), QString(), tr("Open message in New Window..."), QStringLiteral("Ctrl+Return"));
    shortcutHandler->defineAction(QStringLiteral("action_oneattime_go_back"), QStringLiteral("go-previous"), tr("Navigate Back"), QKeySequence(QKeySequence::Back).toString());
    shortcutHandler->defineAction(QStringLiteral("action_zoom_in"), QStringLiteral("zoom-in"), tr("Zoom In"), QKeySequence::ZoomIn);
    shortcutHandler->defineAction(QStringLiteral("action_zoom_out"), QStringLiteral("zoom-out"), tr("Zoom Out"), QKeySequence::ZoomOut);
    shortcutHandler->defineAction(QStringLiteral("action_zoom_original"), QStringLiteral("zoom-original"), tr("Original Size"));
    shortcutHandler->defineAction(QStringLiteral("action_focus_mailbox_tree"), QString(), tr("Move Focus to Mailbox List"));
    shortcutHandler->defineAction(QStringLiteral("action_focus_msg_list"), QString(), tr("Move Focus to Message List"));
    shortcutHandler->defineAction(QStringLiteral("action_focus_message_view"), QString(), tr("Move Focus to Message View"));
    shortcutHandler->defineAction(QStringLiteral("action_focus_next_region"), QString(), tr("Move Focus to Next Pane"), QStringLiteral("F6"));
    shortcutHandler->defineAction(QStringLiteral("action_focus_previous_region"), QString(), tr("Move Focus to Previous Pane"), QStringLiteral("Shift+F6"));
    shortcutHandler->defineAction(QStringLiteral("action_next_account"), QString(), tr("Switch to Next Account"), QStringLiteral("Ctrl+Alt+PgDown"));
    shortcutHandler->defineAction(QStringLiteral("action_previous_account"), QString(), tr("Switch to Previous Account"), QStringLiteral("Ctrl+Alt+PgUp"));
    shortcutHandler->defineAction(QStringLiteral("action_focus_quick_search"), QString(), tr("Move Focus to Quick Search"), QStringLiteral("/"));
    shortcutHandler->defineAction(QStringLiteral("action_find"), QStringLiteral("edit-find"), i18nc("@action", "&Find..."),
                                  QKeySequence::Find);
    shortcutHandler->defineAction(QStringLiteral("action_search_builder"), QStringLiteral("edit-find"),
                                  i18nc("@action", "&Search Builder..."), QStringLiteral("Ctrl+Shift+B"));
    shortcutHandler->defineAction(QStringLiteral("action_clear_search_and_filters"), QStringLiteral("edit-clear-all"),
                                  i18nc("@action", "Clear Search and &Filters"), QStringLiteral("Ctrl+Shift+Backspace"));
    shortcutHandler->defineAction(QStringLiteral("action_go_to_folder"), QStringLiteral("go-jump"), i18nc("@action", "Go to &Folder..."), QStringLiteral("Ctrl+J"));
    shortcutHandler->defineAction(QStringLiteral("action_go_to_next_unread_folder"), QStringLiteral("go-next"),
                                  i18nc("@action", "Go to Next &Unread Folder"), QStringLiteral("Ctrl+Alt+N"));
    shortcutHandler->defineAction(QStringLiteral("action_check_mail"), QStringLiteral("mail-receive"),
                                  i18nc("@action", "Check for &New Mail"), QStringLiteral("F5"));
    shortcutHandler->defineAction(QStringLiteral("action_select_all_messages"), QStringLiteral("edit-select-all"),
                                  i18nc("@action", "Select &All"), QKeySequence(QKeySequence::SelectAll).toString());
    shortcutHandler->defineAction(QStringLiteral("action_select_thread"), QString(), i18nc("@action", "Select &Thread"));
    shortcutHandler->defineAction(QStringLiteral("action_invert_selection"), QStringLiteral("edit-select-invert"),
                                  i18nc("@action", "&Invert Selection"));
    shortcutHandler->defineAction(QStringLiteral("action_deselect_messages"), QStringLiteral("edit-select-none"),
                                  i18nc("@action", "D&eselect"));
    shortcutHandler->defineAction(QStringLiteral("action_show_related_sender"), QStringLiteral("view-filter"),
                                  i18nc("@action", "Messages from This &Sender"));
    shortcutHandler->defineAction(QStringLiteral("action_show_related_recipient"), QStringLiteral("view-filter"),
                                  i18nc("@action", "Messages to This &Recipient"));
    shortcutHandler->defineAction(QStringLiteral("action_show_related_subject"), QStringLiteral("view-filter"),
                                  i18nc("@action", "Messages with This S&ubject"));
    shortcutHandler->defineAction(QStringLiteral("action_show_related_list"), QStringLiteral("view-filter"),
                                  i18nc("@action", "Messages from This &Mailing List"));
    shortcutHandler->defineAction(QStringLiteral("action_select_same_sender"), QString(),
                                  i18nc("@action", "Select All from This &Sender"));
    shortcutHandler->defineAction(QStringLiteral("action_select_same_subject"), QString(),
                                  i18nc("@action", "Select All with This S&ubject"));
    shortcutHandler->defineAction(QStringLiteral("action_select_unread"), QString(), i18nc("@action", "Select &Unread"));
    shortcutHandler->defineAction(QStringLiteral("action_select_flagged"), QString(), i18nc("@action", "Select &Flagged"));
    shortcutHandler->defineAction(QStringLiteral("action_tag_1"), QStringLiteral("mail-tag-1"), tr("Tag with &1st tag"), QStringLiteral("1"));
    shortcutHandler->defineAction(QStringLiteral("action_tag_2"), QStringLiteral("mail-tag-2"), tr("Tag with &2nd tag"), QStringLiteral("2"));
    shortcutHandler->defineAction(QStringLiteral("action_tag_3"), QStringLiteral("mail-tag-3"), tr("Tag with &3rd tag"), QStringLiteral("3"));
    shortcutHandler->defineAction(QStringLiteral("action_tag_4"), QStringLiteral("mail-tag-4"), tr("Tag with &4th tag"), QStringLiteral("4"));
    shortcutHandler->defineAction(QStringLiteral("action_tag_5"), QStringLiteral("mail-tag-5"), tr("Tag with &5th tag"), QStringLiteral("5"));
    shortcutHandler->defineAction(QStringLiteral("action_tag_6"), QStringLiteral("mail-tag-6"), tr("Tag with &6th tag"), QStringLiteral("6"));
    shortcutHandler->defineAction(QStringLiteral("action_tag_7"), QStringLiteral("mail-tag-7"), tr("Tag with &7th tag"), QStringLiteral("7"));
    shortcutHandler->defineAction(QStringLiteral("action_tag_8"), QStringLiteral("mail-tag-8"), tr("Tag with &8th tag"), QStringLiteral("8"));
    shortcutHandler->defineAction(QStringLiteral("action_tag_9"), QStringLiteral("mail-tag-9"), tr("Tag with &9th tag"), QStringLiteral("9"));
    shortcutHandler->defineAction(QStringLiteral("action_shortcut_overview"), QStringLiteral("help-keyboard-shortcuts"),
                                  i18nc("@action", "&Keyboard Shortcuts"), QStringLiteral("?"));
    shortcutHandler->defineAction(QStringLiteral("action_message_to_calendar"), QStringLiteral("appointment-new"),
                                  i18nc("@action", "Add to &Calendar…"));
    shortcutHandler->defineAction(QStringLiteral("action_message_to_note"), QStringLiteral("view-pim-notes"),
                                  i18nc("@action", "Save as &Note"));
    shortcutHandler->defineAction(QStringLiteral("action_new_tab"), QStringLiteral("tab-new"), i18nc("@action", "New &Tab"),
                                  QStringLiteral("Ctrl+T"));
    shortcutHandler->defineAction(QStringLiteral("action_close_tab"), QStringLiteral("tab-close"), i18nc("@action", "&Close Tab"),
                                  QStringLiteral("Ctrl+W"));
    shortcutHandler->defineAction(QStringLiteral("action_next_tab"), QStringLiteral("go-next-view"), i18nc("@action", "Ne&xt Tab"),
                                  QStringLiteral("Ctrl+PgDown"));
    shortcutHandler->defineAction(QStringLiteral("action_previous_tab"), QStringLiteral("go-previous-view"),
                                  i18nc("@action", "Pre&vious Tab"), QStringLiteral("Ctrl+PgUp"));
}

void MainWindow::createActions()
{
    // The shortcuts are a little bit complicated, unfortunately. This is what the other applications use by default:
    //
    // Thunderbird:
    // private: Ctrl+R
    // all: Ctrl+Shift+R
    // list: Ctrl+Shift+L
    // forward: Ctrl+L
    // (no shortcuts for type of forwarding)
    // resend: ctrl+B
    // new message: Ctrl+N
    //
    // KMail:
    // "reply": R
    // private: Shift+A
    // all: A
    // list: L
    // forward as attachment: F
    // forward inline: Shift+F
    // resend: E
    // new: Ctrl+N

    m_actionContactEditor = ShortcutHandler::instance()->createAction(QStringLiteral("action_contact_editor"), this, SLOT(invokeContactEditor()), this);

    m_mainToolbar = addToolBar(tr("Navigation"));
    m_mainToolbar->setObjectName(QStringLiteral("mainToolbar"));

    reloadMboxList = new QAction(style()->standardIcon(QStyle::SP_ArrowRight), tr("&Update List of Child Mailboxes"), this);
    connect(reloadMboxList, &QAction::triggered, this, &MainWindow::slotReloadMboxList);

    resyncMbox = new QAction(UiUtils::loadIcon(QStringLiteral("view-refresh")), tr("Check for &New Messages"), this);
    connect(resyncMbox, &QAction::triggered, this, &MainWindow::slotResyncMbox);

    reloadAllMailboxes = new QAction(tr("&Reload Everything"), this);
    // connect later; POP3 and JMAP accounts visit their server instead, as their folders are on this computer
    for (QAction *check : {resyncMbox, reloadAllMailboxes}) {
        connect(check, &QAction::triggered, this, [this]() {
            if (Pop3Fetcher *fetcher = m_pop3Fetchers.value(m_accounts->activeAccount()))
                fetcher->check(true);
            if (JmapSyncer *syncer = m_jmapSyncers.value(m_accounts->activeAccount()))
                syncer->sync(true);
        });
    }

    exitAction = ShortcutHandler::instance()->createAction(QStringLiteral("action_application_exit"), qApp, SLOT(quit()), this);
    exitAction->setStatusTip(tr("Exit the application"));

    m_actionShortcutOverview = ShortcutHandler::instance()->createAction(QStringLiteral("action_shortcut_overview"), this);
    connect(m_actionShortcutOverview, &QAction::triggered, this, &MainWindow::slotShowShortcutOverview);

    netOffline = ShortcutHandler::instance()->createAction(QStringLiteral("action_network_offline"), this);
    netOffline->setCheckable(true);
    // connect later
    netExpensive = ShortcutHandler::instance()->createAction(QStringLiteral("action_network_expensive"), this);
    netExpensive->setCheckable(true);
    // connect later
    netOnline = ShortcutHandler::instance()->createAction(QStringLiteral("action_network_online"), this);
    netOnline->setCheckable(true);
    // connect later

    QActionGroup *netPolicyGroup = new QActionGroup(this);
    netPolicyGroup->setExclusive(true);
    netPolicyGroup->addAction(netOffline);
    netPolicyGroup->addAction(netExpensive);
    netPolicyGroup->addAction(netOnline);

    //: a debugging tool showing the full contents of the whole IMAP server; all folders, messages and their parts
    showFullView = new QAction(UiUtils::loadIcon(QStringLiteral("edit-find-mail")), tr("Show Full &Tree Window"), this);
    showFullView->setCheckable(true);
    connect(showFullView, &QAction::triggered, allDock, &QWidget::setVisible);
    connect(allDock, &QDockWidget::visibilityChanged, showFullView, &QAction::setChecked);

    //: list of active "tasks", entities which are performing certain action like downloading a message or syncing a mailbox
    showTaskView = new QAction(tr("Show ImapTask t&ree"), this);
    showTaskView->setCheckable(true);
    connect(showTaskView, &QAction::triggered, taskDock, &QWidget::setVisible);
    connect(taskDock, &QDockWidget::visibilityChanged, showTaskView, &QAction::setChecked);

    //: a debugging tool showing the mime tree of the current message
    showMimeView = new QAction(tr("Show &MIME tree"), this);
    showMimeView->setCheckable(true);
    connect(showMimeView, &QAction::triggered, mailMimeDock, &QWidget::setVisible);
    connect(mailMimeDock, &QDockWidget::visibilityChanged, showMimeView, &QAction::setChecked);

    showProtocolLogger = new QAction(tr("Show protocol &log"), this);
    showProtocolLogger->setCheckable(true);
    connect(showProtocolLogger, &QAction::toggled, protocolLoggerDock, &QWidget::setVisible);
    connect(protocolLoggerDock, &QDockWidget::visibilityChanged, showProtocolLogger, &QAction::setChecked);

    //: file to save the debug log into
    logPersistent = new QAction(tr("Log &into %1").arg(Imap::Mailbox::persistentLogFileName()), this);
    logPersistent->setCheckable(true);
    connect(logPersistent, &QAction::triggered, protocolLogger, &ProtocolLoggerWidget::slotSetPersistentLogging);
    connect(protocolLogger, &ProtocolLoggerWidget::persistentLoggingChanged, logPersistent, &QAction::setChecked);

    showImapCapabilities = new QAction(tr("IMAP Server In&formation..."), this);
    connect(showImapCapabilities, &QAction::triggered, this, &MainWindow::slotShowImapInfo);

    showMenuBar = ShortcutHandler::instance()->createAction(QStringLiteral("action_show_menubar"), this);
    showMenuBar->setCheckable(true);
    showMenuBar->setChecked(true);
    connect(showMenuBar, &QAction::triggered, menuBar(), &QMenuBar::setVisible);
    connect(showMenuBar, &QAction::triggered, m_delayedStateSaving, static_cast<void (QTimer::*)()>(&QTimer::start));

    showToolBar = new QAction(tr("Show &Toolbar"), this);
    showToolBar->setCheckable(true);
    connect(showToolBar, &QAction::triggered, m_mainToolbar, &QWidget::setVisible);
    connect(m_mainToolbar, &QToolBar::visibilityChanged, showToolBar, &QAction::setChecked);
    connect(m_mainToolbar, &QToolBar::visibilityChanged, m_delayedStateSaving, static_cast<void (QTimer::*)()>(&QTimer::start));

    configSettings = new QAction(UiUtils::loadIcon(QStringLiteral("configure")),  tr("&Settings..."), this);
    configSettings->setObjectName(QStringLiteral("action_settings"));
    connect(configSettings, &QAction::triggered, this, &MainWindow::slotShowSettings);
    m_actionAddAccount = new QAction(UiUtils::loadIcon(QStringLiteral("list-add")), i18n("&Add Account..."), this);
    m_actionAddAccount->setObjectName(QStringLiteral("action_add_account"));
    connect(m_actionAddAccount, &QAction::triggered, this, &MainWindow::slotAddAccount);
    m_actionAccountSettings = new QAction(UiUtils::loadIcon(QStringLiteral("configure")), i18n("A&ccount Settings..."), this);
    m_actionAccountSettings->setObjectName(QStringLiteral("action_account_settings"));
    connect(m_actionAccountSettings, &QAction::triggered, this, &MainWindow::slotShowAccountSettings);
    m_actionSearchIndex = new QAction(UiUtils::loadIcon(QStringLiteral("edit-find")), i18nc("@action", "Offline Search &Index…"), this);
    m_actionSearchIndex->setObjectName(QStringLiteral("action_search_index_status"));
    m_actionSearchIndex->setToolTip(i18nc("@info:tooltip", "Show how much of the account's mail can be searched offline"));
    connect(m_actionSearchIndex, &QAction::triggered, this, &MainWindow::slotShowSearchIndex);
    m_actionMailRules = new QAction(UiUtils::loadIcon(QStringLiteral("view-filter")), i18nc("@action", "Mail &Rules…"), this);
    m_actionMailRules->setObjectName(QStringLiteral("action_mail_rules"));
    connect(m_actionMailRules, &QAction::triggered, this, [this]() { showMailRules(); });
    m_actionActivity = new QAction(UiUtils::loadIcon(QStringLiteral("view-history")), i18nc("@action", "&Activity…"), this);
    m_actionActivity->setObjectName(QStringLiteral("action_activity"));
    connect(m_actionActivity, &QAction::triggered, this, &MainWindow::showActivity);
    m_actionConfigureNotifications = new QAction(UiUtils::loadIcon(QStringLiteral("preferences-desktop-notification")),
                                                 i18nc("@action", "Configure &Notifications…"), this);
    m_actionConfigureNotifications->setObjectName(QStringLiteral("action_configure_notifications"));
    connect(m_actionConfigureNotifications, &QAction::triggered, this, &MainWindow::configureNotifications);
    m_actionRuleFromSearch = new QAction(UiUtils::loadIcon(QStringLiteral("view-filter")),
                                         i18nc("@action", "Create &Rule from Search…"), this);
    m_actionRuleFromSearch->setObjectName(QStringLiteral("action_rule_from_search"));
    m_actionRuleFromSearch->setToolTip(i18nc("@info:tooltip", "Make a mail rule which matches what the quick search finds"));
    connect(m_actionRuleFromSearch, &QAction::triggered, this, &MainWindow::createRuleFromSearch);
    m_actionNotifyFolder = new QAction(i18nc("@action folder", "&Notify About New Mail"), this);
    m_actionNotifyFolder->setObjectName(QStringLiteral("action_notify_folder"));
    m_actionNotifyFolder->setCheckable(true);
    connect(m_actionNotifyFolder, &QAction::triggered, this, [this](bool checked) {
        const QString folder = m_actionNotifyFolder->data().toString();
        if (folder.isEmpty())
            return;
        UiUtils::NewMail::Settings settings = m_mailAutomation->notificationSettings();
        settings.setAnnounces(m_accounts->activeAccount(), folder, checked);
        m_mailAutomation->setNotificationSettings(settings);
    });
    m_actionRenameAccount = new QAction(i18n("Re&name Account..."), this);
    connect(m_actionRenameAccount, &QAction::triggered, this, &MainWindow::slotRenameAccount);
    m_actionRemoveAccount = new QAction(UiUtils::loadIcon(QStringLiteral("list-remove")), i18n("&Remove Account..."), this);
    connect(m_actionRemoveAccount, &QAction::triggered, this, &MainWindow::slotRemoveAccount);

    QAction *triggerSearch = new QAction(this);
    addAction(triggerSearch);
    triggerSearch->setShortcut(QKeySequence(QStringLiteral(":, =")));
    connect(triggerSearch, &QAction::triggered, msgListWidget, &MessageListWidget::focusRawSearch);

    addAction(ShortcutHandler::instance()->createAction(QStringLiteral("action_focus_quick_search"),
            msgListWidget, SLOT(focusSearch()), this));
    m_actionFind = ShortcutHandler::instance()->createAction(QStringLiteral("action_find"), this, SLOT(slotFind()), this);
    m_actionFind->setToolTip(i18nc("@info:tooltip", "Find text in the open message while the reader has focus, or else search messages"));
    m_searchBuilder = ShortcutHandler::instance()->createAction(QStringLiteral("action_search_builder"), msgListWidget,
            SLOT(openSearchBuilder()), this);
    m_actionFindSavedFiles = new QAction(UiUtils::loadIcon(QStringLiteral("system-search")),
                                         i18nc("@action", "Find Saved &Files…"), this);
    m_actionFindSavedFiles->setObjectName(QStringLiteral("action_find_saved_files"));
    m_actionFindSavedFiles->setToolTip(i18nc("@info:tooltip", "Find attachments and messages which you saved, in your file manager"));
    connect(m_actionFindSavedFiles, &QAction::triggered, this, [this]() {
        findSavedFiles(msgListWidget->searchField()->text());
    });
    m_actionBrowseNextcloud = new QAction(UiUtils::loadIcon(QStringLiteral("folder-cloud")),
                                          i18nc("@action", "Browse &Nextcloud…"), this);
    m_actionBrowseNextcloud->setObjectName(QStringLiteral("action_browse_nextcloud"));
    m_actionBrowseNextcloud->setToolTip(i18nc("@info:tooltip", "Browse the files of your Nextcloud accounts, or connect one"));
    connect(m_actionBrowseNextcloud, &QAction::triggered, this, &MainWindow::browseNextcloud);
    m_clearSearchAndFilters = ShortcutHandler::instance()->createAction(QStringLiteral("action_clear_search_and_filters"), this,
            SLOT(clearSearchAndFilters()), this);

    addAction(ShortcutHandler::instance()->createAction(QStringLiteral("action_focus_mailbox_tree"), mboxTree,
            SLOT(setFocus()), this));
    addAction(ShortcutHandler::instance()->createAction(QStringLiteral("action_focus_msg_list"), msgListWidget->tree,
            SLOT(setFocus()), this));
    addAction(ShortcutHandler::instance()->createAction(QStringLiteral("action_focus_message_view"), this,
            SLOT(slotFocusMessageView()), this));
    addAction(ShortcutHandler::instance()->createAction(QStringLiteral("action_focus_next_region"), this,
            SLOT(slotFocusNextRegion()), this));
    addAction(ShortcutHandler::instance()->createAction(QStringLiteral("action_focus_previous_region"), this,
            SLOT(slotFocusPreviousRegion()), this));
    addAction(ShortcutHandler::instance()->createAction(QStringLiteral("action_next_account"), this,
            SLOT(slotNextAccount()), this));
    addAction(ShortcutHandler::instance()->createAction(QStringLiteral("action_previous_account"), this,
            SLOT(slotPreviousAccount()), this));

    m_goToFolder = ShortcutHandler::instance()->createAction(QStringLiteral("action_go_to_folder"), this,
            SLOT(slotGoToFolder()), this);
    m_goToNextUnreadFolder = ShortcutHandler::instance()->createAction(QStringLiteral("action_go_to_next_unread_folder"),
            this, SLOT(slotGoToNextUnreadFolder()), this);
    m_checkMail = ShortcutHandler::instance()->createAction(QStringLiteral("action_check_mail"), this,
            SLOT(slotCheckMail()), this);
    m_goToFolderMenu = new QMenu(i18nc("@title:menu", "&Go to Folder"), this);
    m_goToFolderMenu->setObjectName(QStringLiteral("goToFolderMenu"));
    m_goToFolderMenu->setIcon(UiUtils::loadIcon(QStringLiteral("go-jump")));
    for (const Imap::Mailbox::SpecialUse kind : Imap::Mailbox::SpecialFolder::navigableKinds()) {
        QAction *action = m_goToFolderMenu->addAction(Imap::Mailbox::PrettyMailboxModel::specialFolderIcon(kind),
                                                      goToSpecialFolderText(kind));
        action->setObjectName(goToSpecialFolderObjectName(kind));
        action->setEnabled(false);
        connect(action, &QAction::triggered, this, [this, kind]() { goToSpecialFolder(kind); });
        m_goToSpecialFolderActions.insert(kind, action);
    }

    m_oneAtTimeGoBack = ShortcutHandler::instance()->createAction(QStringLiteral("action_oneattime_go_back"), this);
    m_oneAtTimeGoBack->setEnabled(false);

    composeMail = ShortcutHandler::instance()->createAction(QStringLiteral("action_compose_mail"), this, SLOT(slotComposeMail()), this);
    m_editDraft = ShortcutHandler::instance()->createAction(QStringLiteral("action_compose_draft"), this, SLOT(slotEditDraft()), this);

    expunge = ShortcutHandler::instance()->createAction(QStringLiteral("action_expunge"), this, SLOT(slotExpunge()), this);

    m_forwardAsAttachment = ShortcutHandler::instance()->createAction(QStringLiteral("action_forward_attachment"), this, SLOT(slotForwardAsAttachment()), this);
    m_resend = ShortcutHandler::instance()->createAction(QStringLiteral("action_resend"), this, SLOT(slotResend()), this);
    markAsRead = ShortcutHandler::instance()->createAction(QStringLiteral("action_mark_as_read"), this);
    markAsRead->setCheckable(true);
    msgListWidget->tree->addAction(markAsRead);
    connect(markAsRead, &QAction::triggered, this, &MainWindow::handleMarkAsRead);

#ifdef IMPRESARIO_HAVE_WEBENGINE
    m_nextMessage = ShortcutHandler::instance()->createAction(QStringLiteral("action_go_to_next_unread"), this, SLOT(slotNextUnread()), this);
    msgListWidget->tree->addAction(m_nextMessage);
    m_messageWidget->messageView->addAction(m_nextMessage);

    m_previousMessage = ShortcutHandler::instance()->createAction(QStringLiteral("action_go_to_previous_unread"), this, SLOT(slotPreviousUnread()), this);
    msgListWidget->tree->addAction(m_previousMessage);
    m_messageWidget->messageView->addAction(m_previousMessage);

    m_markReadNextUnread = ShortcutHandler::instance()->createAction(QStringLiteral("action_mark_read_go_to_next_unread"), this,
                                                                     SLOT(slotMarkReadAndNextUnread()), this);
    m_nextAnyMessage = ShortcutHandler::instance()->createAction(QStringLiteral("action_go_to_next_message"), this,
                                                                 SLOT(slotNextMessage()), this);
    m_previousAnyMessage = ShortcutHandler::instance()->createAction(QStringLiteral("action_go_to_previous_message"), this,
                                                                     SLOT(slotPreviousMessage()), this);
    for (QAction *action : {m_markReadNextUnread, m_nextAnyMessage, m_previousAnyMessage}) {
        msgListWidget->tree->addAction(action);
        m_messageWidget->messageView->addAction(action);
    }
#endif
    m_nextInConversation = ShortcutHandler::instance()->createAction(QStringLiteral("action_go_to_next_in_conversation"), this);
    connect(m_nextInConversation, &QAction::triggered, this, [this]() { goToMessageInConversation(true); });
    m_previousInConversation = ShortcutHandler::instance()->createAction(QStringLiteral("action_go_to_previous_in_conversation"), this);
    connect(m_previousInConversation, &QAction::triggered, this, [this]() { goToMessageInConversation(false); });
    m_expandAllConversations = ShortcutHandler::instance()->createAction(QStringLiteral("action_expand_all_conversations"), this);
    m_expandAllConversations->setEnabled(false);
    connect(m_expandAllConversations, &QAction::triggered, this, [this]() {
        if (showsThreads())
            msgListWidget->tree->expandAll();
    });
    m_collapseAllConversations = ShortcutHandler::instance()->createAction(QStringLiteral("action_collapse_all_conversations"), this);
    m_collapseAllConversations->setEnabled(false);
    connect(m_collapseAllConversations, &QAction::triggered, this, [this]() {
        if (showsThreads())
            msgListWidget->tree->collapseAll();
    });
    for (QAction *action : {m_nextInConversation, m_previousInConversation, m_expandAllConversations, m_collapseAllConversations})
        msgListWidget->tree->addAction(action);
#ifdef IMPRESARIO_HAVE_WEBENGINE
    m_messageWidget->messageView->addAction(m_nextInConversation);
    m_messageWidget->messageView->addAction(m_previousInConversation);
#endif

    markAsDeleted = ShortcutHandler::instance()->createAction(QStringLiteral("action_mark_as_deleted"), this);
    markAsDeleted->setCheckable(true);
    msgListWidget->tree->addAction(markAsDeleted);
    connect(markAsDeleted, &QAction::triggered, this, &MainWindow::handleMarkAsDeleted);
    m_markAsDeletedText = markAsDeleted->text();
    m_markAsDeletedIcon = quickActionIcon(QStringLiteral("list-remove"));
    markAsDeleted->setIcon(m_markAsDeletedIcon);

    m_deletePermanently = ShortcutHandler::instance()->createAction(QStringLiteral("action_delete_permanently"), this);
    m_deletePermanently->setEnabled(false);
    msgListWidget->tree->addAction(m_deletePermanently);
    connect(m_deletePermanently, &QAction::triggered, this, &MainWindow::handleDeletePermanently);

    m_deleteMovesToTrash = new QAction(i18nc("@option:check", "Move Deleted Messages to Trash"), this);
    m_deleteMovesToTrash->setObjectName(QStringLiteral("deleteMovesToTrash"));
    m_deleteMovesToTrash->setCheckable(true);
    m_deleteMovesToTrash->setChecked(deleteMovesToTrash());
    connect(m_deleteMovesToTrash, &QAction::toggled, this, [this](const bool enabled) {
        accountSettings()->setValue(Common::SettingsNames::imapDeleteMovesToTrash, enabled);
        updateDeleteActionMode();
    });

    markAsFlagged = ShortcutHandler::instance()->createAction(QStringLiteral("action_mark_as_flagged"), this);
    markAsFlagged->setCheckable(true);
    msgListWidget->tree->addAction(markAsFlagged);
    connect(markAsFlagged, &QAction::triggered, this, &MainWindow::handleMarkAsFlagged);

    markAsJunk = ShortcutHandler::instance()->createAction(QStringLiteral("action_mark_as_junk"), this);
    markAsJunk->setCheckable(true);
    msgListWidget->tree->addAction(markAsJunk);
    connect(markAsJunk, &QAction::triggered, this, &MainWindow::handleMarkAsJunk);

    markAsNotJunk = ShortcutHandler::instance()->createAction(QStringLiteral("action_mark_as_notjunk"), this);
    markAsNotJunk->setCheckable(true);
    msgListWidget->tree->addAction(markAsNotJunk);
    connect(markAsNotJunk, &QAction::triggered, this, &MainWindow::handleMarkAsNotJunk);

    saveWholeMessage = ShortcutHandler::instance()->createAction(QStringLiteral("action_save_message_as"), this, SLOT(slotSaveCurrentMessageBody()), this);
    msgListWidget->tree->addAction(saveWholeMessage);

    // Offered once turned on and usable; see applyProductivitySettings()
    m_actionMessageToCalendar = ShortcutHandler::instance()->createAction(QStringLiteral("action_message_to_calendar"), this);
    m_actionMessageToCalendar->setToolTip(i18nc("@info:tooltip", "Open an event about this message in the calendar "
                                                                 "application, to review before it is saved"));
    connect(m_actionMessageToCalendar, &QAction::triggered, this, &MainWindow::messageToCalendar);
    m_actionMessageToNote = ShortcutHandler::instance()->createAction(QStringLiteral("action_message_to_note"), this);
    m_actionMessageToNote->setToolTip(i18nc("@info:tooltip", "Save this message as a Markdown note in the notes folder"));
    connect(m_actionMessageToNote, &QAction::triggered, this, &MainWindow::messageToNote);
    for (QAction *action : {m_actionMessageToCalendar, m_actionMessageToNote}) {
        action->setVisible(false);
        action->setEnabled(false);
    }

    viewMsgSource = ShortcutHandler::instance()->createAction(QStringLiteral("action_view_message_source"), this, SLOT(slotViewMsgSource()), this);
    msgListWidget->tree->addAction(viewMsgSource);

    viewMsgHeaders = ShortcutHandler::instance()->createAction(QStringLiteral("action_view_message_headers"), this, SLOT(slotViewMsgHeaders()), this);
    msgListWidget->tree->addAction(viewMsgHeaders);

    msgListWidget->tree->addAction(ShortcutHandler::instance()->createAction(QStringLiteral("action_open_messagewindow"), this,
            SLOT(openCompleteMessageWidget()), this));

    moveToArchive = ShortcutHandler::instance()->createAction(QStringLiteral("action_archive"), this);
    msgListWidget->tree->addAction(moveToArchive);
    connect(moveToArchive, &QAction::triggered, this, &MainWindow::handleMoveToArchive);

    m_markAsUnread = ShortcutHandler::instance()->createAction(QStringLiteral("action_mark_as_unread"), this);
    msgListWidget->tree->addAction(m_markAsUnread);
    connect(m_markAsUnread, &QAction::triggered, this, &MainWindow::handleMarkAsUnread);

    m_moveToFolder = ShortcutHandler::instance()->createAction(QStringLiteral("action_move_to_folder"), this);
    msgListWidget->tree->addAction(m_moveToFolder);
    connect(m_moveToFolder, &QAction::triggered, this, &MainWindow::slotMoveToFolder);

    m_copyToFolder = ShortcutHandler::instance()->createAction(QStringLiteral("action_copy_to_folder"), this);
    msgListWidget->tree->addAction(m_copyToFolder);
    connect(m_copyToFolder, &QAction::triggered, this, &MainWindow::slotCopyToFolder);

    m_moveToMenu = new QMenu(i18nc("@title:menu", "Move &To"), this);
    m_moveToMenu->setObjectName(QStringLiteral("moveToMenu"));
    m_moveToMenu->setIcon(quickActionIcon(QStringLiteral("edit-move")));
    connect(m_moveToMenu, &QMenu::aboutToShow, this, [this]() {
        populateRecentFoldersMenu(m_moveToMenu, Imap::Mailbox::MOVE);
    });
    m_copyToMenu = new QMenu(i18nc("@title:menu", "Cop&y To"), this);
    m_copyToMenu->setObjectName(QStringLiteral("copyToMenu"));
    m_copyToMenu->setIcon(quickActionIcon(QStringLiteral("edit-copy")));
    connect(m_copyToMenu, &QMenu::aboutToShow, this, [this]() {
        populateRecentFoldersMenu(m_copyToMenu, Imap::Mailbox::COPY);
    });
    // Never leave a menu empty, a tool button would not open it
    populateRecentFoldersMenu(m_moveToMenu, Imap::Mailbox::MOVE);
    populateRecentFoldersMenu(m_copyToMenu, Imap::Mailbox::COPY);

    // These are on the toolbar, where the icons are small enough for the monochrome variants
    markAsRead->setIcon(quickActionIcon(QStringLiteral("mail-mark-read")));
    m_markAsUnread->setIcon(quickActionIcon(QStringLiteral("mail-mark-unread")));
    markAsFlagged->setIcon(quickActionIcon(QStringLiteral("mail-flagged")));
    moveToArchive->setIcon(quickActionIcon(QStringLiteral("mail-move-to-archive")));
    m_moveToFolder->setIcon(quickActionIcon(QStringLiteral("edit-move")));
    m_copyToFolder->setIcon(quickActionIcon(QStringLiteral("edit-copy")));

    auto addTagAction = [this](int row) {
        QAction *tag = ShortcutHandler::instance()->createAction(QStringLiteral("action_tag_").append(QString::number(row)), this);
        tag->setCheckable(true);
        msgListWidget->tree->addAction(tag);
        connect(tag, &QAction::triggered, this, [=, this](const bool checked) {
            handleTag(checked, row - 1);
        });
        return tag;
    };
    tag1 = addTagAction(1);
    tag2 = addTagAction(2);
    tag3 = addTagAction(3);
    tag4 = addTagAction(4);
    tag5 = addTagAction(5);
    tag6 = addTagAction(6);
    tag7 = addTagAction(7);
    tag8 = addTagAction(8);
    tag9 = addTagAction(9);

    //: "mailbox" as a "folder of messages", not as a "mail account"
    createChildMailbox = new QAction(tr("Create &Child Mailbox..."), this);
    connect(createChildMailbox, &QAction::triggered, this, &MainWindow::slotCreateMailboxBelowCurrent);

    //: "mailbox" as a "folder of messages", not as a "mail account"
    createTopMailbox = new QAction(tr("Create &New Mailbox..."), this);
    connect(createTopMailbox, &QAction::triggered, this, &MainWindow::slotCreateTopMailbox);

    m_actionMarkMailboxAsRead = new QAction(tr("&Mark Mailbox as Read"), this);
    connect(m_actionMarkMailboxAsRead, &QAction::triggered, this, &MainWindow::slotMarkCurrentMailboxRead);

    m_actionDownloadSearchText = new QAction(UiUtils::loadIcon(QStringLiteral("edit-download")),
                                             i18nc("@action", "Download Message Text for Offline Search…"), this);
    m_actionDownloadSearchText->setObjectName(QStringLiteral("action_download_search_text"));
    connect(m_actionDownloadSearchText, &QAction::triggered, this, &MainWindow::slotDownloadTextForSearch);

    //: "mailbox" as a "folder of messages", not as a "mail account"
    deleteCurrentMailbox = new QAction(tr("&Remove Mailbox"), this);
    connect(deleteCurrentMailbox, &QAction::triggered, this, &MainWindow::slotDeleteCurrentMailbox);

    m_renameMailbox = new QAction(UiUtils::loadIcon(QStringLiteral("edit-rename")), i18nc("@action", "Re&name Folder…"), this);
    m_renameMailbox->setObjectName(QStringLiteral("renameMailbox"));
    m_renameMailbox->setEnabled(false);
    connect(m_renameMailbox, &QAction::triggered, this, &MainWindow::slotRenameCurrentMailbox);

    m_emptyTrash = new QAction(UiUtils::loadIcon(QStringLiteral("trash-empty")), i18nc("@action", "Empty &Trash…"), this);
    m_emptyTrash->setObjectName(QStringLiteral("emptyTrash"));
    connect(m_emptyTrash, &QAction::triggered, this, [this]() { emptySpecialFolder(Imap::Mailbox::SpecialUse::Trash); });

    m_emptyJunk = new QAction(UiUtils::loadIcon(QStringLiteral("trash-empty")), i18nc("@action", "Empty &Junk…"), this);
    m_emptyJunk->setObjectName(QStringLiteral("emptyJunk"));
    connect(m_emptyJunk, &QAction::triggered, this, [this]() { emptySpecialFolder(Imap::Mailbox::SpecialUse::Junk); });
    // The models were set up before the actions existed
    updateGoToFolderActions();

    m_replyPrivate = ShortcutHandler::instance()->createAction(QStringLiteral("action_reply_private"), this, SLOT(slotReplyTo()), this);
    m_replyPrivate->setEnabled(false);

    m_replyAllButMe = ShortcutHandler::instance()->createAction(QStringLiteral("action_reply_all_but_me"), this, SLOT(slotReplyAllButMe()), this);
    m_replyAllButMe->setEnabled(false);

    m_replyAll = ShortcutHandler::instance()->createAction(QStringLiteral("action_reply_all"), this, SLOT(slotReplyAll()), this);
    m_replyAll->setEnabled(false);

    m_replyList = ShortcutHandler::instance()->createAction(QStringLiteral("action_reply_list"), this, SLOT(slotReplyList()), this);
    m_replyList->setEnabled(false);

    m_replyGuess = ShortcutHandler::instance()->createAction(QStringLiteral("action_reply_guess"), this, SLOT(slotReplyGuess()), this);
    m_replyGuess->setEnabled(true);

    actionThreadMsgList = new QAction(UiUtils::loadIcon(QStringLiteral("format-justify-right")), tr("Show Messages in &Threads"), this);
    actionThreadMsgList->setCheckable(true);
    // This action is enabled/disabled by model's capabilities
    actionThreadMsgList->setEnabled(false);
    if (m_settings->value(Common::SettingsNames::guiMsgListShowThreading).toBool()) {
        actionThreadMsgList->setChecked(true);
        // The actual threading will be performed only when model updates its capabilities
    }
    connect(actionThreadMsgList, &QAction::triggered, this, [this](bool checked) {
        using Imap::Mailbox::GroupingMsgListModel;
        // Threads are the Conversation grouping, which replaces the other groupings
        if (GroupingMsgListModel::isRealGrouping(m_groupBy)) {
            setMessageListGrouping(checked ? GroupingMsgListModel::Grouping::Conversation : GroupingMsgListModel::Grouping::None);
            return;
        }
        slotThreadMsgList();
        m_groupBy = checked ? GroupingMsgListModel::Grouping::Conversation : GroupingMsgListModel::Grouping::None;
        syncGroupByActions();
        rememberMessageListView();
    });

    QActionGroup *sortOrderGroup = new QActionGroup(this);
    m_actionSortAscending = new QAction(tr("&Ascending"), sortOrderGroup);
    m_actionSortAscending->setCheckable(true);
    m_actionSortAscending->setChecked(true);
    m_actionSortDescending = new QAction(tr("&Descending"), sortOrderGroup);
    m_actionSortDescending->setCheckable(true);
    // QActionGroup has no toggle signal, but connecting descending will implicitly catch the acscending complement ;-)
    connect(m_actionSortDescending, &QAction::toggled, m_delayedStateSaving, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(sortOrderGroup, &QActionGroup::triggered, this, &MainWindow::slotSortingPreferenceChanged);

    QActionGroup *sortColumnGroup = new QActionGroup(this);
    m_actionSortNone = new QAction(tr("&No sorting"), sortColumnGroup);
    m_actionSortNone->setCheckable(true);
    m_actionSortThreading = new QAction(tr("Sorted by &Threading"), sortColumnGroup);
    m_actionSortThreading->setCheckable(true);
    m_actionSortByArrival = new QAction(tr("A&rrival"), sortColumnGroup);
    m_actionSortByArrival->setCheckable(true);
    m_actionSortByCc = new QAction(tr("&Cc (Carbon Copy)"), sortColumnGroup);
    m_actionSortByCc->setCheckable(true);
    m_actionSortByDate = new QAction(tr("Date from &Message Headers"), sortColumnGroup);
    m_actionSortByDate->setCheckable(true);
    m_actionSortByFrom = new QAction(tr("&From Address"), sortColumnGroup);
    m_actionSortByFrom->setCheckable(true);
    m_actionSortBySize = new QAction(tr("&Size"), sortColumnGroup);
    m_actionSortBySize->setCheckable(true);
    m_actionSortBySubject = new QAction(tr("S&ubject"), sortColumnGroup);
    m_actionSortBySubject->setCheckable(true);
    m_actionSortByTo = new QAction(tr("T&o Address"), sortColumnGroup);
    m_actionSortByTo->setCheckable(true);
    m_actionSortByRelevance = new QAction(i18nc("@action:inmenu sort criterion", "R&elevance"), sortColumnGroup);
    m_actionSortByRelevance->setObjectName(QStringLiteral("messageListSortByRelevance"));
    m_actionSortByRelevance->setCheckable(true);
    // Only a search tells what is relevant, see updateRelevanceSorting()
    m_actionSortByRelevance->setEnabled(false);
    m_actionSortByRelevance->setToolTip(
        i18nc("@info:tooltip", "Relevance sorting needs an active quick search or Show Related filter"));
    // The next search sorts as the last one was sorted by choice
    connect(sortColumnGroup, &QActionGroup::triggered, this, [this](QAction *action) {
        if (!currentSearchTerms().isEmpty())
            rememberRelevanceSorting(action == m_actionSortByRelevance);
    });
    connect(sortColumnGroup, &QActionGroup::triggered, this, &MainWindow::slotSortingPreferenceChanged);
    slotSortingConfirmed(-1, Qt::AscendingOrder);

    actionHideRead = new QAction(tr("&Hide Read Messages"), this);
    actionHideRead->setObjectName(QStringLiteral("hideReadMessages"));
    actionHideRead->setCheckable(true);
    // The same as the Unread quick filter; its state comes from the account, see applyMessageListFilters()
    connect(actionHideRead, &QAction::triggered, this, &MainWindow::slotHideRead);

    {
        using Imap::Mailbox::GroupingMsgListModel;
        m_groupByActions = new QActionGroup(this);
        m_groupByActions->setExclusive(true);
        auto addGroupBy = [this](GroupingMsgListModel::Grouping grouping, const QString &name, const QString &text) {
            auto *action = new QAction(text, m_groupByActions);
            action->setObjectName(QLatin1String("messageListGroupBy") + name);
            action->setCheckable(true);
            action->setData(static_cast<int>(grouping));
            connect(action, &QAction::triggered, this, [this, grouping]() { setMessageListGrouping(grouping); });
        };
        addGroupBy(GroupingMsgListModel::Grouping::None, QStringLiteral("None"), i18nc("@action:inmenu group by", "&None"));
        addGroupBy(GroupingMsgListModel::Grouping::Conversation, QStringLiteral("Conversation"),
                   i18nc("@action:inmenu group by", "&Conversation"));
        addGroupBy(GroupingMsgListModel::Grouping::Date, QStringLiteral("Date"), i18nc("@action:inmenu group by", "&Date"));
        addGroupBy(GroupingMsgListModel::Grouping::Sender, QStringLiteral("Sender"), i18nc("@action:inmenu group by", "&Sender"));
        addGroupBy(GroupingMsgListModel::Grouping::Subject, QStringLiteral("Subject"), i18nc("@action:inmenu group by", "S&ubject"));
        addGroupBy(GroupingMsgListModel::Grouping::Tag, QStringLiteral("Tag"), i18nc("@action:inmenu group by", "&Tag"));
        addGroupBy(GroupingMsgListModel::Grouping::ReadState, QStringLiteral("ReadState"),
                   i18nc("@action:inmenu group by", "&Read State"));
        addGroupBy(GroupingMsgListModel::Grouping::MailingList, QStringLiteral("MailingList"),
                   i18nc("@action:inmenu group by", "&Mailing List"));
        // The Conversation grouping needs threading support on the server
        connect(actionThreadMsgList, &QAction::changed, this, &MainWindow::syncGroupByActions);

        m_actionViewForAllFolders = new QAction(i18nc("@action:inmenu", "Use This View for A&ll Folders"), this);
        m_actionViewForAllFolders->setObjectName(QStringLiteral("messageListViewForAllFolders"));
        m_actionViewForAllFolders->setToolTip(
            i18nc("@info:tooltip", "Sort, thread and group all folders of this account like the current one"));
        m_actionViewForAllFolders->setEnabled(false);
        connect(m_actionViewForAllFolders, &QAction::triggered, this, &MainWindow::applyMessageListViewToAllFolders);
        m_actionResetFolderView = new QAction(i18nc("@action:inmenu", "Reset &View for This Folder"), this);
        m_actionResetFolderView->setObjectName(QStringLiteral("messageListResetFolderView"));
        m_actionResetFolderView->setToolTip(
            i18nc("@info:tooltip", "Sort, thread and group this folder like the other folders of the account"));
        m_actionResetFolderView->setEnabled(false);
        connect(m_actionResetFolderView, &QAction::triggered, this, &MainWindow::resetMessageListViewForFolder);

        m_actionExpandAllGroups = new QAction(quickActionIcon(QStringLiteral("expand-all")),
                                              i18nc("@action:inmenu", "&Expand All Groups"), this);
        m_actionExpandAllGroups->setObjectName(QStringLiteral("messageListExpandAllGroups"));
        m_actionExpandAllGroups->setEnabled(false);
        connect(m_actionExpandAllGroups, &QAction::triggered, this, [this]() {
            m_collapsedGroups.clear();
            msgListWidget->tree->expandAll();
        });
        m_actionCollapseAllGroups = new QAction(quickActionIcon(QStringLiteral("collapse-all")),
                                                i18nc("@action:inmenu", "C&ollapse All Groups"), this);
        m_actionCollapseAllGroups->setObjectName(QStringLiteral("messageListCollapseAllGroups"));
        m_actionCollapseAllGroups->setEnabled(false);
        connect(m_actionCollapseAllGroups, &QAction::triggered, this, [this]() {
            if (!m_groupingMsgListModel || msgListWidget->tree->model() != m_groupingMsgListModel)
                return;
            for (int row = 0, rows = m_groupingMsgListModel->rowCount(); row < rows; ++row)
                m_collapsedGroups.insert(m_groupingMsgListModel->groupKey(m_groupingMsgListModel->index(row, 0)));
            msgListWidget->tree->collapseAll();
        });
        // Remember the groups which the user collapses while the list changes
        connect(msgListWidget->tree, &QTreeView::expanded, this, [this](const QModelIndex &index) {
            if (m_groupingMsgListModel && index.model() == m_groupingMsgListModel && m_groupingMsgListModel->isGroupHeader(index))
                m_collapsedGroups.remove(m_groupingMsgListModel->groupKey(index));
        });
        connect(msgListWidget->tree, &QTreeView::collapsed, this, [this](const QModelIndex &index) {
            if (m_groupingMsgListModel && index.model() == m_groupingMsgListModel && m_groupingMsgListModel->isGroupHeader(index))
                m_collapsedGroups.insert(m_groupingMsgListModel->groupKey(index));
        });
    }

    using Imap::Mailbox::PrettyMsgListModel;
    auto addFilterAction = [this](const QString &objectName, const QString &icon, const QString &text, const QString &toolTip,
                                  PrettyMsgListModel::Filter filter) {
        auto *action = new QAction(quickActionIcon(icon), text, this);
        action->setObjectName(objectName);
        action->setCheckable(true);
        action->setToolTip(toolTip);
        connect(action, &QAction::triggered, this, [this, filter](bool checked) {
            if (checked)
                rememberUnnarrowedPosition();
            prettyMsgListModel->setFilterEnabled(filter, checked);
            saveMessageListFilters();
        });
        msgListWidget->addToolButton(action)->setObjectName(objectName + QLatin1String("Button"));
        return action;
    };
    m_actionFilterUnread = addFilterAction(QStringLiteral("messageListFilterUnread"), QStringLiteral("mail-mark-unread"),
                                           i18nc("@action:inmenu quick filter", "&Unread"),
                                           i18nc("@info:tooltip", "Show only unread messages"), PrettyMsgListModel::FilterUnread);
    m_actionFilterFlagged = addFilterAction(QStringLiteral("messageListFilterFlagged"), QStringLiteral("mail-flagged"),
                                            i18nc("@action:inmenu quick filter", "&Flagged"),
                                            i18nc("@info:tooltip", "Show only flagged messages"), PrettyMsgListModel::FilterFlagged);
    m_actionFilterAttachments = addFilterAction(QStringLiteral("messageListFilterAttachments"), QStringLiteral("mail-attachment"),
                                                i18nc("@action:inmenu quick filter", "With &Attachments"),
                                                i18nc("@info:tooltip", "Show only messages with attachments"),
                                                PrettyMsgListModel::FilterAttachments);
    m_actionClearFilters = new QAction(quickActionIcon(QStringLiteral("edit-clear-all")), i18nc("@action", "&Clear Filters"), this);
    m_actionClearFilters->setObjectName(QStringLiteral("messageListClearFilters"));
    m_actionClearFilters->setToolTip(i18nc("@info:tooltip", "Show all messages again"));
    connect(m_actionClearFilters, &QAction::triggered, this, [this]() {
        prettyMsgListModel->setFilters(PrettyMsgListModel::NoFilter);
        saveMessageListFilters();
    });
    m_clearFiltersButton = msgListWidget->addToolButton(m_actionClearFilters);
    m_clearFiltersButton->setObjectName(QStringLiteral("messageListClearFiltersButton"));
    syncMessageListFilterUi();

    auto addSelectionAction = [this](const QString &name) {
        QAction *action = ShortcutHandler::instance()->createAction(name, this);
        // Only while the message list has the focus, so that Ctrl+A keeps selecting text elsewhere
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        msgListWidget->tree->addAction(action);
        return action;
    };
    m_actionSelectAllMessages = addSelectionAction(QStringLiteral("action_select_all_messages"));
    connect(m_actionSelectAllMessages, &QAction::triggered, msgListWidget->tree, &QAbstractItemView::selectAll);
    m_actionSelectThread = addSelectionAction(QStringLiteral("action_select_thread"));
    m_actionSelectThread->setEnabled(false);
    connect(m_actionSelectThread, &QAction::triggered, this, &MainWindow::slotSelectThread);
    m_actionInvertSelection = addSelectionAction(QStringLiteral("action_invert_selection"));
    connect(m_actionInvertSelection, &QAction::triggered, this, &MainWindow::slotInvertSelection);
    m_actionDeselectMessages = addSelectionAction(QStringLiteral("action_deselect_messages"));
    connect(m_actionDeselectMessages, &QAction::triggered, msgListWidget->tree, &QAbstractItemView::clearSelection);
    connect(actionThreadMsgList, &QAction::changed, this, &MainWindow::scheduleMessageListStatus);

    // The Bat!-style processing: filter or select by what the current message has in common with others
    m_showRelatedMenu = new QMenu(i18nc("@title:menu", "Show &Related"), this);
    m_showRelatedMenu->setObjectName(QStringLiteral("showRelatedMenu"));
    m_showRelatedMenu->setIcon(quickActionIcon(QStringLiteral("view-filter")));
    const std::pair<QAction **, std::pair<const char *, UiUtils::FieldFilter>> related[] = {
        {&m_actionRelatedSender, {"action_show_related_sender", UiUtils::FieldFilter::From}},
        {&m_actionRelatedRecipient, {"action_show_related_recipient", UiUtils::FieldFilter::To}},
        {&m_actionRelatedSubject, {"action_show_related_subject", UiUtils::FieldFilter::Subject}},
        {&m_actionRelatedList, {"action_show_related_list", UiUtils::FieldFilter::MailingList}},
    };
    for (const auto &[member, spec] : related) {
        const UiUtils::FieldFilter field = spec.second;
        *member = addSelectionAction(QString::fromLatin1(spec.first));
        connect(*member, &QAction::triggered, this, [this, field]() { showRelatedMessages(field); });
        m_showRelatedMenu->addAction(*member);
    }
    connect(m_showRelatedMenu, &QMenu::aboutToShow, this, &MainWindow::updateRelatedActions);

    m_selectSimilarMenu = new QMenu(i18nc("@title:menu", "Select &Similar"), this);
    m_selectSimilarMenu->setObjectName(QStringLiteral("selectSimilarMenu"));
    m_selectSimilarMenu->setIcon(quickActionIcon(QStringLiteral("edit-select-all")));
    const std::pair<QAction **, std::pair<const char *, UiUtils::SimilarMessages>> similar[] = {
        {&m_actionSelectSameSender, {"action_select_same_sender", UiUtils::SimilarMessages::FromSender}},
        {&m_actionSelectSameSubject, {"action_select_same_subject", UiUtils::SimilarMessages::WithSubject}},
        {&m_actionSelectUnread, {"action_select_unread", UiUtils::SimilarMessages::Unread}},
        {&m_actionSelectFlagged, {"action_select_flagged", UiUtils::SimilarMessages::Flagged}},
    };
    for (const auto &[member, spec] : similar) {
        const UiUtils::SimilarMessages criterion = spec.second;
        *member = addSelectionAction(QString::fromLatin1(spec.first));
        connect(*member, &QAction::triggered, this, [this, criterion]() { selectSimilarMessages(criterion); });
        m_selectSimilarMenu->addAction(*member);
        if (criterion == UiUtils::SimilarMessages::WithSubject)
            m_selectSimilarMenu->addSeparator();
    }
    connect(m_selectSimilarMenu, &QMenu::aboutToShow, this, &MainWindow::updateRelatedActions);

    m_unifiedModel = new Imap::Mailbox::UnifiedMsgListModel(this);
    m_unifiedModel->setObjectName(QStringLiteral("unifiedMsgListModel"));
    m_prettyUnifiedModel = new Imap::Mailbox::PrettyUnifiedMsgListModel(this);
    m_prettyUnifiedModel->setObjectName(QStringLiteral("prettyUnifiedMsgListModel"));
    m_prettyUnifiedModel->setSourceModel(m_unifiedModel);
    connect(m_unifiedModel, &QAbstractItemModel::dataChanged, this, &MainWindow::scheduleMessageFlagsUpdate);
    connect(m_unifiedModel, &QAbstractItemModel::rowsRemoved, this, &MainWindow::scheduleMessageFlagsUpdate);
    connect(m_prettyUnifiedModel, &QAbstractItemModel::rowsInserted, this, &MainWindow::scheduleMessageListStatus);
    connect(m_prettyUnifiedModel, &QAbstractItemModel::rowsRemoved, this, &MainWindow::scheduleMessageListStatus);
    connect(m_prettyUnifiedModel, &QAbstractItemModel::modelReset, this, &MainWindow::scheduleMessageListStatus);
    connect(m_prettyUnifiedModel, &QAbstractItemModel::layoutChanged, this, &MainWindow::scheduleMessageListStatus);
    connect(m_prettyUnifiedModel, &QAbstractItemModel::dataChanged, this, &MainWindow::scheduleSavedSearchCount);
    connect(m_unifiedModel, &Imap::Mailbox::UnifiedMsgListModel::accountsChanged, this, [this]() {
        if (showsUnifiedList()) {
            updateMessageListScope();
            scheduleMessageListStatus();
        }
    });
    connect(m_accountStatus, &Common::AccountStatusTracker::statusChanged, this, &MainWindow::updateUnifiedStates);
    m_actionUnifiedInbox = ShortcutHandler::instance()->createAction(QStringLiteral("action_unified_inbox"), this);
    m_actionUnifiedInbox->setCheckable(true);
    m_actionUnifiedInbox->setToolTip(i18nc("@info:tooltip", "Show the INBOX of all accounts in one list"));
    connect(m_actionUnifiedInbox, &QAction::toggled, this, [this](bool checked) { setUnifiedInbox(checked); });
    m_unifiedAccountsMenu = new QMenu(i18nc("@title:menu", "Unified Inbox &Accounts"), this);
    m_unifiedAccountsMenu->setObjectName(QStringLiteral("unifiedInboxAccountsMenu"));
    connect(m_unifiedAccountsMenu, &QMenu::aboutToShow, this, &MainWindow::populateUnifiedAccountsMenu);

    m_actionDenseProcessing = ShortcutHandler::instance()->createAction(QStringLiteral("action_dense_processing_layout"), this);
    m_actionDenseProcessing->setCheckable(true);
    m_actionDenseProcessing->setToolTip(i18nc("@info:tooltip", "Compact rows without the message details, contact and related "
                                                               "messages panels, for working through many messages; turn off to "
                                                               "restore the previous layout"));

    QActionGroup *layoutGroup = new QActionGroup(this);
    m_actionLayoutCompact = new QAction(tr("&Compact"), layoutGroup);
    m_actionLayoutCompact->setCheckable(true);
    m_actionLayoutCompact->setChecked(true);
    connect(m_actionLayoutCompact, &QAction::triggered, this, &MainWindow::slotLayoutCompact);
    m_actionLayoutWide = new QAction(tr("&Wide"), layoutGroup);
    m_actionLayoutWide->setCheckable(true);
    connect(m_actionLayoutWide, &QAction::triggered, this, &MainWindow::slotLayoutWide);
    m_actionLayoutOneAtTime = new QAction(tr("&One At a Time"), layoutGroup);
    m_actionLayoutOneAtTime->setCheckable(true);
    connect(m_actionLayoutOneAtTime, &QAction::triggered, this, &MainWindow::slotLayoutOneAtTime);


    m_actionShowOnlySubscribed = new QAction(tr("Show Only S&ubscribed Folders"), this);
    m_actionShowOnlySubscribed->setCheckable(true);
    m_actionShowOnlySubscribed->setEnabled(false);
    connect(m_actionShowOnlySubscribed, &QAction::toggled, this, &MainWindow::slotShowOnlySubscribed);
    m_actionSubscribeMailbox = new QAction(tr("Su&bscribed"), this);
    m_actionSubscribeMailbox->setCheckable(true);
    m_actionSubscribeMailbox->setEnabled(false);
    connect(m_actionSubscribeMailbox, &QAction::triggered, this, &MainWindow::slotSubscribeCurrentMailbox);

    aboutTrojita = new QAction(tr("&About Plasma Impresario..."), this);
    aboutTrojita->setObjectName(QStringLiteral("action_about"));
    connect(aboutTrojita, &QAction::triggered, this, &MainWindow::slotShowAboutTrojita);

    connectModelActions();

    m_composeMenu = new QMenu(tr("Compose Mail"), this);
    m_composeMenu->addAction(composeMail);
    m_composeMenu->addAction(m_editDraft);
    m_composeButton = new QToolButton(this);
    m_composeButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_composeButton->setMenu(m_composeMenu);
    m_composeButton->setDefaultAction(composeMail);

    m_replyButton = new QToolButton(this);
    m_replyButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_replyMenu = new QMenu(m_replyButton);
    m_replyMenu->addAction(m_replyPrivate);
    m_replyMenu->addAction(m_replyAllButMe);
    m_replyMenu->addAction(m_replyAll);
    m_replyMenu->addAction(m_replyList);
    m_replyButton->setMenu(m_replyMenu);
    m_replyButton->setDefaultAction(m_replyPrivate);
    // A corner marker instead of an arrow beside the button shows that it has a menu
    AppearanceStyle::addMenuMarker(m_composeButton);
    AppearanceStyle::addMenuMarker(m_replyButton);

#ifdef IMPRESARIO_HAVE_WEBENGINE
    // The bar above the reader works on the open message, whatever is selected in the list
    m_readerActions = new ReaderActionBar();
    m_readerActions->setReplyMenu(m_replyMenu);
    m_readerActions->setReplyAction(m_replyButton->defaultAction());
    m_readerActions->setForwardAction(m_forwardAsAttachment);
    m_readerActions->setPreferPlainText(m_settings->value(Common::SettingsNames::guiPreferPlaintextRendering, QVariant(true)).toBool());
    m_messageWidget->setActionBar(m_readerActions);
#endif
    // Escape leaves the reader or an embedded panel for the message list. The scopes leave out the find bar and
    // the quick search, which use Escape themselves.
    QList<QWidget *> escapeScopes;
#ifdef IMPRESARIO_HAVE_WEBENGINE
    escapeScopes << m_messageWidget->messageView << m_readerActions;
#else
    escapeScopes << m_messageWidget;
#endif
    for (EmbeddedPanel *panel : std::as_const(m_embeddedPanels))
        escapeScopes << panel;
    for (QWidget *scope : std::as_const(escapeScopes)) {
        if (!scope)
            continue;
        auto *toList = new QShortcut(QKeySequence(Qt::Key_Escape), scope);
        toList->setObjectName(QStringLiteral("escapeToMessageList"));
        toList->setContext(Qt::WidgetWithChildrenShortcut);
        connect(toList, &QShortcut::activated, this, &MainWindow::slotReturnToMessageList);
    }
#ifdef IMPRESARIO_HAVE_WEBENGINE
    connect(m_readerActions, &ReaderActionBar::archiveRequested, this, [this]() {
        actOnOpenMessage([this]() { handleMoveToArchive(); });
    });
    connect(m_readerActions, &ReaderActionBar::trashRequested, this, [this]() {
        actOnOpenMessage([this]() {
            handleMarkAsDeleted(!translatedSelection().constFirst().data(Imap::Mailbox::RoleMessageIsMarkedDeleted).toBool());
        });
    });
    connect(m_readerActions, &ReaderActionBar::markReadRequested, this, [this](bool read) {
        actOnOpenMessage([this, read]() { handleMarkAsRead(read); });
    });
    connect(m_readerActions, &ReaderActionBar::flagRequested, this, [this](bool flagged) {
        actOnOpenMessage([this, flagged]() { handleMarkAsFlagged(flagged); });
    });
    connect(m_readerActions, &ReaderActionBar::zoomInRequested, m_messageWidget->messageView, &MessageView::zoomIn);
    connect(m_readerActions, &ReaderActionBar::zoomOutRequested, m_messageWidget->messageView, &MessageView::zoomOut);
    connect(m_readerActions, &ReaderActionBar::zoomResetRequested, m_messageWidget->messageView, &MessageView::zoomOriginal);
    connect(m_readerActions, &ReaderActionBar::preferPlainTextToggled, this, [this](bool preferPlainText) {
        m_settings->setValue(Common::SettingsNames::guiPreferPlaintextRendering, preferPlainText);
        m_messageWidget->messageView->reloadMessage();
    });
    connect(m_readerActions, &ReaderActionBar::attachmentsRequested, this, &MainWindow::showAttachmentPanel);
    connect(m_attachmentPanel, &AttachmentPanel::attachmentCountChanged, m_readerActions, &ReaderActionBar::setAttachmentCount);
    m_readerActions->setAttachmentCount(m_attachmentPanel->attachmentCount());
    connect(m_messageWidget->messageView, &MessageView::messageChanged, this, &MainWindow::updateReaderActions);
    connect(m_messageWidget->messageView, &MessageView::messageModelChanged, this, &MainWindow::updateReaderActions);
#endif

    m_accountSwitcher = new QComboBox(this);
    m_accountSwitcher->setToolTip(i18n("Account"));
    m_accountSwitcher->setAccessibleName(i18n("Account"));
    connect(m_accountSwitcher, &QComboBox::activated, this, [this](int index) {
        switchAccount(m_accountSwitcher->itemData(index).toString());
    });
    refreshAccountSwitcher();
    m_mainToolbar->addWidget(m_accountSwitcher);
    m_mainToolbar->addSeparator();
    m_mainToolbar->addWidget(m_composeButton);
    m_mainToolbar->addWidget(m_replyButton);
    m_mainToolbar->addAction(m_forwardAsAttachment);
    m_mainToolbar->addAction(expunge);
    m_mainToolbar->addSeparator();
    m_mainToolbar->addAction(markAsRead);
    m_mainToolbar->addAction(markAsDeleted);
    m_mainToolbar->addAction(markAsFlagged);
    m_mainToolbar->addAction(markAsJunk);
    m_mainToolbar->addAction(markAsNotJunk);
    m_mainToolbar->addAction(moveToArchive);
    m_moveButton = new QToolButton(this);
    m_moveButton->setObjectName(QStringLiteral("moveToFolderButton"));
    m_moveButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_moveButton->setMenu(m_moveToMenu);
    m_moveButton->setDefaultAction(m_moveToFolder);
    AppearanceStyle::addMenuMarker(m_moveButton);
    m_mainToolbar->addWidget(m_moveButton);

    // Push the status indicators all the way to the other side of the toolbar -- either to the far right, or far bottom.
    QWidget *toolbarSpacer = new QWidget(m_mainToolbar);
    toolbarSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_mainToolbar->addWidget(toolbarSpacer);

    m_mainToolbar->addSeparator();
    m_mainToolbar->addWidget(busyParsersIndicator);

    networkIndicator = new QToolButton(this);
    // This is deliberate; we want to show this button in the same style as the other ones in the toolbar
    networkIndicator->setPopupMode(QToolButton::MenuButtonPopup);
    networkIndicator->setAccessibleName(i18nc("@action:button", "Network access"));
    AppearanceStyle::addMenuMarker(networkIndicator);
    m_mainToolbar->addWidget(networkIndicator);

    m_menuFromToolBar = new QToolButton(this);
    m_menuFromToolBar->setIcon(UiUtils::loadIcon(QStringLiteral("menu_new")));
    m_menuFromToolBar->setText(QChar(0x205d)); // Unicode 'TRICOLON'
    m_menuFromToolBar->setAccessibleName(i18nc("@action:button", "Main menu"));
    m_menuFromToolBar->setPopupMode(QToolButton::InstantPopup);
    AppearanceStyle::addMenuMarker(m_menuFromToolBar);
    m_mainToolbar->addWidget(m_menuFromToolBar);
    connect(showMenuBar, &QAction::toggled, this, [this](const bool menuBarVisible) {
        // https://bugreports.qt.io/browse/QTBUG-35768 , we have to work on the QAction, not QToolButton
        m_mainToolbar->actions().constLast()->setVisible(!menuBarVisible);
    });

    busyParsersIndicator->setFixedSize(m_mainToolbar->iconSize());

    {
        // Custom widgets which are added into a QToolBar are by default aligned to the left, while QActions are justified.
        // That sucks, because some of our widgets use multiple actions with an expanding arrow at right.
        // Make sure everything is aligned to the left, so that the actual buttons are aligned properly and the extra arrows
        // are, well, at right.
        // I have no idea how this works on RTL layouts.
        QLayout *lay = m_mainToolbar->layout();
        for (int i = 0; i < lay->count(); ++i) {
            QLayoutItem *it = lay->itemAt(i);
            if (it->widget() == toolbarSpacer) {
                // Don't align this one, otherwise it won't push stuff when in horizontal direction
                continue;
            }
            if (it->widget() == busyParsersIndicator) {
                // It looks much better when centered
                it->setAlignment(Qt::AlignJustify);
                continue;
            }
            it->setAlignment(Qt::AlignLeft);
        }
    }

    // Tabs: hidden, and so without working shortcuts, until they are turned on
    m_actionOpenInNewTab = new QAction(quickActionIcon(QStringLiteral("tab-new")), i18nc("@action", "Open in New &Tab"), this);
    m_actionOpenInNewTab->setObjectName(QStringLiteral("action_open_in_new_tab"));
    connect(m_actionOpenInNewTab, &QAction::triggered, this, [this]() { openTab(m_newTabCandidate); });
    m_actionNewTab = ShortcutHandler::instance()->createAction(QStringLiteral("action_new_tab"), this);
    connect(m_actionNewTab, &QAction::triggered, this, [this]() {
        const QModelIndex selected = msgListWidget->tree->currentIndex();
        UiUtils::MailTabs::Tab tab = selected.isValid() && msgListWidget->tree->selectionModel()->isSelected(selected)
            ? messageTab(selected)
            : UiUtils::MailTabs::Tab();
        if (tab.accountId.isEmpty())
            tab = currentLocationTab();
        openTab(tab);
    });
    m_actionCloseTab = ShortcutHandler::instance()->createAction(QStringLiteral("action_close_tab"), this);
    connect(m_actionCloseTab, &QAction::triggered, this, [this]() { closeTab(m_tabSession.current); });
    m_actionNextTab = ShortcutHandler::instance()->createAction(QStringLiteral("action_next_tab"), this);
    m_actionNextTab->setShortcuts({m_actionNextTab->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_Tab)});
    connect(m_actionNextTab, &QAction::triggered, this, [this]() {
        if (m_tabSession.tabs.size() > 1)
            activateTab((m_tabSession.current + 1) % m_tabSession.tabs.size());
    });
    m_actionPreviousTab = ShortcutHandler::instance()->createAction(QStringLiteral("action_previous_tab"), this);
    m_actionPreviousTab->setShortcuts({m_actionPreviousTab->shortcut(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Backtab),
                                       QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab)});
    connect(m_actionPreviousTab, &QAction::triggered, this, [this]() {
        if (m_tabSession.tabs.size() > 1)
            activateTab((m_tabSession.current + m_tabSession.tabs.size() - 1) % m_tabSession.tabs.size());
    });
    for (QAction *action : {m_actionOpenInNewTab, m_actionNewTab, m_actionCloseTab, m_actionNextTab, m_actionPreviousTab})
        action->setVisible(false);

    updateMessageFlags();
}

void MainWindow::connectModelActions()
{
    connect(reloadAllMailboxes, &QAction::triggered, imapModel(), &Imap::Mailbox::Model::reloadMailboxList);
    connect(netOffline, &QAction::triggered,
            qobject_cast<Imap::Mailbox::NetworkWatcher*>(m_imapAccess->networkWatcher()), &Imap::Mailbox::NetworkWatcher::setNetworkOffline);
    connect(netExpensive, &QAction::triggered,
            qobject_cast<Imap::Mailbox::NetworkWatcher*>(m_imapAccess->networkWatcher()), &Imap::Mailbox::NetworkWatcher::setNetworkExpensive);
    connect(netOnline, &QAction::triggered,
            qobject_cast<Imap::Mailbox::NetworkWatcher*>(m_imapAccess->networkWatcher()), &Imap::Mailbox::NetworkWatcher::setNetworkOnline);
    netExpensive->setEnabled(imapAccess()->isConfigured());
    netOnline->setEnabled(imapAccess()->isConfigured());
}

void MainWindow::createMenus()
{
#define ADD_ACTION(MENU, ACTION) \
    MENU->addAction(ACTION); \
    addAction(ACTION);

    QMenu *applicationMenu = menuBar()->addMenu(tr("&Application"));
        ADD_ACTION(applicationMenu, m_actionContactEditor);
        QMenu *netPolicyMenu = applicationMenu->addMenu(tr("&Network Access"));
            ADD_ACTION(netPolicyMenu, netOffline);
            ADD_ACTION(netPolicyMenu, netExpensive);
            ADD_ACTION(netPolicyMenu, netOnline);
        QMenu *debugMenu = applicationMenu->addMenu(tr("&Debugging"));
            ADD_ACTION(debugMenu, showFullView);
            ADD_ACTION(debugMenu, showTaskView);
            ADD_ACTION(debugMenu, showMimeView);
            ADD_ACTION(debugMenu, showProtocolLogger);
            ADD_ACTION(debugMenu, logPersistent);
            debugMenu->addSeparator();
            ADD_ACTION(debugMenu, showImapCapabilities);
            debugMenu->addSeparator();
            ADD_ACTION(debugMenu, reloadAllMailboxes);
            ADD_ACTION(debugMenu, resyncMbox);
        applicationMenu->addSeparator();
        ADD_ACTION(applicationMenu, m_actionAddAccount);
        ADD_ACTION(applicationMenu, m_actionAccountSettings);
        ADD_ACTION(applicationMenu, m_actionSearchIndex);
        ADD_ACTION(applicationMenu, m_actionRenameAccount);
        ADD_ACTION(applicationMenu, m_actionRemoveAccount);
        applicationMenu->addSeparator();
        ADD_ACTION(applicationMenu, configSettings);
        ADD_ACTION(applicationMenu, m_actionConfigureNotifications);
        ADD_ACTION(applicationMenu, m_actionMailRules);
        ADD_ACTION(applicationMenu, m_actionActivity);
        ADD_ACTION(applicationMenu, ShortcutHandler::instance()->shortcutConfigAction());
        applicationMenu->addSeparator();
        ADD_ACTION(applicationMenu, aboutTrojita);
        applicationMenu->addSeparator();
        ADD_ACTION(applicationMenu, exitAction);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
        ADD_ACTION(viewMenu, showMenuBar);
        ADD_ACTION(viewMenu, showToolBar);
        viewMenu->addAction(m_shellHeader->toggleAction());
        QMenu *layoutMenu = viewMenu->addMenu(tr("&Layout"));
            ADD_ACTION(layoutMenu, m_actionLayoutCompact);
            ADD_ACTION(layoutMenu, m_actionLayoutWide);
            ADD_ACTION(layoutMenu, m_actionLayoutOneAtTime);
        auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
        KConfigGroup appearance(config, QStringLiteral("Appearance"));
        const QString sidebarPosition = appearance.readEntry("MailboxSidebarPosition", QStringLiteral("left"));
        if (sidebarPosition != QLatin1String("left") && sidebarPosition != QLatin1String("right"))
            qWarning() << "Unknown mailbox sidebar position:" << sidebarPosition;
        m_mailboxSidebarOnRight = sidebarPosition == QLatin1String("right");
        QMenu *sidebarMenu = viewMenu->addMenu(i18nc("@title:menu", "Folder Sidebar Position"));
        auto *sidebarGroup = new QActionGroup(sidebarMenu);
        auto *sidebarLeft = sidebarMenu->addAction(i18nc("@option:radio", "Left"));
        auto *sidebarRight = sidebarMenu->addAction(i18nc("@option:radio", "Right"));
        sidebarLeft->setObjectName(QStringLiteral("mailboxSidebarLeft"));
        sidebarRight->setObjectName(QStringLiteral("mailboxSidebarRight"));
        for (auto *action : {sidebarLeft, sidebarRight}) {
            action->setCheckable(true);
            sidebarGroup->addAction(action);
        }
        (m_mailboxSidebarOnRight ? sidebarRight : sidebarLeft)->setChecked(true);
        connect(sidebarGroup, &QActionGroup::triggered, this, [this, config, sidebarRight](QAction *action) {
            const bool right = action == sidebarRight;
            setMailboxSidebarRight(right);
            KConfigGroup appearance(config, QStringLiteral("Appearance"));
            appearance.writeEntry("MailboxSidebarPosition", right ? QStringLiteral("right") : QStringLiteral("left"));
            if (!config->sync())
                qWarning() << "Unable to save mailbox sidebar position";
        });
        m_densityMenu = new DensityMenu(mboxTree, msgListWidget->tree, config, viewMenu);
        viewMenu->addMenu(m_densityMenu);
        m_messageRowsMenu = new MessageRowsMenu(msgListWidget->tree, config, viewMenu);
        connect(m_messageRowsMenu, &MessageRowsMenu::appearanceChanged, this, &MainWindow::applyAppearance);
        viewMenu->addMenu(m_messageRowsMenu);
        if (!m_embeddedPanels.isEmpty()) {
            m_embeddedPanelsMenu = new EmbeddedPanelsMenu(config, viewMenu);
            for (EmbeddedPanel *panel : std::as_const(m_embeddedPanels))
                m_embeddedPanelsMenu->addPanel(panel);
            viewMenu->addMenu(m_embeddedPanelsMenu);
        }
        ADD_ACTION(viewMenu, m_actionDenseProcessing);
        // The preset's changes are already saved in the preferences, so only the check mark needs restoring
        m_actionDenseProcessing->setChecked(KConfigGroup(KSharedConfig::openStateConfig(), QStringLiteral("DenseProcessing"))
                                                .readEntry("Enabled", false));
        connect(m_actionDenseProcessing, &QAction::toggled, this, &MainWindow::setDenseProcessingLayout);
        ADD_ACTION(viewMenu, m_actionUnifiedInbox);
        viewMenu->addMenu(m_unifiedAccountsMenu);
        QMenu *filterMenu = viewMenu->addMenu(quickActionIcon(QStringLiteral("view-filter")), i18nc("@title:menu", "&Filter"));
        filterMenu->setObjectName(QStringLiteral("messageListFilterMenu"));
            ADD_ACTION(filterMenu, m_actionFilterUnread);
            ADD_ACTION(filterMenu, m_actionFilterFlagged);
            ADD_ACTION(filterMenu, m_actionFilterAttachments);
            filterMenu->addSeparator();
            ADD_ACTION(filterMenu, m_actionClearFilters);
            ADD_ACTION(filterMenu, m_clearSearchAndFilters);
            filterMenu->addSeparator();
            filterMenu->addMenu(msgListWidget->facetsMenu());
        m_groupByMenu = viewMenu->addMenu(quickActionIcon(QStringLiteral("view-group")), i18nc("@title:menu", "&Group By"));
        m_groupByMenu->setObjectName(QStringLiteral("messageListGroupByMenu"));
            for (QAction *action : m_groupByActions->actions()) {
                ADD_ACTION(m_groupByMenu, action);
            }
            m_groupByMenu->addSeparator();
            ADD_ACTION(m_groupByMenu, m_actionExpandAllGroups);
            ADD_ACTION(m_groupByMenu, m_actionCollapseAllGroups);
            ADD_ACTION(m_groupByMenu, m_expandAllConversations);
            ADD_ACTION(m_groupByMenu, m_collapseAllConversations);
        viewMenu->addAction(m_actionViewForAllFolders);
        viewMenu->addAction(m_actionResetFolderView);
        viewMenu->addSeparator();
        ADD_ACTION(viewMenu, m_actionShowOnlySubscribed);
        viewMenu->addSeparator();
        ADD_ACTION(viewMenu, m_actionNewTab);
        ADD_ACTION(viewMenu, m_actionCloseTab);
        ADD_ACTION(viewMenu, m_actionNextTab);
        ADD_ACTION(viewMenu, m_actionPreviousTab);

    QMenu *mailboxMenu = menuBar()->addMenu(tr("Mail&box"));
        mailboxMenu->addMenu(m_goToFolderMenu);
        ADD_ACTION(mailboxMenu, m_goToFolder);
        ADD_ACTION(mailboxMenu, m_goToNextUnreadFolder);
        ADD_ACTION(mailboxMenu, m_checkMail);
        mailboxMenu->addSeparator();
        ADD_ACTION(mailboxMenu, m_actionFind);
        ADD_ACTION(mailboxMenu, m_searchBuilder);
        mailboxMenu->addAction(m_actionFindSavedFiles);
        mailboxMenu->addAction(m_actionBrowseNextcloud);
        ADD_ACTION(mailboxMenu, m_actionRuleFromSearch);
        QMenu *sortMenu = mailboxMenu->addMenu(tr("S&orting"));
            ADD_ACTION(sortMenu, m_actionSortNone);
            ADD_ACTION(sortMenu, m_actionSortThreading);
            ADD_ACTION(sortMenu, m_actionSortByArrival);
            ADD_ACTION(sortMenu, m_actionSortByCc);
            ADD_ACTION(sortMenu, m_actionSortByDate);
            ADD_ACTION(sortMenu, m_actionSortByFrom);
            ADD_ACTION(sortMenu, m_actionSortBySize);
            ADD_ACTION(sortMenu, m_actionSortBySubject);
            ADD_ACTION(sortMenu, m_actionSortByTo);
            ADD_ACTION(sortMenu, m_actionSortByRelevance);
            sortMenu->addSeparator();
            ADD_ACTION(sortMenu, m_actionSortAscending);
            ADD_ACTION(sortMenu, m_actionSortDescending);
            sortMenu->addSeparator();
            ADD_ACTION(sortMenu, actionThreadMsgList);
            sortMenu->addMenu(m_groupByMenu);
            ADD_ACTION(sortMenu, actionHideRead);
            sortMenu->addSeparator();
            ADD_ACTION(sortMenu, m_actionViewForAllFolders);
            ADD_ACTION(sortMenu, m_actionResetFolderView);
        sortMenu->setObjectName(QStringLiteral("sortMenu"));
        m_sortButton = msgListWidget->addToolMenuButton(sortMenu, quickActionIcon(QStringLiteral("view-sort-ascending")),
                                                        i18nc("@action:button", "Sort"));
        m_sortButton->setObjectName(QStringLiteral("messageListSortButton"));
        for (QAction *action : sortMenu->actions())
            connect(action, &QAction::toggled, this, &MainWindow::updateSortButton);
        updateSortButton();
        mailboxMenu->addSeparator();
#ifdef IMPRESARIO_HAVE_WEBENGINE
        ADD_ACTION(mailboxMenu, m_previousMessage);
        ADD_ACTION(mailboxMenu, m_nextMessage);
        ADD_ACTION(mailboxMenu, m_previousAnyMessage);
        ADD_ACTION(mailboxMenu, m_nextAnyMessage);
#endif
        ADD_ACTION(mailboxMenu, m_previousInConversation);
        ADD_ACTION(mailboxMenu, m_nextInConversation);
        mailboxMenu->addSeparator();
        ADD_ACTION(mailboxMenu, expunge);
        mailboxMenu->addSeparator();
        ADD_ACTION(mailboxMenu, m_renameMailbox);
        ADD_ACTION(mailboxMenu, m_actionNotifyFolder);
        ADD_ACTION(mailboxMenu, m_emptyTrash);
        ADD_ACTION(mailboxMenu, m_emptyJunk);
        connect(mailboxMenu, &QMenu::aboutToShow, this, [this]() {
            m_renameMailbox->setEnabled(canRenameMailbox(mboxTree->currentIndex()));
            prepareNotifyFolderAction(mboxTree->currentIndex());
        });

    QMenu *messageMenu = menuBar()->addMenu(tr("&Message"));
        ADD_ACTION(messageMenu, composeMail);
        ADD_ACTION(messageMenu, m_editDraft);
        messageMenu->addSeparator();
        ADD_ACTION(messageMenu, m_replyGuess);
        ADD_ACTION(messageMenu, m_replyPrivate);
        ADD_ACTION(messageMenu, m_replyAll);
        ADD_ACTION(messageMenu, m_replyAllButMe);
        ADD_ACTION(messageMenu, m_replyList);
        messageMenu->addSeparator();
        ADD_ACTION(messageMenu, m_forwardAsAttachment);
        ADD_ACTION(messageMenu, m_resend);
        messageMenu->addSeparator();
        ADD_ACTION(messageMenu, markAsDeleted);
        ADD_ACTION(messageMenu, m_deletePermanently);
        ADD_ACTION(messageMenu, m_deleteMovesToTrash);
        messageMenu->addSeparator();
        ADD_ACTION(messageMenu, markAsRead);
        ADD_ACTION(messageMenu, m_markAsUnread);
#ifdef IMPRESARIO_HAVE_WEBENGINE
        ADD_ACTION(messageMenu, m_markReadNextUnread);
#endif
        ADD_ACTION(messageMenu, markAsFlagged);
        messageMenu->addSeparator();
        ADD_ACTION(messageMenu, moveToArchive);
        messageMenu->addMenu(m_moveToMenu);
        messageMenu->addMenu(m_copyToMenu);
        ADD_ACTION(messageMenu, m_moveToFolder);
        ADD_ACTION(messageMenu, m_copyToFolder);
        messageMenu->addSeparator();
        ADD_ACTION(messageMenu, m_actionMessageToCalendar);
        ADD_ACTION(messageMenu, m_actionMessageToNote);
        messageMenu->addSeparator();
        // Not added to the window, so that their shortcuts only work in the message list
        messageMenu->addAction(m_actionSelectAllMessages);
        messageMenu->addAction(m_actionSelectThread);
        messageMenu->addAction(m_actionInvertSelection);
        messageMenu->addAction(m_actionDeselectMessages);
        messageMenu->addMenu(m_selectSimilarMenu);
        messageMenu->addSeparator();
        messageMenu->addMenu(m_showRelatedMenu);
        connect(messageMenu, &QMenu::aboutToShow, this, &MainWindow::updateSelectionActions);

    QMenu *helpMenu = menuBar()->addMenu(i18nc("@title:menu", "&Help"));
        ADD_ACTION(helpMenu, m_actionShortcutOverview);

    QMenu *mainMenuBehindToolBar = new QMenu(this);
    m_menuFromToolBar->setMenu(mainMenuBehindToolBar);
    m_menuFromToolBar->menu()->addMenu(applicationMenu);
    m_menuFromToolBar->menu()->addMenu(viewMenu);
    m_menuFromToolBar->menu()->addMenu(mailboxMenu);
    m_menuFromToolBar->menu()->addMenu(messageMenu);
    m_menuFromToolBar->menu()->addMenu(helpMenu);
    m_menuFromToolBar->menu()->addSeparator();
    m_menuFromToolBar->menu()->addAction(showMenuBar);

    networkIndicator->setMenu(netPolicyMenu);
    m_netToolbarDefaultAction = new QAction(this);
    networkIndicator->setDefaultAction(m_netToolbarDefaultAction);
    connect(m_netToolbarDefaultAction, &QAction::triggered, networkIndicator, &QToolButton::showMenu);
    connect(netOffline, &QAction::toggled, this, &MainWindow::updateNetworkIndication);
    connect(netExpensive, &QAction::toggled, this, &MainWindow::updateNetworkIndication);
    connect(netOnline, &QAction::toggled, this, &MainWindow::updateNetworkIndication);

    addToolBar(Qt::LeftToolBarArea, m_mainToolbar);
    m_mainToolbar->actions().constLast()->setVisible(true); // initial state to complement the default of the QMenuBar's visibility
    menuBar()->hide();

#undef ADD_ACTION
}

void MainWindow::createWidgets()
{
    m_connectionStatus = new QLabel(statusBar());
    m_connectionStatus->setObjectName(QStringLiteral("connectionStatus"));
    m_connectionStatus->setAccessibleName(i18nc("@label", "Connection status"));
    m_connectionStatus->setText(i18nc("@info:status", "Not connected"));
    statusBar()->addPermanentWidget(m_connectionStatus);

    m_backgroundAlerts = new QToolButton(statusBar());
    m_backgroundAlerts->setObjectName(QStringLiteral("backgroundAccountAlerts"));
    m_backgroundAlerts->setAccessibleName(i18nc("@label", "Other accounts needing attention"));
    m_backgroundAlerts->setAutoRaise(true);
    m_backgroundAlerts->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_backgroundAlerts->setIcon(UiUtils::SymbolicIcon::fromTheme(QStringLiteral("dialog-warning")));
    m_backgroundAlerts->hide();
    connect(m_backgroundAlerts, &QToolButton::clicked, this, [this]() {
        switchAccount(m_backgroundAlerts->property("accountId").toString());
    });
    // Left of the shown account's connection status
    statusBar()->insertPermanentWidget(0, m_backgroundAlerts);

    m_activityButton = new ActivityButton(m_mailAutomation->activity(), statusBar());
    m_activityButton->setObjectName(QStringLiteral("activityButton"));
    connect(m_activityButton, &QToolButton::clicked, this, &MainWindow::showActivity);
    statusBar()->insertPermanentWidget(0, m_activityButton);

    m_outboxButton = new QToolButton(statusBar());
    m_outboxButton->setObjectName(QStringLiteral("outboxButton"));
    m_outboxButton->setAutoRaise(true);
    m_outboxButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_outboxButton->setIcon(UiUtils::SymbolicIcon::fromTheme(QStringLiteral("mail-folder-outbox")));
    connect(m_outboxButton, &QToolButton::clicked, this, &MainWindow::showOutbox);
    statusBar()->insertPermanentWidget(0, m_outboxButton);
    updateOutboxButton();

    m_oauthSignIn = new QToolButton(statusBar());
    m_oauthSignIn->setObjectName(QStringLiteral("oauthSignInButton"));
    m_oauthSignIn->setAutoRaise(true);
    m_oauthSignIn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_oauthSignIn->setIcon(UiUtils::SymbolicIcon::fromTheme(QStringLiteral("dialog-password")));
    m_oauthSignIn->setText(i18nc("@action:button", "Sign In…"));
    m_oauthSignIn->setToolTip(i18nc("@info:tooltip", "Sign in to this account again in your web browser"));
    m_oauthSignIn->hide();
    connect(m_oauthSignIn, &QToolButton::clicked, this, [this]() { signInAccount(m_accounts->activeAccount()); });
    statusBar()->insertPermanentWidget(1, m_oauthSignIn);

    // The state of the GUI is only saved after a certain time has passed. This is just an optimization to make sure
    // we do not hit the disk continually when e.g. resizing some random widget.
    m_delayedStateSaving = new QTimer(this);
    m_delayedStateSaving->setInterval(1000);
    m_delayedStateSaving->setSingleShot(true);
    connect(m_delayedStateSaving, &QTimer::timeout, this, &MainWindow::saveSizesAndState);

    m_listStatusTimer = new QTimer(this);
    m_listStatusTimer->setInterval(50);
    m_listStatusTimer->setSingleShot(true);
    connect(m_listStatusTimer, &QTimer::timeout, this, &MainWindow::updateMessageListStatus);
    connect(qApp, &QCoreApplication::aboutToQuit, this, &MainWindow::flushUiState);

    mboxTree = new MailBoxTreeView(nullptr, m_settings);
    mboxTree->setAccessibleName(i18nc("@info:accessible", "Folders"));
    mboxTree->setItemDelegate(new DensityDelegate(mboxTree));
    mboxTree->setDesiredExpansion(expandedMailboxes());
    connect(mboxTree, &QWidget::customContextMenuRequested, this, &MainWindow::showContextMenuMboxTree);
    connect(mboxTree, &MailBoxTreeView::mailboxExpansionChanged, this, [this](const QStringList &mailboxNames) {
        accountSettings()->setValue(Common::SettingsNames::guiExpandedMailboxes, mailboxNames);
    });

    msgListWidget = new MessageListWidget(nullptr, m_favoriteTags, m_imapAccess->credentialAccountId());
    m_notificationBar = new InlineNotificationBar(msgListWidget);
    m_notificationBar->setObjectName(QStringLiteral("messageNotificationBar"));
    // Between the quick search field and the list, so that it does not cover any message
    if (auto *listLayout = qobject_cast<QVBoxLayout *>(msgListWidget->layout()))
        listLayout->insertWidget(1, m_notificationBar);
    m_actionStopSearch = new QAction(quickActionIcon(QStringLiteral("process-stop")), i18nc("@action:button", "Stop"), this);
    m_actionStopSearch->setObjectName(QStringLiteral("action_stop_search"));
    m_actionStopSearch->setToolTip(i18nc("@info:tooltip", "Stop searching and keep the messages found so far"));
    m_actionStopSearch->setVisible(false);
    connect(m_actionStopSearch, &QAction::triggered, this, &MainWindow::stopSearchAcross);
    msgListWidget->addStatusButton(m_actionStopSearch)->setObjectName(QStringLiteral("stopSearchButton"));
    m_actionRetrySearch = new QAction(quickActionIcon(QStringLiteral("view-refresh")), i18nc("@action:button", "Retry"), this);
    m_actionRetrySearch->setObjectName(QStringLiteral("action_retry_search"));
    m_actionRetrySearch->setToolTip(i18nc("@info:tooltip", "Search the places which could not be searched again"));
    m_actionRetrySearch->setVisible(false);
    connect(m_actionRetrySearch, &QAction::triggered, this, &MainWindow::retrySearchAcross);
    msgListWidget->addStatusButton(m_actionRetrySearch)->setObjectName(QStringLiteral("retrySearchButton"));
    m_actionSearchServer =
        new QAction(quickActionIcon(QStringLiteral("network-server")), i18nc("@action:button", "Search Server"), this);
    m_actionSearchServer->setObjectName(QStringLiteral("action_search_server"));
    m_actionSearchServer->setToolTip(
        i18nc("@info:tooltip", "Continue the search on the server, adding the messages which are not indexed on this computer"));
    m_actionSearchServer->setVisible(false);
    connect(m_actionSearchServer, &QAction::triggered, this, &MainWindow::continueSearchOnServer);
    msgListWidget->addStatusButton(m_actionSearchServer)->setObjectName(QStringLiteral("searchServerButton"));
    m_actionRevealInFolder = new QAction(UiUtils::loadIcon(QStringLiteral("go-jump")), i18nc("@action:inmenu", "Show in &Folder"), this);
    m_actionRevealInFolder->setObjectName(QStringLiteral("action_reveal_in_folder"));
    m_actionRevealInFolder->setToolTip(i18nc("@info:tooltip", "Show the folder of the message and select the message there"));
    connect(m_actionRevealInFolder, &QAction::triggered, this, [this]() {
        const QModelIndex current = msgListWidget->tree->currentIndex();
        if (current.isValid())
            revealInFolder(current);
    });
    msgListWidget->tree->setContextMenuPolicy(Qt::CustomContextMenu);
    msgListWidget->tree->setAlternatingRowColors(true);
    msgListWidget->setRawSearchEnabled(m_settings->value(Common::SettingsNames::guiAllowRawSearch).toBool());
    connect (msgListWidget, &MessageListWidget::rawSearchSettingChanged, this, &MainWindow::saveRawStateSetting);

    connect(msgListWidget->tree, &QWidget::customContextMenuRequested, this, &MainWindow::showContextMenuMsgListTree);
    connect(msgListWidget->tree, &QAbstractItemView::activated, this, &MainWindow::msgListClicked);
    connect(msgListWidget->tree, &QAbstractItemView::clicked, this, &MainWindow::msgListClicked);
    for (auto signal : {&QAbstractItemView::activated, &QAbstractItemView::clicked}) {
        connect(msgListWidget->tree, signal, this, [this] {
            if (messageListNarrowed())
                m_pickedWhileNarrowed = true;
        });
    }
    connect(msgListWidget->tree, &QAbstractItemView::doubleClicked, this, &MainWindow::openCompleteMessageWidget);
    connect(msgListWidget->tree, &MsgListView::currentReplacedAfterRemoval, this, &MainWindow::slotMessageReplacedAfterRemoval);
    connect(msgListWidget, &MessageListWidget::requestingSearch, this, &MainWindow::slotSearchRequested);
    connect(msgListWidget, &MessageListWidget::clearAllRequested, this, &MainWindow::clearSearchAndFilters);
    connect(msgListWidget, &MessageListWidget::facetsChanged, this, &MainWindow::applySearchFacets);
    connect(msgListWidget->tree->header(), &QHeaderView::sectionMoved, m_delayedStateSaving, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(msgListWidget->tree->header(), &QHeaderView::sectionResized, m_delayedStateSaving, static_cast<void (QTimer::*)()>(&QTimer::start));

    msgListWidget->tree->installEventFilter(this);

#ifdef IMPRESARIO_HAVE_WEBENGINE
    m_messageWidget = new CompleteMessageWidget(this, m_settings, m_pluginManager, m_favoriteTags);
    // The window's Find action hands the key to the reader while it has focus; two actions with the same key in one
    // window would be ambiguous, so that neither of them would run
    m_messageWidget->messageView->findAction()->setShortcut(QKeySequence());
    const auto accountName = [this](const QModelIndex &message) { return messageAccountName(message); };
    m_messageWidget->messageView->setAccountResolver(accountName);
    m_messageWidget->messageView->setSettingResolver(
            [this](const QString &key, const QModelIndex &message) { return messageSetting(key, message); });
    connect(m_messageWidget->messageView, &MessageView::messageChanged, this, &MainWindow::scrollMessageUp);
    connect(m_messageWidget->messageView, &MessageView::messageChanged, this, &MainWindow::slotUpdateMessageActions);
    connect(m_messageWidget->messageView, &MessageView::messageChanged, this, [this]() {
        const bool shown = m_messageWidget->messageView->currentMessage().isValid();
        m_actionMessageToCalendar->setEnabled(shown);
        m_actionMessageToNote->setEnabled(shown);
    });
    // Replies to the conversation being read are not announced
    connect(m_messageWidget->messageView, &MessageView::messageChanged, this, [this]() {
        const QModelIndex message = m_messageWidget->messageView->currentMessage();
        QStringList ids;
        if (message.isValid()) {
            const QString id = QString::fromUtf8(message.data(Imap::Mailbox::RoleMessageMessageId).toByteArray());
            if (!id.isEmpty())
                ids << id;
            const auto inReplyTo = message.data(Imap::Mailbox::RoleMessageInReplyTo).value<QList<QByteArray>>();
            for (const QByteArray &parent : inReplyTo)
                ids << QString::fromUtf8(parent);
        }
        m_mailAutomation->setReading(ids);
    });
    connect(m_messageWidget->messageView, &MessageView::linkHovered, this, [this](const QString &url, const QString &text) {
        // A tooltip still waiting for the previous link is dropped
        ++m_hoveredLinkGeneration;
        if (url.isEmpty()) {
            QToolTip::hideText();
        } else {
            // indirection due to https://bugs.kde.org/show_bug.cgi?id=363783
            const unsigned generation = m_hoveredLinkGeneration;
            QTimer::singleShot(250, this, [this, url, text, generation]() {
                if (generation == m_hoveredLinkGeneration)
                    QToolTip::showText(QCursor::pos(), linkTargetDescription(QUrl(url), text));
            });
        }
    });
    connect(m_messageWidget->messageView, &MessageView::transferError, this, &MainWindow::slotDownloadTransferError);
    connect(m_messageWidget->messageView, &MessageView::fileSaved, this, &MainWindow::slotFileSaved);
    // Space pages through the open message, then continues with the next unread one
    connect(m_messageWidget, &CompleteMessageWidget::pagedPastEnd, this, [this]() {
        if (m_messageWidget->messageView->currentMessage().isValid())
            slotNextUnread();
    });

    m_attachmentPanel = new AttachmentPanel();
    m_attachmentPanel->setNetworkAccessManager(m_messageWidget->messageView->networkAccessManager());
    connect(m_messageWidget->messageView, &MessageView::messageModelChanged, m_attachmentPanel, &AttachmentPanel::setMessageModel);
    connect(m_attachmentPanel, &AttachmentPanel::transferError, this, &MainWindow::slotDownloadTransferError);
    connect(m_attachmentPanel, &AttachmentPanel::fileSaved, this, &MainWindow::slotFileSaved);
    connect(m_attachmentPanel, &AttachmentPanel::findSavedCopiesRequested, this, &MainWindow::findSavedFiles);
    connect(m_attachmentPanel, &AttachmentPanel::saveToNextcloudRequested, this, &MainWindow::saveAttachmentToNextcloud);

    m_messageDetailsPanel = new MessageDetailsPanel();
    m_messageDetailsPanel->setAccountResolver(accountName);
    connect(m_messageWidget->messageView, &MessageView::messageModelChanged, m_messageDetailsPanel, &MessageDetailsPanel::setMessageModel);
    m_contactPanel = new ContactPanel();
    m_relatedMessagesPanel = new RelatedMessagesPanel();
    connect(m_messageWidget->messageView, &MessageView::messageChanged, this, &MainWindow::syncCurrentTab);
    for (MessageContextPanel *panel : std::initializer_list<MessageContextPanel *>{m_messageDetailsPanel, m_contactPanel, m_relatedMessagesPanel}) {
        connect(m_messageWidget->messageView, &MessageView::messageChanged, panel, [this, panel]() {
            panel->setMessage(m_messageWidget->messageView->currentMessage());
        });
    }
    connect(m_contactPanel, &ContactPanel::composeRequested, this, &MainWindow::slotComposeMailUrl);
    connect(m_contactPanel, &ContactPanel::manageContactRequested, this, &MainWindow::slotManageContact);
    connect(m_contactPanel, &ContactPanel::addContactRequested, this, &MainWindow::slotAddContact);
    connect(m_relatedMessagesPanel, &RelatedMessagesPanel::messageActivated, this, &MainWindow::activateRelatedMessage);
    const auto messageAccount = [this](const QModelIndex &message) {
        return accountOfModel(qobject_cast<const Imap::Mailbox::Model *>(Imap::deproxifiedIndex(message).model()));
    };
    m_relatedMessagesPanel->setAccountResolver(messageAccount);
    m_relatedMessagesPanel->setServerSearchGuard([this, messageAccount](const QModelIndex &message) {
        const QString accountId = messageAccount(message);
        if (accountId.isEmpty())
            return i18nc("@info", "The account of the message is not known");
        if (const Imap::Mailbox::FolderSearch *search = m_folderSearches.value(accountId); search && search->isRunning())
            return i18nc("@info", "The server is searched once the search across folders is done");
        if (m_agendaSearch && m_agendaSearch->isRunning() && m_agendaSearchItem.accountId == accountId)
            return i18nc("@info", "The server is searched once the message from My Day is open");
        return QString();
    });
    m_relatedMessagesPanel->setConfig(KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")));
    connect(m_relatedMessagesPanel->serverSearch(), &UiUtils::RelatedSearch::selectionChanged, this,
            [this](Imap::Mailbox::Model *model) { reselectShownFolder(m_accountAccess.value(accountOfModel(model))); });
    connect(m_relatedMessagesPanel, &RelatedMessagesPanel::chooseFoldersRequested, this, &MainWindow::chooseRelatedFolders);
    m_searchResultsPanel = new SearchResultsPanel(m_favoriteTags);
    m_searchResultsPanel->setStateConfig(KSharedConfig::openStateConfig());
    connect(m_searchResultsPanel, &SearchResultsPanel::messageActivated, this, &MainWindow::activateRelatedMessage);
    connect(m_searchResultsPanel, &SearchResultsPanel::revealRequested, this, &MainWindow::revealInFolder);
    connect(m_searchResultsPanel, &SearchResultsPanel::stopRequested, this, &MainWindow::stopSearchAcross);
    connect(m_searchResultsPanel, &SearchResultsPanel::retryRequested, this, &MainWindow::retrySearchAcross);
    connect(m_searchResultsPanel, &SearchResultsPanel::searchServerRequested, this, &MainWindow::continueSearchOnServer);
    connect(m_searchResultsPanel, &SearchResultsPanel::enabledChanged, this, &MainWindow::moveSearchResults);
    m_agendaMessagesTimer = new QTimer(this);
    m_agendaMessagesTimer->setSingleShot(true);
    m_agendaMessagesTimer->setInterval(500);
    connect(m_agendaMessagesTimer, &QTimer::timeout, this, &MainWindow::refreshAgendaMessages);
    m_agendaPanel = new AgendaPanel();
    connect(m_agendaPanel, &AgendaPanel::messageActivated, this, &MainWindow::openAgendaMessage);
    connect(m_agendaPanel, &AgendaPanel::joinRequested, this, &MainWindow::joinMeeting);
    connect(m_agendaPanel, &AgendaPanel::calendarRequested, this, &MainWindow::openCalendarApplication);
    connect(m_agendaPanel, &AgendaPanel::enabledChanged, this, &MainWindow::updateAgendaSource);
    m_embeddedPanels = {m_attachmentPanel, m_messageDetailsPanel, m_contactPanel, m_relatedMessagesPanel, m_searchResultsPanel,
                        m_agendaPanel};
#else
    m_messageWidget = new QLabel(
        i18nc("@info reader placeholder in builds without a web engine", "Messages cannot be shown in this build."));
#endif

    m_readerColumn = new QSplitter(Qt::Vertical);
    m_readerColumn->setObjectName(QStringLiteral("readerColumn"));
    m_readerColumn->setChildrenCollapsible(false);
    m_readerColumn->addWidget(m_messageWidget);
    m_readerColumn->setStretchFactor(0, 1);
    connect(m_readerColumn, &QSplitter::splitterMoved, m_delayedStateSaving, static_cast<void (QTimer::*)()>(&QTimer::start));

    createShell();
    for (EmbeddedPanel *panel : std::as_const(m_embeddedPanels)) {
        connect(panel, &EmbeddedPanel::relevanceChanged, this, [this]() {
            updateShellRegions();
        });
        connect(panel, &EmbeddedPanel::placementChanged, this, [this, panel]() {
            // A panel starts from its size hint in its new region, which may also run the other way
            m_panelExtents.remove(panel);
            rememberShellExtents();
            placeEmbeddedPanels();
            updateShellRegions(false);
        });
    }
    placeEmbeddedPanels();

    allDock = new QDockWidget(tr("Everything"), this);
    allDock->setObjectName(QStringLiteral("allDock"));
    allTree = new QTreeView(allDock);
    allDock->hide();
    allTree->setUniformRowHeights(true);
    allTree->setHeaderHidden(true);
    allDock->setWidget(allTree);
    addDockWidget(Qt::LeftDockWidgetArea, allDock);
    taskDock = new QDockWidget(tr("IMAP Tasks"), this);
    taskDock->setObjectName(QStringLiteral("taskDock"));
    taskTree = new QTreeView(taskDock);
    taskDock->hide();
    taskTree->setHeaderHidden(true);
    taskDock->setWidget(taskTree);
    addDockWidget(Qt::LeftDockWidgetArea, taskDock);
    mailMimeDock = new QDockWidget(tr("MIME Tree"), this);
    mailMimeDock->setObjectName(QStringLiteral("mailMimeDock"));
    mailMimeTree = new QTreeView(mailMimeDock);
    mailMimeDock->hide();
    mailMimeTree->setUniformRowHeights(true);
    mailMimeTree->setHeaderHidden(true);
    mailMimeDock->setWidget(mailMimeTree);
    addDockWidget(Qt::RightDockWidgetArea, mailMimeDock);
#ifdef IMPRESARIO_HAVE_WEBENGINE
    connect(m_messageWidget->messageView, &MessageView::messageModelChanged, this, &MainWindow::slotMessageModelChanged);
#endif
    protocolLoggerDock = new QDockWidget(tr("Protocol log"), this);
    protocolLoggerDock->setObjectName(QStringLiteral("protocolLoggerDock"));
    protocolLogger = new ProtocolLoggerWidget(protocolLoggerDock);
    protocolLoggerDock->hide();
    protocolLoggerDock->setWidget(protocolLogger);
    addDockWidget(Qt::BottomDockWidgetArea, protocolLoggerDock);

    busyParsersIndicator = new TaskProgressIndicator(this);
}

void MainWindow::setupModels(bool reconnect)
{
    if (reconnect) {
        m_imapAccess->reloadConfiguration();
        m_imapAccess->doConnect();
        monitorAccount(m_accounts->activeAccount());
    }

#ifdef IMPRESARIO_HAVE_WEBENGINE
    m_messageWidget->messageView->setNetworkWatcher(qobject_cast<Imap::Mailbox::NetworkWatcher*>(m_imapAccess->networkWatcher()));
    m_relatedMessagesPanel->setMessageList(m_imapAccess->msgListModel());
    for (const QMetaObject::Connection &connection : std::as_const(m_agendaListConnections))
        disconnect(connection);
    m_agendaListConnections.clear();
    if (QAbstractItemModel *list = m_imapAccess->msgListModel()) {
        m_agendaListConnections = {
            connect(list, &QAbstractItemModel::dataChanged, this, &MainWindow::scheduleAgendaMessages),
            connect(list, &QAbstractItemModel::rowsInserted, this, &MainWindow::scheduleAgendaMessages),
            connect(list, &QAbstractItemModel::rowsRemoved, this, &MainWindow::scheduleAgendaMessages),
            connect(list, &QAbstractItemModel::modelReset, this, &MainWindow::scheduleAgendaMessages),
            connect(list, &QAbstractItemModel::layoutChanged, this, &MainWindow::scheduleAgendaMessages),
        };
    }
    scheduleAgendaMessages();
#endif

    auto realThreadingModel = qobject_cast<Imap::Mailbox::ThreadingMsgListModel*>(m_imapAccess->threadingMsgListModel());
    Q_ASSERT(realThreadingModel);
    auto realMsgListModel = qobject_cast<Imap::Mailbox::MsgListModel*>(m_imapAccess->msgListModel());
    Q_ASSERT(realMsgListModel);

    prettyMboxModel = new Imap::Mailbox::PrettyMailboxModel(this, qobject_cast<QAbstractItemModel *>(m_imapAccess->mailboxModel()));
    prettyMboxModel->setObjectName(QStringLiteral("prettyMboxModel"));
    connect(realThreadingModel, &Imap::Mailbox::ThreadingMsgListModel::sortingFailed,
            msgListWidget, &MessageListWidget::slotSortingFailed);
    connect(realThreadingModel, &Imap::Mailbox::ThreadingMsgListModel::sortingFailed,
            this, &MainWindow::scheduleMessageListStatus);
    prettyMsgListModel = new Imap::Mailbox::PrettyMsgListModel(this);
    prettyMsgListModel->setSourceModel(m_imapAccess->threadingMsgListModel());
    prettyMsgListModel->setObjectName(QStringLiteral("prettyMsgListModel"));
    {
        // The defaults of all accounts are the threading preference and the sorting of the saved header
        UiUtils::MessageListView global;
        global.threading = m_settings->value(Common::SettingsNames::guiMsgListShowThreading).toBool();
        if (m_globalSort) {
            global.sortColumn = m_globalSort->first;
            global.sortOrder = m_globalSort->second;
        }
        m_viewStore = UiUtils::ViewStateStore();
        m_viewStore.load(*accountSettings());
        m_viewStore.setGlobalDefault(global);
        m_viewMailbox.clear();
        // The message list of an account which was shown before still has its folder open
        m_positionMailbox = currentMailboxName();
        m_livePosition = UiUtils::FolderPosition();
        m_unnarrowedMailbox.clear();
        m_unnarrowedPosition = UiUtils::FolderPosition();
        m_pickedWhileNarrowed = false;
    }
    applyMessageListFilters();
    if (msgListWidget)
        prettyMsgListModel->setFacets(msgListWidget->facets());
    connect(prettyMsgListModel, &Imap::Mailbox::PrettyMsgListModel::filtersChanged, this, &MainWindow::syncMessageListFilterUi);
    connect(prettyMsgListModel, &QAbstractItemModel::rowsInserted, this, &MainWindow::scheduleMessageListStatus);
    connect(prettyMsgListModel, &QAbstractItemModel::rowsRemoved, this, &MainWindow::scheduleMessageListStatus);
    connect(prettyMsgListModel, &QAbstractItemModel::modelReset, this, &MainWindow::scheduleMessageListStatus);
    connect(prettyMsgListModel, &QAbstractItemModel::layoutChanged, this, &MainWindow::scheduleMessageListStatus);
    connect(prettyMsgListModel, &QAbstractItemModel::dataChanged, this, &MainWindow::scheduleSavedSearchCount);
    m_groupingMsgListModel = new Imap::Mailbox::GroupingMsgListModel(this);
    m_groupingMsgListModel->setObjectName(QStringLiteral("groupingMsgListModel"));
    m_groupingMsgListModel->setFavoriteTags(m_favoriteTags);
    // New groups start collapsed in the view
    connect(m_groupingMsgListModel, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex &parent) {
        if (!parent.isValid())
            QMetaObject::invokeMethod(this, &MainWindow::applyGroupExpansion, Qt::QueuedConnection);
    });
    connect(m_groupingMsgListModel, &QAbstractItemModel::layoutChanged, this, &MainWindow::applyGroupExpansion,
            Qt::QueuedConnection);
    connect(m_groupingMsgListModel, &QAbstractItemModel::modelReset, this, &MainWindow::applyGroupExpansion,
            Qt::QueuedConnection);
    m_collapsedGroups.clear();
    connect(imapModel(), &Imap::Mailbox::Model::messageCountPossiblyChanged, this, &MainWindow::scheduleMessageListStatus);
    connect(realMsgListModel, &Imap::Mailbox::MsgListModel::mailboxChanged, this, &MainWindow::scheduleMessageListStatus);

    connect(mboxTree, &MailBoxTreeView::clicked,
            realMsgListModel,
            static_cast<void (Imap::Mailbox::MsgListModel::*)(const QModelIndex &)>(&Imap::Mailbox::MsgListModel::setMailbox));
    connect(mboxTree, &MailBoxTreeView::activated,
            realMsgListModel,
            static_cast<void (Imap::Mailbox::MsgListModel::*)(const QModelIndex &)>(&Imap::Mailbox::MsgListModel::setMailbox));
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::dataChanged, this, &MainWindow::scheduleMessageFlagsUpdate);
    // Sorting an empty list does not stick, so the folder's sorting applies once its messages are there
    connect(realMsgListModel, &Imap::Mailbox::MsgListModel::messagesAvailable, this, [this]() {
        if (!m_viewMailbox.isEmpty() && m_viewMailbox == currentMailboxName())
            applyMessageListSort();
    });
    connect(qobject_cast<Imap::Mailbox::MsgListModel*>(m_imapAccess->msgListModel()), &Imap::Mailbox::MsgListModel::messagesAvailable,
            this, &MainWindow::slotScrollToUnseenMessage);
    // When a resynchronization replaces all messages of the folder, the same messages are selected again
    connect(realMsgListModel, &QAbstractItemModel::rowsRemoved, this, [this, realMsgListModel](const QModelIndex &parent) {
        if (showsUnifiedList() || parent.isValid() || realMsgListModel->rowCount() || !m_messageRestorer || !m_livePosition.isValid() ||
            m_positionMailbox.isEmpty() || m_positionMailbox != currentMailboxName() || m_messageRestorer->isPending())
            return;
        m_messageRestorer->restorePosition(m_positionMailbox, m_livePosition.uidValidity, m_livePosition.uid,
                                           m_livePosition.anchorUid, m_livePosition.selectedUids);
    });
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::rowsInserted, msgListWidget, &MessageListWidget::slotAutoEnableDisableSearch);
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::rowsRemoved, msgListWidget, &MessageListWidget::slotAutoEnableDisableSearch);
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::rowsRemoved, this, &MainWindow::scheduleMessageFlagsUpdate);
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::layoutChanged, msgListWidget, &MessageListWidget::slotAutoEnableDisableSearch);
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::layoutChanged, this, &MainWindow::scheduleMessageFlagsUpdate);
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::modelReset, msgListWidget, &MessageListWidget::slotAutoEnableDisableSearch);
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::modelReset, this, &MainWindow::scheduleMessageFlagsUpdate);
    connect(realMsgListModel, &Imap::Mailbox::MsgListModel::mailboxChanged, this, &MainWindow::slotMailboxChanged);
    connect(realMsgListModel, &Imap::Mailbox::MsgListModel::mailboxChanged, this, &MainWindow::syncCurrentTab);
    // Reloading the folder list, as after going online again, replaces the shown folder with a new item
    connect(realMsgListModel, &Imap::Mailbox::MsgListModel::mailboxLost, this, [this](const QString &mailbox) {
        if (!m_unifiedInbox)
            mboxTree->restoreMailbox(mailbox);
    });
    connect(realMsgListModel, &Imap::Mailbox::MsgListModel::mailboxChanged, this, [this]() {
        commitFolderPosition();
        m_positionMailbox = currentMailboxName();
        m_livePosition = UiUtils::FolderPosition();
        m_unnarrowedMailbox.clear();
        m_unnarrowedPosition = UiUtils::FolderPosition();
        m_pickedWhileNarrowed = false;
        pruneFolderStates();
        applyMessageListView();
        updateMessageListScope();
        // Later, after a click on the folder has cancelled restoring another message
        QMetaObject::invokeMethod(this, [this]() { restoreFolderPosition(); }, Qt::QueuedConnection);
    });
    KConfigGroup selection(KSharedConfig::openStateConfig(), QStringLiteral("MailboxSelection"));
    KConfigGroup accountSelection = selection.group(m_imapAccess->credentialAccountId());
    connect(realMsgListModel, &Imap::Mailbox::MsgListModel::mailboxChanged, this,
            [this, accountSelection](const QModelIndex &mailbox) mutable {
        // The Unified Inbox opens the INBOX without the user choosing it
        if (m_unifiedInbox)
            return;
        const QString name = mailbox.data(Imap::Mailbox::RoleMailboxName).toString();
        if (!name.isEmpty()) {
            accountSelection.writeEntry("Mailbox", name);
            if (!accountSelection.sync())
                qWarning() << "Could not save mailbox selection";
        }
    });

    connect(imapModel(), &Imap::Mailbox::Model::alertReceived, this, &MainWindow::alertReceived);
    connect(imapModel(), &Imap::Mailbox::Model::imapError, this, &MainWindow::imapError);
    connect(imapModel(), &Imap::Mailbox::Model::networkError, this, &MainWindow::networkError);
    connect(imapModel(), &Imap::Mailbox::Model::authRequested, this, &MainWindow::authenticationRequested, Qt::QueuedConnection);
    connect(imapModel(), &Imap::Mailbox::Model::authAttemptFailed, this, [this](const QString &message) {
        m_ignoreStoredPassword = true;
        updateConnectionStatus(i18nc("@info:status", "Connection or authentication failed"));
        m_connectionError = true;
        m_connectionStatus->setToolTip(Qt::convertFromPlainText(message));
        m_connectionStatus->setAccessibleDescription(message);
    });

    connect(imapModel(), &Imap::Mailbox::Model::networkPolicyOffline, this, &MainWindow::networkPolicyOffline);
    connect(imapModel(), &Imap::Mailbox::Model::networkPolicyExpensive, this, &MainWindow::networkPolicyExpensive);
    connect(imapModel(), &Imap::Mailbox::Model::networkPolicyOnline, this, &MainWindow::networkPolicyOnline);
    connect(imapModel(), &Imap::Mailbox::Model::connectionStateChanged, this, [this](uint, const Imap::ConnectionState state) {
        if (state == Imap::CONN_STATE_AUTHENTICATED) {
            m_ignoreStoredPassword = false;
        }
    });
    connect(imapModel(), &Imap::Mailbox::Model::connectionStateChanged, this, &MainWindow::showConnectionStatus);

    connect(imapModel(), &Imap::Mailbox::Model::mailboxDeletionFailed, this, &MainWindow::slotMailboxDeleteFailed);
    connect(imapModel(), &Imap::Mailbox::Model::mailboxCreationFailed, this, &MainWindow::slotMailboxCreateFailed);
    connect(imapModel(), &Imap::Mailbox::Model::mailboxSyncFailed, this, &MainWindow::slotMailboxSyncFailed);
    connect(imapModel(), &Imap::Mailbox::Model::mailboxRenameSucceeded, this, &MainWindow::slotMailboxRenameSucceeded,
            Qt::UniqueConnection);
    connect(imapModel(), &Imap::Mailbox::Model::mailboxRenameFailed, this, &MainWindow::slotMailboxRenameFailed,
            Qt::UniqueConnection);
    connect(imapModel(), &Imap::Mailbox::Model::mailboxEmptied, this, &MainWindow::slotMailboxEmptied, Qt::UniqueConnection);
    connect(imapModel(), &Imap::Mailbox::Model::mailboxEmptyFailed, this, &MainWindow::slotMailboxEmptyFailed,
            Qt::UniqueConnection);
    // Feedback and pending renames belong to the account which was shown before
    m_renamedCurrentMailbox.clear();
    m_notificationBar->dismiss();
    if (m_deleteMovesToTrash) {
        QSignalBlocker blocker(m_deleteMovesToTrash);
        m_deleteMovesToTrash->setChecked(deleteMovesToTrash());
    }

    connect(imapModel(), &Imap::Mailbox::Model::logged, protocolLogger, &ProtocolLoggerWidget::log);
    connect(imapModel(), &Imap::Mailbox::Model::connectionStateChanged, protocolLogger, &ProtocolLoggerWidget::onConnectionClosed);

    auto nw = qobject_cast<Imap::Mailbox::NetworkWatcher *>(m_imapAccess->networkWatcher());
    Q_ASSERT(nw);
    connect(nw, &Imap::Mailbox::NetworkWatcher::reconnectAttemptScheduled,
            this, [this](const int timeout) {
            //~ singular Attempting to reconnect in %n second
            //~ plural Attempting to reconnect in %n seconds
            showStatusMessage(tr("Attempting to reconnect in %n second(s)..", nullptr, timeout/1000));
            });
    connect(nw, &Imap::Mailbox::NetworkWatcher::resetReconnectState, this, &MainWindow::slotResetReconnectState);

    connect(imapModel(), &Imap::Mailbox::Model::mailboxFirstUnseenMessage, this, &MainWindow::slotScrollToUnseenMessage);

    connect(imapModel(), &Imap::Mailbox::Model::capabilitiesUpdated, this, &MainWindow::slotCapabilitiesUpdated);

    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::modelReset, this, &MainWindow::slotUpdateWindowTitle);
    connect(imapModel(), &Imap::Mailbox::Model::messageCountPossiblyChanged, this, &MainWindow::slotUpdateWindowTitle);

    connect(prettyMsgListModel, &Imap::Mailbox::PrettyMsgListModel::sortingPreferenceChanged, this, &MainWindow::slotSortingConfirmed);

    //Imap::Mailbox::ModelWatcher* w = new Imap::Mailbox::ModelWatcher( this );
    //w->setModel( imapModel() );

    //ModelTest* tester = new ModelTest( prettyMboxModel, this ); // when testing, test just one model at time

    msgListWidget->tree->setModel(prettyMsgListModel);
    auto *messageRestorer = new MessageSelectionRestorer(msgListWidget->tree, prettyMsgListModel);
    m_messageRestorer = messageRestorer;
    connect(messageRestorer, &MessageSelectionRestorer::messageRestored, this, &MainWindow::msgListClicked);
    connect(messageRestorer, &MessageSelectionRestorer::restoreFailed, this, [this]() {
        // The message is gone, so nothing is selected and the list shows its newest messages
        if (QItemSelectionModel *selection = msgListWidget->tree->selectionModel())
            selection->clear();
        slotScrollToUnseenMessage();
    });
    connect(imapModel(), &QAbstractItemModel::dataChanged, messageRestorer, &MessageSelectionRestorer::scheduleCheck);
    connect(realMsgListModel, &Imap::Mailbox::MsgListModel::mailboxChanged, messageRestorer,
            [messageRestorer](const QModelIndex &) { messageRestorer->cancel(); });
    connect(mboxTree, &MailBoxTreeView::clicked, messageRestorer, [messageRestorer](const QModelIndex &) { messageRestorer->cancel(); });
    connect(mboxTree, &MailBoxTreeView::activated, messageRestorer, [messageRestorer](const QModelIndex &) { messageRestorer->cancel(); });
    mboxTree->setModel(prettyMboxModel);
    connect(mboxTree, &MailBoxTreeView::mailboxRestored, realMsgListModel, [this, realMsgListModel](const QModelIndex &mailbox) {
        // The folder from the last session is the one to go back to from the Unified Inbox
        if (m_unifiedInbox) {
            m_unifiedReturnMailbox = mailbox.data(Imap::Mailbox::RoleMailboxName).toString();
            return;
        }
        realMsgListModel->setMailbox(mailbox);
    });
    connect(mboxTree, &MailBoxTreeView::mailboxRestored, messageRestorer,
            [this, messageRestorer, accountSelection](const QModelIndex &mailbox) {
        if (m_unifiedInbox)
            return;
        const QString name = mailbox.data(Imap::Mailbox::RoleMailboxName).toString();
        // A message which was asked for wins over the one selected last time
        if (isRevealing(name))
            return;
        if (name == accountSelection.readEntry("MessageMailbox", QString())) {
            messageRestorer->restore(name, accountSelection.readEntry("MessageUidValidity", 0u),
                                     accountSelection.readEntry("MessageUid", 0u));
        }
    });
    mboxTree->restoreMailbox(accountSelection.readEntry("Mailbox", QString()));
    connectMessageListSelection();
    applyMessageListView();
    updateMessageListScope();
    if (!m_positionMailbox.isEmpty())
        QMetaObject::invokeMethod(this, [this]() { restoreFolderPosition(); }, Qt::QueuedConnection);
    if (m_actionSortByRelevance)
        updateRelevanceSorting(realThreadingModel->currentSearchCondition());
    scheduleMessageListStatus();

    allTree->setModel(imapModel());
    taskTree->setModel(imapModel()->taskModel());
    connect(imapModel()->taskModel(), &QAbstractItemModel::layoutChanged, taskTree, &QTreeView::expandAll);
    connect(imapModel()->taskModel(), &QAbstractItemModel::modelReset, taskTree, &QTreeView::expandAll);
    connect(imapModel()->taskModel(), &QAbstractItemModel::rowsInserted, taskTree, &QTreeView::expandAll);
    connect(imapModel()->taskModel(), &QAbstractItemModel::rowsRemoved, taskTree, &QTreeView::expandAll);
    connect(imapModel()->taskModel(), &QAbstractItemModel::rowsMoved, taskTree, &QTreeView::expandAll);

    busyParsersIndicator->setImapModel(imapModel());
    connect(imapModel(), &Imap::Mailbox::Model::specialFoldersChanged, this, &MainWindow::updateGoToFolderActions,
            Qt::UniqueConnection);
    updateGoToFolderActions();
    // The account's index is new after switching accounts or saving the account settings
    if (m_searchIndexDialog)
        m_searchIndexDialog->setIndexer(m_imapAccess->searchIndexer(), m_accounts->displayName(m_accounts->activeAccount()));

    auto accountIconName = accountSettings()->value(Common::SettingsNames::imapAccountIcon).toString();
    if (accountIconName.isEmpty()) {
        qApp->setWindowIcon(QIcon(QStringLiteral(":/icons/plasma-impresario.svg")));
    } else if (accountIconName.contains(QDir::separator())) {
        // Absolute paths are OK for users, but unsupported by our icon loader
        qApp->setWindowIcon(QIcon(accountIconName));
    } else {
        qApp->setWindowIcon(UiUtils::loadIcon(accountIconName));
    }
}

void MainWindow::createSysTray()
{
    if (m_trayIcon)
        return;

    m_trayIcon = new QSystemTrayIcon(this);
    handleTrayIconChange();

    QAction *showAction = new QAction(i18nc("@action:inmenu", "&Show Plasma Impresario"), m_trayIcon);
    connect(showAction, &QAction::triggered, this, &MainWindow::showMainWindow);
    // Quitting ends every background task, also while the window is closed
    QAction* quitAction = new QAction(i18nc("@action:inmenu", "&Quit"), m_trayIcon);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    QMenu *trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(showAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
    m_trayIcon->setContextMenu(trayIconMenu);

    // QMenu cannot be a child of QSystemTrayIcon, and we don't want the QMenu in MainWindow scope.
    connect(m_trayIcon, &QObject::destroyed, trayIconMenu, &QObject::deleteLater);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::slotIconActivated);
    connect(imapModel(), &Imap::Mailbox::Model::messageCountPossiblyChanged, this, &MainWindow::handleTrayIconChange);
    m_trayIcon->setVisible(true);
    m_trayIcon->show();
}

void MainWindow::removeSysTray()
{
    delete m_trayIcon;
    m_trayIcon = nullptr;
}

void MainWindow::keepPrivateFilesTidy()
{
    // Caches written before they were kept private are tightened once at startup, away from the GUI thread
    // The paths are looked up here, since looking them up is not thread-safe
    const QString cache = Common::writablePath(Common::LOCATION_CACHE);
    const QString opened = Common::PrivateFiles::openedAttachmentsFolder();
    const QString fetched = Common::PrivateFiles::fetchedAttachmentsFolder();
    // Copies fetched for a message being composed before this started were left behind when the application ended
    // unexpectedly; those of a composer opened meanwhile are newer
    const QDateTime started = QDateTime::currentDateTimeUtc();
    const bool hasProfile = !Common::Application::profileName().isEmpty();
    QThreadPool::globalInstance()->start([cache, opened, fetched, started, hasProfile]() {
        namespace Private = Common::PrivateFiles;
        // A profile's cache is inside the application's, which holds the other profiles' caches
        if (hasProfile && QFileInfo::exists(cache))
            Private::makeFolder(QFileInfo(QDir::cleanPath(cache)).path());
        Private::tighten(cache);
        Private::removeOldOpenedAttachments(opened);
        Private::removeCopiesChangedBefore(fetched, started);
    });
    // Running in the background can last for days, so the copies of opened attachments are checked every hour
    auto *timer = new QTimer(this);
    timer->setInterval(std::chrono::hours(1));
    connect(timer, &QTimer::timeout, this, []() {
        Common::PrivateFiles::removeOldOpenedAttachments(Common::PrivateFiles::openedAttachmentsFolder());
    });
    timer->start();
}

bool MainWindow::runsInBackground() const
{
    return UiUtils::BackgroundRunning::runsInBackground(*KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")));
}

void MainWindow::slotToggleSysTray()
{
    bool showSystray = m_settings->value(Common::SettingsNames::guiShowSystray, QVariant(true)).toBool();
    if (showSystray && !m_trayIcon && QSystemTrayIcon::isSystemTrayAvailable()) {
        createSysTray();
    } else if (!showSystray && m_trayIcon) {
        removeSysTray();
    }
    // Only running in the background keeps the application alive without a window, not the tray icon alone
    qApp->setQuitOnLastWindowClosed(!runsInBackground());
}

void MainWindow::handleTrayIconChange()
{
    if (!m_trayIcon)
        return;

    const bool isOffline = qobject_cast<Imap::Mailbox::NetworkWatcher *>(m_imapAccess->networkWatcher())->effectiveNetworkPolicy()
            == Imap::Mailbox::NETWORK_OFFLINE;
    auto pixmap = qApp->windowIcon()
                .pixmap(QSize(32, 32), isOffline ? QIcon::Disabled : QIcon::Normal);
    QString tooltip;
    auto profileName = Common::Application::profileName();
    if (profileName.isEmpty()) {
        tooltip = QStringLiteral("Plasma Impresario");
    } else {
        tooltip = QStringLiteral("Plasma Impresario [%1]").arg(profileName);
    }

    int unreadCount = 0;
    bool numbersValid = false;

    auto watchingMode = settings()->value(Common::SettingsNames::watchedFoldersKey).toString();
    if (watchingMode == Common::SettingsNames::watchAll || watchingMode == Common::SettingsNames::watchSubscribed) {
        bool subscribedOnly = watchingMode == Common::SettingsNames::watchSubscribed;
        unreadCount = std::accumulate(UiUtils::QaimDfsIterator(m_imapAccess->mailboxModel()->index(0, 0), m_imapAccess->mailboxModel()),
                                      UiUtils::QaimDfsIterator(), 0, [subscribedOnly](const int acc, const QModelIndex &idx) {

            if (subscribedOnly && !idx.data(Imap::Mailbox::RoleMailboxIsSubscribed).toBool())
                return acc;

            auto x = idx.data(Imap::Mailbox::RoleUnreadMessageCount).toInt();
            if (x > 0) {
                return acc + x;
            } else {
                return acc;
            }
        });
        // only show stuff if there are some mailboxes, and if there are such messages
        numbersValid = m_imapAccess->mailboxModel()->hasChildren() && unreadCount > 0;

    } else {
        // just for the INBOX
        QModelIndex mailbox = imapModel()->index(1, 0, QModelIndex());
        if (mailbox.isValid() && mailbox.data(Imap::Mailbox::RoleMailboxName).toString() == QLatin1String("INBOX")
                && mailbox.data(Imap::Mailbox::RoleUnreadMessageCount).toInt() > 0) {
            unreadCount = mailbox.data(Imap::Mailbox::RoleUnreadMessageCount).toInt();
            numbersValid = true;
        }
    }

    if (numbersValid) {
        QFont f;
        f.setPixelSize(pixmap.height() * 0.59);
        f.setWeight(QFont::Bold);

        QString text = QString::number(unreadCount);
        QFontMetrics fm(f);
        if (unreadCount > 666) {
            // You just have too many messages.
            text = QStringLiteral("🐮");
            fm = QFontMetrics(f);
        } else if (fm.horizontalAdvance(text) > pixmap.width()) {
            f.setPixelSize(f.pixelSize() * pixmap.width() / fm.horizontalAdvance(text));
            fm = QFontMetrics(f);
        }

        QRect boundingRect = fm.tightBoundingRect(text);
        boundingRect.setWidth(boundingRect.width() + 2);
        boundingRect.setHeight(boundingRect.height() + 2);
        boundingRect.moveCenter(QPoint(pixmap.width() / 2, pixmap.height() / 2));
        boundingRect = boundingRect.intersected(pixmap.rect());

        QPainterPath path;
        path.addText(boundingRect.bottomLeft(), f, text);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QColor(255,255,255, 180));
        painter.setBrush(isOffline ? StateColors::text(StateColors::State::Negative) : QColor(Qt::black));
        painter.drawPath(path);

        //: This is a tooltip for the tray icon. It will be prefixed by something like "Trojita" or "Trojita [work]"
        //~ singular %n unread message
        //~ plural %n unread messages
        tooltip += tr(" - %n unread message(s)", nullptr, unreadCount);
    } else if (isOffline) {
        //: A tooltip suffix when offline. The prefix is something like "Trojita" or "Trojita [work]"
        tooltip += tr(" - offline");
    }
    m_trayIcon->setToolTip(tooltip);
    const bool symbolic = UiUtils::SettingsStore::value(*KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")),
                                                        m_settings, QStringLiteral("Appearance/SymbolicTrayIcon"))
                              .toBool();
    if (symbolic) {
        // The panel colours a symbolic icon as it does the other tray icons; the count is in the tooltip
        m_trayIcon->setIcon(UiUtils::SymbolicIcon::fromTheme(
            isOffline ? QStringLiteral("network-offline")
                      : numbersValid ? QStringLiteral("mail-unread-new") : QStringLiteral("mail-message")));
    } else {
        m_trayIcon->setIcon(QIcon(pixmap));
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    flushUiState();
    if (runsInBackground()) {
        const QString reopen = m_trayIcon && m_trayIcon->isVisible()
                ? i18nc("@info", "Plasma Impresario keeps running in the background, checking for new mail and showing "
                                 "notifications. Open it again from its icon in the system tray, or quit it there. Running "
                                 "in the background can be turned off in the settings.")
                : i18nc("@info", "Plasma Impresario keeps running in the background, checking for new mail and showing "
                                 "notifications. Open it again from the application launcher. Running in the background "
                                 "can be turned off in the settings.");
        Util::askForSomethingUnlessTold(i18nc("@title:window", "Plasma Impresario"), reopen,
                                        Common::SettingsNames::guiOnSystrayClose, QMessageBox::Ok, this, m_settings);
        hide();
        event->ignore();
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::ActivationChange && m_mailAutomation)
        m_mailAutomation->setWindowActive(isActiveWindow());
}

bool MainWindow::eventFilter(QObject *o, QEvent *e)
{
    // A hidden splitter receives its pending resize before it becomes visible, so also look at the show event
    if ((o == m_shellRow || o == m_shellBody) && (e->type() == QEvent::Resize || e->type() == QEvent::Show)) {
        applyPendingShellSizes(static_cast<QSplitter *>(o));
        return false;
    }
    if (auto *region = qobject_cast<QSplitter *>(o);
        region && m_panelRegionsPending.contains(region) && (e->type() == QEvent::Resize || e->type() == QEvent::Show)) {
        fitPanelRegion(region);
        return false;
    }
#ifdef IMPRESARIO_HAVE_WEBENGINE
    if (msgListWidget && o == msgListWidget->tree && m_messageWidget->messageView) {
        if (e->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(e);
            // Ctrl+Space toggles the selection of the current message instead
            if ((keyEvent->key() == Qt::Key_Space || keyEvent->key() == Qt::Key_Backspace)
                    && !(keyEvent->modifiers() & Qt::ControlModifier)) {
                QCoreApplication::sendEvent(m_messageWidget, keyEvent);
                return true;
            }
            return false;
        }
        return false;
    }
#endif
    if (msgListWidget && msgListWidget->tree && o == msgListWidget->tree->header()->viewport()) {
        // installed if sorting is not really possible.
        if (e->type() != QEvent::MouseButtonPress && e->type() != QEvent::MouseButtonRelease)
            return false;
        QWidget *header = static_cast<QWidget*>(o);
        QMouseEvent *mouse = static_cast<QMouseEvent*>(e);
        if (e->type() == QEvent::MouseButtonPress) {
            if (mouse->button() == Qt::LeftButton && header->cursor().shape() == Qt::ArrowCursor) {
                m_headerDragStart = mouse->pos();
            }
            return false;
        }
        if (e->type() == QEvent::MouseButtonRelease) {
            if (mouse->button() == Qt::LeftButton && header->cursor().shape() == Qt::ArrowCursor &&
               (m_headerDragStart - mouse->pos()).manhattanLength() < QApplication::startDragDistance()) {
                    m_actionSortDescending->toggle();
                    Qt::SortOrder order = m_actionSortDescending->isChecked() ? Qt::DescendingOrder : Qt::AscendingOrder;
                    msgListWidget->tree->header()->setSortIndicator(-1, order);
                    rememberMessageListSort(-1, order);
                    return true; // prevent regular click
            }
        }
    }
    return false;
}

void MainWindow::slotIconActivated(const QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        setVisible(!isVisible());
        if (isVisible())
            showMainWindow();
    }
}

void MainWindow::showMainWindow()
{
    activateForRequest(this);
}

void MainWindow::setActivationToken(const QString &token)
{
    m_activationToken.handOver(token, QDateTime::currentDateTimeUtc());
}

bool MainWindow::hasActivationToken() const
{
    return m_activationToken.isPending();
}

void MainWindow::saveUnsentMessages()
{
    const auto windows = QApplication::topLevelWidgets();
    for (QWidget *window : windows) {
        if (auto *composer = qobject_cast<ComposeWidget *>(window))
            composer->saveDraftNow();
    }
}

void MainWindow::activateForRequest(QWidget *window)
{
    window->setVisible(true);
    window->raise();
    window->activateWindow();
    const QString token = m_activationToken.take(QDateTime::currentDateTimeUtc());
    QWindow *handle = window->windowHandle();
    if (!handle)
        return;
    if (!token.isEmpty()) {
        if (KWindowSystem::isPlatformWayland()) {
            KWindowSystem::setCurrentXdgActivationToken(token);
        } else if (KWindowSystem::isPlatformX11()) {
            // KWindowSystem reads the startup ID of a request handled by a running instance from the environment
            qputenv("DESKTOP_STARTUP_ID", token.toLatin1());
            KWindowSystem::updateStartupId(handle);
            qunsetenv("DESKTOP_STARTUP_ID");
        }
    }
    // Without a token, Plasma may only mark the window as wanting attention instead of raising it
    KWindowSystem::activateWindow(handle);
}

void MainWindow::saveMessageSelection(const QModelIndex &index)
{
    // Messages of the Unified Inbox may belong to other accounts than the shown one
    if (showsUnifiedList())
        return;
    KConfigGroup selection(KSharedConfig::openStateConfig(), QStringLiteral("MailboxSelection"));
    KConfigGroup accountSelection = selection.group(m_imapAccess->credentialAccountId());
    const QString mailbox = index.data(Imap::Mailbox::RoleMailboxName).toString();
    const uint uidValidity = index.data(Imap::Mailbox::RoleMailboxUidValidity).toUInt();
    const uint uid = index.data(Imap::Mailbox::RoleMessageUid).toUInt();
    if (!mailbox.isEmpty() && uidValidity && uid) {
        accountSelection.writeEntry("MessageMailbox", mailbox);
        accountSelection.writeEntry("MessageUidValidity", uidValidity);
        accountSelection.writeEntry("MessageUid", uid);
    } else {
        accountSelection.deleteEntry("MessageMailbox");
        accountSelection.deleteEntry("MessageUidValidity");
        accountSelection.deleteEntry("MessageUid");
    }
    if (!accountSelection.sync())
        qWarning() << "Could not save message selection";
}

void MainWindow::slotMessageReplacedAfterRemoval(const QModelIndex &index)
{
    if (!index.isValid() || !index.data(Imap::Mailbox::RoleMessageUid).isValid())
        return;
    // Only messages of an IMAP mailbox can be remembered and shown
    if (!qobject_cast<const Imap::Mailbox::Model *>(Imap::deproxifiedIndex(index).model()))
        return;
    saveMessageSelection(index);
#ifdef IMPRESARIO_HAVE_WEBENGINE
    // As after keyboard navigation, the reader follows and marks the message as read according to the settings
    if ((m_messageWidget->isVisible() && !m_messageWidget->size().isEmpty()) || m_layoutMode == LAYOUT_ONE_AT_TIME)
        m_messageWidget->messageView->setMessage(index);
#endif
}

void MainWindow::msgListClicked(const QModelIndex &index)
{
    Q_ASSERT(index.isValid());

    // The header of a group is not a message
    if (index.data(Imap::Mailbox::RoleIsGroupHeader).toBool())
        return;

    if (qApp->keyboardModifiers() & Qt::ShiftModifier || qApp->keyboardModifiers() & Qt::ControlModifier)
        return;

    if (index.column() != Imap::Mailbox::MsgListModel::SEEN && index.column() != Imap::Mailbox::MsgListModel::FLAGGED)
        saveMessageSelection(index);
    if (! index.data(Imap::Mailbox::RoleMessageUid).isValid())
        return;
#ifdef IMPRESARIO_HAVE_WEBENGINE
    // Because it's quite possible that we have switched into another mailbox, make sure that we're in the "current" one so that
    // user will be notified about new arrivals, etc.
    QModelIndex translated = Imap::deproxifiedIndex(index);
    // In the Unified Inbox, the message may belong to another account than the shown one
    auto *messageModel = qobject_cast<Imap::Mailbox::Model *>(const_cast<QAbstractItemModel *>(translated.model()));
    if (!messageModel)
        messageModel = imapModel();
    messageModel->switchToMailbox(translated.parent().parent());

    if (index.column() == Imap::Mailbox::MsgListModel::SEEN) {
        if (!translated.data(Imap::Mailbox::RoleIsFetched).toBool())
            return;
        Imap::Mailbox::FlagsOperation flagOp = translated.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool() ?
                                               Imap::Mailbox::FLAG_REMOVE : Imap::Mailbox::FLAG_ADD;
        const bool read = flagOp == Imap::Mailbox::FLAG_ADD;
        actOnModel(messageModel, [&]() {
            trackFlagChange(
                QModelIndexList() << translated,
                [=](Imap::Mailbox::Model *model, const QModelIndexList &messages) {
                    return model->markMessagesRead(messages, flagOp);
                },
                QString(), QString(),
                [read](const QString &message) {
                    return read ? i18ncp("@info", "Could not mark %1 message as read: %2",
                                      "Could not mark %1 messages as read: %2", 1, message)
                                : i18ncp("@info", "Could not mark %1 message as unread: %2",
                                      "Could not mark %1 messages as unread: %2", 1, message);
                });
        });

        if (translated == m_messageWidget->messageView->currentMessage()) {
            m_messageWidget->messageView->stopAutoMarkAsRead();
        }
    } else if (index.column() == Imap::Mailbox::MsgListModel::FLAGGED) {
        if (!translated.data(Imap::Mailbox::RoleIsFetched).toBool())
            return;

        Imap::Mailbox::FlagsOperation flagOp = translated.data(Imap::Mailbox::RoleMessageIsMarkedFlagged).toBool() ?
                                               Imap::Mailbox::FLAG_REMOVE : Imap::Mailbox::FLAG_ADD;
        const bool flagged = flagOp == Imap::Mailbox::FLAG_ADD;
        actOnModel(messageModel, [&]() {
            trackFlagChange(
                QModelIndexList() << translated,
                [=](Imap::Mailbox::Model *model, const QModelIndexList &messages) {
                    return model->setMessageFlags(messages, Imap::Mailbox::FlagNames::flagged, flagOp);
                },
                QString(), QString(),
                [flagged](const QString &message) {
                    return flagged
                        ? i18ncp("@info", "Could not flag %1 message: %2", "Could not flag %1 messages: %2", 1, message)
                        : i18ncp("@info", "Could not remove the flag from %1 message: %2",
                              "Could not remove the flag from %1 messages: %2", 1, message);
                });
        });
    } else {
        if ((m_messageWidget->isVisible() && !m_messageWidget->size().isEmpty()) || m_layoutMode == LAYOUT_ONE_AT_TIME) {
            // isVisible() won't work, the splitter manipulates width, not the visibility state
            m_messageWidget->messageView->setMessage(index);
        }
        msgListWidget->tree->setCurrentIndex(index);
    }
#endif
}

void MainWindow::openCompleteMessageWidget()
{
#ifdef IMPRESARIO_HAVE_WEBENGINE
    const QModelIndex index = msgListWidget->tree->currentIndex();

    if (! index.data(Imap::Mailbox::RoleMessageUid).isValid())
        return;

    CompleteMessageWidget *widget = new CompleteMessageWidget(0, m_settings, m_pluginManager, m_favoriteTags);
    // The window has no parent, so it may outlive this one
    widget->messageView->setAccountResolver([window = QPointer<MainWindow>(this)](const QModelIndex &message) {
        return window ? window->messageAccountName(message) : QString();
    });
    widget->messageView->setSettingResolver(
            [window = QPointer<MainWindow>(this)](const QString &key, const QModelIndex &message) {
                return window ? window->messageSetting(key, message) : QVariant();
            });
    widget->messageView->setMessage(index);
    widget->messageView->setNetworkWatcher(qobject_cast<Imap::Mailbox::NetworkWatcher*>(m_imapAccess->networkWatcher()));
    widget->setFocusPolicy(Qt::StrongFocus);
    widget->setWindowTitle(index.data(Imap::Mailbox::RoleMessageSubject).toString());
    widget->setAttribute(Qt::WA_DeleteOnClose);
    QAction *closeAction = ShortcutHandler::instance()->createAction(QStringLiteral("action_messagewindow_close"), widget, SLOT(close()), widget);
    widget->addAction(closeAction);
    widget->show();
#endif
}

void MainWindow::showContextMenuMboxTree(const QPoint &position)
{
    QList<QAction *> actionList;
    const QModelIndex clicked = mboxTree->indexAt(position);
    if (clicked.isValid()) {
        if (m_tabsEnabled && clicked.data(Imap::Mailbox::RoleMailboxIsSelectable).toBool()) {
            m_newTabCandidate = UiUtils::MailTabs::Tab();
            m_newTabCandidate.accountId = currentAccountId();
            m_newTabCandidate.mailbox = clicked.data(Imap::Mailbox::RoleMailboxName).toString();
            actionList.append(m_actionOpenInNewTab);
        }
        actionList.append(createChildMailbox);
        m_renameMailbox->setEnabled(canRenameMailbox(clicked));
        actionList.append(m_renameMailbox);
        actionList.append(deleteCurrentMailbox);
        const auto specialUse = static_cast<Imap::Mailbox::SpecialUse>(clicked.data(Imap::Mailbox::RoleMailboxSpecialUse).toInt());
        if (specialUse == Imap::Mailbox::SpecialUse::Trash)
            actionList.append(m_emptyTrash);
        else if (specialUse == Imap::Mailbox::SpecialUse::Junk)
            actionList.append(m_emptyJunk);
        actionList.append(m_actionMarkMailboxAsRead);
        prepareNotifyFolderAction(clicked);
        actionList.append(m_actionNotifyFolder);
        actionList.append(resyncMbox);
        actionList.append(reloadMboxList);
        // Only with the local index enabled; the index itself never downloads anything
        if (const Imap::Mailbox::SearchIndexer *indexer = m_imapAccess ? m_imapAccess->searchIndexer() : nullptr;
            indexer && clicked.data(Imap::Mailbox::RoleMailboxIsSelectable).toBool()) {
            m_actionDownloadSearchText->setData(clicked.data(Imap::Mailbox::RoleMailboxName).toString());
            m_actionDownloadSearchText->setEnabled(indexer->isAvailable() && imapModel() && imapModel()->isNetworkAvailable()
                                                   && !(m_searchTextFetcher && m_searchTextFetcher->isRunning()));
            actionList.append(m_actionDownloadSearchText);
        }

        actionList.append(m_actionSubscribeMailbox);
        m_actionSubscribeMailbox->setChecked(mboxTree->indexAt(position).data(Imap::Mailbox::RoleMailboxIsSubscribed).toBool());
        if (QMenu *colors = folderColorMenu(clicked))
            actionList.append(colors->menuAction());
    } else {
        actionList.append(createTopMailbox);
    }
    actionList.append(reloadAllMailboxes);
    actionList.append(m_actionShowOnlySubscribed);
    QMenu::exec(actionList, mboxTree->mapToGlobal(position), nullptr, this);
}

QMenu *MainWindow::folderColorMenu(const QModelIndex &mailbox)
{
    const QString accountId = m_accounts->activeAccount();
    const QString folder = mailbox.data(Imap::Mailbox::RoleMailboxName).toString();
    if (accountId.isEmpty() || folder.isEmpty())
        return nullptr;
    delete m_folderColorMenu;
    m_folderColorMenu = new QMenu(i18nc("@title:menu", "Folder Colour"), this);
    m_folderColorMenu->setObjectName(QStringLiteral("folderColorMenu"));
    m_folderColorMenu->setIcon(UiUtils::SymbolicIcon::fromTheme(QStringLiteral("color-management")));
    const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
    const QColor own = UiUtils::SettingsStore::folderColors(*config).folders.value(accountId).value(folder);
    const auto store = [this, accountId, folder](const QColor &color) {
        const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
        UiUtils::SettingsStore::setFolderColor(*config, accountId, folder, color);
        config->sync();
        applyAppearance();
    };
    static const QHash<QString, KLazyLocalizedString> names{
        {QStringLiteral("red"), kli18nc("@item:inmenu folder colour", "Red")},
        {QStringLiteral("orange"), kli18nc("@item:inmenu folder colour", "Orange")},
        {QStringLiteral("yellow"), kli18nc("@item:inmenu folder colour", "Yellow")},
        {QStringLiteral("green"), kli18nc("@item:inmenu folder colour", "Green")},
        {QStringLiteral("cyan"), kli18nc("@item:inmenu folder colour", "Cyan")},
        {QStringLiteral("blue"), kli18nc("@item:inmenu folder colour", "Blue")},
        {QStringLiteral("violet"), kli18nc("@item:inmenu folder colour", "Violet")},
        {QStringLiteral("magenta"), kli18nc("@item:inmenu folder colour", "Magenta")},
        {QStringLiteral("brown"), kli18nc("@item:inmenu folder colour", "Brown")},
        {QStringLiteral("grey"), kli18nc("@item:inmenu folder colour", "Grey")},
        {QStringLiteral("black"), kli18nc("@item:inmenu folder colour", "Black")},
        {QStringLiteral("white"), kli18nc("@item:inmenu folder colour", "White")},
    };
    auto *group = new QActionGroup(m_folderColorMenu);
    for (const QString &name : UiUtils::FolderColors::iconColors()) {
        const QColor color = UiUtils::FolderColors::iconColor(name);
        QPixmap swatch(16, 16);
        swatch.fill(color);
        // Coloured folder icons of the icon theme where it has them, else a swatch
        const QString iconName = QStringLiteral("folder-") + name;
        QAction *action = m_folderColorMenu->addAction(
            QIcon::hasThemeIcon(iconName) ? QIcon::fromTheme(iconName) : QIcon(swatch), names.value(name).toString());
        action->setObjectName(QStringLiteral("folderColor-") + name);
        action->setCheckable(true);
        action->setChecked(own.isValid() && QColor(own.rgb()) == color);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [store, color]() { store(color); });
    }
    m_folderColorMenu->addSeparator();
    QAction *custom = m_folderColorMenu->addAction(UiUtils::SymbolicIcon::fromTheme(QStringLiteral("color-picker")),
                                                   i18nc("@action:inmenu", "Other Colour…"));
    custom->setObjectName(QStringLiteral("folderColorCustom"));
    connect(custom, &QAction::triggered, this, [this, store, own, folder]() {
        const QColor color = QColorDialog::getColor(own.isValid() ? own : QColor(61, 174, 233), this,
                                                    i18nc("@title:window %1 is a folder", "Colour of %1", folder));
        if (color.isValid())
            store(color);
    });
    QAction *remove = m_folderColorMenu->addAction(UiUtils::SymbolicIcon::fromTheme(QStringLiteral("edit-clear")),
                                                   i18nc("@action:inmenu", "No Colour of Its Own"));
    remove->setObjectName(QStringLiteral("folderColorRemove"));
    remove->setToolTip(i18nc("@info:tooltip", "Folder colour rules from the Theme settings can still colour it"));
    remove->setEnabled(own.isValid());
    connect(remove, &QAction::triggered, this, [store]() { store(QColor()); });
    return m_folderColorMenu;
}

void MainWindow::showContextMenuMsgListTree(const QPoint &position)
{
    QList<QAction *> actionList;
    QModelIndex index = msgListWidget->tree->indexAt(position);
    // The header of a group gets the actions on the whole list
    if (index.data(Imap::Mailbox::RoleIsGroupHeader).toBool())
        index = QModelIndex();
    if (index.isValid()) {
        updateMessageFlagsOf(index);
        if (m_tabsEnabled) {
            m_newTabCandidate = messageTab(index);
            if (!m_newTabCandidate.accountId.isEmpty())
                actionList.append(m_actionOpenInNewTab);
        }
        actionList.append(markAsRead);
        actionList.append(m_markAsUnread);
        actionList.append(markAsDeleted);
        actionList.append(m_deletePermanently);
        actionList.append(markAsFlagged);
        actionList.append(markAsJunk);
        actionList.append(markAsNotJunk);
        actionList.append(moveToArchive);
        actionList.append(m_moveToMenu->menuAction());
        actionList.append(m_copyToMenu->menuAction());
        actionList.append(m_actionMarkMailboxAsRead);
        actionList.append(saveWholeMessage);
#ifdef IMPRESARIO_HAVE_WEBENGINE
        // They act on the message in the reader, so they are offered for that one only
        if (Imap::deproxifiedIndex(index) == Imap::deproxifiedIndex(m_messageWidget->messageView->currentMessage())) {
            for (QAction *action : {m_actionMessageToCalendar, m_actionMessageToNote}) {
                if (action->isVisible())
                    actionList.append(action);
            }
        }
#endif
        actionList.append(viewMsgSource);
        actionList.append(viewMsgHeaders);
        // Among results or other narrowed messages, the message can be seen among the rest of its folder
        if (showsUnifiedList() || messageListNarrowed())
            actionList.append(m_actionRevealInFolder);
        auto appendTagIfExists = [this,&actionList](const int row, QAction *tag) {
            if (m_favoriteTags->rowCount() > row - 1)
                actionList.append(tag);
        };
        appendTagIfExists(1, tag1);
        appendTagIfExists(2, tag2);
        appendTagIfExists(3, tag3);
        appendTagIfExists(4, tag4);
        appendTagIfExists(5, tag5);
        appendTagIfExists(6, tag6);
        appendTagIfExists(7, tag7);
        appendTagIfExists(8, tag8);
        appendTagIfExists(9, tag9);
    }
    QAction selectionSeparator;
    selectionSeparator.setSeparator(true);
    QAction filterSeparator;
    filterSeparator.setSeparator(true);
    QAction relatedSeparator;
    relatedSeparator.setSeparator(true);
    updateSelectionActions();
    if (index.isValid()) {
        actionList << &selectionSeparator;
        if (m_actionSelectThread->isEnabled())
            actionList << m_actionSelectThread;
        actionList << m_actionSelectAllMessages << m_selectSimilarMenu->menuAction() << &relatedSeparator
                   << m_showRelatedMenu->menuAction();
    } else {
        // Below the messages: actions on the whole list
        actionList << m_actionSelectAllMessages << &selectionSeparator << m_actionFilterUnread << m_actionFilterFlagged
                   << m_actionFilterAttachments << m_actionClearFilters << m_clearSearchAndFilters << &filterSeparator
                   << m_actionMarkMailboxAsRead;
        if (msgListWidget->tree->model() == m_groupingMsgListModel)
            actionList << &relatedSeparator << m_actionExpandAllGroups << m_actionCollapseAllGroups;
        else if (showsThreads())
            actionList << &relatedSeparator << m_expandAllConversations << m_collapseAllConversations;
    }
    if (! actionList.isEmpty())
        QMenu::exec(actionList, msgListWidget->tree->mapToGlobal(position), nullptr, this);
}

/** @short Ask for an updated list of mailboxes situated below the selected one

*/
void MainWindow::slotReloadMboxList()
{
    Q_FOREACH(const QModelIndex &item, mboxTree->selectionModel()->selectedIndexes()) {
        Q_ASSERT(item.isValid());
        if (item.column() != 0)
            continue;
        Imap::Mailbox::TreeItemMailbox *mbox = dynamic_cast<Imap::Mailbox::TreeItemMailbox *>(
                Imap::Mailbox::Model::realTreeItem(item)
                                               );
        Q_ASSERT(mbox);
        mbox->rescanForChildMailboxes(imapModel());
    }
}

/** @short Request a check for new messages in selected mailbox */
void MainWindow::slotResyncMbox()
{
    if (! imapModel()->isNetworkAvailable())
        return;

    Q_FOREACH(const QModelIndex &item, mboxTree->selectionModel()->selectedIndexes()) {
        Q_ASSERT(item.isValid());
        if (item.column() != 0)
            continue;
        imapModel()->resyncMailbox(item);
    }
}

void MainWindow::alertReceived(const QString &message)
{
    //: "ALERT" is a special warning which we're required to show to the user
    Gui::Util::messageBoxWarning(this, tr("IMAP Alert"), message);
}

void MainWindow::imapError(const QString &message)
{
    Gui::Util::messageBoxCritical(this, tr("IMAP Protocol Error"), message);
    // Show the IMAP logger -- maybe some user will take that as a hint that they shall include it in the bug report.
    // </joke>
    showProtocolLogger->setChecked(true);
}

void MainWindow::networkError(const QString &message)
{
    updateConnectionStatus(i18nc("@info:status", "Network error"));
    m_connectionError = true;
    m_connectionStatus->setToolTip(Qt::convertFromPlainText(message));
    m_connectionStatus->setAccessibleDescription(message);
    const QString title = tr("Network Error");
    if (!m_networkErrorMessageBox) {
        m_networkErrorMessageBox = new QMessageBox(QMessageBox::Critical, title,
                                                   QString(), QMessageBox::Ok, this);
    }
    // User must be informed about a new (but not recurring) error
    if (message != m_networkErrorMessageBox->text()) {
        m_networkErrorMessageBox->setText(message);
        if (qApp->applicationState() == Qt::ApplicationActive) {
            m_networkErrorMessageBox->setProperty(netErrorUnseen, false);
            m_networkErrorMessageBox->show();
        } else {
            m_networkErrorMessageBox->setProperty(netErrorUnseen, true);
            if (m_trayIcon && m_trayIcon->isVisible())
                m_trayIcon->showMessage(title, message, QSystemTrayIcon::Warning, 3333);
        }
    }
}

void MainWindow::cacheError(const QString &message)
{
    Gui::Util::messageBoxCritical(this, tr("IMAP Cache Error"),
                                  tr("The caching subsystem managing a cache of the data already "
                                     "downloaded from the IMAP server is having troubles. "
                                     "All caching will be disabled.\n\n%1").arg(message));
}

void MainWindow::networkPolicyOffline()
{
    netExpensive->setChecked(false);
    netOnline->setChecked(false);
    netOffline->setChecked(true);
    updateActionsOnlineOffline(false);
    updateConnectionStatus(tr("Offline"));
    showStatusMessage(tr("Offline"));
    handleTrayIconChange();
}

void MainWindow::networkPolicyExpensive()
{
    if (netOffline->isChecked())
        resumeCurrentMailbox();
    netOffline->setChecked(false);
    netOnline->setChecked(false);
    netExpensive->setChecked(true);
    updateActionsOnlineOffline(true);
    updateConnectionStatus(i18nc("@info:status", "Network access enabled"));
    handleTrayIconChange();
}

void MainWindow::networkPolicyOnline()
{
    if (netOffline->isChecked())
        resumeCurrentMailbox();
    netOffline->setChecked(false);
    netExpensive->setChecked(false);
    netOnline->setChecked(true);
    updateActionsOnlineOffline(true);
    updateConnectionStatus(i18nc("@info:status", "Network access enabled"));
    handleTrayIconChange();
}

void MainWindow::resumeCurrentMailbox()
{
    auto *model = qobject_cast<Imap::Mailbox::MsgListModel *>(m_imapAccess->msgListModel());
    const QString name = model->currentMailbox().data(Imap::Mailbox::RoleMailboxName).toString();
    if (!name.isEmpty())
        mboxTree->restoreMailbox(name);
}

void MainWindow::updateGoToFolderActions()
{
    const Imap::Mailbox::Model *model = imapModel();
    for (auto it = m_goToSpecialFolderActions.constBegin(); it != m_goToSpecialFolderActions.constEnd(); ++it) {
        const QString mailbox = model ? model->specialFolderMailbox(it.key()) : QString();
        it.value()->setEnabled(!mailbox.isEmpty());
        it.value()->setToolTip(mailbox);
    }
    const bool online = model && model->isNetworkAvailable();
    if (m_emptyTrash)
        m_emptyTrash->setEnabled(online && !model->specialFolderMailbox(Imap::Mailbox::SpecialUse::Trash).isEmpty());
    if (m_emptyJunk)
        m_emptyJunk->setEnabled(online && !model->specialFolderMailbox(Imap::Mailbox::SpecialUse::Junk).isEmpty());
    updateDeleteActionMode();
}

void MainWindow::goToSpecialFolder(Imap::Mailbox::SpecialUse kind)
{
    if (const Imap::Mailbox::Model *model = imapModel())
        goToMailbox(model->specialFolderMailbox(kind));
}

void MainWindow::goToMailbox(const QString &mailbox)
{
    if (mailbox.isEmpty() || !prettyMboxModel)
        return;
    setUnifiedInbox(false, false);
    // The tree selects the mailbox and opens it as soon as its MailboxFinder locates the index
    mboxTree->restoreMailbox(mailbox);
    if (m_mainStack) {
        m_mainStack->setCurrentWidget(msgListWidget);
        m_oneAtTimeGoBack->setEnabled(true);
    }
    msgListWidget->tree->setFocus();
}

void MainWindow::slotGoToFolder()
{
    if (!prettyMboxModel)
        return;
    auto *dialog = new MailboxPickerDialog(this);
    dialog->setObjectName(QStringLiteral("goToFolderDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(i18nc("@title:window", "Go to Folder"));
    dialog->setMailboxes(prettyMboxModel);
    connect(dialog, &QDialog::accepted, this, [this, dialog]() { goToMailbox(dialog->selectedMailbox()); });
    dialog->open();
}

void MainWindow::slotGoToNextUnreadFolder()
{
    if (!prettyMboxModel)
        return;
    const QModelIndex next = UiUtils::nextUnreadMailbox(prettyMboxModel, currentMailboxName());
    if (!next.isValid()) {
        showStatusMessage(i18nc("@info:status", "No other folder has unread messages"));
        return;
    }
    goToMailbox(next.data(Imap::Mailbox::RoleMailboxName).toString());
}

void MainWindow::slotCheckMail()
{
    int checked = 0;
    for (const QString &accountId : m_accounts->accountIds()) {
        Imap::ImapAccess *access = m_accountAccess.value(accountId);
        auto *model = access ? qobject_cast<Imap::Mailbox::Model *>(access->imapModel()) : nullptr;
        if (!model || !model->isNetworkAvailable())
            continue;
        // Counts which were known are asked for again as soon as a view or the account sidebar reads them
        model->invalidateAllMessageCounts();
        ++checked;
    }
    auto *msgList = qobject_cast<Imap::Mailbox::MsgListModel *>(m_imapAccess->msgListModel());
    const QModelIndex current = msgList ? msgList->currentMailbox() : QModelIndex();
    if (current.isValid() && imapModel() && imapModel()->isNetworkAvailable())
        imapModel()->resyncMailbox(current);
    scheduleAccountUnreadRefresh();
    showStatusMessage(checked ? i18ncp("@info:status", "Checking %1 account for new mail",
                                       "Checking %1 accounts for new mail", checked)
                              : i18nc("@info:status", "No account is online"));
}

void MainWindow::slotMoveToFolder()
{
    pickFolder(Imap::Mailbox::MOVE);
}

void MainWindow::slotCopyToFolder()
{
    pickFolder(Imap::Mailbox::COPY);
}

void MainWindow::pickFolder(const Imap::Mailbox::CopyMoveOperation op)
{
    // Folders belong to one account, so the messages have to be of one account as well
    if (routeToMessageAccounts([this, op]() { pickFolder(op); }, true))
        return;
    if (!prettyMboxModel || !imapModel()->isNetworkAvailable())
        return;
    QList<QPersistentModelIndex> messages;
    for (const QModelIndex &index : translatedSelection())
        messages << QPersistentModelIndex(index);
    if (messages.isEmpty())
        return;

    const bool move = op == Imap::Mailbox::MOVE;
    const QString current = currentMailboxName();
    auto *dialog = new MailboxPickerDialog(this);
    dialog->setObjectName(move ? QStringLiteral("moveToFolderDialog") : QStringLiteral("copyToFolderDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(move ? i18nc("@title:window", "Move to Folder") : i18nc("@title:window", "Copy to Folder"));
    dialog->setPrompt(move ? i18ncp("@label", "Move %1 message to:", "Move %1 messages to:", messages.size())
                           : i18ncp("@label", "Copy %1 message to:", "Copy %1 messages to:", messages.size()));
    if (currentAccess() != m_imapAccess)
        dialog->setMailboxes(new Imap::Mailbox::PrettyMailboxModel(dialog, currentAccess()->mailboxModel()), {current});
    else
        dialog->setMailboxes(prettyMboxModel, {current});
    const QStringList recent = UiUtils::RecentFolders::load(*accountSettings(), Common::SettingsNames::imapRecentDestinationFolders)
            .offered(UiUtils::RecentFolders::StateLookup(), {current});
    if (!recent.isEmpty())
        dialog->setCurrentMailbox(recent.constFirst());
    QPointer<Imap::Mailbox::Model> model = imapModel();
    connect(dialog, &QDialog::accepted, this, [this, dialog, model, messages, op]() {
        // The selection may have changed, or the messages may be gone, while the dialog was open
        if (!showsModel(model))
            return;
        QModelIndexList stillThere;
        for (const QPersistentModelIndex &index : messages) {
            if (index.isValid())
                stillThere << index;
        }
        actOnModel(model, [&]() { copyOrMoveToFolder(stillThere, dialog->selectedMailbox(), op); });
    });
    dialog->open();
}

void MainWindow::populateRecentFoldersMenu(QMenu *menu, const Imap::Mailbox::CopyMoveOperation op)
{
    using UiUtils::RecentFolders;
    if (showsUnifiedList() && !m_acting) {
        // The recent folders of the account which all selected messages belong to
        QString accountId;
        bool oneAccount = true;
        for (const QModelIndex &message : translatedSelection()) {
            const QString id = accountOfModel(qobject_cast<const Imap::Mailbox::Model *>(message.model()));
            if (!accountId.isEmpty() && id != accountId)
                oneAccount = false;
            accountId = id;
        }
        if (oneAccount && !accountId.isEmpty() && accountId != m_accounts->activeAccount()) {
            actAs(accountId, QModelIndexList(), [this, menu, op]() { populateRecentFoldersMenu(menu, op); });
            return;
        }
    }
    menu->clear();
    QSettings *settings = accountSettings();
    RecentFolders recent = RecentFolders::load(*settings, Common::SettingsNames::imapRecentDestinationFolders);
    // The unfiltered folder tree, so that unsubscribed folders do not look deleted
    const RecentFolders::StateLookup state = RecentFolders::stateFromModel(currentAccess()->mailboxModel());
    if (recent.prune(state))
        recent.save(*settings, Common::SettingsNames::imapRecentDestinationFolders);

    const QStringList folders = recent.offered(state, {currentMailboxName()});
    for (const QString &mailbox : folders) {
        QString label = mailbox;
        QAction *action = menu->addAction(label.replace(QLatin1Char('&'), QLatin1String("&&")));
        action->setData(mailbox);
        action->setToolTip(mailbox);
        const QIcon special = Imap::Mailbox::PrettyMailboxModel::specialFolderIcon(imapModel()->mailboxSpecialUse(mailbox));
        action->setIcon(special.isNull() ? UiUtils::loadIcon(QStringLiteral("folder")) : special);
        connect(action, &QAction::triggered, this, [this, mailbox, op]() {
            if (!routeToMessageAccounts([this, mailbox, op]() { copyOrMoveToFolder(translatedSelection(), mailbox, op); }, true))
                copyOrMoveToFolder(translatedSelection(), mailbox, op);
        });
    }
    if (!folders.isEmpty())
        menu->addSeparator();
    QAction *other = menu->addAction(i18nc("@action:inmenu", "Other Folder…"));
    other->setObjectName(op == Imap::Mailbox::MOVE ? QStringLiteral("moveToOtherFolder") : QStringLiteral("copyToOtherFolder"));
    connect(other, &QAction::triggered, this, [this, op]() {
        pickFolder(op);
    });
}

void MainWindow::rememberRecentFolder(const QString &mailbox)
{
    QSettings *settings = accountSettings();
    auto recent = UiUtils::RecentFolders::load(*settings, Common::SettingsNames::imapRecentDestinationFolders);
    recent.add(mailbox);
    recent.save(*settings, Common::SettingsNames::imapRecentDestinationFolders);
}

void MainWindow::copyOrMoveToFolder(const QModelIndexList &messages, const QString &mailbox,
                                    const Imap::Mailbox::CopyMoveOperation op)
{
    Imap::Mailbox::Model *model = imapModel();
    if (mailbox.isEmpty() || messages.isEmpty() || !model || !model->isNetworkAvailable())
        return;
    rememberRecentFolder(mailbox);
    if (op == Imap::Mailbox::MOVE)
        trackMove(messages, sourceMailboxName(messages), mailbox, MoveKind::Folder);
    else
        trackCopy(messages, mailbox);
}

/** @short Deletes a network error message box instance upon resetting of reconnect state */
void MainWindow::slotResetReconnectState()
{
    if (m_networkErrorMessageBox) {
        delete m_networkErrorMessageBox;
        m_networkErrorMessageBox = nullptr;
    }
}

void MainWindow::slotShowSettings()
{
    // Application settings are read where they are used, so the account connection stays as it is.
    SettingsDialog dialog(this, SettingsDialog::Scope::Application, m_senderIdentities, accountIdentitiesModel(),
                          m_favoriteTags, m_settings);
    if (dialog.exec() == QDialog::Accepted) {
        removeSysTray();
        slotToggleSysTray();
        scheduleAccountUnreadRefresh();
        applyApplicationSettings();
    } else {
        // A preview that was not saved goes back to the saved appearance
        applyAppearance();
    }
}

void MainWindow::applyApplicationSettings()
{
    const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
    KConfigGroup appearance(config, QStringLiteral("Appearance"));
    if (m_densityMenu)
        m_densityMenu->setComfortable(appearance.readEntry("RowDensity", QStringLiteral("compact"))
                                      == QLatin1String("comfortable"));
    const bool right = appearance.readEntry("MailboxSidebarPosition", QStringLiteral("left")) == QLatin1String("right");
    if (right != m_mailboxSidebarOnRight)
        setMailboxSidebarRight(right);
    if (auto *action = findChild<QAction *>(right ? QStringLiteral("mailboxSidebarRight") : QStringLiteral("mailboxSidebarLeft")))
        action->setChecked(true);
    if (m_shellHeader)
        m_shellHeader->toggleAction()->setChecked(appearance.readEntry("ShowHeader", false));
    const bool focus = UiUtils::SettingsStore::value(*config, m_settings, QStringLiteral("Appearance/FocusLayout")).toBool();
    if (focus != m_focusLayout) {
        m_focusLayout = focus;
        updateShellRegions(true, true);
    }
    msgListWidget->applySearchSettings();
    watchSharedKeymap();
    watchFollowedTheme();
    applyTabSettings();
    applyVimSettings();
    applyProductivitySettings();
    applyAppearance();
}

void MainWindow::registerAppearance()
{
    auto *appearance = AppearanceManager::instance();
    appearance->registerWindow(this);
    appearance->registerPane(mboxTree);
    appearance->registerPane(msgListWidget->tree);
    appearance->registerPane(m_readerColumn);
    appearance->registerFont(msgListWidget->tree, AppearanceManager::FontTarget::MessageList);
    appearance->registerFont(mboxTree, AppearanceManager::FontTarget::FolderTree);
    appearance->registerFont(m_tabBar, AppearanceManager::FontTarget::Tabs);
    appearance->registerFont(statusBar(), AppearanceManager::FontTarget::StatusBar);
    connect(appearance, &AppearanceManager::changed, this, &MainWindow::applyListAppearance);
    applyAppearance();
}

void MainWindow::applyAppearance()
{
    mboxTree->setAccountId(m_accounts->activeAccount());
    msgListWidget->tree->setAccountId(m_accounts->activeAccount());
    const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
    AppearanceManager::instance()->load(*config, m_activities ? m_activities->current() : QString(),
                                        m_accounts->activeAccount());
    handleTrayIconChange();
}

void MainWindow::applyListAppearance()
{
    auto *appearance = AppearanceManager::instance();
    msgListWidget->tree->setAlternatingRowColors(appearance->value(QStringLiteral("StripeRows")).toBool());
    mboxTree->setAlternatingRowColors(appearance->value(QStringLiteral("StripeFolderTree")).toBool());
    msgListWidget->tree->viewport()->update();
    mboxTree->viewport()->update();
    // Folder and account colours of the tabs can have changed
    refreshTabs();
}

void MainWindow::followActivity(const QString &activityId)
{
    const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
    if (!activityId.isEmpty()
        && UiUtils::SettingsStore::value(*config, m_settings, QStringLiteral("Workspaces/FollowActivity")).toBool()) {
        const QString accountId = UiUtils::Workspaces::accountForActivity(UiUtils::SettingsStore::workspaces(*config),
                                                                          activityId, m_accounts->activeAccount(),
                                                                          m_accounts->accountIds());
        if (!accountId.isEmpty() && accountId != m_accounts->activeAccount()) {
            // Switching applies the workspace's appearance
            switchAccount(accountId);
            return;
        }
    }
    applyAppearance();
}

void MainWindow::watchSharedKeymap()
{
    const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
    const bool follow = UiUtils::SettingsStore::value(*config, m_settings, QStringLiteral("Keyboard/FollowSharedKeymap")).toBool();
    const QString path = follow ? UiUtils::SettingsStore::followedProfile(*config) : QString();
    if (m_keymapWatcher) {
        if (m_keymapWatcher->files() == QStringList{path})
            return;
        delete m_keymapWatcher;
        m_keymapWatcher = nullptr;
    }
    if (path.isEmpty())
        return;
    m_keymapWatcher = new QFileSystemWatcher(this);
    const auto import = [this, path]() {
        QFile file(path);
        // Editors replace the file when saving it, which drops it from the watcher
        if (m_keymapWatcher && !m_keymapWatcher->files().contains(path) && QFileInfo::exists(path))
            m_keymapWatcher->addPath(path);
        if (!file.open(QIODevice::ReadOnly))
            return;
        const UiUtils::Keymap::ParseResult result = UiUtils::Keymap::fromJson(file.readAll());
        if (!result.profile) {
            qWarning() << "The followed keymap" << path << "could not be read:" << result.error;
            return;
        }
        const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
        if (UiUtils::SettingsStore::keymapProfile(*config) == *result.profile)
            return;
        UiUtils::SettingsStore::importKeymap(*config, *result.profile);
        config->sync();
        applyVimSettings();
    };
    connect(m_keymapWatcher, &QFileSystemWatcher::fileChanged, this, import);
    if (QFileInfo::exists(path))
        m_keymapWatcher->addPath(path);
    import();
}

void MainWindow::watchFollowedTheme()
{
    const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
    const QString path = UiUtils::SettingsStore::followedTheme(*config);
    if (m_themeWatcher) {
        if (m_themeWatcher->files() == QStringList{path})
            return;
        delete m_themeWatcher;
        m_themeWatcher = nullptr;
    }
    if (path.isEmpty())
        return;
    m_themeWatcher = new QFileSystemWatcher(this);
    const auto import = [this, path]() {
        // Editors replace the file when saving it, which drops it from the watcher
        if (m_themeWatcher && !m_themeWatcher->files().contains(path) && QFileInfo::exists(path))
            m_themeWatcher->addPath(path);
        if (!QFileInfo::exists(path))
            return;
        const UiUtils::ThemeProfiles::ParseResult result = UiUtils::ThemeProfiles::importFile(path);
        if (!result.profile) {
            qWarning() << "The followed theme" << path << "could not be read:" << result.error;
            return;
        }
        const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
        const UiUtils::Theme::Values global = UiUtils::SettingsStore::theme(*config);
        const UiUtils::Theme::Values applied = UiUtils::ThemeProfiles::appliedTo(global, *result.profile);
        if (applied == global)
            return;
        UiUtils::SettingsStore::setTheme(*config, applied);
        config->sync();
        applyAppearance();
    };
    connect(m_themeWatcher, &QFileSystemWatcher::fileChanged, this, import);
    if (QFileInfo::exists(path))
        m_themeWatcher->addPath(path);
    import();
}

void MainWindow::applyVimSettings()
{
    if (!m_vimMode)
        return;
    const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
    auto setting = [&config, this](const char *key) {
        return UiUtils::SettingsStore::value(*config, m_settings, QLatin1String(key));
    };
    const bool enabled = setting("Keyboard/VimMode").toBool();
    m_vimMode->setBindings(UiUtils::Keymap::merged(UiUtils::SettingsStore::bindings(*config)));
    m_vimMode->setTimeout(setting("Keyboard/VimTimeout").toInt());
    m_vimMode->setHints(setting("Keyboard/VimHints").toBool(), setting("Keyboard/VimHintDelay").toInt());
    m_vimMode->setEnabled(enabled);
    m_vimPending->setText(i18nc("@info:status Vim mode is on", "Vim"));
    m_vimPending->setToolTip(i18nc("@info:tooltip", "Vim mode is on; the keys of an unfinished command show here"));
    m_vimPending->setVisible(enabled);
}

void MainWindow::applyProductivitySettings()
{
    if (!m_actionMessageToCalendar)
        return;
#ifdef IMPRESARIO_HAVE_WEBENGINE
    const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
    auto setting = [&config, this](const char *key) {
        return UiUtils::SettingsStore::value(*config, m_settings, QLatin1String(key));
    };
    m_actionMessageToCalendar->setVisible(setting("Integrations/MailToCalendar").toBool()
                                          && KApplicationTrader::preferredService(QStringLiteral("text/calendar")));
    const QString folder = setting("Integrations/NotesFolder").toString();
    m_actionMessageToNote->setVisible(setting("Integrations/MailToNotes").toBool() && !folder.isEmpty()
                                      && QFileInfo(folder).isDir());
#endif
}

QString MainWindow::notesFolder() const
{
    const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
    const QString folder = UiUtils::SettingsStore::value(*config, m_settings, QStringLiteral("Integrations/NotesFolder")).toString();
    if (!m_activities || !UiUtils::SettingsStore::value(*config, m_settings, QStringLiteral("Integrations/WorkContext")).toBool())
        return folder;
    return UiUtils::Workspaces::notesFolder(UiUtils::SettingsStore::workspaces(*config), m_activities->current(), folder);
}

void MainWindow::withReaderMessage(const std::function<void(const UiUtils::MailToProductivity::Message &)> &fn)
{
#ifdef IMPRESARIO_HAVE_WEBENGINE
    const QModelIndex index = Imap::deproxifiedIndex(m_messageWidget->messageView->currentMessage());
    if (!index.isValid())
        return;
    UiUtils::MailToProductivity::Message message;
    message.subject = index.data(Imap::Mailbox::RoleMessageSubject).toString();
    message.from = Imap::Message::MailAddress::prettyList(index.data(Imap::Mailbox::RoleMessageFrom).toList(),
                                                          Imap::Message::MailAddress::FORMAT_READABLE);
    const QVariantList to = index.data(Imap::Mailbox::RoleMessageTo).toList();
    for (const QVariant &address : to)
        message.to << Imap::Message::MailAddress::prettyList(QVariantList{address}, Imap::Message::MailAddress::FORMAT_READABLE);
    message.date = index.data(Imap::Mailbox::RoleMessageDate).toDateTime();
    message.messageId = QString::fromUtf8(index.data(Imap::Mailbox::RoleMessageMessageId).toByteArray());
    if (message.messageId.startsWith(QLatin1Char('<')) && message.messageId.endsWith(QLatin1Char('>')))
        message.messageId = message.messageId.mid(1, message.messageId.size() - 2);
    message.accountName = m_accounts->displayName(accountOfModel(qobject_cast<const Imap::Mailbox::Model *>(index.model())));
    message.folder = index.data(Imap::Mailbox::RoleMailboxName).toString();
    if (m_messageWidget->messageView->isCryptoMessage()) {
        // Decrypted text is not written out where the message's encryption no longer protects it
        fn(message);
        return;
    }
    QPointer<MainWindow> self(this);
    QPersistentModelIndex shown(index);
    m_messageWidget->messageView->quoteText([self, shown, message, fn](const QString &text) mutable {
        if (!self || !shown.isValid())
            return;
        message.text = text;
        fn(message);
    });
#else
    Q_UNUSED(fn);
#endif
}

void MainWindow::messageToCalendar()
{
    withReaderMessage([this](const UiUtils::MailToProductivity::Message &message) {
        using namespace UiUtils::MailToProductivity;
        const QDateTime now = QDateTime::currentDateTime();
        const QString uid = QUuid::createUuid().toString(QUuid::WithoutBraces) + QLatin1String("@plasma-impresario");
        const QByteArray ics = calendarEvent(message, suggestedStart(now), 30, uid, now.toUTC());
        QString error;
        const QString path = saveEvent(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                                           + QLatin1String("/events"),
                                       ics, uid, now, &error);
        if (path.isEmpty()) {
            m_notificationBar->showError(error);
            return;
        }
        // The calendar application shows the event to be reviewed and saved there
        auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(path), QStringLiteral("text/calendar"));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
        job->start();
    });
}

void MainWindow::messageToNote()
{
    withReaderMessage([this](const UiUtils::MailToProductivity::Message &message) {
        QString error;
        const QString path = UiUtils::MailToProductivity::saveNote(notesFolder(), message, &error);
        if (path.isEmpty()) {
            m_notificationBar->showError(error);
            return;
        }
        InlineNotificationBar::Action open;
        open.text = i18nc("@action:button", "Open Note");
        open.iconName = QStringLiteral("document-open");
        open.objectName = QStringLiteral("openNote");
        open.callback = [this, path]() {
            auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(path));
            job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
            job->start();
        };
        m_notificationBar->showInfo(i18nc("@info %1 is a file name", "Saved the note %1", QFileInfo(path).fileName()), {open});
    });
}

std::optional<UiUtils::Keymap::Context> MainWindow::vimContext(QWidget *focus) const
{
    using UiUtils::Keymap::Context;
    if (!focus)
        return std::nullopt;
    if (mboxTree && (focus == mboxTree || mboxTree->isAncestorOf(focus)))
        return Context::FolderTree;
    if (msgListWidget && (focus == msgListWidget->tree || msgListWidget->tree->isAncestorOf(focus)))
        return Context::MessageList;
    if (m_messageWidget && (focus == m_messageWidget || m_messageWidget->isAncestorOf(focus)))
        return Context::Reader;
    return std::nullopt;
}

void MainWindow::runVimCommand(const QString &command, UiUtils::Keymap::Context context)
{
    using UiUtils::Keymap::Context;
    QWidget *focus = QApplication::focusWidget();
    auto key = [this, focus](int key) { m_vimMode->sendKey(focus, key); };
    auto trigger = [](QAction *action) {
        if (action && action->isEnabled() && action->isVisible())
            action->trigger();
    };
    QTreeView *list = msgListWidget->tree;

    if (command == QLatin1String("move.down")) {
        key(Qt::Key_Down);
    } else if (command == QLatin1String("move.up")) {
        key(Qt::Key_Up);
    } else if (command == QLatin1String("move.top")) {
        key(Qt::Key_Home);
    } else if (command == QLatin1String("move.bottom")) {
        key(Qt::Key_End);
    } else if (command == QLatin1String("move.out")) {
        if (context == Context::FolderTree)
            key(Qt::Key_Left);
        else if (context == Context::MessageList)
            mboxTree->setFocus(Qt::ShortcutFocusReason);
        else
            list->setFocus(Qt::ShortcutFocusReason);
    } else if (command == QLatin1String("move.in")) {
        if (context == Context::FolderTree) {
            const QModelIndex current = mboxTree->currentIndex();
            if (current.isValid() && mboxTree->model()->hasChildren(current) && !mboxTree->isExpanded(current))
                mboxTree->expand(current);
            else
                list->setFocus(Qt::ShortcutFocusReason);
        } else if (context == Context::MessageList) {
            const QModelIndex current = list->currentIndex();
            if (current.isValid()) {
                msgListClicked(current);
                if (m_messageWidget->isVisible())
                    m_messageWidget->setFocus(Qt::ShortcutFocusReason);
            }
        }
    } else if (command == QLatin1String("page.down") || command == QLatin1String("page.up")) {
        const bool down = command == QLatin1String("page.down");
        if (context == Context::Reader) {
            key(down ? Qt::Key_PageDown : Qt::Key_PageUp);
        } else {
            QAbstractScrollArea *area = nullptr;
            for (QWidget *widget = focus; widget && !area; widget = widget->parentWidget())
                area = qobject_cast<QAbstractScrollArea *>(widget);
            if (area) {
                QScrollBar *bar = area->verticalScrollBar();
                const int step = std::max(1, bar->pageStep() / 2);
                bar->setValue(bar->value() + (down ? step : -step));
            }
        }
    } else if (command == QLatin1String("search")) {
        msgListWidget->focusSearch();
    } else if (command == QLatin1String("select.toggle")) {
        const QModelIndex current = list->currentIndex();
        if (current.isValid())
            list->selectionModel()->select(current, QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
    } else if (command == QLatin1String("tab.next")) {
        trigger(m_actionNextTab);
    } else if (command == QLatin1String("tab.previous")) {
        trigger(m_actionPreviousTab);
    } else if (command == QLatin1String("help")) {
        slotShowShortcutOverview();
    } else if (command == QLatin1String("go.inbox")) {
        goToMailbox(QStringLiteral("INBOX"));
    } else if (const UiUtils::Keymap::Command *local = UiUtils::Keymap::command(command); local && !local->action.isEmpty()) {
        trigger(findChild<QAction *>(local->action));
    }
}

void MainWindow::applyTabSettings()
{
    const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
    const bool enabled = UiUtils::SettingsStore::value(*config, m_settings, QStringLiteral("Tabs/Enabled")).toBool();
    const bool wasEnabled = std::exchange(m_tabsEnabled, enabled);
    for (QAction *action : {m_actionOpenInNewTab, m_actionNewTab, m_actionCloseTab, m_actionNextTab, m_actionPreviousTab}) {
        if (action)
            action->setVisible(enabled);
    }
    placeTabBar();
    if (!enabled || wasEnabled) {
        if (enabled)
            refreshTabs();
        return;
    }
    // Tabs of accounts which are gone are dropped; the others open as they were left
    m_tabSession = UiUtils::MailTabs::restorable(UiUtils::SettingsStore::tabs(*KSharedConfig::openStateConfig()),
                                                 m_accounts->accountIds());
    if (m_tabSession.tabs.isEmpty()) {
        // The first tab holds what is shown, and follows it once a folder opens
        UiUtils::MailTabs::Tab here = currentLocationTab();
        if (here.accountId.isEmpty()) {
            here.accountId = m_accounts->activeAccount();
            here.mailbox = currentMailboxName();
        }
        m_tabSession.tabs.append(here);
        m_tabSession.current = 0;
        refreshTabs();
        return;
    }
    refreshTabs();
    QTimer::singleShot(0, this, [this]() { activateTab(m_tabSession.current); });
}

void MainWindow::placeTabBar()
{
    if (!m_tabBar || !m_shellLayout || !m_mainAreaLayout)
        return;
    using UiUtils::MailTabs::Region;
    const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
    Region region = UiUtils::MailTabs::regionFromConfigName(
        UiUtils::SettingsStore::value(*config, m_settings, QStringLiteral("Tabs/Region")).toString());
    const Qt::Orientation orientation =
        UiUtils::SettingsStore::value(*config, m_settings, QStringLiteral("Tabs/Orientation")).toString() == QLatin1String("vertical")
        ? Qt::Vertical
        : Qt::Horizontal;
    // The focus layout hides the side columns, but not the tabs
    if (m_focusLayout && (region == Region::Left || region == Region::Right))
        region = Region::Main;

    QWidget *wanted = nullptr;
    switch (region) {
    case Region::Header:
    case Region::Footer:
        wanted = centralWidget();
        break;
    case Region::Main:
        wanted = m_mainArea;
        break;
    case Region::Left:
        wanted = m_shellLeft.widget;
        break;
    case Region::Right:
        wanted = m_shellRight.widget;
        break;
    }
    const bool moved = !m_tabsEnabled || m_tabBar->parentWidget() != wanted || m_tabBar->region() != region
        || m_tabBar->orientation() != orientation;
    if (moved) {
        m_shellLayout->removeWidget(m_tabBar);
        m_mainAreaLayout->removeWidget(m_tabBar);
        // Taking it out of a column's splitter
        m_tabBar->setParent(this);
        m_tabBar->hide();
    }
    if (m_tabsEnabled && moved) {
        m_tabBar->setPlacement(region, orientation);
        const Qt::Alignment alignment = orientation == Qt::Vertical ? Qt::AlignLeft : Qt::Alignment();
        switch (region) {
        case Region::Header:
            m_shellLayout->insertWidget(m_shellLayout->indexOf(m_shellHeader) + 1, m_tabBar, 0, alignment);
            break;
        case Region::Footer:
            m_shellLayout->addWidget(m_tabBar, 0, alignment);
            break;
        case Region::Main:
            m_mainAreaLayout->setDirection(orientation == Qt::Horizontal ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
            m_mainAreaLayout->insertWidget(0, m_tabBar);
            break;
        case Region::Left:
        case Region::Right: {
            QSplitter *column = static_cast<QSplitter *>(wanted);
            column->addWidget(m_tabBar);
            const int index = column->indexOf(m_tabBar);
            column->setStretchFactor(index, 0);
            column->setCollapsible(index, false);
            break;
        }
        }
        m_tabBar->show();
    }
    updateShellRegions(true, false);
}

void MainWindow::saveTabs()
{
    UiUtils::MailTabs::Session saved = m_tabSession;
    // Subjects stay out of the state file; a restored tab shows its folder until its message opens again
    for (UiUtils::MailTabs::Tab &tab : saved.tabs)
        tab.subject.clear();
    UiUtils::SettingsStore::setTabs(*KSharedConfig::openStateConfig(), saved);
}

void MainWindow::refreshTabs()
{
    if (!m_tabBar || !m_tabsEnabled)
        return;
    m_tabSession.current = std::clamp(m_tabSession.current, 0, std::max(0, int(m_tabSession.tabs.size()) - 1));
    QList<MailTabBar::Entry> entries;
    for (const UiUtils::MailTabs::Tab &tab : std::as_const(m_tabSession.tabs)) {
        QChar separator = QLatin1Char('/');
        if (Imap::ImapAccess *access = m_accountAccess.value(tab.accountId)) {
            if (auto *model = qobject_cast<Imap::Mailbox::Model *>(access->imapModel())) {
                const QString found = model->mailboxIndexByName(tab.mailbox).data(Imap::Mailbox::RoleMailboxSeparator).toString();
                if (!found.isEmpty())
                    separator = found.at(0);
            }
        }
        auto *appearance = AppearanceManager::instance();
        QColor color = appearance->folderColor(tab.accountId, tab.mailbox, separator);
        if (!color.isValid())
            color = appearance->accountColor(tab.accountId);
        entries.append({UiUtils::MailTabs::label(tab, separator),
                        UiUtils::MailTabs::toolTip(tab, m_accounts->displayName(tab.accountId)), color});
    }
    m_tabBar->setEntries(entries, m_tabSession.current);
    const bool several = m_tabSession.tabs.size() > 1;
    for (QAction *action : {m_actionCloseTab, m_actionNextTab, m_actionPreviousTab}) {
        if (action)
            action->setEnabled(several);
    }
    if (auto *column = qobject_cast<QSplitter *>(m_tabBar->parentWidget()))
        fitPanelRegion(column);
    saveTabs();
}

UiUtils::MailTabs::Tab MainWindow::messageTab(const QModelIndex &message) const
{
    UiUtils::MailTabs::Tab tab;
    const QModelIndex index = Imap::deproxifiedIndex(message);
    if (!index.isValid() || !index.data(Imap::Mailbox::RoleMessageUid).toUInt())
        return tab;
    tab.accountId = accountOfModel(qobject_cast<const Imap::Mailbox::Model *>(index.model()));
    tab.mailbox = index.data(Imap::Mailbox::RoleMailboxName).toString();
    tab.uidValidity = index.data(Imap::Mailbox::RoleMailboxUidValidity).toUInt();
    tab.uid = index.data(Imap::Mailbox::RoleMessageUid).toUInt();
    tab.subject = index.data(Imap::Mailbox::RoleMessageSubject).toString();
    if (tab.accountId.isEmpty() || tab.mailbox.isEmpty())
        return UiUtils::MailTabs::Tab();
    return tab;
}

UiUtils::MailTabs::Tab MainWindow::currentLocationTab() const
{
    const QString mailbox = m_unifiedInbox ? QString() : currentMailboxName();
#ifdef IMPRESARIO_HAVE_WEBENGINE
    if (m_messageWidget) {
        const UiUtils::MailTabs::Tab shown = messageTab(m_messageWidget->messageView->currentMessage());
        // A message of the folder, or one found by a search or in the Unified Inbox, which show other folders' mail
        if (!shown.accountId.isEmpty()
            && (mailbox.isEmpty() || searchShownInList()
                || (shown.mailbox == mailbox && shown.accountId == m_accounts->activeAccount())))
            return shown;
    }
#endif
    UiUtils::MailTabs::Tab tab;
    if (mailbox.isEmpty())
        return tab;
    tab.accountId = m_accounts->activeAccount();
    tab.mailbox = mailbox;
    return tab;
}

void MainWindow::syncCurrentTab()
{
    if (!m_tabsEnabled || m_activatingTab || m_tabSession.tabs.isEmpty())
        return;
    const UiUtils::MailTabs::Tab here = currentLocationTab();
    if (here.accountId.isEmpty())
        return;
    UiUtils::MailTabs::Tab &current = m_tabSession.tabs[m_tabSession.current];
    if (current == here)
        return;
    current = here;
    refreshTabs();
}

void MainWindow::activateTab(int index)
{
    if (!m_tabsEnabled || index < 0 || index >= m_tabSession.tabs.size())
        return;
    m_tabSession.current = index;
    refreshTabs();
    const UiUtils::MailTabs::Tab tab = m_tabSession.tabs.at(index);
    const UiUtils::MailTabs::Tab here = currentLocationTab();
    if (tab.accountId.isEmpty() || tab.mailbox.isEmpty()
        || (here.accountId == tab.accountId && here.mailbox == tab.mailbox && here.uid == tab.uid))
        return;
    m_activatingTab = true;
    if (tab.uid) {
        // A message which is gone leaves its folder shown, and the tab then holds the folder
        revealLocation(tab.accountId, tab.mailbox, tab.uidValidity, tab.uid);
    } else {
        if (searchShownInList())
            leaveSearchAcross();
        if (tab.accountId != m_accounts->activeAccount())
            switchAccount(tab.accountId);
        if (tab.accountId == m_accounts->activeAccount())
            goToMailbox(tab.mailbox);
    }
    m_activatingTab = false;
}

void MainWindow::openTab(const UiUtils::MailTabs::Tab &tab)
{
    if (!m_tabsEnabled || tab.accountId.isEmpty() || tab.mailbox.isEmpty())
        return;
    const int at = std::min(m_tabSession.current + 1, int(m_tabSession.tabs.size()));
    m_tabSession.tabs.insert(at, tab);
    activateTab(at);
}

void MainWindow::closeTab(int index)
{
    // The last tab stays: it is the view itself
    if (!m_tabsEnabled || m_tabSession.tabs.size() <= 1 || index < 0 || index >= m_tabSession.tabs.size())
        return;
    const bool wasCurrent = index == m_tabSession.current;
    const int next = UiUtils::MailTabs::currentAfterClose(m_tabSession.tabs.size(), index, m_tabSession.current);
    m_tabSession.tabs.removeAt(index);
    m_tabSession.current = std::max(0, next);
    if (wasCurrent)
        activateTab(m_tabSession.current);
    else
        refreshTabs();
}

SettingsContext MainWindow::settingsContext()
{
    SettingsContext context;
    for (const QString &id : m_accounts->accountIds())
        context.accounts.append({id, m_accounts->displayName(id)});
    context.folders = [this](const QString &accountId) {
        QStringList folders;
        Imap::ImapAccess *access = m_accountAccess.value(accountId);
        QAbstractItemModel *model = access ? access->mailboxModel() : nullptr;
        if (!model)
            return folders;
        for (auto it = UiUtils::QaimDfsIterator(model->index(0, 0), model); it != UiUtils::QaimDfsIterator(); ++it) {
            const QString name = it->data(Imap::Mailbox::RoleMailboxName).toString();
            if (!name.isEmpty())
                folders.append(name);
        }
        return folders;
    };
    context.accountSettings = [this](const QString &accountId) { return m_accounts->settings(accountId); };

    context.activitiesAvailable = m_activities && m_activities->available();
    if (context.activitiesAvailable) {
        const auto activities = m_activities->activities();
        for (const auto &activity : activities)
            context.activities.append({activity.first, activity.second});
    }
    using UiUtils::SettingsSchema::Feature;
    if (KApplicationTrader::preferredService(QStringLiteral("text/calendar")))
        context.available |= Feature::Calendar;
    const QString notesFolder = UiUtils::SettingsStore::value(*KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")),
                                                              m_settings, QStringLiteral("Integrations/NotesFolder")).toString();
    if (!notesFolder.isEmpty() && QFileInfo(notesFolder).isDir())
        context.available |= Feature::Notes;
    if (context.activitiesAvailable)
        context.available |= Feature::Activities;

    const auto actions = findChildren<QAction *>();
    for (const QAction *action : actions) {
        for (const QKeySequence &sequence : action->shortcuts()) {
            if (sequence.count() == 1 && !(sequence[0].keyboardModifiers() & ~Qt::ShiftModifier))
                context.shortcuts.append(sequence);
        }
    }

    context.configureNotifications = [this]() { configureNotifications(); };
    context.manageSearchIndex = [this](QWidget *parent) { showSearchIndexOver(parent); };
    context.clearSearchHistory = [this]() {
        if (auto *action = msgListWidget->findChild<QAction *>(QStringLiteral("clearSearchHistory")))
            action->trigger();
    };
    context.confirm = [](QWidget *parent, const QString &title, const QString &text) {
        return QMessageBox::question(parent, title, text) == QMessageBox::Yes;
    };
    context.chooseFile = [](QWidget *parent, const QString &title, SettingsContext::FileMode mode, const QString &start) {
        switch (mode) {
        case SettingsContext::FileMode::Directory:
            return QFileDialog::getExistingDirectory(parent, title, start);
        case SettingsContext::FileMode::Open:
            return QFileDialog::getOpenFileName(parent, title, start);
        case SettingsContext::FileMode::Save:
            return QFileDialog::getSaveFileName(parent, title, start);
        }
        return QString();
    };
    context.nextcloudAccounts = [this]() {
        QList<SettingsContext::NextcloudAccount> accounts;
        for (const Nextcloud::Account &account : m_nextcloud->accounts())
            accounts.append({account.id, account.title(), account.source == Nextcloud::Account::Source::OnlineAccounts});
        return accounts;
    };
    context.connectNextcloud = [this](QWidget *parent, std::function<void()> changed) {
        connectNextcloud(parent, [changed](const QString &) {
            if (changed)
                changed();
        });
    };
    context.disconnectNextcloud = [this](QWidget *parent, const QString &id, std::function<void(const QString &)> done) {
        m_nextcloud->removeAccount(id, parent, std::move(done));
    };
    context.legacy = m_settings;
    return context;
}

void MainWindow::connectNextcloud(QWidget *parent, std::function<void(const QString &id)> connected)
{
    auto *dialog = new NextcloudConnectDialog(m_nextcloud, parent ? parent : this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &NextcloudConnectDialog::connected, this, [connected = std::move(connected)](const QString &id) {
        if (connected)
            connected(id);
    });
    dialog->open();
}

NextcloudBrowserDialog *MainWindow::createNextcloudBrowser(int mode, QWidget *parent)
{
    auto *browser = new NextcloudBrowserDialog(m_nextcloud, NextcloudBrowserDialog::Mode(mode), parent ? parent : this);
    browser->setAttribute(Qt::WA_DeleteOnClose);
    connect(browser, &NextcloudBrowserDialog::connectRequested, this, [this, browser]() {
        QPointer<NextcloudBrowserDialog> guard(browser);
        connectNextcloud(browser, [guard](const QString &id) {
            if (guard)
                guard->setAccount(id);
        });
    });
    return browser;
}

void MainWindow::browseNextcloud()
{
    if (!m_nextcloudBrowser)
        m_nextcloudBrowser = createNextcloudBrowser(int(NextcloudBrowserDialog::Mode::Browse), this);
    m_nextcloudBrowser->show();
    m_nextcloudBrowser->raise();
    m_nextcloudBrowser->activateWindow();
}

void MainWindow::saveAttachmentToNextcloud(const QPersistentModelIndex &part)
{
    if (!part.isValid())
        return;
    auto *browser = createNextcloudBrowser(int(NextcloudBrowserDialog::Mode::ChooseFolder), this);
    browser->setWindowTitle(i18nc("@title:window", "Save Attachment to Nextcloud"));
    connect(browser, &NextcloudBrowserDialog::folderChosen, this, [this, part](const QString &accountId, const QString &folder) {
        m_attachmentPanel->readPart(part, this, [this, accountId, folder](const QString &fileName, const QByteArray &data) {
            m_nextcloudTransfers->store(
                accountId, folder, fileName, data, this,
                [this, accountId, folder](const QString &path) {
                    const QUrl page = m_nextcloud->account(accountId).webFolderUrl(folder);
                    m_notificationBar->showInfo(
                        i18nc("@info %1 is a file name, %2 a folder in Nextcloud", "Saved \"%1\" to %2 in Nextcloud.",
                              path.section(QLatin1Char('/'), -1),
                              folder),
                        {{i18nc("@action:button", "Open in Browser"), QStringLiteral("internet-services"),
                          QStringLiteral("openNextcloudFolder"), [page]() { QDesktopServices::openUrl(page); }}});
                },
                [this, fileName](const QString &message) {
                    m_notificationBar->showError(
                        i18nc("@info %1 is a file name, %2 an error message", "Could not save \"%1\" to Nextcloud: %2",
                              fileName, message));
                });
        });
    });
    browser->open();
}

void MainWindow::showSearchIndexOver(QWidget *parent)
{
    Q_UNUSED(parent);
    slotShowSearchIndex();
    if (!m_searchIndexDialog)
        return;
    // A modal settings dialog blocks the windows which are not modal themselves
    m_searchIndexDialog->hide();
    m_searchIndexDialog->setWindowModality(Qt::ApplicationModal);
    m_searchIndexDialog->show();
    m_searchIndexDialog->raise();
}

void MainWindow::findSavedFiles(const QString &text)
{
    if (!m_savedFilesDialog) {
        m_savedFilesDialog = new SavedFilesSearchDialog(this);
        m_savedFilesDialog->setAttribute(Qt::WA_DeleteOnClose);
    }
    m_savedFilesDialog->setSearchText(text);
    m_savedFilesDialog->setFolder(Util::savedFilesFolder());
    m_savedFilesDialog->show();
    m_savedFilesDialog->raise();
    m_savedFilesDialog->activateWindow();
}

void MainWindow::slotShowSearchIndex()
{
    if (!m_searchIndexDialog) {
        m_searchIndexDialog = new SearchIndexDialog(this);
        m_searchIndexDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_searchIndexDialog, &SearchIndexDialog::pausedChanged, this, [this](bool paused) {
            // The dialog shows the index of the shown account
            if (m_imapAccess)
                m_imapAccess->setSearchIndexPaused(paused);
        });
        connect(m_searchIndexDialog, &SearchIndexDialog::excludedFoldersChanged, this, [this](const QStringList &folders) {
            if (m_imapAccess)
                m_imapAccess->setSearchIndexExcluded(folders);
        });
        connect(m_searchIndexDialog, &SearchIndexDialog::deleteRequested, this, [this]() {
            if (!m_imapAccess || !m_searchIndexDialog)
                return;
            QString problem;
            const bool deleted = m_imapAccess->deleteSearchIndex(&problem);
            m_searchIndexDialog->setIndexer(nullptr, m_accounts->displayName(m_accounts->activeAccount()));
            m_searchIndexDialog->showDeleted();
            if (!deleted) {
                Gui::Util::messageBoxWarning(m_searchIndexDialog, i18nc("@title:window", "Search Index Not Deleted"),
                                             i18nc("@info %1 is the reason",
                                                   "The offline search index could not be deleted completely: %1",
                                                   problem));
            }
        });
    }
    m_searchIndexDialog->setIndexer(m_imapAccess ? m_imapAccess->searchIndexer() : nullptr,
                                    m_accounts->displayName(m_accounts->activeAccount()));
    m_searchIndexDialog->show();
    m_searchIndexDialog->raise();
    m_searchIndexDialog->activateWindow();
}

void MainWindow::slotShowAccountSettings()
{
    SettingsDialog *dialog = new SettingsDialog(this, SettingsDialog::Scope::Account, m_senderIdentities,
                                                accountIdentitiesModel(), m_favoriteTags, m_settings);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(i18nc("@title:window %1 is the account name", "Account Settings: %1",
                                 m_accounts->displayName(m_accounts->activeAccount())));
    if (dialog->exec() == QDialog::Accepted) {
        setUnifiedInbox(false, false);
        if (Pop3Fetcher *fetcher = m_pop3Fetchers.value(m_accounts->activeAccount()))
            fetcher->settingsChanged();
        if (JmapSyncer *syncer = m_jmapSyncers.value(m_accounts->activeAccount()))
            syncer->settingsChanged();
        // FIXME: wipe cache in case we're moving between servers
        commitFolderPosition();
        nukeModels();
        setupModels();
        connectModelActions();
        // The systray is still connected to the old model -- got to make sure it's getting updated
        removeSysTray();
        slotToggleSysTray();
    }
    QString method = accountSettings()->value(Common::SettingsNames::imapMethodKey).toString();
    if (m_imapAccess->protocol() == Imap::AccountProtocol::Pop3) {
        if (accountSettings()->value(Common::SettingsNames::pop3HostKey).toString().isEmpty())
            Gui::Util::messageBoxCritical(this, i18nc("@title:window", "No Configuration"),
                                          i18nc("@info", "No POP3 server is configured. Plasma Impresario cannot "
                                                         "download mail for this account without one."));
    } else if (m_imapAccess->protocol() == Imap::AccountProtocol::Jmap) {
        if (accountSettings()->value(Common::SettingsNames::jmapUrlKey).toString().isEmpty())
            Gui::Util::messageBoxCritical(this, i18nc("@title:window", "No Configuration"),
                                          i18nc("@info", "No JMAP server is configured. Plasma Impresario cannot "
                                                         "synchronize mail for this account without one."));
    } else if (method != Common::SettingsNames::methodTCP && method != Common::SettingsNames::methodSSL &&
            method != Common::SettingsNames::methodProcess ) {
        Gui::Util::messageBoxCritical(this, tr("No Configuration"),
                                      tr("No IMAP account is configured. Plasma Impresario cannot do much without one."));
    }
    applySizesAndState();
}

namespace {
const QString legacyAccountKey = QStringLiteral("accounts/legacy");

bool usesOAuth(const QSettings &settings)
{
    return OAuth::isEnabled(settings, OAuth::Service::Imap) || OAuth::isEnabled(settings, OAuth::Service::Smtp);
}

OAuth::TokenStore::PluginGetter passwordGetter(Plugins::PluginManager *manager)
{
    QPointer<Plugins::PluginManager> plugins = manager;
    return [plugins]() -> Plugins::PasswordPlugin * { return plugins ? plugins->password() : nullptr; };
}

/** @short Revoke and delete a stored grant without an account session, e.g. of a removed account */
void revokeDetached(const OAuth::AccountConfig &config, Plugins::PluginManager *manager)
{
    auto *session = new OAuth::Session(config, passwordGetter(manager), nullptr);
    QObject::connect(session, &OAuth::Session::signedOut, session, &QObject::deleteLater);
    QObject::connect(session, &OAuth::Session::signOutFailed, session, &QObject::deleteLater);
    session->signOut();
}
}

QSettings *MainWindow::accountSettings() const
{
    return m_accounts->settings(currentAccountId());
}

Composer::SenderIdentitiesModel *MainWindow::senderIdentitiesModel()
{
    switch (Composer::AccountIdentities::source(*accountSettings())) {
    case Composer::AccountIdentities::Source::Shared:
        return m_senderIdentities;
    case Composer::AccountIdentities::Source::None:
        if (!m_noIdentities)
            m_noIdentities = new Composer::SenderIdentitiesModel(this);
        return m_noIdentities;
    case Composer::AccountIdentities::Source::Own:
        break;
    }
    return accountIdentitiesModel();
}

Composer::SenderIdentitiesModel *MainWindow::accountIdentitiesModel()
{
    const QString accountId = currentAccountId();
    if (Composer::SenderIdentitiesModel *existing = m_accountIdentities.value(accountId))
        return existing;
    auto *identities = new Composer::SenderIdentitiesModel(this);
    identities->loadFromSettings(*accountSettings());
    m_accountIdentities.insert(accountId, identities);
    return identities;
}

void MainWindow::setupAccounts()
{
    const QFileInfo main(m_settings->fileName());
    const QString prefix = main.path() + QLatin1Char('/') + main.completeBaseName() + QLatin1Char('-');
    m_accounts = new Common::AccountRegistry(m_settings, [prefix](const QString &accountId) {
        return std::make_unique<QSettings>(prefix + accountId + QStringLiteral(".conf"), QSettings::IniFormat);
    }, this);
    m_accountStatus = new Common::AccountStatusTracker(this);
    m_accountNotices = new UiUtils::AccountNotices(this);
    m_accountNotices->setObjectName(QStringLiteral("accountNotices"));
    connect(m_accountStatus, &Common::AccountStatusTracker::statusChanged, this, &MainWindow::mailStatusChanged);
    connect(this, &MainWindow::accountsChanged, this, &MainWindow::mailStatusChanged);
    connect(m_accountStatus, &Common::AccountStatusTracker::statusChanged, this, [this](const QString &accountId) {
        if (m_accountSwitcher)
            refreshAccountSwitcher();
        // A notification about a problem which is solved would only mislead
        if (m_accountStatus->status(accountId).kind == Common::AccountStatusKind::Connected)
            m_accountNotices->close(accountId);
    });
    m_sleepMonitor = new UiUtils::SleepMonitor(this);
    m_sleepMonitor->setObjectName(QStringLiteral("sleepMonitor"));
    m_sleepMonitor->startOnSystemBus();
    connect(m_sleepMonitor, &UiUtils::SleepMonitor::sleepingChanged, this, [this]() {
        for (Imap::ImapAccess *access : std::as_const(m_accountAccess))
            applySystemSleep(access);
    });
    // The shown account reports its own errors in the footer and in dialogs.
    connect(m_accountStatus, &Common::AccountStatusTracker::attentionRaised, this, [this](const QString &accountId) {
        if (accountId != m_accounts->activeAccount() && m_accounts->accountIds().contains(accountId))
            notifyBackgroundAccount(accountId);
    });
    if (m_accounts->accountIds().isEmpty()) {
        const QString accountId = m_accounts->addAccount(i18n("Mail account"));
        Common::AccountRegistry::copyAccountKeys(*m_settings, *m_accounts->settings(accountId));
        m_settings->setValue(legacyAccountKey, accountId);
    }
    setupMailAutomation();
    setupOutbox();
    m_imapAccess = accountAccess(m_accounts->activeAccount());
}

void MainWindow::setupMailAutomation()
{
    m_mailAutomation = new MailAutomation(KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")),
                                          KSharedConfig::openStateConfig(), this);
    m_mailAutomation->setObjectName(QStringLiteral("mailAutomation"));
    m_mailAutomation->setActiveAccount(m_accounts->activeAccount());
    m_mailAutomation->setWindowActive(isActiveWindow());
    m_mailAutomation->setFolderResolver([this](const QString &accountId) {
        MailAutomation::Folders folders;
        withAccount(accountId, [this, &folders]() {
            folders.archive = archiveTarget();
            if (const Imap::Mailbox::Model *model = imapModel())
                folders.trash = model->specialFolderMailbox(Imap::Mailbox::SpecialUse::Trash);
        });
        return folders;
    });
    connect(m_mailAutomation, &MailAutomation::statusMessage, this, &MainWindow::showStatusMessage);
    connect(m_mailAutomation, &MailAutomation::openRequested, this, &MainWindow::openAnnouncedMail);
    connect(m_mailAutomation, &MailAutomation::attention, this,
            [this](const UiUtils::ActivityLog::Entry &entry, UiUtils::OutputSurfaces::Surfaces surfaces) {
        using UiUtils::ActivityLog;
        using UiUtils::OutputSurfaces::Surface;
        const QString text = entry.error.isEmpty() ? entry.title
            : i18nc("@info %1 is what failed, %2 the reason", "%1: %2", entry.title, entry.error);
        if (surfaces.testFlag(Surface::InlineBar) && m_notificationBar) {
            if (entry.action == ActivityLog::Action::ReviewRule) {
                const QString ruleId = entry.target;
                m_notificationBar->showMessage(InlineNotificationBar::Kind::Info, text,
                    {{i18nc("@action:button", "Review Rule…"), QStringLiteral("view-filter"),
                      QStringLiteral("reviewAutomationRule"), [this, ruleId]() { showMailRules(ruleId); }}});
            } else {
                m_notificationBar->showMessage(InlineNotificationBar::Kind::Error, text,
                    {{i18nc("@action:button", "Show Activity…"), QStringLiteral("view-history"),
                      QStringLiteral("showAutomationActivity"), [this]() { showActivity(); }}});
            }
        }
#ifdef IMPRESARIO_HAVE_KNOTIFICATIONS
        if (surfaces.testFlag(Surface::DesktopNotification)) {
            auto *notification = new KNotification(QStringLiteral("automationProblem"), KNotification::CloseOnTimeout);
            notification->setComponentName(QStringLiteral("plasma-impresario"));
            notification->setTitle(entry.title);
            notification->setText((entry.error.isEmpty() ? entry.detail : entry.error).toHtmlEscaped());
            notification->setWindow(windowHandle());
            KNotificationAction *open = notification->addDefaultAction(i18nc("@action:button", "Show Activity"));
            connect(open, &KNotificationAction::activated, this, [this, notification]() {
                setActivationToken(notification->xdgActivationToken());
                showMainWindow();
                showActivity();
            });
            notification->sendEvent();
        }
#endif
    });
    const auto updateNames = [this]() {
        for (const QString &accountId : m_accounts->accountIds())
            m_mailAutomation->setAccountName(accountId, m_accounts->displayName(accountId));
    };
    connect(m_accountStatus, &Common::AccountStatusTracker::statusChanged, this, &MainWindow::recordSyncActivity);
    MailSafetyPrompt::setActivityLog(m_mailAutomation->activity());
    connect(this, &MainWindow::accountsChanged, m_mailAutomation, updateNames);
}

void MainWindow::recordSyncActivity(const QString &accountId)
{
    using UiUtils::ActivityLog;
    using Kind = Common::AccountStatusKind;
    if (!m_accounts->accountIds().contains(accountId))
        return;
    const Common::AccountStatus status = m_accountStatus->status(accountId);
    if (status.kind == Kind::Idle)
        return;
    ActivityLog::Entry entry;
    entry.key = QLatin1String("sync:") + accountId;
    entry.kind = ActivityLog::Kind::Sync;
    entry.accountId = accountId;
    entry.title = i18nc("@info %1 is an account", "Connection of %1", m_accounts->displayName(accountId));
    entry.detail = Common::AccountStatusTracker::label(status.kind);
    switch (status.kind) {
    case Kind::Connecting:
        entry.state = ActivityLog::State::Running;
        break;
    case Kind::Connected:
        entry.state = ActivityLog::State::Done;
        break;
    case Kind::Offline:
        entry.state = ActivityLog::State::Waiting;
        break;
    case Kind::AuthRequired:
    case Kind::AuthFailed:
        entry.state = ActivityLog::State::Failed;
        entry.error = status.detail.isEmpty() ? entry.detail : status.detail;
        entry.action = usesOAuth(*m_accounts->settings(accountId)) ? ActivityLog::Action::SignIn
                                                                  : ActivityLog::Action::OpenAccount;
        entry.target = accountId;
        break;
    case Kind::NetworkError: {
        entry.state = ActivityLog::State::Failed;
        entry.error = status.detail.isEmpty() ? entry.detail : status.detail;
        const QDateTime at = m_reconnectAt.value(accountId);
        if (at.isValid() && at > QDateTime::currentDateTime())
            entry.error = i18nc("@info %1 is an error, %2 a time", "%1 Trying again at %2.", entry.error,
                QLocale().toString(at.time(), QLocale::ShortFormat));
        entry.action = ActivityLog::Action::Retry;
        entry.target = accountId;
        break;
    }
    case Kind::Idle:
        return;
    }
    if (status.kind != Kind::NetworkError)
        m_reconnectAt.remove(accountId);
    m_mailAutomation->activity()->record(entry);
    if (entry.action == ActivityLog::Action::Retry)
        m_mailAutomation->activity()->setRetry(entry.key, [this, accountId]() { reconnectAccount(accountId); });
}

void MainWindow::reconnectAccount(const QString &accountId)
{
    if (Pop3Fetcher *fetcher = m_pop3Fetchers.value(accountId)) {
        fetcher->check(accountId == m_accounts->activeAccount());
        return;
    }
    if (JmapSyncer *syncer = m_jmapSyncers.value(accountId)) {
        syncer->sync(accountId == m_accounts->activeAccount());
        return;
    }
    Imap::ImapAccess *access = m_accountAccess.value(accountId);
    auto *watcher = access ? qobject_cast<Imap::Mailbox::NetworkWatcher *>(access->networkWatcher()) : nullptr;
    if (!watcher)
        return;
    m_reconnectAt.remove(accountId);
    watcher->reconnectNow();
}

void MainWindow::registerAutomationAccount(const QString &accountId, Imap::ImapAccess *access)
{
    if (!m_mailAutomation)
        return;
    // POP3 and JMAP accounts read their mail from a local store which the automation does not watch
    auto *model = qobject_cast<Imap::Mailbox::Model *>(access->imapModel());
    if (access->protocol() == Imap::AccountProtocol::Imap && model)
        m_mailAutomation->addAccount(accountId, m_accounts->displayName(accountId), model);
    else
        m_mailAutomation->removeAccount(accountId);
}

QList<QPair<QString, QString>> MainWindow::automationAccounts() const
{
    QList<QPair<QString, QString>> accounts;
    const QStringList covered = m_mailAutomation ? m_mailAutomation->accounts() : QStringList();
    for (const QString &accountId : m_accounts->accountIds()) {
        if (covered.contains(accountId))
            accounts.append({accountId, m_accounts->displayName(accountId)});
    }
    return accounts;
}

void MainWindow::showMailRules(const QString &ruleId, const std::function<void(RulesDialog &)> &prepare)
{
    RulesDialog dialog(automationAccounts(), this);
    dialog.setRules(m_mailAutomation->rules());
    const QString accountId = m_accounts->activeAccount();
    const QString folder = currentMailboxName();
    if (!showsUnifiedList() && !folder.isEmpty() && m_imapAccess && m_mailAutomation->accounts().contains(accountId)) {
        const QPointer<QAbstractItemModel> list = m_imapAccess->msgListModel();
        dialog.setShownFolder(accountId, folder, [list, accountId, folder]() {
            QList<UiUtils::MailRules::Message> messages;
            if (!list)
                return messages;
            for (int row = 0; row < list->rowCount(); ++row) {
                const QModelIndex index = list->index(row, 0);
                if (index.data(Imap::Mailbox::RoleIsFetched).toBool())
                    messages.append(UiUtils::NewMailWatcher::message(index, accountId, folder));
            }
            return messages;
        });
        dialog.setApplier([this](const QList<UiUtils::MailRules::Rule> &rules, const QString &account,
                                 const QString &mailbox, const QList<UiUtils::MailRules::Message> &messages) {
            return m_mailAutomation->apply(rules, account, mailbox, messages);
        });
    }
    dialog.selectRule(ruleId);
    if (prepare) {
        QMetaObject::invokeMethod(&dialog, [&dialog, prepare]() { prepare(dialog); }, Qt::QueuedConnection);
    }
    if (dialog.exec() == QDialog::Accepted)
        m_mailAutomation->setRules(dialog.rules());
}

void MainWindow::createRuleFromSearch()
{
    const QString text = msgListWidget->searchText().trimmed();
    if (text.isEmpty()) {
        m_notificationBar->showInfo(i18nc("@info", "Type a search in the message list first; the rule matches what "
                                                   "it finds."));
        return;
    }
    offerRuleFromQuery(text, text);
}

void MainWindow::createRuleFromSavedSearch(const QString &owner, const QString &id)
{
    const KConfigGroup group = MessageListWidget::savedSearchesRoot().group(owner);
    const UiUtils::SavedSearch search = group.hasGroup(id) ? UiUtils::SavedSearches::read(group.group(id), owner)
                                                           : UiUtils::SavedSearch();
    if (!search.isValid()) {
        reloadSavedSearchFolders();
        return;
    }
    offerRuleFromQuery(search.query, search.name);
}

void MainWindow::offerRuleFromQuery(const QString &text, const QString &name)
{
    UiUtils::MailRules::Conversion conversion =
        UiUtils::MailRules::fromQuery(UiUtils::SearchQuery::Query::parse(text, QDate::currentDate()));
    if (!conversion.isUsable()) {
        m_notificationBar->showWarning(
            i18nc("@info %1 lists parts of a search", "Rules cannot check every part of this search: %1. Change the "
                                                      "search or make the rule by hand.",
                  conversion.unsupported.join(QLatin1String(", "))),
            {{i18nc("@action:button", "Mail Rules…"), QStringLiteral("view-filter"), QStringLiteral("openMailRules"),
              [this]() { showMailRules(); }}});
        return;
    }
    UiUtils::MailRules::Rule rule = conversion.rule;
    rule.name = name;
    rule.accountId = m_accounts->activeAccount();
    if (!conversion.notes.isEmpty())
        m_notificationBar->showInfo(conversion.notes.join(QLatin1Char(' ')));
    showMailRules(QString(), [rule](RulesDialog &dialog) { dialog.addRule(rule); });
}

void MainWindow::showActivity()
{
    if (!m_activityDialog) {
        m_activityDialog = new ActivityDialog(m_mailAutomation->activity(), this);
        m_activityDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_activityDialog, &ActivityDialog::actionRequested, this, [this](const UiUtils::ActivityLog::Entry &entry) {
            using Action = UiUtils::ActivityLog::Action;
            switch (entry.action) {
            case Action::ReviewRule:
                showMailRules(entry.target);
                break;
            case Action::OpenSettings:
                showMailRules();
                break;
            case Action::OpenAccount:
                switchAccount(entry.target);
                showMainWindow();
                break;
            case Action::SignIn:
                signInAccount(entry.target);
                break;
            case Action::Retry:
                m_mailAutomation->activity()->retry(entry.key);
                break;
            case Action::OpenDraft:
                openSavedDraft(entry.target);
                break;
            case Action::OpenOutbox:
                showOutbox();
                break;
            case Action::None:
                break;
            }
        });
    }
    m_activityDialog->show();
    m_activityDialog->raise();
    m_activityDialog->activateWindow();
}

bool MainWindow::canQueueMessages(const QString &accountId) const
{
    using Common::SettingsNames;
    const QSettings *account = m_accounts->accountIds().contains(accountId) ? m_accounts->settings(accountId) : nullptr;
    if (!account)
        return false;
    if (JmapSyncer::sendsThroughJmap(*account))
        return true;
    // Sending through IMAP needs the message on the server first, which the outbox does not do
    const QString method = account->value(SettingsNames::msaMethodKey).toString();
    return method == SettingsNames::methodSMTP || method == SettingsNames::methodSSMTP
        || method == SettingsNames::methodSENDMAIL;
}

bool MainWindow::sendsOverNetwork(const QString &accountId) const
{
    const QSettings *account = m_accounts->accountIds().contains(accountId) ? m_accounts->settings(accountId) : nullptr;
    return account
        && (JmapSyncer::sendsThroughJmap(*account)
            || account->value(Common::SettingsNames::msaMethodKey).toString() != Common::SettingsNames::methodSENDMAIL);
}

void MainWindow::openSavedDraft(const QString &path)
{
    if (path.endsWith(QLatin1String(".draft")))
        ComposeWidget::warnIfMsaNotConfigured(ComposeWidget::createDraft(this, path), this);
    else if (!path.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

bool MainWindow::outboxAccountOnline(const QString &accountId) const
{
    const Imap::ImapAccess *access = m_accountAccess.value(accountId);
    const auto *model = access ? qobject_cast<const Imap::Mailbox::Model *>(access->imapModel()) : nullptr;
    return model && model->isNetworkAvailable();
}

void MainWindow::recordOutboxEntry(const Composer::Outbox::Entry &message, UiUtils::ActivityLog::State state,
    const QString &detail, const QString &error, UiUtils::ActivityLog::Action action, const QString &target)
{
    using UiUtils::ActivityLog;
    if (!m_mailAutomation)
        return;
    ActivityLog *log = m_mailAutomation->activity();
    ActivityLog::Entry entry;
    entry.key = QLatin1String("outbox:") + message.id;
    entry.kind = ActivityLog::Kind::Job;
    entry.state = state;
    entry.accountId = message.accountId;
    entry.title = i18nc("@info message subject", "Sending “%1”",
        message.subject.trimmed().isEmpty() ? i18nc("@info", "(no subject)") : message.subject.trimmed());
    entry.detail = detail;
    entry.error = error;
    entry.action = action;
    entry.target = target;
    log->record(entry);
    if (entry.action != ActivityLog::Action::Retry)
        return;
    const QString id = message.id;
    log->setRetry(entry.key, [this, id]() {
        const Composer::Outbox::Entry current = m_outbox->entry(id);
        if (current.id.isEmpty())
            return;
        if (!outboxAccountOnline(current.accountId)) {
            recordOutboxEntry(current, ActivityLog::State::Waiting,
                i18nc("@info", "Waiting for the account to be online"), QString(), ActivityLog::Action::OpenOutbox);
            return;
        }
        m_outbox->sendNow(id);
    });
}

void MainWindow::recordOutboxState(const Composer::Outbox::Entry &message)
{
    using UiUtils::ActivityLog;
    using State = Composer::Outbox::State;
    const auto record = [&](ActivityLog::State state, const QString &detail, const QString &error,
                            ActivityLog::Action action) { recordOutboxEntry(message, state, detail, error, action); };
    switch (message.state) {
    case State::Queued:
        if (message.attempts > 0) {
            record(ActivityLog::State::Failed,
                message.nextAttempt.isValid()
                    ? i18nc("@info time", "Trying again at %1",
                          QLocale().toString(message.nextAttempt.toLocalTime().time(), QLocale::ShortFormat))
                    : i18nc("@info", "Trying again soon"),
                message.lastError, ActivityLog::Action::Retry);
        } else {
            record(ActivityLog::State::Waiting,
                outboxAccountOnline(message.accountId) ? i18nc("@info", "Waiting to be sent")
                                                       : i18nc("@info", "Waiting for the account to be online"),
                QString(), ActivityLog::Action::OpenOutbox);
        }
        break;
    case State::Sending:
        record(ActivityLog::State::Running, i18nc("@info", "Handing the message to the outgoing server"), QString(),
            ActivityLog::Action::None);
        break;
    case State::Submitted:
        record(message.attempts > 0 ? ActivityLog::State::Failed : ActivityLog::State::Running,
            i18nc("@info folder name", "Sent; the copy is still to be filed in %1", message.sentFolder),
            message.lastError, message.attempts > 0 ? ActivityLog::Action::Retry : ActivityLog::Action::None);
        break;
    case State::Interrupted:
        record(ActivityLog::State::Failed,
            i18nc("@info", "The message may or may not have been sent; the outbox asks what to do"), message.lastError,
            ActivityLog::Action::OpenOutbox);
        break;
    case State::Refused:
        record(ActivityLog::State::Failed,
            i18nc("@info", "The server refused the message, and it could not be moved to the drafts"),
            message.lastError, ActivityLog::Action::OpenOutbox);
        break;
    }
}

void MainWindow::setupOutbox()
{
    using Composer::Outbox;
    using UiUtils::ActivityLog;
    const QString data = Common::writablePath(Common::LOCATION_DATA);
    m_outbox = new Outbox(data + QLatin1String("Outbox"), data + tr("Drafts"), this);
    m_outbox->setObjectName(QStringLiteral("outbox"));
    ActivityLog *log = m_mailAutomation->activity();
    connect(m_outbox, &Outbox::sending, this, [this](const QString &id) { recordOutboxState(m_outbox->entry(id)); });
    connect(m_outbox, &Outbox::failed, this, [this](const QString &id) { recordOutboxState(m_outbox->entry(id)); });
    connect(
        m_outbox, &Outbox::interrupted, this, [this](const QString &id) { recordOutboxState(m_outbox->entry(id)); });
    connect(m_outbox, &Outbox::sent, this, [log](const QString &id) {
        const int row = log->find(QLatin1String("outbox:") + id);
        if (row < 0)
            return;
        ActivityLog::Entry entry = log->entry(row);
        entry.state = ActivityLog::State::Done;
        entry.detail = i18nc("@info", "Sent");
        entry.error.clear();
        entry.action = ActivityLog::Action::None;
        entry.time = QDateTime();
        log->record(entry);
    });
    connect(m_outbox, &Outbox::refused, this,
        [this](const Outbox::Entry &message, const QString &reason, const QString &draftPath) {
            if (draftPath.isEmpty()) {
                recordOutboxState(message);
                return;
            }
            recordOutboxEntry(message, ActivityLog::State::Failed,
                i18nc("@info", "The server refused the message; it is back in the drafts"), reason,
                ActivityLog::Action::OpenDraft, draftPath);
        });
    connect(m_outbox, &Outbox::changed, this, [this, log]() {
        updateOutboxButton();
        // Messages which were just queued, or kept from the last session, show up as waiting
        const QList<Outbox::Entry> entries = m_outbox->entries();
        for (const Outbox::Entry &message : entries) {
            if (log->find(QLatin1String("outbox:") + message.id) < 0)
                recordOutboxState(message);
        }
    });
    const QList<Outbox::Entry> entries = m_outbox->entries();
    for (const Outbox::Entry &message : entries)
        recordOutboxState(message);
}

void MainWindow::registerOutboxAccount(const QString &accountId)
{
    if (!m_outbox)
        return;
    Imap::ImapAccess *access = m_accountAccess.value(accountId);
    QPointer<Imap::Mailbox::Model> model = access ? qobject_cast<Imap::Mailbox::Model *>(access->imapModel()) : nullptr;
    if (Imap::Mailbox::HeaderBackfill *backfill = access ? access->headerBackfill() : nullptr) {
        // A message to send goes first; the headers of the open folder wait for it
        QPointer<Composer::Outbox> outbox = m_outbox;
        backfill->setOtherWorkPending([outbox, accountId]() { return outbox && outbox->hasWorkDue(accountId); });
    }
    if (!model || !canQueueMessages(accountId)) {
        m_outbox->removeTransport(accountId);
        return;
    }
    Composer::Outbox::Transport transport;
    transport.createMsa = [this, accountId](QObject *parent) -> MSA::AbstractMSA * {
        MSA::MSAFactory *created = nullptr;
        withAccount(accountId, [this, &created]() { created = msaFactory(); });
        if (!created)
            return nullptr;
        // The factory lives as long as what it creates
        std::shared_ptr<MSA::MSAFactory> factory(created);
        MSA::AbstractMSA *msa = factory->create(parent);
        if (msa)
            connect(msa, &QObject::destroyed, [factory]() { });
        return msa;
    };
    // A local sendmail program takes the message whether the account is online or not
    if (sendsOverNetwork(accountId))
        transport.online = [model]() { return model && model->isNetworkAvailable(); };
    transport.fileSentCopy = [model](const QString &folder, const QByteArray &data, const QDateTime &timestamp,
                                 const Composer::Outbox::FilingReply &reply) {
        if (!model) {
            reply(false, tr("The account is no longer connected."));
            return;
        }
        Imap::Mailbox::ImapTask *task = model->appendIntoMailbox(folder, data, {QStringLiteral("\\Seen")}, timestamp);
        // Exactly one of the two answers
        auto answered = std::make_shared<bool>(false);
        connect(task, &Imap::Mailbox::ImapTask::completed, task, [reply, answered]() {
            if (!std::exchange(*answered, true))
                reply(true, QString());
        });
        connect(task, &Imap::Mailbox::ImapTask::failed, task, [reply, answered](const QString &error) {
            if (!std::exchange(*answered, true))
                reply(false, error);
        });
    };
    QPointer<QSettings> account = m_accounts->settings(accountId);
    QPointer<Imap::ImapAccess> accessGuard = access;
    QPointer<Plugins::PluginManager> plugins = pluginManager();
    transport.password = [account, accessGuard, model, plugins](
                             const QString &, const QString &, const Composer::Outbox::PasswordReply &reply) {
        if (!account || !accessGuard) {
            reply(QString());
            return;
        }
        const bool jmap = JmapSyncer::sendsThroughJmap(*account);
        if (!jmap && account->value(Common::SettingsNames::smtpAuthReuseImapCredsKey, false).toBool()) {
            reply(model ? model->imapPassword() : QString());
            return;
        }
        Plugins::PasswordPlugin *password = plugins ? plugins->password() : nullptr;
        Plugins::PasswordJob *job = password ? password->requestPassword(accessGuard->credentialAccountId(),
                                                   jmap ? JmapSyncer::passwordType() : QStringLiteral("smtp"))
                                             : nullptr;
        if (!job) {
            reply(QString());
            return;
        }
        connect(job, &Plugins::PasswordJob::passwordAvailable, job, [reply](const QString &stored) { reply(stored); });
        connect(job, &Plugins::PasswordJob::error, job,
            [reply](Plugins::PasswordJob::Error, const QString &) { reply(QString()); });
        job->setAutoDelete(true);
        job->start();
    };
    QPointer<QSettings> profile = m_settings;
    QPointer<QSettings> host = account;
    transport.certificate = [profile, host](const QList<QSslCertificate> &chain, const QList<QSslError> &) {
        if (!profile || !host || chain.isEmpty())
            return false;
        const QString smtpHost = host->value(Common::SettingsNames::smtpHostKey).toString();
        return Common::TrustedCertificates::isTrusted(*profile, smtpHost, chain.first());
    };
    m_outbox->setTransport(accountId, transport);
    // Coming online sends what waited for the connection; waking twice only looks at the queue twice
    for (auto signal : {&Imap::Mailbox::Model::networkPolicyOnline, &Imap::Mailbox::Model::networkPolicyExpensive})
        connect(model, signal, m_outbox, [this, accountId]() { m_outbox->wake(accountId); });
}

void MainWindow::updateOutboxButton()
{
    if (!m_outboxButton || !m_outbox)
        return;
    const QList<Composer::Outbox::Entry> entries = m_outbox->entries();
    const auto attention = std::count_if(entries.cbegin(), entries.cend(), [](const Composer::Outbox::Entry &entry) {
        return entry.state == Composer::Outbox::State::Interrupted || entry.state == Composer::Outbox::State::Refused;
    });
    m_outboxButton->setText(i18ncp("@action:button", "%1 to send", "%1 to send", entries.size()));
    m_outboxButton->setToolTip(attention > 0
            ? i18ncp("@info:tooltip", "%1 message waiting to be sent needs your decision",
                  "%1 messages waiting to be sent need your decision", static_cast<int>(attention))
            : i18nc("@info:tooltip", "Show the messages waiting to be sent"));
    m_outboxButton->setVisible(!entries.isEmpty());
}

void MainWindow::showOutbox()
{
    if (!m_outbox)
        return;
    if (!m_outboxDialog) {
        m_outboxDialog
            = new OutboxDialog(m_outbox, [this](const QString &accountId) { return accountName(accountId); }, this);
        m_outboxDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_outboxDialog, &OutboxDialog::draftMoved, this, &MainWindow::openSavedDraft);
    }
    m_outboxDialog->show();
    m_outboxDialog->raise();
    m_outboxDialog->activateWindow();
}

void MainWindow::configureNotifications()
{
    NotificationsDialog dialog(automationAccounts(), m_mailAutomation->notificationSettings(), this);
    dialog.selectAccount(m_accounts->activeAccount());
    if (dialog.exec() == QDialog::Accepted)
        m_mailAutomation->setNotificationSettings(dialog.settings());
}

void MainWindow::openAnnouncedMail(const QString &accountId, const QString &folder, uint uid,
                                   const QString &activationToken)
{
    if (!m_accounts->accountIds().contains(accountId))
        return;
    if (!activationToken.isEmpty())
        setActivationToken(activationToken);
    uint uidValidity = 0;
    if (uid && !folder.isEmpty()) {
        if (Imap::ImapAccess *access = m_accountAccess.value(accountId)) {
            if (auto *model = qobject_cast<Imap::Mailbox::Model *>(access->imapModel()))
                uidValidity = model->mailboxIndexByName(folder).data(Imap::Mailbox::RoleMailboxUidValidity).toUInt();
        }
    }
    // Revealing a message needs to know the folder's UIDVALIDITY; otherwise the folder is shown
    if (uid && uidValidity) {
        revealLocation(accountId, folder, uidValidity, uid);
    } else {
        switchAccount(accountId);
        if (accountId == m_accounts->activeAccount())
            goToMailbox(folder.isEmpty() ? QStringLiteral("INBOX") : folder);
    }
    showMainWindow();
}

void MainWindow::prepareNotifyFolderAction(const QModelIndex &mailbox)
{
    const QString accountId = m_accounts->activeAccount();
    const QString folder = mailbox.data(Imap::Mailbox::RoleMailboxName).toString();
    const UiUtils::NewMail::Settings settings = m_mailAutomation->notificationSettings();
    m_actionNotifyFolder->setData(folder);
    m_actionNotifyFolder->setChecked(!folder.isEmpty() && settings.announces(accountId, folder));
    m_actionNotifyFolder->setEnabled(mailbox.isValid() && !folder.isEmpty() && !showsUnifiedList()
                                     && mailbox.data(Imap::Mailbox::RoleMailboxIsSelectable).toBool()
                                     && m_mailAutomation->accounts().contains(accountId)
                                     && settings.mode(accountId) != UiUtils::NewMail::Mode::Off);
}

QStringList MainWindow::expandedMailboxes() const
{
    const QSettings *account = accountSettings();
    if (account->contains(Common::SettingsNames::guiExpandedMailboxes))
        return account->value(Common::SettingsNames::guiExpandedMailboxes).toStringList();
    // Expansion used to be shared by the whole profile; only the migrated account inherits it.
    if (m_accounts->activeAccount() == m_settings->value(legacyAccountKey).toString())
        return m_settings->value(Common::SettingsNames::guiExpandedMailboxes).toStringList();
    return {};
}

Imap::ImapAccess *MainWindow::accountAccess(const QString &accountId)
{
    if (Imap::ImapAccess *existing = m_accountAccess.value(accountId))
        return existing;
    // The migrated account keeps the profile name so its stored passwords and cache stay reachable.
    const QString name = accountId == m_settings->value(legacyAccountKey).toString()
        ? Common::Application::profileName() : accountId;
    auto *access = new Imap::ImapAccess(this, m_accounts->settings(accountId), m_pluginManager, name, m_settings);
    connect(access, &Imap::ImapAccess::cacheError, this, &MainWindow::cacheError);
    // Folders chosen for saved searches are only known to be gone once their account's folders were listed
    const auto watchMailboxes = [this, access]() {
        QAbstractItemModel *mailboxes = access->mailboxModel();
        if (!mailboxes)
            return;
        const auto schedule = [this]() {
            if (m_savedSearchProblemsTimer && !m_savedSearchProblemsTimer->isActive())
                m_savedSearchProblemsTimer->start();
        };
        schedule();
        // Lambdas cannot be unique connections, so a model which is reused keeps its first ones
        static const char watched[] = "savedSearchProblemsWatched";
        if (mailboxes->property(watched).toBool())
            return;
        mailboxes->setProperty(watched, true);
        connect(mailboxes, &QAbstractItemModel::rowsInserted, this, schedule);
        connect(mailboxes, &QAbstractItemModel::rowsRemoved, this, schedule);
        connect(mailboxes, &QAbstractItemModel::modelReset, this, schedule);
        connect(mailboxes, &QAbstractItemModel::dataChanged, this, schedule);
    };
    connect(access, &Imap::ImapAccess::modelsChanged, this, watchMailboxes);
    watchMailboxes();
    connect(access, &Imap::ImapAccess::modelsChanged, this, &MainWindow::scheduleAgendaMessages);
    scheduleAgendaMessages();
    connect(access, &Imap::ImapAccess::checkSslPolicy, this, [this, accountId]() { checkSslPolicy(accountId); },
            Qt::QueuedConnection);
    connect(access, &Imap::ImapAccess::modelsChanged, this, [this, access]() { applySystemSleep(access); });
    m_accountAccess.insert(accountId, access);
    applySystemSleep(access);
    connect(access, &Imap::ImapAccess::modelsChanged, this,
            [this, accountId, access]() { registerAutomationAccount(accountId, access); });
    registerAutomationAccount(accountId, access);
    return access;
}

QString MainWindow::savedSearchOwner(const QString &accountId) const
{
    // As ImapAccess::credentialAccountId(), without creating the access of an account which is not running
    if (accountId != m_settings->value(legacyAccountKey).toString())
        return accountId;
    const QString profile = Common::Application::profileName();
    return profile.isEmpty() ? QStringLiteral("account-0") : profile;
}

void MainWindow::reloadSavedSearchFolders()
{
    if (!m_savedSearchFolders)
        return;
    QStringList owners;
    QHash<QString, QString> names;
    for (const QString &accountId : m_accounts->accountIds()) {
        const QString owner = savedSearchOwner(accountId);
        owners << owner;
        names.insert(owner, m_accounts->displayName(accountId));
    }
    // Searches of removed accounts stay until the user moves or deletes them
    const QList<UiUtils::SavedSearch> searches =
        UiUtils::SavedSearches::load(MessageListWidget::savedSearchesRoot(), owners, UiUtils::SavedSearches::Orphans::Include);
    m_savedSearchFolders->setSearches(searches, owners, names);
    KConfigGroup counts(KSharedConfig::openStateConfig(), QStringLiteral("SavedSearchCounts"));
    UiUtils::SavedSearches::pruneCounts(counts, searches);
    for (const UiUtils::SavedSearch &search : searches)
        m_savedSearchFolders->setCounts(search.owner, search.id, UiUtils::SavedSearches::readCounts(counts, search.owner, search.id));
    m_savedSearchFolders->setActive(msgListWidget->activeSavedSearchOwner(), msgListWidget->activeSavedSearchId());
    refreshSavedSearchProblems();
    scheduleSavedSearchCount();
}

void MainWindow::refreshSavedSearchProblems()
{
    if (!m_savedSearchFolders)
        return;
    QStringList owners;
    const QStringList accountIds = m_accounts->accountIds();
    for (const QString &accountId : accountIds)
        owners << savedSearchOwner(accountId);
    const auto presence = [this](const UiUtils::SearchScope::Folder &folder) { return folderPresence(folder); };
    const QList<UiUtils::SavedSearch> searches = m_savedSearchFolders->searches();
    for (const UiUtils::SavedSearch &search : searches) {
        m_savedSearchFolders->setProblems(search.owner, search.id,
                                          UiUtils::SavedSearches::problems(search, owners, accountIds, presence));
    }
}

UiUtils::SavedSearches::FolderPresence MainWindow::folderPresence(const UiUtils::SearchScope::Folder &folder) const
{
    using UiUtils::SavedSearches::FolderPresence;
    Imap::ImapAccess *access = m_accountAccess.value(folder.accountId);
    auto *model = access ? qobject_cast<Imap::Mailbox::Model *>(access->imapModel()) : nullptr;
    if (!model || folder.mailbox.isEmpty())
        return FolderPresence::Unknown;
    if (model->mailboxIndexByName(folder.mailbox).isValid())
        return FolderPresence::Present;
    // Every server has an INBOX; without it, the top folders have not been listed yet
    const QModelIndex inbox = model->mailboxIndexByName(QStringLiteral("INBOX"));
    if (!inbox.isValid())
        return FolderPresence::Unknown;
    const QString separator = inbox.data(Imap::Mailbox::RoleMailboxSeparator).toString();
    QString parent = folder.mailbox;
    while (!separator.isEmpty()) {
        const qsizetype cut = parent.lastIndexOf(separator);
        if (cut <= 0)
            break;
        parent.truncate(cut);
        const QModelIndex index = model->mailboxIndexByName(parent);
        // The nearest listed folder above it decides: gone only when its subfolders were listed without it
        if (index.isValid())
            return index.data(Imap::Mailbox::RoleIsFetched).toBool() ? FolderPresence::Missing : FolderPresence::Unknown;
    }
    return FolderPresence::Missing;
}

void MainWindow::activateSavedSearch(const QString &owner, const QString &id)
{
    openSavedSearch(owner, id, false);
}

void MainWindow::editSavedSearch(const QString &owner, const QString &id)
{
    openSavedSearch(owner, id, true);
}

void MainWindow::openSavedSearch(const QString &owner, const QString &id, bool edit)
{
    const KConfigGroup group = MessageListWidget::savedSearchesRoot().group(owner);
    const UiUtils::SavedSearch search = group.hasGroup(id) ? UiUtils::SavedSearches::read(group.group(id), owner)
                                                           : UiUtils::SavedSearch();
    if (!search.isValid()) {
        // Deleted or changed meanwhile, such as by another window
        reloadSavedSearchFolders();
        return;
    }
    if (search.dependsOnOwner()) {
        const QStringList accountIds = m_accounts->accountIds();
        const auto account = std::find_if(accountIds.cbegin(), accountIds.cend(),
                                          [this, &owner](const QString &accountId) { return savedSearchOwner(accountId) == owner; });
        if (account == accountIds.cend()) {
            const QString shownOwner = savedSearchOwner(m_accounts->activeAccount());
            const QString name = search.name;
            m_notificationBar->showInfo(
                i18nc("@info %1 is the name of a saved search", "The account of the saved search \"%1\" was removed, "
                                                                "so it cannot run.", name),
                {
                    {i18nc("@action:button %1 is an account", "Move to %1", m_accounts->displayName(m_accounts->activeAccount())),
                     QStringLiteral("user-identity"), QStringLiteral("moveSavedSearchToShownAccount"),
                     [this, owner, id, shownOwner]() { changeSavedSearchOwner(owner, id, shownOwner); }},
                    {i18nc("@action:button", "Delete..."), QStringLiteral("edit-delete"),
                     QStringLiteral("deleteOrphanedSavedSearch"), [this, owner, id]() { msgListWidget->deleteSavedSearch(owner, id); }},
                });
            return;
        }
        // Choosing the shown account in the Unified Inbox goes back to its folder
        if (*account != m_accounts->activeAccount() || m_unifiedInbox)
            switchAccount(*account);
        if (*account != m_accounts->activeAccount() || m_unifiedInbox)
            return;
    }
    if (edit)
        msgListWidget->editSavedSearch(owner, id);
    else
        msgListWidget->runSavedSearch(owner, id);
}

void MainWindow::changeSavedSearchOwner(const QString &owner, const QString &id, const QString &newOwner)
{
    KConfigGroup root = MessageListWidget::savedSearchesRoot();
    if (!root.group(owner).hasGroup(id)) {
        reloadSavedSearchFolders();
        return;
    }
    const QString name = root.group(owner).group(id).readEntry("Name", QString());
    if (UiUtils::SavedSearches::isNameTaken(root, newOwner, name, id)) {
        Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Move Saved Search"),
                                     i18nc("@info %1 is the name of a saved search", "The account already has a saved "
                                           "search called \"%1\". Rename one of them first.", name));
        return;
    }
    if (!UiUtils::SavedSearches::changeOwner(root, owner, id, newOwner)) {
        reloadSavedSearchFolders();
        return;
    }
    if (!root.sync())
        Gui::Util::messageBoxWarning(this, i18nc("@title:window", "Move Saved Search"),
                                     i18nc("@info", "The search could not be moved."));
    // The counts were found in the folders of the old account
    KConfigGroup counts(KSharedConfig::openStateConfig(), QStringLiteral("SavedSearchCounts"));
    counts.deleteEntry(UiUtils::SavedSearches::countsKey(owner, id));
    counts.sync();
    if (msgListWidget->editedSavedSearchOwner() == owner && msgListWidget->editedSavedSearchId() == id)
        msgListWidget->endSavedSearchEdit();
    if (msgListWidget->activeSavedSearchOwner() == owner && msgListWidget->activeSavedSearchId() == id)
        msgListWidget->forgetActiveSavedSearch();
    reloadSavedSearchFolders();
}

void MainWindow::addSearchFolderAccounts(SearchFoldersDialog *dialog)
{
    for (const QString &accountId : m_accounts->accountIds()) {
        Imap::ImapAccess *access = m_accountAccess.value(accountId);
        QAbstractItemModel *mailboxes = nullptr;
        if (access == m_imapAccess)
            mailboxes = prettyMboxModel;
        else if (access && access->mailboxModel())
            mailboxes = new Imap::Mailbox::PrettyMailboxModel(dialog, access->mailboxModel());
        dialog->addAccount(accountId, m_accounts->displayName(accountId), mailboxes);
    }
}

void MainWindow::chooseSavedSearchFolders(const QString &owner, const QString &id)
{
    const KConfigGroup group = MessageListWidget::savedSearchesRoot().group(owner);
    const UiUtils::SavedSearch search = group.hasGroup(id) ? UiUtils::SavedSearches::read(group.group(id), owner)
                                                           : UiUtils::SavedSearch();
    if (!search.isValid()) {
        reloadSavedSearchFolders();
        return;
    }
    auto *dialog = new SearchFoldersDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setObjectName(QStringLiteral("savedSearchFoldersDialog"));
    dialog->setWindowTitle(i18nc("@title:window %1 is the name of a saved search", "Folders of \"%1\"", search.name));
    addSearchFolderAccounts(dialog);
    dialog->setSelectedFolders(search.folders);
    connect(dialog, &QDialog::accepted, this, [this, dialog, owner, id]() {
        KConfigGroup account = MessageListWidget::savedSearchesRoot().group(owner);
        if (!account.hasGroup(id)) {
            reloadSavedSearchFolders();
            return;
        }
        KConfigGroup group = account.group(id);
        UiUtils::SavedSearch search = UiUtils::SavedSearches::read(group, owner);
        search.folders = dialog->selectedFolders();
        UiUtils::SavedSearches::write(group, search);
        group.sync();
        reloadSavedSearchFolders();
        // The virtual folder shows what the changed search finds
        if (msgListWidget->activeSavedSearchOwner() == owner && msgListWidget->activeSavedSearchId() == id)
            activateSavedSearch(owner, id);
    });
    dialog->open();
}

void MainWindow::removeMissingSavedSearchFolders(const QString &owner, const QString &id)
{
    KConfigGroup account = MessageListWidget::savedSearchesRoot().group(owner);
    if (!account.hasGroup(id) || !m_savedSearchFolders) {
        reloadSavedSearchFolders();
        return;
    }
    KConfigGroup group = account.group(id);
    UiUtils::SavedSearch search = UiUtils::SavedSearches::read(group, owner);
    const QStringList missing = m_savedSearchFolders->problems(owner, id).missingFolders;
    QStringList kept;
    for (const QString &folder : std::as_const(search.folders)) {
        if (!missing.contains(folder))
            kept << folder;
    }
    // Leaving out every folder would leave nothing to search
    if (kept.isEmpty() || kept.size() == search.folders.size())
        return;
    search.folders = kept;
    UiUtils::SavedSearches::write(group, search);
    group.sync();
    reloadSavedSearchFolders();
    if (msgListWidget->activeSavedSearchOwner() == owner && msgListWidget->activeSavedSearchId() == id)
        activateSavedSearch(owner, id);
}

void MainWindow::storeSavedSearchOrder(const QList<UiUtils::SavedSearch> &searches)
{
    KConfigGroup root = MessageListWidget::savedSearchesRoot();
    UiUtils::SavedSearches::writeOrder(root, searches);
    root.sync();
}

void MainWindow::scheduleSavedSearchCount()
{
    if (m_savedSearchCountTimer && msgListWidget && !msgListWidget->activeSavedSearchOwner().isEmpty())
        m_savedSearchCountTimer->start();
}

void MainWindow::countActiveSavedSearch()
{
    using namespace Imap::Mailbox;
    const QString owner = msgListWidget->activeSavedSearchOwner();
    const QString id = msgListWidget->activeSavedSearchId();
    if (owner.isEmpty() || !m_savedSearchFolders)
        return;
    UiUtils::SavedSearches::Counts counts;
    bool done = true;
    if (searchShownInList()) {
        if (!m_unifiedModel)
            return;
        counts = {m_unifiedModel->messageCount(), m_unifiedModel->unreadCount()};
        // The folders searched so far; the count is kept once every folder was searched
        done = !isSearchAcrossRunning() && !m_searchStopped;
    } else {
        if (showsUnifiedList())
            return;
        auto *threading = qobject_cast<ThreadingMsgListModel *>(m_imapAccess->threadingMsgListModel());
        const auto *msgList = qobject_cast<MsgListModel *>(m_imapAccess->msgListModel());
        if (!threading || !msgList || !msgList->currentMailbox().isValid() || threading->isUpdatePending()
            || !threading->searchFailure().isEmpty())
            return;
        counts = {0, 0};
        // Every message found, whichever filters of the list hide some of them
        std::function<void(const QModelIndex &)> countRows = [&](const QModelIndex &parent) {
            for (int row = 0, rows = threading->rowCount(parent); row < rows; ++row) {
                const QModelIndex index = threading->index(row, 0, parent);
                // A thread may hold a placeholder for a message which is not in the folder
                if (threading->mapToSource(index).isValid()) {
                    ++counts.total;
                    const QVariant read = index.data(RoleMessageIsMarkedRead);
                    if (read.isValid() && !read.toBool())
                        ++counts.unread;
                }
                countRows(index);
            }
        };
        countRows(QModelIndex());
    }
    if (counts != m_savedSearchFolders->counts(owner, id))
        m_savedSearchFolders->setCounts(owner, id, counts);
    KConfigGroup state(KSharedConfig::openStateConfig(), QStringLiteral("SavedSearchCounts"));
    if (done && counts != UiUtils::SavedSearches::readCounts(state, owner, id)) {
        UiUtils::SavedSearches::writeCounts(state, owner, id, counts);
        state.sync();
    }
}

void MainWindow::refreshAccountSwitcher()
{
    const QSignalBlocker blocker(m_accountSwitcher);
    m_accountSwitcher->clear();
    for (const QString &accountId : m_accounts->accountIds()) {
        const Common::AccountStatus status = m_accountStatus->status(accountId);
        const int row = m_accountSwitcher->count();
        QString text = m_accounts->displayName(accountId);
        const bool attention = Common::AccountStatusTracker::needsAttention(status.kind);
        if (attention)
            text += QStringLiteral(" (%1)").arg(Common::AccountStatusTracker::label(status.kind));
        m_accountSwitcher->addItem(text, accountId);
        if (attention) {
            m_accountSwitcher->setItemIcon(row, UiUtils::loadIcon(QStringLiteral("dialog-warning")));
            QString tip = Common::AccountStatusTracker::label(status.kind);
            if (!status.detail.isEmpty())
                tip += QLatin1String(": ") + status.detail;
            m_accountSwitcher->setItemData(row, tip, Qt::ToolTipRole);
        }
    }
    m_accountSwitcher->setCurrentIndex(m_accountSwitcher->findData(m_accounts->activeAccount()));

    if (!m_capabilityGate) {
        // The first refresh comes after the actions are created
        m_capabilityGate = new CapabilityActionGate(this);
        for (QAction *action : {reloadMboxList, createChildMailbox, createTopMailbox, deleteCurrentMailbox,
                                m_actionSubscribeMailbox, m_actionShowOnlySubscribed})
            m_capabilityGate->addAction(action, Imap::AccountOperation::ServerFolders);
        for (QAction *action : {markAsJunk, markAsNotJunk, tag1, tag2, tag3, tag4, tag5, tag6, tag7, tag8, tag9})
            m_capabilityGate->addAction(action, Imap::AccountOperation::ServerSideFlags);
        m_capabilityGate->addAction(moveToArchive, Imap::AccountOperation::ServerSideMove);
        for (QAction *action : {m_renameMailbox, m_emptyTrash, m_emptyJunk, m_goToFolder, m_deleteMovesToTrash,
                                m_copyToFolder, m_copyToMenu->menuAction()})
            m_capabilityGate->addAction(action, Imap::AccountOperation::ServerFolders);
        for (auto it = m_goToSpecialFolderActions.cbegin(); it != m_goToSpecialFolderActions.cend(); ++it) {
            if (it.key() != Imap::Mailbox::SpecialUse::Inbox)
                m_capabilityGate->addAction(it.value(), Imap::AccountOperation::ServerFolders);
        }
        for (QAction *action : {m_moveToFolder, m_moveToMenu->menuAction()})
            m_capabilityGate->addAction(action, Imap::AccountOperation::ServerSideMove);
    }
    m_capabilityGate->apply(Imap::AccountCapabilities::forAccount(*accountSettings()));
    m_accountSidebar->refresh(*m_accounts, *m_accountStatus);
    refreshAccountUnread();
    m_shellHeader->setAccount(*m_accounts, *m_accountStatus);
    updateBackgroundAlerts();
    if (m_oauthSignIn) {
        const Common::AccountStatusKind kind = m_accountStatus->status(m_accounts->activeAccount()).kind;
        m_oauthSignIn->setVisible(usesOAuth(*accountSettings())
                                  && (kind == Common::AccountStatusKind::AuthFailed
                                      || kind == Common::AccountStatusKind::AuthRequired));
    }
}

void MainWindow::updateBackgroundAlerts()
{
    if (!m_backgroundAlerts)
        return;
    QStringList flagged;
    // In account order, so that the button switches to the first flagged account in the list
    for (const QString &accountId : m_accounts->accountIds()) {
        if (accountId != m_accounts->activeAccount()
            && Common::AccountStatusTracker::needsAttention(m_accountStatus->status(accountId).kind))
            flagged << accountId;
    }
    m_backgroundAlerts->setVisible(!flagged.isEmpty());
    m_backgroundAlerts->setProperty("accountId", flagged.value(0));
    if (flagged.isEmpty())
        return;
    QStringList lines;
    for (const QString &accountId : std::as_const(flagged)) {
        const Common::AccountStatus status = m_accountStatus->status(accountId);
        QString line = i18nc("@info:tooltip account name and its status", "%1: %2", m_accounts->displayName(accountId),
                             Common::AccountStatusTracker::label(status.kind));
        if (!status.detail.isEmpty())
            line += QLatin1String(": ") + status.detail;
        lines << line;
    }
    const QString text = flagged.size() == 1
        ? i18nc("@info:status account name and its status", "%1: %2", m_accounts->displayName(flagged.first()),
                Common::AccountStatusTracker::label(m_accountStatus->status(flagged.first()).kind))
        : i18ncp("@info:status", "%1 account needs attention", "%1 accounts need attention", flagged.size());
    m_backgroundAlerts->setText(text);
    m_backgroundAlerts->setToolTip(lines.join(QLatin1Char('\n')) + QLatin1Char('\n')
                                   + i18nc("@info:tooltip", "Click to switch to %1.",
                                           m_accounts->displayName(flagged.first())));
    m_backgroundAlerts->setAccessibleDescription(lines.join(QLatin1Char('\n')));
}

void MainWindow::applySystemSleep(Imap::ImapAccess *access)
{
    if (auto *watcher = qobject_cast<Imap::Mailbox::NetworkWatcher *>(access->networkWatcher()))
        watcher->setSystemSleeping(m_sleepMonitor && m_sleepMonitor->isSleeping());
}

void MainWindow::notifyBackgroundAccount(const QString &accountId)
{
    const Common::AccountStatus status = m_accountStatus->status(accountId);
    const QString title = i18nc("@title notification, %1 is the account name", "%1 needs attention",
                                m_accounts->displayName(accountId));
    QString text = Common::AccountStatusTracker::label(status.kind);
    if (!status.detail.isEmpty())
        text += QLatin1String(": ") + status.detail;
#ifdef IMPRESARIO_HAVE_KNOTIFICATIONS
    auto *notification = new KNotification(QStringLiteral("accountNeedsAttention"), KNotification::CloseOnTimeout);
    notification->setComponentName(QStringLiteral("plasma-impresario"));
    notification->setTitle(title);
    notification->setText(text.toHtmlEscaped());
    notification->setWindow(windowHandle());
    KNotificationAction *open = notification->addDefaultAction(i18nc("@action:button", "Open Account"));
    connect(open, &KNotificationAction::activated, this, [this, accountId, notification]() {
        if (!m_accounts->accountIds().contains(accountId))
            return;
        setActivationToken(notification->xdgActivationToken());
        switchAccount(accountId);
        showMainWindow();
    });
    if (status.kind == Common::AccountStatusKind::AuthFailed && usesOAuth(*m_accounts->settings(accountId))) {
        KNotificationAction *signIn = notification->addAction(i18nc("@action:button", "Sign In"));
        connect(signIn, &KNotificationAction::activated, this, [this, accountId]() { signInAccount(accountId); });
    }
    m_accountNotices->show(accountId, notification);
    notification->sendEvent();
#else
    if (m_trayIcon && m_trayIcon->isVisible())
        m_trayIcon->showMessage(title, text, QSystemTrayIcon::Warning, 3333);
#endif
}

void MainWindow::detachAccountModels()
{
    auto *imap = qobject_cast<Imap::Mailbox::Model *>(m_imapAccess->imapModel());
    const QList<QObject *> models = {imap, m_imapAccess->msgListModel(), m_imapAccess->threadingMsgListModel(),
                                     m_imapAccess->networkWatcher(), m_imapAccess->mailboxModel()};
    for (QObject *model : models) {
        if (!model)
            continue;
        model->disconnect(this);
        model->disconnect(msgListWidget);
        model->disconnect(protocolLogger);
        mboxTree->disconnect(model);
        for (QAction *action : {reloadAllMailboxes, netOffline, netExpensive, netOnline})
            action->disconnect(model);
    }
    if (imap)
        imap->taskModel()->disconnect(taskTree);
}

void MainWindow::monitorAccount(const QString &accountId)
{
    Imap::ImapAccess *access = accountAccess(accountId);
    auto *model = qobject_cast<Imap::Mailbox::Model *>(access->imapModel());
    Q_ASSERT(model);
    registerOutboxAccount(accountId);
    using Common::AccountStatusKind;
    if (access->protocol() != Imap::AccountProtocol::Imap) {
        // The folders are on this computer and always open; the status is that of the last visit to the server
        Pop3Fetcher *fetcher = nullptr;
        JmapSyncer *syncer = nullptr;
        if (access->protocol() == Imap::AccountProtocol::Pop3) {
            fetcher = m_pop3Fetchers.value(accountId);
            if (!fetcher) {
                fetcher = new Pop3Fetcher(accountId, access, m_accounts->settings(accountId), m_settings,
                                          pluginManager(), m_accountStatus, this);
                m_pop3Fetchers.insert(accountId, fetcher);
            }
        } else {
            syncer = m_jmapSyncers.value(accountId);
            if (!syncer) {
                syncer = new JmapSyncer(accountId, access, m_accounts->settings(accountId), m_settings,
                                        pluginManager(), m_accountStatus, this);
                m_jmapSyncers.insert(accountId, syncer);
                // Folders made, renamed or removed on the server appear without asking the folder list again
                connect(syncer, &JmapSyncer::foldersChanged, model, &Imap::Mailbox::Model::reloadMailboxList);
            }
        }
        for (auto signal : {&Imap::Mailbox::Model::networkPolicyOnline, &Imap::Mailbox::Model::networkPolicyExpensive})
            connect(model, signal, model, &Imap::Mailbox::Model::reloadMailboxList, Qt::SingleShotConnection);
        connect(model, &Imap::Mailbox::Model::messageCountPossiblyChanged, access, [this]() { scheduleAccountUnreadRefresh(); });
        connect(model, &QAbstractItemModel::rowsInserted, access, [this](const QModelIndex &parent) {
            if (!parent.isValid())
                scheduleAccountUnreadRefresh();
        });
        scheduleAccountUnreadRefresh();
        QPointer<Pop3Fetcher> guard = fetcher;
        QPointer<JmapSyncer> syncerGuard = syncer;
        QMetaObject::invokeMethod(this, [this, guard, syncerGuard, accountId]() {
            if (guard)
                guard->check(accountId == m_accounts->activeAccount());
            if (syncerGuard)
                syncerGuard->sync(accountId == m_accounts->activeAccount());
        }, Qt::QueuedConnection);
        return;
    }
    // A new model numbers its connections afresh
    m_connectionStates.remove(accountId);
    connect(model, &Imap::Mailbox::Model::connectionStateChanged, access,
        [this, accountId](const uint connection, const Imap::ConnectionState state) {
            using Summary = Imap::Mailbox::ConnectionStates::Summary;
            Imap::Mailbox::ConnectionStates &states = m_connectionStates[accountId];
            states.update(connection, state);
            const AccountStatusKind current = m_accountStatus->status(accountId).kind;
            switch (states.summary()) {
            case Summary::Disconnected:
                // Going offline and losing the connection say so themselves, and keep saying it
                if (current != AccountStatusKind::Offline && !Common::AccountStatusTracker::needsAttention(current))
                    m_accountStatus->setStatus(accountId, AccountStatusKind::Idle);
                break;
            case Summary::Connecting:
                m_accountStatus->setStatus(accountId, AccountStatusKind::Connecting);
                break;
            case Summary::Connected:
                m_accountStatus->setStatus(accountId, AccountStatusKind::Connected);
                break;
            }
        });
    connect(model, &Imap::Mailbox::Model::networkPolicyOffline, access, [this, accountId]() {
        if (!Common::AccountStatusTracker::needsAttention(m_accountStatus->status(accountId).kind))
            m_accountStatus->setStatus(accountId, AccountStatusKind::Offline);
    });
    // The activity log says when an account which lost its connection tries again
    if (auto *watcher = qobject_cast<Imap::Mailbox::NetworkWatcher *>(access->networkWatcher())) {
        connect(watcher, &Imap::Mailbox::NetworkWatcher::reconnectAttemptScheduled, access,
            [this, accountId](const int timeout) {
                m_reconnectAt.insert(accountId, QDateTime::currentDateTime().addMSecs(timeout));
                if (m_accountStatus->status(accountId).kind == AccountStatusKind::NetworkError)
                    recordSyncActivity(accountId);
            });
        connect(watcher, &Imap::Mailbox::NetworkWatcher::resetReconnectState, access,
            [this, accountId]() { m_reconnectAt.remove(accountId); });
    }
    // A folder which could not be synchronized stays in the activity log until it is
    connect(model, &Imap::Mailbox::Model::mailboxSyncFailed, access,
        [this, accountId](
            const QString &mailbox, const QString &message) { recordFolderSyncFailed(accountId, mailbox, message); });
    connect(model, &Imap::Mailbox::Model::mailboxSyncingProgress, access,
        [this, accountId](const QModelIndex &mailbox, const Imap::Mailbox::MailboxSyncingProgress state) {
            if (state != Imap::Mailbox::STATE_DONE || !m_mailAutomation)
                return;
            using UiUtils::ActivityLog;
            ActivityLog *log = m_mailAutomation->activity();
            const QString key = QLatin1String("folder-sync:") + accountId + QLatin1Char(':')
                + mailbox.data(Imap::Mailbox::RoleMailboxName).toString();
            const int row = log->find(key);
            if (row < 0 || log->entry(row).state == ActivityLog::State::Done)
                return;
            ActivityLog::Entry entry = log->entry(row);
            entry.state = ActivityLog::State::Done;
            entry.detail = i18nc("@info", "Synchronized");
            entry.error.clear();
            entry.action = ActivityLog::Action::None;
            entry.time = QDateTime();
            log->record(entry);
        });
    // The model only connects once something reads it; without a view, list the folders so the account logs in.
    for (auto signal : {&Imap::Mailbox::Model::networkPolicyOnline, &Imap::Mailbox::Model::networkPolicyExpensive})
        connect(model, signal, model, &Imap::Mailbox::Model::reloadMailboxList, Qt::SingleShotConnection);
    connect(model, &Imap::Mailbox::Model::networkError, access, [this, accountId](const QString &message) {
        m_accountStatus->setStatus(accountId, AccountStatusKind::NetworkError, message);
    });
    connect(model, &Imap::Mailbox::Model::authAttemptFailed, access, [this, accountId](const QString &message) {
        m_accountStatus->setStatus(accountId, AccountStatusKind::AuthFailed, message);
    });
    connect(model, &Imap::Mailbox::Model::authRequested, access, [this, accountId]() {
        if (accountId == m_accounts->activeAccount())
            return;
        const Common::AccountStatus status = m_accountStatus->status(accountId);
        if (status.kind == AccountStatusKind::AuthFailed)
            m_accountStatus->setStatus(accountId, AccountStatusKind::AuthRequired, status.detail);
        else
            requestStoredPassword(accountId);
    }, Qt::QueuedConnection);
    // The sidebar shows the unread INBOX count, which is known once the folders are listed and the count arrives
    connect(model, &Imap::Mailbox::Model::messageCountPossiblyChanged, access, [this]() { scheduleAccountUnreadRefresh(); });
    connect(model, &QAbstractItemModel::rowsInserted, access, [this](const QModelIndex &parent) {
        if (!parent.isValid())
            scheduleAccountUnreadRefresh();
    });
    scheduleAccountUnreadRefresh();
}

void MainWindow::scheduleAccountUnreadRefresh()
{
    if (!m_accountUnreadTimer) {
        m_accountUnreadTimer = new QTimer(this);
        m_accountUnreadTimer->setInterval(100);
        m_accountUnreadTimer->setSingleShot(true);
        connect(m_accountUnreadTimer, &QTimer::timeout, this, &MainWindow::refreshAccountUnread);
    }
    m_accountUnreadTimer->start();
}

void MainWindow::refreshAccountUnread()
{
    QList<int> counts;
    for (const QString &accountId : m_accounts->accountIds()) {
        Imap::ImapAccess *access = m_accountAccess.value(accountId);
        auto *model = access ? qobject_cast<Imap::Mailbox::Model *>(access->imapModel()) : nullptr;
        int count = -1;
        if (model) {
            // Reading the count asks the server for it when it is not known yet
            const QVariant unread = model->mailboxIndexByName(QStringLiteral("INBOX")).data(Imap::Mailbox::RoleUnreadMessageCount);
            if (unread.isValid())
                count = unread.toInt();
        }
        if (m_accountSidebar)
            m_accountSidebar->setUnreadCount(accountId, count);
        counts.append(count);
        m_inboxUnread.insert(accountId, count);
        // The widgets' unread messages come from the index, which catches up after the counts
        if (Imap::Mailbox::SearchIndexer *indexer = access ? access->searchIndexer() : nullptr)
            connect(indexer, &Imap::Mailbox::SearchIndexer::finished, this, &MainWindow::mailStatusChanged,
                    Qt::UniqueConnection);
    }
    publishUnreadCount(UiUtils::DesktopFeedback::totalUnread(counts));
    emit mailStatusChanged();
}

void MainWindow::publishUnreadCount(int unread)
{
    namespace DesktopFeedback = UiUtils::DesktopFeedback;
    // Turning the setting off hides the count instead of leaving the last one on the task manager entry
    if (!DesktopFeedback::showsUnreadCount(*KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"))))
        unread = 0;
    if (!m_unreadBadge.update(unread))
        return;
    const QString desktopFileName = QGuiApplication::desktopFileName();
    if (!desktopFileName.isEmpty())
        QDBusConnection::sessionBus().send(DesktopFeedback::launcherEntryUpdate(desktopFileName, unread));
}

int MainWindow::unreadCountOnTaskManager() const
{
    return m_unreadBadge.shown();
}

void MainWindow::requestStoredPassword(const QString &accountId)
{
    Imap::ImapAccess *access = accountAccess(accountId);
    auto *model = qobject_cast<Imap::Mailbox::Model *>(access->imapModel());
    Plugins::PasswordPlugin *password = pluginManager()->password();
    Plugins::PasswordJob *job = password ? password->requestPassword(access->credentialAccountId(), QStringLiteral("imap")) : nullptr;
    if (!job) {
        m_accountStatus->setStatus(accountId, Common::AccountStatusKind::AuthRequired);
        return;
    }
    connect(job, &Plugins::PasswordJob::passwordAvailable, model, &Imap::Mailbox::Model::setImapPassword);
    connect(job, &Plugins::PasswordJob::error, this, [this, accountId](Plugins::PasswordJob::Error, const QString &) {
        m_accountStatus->setStatus(accountId, Common::AccountStatusKind::AuthRequired);
    });
    job->setAutoDelete(true);
    job->start();
}

void MainWindow::connectBackgroundAccounts()
{
    for (const QString &accountId : m_accounts->accountIds()) {
        if (accountId == m_accounts->activeAccount())
            continue;
        Imap::ImapAccess *access = accountAccess(accountId);
        if (!access->isConfigured() || access->imapModel())
            continue;
        access->reloadConfiguration();
        access->doConnect();
        monitorAccount(accountId);
    }
}

void MainWindow::switchAccount(const QString &accountId)
{
    // The results of the shown account's search would stay without it
    if (searchShownInList() && accountId != m_accounts->activeAccount()) {
        leaveSearchAcross();
        msgListWidget->clearSearch();
    }
    // Choosing the shown account goes back to its folder
    if (m_unifiedInbox && accountId == m_accounts->activeAccount()) {
        setUnifiedInbox(false);
        return;
    }
    if (accountId == m_accounts->activeAccount() || !m_accounts->accountIds().contains(accountId))
        return;
    setUnifiedInbox(false, false);
    commitFolderPosition();
    detachAccountModels();
    nukeModels();
    m_accounts->setActiveAccount(accountId);
    m_mailAutomation->setActiveAccount(accountId);
    // The shown account reports its problems in the window
    m_accountNotices->close(accountId);
    m_imapAccess = accountAccess(accountId);
    mboxTree->setDesiredExpansion(expandedMailboxes());
    msgListWidget->setAccountId(m_imapAccess->credentialAccountId());
    setupModels(m_imapAccess->imapModel() == nullptr);
    connectModelActions();
    removeSysTray();
    slotToggleSysTray();
    m_ignoreStoredPassword = false;
    m_connectionError = false;
    updateConnectionStatus(i18nc("@info:status", "Account: %1", m_accounts->displayName(accountId)));
    refreshAccountSwitcher();
    slotUpdateWindowTitle();
    const Common::AccountStatus status = m_accountStatus->status(accountId);
    if (Pop3Fetcher *fetcher = m_pop3Fetchers.value(accountId)) {
        // Shown now, so a check may ask for the password or the certificate it could not ask for before
        if (Common::AccountStatusTracker::needsAttention(status.kind))
            fetcher->check(true);
    } else if (JmapSyncer *syncer = m_jmapSyncers.value(accountId)) {
        if (Common::AccountStatusTracker::needsAttention(status.kind))
            syncer->sync(true);
    } else if (status.kind == Common::AccountStatusKind::AuthRequired) {
        m_ignoreStoredPassword = !status.detail.isEmpty();
        authenticationRequested();
    }
    const QString startFolder = UiUtils::SettingsStore::value(*KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")),
                                                              m_settings, QStringLiteral("Shell/StartFolder"), accountId)
                                    .toString();
    if (!startFolder.isEmpty())
        goToMailbox(startFolder);
    applyAppearance();
}

void MainWindow::slotAddAccount()
{
    AddAccountDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted || dialog.accountName().isEmpty())
        return;
    const QString accountId = m_accounts->addAccount(dialog.accountName());
    QSettings *settings = m_accounts->settings(accountId);
    Composer::AccountIdentities::initializeNewAccount(*settings);
    if (dialog.protocol() == Imap::AccountProtocol::Pop3) {
        Imap::AccountCapabilities::setProtocol(*settings, Imap::AccountProtocol::Pop3);
        // Most providers sign in with the address, and sending cannot borrow an IMAP sign-in
        if (UiUtils::ServerDiscovery::parseAddress(dialog.emailAddress()).isValid()) {
            settings->setValue(Common::SettingsNames::pop3UserKey, dialog.emailAddress());
            settings->setValue(Common::SettingsNames::smtpUserKey, dialog.emailAddress());
        }
    } else if (dialog.protocol() == Imap::AccountProtocol::Jmap) {
        Imap::AccountCapabilities::setProtocol(*settings, Imap::AccountProtocol::Jmap);
        if (UiUtils::ServerDiscovery::parseAddress(dialog.emailAddress()).isValid()) {
            settings->setValue(Common::SettingsNames::jmapUserKey, dialog.emailAddress());
            settings->setValue(Common::SettingsNames::smtpUserKey, dialog.emailAddress());
        }
    }
    // The account is switched to before it has servers, so that nothing connects until its settings are saved
    switchAccount(accountId);
    const UiUtils::ServerDiscovery::Result found = dialog.result();
    if (found.isValid())
        UiUtils::ServerDiscovery::applyTo(*settings, found, dialog.useOAuth());
    if (UiUtils::ServerDiscovery::parseAddress(dialog.emailAddress()).isValid()) {
        Composer::SenderIdentitiesModel identities;
        identities.loadFromSettings(*settings);
        if (identities.rowCount() == 0) {
            identities.appendIdentity(Composer::ItemSenderIdentity(dialog.realName(), dialog.emailAddress(), QString(), QString()));
            identities.saveToSettings(*settings);
        }
        if (Composer::SenderIdentitiesModel *loaded = m_accountIdentities.value(accountId))
            loaded->loadFromSettings(*settings);
    }
    settings->sync();
    m_imapAccess->reloadConfiguration();
    emit accountsChanged();
    slotShowAccountSettings();
}

void MainWindow::cycleAccount(int step)
{
    const QStringList ids = m_accounts->accountIds();
    const int count = ids.size();
    const int current = ids.indexOf(m_accounts->activeAccount());
    switchAccount(ids.at((current + step % count + count) % count));
}

void MainWindow::slotNextAccount()
{
    cycleAccount(1);
}

void MainWindow::slotPreviousAccount()
{
    cycleAccount(-1);
}

void MainWindow::slotFocusMessageView()
{
#ifdef IMPRESARIO_HAVE_WEBENGINE
    m_messageWidget->messageView->setFocus();
#else
    m_messageWidget->setFocus();
#endif
}

void MainWindow::slotReturnToMessageList()
{
    if (!msgListWidget->tree->isVisible() && m_oneAtTimeGoBack->isEnabled())
        m_oneAtTimeGoBack->trigger();
    if (msgListWidget->tree->isVisible())
        msgListWidget->tree->setFocus(Qt::ShortcutFocusReason);
}

void MainWindow::slotFocusNextRegion()
{
    cycleFocusRegion(1);
}

void MainWindow::slotFocusPreviousRegion()
{
    cycleFocusRegion(-1);
}

void MainWindow::slotFind()
{
#ifdef IMPRESARIO_HAVE_WEBENGINE
    QWidget *focus = focusWidget();
    const bool inReader = focus
        && (m_messageWidget->isAncestorOf(focus) || (m_readerActions && m_readerActions->isAncestorOf(focus)));
    if (inReader && m_messageWidget->messageView->currentMessage().isValid()) {
        m_messageWidget->messageView->findAction()->trigger();
        return;
    }
#endif
    // A layout which shows one pane at a time hides the list, and with it the search, while a message is open
    if (!msgListWidget->searchField()->isVisible() && m_layoutMode == LAYOUT_ONE_AT_TIME && m_oneAtTimeGoBack
        && m_oneAtTimeGoBack->isEnabled())
        m_oneAtTimeGoBack->trigger();
    msgListWidget->focusSearch();
    // Typing replaces the previous search
    msgListWidget->searchField()->selectAll();
}

void MainWindow::cycleFocusRegion(int step)
{
    // Each region is entered through its target; focus anywhere inside the container counts as being in it.
    struct Region {
        QWidget *container;
        QWidget *target;
    };
    QList<Region> regions;
    regions.append(Region{m_shellHeader, m_shellHeader->focusTarget()});
    if (m_accountSwitcher)
        regions.append(Region{m_accountSwitcher, m_accountSwitcher});
    regions.append(Region{m_accountSidebar, mboxTree});
    regions.append(Region{msgListWidget, msgListWidget->tree});
#ifdef IMPRESARIO_HAVE_WEBENGINE
    regions.append(Region{m_messageWidget, m_messageWidget->messageView});
#else
    regions.append(Region{m_messageWidget, m_messageWidget});
#endif
    for (EmbeddedPanel *panel : std::as_const(m_embeddedPanels))
        regions.append(Region{panel, panel->focusTarget()});

    // Visit the panes in screen order, whatever the layout and placement: the header, then the columns from the
    // leading side, each from top to bottom, then the footer
    QWidget *footer = m_shellFooter.widget;
    const bool rightToLeft = layoutDirection() == Qt::RightToLeft;
    const auto band = [this, footer](const Region &region) {
        if (region.container == m_shellHeader || region.container == m_accountSwitcher)
            return 0;
        return footer && footer->isAncestorOf(region.container) ? 2 : 1;
    };
    const auto position = [rightToLeft](const Region &region) {
        const QRect rect(region.container->mapToGlobal(QPoint(0, 0)), region.container->size());
        return std::make_pair(rightToLeft ? -rect.right() : rect.left(), rect.top());
    };
    std::stable_sort(regions.begin(), regions.end(), [&](const Region &a, const Region &b) {
        const int bandA = band(a);
        const int bandB = band(b);
        if (bandA != bandB)
            return bandA < bandB;
        return bandA != 0 && position(a) < position(b);
    });

    // The quick search is a stop of its own, just before the list of messages or results that it fills
    QLineEdit *search = msgListWidget->searchField();
    for (int i = 0; i < regions.size(); ++i) {
        if (regions.at(i).container == msgListWidget) {
            regions.insert(i, Region{search, search});
            break;
        }
    }

    const int count = regions.size();
    QWidget *focus = focusWidget();
    int current = -1;
    for (int i = 0; i < count && focus; ++i) {
        // Focus in the search field is also inside the message list, so the innermost region wins
        if ((regions.at(i).container == focus || regions.at(i).container->isAncestorOf(focus))
            && (current < 0 || regions.at(current).container->isAncestorOf(regions.at(i).container)))
            current = i;
    }
    if (current < 0)
        current = step > 0 ? -1 : count;
    for (int moved = 1; moved <= count; ++moved) {
        const int index = ((current + step * moved) % count + count) % count;
        QWidget *target = regions.at(index).target;
        if (target->isVisible() && target->isEnabled()) {
            target->setFocus(step > 0 ? Qt::TabFocusReason : Qt::BacktabFocusReason);
            return;
        }
    }
}

void MainWindow::slotRenameAccount()
{
    renameAccount(m_accounts->activeAccount());
}

void MainWindow::renameAccount(const QString &accountId)
{
    bool accepted = false;
    const QString name = QInputDialog::getText(this, i18n("Rename Account"), i18n("Account name:"), QLineEdit::Normal,
                                               m_accounts->displayName(accountId), &accepted).trimmed();
    if (!accepted || name.isEmpty())
        return;
    m_accounts->setDisplayName(accountId, name);
    if (m_unifiedInbox)
        syncUnifiedSources();
    refreshAccountSwitcher();
    updateMessageListScope();
    slotUpdateWindowTitle();
    emit accountsChanged();
}

void MainWindow::slotRemoveAccount()
{
    confirmRemoveAccount(m_accounts->activeAccount());
}

void MainWindow::confirmRemoveAccount(const QString &accountId)
{
    if (m_accounts->accountIds().size() < 2) {
        Gui::Util::messageBoxWarning(this, i18n("Cannot Remove Account"), i18n("At least one account is required."));
        return;
    }
    QString question = i18n("Remove account \"%1\" and its settings? The mail and the search index kept on this "
                            "computer for it are deleted too. Messages on the server are not affected.",
                            m_accounts->displayName(accountId));
    const QSettings &settings = *m_accounts->settings(accountId);
    if (Imap::AccountCapabilities::forAccount(settings).protocol() == Imap::AccountProtocol::Pop3)
        question += QLatin1String("\n\n")
            + i18n("This is a POP3 account: mail already removed from the server exists only on this computer and "
                   "cannot be recovered after the account is removed.");
    else if (Imap::AccountCapabilities::forAccount(settings).protocol() == Imap::AccountProtocol::Jmap)
        question += QLatin1String("\n\n")
            + i18n("This is a JMAP account: folders that exist only on this computer, and the messages in them, "
                   "cannot be recovered after the account is removed.");
    if (usesOAuth(settings)) {
        const OAuth::Provider provider = OAuth::AccountConfig::fromSettings(settings, accountId).provider;
        if (provider.isValid() && !provider.revocationEndpoint.isValid())
            question += QLatin1String("\n\n")
                + i18n("%1 cannot revoke the sign-in from Plasma Impresario. Afterwards, remove its access at %2.",
                       provider.displayName, provider.accessManagementPage.toString());
    }
    if (QMessageBox::question(this, i18n("Remove Account"), question) != QMessageBox::Yes)
        return;
    removeAccountNow(accountId);
}

void MainWindow::showAccountMenu(const QString &accountId, const QPoint &globalPos)
{
    if (!m_accounts->accountIds().contains(accountId))
        return;
    auto *menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("accountMenu"));
    menu->setAttribute(Qt::WA_DeleteOnClose);
    // The settings dialog edits the account shown in the window, so it switches to the account first.
    QAction *settings = menu->addAction(m_actionAccountSettings->icon(), m_actionAccountSettings->text());
    settings->setObjectName(QStringLiteral("action_account_menu_settings"));
    connect(settings, &QAction::triggered, this, [this, accountId]() {
        switchAccount(accountId);
        if (m_accounts->activeAccount() == accountId)
            slotShowAccountSettings();
    }, Qt::QueuedConnection);
    QAction *rename = menu->addAction(m_actionRenameAccount->text());
    rename->setObjectName(QStringLiteral("action_account_menu_rename"));
    connect(rename, &QAction::triggered, this, [this, accountId]() { renameAccount(accountId); }, Qt::QueuedConnection);
    if (usesOAuth(*m_accounts->settings(accountId))) {
        QAction *signIn = menu->addAction(QIcon::fromTheme(QStringLiteral("dialog-password")),
                                          i18nc("@action:inmenu", "Sign In…"));
        signIn->setObjectName(QStringLiteral("action_account_menu_sign_in"));
        connect(signIn, &QAction::triggered, this, [this, accountId]() { signInAccount(accountId); },
                Qt::QueuedConnection);
    }
    menu->addSeparator();
    QAction *remove = menu->addAction(m_actionRemoveAccount->icon(), m_actionRemoveAccount->text());
    remove->setObjectName(QStringLiteral("action_account_menu_remove"));
    remove->setEnabled(m_accounts->accountIds().size() > 1);
    connect(remove, &QAction::triggered, this, [this, accountId]() { confirmRemoveAccount(accountId); },
            Qt::QueuedConnection);
    menu->popup(globalPos);
}

void MainWindow::removeAccountNow(const QString &accountId)
{
    QStringList remaining = m_accounts->accountIds();
    if (!remaining.removeOne(accountId) || remaining.isEmpty())
        return;
    if (accountId == m_accounts->activeAccount())
        switchAccount(remaining.first());
    if (m_outbox) {
        // What the account still had to send goes back to the drafts instead of waiting for a server which is gone
        m_outbox->removeTransport(accountId);
        const QList<Composer::Outbox::Entry> waiting = m_outbox->entries();
        for (const Composer::Outbox::Entry &message : waiting) {
            if (message.accountId != accountId)
                continue;
            if (message.state == Composer::Outbox::State::Submitted)
                m_outbox->forget(message.id);
            else
                m_outbox->moveToDrafts(message.id);
        }
    }
    Imap::ImapAccess *access = accountAccess(accountId);
    const QSettings &removed = *m_accounts->settings(accountId);
    const bool revokeOAuth = usesOAuth(removed) && !sharesOAuthGrant(accountId);
    const OAuth::AccountConfig oauthConfig = OAuth::AccountConfig::fromSettings(removed, access->credentialAccountId());
    if (Plugins::PasswordPlugin *password = pluginManager()->password()) {
        QStringList types{QStringLiteral("imap"), QStringLiteral("smtp"), Pop3Fetcher::passwordType(),
                          JmapSyncer::passwordType()};
        // Revoking deletes the stored grant once the provider has answered
        if (!revokeOAuth)
            types << OAuth::credentialType();
        for (const QString &type : std::as_const(types)) {
            if (Plugins::PasswordJob *job = password->deletePassword(access->credentialAccountId(), type)) {
                job->setAutoDelete(true);
                job->start();
            }
        }
    }
    if (revokeOAuth)
        revokeDetached(oauthConfig, pluginManager());
    const QString cacheDirectory = access->cacheDirectory();
    const QString localStoreFolder = access->protocol() == Imap::AccountProtocol::Imap
        ? QString()
        : Imap::ImapAccess::localStoreFolder(access->credentialAccountId());
    m_pop3Fetchers.remove(accountId);
    m_jmapSyncers.remove(accountId);
    if (m_unifiedModel)
        m_unifiedModel->removeAccount(accountId);
    if (m_searchResultsPanel)
        m_searchResultsPanel->results()->removeAccount(accountId);
    if (m_agendaSearch && m_agendaSearchItem.accountId == accountId)
        cancelAgendaSearch();
    m_mailAutomation->forgetAccount(accountId);
    // Deleting the account's models also closes its cache database, so the files can be removed.
    delete m_accountAccess.take(accountId);
    m_connectionStates.remove(accountId);
    scheduleAgendaMessages();
    if (!Imap::removeAccountCache(cacheDirectory, Common::writablePath(Common::LOCATION_CACHE))) {
        qWarning() << "Could not delete the offline cache of the removed account in" << cacheDirectory;
        Gui::Util::messageBoxWarning(this, i18n("Offline Mail Not Deleted"),
                                     i18n("The mail and the search index kept on this computer for the removed account "
                                          "could not be deleted. Remove the folder %1 to delete them.",
                                          cacheDirectory));
    }
    // The access held the last reference to the local store, so its database is closed by now
    if (!localStoreFolder.isEmpty() && QFileInfo::exists(localStoreFolder)
        && !Imap::removeAccountCache(localStoreFolder, QFileInfo(localStoreFolder).path())) {
        qWarning() << "Could not delete the mail of the removed account in" << localStoreFolder;
        Gui::Util::messageBoxWarning(this, i18n("Mail Not Deleted"),
                                     i18n("The mail kept on this computer for the removed account could not be "
                                          "deleted. Remove the folder %1 to delete it.",
                                          localStoreFolder));
    }
    // Open composers may still list this account's identities, so the model lives until the window closes.
    m_accountIdentities.remove(accountId);
    m_accountNotices->close(accountId);
    m_accountStatus->forget(accountId);
    m_accounts->removeAccount(accountId);
    {
        // Its overrides and its place in the workspaces go with it
        const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
        UiUtils::SettingsStore::removeAccount(*config, accountId);
        UiUtils::SettingsStore::setWorkspaces(
            *config, UiUtils::Workspaces::withoutAccount(UiUtils::SettingsStore::workspaces(*config), accountId));
        config->sync();
        // Its tabs close; the saved session loses them even while tabs are off
        auto state = KSharedConfig::openStateConfig();
        UiUtils::SettingsStore::setTabs(*state, UiUtils::MailTabs::withoutAccount(UiUtils::SettingsStore::tabs(*state), accountId));
        if (m_tabsEnabled) {
            const bool currentClosed = !m_tabSession.tabs.isEmpty()
                && m_tabSession.tabs.at(m_tabSession.current).accountId == accountId;
            m_tabSession = UiUtils::MailTabs::withoutAccount(m_tabSession, accountId);
            if (m_tabSession.tabs.isEmpty()) {
                m_tabSession.tabs.append(currentLocationTab());
                m_tabSession.current = 0;
            }
            if (currentClosed)
                activateTab(m_tabSession.current);
            else
                refreshTabs();
        }
    }
    if (m_unifiedInbox)
        syncUnifiedSources();
    refreshAccountSwitcher();
    slotUpdateWindowTitle();
    emit accountsChanged();
}

bool MainWindow::sharesOAuthGrant(const QString &accountId) const
{
    // Google revokes every token of a user's grant to the client, so another account would be signed out too
    const QSettings &settings = *m_accounts->settings(accountId);
    const OAuth::AccountConfig config = OAuth::AccountConfig::fromSettings(settings, accountId);
    const QString user = settings.value(Common::SettingsNames::imapUserKey).toString();
    for (const QString &other : m_accounts->accountIds()) {
        if (other == accountId)
            continue;
        const QSettings &otherSettings = *m_accounts->settings(other);
        if (!usesOAuth(otherSettings))
            continue;
        const OAuth::AccountConfig otherConfig = OAuth::AccountConfig::fromSettings(otherSettings, other);
        if (otherConfig.provider.id == config.provider.id && otherConfig.clientId == config.clientId
            && otherSettings.value(Common::SettingsNames::imapUserKey).toString().compare(user, Qt::CaseInsensitive)
                == 0)
            return true;
    }
    return false;
}

void MainWindow::signInAccount(const QString &accountId)
{
    if (!m_accounts->accountIds().contains(accountId) || !usesOAuth(*m_accounts->settings(accountId)))
        return;
    QPointer<Imap::ImapAccess> access = accountAccess(accountId);
    auto *session = access->imapModel() ? qobject_cast<OAuth::Session *>(OAuth::TokenSource::find(access->imapModel()))
                                        : nullptr;
    if (!session) {
        // Only SMTP signs in with OAuth, or the account is not connected yet
        session = new OAuth::Session(
            OAuth::AccountConfig::fromSettings(*m_accounts->settings(accountId), access->credentialAccountId()),
            passwordGetter(pluginManager()), this);
        connect(session, &OAuth::Session::authorized, session, &QObject::deleteLater);
        connect(session, &OAuth::Session::authorizationFailed, session, &QObject::deleteLater);
    }
    if (session->isAuthorizing())
        return;
    // Answers only this attempt: whichever outcome arrives first disconnects both
    auto *attempt = new QObject(this);
    connect(session, &QObject::destroyed, attempt, &QObject::deleteLater);
    connect(session, &OAuth::Session::authorized, attempt, [this, accountId, access, attempt]() {
        attempt->deleteLater();
        if (!access || !m_accounts->accountIds().contains(accountId))
            return;
        m_accountStatus->setStatus(accountId, Common::AccountStatusKind::Idle);
        if (accountId == m_accounts->activeAccount()) {
            m_connectionError = false;
            updateConnectionStatus(i18nc("@info:status", "Signed in"));
        }
        // A failed sign-in left the account offline; connect it with the new grant
        if (auto *watcher = qobject_cast<Imap::Mailbox::NetworkWatcher *>(access->networkWatcher()))
            watcher->setNetworkOnline();
        if (auto *model = qobject_cast<Imap::Mailbox::Model *>(access->imapModel());
            model && accountId != m_accounts->activeAccount())
            model->reloadMailboxList();
    });
    connect(session, &OAuth::Session::authorizationFailed, attempt, [this, accountId, attempt](const QString &message) {
        attempt->deleteLater();
        if (m_accounts->accountIds().contains(accountId))
            m_accountStatus->setStatus(accountId, Common::AccountStatusKind::AuthFailed, message);
    });
    if (accountId == m_accounts->activeAccount())
        updateConnectionStatus(i18nc("@info:status", "Waiting for you to finish signing in in your web browser…"));
    session->authorize();
}

void MainWindow::authenticationRequested()
{
    Plugins::PasswordPlugin *password = pluginManager()->password();
    if (password) {
        Plugins::PasswordJob *job = password->requestPassword(m_imapAccess->credentialAccountId(), QStringLiteral("imap"));
        if (job) {
            connect(job, &Plugins::PasswordJob::passwordAvailable, this, [this](const QString &password) {
                authenticationContinue(password);
            });
            connect(job, &Plugins::PasswordJob::error, this, [this](const Plugins::PasswordJob::Error error, const QString &message) {
                if (error == Plugins::PasswordJob::Error::NoSuchPassword) {
                    authenticationContinue(QString());
                } else {
                    authenticationContinue(QString(), tr("Failed to retrieve password from the store: %1").arg(message));
                }
            });
            job->setAutoDelete(true);
            job->start();
            return;
        }
    }

    authenticationContinue(QString());

}

void MainWindow::authenticationContinue(const QString &password, const QString &errorMessage)
{
    const QString &user = accountSettings()->value(Common::SettingsNames::imapUserKey).toString();
    QString pass = password;
    if (m_ignoreStoredPassword || pass.isEmpty()) {
        auto dialog = PasswordDialog::getPassword(this, tr("Authentication Required"),
                                                  tr("<p>Please provide IMAP password for user <b>%1</b> on <b>%2</b>:</p>").arg(
                                                      user.toHtmlEscaped(),
                                                      accountSettings()->value(Common::SettingsNames::imapHostKey).toString().toHtmlEscaped()
                                                      ),
                                                  errorMessage + (errorMessage.isEmpty() ? QString() : QStringLiteral("\n\n"))
                                                  + imapModel()->imapAuthError());
        connect(dialog, &PasswordDialog::gotPassword, imapModel(), &Imap::Mailbox::Model::setImapPassword);
        connect(dialog, &PasswordDialog::rejected, imapModel(), &Imap::Mailbox::Model::unsetImapPassword);
    } else {
        imapModel()->setImapPassword(pass);
    }
}

void MainWindow::checkSslPolicy(const QString &accountId)
{
    // Background accounts ask too; the account may be removed while the prompt is open.
    QPointer<Imap::ImapAccess> access = m_accountAccess.value(accountId);
    if (!access)
        return;
    QString title = access->sslInfoTitle();
    if (m_accounts->accountIds().size() > 1)
        title = i18nc("@title:window %1 is an account name, %2 the certificate prompt title", "%1: %2",
                      m_accounts->displayName(accountId), title);
    const bool accepted = QMessageBox(static_cast<QMessageBox::Icon>(access->sslInfoIcon()), title, access->sslInfoMessage(),
                                      QMessageBox::Yes | QMessageBox::No, this).exec() == QMessageBox::Yes;
    if (access)
        access->setSslPolicy(accepted);
}

void MainWindow::nukeModels()
{
#ifdef IMPRESARIO_HAVE_WEBENGINE
    m_messageWidget->messageView->setEmpty();
#endif
    mboxTree->setModel(nullptr);
    msgListWidget->tree->setModel(nullptr);
    allTree->setModel(nullptr);
    taskTree->setModel(nullptr);
    delete m_groupingMsgListModel;
    m_groupingMsgListModel = nullptr;
    delete prettyMsgListModel;
    prettyMsgListModel = nullptr;
    delete prettyMboxModel;
    prettyMboxModel = nullptr;
}

void MainWindow::recoverDrafts()
{
    QDir draftPath(Common::writablePath(Common::LOCATION_CACHE) + QLatin1String("Drafts/"));
    QStringList drafts(draftPath.entryList(QStringList() << QStringLiteral("*.draft")));
    Q_FOREACH(const QString &draft, drafts) {
        ComposeWidget *w = ComposeWidget::warnIfMsaNotConfigured(ComposeWidget::createDraft(this, draftPath.filePath(draft)), this);
        // No need to further try creating widgets for drafts if a nullptr is being returned by ComposeWidget::warnIfMsaNotConfigured
        if (!w)
            break;
    }
}

void MainWindow::slotComposeMail()
{
    ComposeWidget::warnIfMsaNotConfigured(ComposeWidget::createBlank(this), this);
}

void MainWindow::slotEditDraft()
{
    QString path(Common::writablePath(Common::LOCATION_DATA) + tr("Drafts"));
    QDir().mkpath(path);
    FileChooser::Request request;
    request.title = tr("Edit draft");
    request.folder = QUrl::fromLocalFile(path);
    request.filters = {{tr("Drafts"), {QStringLiteral("*.draft")}}};
    path = FileChooser::chooseLocalPath(this, request);
    if (!path.isNull()) {
        ComposeWidget::warnIfMsaNotConfigured(ComposeWidget::createDraft(this, path), this);
    }
}

QModelIndexList MainWindow::translatedSelection() const
{
    // The messages of the account which an action works on right now
    if (m_acting)
        return m_actingSelection;
    QModelIndexList translatedIndexes;
    Q_FOREACH(const QModelIndex &index, msgListWidget->tree->selectedTree()) {
        // The headers of groups have no message behind them
        const QModelIndex translated = Imap::deproxifiedIndex(index);
        if (translated.isValid())
            translatedIndexes << translated;
    }
    return translatedIndexes;
}

void MainWindow::handleMarkAsRead(bool value)
{
    if (routeToMessageAccounts([this, value]() { handleMarkAsRead(value); }))
        return;
#ifdef IMPRESARIO_HAVE_WEBENGINE
    const QModelIndexList translatedIndexes = translatedSelection();
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleMarkAsRead: no valid messages";
    } else {
        const int count = translatedIndexes.size();
        // The change of a single message is visible in the list
        const QString done = count < 2 ? QString()
            : value ? i18ncp("@info", "Marked %1 message as read", "Marked %1 messages as read", count)
                    : i18ncp("@info", "Marked %1 message as unread", "Marked %1 messages as unread", count);
        trackFlagChange(
            translatedIndexes,
            [=](Imap::Mailbox::Model *model, const QModelIndexList &messages) {
                return model->markMessagesRead(messages, value ? Imap::Mailbox::FLAG_ADD : Imap::Mailbox::FLAG_REMOVE);
            },
            value ? i18ncp("@info", "Marking %1 message as read…", "Marking %1 messages as read…", count)
                  : i18ncp("@info", "Marking %1 message as unread…", "Marking %1 messages as unread…", count),
            done,
            [value, count](const QString &message) {
                return value ? i18ncp("@info", "Could not mark %1 message as read: %2",
                                   "Could not mark %1 messages as read: %2", count, message)
                             : i18ncp("@info", "Could not mark %1 message as unread: %2",
                                   "Could not mark %1 messages as unread: %2", count, message);
            });
        if (translatedIndexes.contains(m_messageWidget->messageView->currentMessage())) {
            m_messageWidget->messageView->stopAutoMarkAsRead();
        }
    }
#endif
}

void MainWindow::handleMarkAsUnread()
{
    handleMarkAsRead(false);
}

void MainWindow::slotNextUnread()
{
#ifdef IMPRESARIO_HAVE_WEBENGINE
    QModelIndex current = msgListWidget->tree->currentIndex();

    UiUtils::gotoNext(msgListWidget->tree->model(), current,
    [](const QModelIndex &idx) {
        return !idx.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool() && !idx.data(Imap::Mailbox::RoleIsGroupHeader).toBool();
    },
    [this](const QModelIndex &idx) {
        Q_ASSERT(!idx.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool());
        m_messageWidget->messageView->setMessage(idx);
        msgListWidget->tree->setCurrentIndex(idx);
    },
    []() {
        // nothing to do
    });
#endif
}

void MainWindow::slotMarkReadAndNextUnread()
{
#ifdef IMPRESARIO_HAVE_WEBENGINE
    const QModelIndexList marked = translatedSelection();
    // Found before marking, because hiding read messages may remove the current one from the list
    QPersistentModelIndex next;
    UiUtils::gotoNext(msgListWidget->tree->model(), msgListWidget->tree->currentIndex(),
    [&marked](const QModelIndex &idx) {
        return !idx.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool() && !idx.data(Imap::Mailbox::RoleIsGroupHeader).toBool()
            && !marked.contains(Imap::deproxifiedIndex(idx));
    },
    [&next](const QModelIndex &idx) { next = idx; },
    []() {
        // stay on the last message
    });
    if (!marked.isEmpty())
        handleMarkAsRead(true);
    if (next.isValid()) {
        m_messageWidget->messageView->setMessage(next);
        msgListWidget->tree->setCurrentIndex(next);
    }
#endif
}

void MainWindow::slotNextMessage()
{
    goToAdjacentMessage(true);
}

void MainWindow::slotPreviousMessage()
{
    goToAdjacentMessage(false);
}

void MainWindow::goToAdjacentMessage(bool forward)
{
    QTreeView *tree = msgListWidget->tree;
    const QModelIndex target = UiUtils::adjacentMessage(tree->model(), tree->currentIndex(), forward,
                                                        [tree](const QModelIndex &idx) { return tree->isExpanded(idx); });
    if (!target.isValid())
        return;
#ifdef IMPRESARIO_HAVE_WEBENGINE
    m_messageWidget->messageView->setMessage(target);
#endif
    tree->setCurrentIndex(target);
}

void MainWindow::goToMessageInConversation(bool forward)
{
    QTreeView *tree = msgListWidget->tree;
    const QModelIndex current = tree->currentIndex();
    if (!current.isValid())
        return;
    // The Unified Inbox shows messages of several folders, whose conversations are not known
    if (showsUnifiedList())
        return;
    // A thread is the conversation. A flat or grouped list follows the subject and Message-ID links of the loaded rows,
    // also to messages which the quick filters hide; those are shown while they are current.
    QModelIndex target;
    if (showsThreads()) {
        target = UiUtils::adjacentInThread(tree->model(), current, forward);
    } else {
        const QModelIndex message =
            UiUtils::adjacentByDate(current, forward, UiUtils::collectConversation(m_imapAccess->msgListModel(), current));
        target = revealMessage(message);
    }
    if (!target.isValid())
        return;
    // Expands a collapsed thread or group on the way
    tree->scrollTo(target);
#ifdef IMPRESARIO_HAVE_WEBENGINE
    m_messageWidget->messageView->setMessage(target);
#endif
    tree->setCurrentIndex(target);
}

QModelIndex MainWindow::revealMessage(const QModelIndex &message)
{
    auto *msgList = qobject_cast<QAbstractProxyModel *>(m_imapAccess->msgListModel());
    auto *threading = qobject_cast<QAbstractProxyModel *>(m_imapAccess->threadingMsgListModel());
    if (!message.isValid() || !msgList || !threading || !prettyMsgListModel)
        return QModelIndex();
    const QModelIndex source = threading->mapFromSource(msgList->mapFromSource(message));
    if (!source.isValid())
        return QModelIndex();
    QModelIndex pretty = prettyMsgListModel->mapFromSource(source);
    if (!pretty.isValid()) {
        prettyMsgListModel->setKeptVisibleIndex(source);
        pretty = prettyMsgListModel->mapFromSource(source);
    }
    return fromPrettyIndex(pretty);
}

void MainWindow::slotPreviousUnread()
{
#ifdef IMPRESARIO_HAVE_WEBENGINE
    QModelIndex current = msgListWidget->tree->currentIndex();

    UiUtils::gotoPrevious(msgListWidget->tree->model(), current,
    [](const QModelIndex &idx) {
        return !idx.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool() && !idx.data(Imap::Mailbox::RoleIsGroupHeader).toBool();
    },
    [this](const QModelIndex &idx) {
        Q_ASSERT(!idx.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool());
        m_messageWidget->messageView->setMessage(idx);
        msgListWidget->tree->setCurrentIndex(idx);
    },
    []() {
        // nothing to do
    });
#endif
}

void MainWindow::handleTag(const bool checked, const int index)
{
    if (routeToMessageAccounts([this, checked, index]() { handleTag(checked, index); }))
        return;
    const QModelIndexList &translatedIndexes = translatedSelection();
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleTag: no valid messages";
    } else {
        const QString tagName = m_favoriteTags->tagNameByIndex(index);
        if (tagName.isEmpty())
            return;
        const int count = translatedIndexes.size();
        const QString done = count < 2 ? QString()
            : checked ? i18ncp("@info", "Added the tag %2 to %1 message", "Added the tag %2 to %1 messages", count, tagName)
                      : i18ncp("@info", "Removed the tag %2 from %1 message", "Removed the tag %2 from %1 messages", count, tagName);
        trackFlagChange(
            translatedIndexes,
            [=](Imap::Mailbox::Model *model, const QModelIndexList &messages) {
                return model->setMessageFlags(
                    messages, tagName, checked ? Imap::Mailbox::FLAG_ADD : Imap::Mailbox::FLAG_REMOVE);
            },
            checked ? i18ncp("@info", "Adding the tag %2 to %1 message…", "Adding the tag %2 to %1 messages…", count,
                          tagName)
                    : i18ncp("@info", "Removing the tag %2 from %1 message…", "Removing the tag %2 from %1 messages…",
                          count, tagName),
            done,
            [checked, count, tagName](const QString &message) {
                return checked ? i18ncp("@info", "Could not add the tag %2 to %1 message: %3",
                                     "Could not add the tag %2 to %1 messages: %3", count, tagName, message)
                               : i18ncp("@info", "Could not remove the tag %2 from %1 message: %3",
                                     "Could not remove the tag %2 from %1 messages: %3", count, tagName, message);
            });
    }
}

void MainWindow::handleMarkAsDeleted(bool value)
{
    if (routeToMessageAccounts([this, value]() { handleMarkAsDeleted(value); }))
        return;
    const QModelIndexList translatedIndexes = translatedSelection();
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleMarkAsDeleted: no valid messages";
    } else if (const QString trash = deleteTrashTarget(); !trash.isEmpty()) {
        trackMove(translatedIndexes, sourceMailboxName(translatedIndexes), trash, MoveKind::Trash);
    } else {
        // A shortcut can fire while the action still says "Move to Trash", which is not a toggle
        if (!markAsDeleted->isCheckable())
            value = true;
        const int count = translatedIndexes.size();
        const QString done = count < 2 ? QString()
            : value ? i18ncp("@info", "Marked %1 message as deleted", "Marked %1 messages as deleted", count)
                    : i18ncp("@info", "Removed the deleted mark from %1 message", "Removed the deleted mark from %1 messages", count);
        trackFlagChange(
            translatedIndexes,
            [=](Imap::Mailbox::Model *model, const QModelIndexList &messages) {
                return model->markMessagesDeleted(
                    messages, value ? Imap::Mailbox::FLAG_ADD : Imap::Mailbox::FLAG_REMOVE);
            },
            value ? i18ncp("@info", "Marking %1 message as deleted…", "Marking %1 messages as deleted…", count)
                  : i18ncp("@info", "Removing the deleted mark from %1 message…",
                        "Removing the deleted mark from %1 messages…", count),
            done,
            [value, count](const QString &message) {
                return value ? i18ncp("@info", "Could not mark %1 message as deleted: %2",
                                   "Could not mark %1 messages as deleted: %2", count, message)
                             : i18ncp("@info", "Could not remove the deleted mark from %1 message: %2",
                                   "Could not remove the deleted mark from %1 messages: %2", count, message);
            });
    }
}

void MainWindow::handleMarkAsFlagged(const bool value)
{
    if (routeToMessageAccounts([this, value]() { handleMarkAsFlagged(value); }))
        return;
    const QModelIndexList translatedIndexes = translatedSelection();
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleMarkAsFlagged: no valid messages";
    } else {
        const int count = translatedIndexes.size();
        const QString done = count < 2 ? QString()
            : value ? i18ncp("@info", "Flagged %1 message", "Flagged %1 messages", count)
                    : i18ncp("@info", "Removed the flag from %1 message", "Removed the flag from %1 messages", count);
        trackFlagChange(
            translatedIndexes,
            [=](Imap::Mailbox::Model *model, const QModelIndexList &messages) {
                return model->setMessageFlags(messages, Imap::Mailbox::FlagNames::flagged,
                    value ? Imap::Mailbox::FLAG_ADD : Imap::Mailbox::FLAG_REMOVE);
            },
            value ? i18ncp("@info", "Flagging %1 message…", "Flagging %1 messages…", count)
                  : i18ncp("@info", "Removing the flag from %1 message…", "Removing the flag from %1 messages…", count),
            done,
            [value, count](const QString &message) {
                return value
                    ? i18ncp("@info", "Could not flag %1 message: %2", "Could not flag %1 messages: %2", count, message)
                    : i18ncp("@info", "Could not remove the flag from %1 message: %2",
                          "Could not remove the flag from %1 messages: %2", count, message);
            });
    }
}

void MainWindow::handleMarkAsJunk(const bool value)
{
    if (routeToMessageAccounts([this, value]() { handleMarkAsJunk(value); }))
        return;
    const QModelIndexList translatedIndexes = translatedSelection();
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleMarkAsJunk: no valid messages";
    } else {
        const int count = translatedIndexes.size();
        auto failed = [value, count](const QString &message) {
            return value ? i18ncp("@info", "Could not mark %1 message as junk: %2", "Could not mark %1 messages as junk: %2", count,
                                  message)
                         : i18ncp("@info", "Could not remove the junk mark from %1 message: %2",
                                  "Could not remove the junk mark from %1 messages: %2", count, message);
        };
        if (value) {
            trackFlagChange(
                translatedIndexes,
                [=](Imap::Mailbox::Model *model, const QModelIndexList &messages) {
                    return model->setMessageFlags(
                        messages, Imap::Mailbox::FlagNames::notjunk, Imap::Mailbox::FLAG_REMOVE);
                },
                QString(), QString(), failed);
        }
        const QString done = count < 2 ? QString()
            : value ? i18ncp("@info", "Marked %1 message as junk", "Marked %1 messages as junk", count)
                    : i18ncp("@info", "Removed the junk mark from %1 message", "Removed the junk mark from %1 messages", count);
        trackFlagChange(
            translatedIndexes,
            [=](Imap::Mailbox::Model *model, const QModelIndexList &messages) {
                return model->setMessageFlags(messages, Imap::Mailbox::FlagNames::junk,
                    value ? Imap::Mailbox::FLAG_ADD : Imap::Mailbox::FLAG_REMOVE);
            },
            value ? i18ncp("@info", "Marking %1 message as junk…", "Marking %1 messages as junk…", count)
                  : i18ncp("@info", "Removing the junk mark from %1 message…",
                        "Removing the junk mark from %1 messages…", count),
            done, failed);
        if (value)
            moveAfterJunkMark(translatedIndexes, true);
    }
}

void MainWindow::handleMarkAsNotJunk(const bool value)
{
    if (routeToMessageAccounts([this, value]() { handleMarkAsNotJunk(value); }))
        return;
    const QModelIndexList translatedIndexes = translatedSelection();
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleMarkAsNotJunk: no valid messages";
    } else {
        const int count = translatedIndexes.size();
        auto failed = [value, count](const QString &message) {
            return value ? i18ncp("@info", "Could not mark %1 message as not junk: %2", "Could not mark %1 messages as not junk: %2",
                                  count, message)
                         : i18ncp("@info", "Could not remove the not-junk mark from %1 message: %2",
                                  "Could not remove the not-junk mark from %1 messages: %2", count, message);
        };
        if (value) {
            trackFlagChange(
                translatedIndexes,
                [=](Imap::Mailbox::Model *model, const QModelIndexList &messages) {
                    return model->setMessageFlags(messages, Imap::Mailbox::FlagNames::junk, Imap::Mailbox::FLAG_REMOVE);
                },
                QString(), QString(), failed);
        }
        const QString done = count < 2 ? QString()
            : value ? i18ncp("@info", "Marked %1 message as not junk", "Marked %1 messages as not junk", count)
                    : i18ncp("@info", "Removed the not-junk mark from %1 message", "Removed the not-junk mark from %1 messages",
                             count);
        trackFlagChange(
            translatedIndexes,
            [=](Imap::Mailbox::Model *model, const QModelIndexList &messages) {
                return model->setMessageFlags(messages, Imap::Mailbox::FlagNames::notjunk,
                    value ? Imap::Mailbox::FLAG_ADD : Imap::Mailbox::FLAG_REMOVE);
            },
            value ? i18ncp("@info", "Marking %1 message as not junk…", "Marking %1 messages as not junk…", count)
                  : i18ncp("@info", "Removing the not-junk mark from %1 message…",
                        "Removing the not-junk mark from %1 messages…", count),
            done, failed);
        if (value)
            moveAfterJunkMark(translatedIndexes, false);
    }
}

void MainWindow::moveAfterJunkMark(const QModelIndexList &messages, const bool junk)
{
    Imap::Mailbox::Model *model = imapModel();
    if (!model || messages.isEmpty() || !model->isNetworkAvailable()
        || !UiUtils::MailSafetySettings::movesJunk(*KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"))))
        return;
    const QString junkFolder = model->specialFolderMailbox(Imap::Mailbox::SpecialUse::Junk);
    const QString source = messages.constFirst().data(Imap::Mailbox::RoleMailboxName).toString();
    if (junkFolder.isEmpty() || source.isEmpty()
        || !std::all_of(messages.cbegin(), messages.cend(), [&source](const QModelIndex &message) {
               return message.data(Imap::Mailbox::RoleMailboxName).toString() == source;
           }))
        return;
    // Junk leaves any other folder for the Junk folder; not junk leaves only the Junk folder, for the inbox
    if ((source == junkFolder) == junk)
        return;
    const QString target = junk ? junkFolder : QStringLiteral("INBOX");
    trackMove(messages, source, target, MoveKind::Folder);
}

void MainWindow::slotMoveToArchiveFailed(const QString &error, const int count)
{
    if (count > 0)
        m_notificationBar->showError(i18ncp("@info", "Could not archive %1 message: %2", "Could not archive %1 messages: %2", count, error));
    else
        m_notificationBar->showError(i18nc("@info", "Failed to archive: %1", error));
}

void MainWindow::handleMoveToArchive()
{
    if (routeToMessageAccounts([this]() { handleMoveToArchive(); }))
        return;
    const QModelIndexList translatedIndexes = translatedSelection();
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleMoveToArchive: no valid messages";
    } else {
        const QString archiveFolderName = archiveTarget();
        trackMove(translatedIndexes, sourceMailboxName(translatedIndexes), archiveFolderName, MoveKind::Archive);
    }
}

void MainWindow::handleDeletePermanently()
{
    using UiUtils::DestructiveActions::Confirmation;
    const QModelIndexList translatedIndexes = translatedSelection();
    // Messages of other accounts than the shown one are deleted by their accounts, after one question for all
    const bool routing = showsUnifiedList() && !m_acting &&
                         std::any_of(translatedIndexes.cbegin(), translatedIndexes.cend(), [this](const QModelIndex &message) {
        return message.model() != m_imapAccess->imapModel();
    });
    if (translatedIndexes.isEmpty())
        return;
    if (!routing && !imapModel()->isNetworkAvailable()) {
        if (m_acting)
            m_notificationBar->showError(forAccountOf(imapModel(), i18nc("@info", "Could not delete the messages because the account is offline.")));
        return;
    }

    // The model can change while the question is shown
    const QList<QPersistentModelIndex> selected(translatedIndexes.cbegin(), translatedIndexes.cend());
    const int count = selected.size();
    if (!m_acting && !confirmPermanentAction(Confirmation::DeletePermanently, i18nc("@title:window", "Delete Permanently"),
                                i18ncp("@info", "Permanently delete %1 message? It cannot be recovered.",
                                       "Permanently delete %1 messages? They cannot be recovered.", count),
                                i18nc("@action:button", "&Delete Permanently"))) {
        return;
    }

    QModelIndexList messages;
    for (const QPersistentModelIndex &message : selected) {
        if (message.isValid())
            messages << message;
    }
    if (messages.isEmpty())
        return;
    if (routing) {
        routeToMessageAccounts([this]() { handleDeletePermanently(); }, false, messages);
        return;
    }

    bool expungeScheduled = false;
    Imap::Mailbox::ImapTask *task = imapModel()->deleteMessagesPermanently(messages, &expungeScheduled);
    if (!task) {
        m_notificationBar->showError(i18nc("@info", "Could not delete the messages because the account is offline."));
        return;
    }
    QPointer<Imap::Mailbox::Model> model = imapModel();
    const int deleted = messages.size();
    connect(task, &Imap::Mailbox::ImapTask::completed, this, [this, model, deleted, expungeScheduled]() {
        if (!showsModel(model))
            return;
        if (expungeScheduled) {
            m_notificationBar->showInfo(forAccountOf(model, i18ncp("@info", "Permanently deleted %1 message",
                                                                   "Permanently deleted %1 messages", deleted)));
            return;
        }
        // Without UIDPLUS, only an EXPUNGE of the whole folder can remove them
        InlineNotificationBar::Action expungeNow{i18nc("@action:button", "Expunge"), QStringLiteral("trash-empty"),
                                                 QStringLiteral("expungeMailbox"), [this, model]() {
            if (showsModel(model))
                actOnModel(model, [this]() { slotExpunge(); });
        }};
        m_notificationBar->showInfo(forAccountOf(model, i18ncp("@info",
                                           "Marked %1 message as deleted. The server cannot remove single messages; "
                                           "Expunge removes all messages marked as deleted in this folder.",
                                           "Marked %1 messages as deleted. The server cannot remove single messages; "
                                           "Expunge removes all messages marked as deleted in this folder.", deleted)),
                                    {expungeNow});
    });
    connect(task, &Imap::Mailbox::ImapTask::failed, this, [this, model, deleted](const QString &message) {
        if (showsModel(model))
            m_notificationBar->showError(forAccountOf(model, i18ncp("@info", "Could not delete %1 message permanently: %2",
                                                                    "Could not delete %1 messages permanently: %2", deleted, message)));
    });
    showBusyWhileRunning(task, i18ncp("@info", "Deleting %1 message permanently…", "Deleting %1 messages permanently…", deleted));
}

void MainWindow::setConfirmationPrompt(const ConfirmationPrompt &prompt)
{
    m_confirmationPrompt = prompt;
}

void MainWindow::setFileManagerLauncher(const FileManagerLauncher &launcher)
{
    m_fileManagerLauncher = launcher;
}

void MainWindow::slotFileSaved(const QString &fileName)
{
    if (fileName.isEmpty())
        return;
    // Only reports where the file went; saving says nothing about whether its content is safe to open
    m_notificationBar->showInfo(
        i18nc("@info %1 is a file name", "Saved \"%1\".", QFileInfo(fileName).fileName()),
        {{i18nc("@action:button", "Show in Folder"), QStringLiteral("document-open-folder"),
          QStringLiteral("showSavedFileInFolder"), [this, fileName]() { showSavedFileInFolder(fileName); }}});
}

void MainWindow::showSavedFileInFolder(const QString &fileName)
{
    const QFileInfo file(fileName);
    if (!file.exists()) {
        m_notificationBar->showError(i18nc("@info %1 is a file name, %2 a folder",
                                           "\"%1\" is no longer in %2; it was moved or deleted.", file.fileName(),
                                           QDir::toNativeSeparators(file.absolutePath())));
        return;
    }
    const QPointer<MainWindow> self(this);
    const auto failed = [self, fileName](const QString &error) {
        if (self)
            self->m_notificationBar->showError(i18nc("@info %1 is a file name, %2 an error message",
                                                     "No file manager could show \"%1\": %2",
                                                     QFileInfo(fileName).fileName(), error));
    };
    const QUrl url = QUrl::fromLocalFile(file.absoluteFilePath());
    if (m_fileManagerLauncher) {
        m_fileManagerLauncher(url, failed);
        return;
    }
    // Asks the desktop's file manager through org.freedesktop.FileManager1, so that whichever one is registered
    // selects the file; without that service KIO opens the folder in the default application for folders
    auto *job = new KIO::OpenFileManagerWindowJob(this);
    job->setHighlightUrls({url});
    connect(job, &KJob::result, this, [failed](KJob *finished) {
        if (finished->error())
            failed(finished->errorString());
    });
    job->start();
}

bool MainWindow::confirmPermanentAction(const UiUtils::DestructiveActions::Confirmation kind, const QString &title,
                                        const QString &text, const QString &acceptText)
{
    using namespace UiUtils::DestructiveActions;
    KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")), configGroupName());
    return confirm(group, kind, [this, kind, &title, &text, &acceptText]() -> ConfirmationAnswer {
        if (m_confirmationPrompt)
            return m_confirmationPrompt(kind, title, text);
        QMessageBox box(QMessageBox::Warning, title, text, QMessageBox::NoButton, this);
        box.setObjectName(QStringLiteral("confirmPermanentAction"));
        QPushButton *accept = box.addButton(acceptText, QMessageBox::DestructiveRole);
        QPushButton *cancel = box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(cancel);
        box.setEscapeButton(cancel);
        auto *dontAsk = new QCheckBox(i18nc("@option:check", "Do not ask again"), &box);
        dontAsk->setObjectName(QStringLiteral("dontAskAgain"));
        box.setCheckBox(dontAsk);
        box.exec();
        ConfirmationAnswer answer;
        answer.accepted = box.clickedButton() == accept;
        answer.dontAskAgain = dontAsk->isChecked();
        return answer;
    });
}

QString MainWindow::currentMailboxName() const
{
    auto *model = qobject_cast<Imap::Mailbox::MsgListModel *>(currentAccess()->msgListModel());
    return model ? model->currentMailbox().data(Imap::Mailbox::RoleMailboxName).toString() : QString();
}

QString MainWindow::sourceMailboxName(const QModelIndexList &messages) const
{
    const QString first = messages.isEmpty() ? QString() : messages.constFirst().data(Imap::Mailbox::RoleMailboxName).toString();
    const bool shared = !first.isEmpty() && std::all_of(messages.cbegin(), messages.cend(), [&first](const QModelIndex &message) {
        return message.data(Imap::Mailbox::RoleMailboxName).toString() == first;
    });
    return shared ? first : currentMailboxName();
}

QString MainWindow::archiveTarget() const
{
    // A folder chosen in the settings wins, even before the folder above it has been listed
    const QString configured = accountSettings()->value(Common::SettingsNames::imapArchiveFolderName).toString();
    if (!configured.isEmpty())
        return configured;
    // Otherwise whatever the model resolved for Archive, such as the server's \Archive folder
    const Imap::Mailbox::Model *model = imapModel();
    const QString special = model ? model->specialFolderMailbox(Imap::Mailbox::SpecialUse::Archive) : QString();
    return special.isEmpty() ? Common::SettingsNames::imapDefaultArchiveFolderName : special;
}

bool MainWindow::deleteMovesToTrash() const
{
    return accountSettings()->value(Common::SettingsNames::imapDeleteMovesToTrash, true).toBool();
}

QString MainWindow::deleteTrashTarget() const
{
    using namespace UiUtils::DestructiveActions;
    const Imap::Mailbox::Model *model = imapModel();
    if (!model)
        return QString();
    const QString trash = model->specialFolderMailbox(Imap::Mailbox::SpecialUse::Trash);
    return chooseDeleteAction(deleteMovesToTrash(), trash, sourceMailboxName(translatedSelection())) == DeleteAction::MoveToTrash
               ? trash
               : QString();
}

void MainWindow::updateDeleteActionMode()
{
    // The models are set up before the actions exist
    if (!m_deleteMovesToTrash)
        return;
    const QString trash = deleteTrashTarget();
    // setIcon() notifies every button of the action even for the same icon, so only a new mode is applied
    if (!m_deleteActionTrash || *m_deleteActionTrash != trash) {
        m_deleteActionTrash = trash;
        markAsDeleted->setCheckable(trash.isEmpty());
        if (trash.isEmpty()) {
            markAsDeleted->setText(m_markAsDeletedText);
            markAsDeleted->setIcon(m_markAsDeletedIcon);
            markAsDeleted->setToolTip(QString());
        } else {
            markAsDeleted->setText(i18nc("@action", "Move to &Trash"));
            markAsDeleted->setIcon(quickActionIcon(QStringLiteral("user-trash")));
            markAsDeleted->setToolTip(i18nc("@info:tooltip", "Move the selected messages to %1", trash));
        }
    }
    updateReaderActions();
}

void MainWindow::slotShowShortcutOverview()
{
    if (!m_shortcutOverview) {
        // Menus first, in the order of the menu bar, then the parts of the window
        QStringList areaOrder;
        for (const QAction *top : menuBar()->actions()) {
            if (top->menu())
                areaOrder << UiUtils::ShortcutOverview::plainText(top->text());
        }
        areaOrder << i18nc("@title:group", "Message List") << i18nc("@title:group", "Message Reader")
                  << i18nc("@title:group", "General") << i18nc("@title:group", "Other Windows")
                  << i18nc("@title:group", "Vim Mode");
        m_shortcutOverview = new ShortcutOverviewDialog([this]() { return shortcutOverviewEntries(); }, areaOrder, this);
    }
    m_shortcutOverview->show();
    m_shortcutOverview->raise();
    m_shortcutOverview->activateWindow();
}

QList<UiUtils::ShortcutOverview::Entry> MainWindow::shortcutOverviewEntries() const
{
    using UiUtils::ShortcutOverview::Entry;
    using UiUtils::ShortcutOverview::plainText;

    // The top-level menu which each action of the menu bar is in, including its submenus
    QHash<const QAction *, QString> menuAreas;
    std::function<void(const QMenu *, const QString &)> collect = [&menuAreas, &collect](const QMenu *menu, const QString &area) {
        for (const QAction *action : menu->actions()) {
            if (const QMenu *submenu = action->menu())
                collect(submenu, area);
            else if (!menuAreas.contains(action))
                menuAreas.insert(action, area);
        }
    };
    for (const QAction *top : menuBar()->actions()) {
        if (const QMenu *menu = top->menu())
            collect(menu, plainText(top->text()));
    }

    const QString listArea = i18nc("@title:group", "Message List");
    const QString readerArea = i18nc("@title:group", "Message Reader");
    const QString generalArea = i18nc("@title:group", "General");
    const QString otherArea = i18nc("@title:group", "Other Windows");
    const QList<QAction *> listActions = msgListWidget->tree->actions();
    QList<Entry> entries;
    const auto &descriptions = ShortcutHandler::instance()->actionDescriptions();
    for (auto it = descriptions.cbegin(); it != descriptions.cend(); ++it) {
        const QAction *action = it->action;
        Entry entry;
        entry.id = it.key();
        entry.text = plainText(action ? action->text() : it->text);
        if (action)
            entry.shortcuts = action->shortcuts();
        else if (!it->shortcut.isEmpty())
            entry.shortcuts = {QKeySequence(it->shortcut)};

        QWidget *owner = action ? qobject_cast<QWidget *>(action->parent()) : nullptr;
        bool inReader = false;
        for (QWidget *widget = owner; widget && !inReader; widget = widget->parentWidget())
            inReader = qobject_cast<MessageView *>(widget);
        if (!action)
            entry.area = otherArea; // Created only with a window which is not open
        else if (menuAreas.contains(action))
            entry.area = menuAreas.value(action);
        else if (inReader)
            entry.area = readerArea;
        else if (listActions.contains(const_cast<QAction *>(action)))
            entry.area = listArea;
        else
            entry.area = owner && owner->window() != this ? otherArea : generalArea;
        entries << entry;
    }
    if (m_vimMode && m_vimMode->isEnabled()) {
        // Vim sequences are not key sequences of actions, so they are named in the text
        const UiUtils::Keymap::Bindings bindings = UiUtils::Keymap::merged(
            UiUtils::SettingsStore::bindings(*KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"))));
        for (const UiUtils::Keymap::Command &command : UiUtils::Keymap::commands()) {
            const QString keys = bindings.value(command.id);
            if (keys.isEmpty())
                continue;
            Entry entry;
            entry.id = QLatin1String("vim.") + command.id;
            entry.area = i18nc("@title:group", "Vim Mode");
            entry.text = i18nc("@info Vim keys and what they do", "%1: %2", keys, UiUtils::Keymap::description(command.id));
            entries << entry;
        }
    }
    return entries;
}

namespace {
/** The title of an activity entry from the progress text of its work */
QString withoutEllipsis(QString text)
{
    if (text.endsWith(QChar(0x2026)))
        text.chop(1);
    return text;
}
}

void MainWindow::trackMove(const QModelIndexList &messages, const QString &source, const QString &target,
    const MoveKind kind, const QString &retryKey)
{
    Imap::Mailbox::Model *current = imapModel();
    if (!current || messages.isEmpty() || target.isEmpty())
        return;
    // Named before the move takes the messages away
    const QList<MessageRef> refs = messageRefs(messages);
    const int count = messages.size();
    Imap::Mailbox::ImapTask *task = current->copyMoveMessages(target, messages, Imap::Mailbox::CopyMoveOperation::MOVE);
    if (!task)
        return;
    QPointer<Imap::Mailbox::Model> model = current;
    QString busy;
    switch (kind) {
    case MoveKind::Trash:
        busy = i18ncp("@info", "Moving %1 message to Trash…", "Moving %1 messages to Trash…", count);
        break;
    case MoveKind::Archive:
        busy = i18ncp("@info", "Archiving %1 message…", "Archiving %1 messages…", count);
        break;
    case MoveKind::Folder:
        busy = i18ncp("@info", "Moving %1 message to %2…", "Moving %1 messages to %2…", count, target);
        break;
    }
    connect(task, &Imap::Mailbox::ImapTask::completed, this,
        [this, model, source, target, count, kind, retryKey](Imap::Mailbox::ImapTask *finished) {
            if (!retryKey.isEmpty())
                recordOperationDone(retryKey);
            if (!showsModel(model))
                return;
            QString text;
            QString openText;
            switch (kind) {
            case MoveKind::Trash:
                text = i18ncp("@info", "Moved %1 message to Trash", "Moved %1 messages to Trash", count);
                openText = i18nc("@action:button", "Open Trash");
                break;
            case MoveKind::Archive:
                text = i18ncp("@info", "Archived %1 message", "Archived %1 messages", count);
                openText = i18nc("@action:button", "Open Archive");
                break;
            case MoveKind::Folder:
                text = i18ncp("@info", "Moved %1 message to %2", "Moved %1 messages to %2", count, target);
                openText = i18nc("@action:button", "Open Folder");
                break;
            }
            InlineNotificationBar::Action action;
            auto *copyMove = qobject_cast<Imap::Mailbox::CopyMoveMessagesTask *>(finished);
            if (copyMove && copyMove->copyUidValidity() && !copyMove->copyUidDestinationUids().isEmpty()
                && !source.isEmpty()) {
                const uint uidValidity = copyMove->copyUidValidity();
                const Imap::Uids uids = copyMove->copyUidDestinationUids();
                action = {i18nc("@action:button", "Undo"), QStringLiteral("edit-undo"), QStringLiteral("undoMove"),
                    [this, model, source, target, uidValidity, uids]() {
                        if (showsModel(model))
                            actOnModel(model, [&]() { undoMove(source, target, uidValidity, uids); });
                    }};
            } else {
                // Without COPYUID (RFC 4315) there is no telling which messages in the target are the moved ones
                action
                    = {openText, QStringLiteral("go-jump"), QStringLiteral("openMoveTarget"), [this, model, target]() {
                           if (!showsModel(model))
                               return;
                           // The folder belongs to the account of the messages, which the Unified Inbox may not show
                           if (const QString accountId = accountOfModel(model);
                               !accountId.isEmpty() && accountId != m_accounts->activeAccount())
                               switchAccount(accountId);
                           goToMailbox(target);
                       }};
            }
            m_notificationBar->showInfo(forAccountOf(model, text), {action});
        });
    connect(task, &Imap::Mailbox::ImapTask::failed, this,
        [this, model, source, target, count, kind, refs, retryKey, busy](const QString &message) {
            recordFailedOperation(model, retryKey, withoutEllipsis(busy), message,
                [this, model, source, target, kind, refs](const QString &key) {
                    const QModelIndexList again = resolveMessageRefs(model, refs);
                    if (again.isEmpty())
                        return false;
                    trackMove(again, source, target, kind, key);
                    return true;
                });
            if (!showsModel(model))
                return;
            switch (kind) {
            case MoveKind::Archive:
                slotMoveToArchiveFailed(forAccountOf(model, message), count);
                break;
            case MoveKind::Trash:
                m_notificationBar->showError(forAccountOf(model,
                    i18ncp("@info", "Could not move %1 message to Trash: %2", "Could not move %1 messages to Trash: %2",
                        count, message)));
                break;
            case MoveKind::Folder:
                m_notificationBar->showError(forAccountOf(model,
                    i18ncp("@info", "Could not move %1 message to %2: %3", "Could not move %1 messages to %2: %3",
                        count, target, message)));
                break;
            }
        });
    showBusyWhileRunning(task, busy);
}

void MainWindow::trackCopy(const QModelIndexList &messages, const QString &target, const QString &retryKey)
{
    Imap::Mailbox::Model *current = imapModel();
    if (!current || messages.isEmpty() || target.isEmpty())
        return;
    const QList<MessageRef> refs = messageRefs(messages);
    const int count = messages.size();
    Imap::Mailbox::ImapTask *task = current->copyMoveMessages(target, messages, Imap::Mailbox::CopyMoveOperation::COPY);
    if (!task)
        return;
    QPointer<Imap::Mailbox::Model> model = current;
    const QString busy = i18ncp("@info", "Copying %1 message to %2…", "Copying %1 messages to %2…", count, target);
    connect(task, &Imap::Mailbox::ImapTask::completed, this, [this, model, target, count, retryKey]() {
        if (!retryKey.isEmpty())
            recordOperationDone(retryKey);
        if (!showsModel(model))
            return;
        const InlineNotificationBar::Action open{i18nc("@action:button", "Open Folder"), QStringLiteral("go-jump"),
                                                 QStringLiteral("openCopyTarget"), [this, model, target]() {
            if (!showsModel(model))
                return;
            // The folder belongs to the account of the messages, which the Unified Inbox may not show
            if (const QString accountId = accountOfModel(model); !accountId.isEmpty() && accountId != m_accounts->activeAccount())
                switchAccount(accountId);
            goToMailbox(target);
        }};
        m_notificationBar->showInfo(forAccountOf(model, i18ncp("@info", "Copied %1 message to %2", "Copied %1 messages to %2",
                                                               count, target)),
                                    {open});
    });
    connect(task, &Imap::Mailbox::ImapTask::failed, this,
        [this, model, target, count, refs, retryKey, busy](const QString &message) {
            recordFailedOperation(
                model, retryKey, withoutEllipsis(busy), message, [this, model, target, refs](const QString &key) {
                    const QModelIndexList again = resolveMessageRefs(model, refs);
                    if (again.isEmpty())
                        return false;
                    trackCopy(again, target, key);
                    return true;
                });
            if (!showsModel(model))
                return;
            m_notificationBar->showError(forAccountOf(model,
                i18ncp("@info", "Could not copy %1 message to %2: %3", "Could not copy %1 messages to %2: %3", count,
                    target, message)));
        });
    showBusyWhileRunning(task, busy);
}

void MainWindow::showBusyWhileRunning(Imap::Mailbox::ImapTask *task, const QString &busyText)
{
    if (!task || task->isFinished() || busyText.isEmpty())
        return;
    QPointer<Imap::Mailbox::Model> model = imapModel();
    auto busyId = std::make_shared<quint64>(0);
    // Quick operations finish without a flash of the progress indicator
    QPointer<QTimer> delay = new QTimer(task);
    delay->setSingleShot(true);
    delay->setInterval(400);
    // The bar as the context: tasks can outlive it while the window goes away
    connect(delay, &QTimer::timeout, m_notificationBar, [this, model, busyId, busyText]() {
        if (showsModel(model))
            *busyId = m_notificationBar->showBusy(forAccountOf(model, busyText));
    });
    auto finish = [this, delay, busyId]() {
        if (delay)
            delay->stop();
        // A result shown in the meantime stays
        m_notificationBar->dismissBusy(*busyId);
    };
    connect(task, &Imap::Mailbox::ImapTask::completed, m_notificationBar, finish);
    connect(task, &Imap::Mailbox::ImapTask::failed, m_notificationBar, finish);
    connect(task, &QObject::destroyed, m_notificationBar, finish);
    delay->start();
}

void MainWindow::trackFlagChange(const QModelIndexList &messages, const FlagOperation &operation,
    const QString &busyText, const QString &doneText, const std::function<QString(const QString &)> &failedText,
    const QString &retryKey)
{
    Imap::Mailbox::Model *current = imapModel();
    if (!current || messages.isEmpty() || !operation)
        return;
    const QList<MessageRef> refs = messageRefs(messages);
    Imap::Mailbox::ImapTask *task = operation(current, messages);
    if (!task)
        return;
    QPointer<Imap::Mailbox::Model> model = current;
    connect(task, &Imap::Mailbox::ImapTask::completed, this, [this, model, doneText, retryKey]() {
        if (!retryKey.isEmpty())
            recordOperationDone(retryKey);
        if (!doneText.isEmpty() && showsModel(model))
            m_notificationBar->showInfo(forAccountOf(model, doneText));
    });
    connect(task, &Imap::Mailbox::ImapTask::failed, this,
        [this, model, refs, operation, busyText, doneText, failedText, retryKey](const QString &message) {
            // A single change shows no progress, so its error names it
            const QString title = busyText.isEmpty() ? failedText(message) : withoutEllipsis(busyText);
            recordFailedOperation(model, retryKey, title, busyText.isEmpty() ? QString() : message,
                [this, model, refs, operation, busyText, doneText, failedText](const QString &key) {
                    const QModelIndexList again = resolveMessageRefs(model, refs);
                    if (again.isEmpty())
                        return false;
                    trackFlagChange(again, operation, busyText, doneText, failedText, key);
                    return true;
                });
            if (showsModel(model))
                m_notificationBar->showError(forAccountOf(model, failedText(message)));
        });
    showBusyWhileRunning(task, busyText);
}

QList<MainWindow::MessageRef> MainWindow::messageRefs(const QModelIndexList &messages)
{
    QList<MessageRef> refs;
    refs.reserve(messages.size());
    for (const QModelIndex &message : messages) {
        MessageRef ref;
        ref.mailbox = message.data(Imap::Mailbox::RoleMailboxName).toString();
        ref.uidValidity = message.data(Imap::Mailbox::RoleMailboxUidValidity).toUInt();
        ref.uid = message.data(Imap::Mailbox::RoleMessageUid).toUInt();
        if (!ref.mailbox.isEmpty() && ref.uid)
            refs << ref;
    }
    return refs;
}

QModelIndexList MainWindow::resolveMessageRefs(Imap::Mailbox::Model *model, const QList<MessageRef> &refs)
{
    QModelIndexList found;
    if (!model)
        return found;
    for (const MessageRef &ref : refs) {
        const QModelIndex mailbox = model->mailboxIndexByName(ref.mailbox);
        if (!mailbox.isValid())
            continue;
        // Loads what the cache knows about the folder, as after the folder list was loaded again
        model->rowCount(model->index(0, 0, mailbox));
        // UIDs of a folder whose UIDVALIDITY changed name other messages
        const uint uidValidity = mailbox.data(Imap::Mailbox::RoleMailboxUidValidity).toUInt();
        if (ref.uidValidity && uidValidity && uidValidity != ref.uidValidity)
            continue;
        const QModelIndex message = model->messageIndexByUid(ref.mailbox, ref.uid);
        if (message.isValid())
            found << message;
    }
    return found;
}

void MainWindow::recordFailedOperation(
    Imap::Mailbox::Model *model, QString key, const QString &title, const QString &error, const Rerun &rerun)
{
    using UiUtils::ActivityLog;
    if (!model || !m_mailAutomation)
        return;
    ActivityLog *log = m_mailAutomation->activity();
    if (key.isEmpty())
        key = QStringLiteral("operation:%1").arg(++m_operationSerial);
    ActivityLog::Entry entry;
    entry.key = key;
    entry.kind = ActivityLog::Kind::Job;
    entry.state = ActivityLog::State::Failed;
    entry.accountId = accountOfModel(model);
    entry.title = title;
    entry.error = error;
    entry.action = ActivityLog::Action::Retry;
    log->record(entry);
    QPointer<Imap::Mailbox::Model> guard = model;
    log->setRetry(key, [this, guard, key, title, rerun]() {
        if (!guard)
            return;
        if (!guard->isNetworkAvailable()) {
            recordFailedOperation(guard, key, title, i18nc("@info", "The account is offline."), rerun);
            return;
        }
        bool started = false;
        actOnModel(guard, [&]() { started = rerun(key); });
        if (started)
            return;
        // Another client, or the server despite the error, already did it
        ActivityLog *log = m_mailAutomation->activity();
        const int row = log->find(key);
        if (row < 0)
            return;
        ActivityLog::Entry resolved = log->entry(row);
        resolved.state = ActivityLog::State::Done;
        resolved.detail = i18nc("@info", "The messages are no longer there; nothing is left to do.");
        resolved.error.clear();
        resolved.action = ActivityLog::Action::None;
        resolved.time = QDateTime();
        log->record(resolved);
    });
}

void MainWindow::recordOperationDone(const QString &key)
{
    using UiUtils::ActivityLog;
    if (!m_mailAutomation)
        return;
    ActivityLog *log = m_mailAutomation->activity();
    const int row = log->find(key);
    if (row < 0)
        return;
    ActivityLog::Entry entry = log->entry(row);
    entry.state = ActivityLog::State::Done;
    entry.detail = i18nc("@info", "Done after trying again");
    entry.error.clear();
    entry.action = ActivityLog::Action::None;
    entry.time = QDateTime();
    log->record(entry);
}

void MainWindow::recordFolderSyncFailed(const QString &accountId, const QString &mailbox, const QString &message)
{
    using UiUtils::ActivityLog;
    if (!m_mailAutomation || accountId.isEmpty() || mailbox.isEmpty())
        return;
    ActivityLog *log = m_mailAutomation->activity();
    ActivityLog::Entry entry;
    entry.key = QLatin1String("folder-sync:") + accountId + QLatin1Char(':') + mailbox;
    entry.kind = ActivityLog::Kind::Sync;
    entry.state = ActivityLog::State::Failed;
    entry.accountId = accountId;
    entry.title = i18nc("@info folder name", "Opening %1", mailbox);
    entry.error = message;
    entry.action = ActivityLog::Action::Retry;
    log->record(entry);
    log->setRetry(entry.key, [this, accountId, mailbox]() {
        Imap::ImapAccess *access = m_accountAccess.value(accountId);
        auto *model = access ? qobject_cast<Imap::Mailbox::Model *>(access->imapModel()) : nullptr;
        if (!model)
            return;
        const QModelIndex index = model->mailboxIndexByName(mailbox);
        if (!model->isNetworkAvailable() || !index.isValid()) {
            recordFolderSyncFailed(accountId, mailbox,
                index.isValid() ? i18nc("@info", "The account is offline.")
                                : i18nc("@info", "The folder no longer exists."));
            return;
        }
        // The folder's synchronization reports success; see monitorAccount()
        model->resyncMailbox(index);
    });
}

void MainWindow::undoMove(const QString &source, const QString &target, const uint uidValidity, const Imap::Uids &uids)
{
    Imap::Mailbox::Model *model = imapModel();
    const QModelIndex targetMailbox = model->mailboxIndexByName(target);
    Imap::Mailbox::ImapTask *task = targetMailbox.isValid()
            ? model->copyMoveMessagesByUid(targetMailbox, uids, source, Imap::Mailbox::CopyMoveOperation::MOVE, uidValidity)
            : nullptr;
    if (!task) {
        m_notificationBar->showError(i18nc("@info", "Could not move the messages back to %1.", source));
        return;
    }
    QPointer<Imap::Mailbox::Model> guard = model;
    const int count = uids.size();
    connect(task, &Imap::Mailbox::ImapTask::completed, this, [this, guard, source, target, count]() {
        if (!showsModel(guard))
            return;
        m_notificationBar->showInfo(forAccountOf(guard, i18ncp("@info", "Moved %1 message back to %2", "Moved %1 messages back to %2",
                                                               count, source)));
        actOnModel(guard, [&]() { reopenCurrentMailbox(target); });
    });
    connect(task, &Imap::Mailbox::ImapTask::failed, this, [this, guard, source, target](const QString &message) {
        if (!showsModel(guard))
            return;
        m_notificationBar->showError(forAccountOf(guard, i18nc("@info", "Could not move the messages back to %1: %2", source, message)));
        actOnModel(guard, [&]() { reopenCurrentMailbox(target); });
    });
    showBusyWhileRunning(task, i18ncp("@info", "Moving %1 message back to %2…", "Moving %1 messages back to %2…", count, source));
}

void MainWindow::reopenCurrentMailbox(const QString &usedMailbox)
{
    // Only one mailbox is selected per connection; selecting another one leaves the message list stale
    auto *msgList = qobject_cast<Imap::Mailbox::MsgListModel *>(currentAccess()->msgListModel());
    const QModelIndex current = msgList ? msgList->currentMailbox() : QModelIndex();
    if (current.isValid() && current.data(Imap::Mailbox::RoleMailboxName).toString() != usedMailbox)
        imapModel()->switchToMailbox(current);
}

void MainWindow::slotExpunge()
{
    auto *msgList = qobject_cast<Imap::Mailbox::MsgListModel *>(currentAccess()->msgListModel());
    const QModelIndex mailbox = msgList->currentMailbox();
    if (!mailbox.isValid())
        return;
    const QString name = mailbox.data(Imap::Mailbox::RoleMailboxName).toString();
    int marked = 0;
    for (int row = 0; row < msgList->rowCount(); ++row) {
        if (msgList->index(row, 0).data(Imap::Mailbox::RoleMessageIsMarkedDeleted).toBool())
            ++marked;
    }
    // Flags of messages which have not been loaded yet are unknown, so zero means "maybe none"
    const QString text = marked > 0
            ? i18ncp("@info", "Permanently remove %1 message marked as deleted from %2? It cannot be recovered.",
                     "Permanently remove %1 messages marked as deleted from %2? They cannot be recovered.", marked, name)
            : i18nc("@info", "Permanently remove all messages marked as deleted from %1? They cannot be recovered.", name);
    if (!confirmPermanentAction(UiUtils::DestructiveActions::Confirmation::Expunge, i18nc("@title:window", "Expunge"), text,
                                i18nc("@action:button", "&Expunge"))) {
        return;
    }
    const QModelIndex current = msgList->currentMailbox();
    if (current.isValid() && current.data(Imap::Mailbox::RoleMailboxName).toString() == name)
        imapModel()->expungeMailbox(current);
}

void MainWindow::slotMarkCurrentMailboxRead()
{
    imapModel()->markMailboxAsRead(mboxTree->currentIndex());
}

void MainWindow::slotDownloadTextForSearch()
{
    using Imap::Mailbox::SearchTextFetcher;
    const QString mailbox = m_actionDownloadSearchText->data().toString();
    Imap::Mailbox::SearchIndexer *indexer = m_imapAccess ? m_imapAccess->searchIndexer() : nullptr;
    Imap::Mailbox::Model *model = imapModel();
    if (mailbox.isEmpty() || !indexer || !indexer->isAvailable() || !model || !model->isNetworkAvailable()
        || (m_searchTextFetcher && m_searchTextFetcher->isRunning()))
        return;
    Imap::Uids missing;
    const auto entries = indexer->index()->entries(mailbox);
    for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
        if (!it->hasText)
            missing << it.key();
    }
    if (missing.isEmpty()) {
        m_notificationBar->showInfo(
            i18nc("@info folder name", "The text of every indexed message in %1 is already on this computer.", mailbox));
        return;
    }
    std::sort(missing.begin(), missing.end());
    const QString question = i18ncp("@info folder name",
                                    "Download the text of %1 message in %2, so that it can be searched offline?",
                                    "Download the text of %1 messages in %2, so that they can be searched offline?",
                                    int(missing.size()), mailbox);
    if (QMessageBox::question(this, i18nc("@title:window", "Download Message Text"), question) != QMessageBox::Yes)
        return;
    if (m_searchTextFetcher)
        m_searchTextFetcher->deleteLater();
    auto *fetcher = new SearchTextFetcher(model, mailbox, missing, this);
    m_searchTextFetcher = fetcher;
    const quint64 busy = m_notificationBar->showBusy(
        i18ncp("@info folder name", "Downloading the text of %1 message in %2…",
               "Downloading the text of %1 messages in %2…", int(missing.size()), mailbox));
    QPointer<Imap::ImapAccess> access = m_imapAccess;
    QPointer<Imap::Mailbox::SearchIndexer> index = indexer;
    UiUtils::ActivityLog::Entry job;
    job.key = QLatin1String("search-text:") + m_accounts->activeAccount() + QLatin1Char(':') + mailbox;
    job.kind = UiUtils::ActivityLog::Kind::Job;
    job.state = UiUtils::ActivityLog::State::Running;
    job.accountId = m_accounts->activeAccount();
    job.title = i18nc("@info folder name", "Text of %1 for offline search", mailbox);
    job.progress = 0;
    m_mailAutomation->activity()->record(job);
    connect(fetcher, &SearchTextFetcher::progressChanged, this, [this, fetcher, job]() mutable {
        if (fetcher->total() > 0)
            job.progress = 100 * (fetcher->downloaded() + fetcher->failed()) / fetcher->total();
        job.time = QDateTime();
        m_mailAutomation->activity()->record(job);
    });
    connect(fetcher, &SearchTextFetcher::finished, this, [this, fetcher, job]() mutable {
        job.time = QDateTime();
        job.progress = -1;
        job.state = fetcher->failed() > 0 ? UiUtils::ActivityLog::State::Failed : UiUtils::ActivityLog::State::Done;
        job.detail = i18ncp("@info", "%1 message downloaded", "%1 messages downloaded", fetcher->downloaded());
        if (fetcher->failed() > 0)
            job.error = i18ncp("@info", "%1 message could not be downloaded", "%1 messages could not be downloaded",
                               fetcher->failed());
        m_mailAutomation->activity()->record(job);
    });
    connect(fetcher, &SearchTextFetcher::finished, this, [this, fetcher, busy, access, index]() {
        m_notificationBar->dismissBusy(busy);
        reselectShownFolder(access);
        // Index what arrived right away rather than after the usual pause
        if (index)
            index->update();
        if (fetcher->failed() > 0) {
            m_notificationBar->showError(i18ncp("@info folder name",
                                                "The text of %1 message in %2 could not be downloaded.",
                                                "The text of %1 messages in %2 could not be downloaded.",
                                                fetcher->failed(), fetcher->mailbox()));
        } else {
            m_notificationBar->showInfo(i18ncp("@info folder name",
                                               "Downloaded the text of %1 message in %2 for offline search.",
                                               "Downloaded the text of %1 messages in %2 for offline search.",
                                               fetcher->downloaded(), fetcher->mailbox()));
        }
        fetcher->deleteLater();
    });
    fetcher->start();
}

void MainWindow::slotCreateMailboxBelowCurrent()
{
    createMailboxBelow(mboxTree->currentIndex());
}

void MainWindow::slotCreateTopMailbox()
{
    createMailboxBelow(QModelIndex());
}

void MainWindow::createMailboxBelow(const QModelIndex &index)
{
    Imap::Mailbox::TreeItemMailbox *mboxPtr = index.isValid() ?
            dynamic_cast<Imap::Mailbox::TreeItemMailbox *>(
                Imap::Mailbox::Model::realTreeItem(index)) :
            nullptr;

    Ui::CreateMailboxDialog ui;
    QDialog *dialog = new QDialog(this);
    ui.setupUi(dialog);

    dialog->setWindowTitle(mboxPtr ?
                           tr("Create a Subfolder of %1").arg(mboxPtr->mailbox()) :
                           tr("Create a Top-level Mailbox"));

    if (dialog->exec() == QDialog::Accepted) {
        QStringList parts;
        if (mboxPtr)
            parts << mboxPtr->mailbox();
        parts << ui.mailboxName->text();
        if (ui.otherMailboxes->isChecked())
            parts << QString();
        QString targetName = parts.join(mboxPtr ? mboxPtr->separator() : QString());   // FIXME: top-level separator
        imapModel()->createMailbox(targetName,
                                   ui.subscribe->isChecked() ?
                                       Imap::Mailbox::AutoSubscription::SUBSCRIBE :
                                       Imap::Mailbox::AutoSubscription::NO_EXPLICIT_SUBSCRIPTION
                                       );
    }
}

void MainWindow::slotDeleteCurrentMailbox()
{
    if (! mboxTree->currentIndex().isValid())
        return;

    QModelIndex mailbox = Imap::deproxifiedIndex(mboxTree->currentIndex());
    Q_ASSERT(mailbox.isValid());
    QString name = mailbox.data(Imap::Mailbox::RoleMailboxName).toString();

    if (QMessageBox::question(this, tr("Delete Mailbox"),
                              tr("Are you sure to delete mailbox %1?").arg(name),
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        imapModel()->deleteMailbox(name);
    }
}

bool MainWindow::canRenameMailbox(const QModelIndex &index) const
{
    using namespace Imap::Mailbox;
    return index.isValid() && imapModel() && imapModel()->isNetworkAvailable()
            && !index.data(RoleMailboxName).toString().isEmpty()
            && !index.data(RoleMailboxIsINBOX).toBool()
            && index.data(RoleMailboxIsSelectable).toBool();
}

void MainWindow::slotRenameCurrentMailbox()
{
    using namespace Imap::Mailbox;
    const QModelIndex index = mboxTree->currentIndex();
    if (!canRenameMailbox(index))
        return;

    const QString oldName = index.data(RoleMailboxName).toString();
    const QString separator = index.data(RoleMailboxSeparator).toString();
    const int split = separator.isEmpty() ? -1 : oldName.lastIndexOf(separator);
    const QString prefix = split >= 0 ? oldName.left(split + separator.size()) : QString();
    const QString leaf = oldName.mid(prefix.size());

    bool ok = false;
    const QString newLeaf = QInputDialog::getText(this, i18nc("@title:window", "Rename Folder"),
                                                  i18nc("@label:textbox", "New name for %1:", oldName),
                                                  QLineEdit::Normal, leaf, &ok).trimmed();
    if (!ok || newLeaf.isEmpty() || newLeaf == leaf)
        return;
    if (!separator.isEmpty() && newLeaf.contains(separator)) {
        m_notificationBar->showError(i18nc("@info", "A folder name cannot contain \"%1\".", separator));
        return;
    }

    const QString newName = prefix + newLeaf;
    // The server closes the open mailbox when it, or a folder above it, is renamed
    const QString current = currentMailboxName();
    if (current == oldName || (!separator.isEmpty() && current.startsWith(oldName + separator)))
        m_renamedCurrentMailbox.insert(oldName, newName + current.mid(oldName.size()));
    imapModel()->renameMailbox(oldName, newName);
}

void MainWindow::emptySpecialFolder(const Imap::Mailbox::SpecialUse kind)
{
    using namespace Imap::Mailbox;
    using UiUtils::DestructiveActions::Confirmation;
    Model *model = imapModel();
    if (!model || !model->isNetworkAvailable())
        return;

    // A folder picked in the tree wins over the account's main Trash or Junk folder
    QString name;
    const QModelIndex treeIndex = mboxTree->currentIndex();
    if (treeIndex.isValid() && static_cast<SpecialUse>(treeIndex.data(RoleMailboxSpecialUse).toInt()) == kind)
        name = treeIndex.data(RoleMailboxName).toString();
    if (name.isEmpty())
        name = model->specialFolderMailbox(kind);
    const QModelIndex mailbox = model->mailboxIndexByName(name);
    if (!mailbox.isValid())
        return;

    // Unknown until the folder has been opened or its status fetched
    const QVariant total = mailbox.data(RoleTotalMessageCount);
    if (total.isValid() && total.toInt() == 0) {
        m_notificationBar->showInfo(i18nc("@info", "%1 is already empty", name));
        return;
    }
    const bool trash = kind == SpecialUse::Trash;
    const QString text = total.isValid()
            ? i18ncp("@info", "Permanently delete %1 message in %2? It cannot be recovered.",
                     "Permanently delete all %1 messages in %2? They cannot be recovered.", total.toInt(), name)
            : i18nc("@info", "Permanently delete all messages in %1? They cannot be recovered.", name);
    if (!confirmPermanentAction(Confirmation::EmptyFolder,
                                trash ? i18nc("@title:window", "Empty Trash") : i18nc("@title:window", "Empty Junk"), text,
                                trash ? i18nc("@action:button", "&Empty Trash") : i18nc("@action:button", "&Empty Junk"))) {
        return;
    }
    const QModelIndex confirmed = imapModel()->mailboxIndexByName(name);
    if (confirmed.isValid())
        imapModel()->emptyMailbox(confirmed);
}

void MainWindow::updateMessageFlags()
{
    updateMessageFlagsOf(QModelIndex());
}

/** @short Update the message actions once the current burst of model changes is over

Syncing a folder changes the flags of every message one by one, and another client's removals arrive message by message;
updating every action for each of them made opening a folder of 50,000 messages take about a minute.
*/
void MainWindow::scheduleMessageFlagsUpdate()
{
    if (std::exchange(m_messageFlagsUpdatePending, true))
        return;
    QMetaObject::invokeMethod(
        this,
        [this]() {
            m_messageFlagsUpdatePending = false;
            updateMessageFlags();
        },
        Qt::QueuedConnection);
}

void MainWindow::updateMessageFlagsOf(const QModelIndex &index)
{
    if (!m_acting)
        updateReaderActions();
    QModelIndexList indexes = index.isValid() ? QModelIndexList() << index : translatedSelection();
    const bool isValid = !indexes.isEmpty() &&
                         // either we operate on the -already valided- selection or the index must be valid
                         (!index.isValid() || index.data(Imap::Mailbox::RoleMessageUid).toUInt() > 0);
    bool okToModify = isValid;
    if (showsUnifiedList() && !m_acting) {
        // Every account of the selected messages has to be reachable
        for (const QModelIndex &message : std::as_const(indexes)) {
            const auto *model = qobject_cast<const Imap::Mailbox::Model *>(message.model());
            if (!model || !model->isNetworkAvailable()) {
                okToModify = false;
                break;
            }
        }
    } else {
        okToModify = okToModify && imapModel()->isNetworkAvailable();
    }

    markAsRead->setEnabled(okToModify);
    markAsDeleted->setEnabled(okToModify);
    if (m_deletePermanently)
        m_deletePermanently->setEnabled(okToModify);
    markAsFlagged->setEnabled(okToModify);
    markAsJunk->setEnabled(okToModify);
    markAsNotJunk->setEnabled(okToModify);
    for (QAction *action : {m_markAsUnread, m_moveToFolder, m_copyToFolder}) {
        if (action)
            action->setEnabled(okToModify);
    }
    for (QMenu *menu : {m_moveToMenu, m_copyToMenu}) {
        if (menu)
            menu->menuAction()->setEnabled(okToModify);
    }

    // There's no point in moving from Archive to, well, Archive
    const QString archiveFolderName = archiveTarget();
    moveToArchive->setEnabled(okToModify &&
                              std::any_of(indexes.cbegin(), indexes.cend(),
                                          [archiveFolderName](const QModelIndex &i) {
        return i.data(Imap::Mailbox::RoleMailboxName) != archiveFolderName;
    }));

    tag1->setEnabled(okToModify);
    tag2->setEnabled(okToModify);
    tag3->setEnabled(okToModify);
    tag4->setEnabled(okToModify);
    tag5->setEnabled(okToModify);
    tag6->setEnabled(okToModify);
    tag7->setEnabled(okToModify);
    tag8->setEnabled(okToModify);
    tag9->setEnabled(okToModify);

    bool isRead    = isValid,
         isDeleted = isValid,
         isFlagged = isValid,
         isJunk    = isValid,
         isNotJunk = isValid,
         hasTag1   = isValid,
         hasTag2   = isValid,
         hasTag3   = isValid,
         hasTag4   = isValid,
         hasTag5   = isValid,
         hasTag6   = isValid,
         hasTag7   = isValid,
         hasTag8   = isValid,
         hasTag9   = isValid;
    auto updateTag = [this](const QModelIndex &i, bool &hasTag, int index) {
        if (hasTag && !m_favoriteTags->tagNameByIndex(index).isEmpty() &&
                !i.data(Imap::Mailbox::RoleMessageFlags).toStringList().contains(m_favoriteTags->tagNameByIndex(index)))
        {
            hasTag = false;
        }
    };
    Q_FOREACH (const QModelIndex &i, indexes) {
#define UPDATE_STATE(PROP) \
        if (is##PROP && !i.data(Imap::Mailbox::RoleMessageIsMarked##PROP).toBool()) \
            is##PROP = false;
        UPDATE_STATE(Read)
        UPDATE_STATE(Deleted)
        UPDATE_STATE(Flagged)
        UPDATE_STATE(Junk)
        UPDATE_STATE(NotJunk)
#undef UPDATE_STATE
        updateTag(i, hasTag1, 0);
        updateTag(i, hasTag2, 1);
        updateTag(i, hasTag3, 2);
        updateTag(i, hasTag4, 3);
        updateTag(i, hasTag5, 4);
        updateTag(i, hasTag6, 5);
        updateTag(i, hasTag7, 6);
        updateTag(i, hasTag8, 7);
        updateTag(i, hasTag9, 8);
    }
    markAsRead->setChecked(isRead);
    updateDeleteActionMode();
    markAsDeleted->setChecked(isDeleted);
    markAsFlagged->setChecked(isFlagged);
    markAsJunk->setChecked(isJunk && !isNotJunk);
    markAsNotJunk->setChecked(isNotJunk && !isJunk);

    tag1->setChecked(hasTag1);
    tag2->setChecked(hasTag2);
    tag3->setChecked(hasTag3);
    tag4->setChecked(hasTag4);
    tag5->setChecked(hasTag5);
    tag6->setChecked(hasTag6);
    tag7->setChecked(hasTag7);
    tag8->setChecked(hasTag8);
    tag9->setChecked(hasTag9);
}

void MainWindow::updateActionsOnlineOffline(bool online)
{
    reloadMboxList->setEnabled(online);
    resyncMbox->setEnabled(online);
    expunge->setEnabled(online);
    createChildMailbox->setEnabled(online);
    createTopMailbox->setEnabled(online);
    deleteCurrentMailbox->setEnabled(online);
    if (m_renameMailbox)
        m_renameMailbox->setEnabled(online && canRenameMailbox(mboxTree->currentIndex()));
    updateGoToFolderActions();
    m_actionMarkMailboxAsRead->setEnabled(online);
    updateMessageFlags();
    showImapCapabilities->setEnabled(online);
    if (!online) {
        m_replyGuess->setEnabled(false);
        m_replyPrivate->setEnabled(false);
        m_replyAll->setEnabled(false);
        m_replyAllButMe->setEnabled(false);
        m_replyList->setEnabled(false);
        m_forwardAsAttachment->setEnabled(false);
        m_resend->setEnabled(false);
    }
}

void MainWindow::slotUpdateMessageActions()
{
#ifdef IMPRESARIO_HAVE_WEBENGINE
    // The identities of the account which received the message decide who a reply goes to
    const auto *messageModel = qobject_cast<const Imap::Mailbox::Model *>(
        Imap::deproxifiedIndex(m_messageWidget->messageView->currentMessage()).model());
    if (!m_acting && messageModel && messageModel != imapModel() &&
        actOnModel(messageModel, [this]() { slotUpdateMessageActions(); }))
        return;
    Composer::RecipientList dummy;
    m_replyPrivate->setEnabled(Composer::Util::replyRecipientList(Composer::REPLY_PRIVATE, senderIdentitiesModel(),
                                                                  m_messageWidget->messageView->currentMessage(), dummy));
    m_replyAllButMe->setEnabled(Composer::Util::replyRecipientList(Composer::REPLY_ALL_BUT_ME, senderIdentitiesModel(),
                                                                   m_messageWidget->messageView->currentMessage(), dummy,
                                                                   Composer::AccountIdentities::loginAddresses(*accountSettings())));
    m_replyAll->setEnabled(Composer::Util::replyRecipientList(Composer::REPLY_ALL, senderIdentitiesModel(),
                                                              m_messageWidget->messageView->currentMessage(), dummy));
    m_replyList->setEnabled(Composer::Util::replyRecipientList(Composer::REPLY_LIST, senderIdentitiesModel(),
                                                               m_messageWidget->messageView->currentMessage(), dummy));
    m_replyGuess->setEnabled(m_replyPrivate->isEnabled() || m_replyAllButMe->isEnabled()
                             || m_replyAll->isEnabled() || m_replyList->isEnabled());

    // Check the default reply mode
    // I suspect this is not going to work for everybody. Suggestions welcome...
    if (m_replyList->isEnabled()) {
        m_replyButton->setDefaultAction(m_replyList);
    } else if (m_replyAllButMe->isEnabled()) {
        m_replyButton->setDefaultAction(m_replyAllButMe);
    } else {
        m_replyButton->setDefaultAction(m_replyPrivate);
    }
    if (m_readerActions)
        m_readerActions->setReplyAction(m_replyButton->defaultAction());

    m_forwardAsAttachment->setEnabled(m_messageWidget->messageView->currentMessage().isValid());
    m_resend->setEnabled(m_messageWidget->messageView->currentMessage().isValid());
#endif
}

void MainWindow::scrollMessageUp()
{
#ifdef IMPRESARIO_HAVE_WEBENGINE
    m_messageWidget->area->ensureVisible(0, 0, 0, 0);
#endif
}

void MainWindow::slotReplyTo()
{
    reply(Composer::REPLY_PRIVATE);
}

void MainWindow::slotReplyAll()
{
    reply(Composer::REPLY_ALL);
}

void MainWindow::slotReplyAllButMe()
{
    reply(Composer::REPLY_ALL_BUT_ME);
}

void MainWindow::slotReplyList()
{
    reply(Composer::REPLY_LIST);
}

void MainWindow::slotReplyGuess()
{
    if (m_replyButton->defaultAction() == m_replyAllButMe) {
        slotReplyAllButMe();
    } else if (m_replyButton->defaultAction() == m_replyAll) {
        slotReplyAll();
    } else if (m_replyButton->defaultAction() == m_replyList) {
        slotReplyList();
    } else {
        slotReplyTo();
    }
}

void MainWindow::slotForwardAsAttachment()
{
#ifdef IMPRESARIO_HAVE_WEBENGINE
    const QModelIndex messageIndex = m_messageWidget->message();
    auto forwardNow = [&]() { m_msgActionHandler.forward(messageIndex, Composer::ForwardMode::FORWARD_AS_ATTACHMENT); };
    if (!actOnModel(qobject_cast<const Imap::Mailbox::Model *>(Imap::deproxifiedIndex(messageIndex).model()), forwardNow))
        forwardNow();
#endif
}

void MainWindow::slotResend()
{
    if (msgListWidget->tree->currentIndex().data(Imap::Mailbox::RoleIsGroupHeader).toBool())
        return;
    QModelIndex index;
    Imap::Mailbox::Model::realTreeItem(msgListWidget->tree->currentIndex(), nullptr, &index);
    if (!index.isValid())
        return;

    auto recipients = QList<QPair<Composer::RecipientKind,QString>>();
    for (const auto &kind: {Imap::Mailbox::RoleMessageTo, Imap::Mailbox::RoleMessageCc, Imap::Mailbox::RoleMessageBcc}) {
        const auto addresses = index.data(kind).toList();
        for (const auto &oneAddr : std::as_const(addresses)) {
            Q_ASSERT(oneAddr.typeId() == QMetaType::QStringList);
            QStringList item = oneAddr.toStringList();
            Q_ASSERT(item.size() == 4);
            Imap::Message::MailAddress a(item[0], item[1], item[2], item[3]);
            Composer::RecipientKind translatedKind = Composer::RecipientKind::ADDRESS_TO;
            switch (kind) {
            case Imap::Mailbox::RoleMessageTo:
                translatedKind = Composer::RecipientKind::ADDRESS_RESENT_TO;
                break;
            case Imap::Mailbox::RoleMessageCc:
                translatedKind = Composer::RecipientKind::ADDRESS_RESENT_CC;
                break;
            case Imap::Mailbox::RoleMessageBcc:
                translatedKind = Composer::RecipientKind::ADDRESS_RESENT_BCC;
                break;
            default:
                Q_ASSERT(false);
                break;
            }
            recipients.push_back({translatedKind, a.asPrettyString()});
        }
    }

    auto resendNow = [&]() {
        ComposeWidget::warnIfMsaNotConfigured(ComposeWidget::createFromReadOnly(this, index, recipients), this);
    };
    if (!actOnModel(qobject_cast<const Imap::Mailbox::Model *>(index.model()), resendNow))
        resendNow();

}

void MainWindow::slotComposeMailUrl(const QUrl &url)
{
    ComposeWidget::warnIfMsaNotConfigured(ComposeWidget::createFromUrl(this, url), this);
}

void MainWindow::composeMailFromOtherApplication(const QUrl &url, const QStringList &files)
{
    ComposeWidget *composer = url.isEmpty() && files.isEmpty()
        ? ComposeWidget::warnIfMsaNotConfigured(ComposeWidget::createBlank(this), this)
        : ComposeWidget::warnIfMsaNotConfigured(
              ComposeWidget::createFromUrl(this, url.isEmpty() ? QUrl(QStringLiteral("mailto:")) : url,
                                           ComposeWidget::UrlOrigin::OtherApplication, files),
              this);
    // The composer is the window the request is about, so it comes to the front rather than the main window
    if (composer)
        activateForRequest(composer);
}

void MainWindow::slotManageContact(const QUrl &url)
{
    Imap::Message::MailAddress addr;
    if (!Imap::Message::MailAddress::fromUrl(addr, url, QStringLiteral("x-trojita-manage-contact")))
        return;

    Plugins::AddressbookPlugin *addressbook = pluginManager()->addressbook();
    if (!addressbook)
        return;

    addressbook->openContactWindow(addr.mailbox + QLatin1Char('@') + addr.host, addr.name);
}

void MainWindow::slotAddContact(const QUrl &url)
{
    Imap::Message::MailAddress addr;
    if (!Imap::Message::MailAddress::fromUrl(addr, url, QStringLiteral("x-trojita-manage-contact")))
        return;

    Plugins::AddressbookPlugin *addressbook = pluginManager()->addressbook();
    if (!addressbook)
        return;

    addressbook->addContact(addr.mailbox + QLatin1Char('@') + addr.host, addr.name);
}

void MainWindow::invokeContactEditor()
{
    Plugins::AddressbookPlugin *addressbook = pluginManager()->addressbook();
    if (!addressbook)
        return;

    addressbook->openAddressbookWindow();
}

/** @short Create an MSAFactory as per the settings */
MSA::MSAFactory *MainWindow::msaFactory()
{
    using namespace Common;
    if (JmapSyncer::sendsThroughJmap(*accountSettings())) {
        QPointer<QSettings> profile = m_settings;
        // The server files the message in Sent; the next synchronization brings that copy here
        QPointer<JmapSyncer> syncer = m_jmapSyncers.value(m_accounts->activeAccount());
        return new Jmap::SubmissionFactory(
            Jmap::Settings::fromSettings(*accountSettings()),
            [profile](const QString &host, const QSslCertificate &certificate) {
                return profile && Common::TrustedCertificates::isTrusted(*profile, host, certificate);
            },
            [syncer]() {
                if (syncer)
                    syncer->sync(false);
            });
    }
    QString method = accountSettings()->value(SettingsNames::msaMethodKey).toString();
    MSA::MSAFactory *msaFactory = nullptr;
    if (method == SettingsNames::methodSMTP || method == SettingsNames::methodSSMTP) {
        auto smtpFactory = new MSA::SMTPFactory(accountSettings()->value(SettingsNames::smtpHostKey).toString(),
                                            accountSettings()->value(SettingsNames::smtpPortKey).toInt(),
                                            (method == SettingsNames::methodSSMTP),
                                            (method == SettingsNames::methodSMTP)
                                            && accountSettings()->value(SettingsNames::smtpStartTlsKey).toBool(),
                                            accountSettings()->value(SettingsNames::smtpAuthKey).toBool(),
                                            accountSettings()->value(SettingsNames::smtpAuthReuseImapCredsKey, false).toBool() ?
                                                accountSettings()->value(SettingsNames::imapUserKey).toString() :
                                                accountSettings()->value(SettingsNames::smtpUserKey).toString());
        smtpFactory->setPinnedPublicKey(accountSettings()->value(SettingsNames::smtpSslPemPubKey).toByteArray());
        if (OAuth::isEnabled(*accountSettings(), OAuth::Service::Smtp)) {
            QPointer<Plugins::PluginManager> plugins = pluginManager();
            smtpFactory->setOAuth(OAuth::AccountConfig::fromSettings(*accountSettings(), imapAccess()->credentialAccountId()),
                                  [plugins]() -> Plugins::PasswordPlugin * {
                                      return plugins ? plugins->password() : nullptr;
                                  });
        }
        msaFactory = smtpFactory;
    } else if (method == SettingsNames::methodSENDMAIL) {
        QStringList args = accountSettings()->value(SettingsNames::sendmailKey, SettingsNames::sendmailDefaultCmd).toString().split(QLatin1Char(' '));
        if (args.isEmpty()) {
            return nullptr;
        }
        QString appName = args.takeFirst();
        msaFactory = new MSA::SendmailFactory(appName, args);
    } else if (method == SettingsNames::methodImapSendmail) {
        if (!imapModel()->capabilities().contains(QStringLiteral("X-DRAFT-I01-SENDMAIL"))) {
            return nullptr;
        }
        msaFactory = new MSA::ImapSubmitFactory(qobject_cast<Imap::Mailbox::Model*>(imapAccess()->imapModel()));
    } else {
        return nullptr;
    }
    return msaFactory;
}

void MainWindow::slotMailboxDeleteFailed(const QString &mailbox, const QString &msg)
{
    Gui::Util::messageBoxWarning(this, tr("Can't delete mailbox"),
                                 tr("Deleting mailbox \"%1\" failed with the following message:\n%2").arg(mailbox, msg));
}

void MainWindow::slotMailboxCreateFailed(const QString &mailbox, const QString &msg)
{
    Gui::Util::messageBoxWarning(this, tr("Can't create mailbox"),
                                 tr("Creating mailbox \"%1\" failed with the following message:\n%2").arg(mailbox, msg));
}

void MainWindow::slotMailboxSyncFailed(const QString &mailbox, const QString &msg)
{
    // The activity log keeps the failure with a Retry for every account; see monitorAccount()
    const QString key = QLatin1String("folder-sync:") + m_accounts->activeAccount() + QLatin1Char(':') + mailbox;
    InlineNotificationBar::Action retry{i18nc("@action:button", "Try Again"), QStringLiteral("view-refresh"),
        QStringLiteral("retryFolderSync"), [this, key]() {
            if (m_mailAutomation)
                m_mailAutomation->activity()->retry(key);
        }};
    m_notificationBar->showError(i18nc("@info folder name, error", "Could not open %1: %2", mailbox, msg), {retry});
}

void MainWindow::slotMailboxRenameSucceeded(const QString &oldName, const QString &newName)
{
    m_notificationBar->showInfo(i18nc("@info", "Renamed %1 to %2", oldName, newName));
    {
        // The folder and those below it keep their arrangement and position
        const QString separator = imapModel()->mailboxIndexByName(newName).data(Imap::Mailbox::RoleMailboxSeparator).toString();
        if (m_viewStore.renameFolder(oldName, newName, separator))
            saveMessageListViews();
        if (folderPositions().renameFolder(oldName, newName, separator))
            saveFolderPositions();
        const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
        UiUtils::SettingsStore::renameFolder(*config, currentAccountId(), oldName, newName,
                                             separator.isEmpty() ? QChar() : separator.at(0));
        config->sync();
        const QChar tabSeparator = separator.isEmpty() ? QLatin1Char('/') : separator.at(0);
        if (m_tabsEnabled) {
            m_tabSession = UiUtils::MailTabs::withRenamedFolder(m_tabSession, currentAccountId(), oldName, newName, tabSeparator);
            refreshTabs();
        } else {
            auto state = KSharedConfig::openStateConfig();
            UiUtils::SettingsStore::setTabs(*state, UiUtils::MailTabs::withRenamedFolder(UiUtils::SettingsStore::tabs(*state),
                                                                                         currentAccountId(), oldName, newName,
                                                                                         tabSeparator));
        }
        if (m_positionMailbox == oldName)
            m_positionMailbox = newName;
    }
    const QString reopen = m_renamedCurrentMailbox.take(oldName);
    if (!reopen.isEmpty())
        goToMailbox(reopen);
}

void MainWindow::slotMailboxRenameFailed(const QString &oldName, const QString &newName, const QString &msg)
{
    m_renamedCurrentMailbox.remove(oldName);
    m_notificationBar->showError(i18nc("@info", "Could not rename %1 to %2: %3", oldName, newName, msg));
}

void MainWindow::slotMailboxEmptied(const QString &mailbox)
{
    m_notificationBar->showInfo(i18nc("@info", "Emptied %1", mailbox));
    reopenCurrentMailbox(mailbox);
}

void MainWindow::slotMailboxEmptyFailed(const QString &mailbox, const QString &msg)
{
    m_notificationBar->showError(i18nc("@info", "Could not empty %1: %2", mailbox, msg));
    reopenCurrentMailbox(mailbox);
}

void MainWindow::slotMailboxChanged(const QModelIndex &mailbox)
{
    using namespace Imap::Mailbox;
    QString mailboxName = mailbox.data(RoleMailboxName).toString();
    bool isSentMailbox = mailbox.isValid() && !mailboxName.isEmpty() &&
            accountSettings()->value(Common::SettingsNames::composerSaveToImapKey).toBool() &&
            mailboxName == accountSettings()->value(Common::SettingsNames::composerImapSentKey).toString();
    QTreeView *tree = msgListWidget->tree;

    // Automatically trigger visibility of the TO and FROM columns
    if (isSentMailbox) {
        if (tree->isColumnHidden(MsgListModel::TO) && !tree->isColumnHidden(MsgListModel::FROM)) {
            tree->hideColumn(MsgListModel::FROM);
            tree->showColumn(MsgListModel::TO);
        }
    } else {
        if (tree->isColumnHidden(MsgListModel::FROM) && !tree->isColumnHidden(MsgListModel::TO)) {
            tree->hideColumn(MsgListModel::TO);
            tree->showColumn(MsgListModel::FROM);
        }
    }

    updateMessageFlags();
    slotScrollToUnseenMessage();
}

void MainWindow::showConnectionStatus(uint parserId, Imap::ConnectionState state)
{
    // One connection closing while others stay open leaves the account connected
    Imap::Mailbox::ConnectionStates &states = m_connectionStates[m_accounts->activeAccount()];
    states.update(parserId, state);
    if ((state == Imap::CONN_STATE_NONE || state == Imap::CONN_STATE_LOGOUT) && states.count() > 0)
        state = states.furthest();
    const Imap::ConnectionState previousState = m_shownConnectionState;
    QString message = connectionStateToString(state);

    if (netOffline->isChecked())
        updateConnectionStatus(tr("Offline"));
    else if (state != Imap::CONN_STATE_NONE || !m_connectionError)
        updateConnectionStatus(message.isEmpty() ? i18nc("@info:status", "Not connected") : message);

    if (state == Imap::ConnectionState::CONN_STATE_SELECTED && previousState >= Imap::ConnectionState::CONN_STATE_SELECTED) {
        // prevent excessive popups when we "reset the state" to something which is shown quite often
        showStatusMessage(QString());
    } else {
        showStatusMessage(message);
    }
    m_shownConnectionState = state;
}

void MainWindow::slotShowLinkTarget(const QString &link)
{
    if (link.isEmpty()) {
        QToolTip::hideText();
    } else {
        QToolTip::showText(QCursor::pos(), tr("Link target: %1").arg(UiUtils::Formatting::htmlEscaped(link)));
    }
}

void MainWindow::slotShowAboutTrojita()
{
    Ui::AboutDialog ui;
    QDialog *widget = new QDialog(this);
    widget->setAttribute(Qt::WA_DeleteOnClose);
    ui.setupUi(widget);
    ui.versionLabel->setText(Common::Application::version);
    ui.qtVersion->setText(QStringLiteral("<a href=\"about-qt\">Qt " QT_VERSION_STR "</a>"));
    connect(ui.qtVersion, &QLabel::linkActivated, qApp, &QApplication::aboutQt);

    std::vector<std::pair<QString, bool>> features;
    features.emplace_back(tr("Plugins"),
#ifdef WITH_SHARED_PLUGINS
            true
#else
            false
#endif
                       );
    features.emplace_back(tr("Encrypted and signed messages"),
#ifdef IMPRESARIO_HAVE_CRYPTO_MESSAGES
            true
#else
            false
#endif
                       );
    features.emplace_back(tr("IMAP compression"),
#ifdef IMPRESARIO_HAVE_ZLIB
            true
#else
            false
#endif
                       );

    QString featuresText = QStringLiteral("<ul>");
    for (const auto &x: features) {
        featuresText += x.second ?
                    tr("<li>%1: supported</li>").arg(x.first)
                  : tr("<li>%1: <strong>disabled</strong></li>").arg(x.first);
    }
    featuresText += QStringLiteral("</ul>");
    ui.descriptionLabel->setText(ui.descriptionLabel->text() + featuresText);

    QStringList copyright;
    // Find the names of the authors and remove date codes from there
    QFile license(QStringLiteral(":/LICENSE"));
    if (license.open(QFile::ReadOnly)) {
        copyright = copyrightHolders(&license);
    } else {
        copyright << tr("Failed to open license file.");
    }

    ui.credits->setTextFormat(Qt::PlainText);
    ui.credits->setText(copyright.join(QStringLiteral("\n")));
    widget->show();
}

QStringList MainWindow::copyrightHolders(QFile *file) const
{
    QStringList ret;

    const auto copyrightHolders = QString::fromUtf8(file->readAll()).split(QLatin1Char('\n'));
    for (const auto &line : std::as_const(copyrightHolders)) {
        const QString prefix(QStringLiteral("Copyright (C) "));
        if (line.startsWith(prefix)) {
            const int pos = prefix.size();
            static const QRegularExpression re(QLatin1String("(\\d) - (\\d)"));
            ret << QChar(0xa9 /* COPYRIGHT SIGN */) + QLatin1Char(' ') +
                        line.mid(pos).replace(re,
                        QLatin1String("\\1") + QChar(0x2014 /* EM DASH */) + QLatin1String("\\2"));
        }
    }

    return ret;
}

void MainWindow::slotSaveCurrentMessageBody()
{
    Q_FOREACH(const QModelIndex &item, msgListWidget->tree->selectionModel()->selectedIndexes()) {
        Q_ASSERT(item.isValid());
        if (item.column() != 0)
            continue;
        if (! item.data(Imap::Mailbox::RoleMessageUid).isValid())
            continue;

        QModelIndex messageIndex = Imap::deproxifiedIndex(item);

        Imap::Network::MsgPartNetAccessManager *netAccess = new Imap::Network::MsgPartNetAccessManager(this);
        netAccess->setModelMessage(messageIndex);
        Imap::Network::FileDownloadManager *fileDownloadManager =
            new Imap::Network::FileDownloadManager(this, netAccess, messageIndex);
        connect(fileDownloadManager, &Imap::Network::FileDownloadManager::succeeded, this, [this, fileDownloadManager]() {
            slotFileSaved(fileDownloadManager->savedFileName());
        });
        connect(fileDownloadManager, &Imap::Network::FileDownloadManager::succeeded, fileDownloadManager, &QObject::deleteLater);
        connect(fileDownloadManager, &Imap::Network::FileDownloadManager::transferError, fileDownloadManager, &QObject::deleteLater);
        connect(fileDownloadManager, &Imap::Network::FileDownloadManager::fileNameRequested,
                this, &MainWindow::slotDownloadMessageFileNameRequested);
        connect(fileDownloadManager, &Imap::Network::FileDownloadManager::transferError,
                this, &MainWindow::slotDownloadTransferError);
        connect(fileDownloadManager, &QObject::destroyed, netAccess, &QObject::deleteLater);
        fileDownloadManager->downloadMessage();
    }
}

void MainWindow::slotDownloadTransferError(const QString &errorString)
{
    // Reported both when the data cannot be downloaded, for example offline, and when the file cannot be written
    Gui::Util::messageBoxCritical(this, i18nc("@title:window", "Could Not Save File"),
                                  i18nc("@info %1 is an error message", "The file could not be downloaded or saved.\n%1",
                                        errorString));
}

void MainWindow::slotDownloadMessageFileNameRequested(QString *fileName)
{
    *fileName = QFileDialog::getSaveFileName(this, tr("Save Message"), QDir(Util::savedFilesFolder()).filePath(Common::PrivateFiles::safeFileName(*fileName)),
                                             QString(), nullptr, QFileDialog::HideNameFilterDetails);
    Util::rememberSavedFile(*fileName);
}

void MainWindow::slotViewMsgSource()
{
#ifdef IMPRESARIO_HAVE_WEBENGINE

    Q_FOREACH(const QModelIndex &item, msgListWidget->tree->selectionModel()->selectedIndexes()) {
        Q_ASSERT(item.isValid());
        if (item.column() != 0)
            continue;
        if (! item.data(Imap::Mailbox::RoleMessageUid).isValid())
            continue;
        auto w = messageSourceWidget(item);
        //: Translators: %1 is the UID of a message (a number) and %2 is the name of a mailbox.
        w->setWindowTitle(tr("Message source of UID %1 in %2").arg(
                              QString::number(item.data(Imap::Mailbox::RoleMessageUid).toUInt()),
                              Imap::deproxifiedIndex(item).parent().parent().data(Imap::Mailbox::RoleMailboxName).toString()
                              ));
        w->show();
    }
#endif
}

QWidget *MainWindow::messageSourceWidget(const QModelIndex &message)
{
#ifdef IMPRESARIO_HAVE_WEBENGINE
    QModelIndex messageIndex = Imap::deproxifiedIndex(message);
    MessageSourceWidget *sourceWidget = new MessageSourceWidget(0, messageIndex);
    sourceWidget->setAttribute(Qt::WA_DeleteOnClose);
    QAction *close = new QAction(UiUtils::loadIcon(QStringLiteral("window-close")), tr("Close"), sourceWidget);
    sourceWidget->addAction(close);
    close->setShortcut(tr("Ctrl+W"));
    connect(close, &QAction::triggered, sourceWidget, &QWidget::close);
    return sourceWidget;
#endif
    return new QWidget(nullptr);
}

void MainWindow::slotViewMsgHeaders()
{
#ifdef IMPRESARIO_HAVE_WEBENGINE
    Q_FOREACH(const QModelIndex &item, msgListWidget->tree->selectionModel()->selectedIndexes()) {
        Q_ASSERT(item.isValid());
        if (item.column() != 0)
            continue;
        if (! item.data(Imap::Mailbox::RoleMessageUid).isValid())
            continue;
        QModelIndex messageIndex = Imap::deproxifiedIndex(item);

        auto widget = new MessageHeadersWidget(nullptr, messageIndex);
        widget->setAttribute(Qt::WA_DeleteOnClose);
        QAction *close = new QAction(UiUtils::loadIcon(QStringLiteral("window-close")), tr("Close"), widget);
        widget->addAction(close);
        close->setShortcut(tr("Ctrl+W"));
        connect(close, &QAction::triggered, widget, &QWidget::close);
        widget->show();
    }
#endif
}

void MainWindow::slotSubscribeCurrentMailbox()
{
    QModelIndex index = mboxTree->currentIndex();
    if (! index.isValid())
        return;

    QString mailbox = index.data(Imap::Mailbox::RoleMailboxName).toString();
    if (m_actionSubscribeMailbox->isChecked()) {
        imapModel()->subscribeMailbox(mailbox);
    } else {
        imapModel()->unsubscribeMailbox(mailbox);
    }
}

void MainWindow::slotShowOnlySubscribed()
{
    if (m_actionShowOnlySubscribed->isEnabled()) {
        m_settings->setValue(Common::SettingsNames::guiMailboxListShowOnlySubscribed, m_actionShowOnlySubscribed->isChecked());
        prettyMboxModel->setShowOnlySubscribed(m_actionShowOnlySubscribed->isChecked());
    }
}

void MainWindow::slotScrollToUnseenMessage()
{
    // The message which the user is at, or which was restored, stays in view
    const QModelIndex current = msgListWidget->tree->currentIndex();
    if (current.isValid()) {
        msgListWidget->tree->scrollTo(current);
        return;
    }
    // Otherwise the newest messages, which are at the end of the list unless it is sorted in descending order
    if (m_actionSortDescending->isChecked()) {
        msgListWidget->tree->scrollToTop();
    } else {
        msgListWidget->tree->scrollToBottom();
    }
}

void MainWindow::slotScrollToCurrent()
{
    // TODO: begs for lambda
    if (QScrollBar *vs = msgListWidget->tree->verticalScrollBar()) {
        vs->setValue(vs->maximum() - vs->value()); // implies vs->minimum() == 0
    }
}

void MainWindow::slotThreadMsgList()
{
    // The groups show a flat list; choosing the Conversation grouping brings the threads back
    if (showsUnifiedList() || Imap::Mailbox::GroupingMsgListModel::isRealGrouping(m_groupBy))
        return;

    // We want to save user's preferences and not override them with "threading disabled" when the server
    // doesn't report them, like in initial greetings. That's why we have to check for isEnabled() here.
    const bool useThreading = actionThreadMsgList->isChecked();

    // Switching between threaded/unthreaded view shall reset the sorting criteria. The goal is to make
    // sorting rather seldomly used as people shall instead use proper threading.
    if (useThreading) {
        m_actionSortThreading->setEnabled(true);
        if (!m_actionSortThreading->isChecked())
            m_actionSortThreading->trigger();
        m_actionSortNone->setEnabled(false);
    } else {
        m_actionSortNone->setEnabled(true);
        if (!m_actionSortNone->isChecked())
            m_actionSortNone->trigger();
        m_actionSortThreading->setEnabled(false);
    }

    QPersistentModelIndex currentItem = msgListWidget->tree->currentIndex();

    if (useThreading && actionThreadMsgList->isEnabled()) {
        msgListWidget->tree->setRootIsDecorated(true);
        qobject_cast<Imap::Mailbox::ThreadingMsgListModel *>(m_imapAccess->threadingMsgListModel())->setUserWantsThreading(true);
    } else {
        msgListWidget->tree->setRootIsDecorated(false);
        qobject_cast<Imap::Mailbox::ThreadingMsgListModel *>(m_imapAccess->threadingMsgListModel())->setUserWantsThreading(false);
    }

    if (currentItem.isValid()) {
        msgListWidget->tree->scrollTo(currentItem);
    } else {
        // If we cannot determine the current item, at least scroll to a predictable place. Without this, the view
        // would jump to "weird" places, probably due to some heuristics about trying to show "roughly the same"
        // objects as what was visible before the reshuffling.
        slotScrollToUnseenMessage();
    }
}

void MainWindow::slotSortingPreferenceChanged()
{
    Qt::SortOrder order = m_actionSortDescending->isChecked() ? Qt::DescendingOrder : Qt::AscendingOrder;

    using namespace Imap::Mailbox;

    // Sorted here rather than by the server, so the header keeps showing the previous sorting to go back to
    if (m_actionSortByRelevance && m_actionSortByRelevance->isChecked()) {
        if (showsUnifiedList()) {
            updateUnifiedRelevance();
            return;
        }
        const QStringList terms = m_folderSearchTerms;
        if (!terms.isEmpty()) {
            prettyMsgListModel->setRelevanceTerms(terms, order);
        } else {
            slotSortingConfirmed(msgListWidget->tree->header()->sortIndicatorSection(),
                                 msgListWidget->tree->header()->sortIndicatorOrder());
        }
        return;
    }
    if (prettyMsgListModel && prettyMsgListModel->isSortingByRelevance())
        prettyMsgListModel->setRelevanceTerms(QStringList(), order);
    const bool unifiedRelevance = m_prettyUnifiedModel && m_prettyUnifiedModel->isSortingByRelevance();
    if (unifiedRelevance)
        m_prettyUnifiedModel->setRelevanceTerms(QStringList(), order);

    int column = -1;
    if (m_actionSortByArrival->isChecked()) {
        column = MsgListModel::RECEIVED_DATE;
    } else if (m_actionSortByCc->isChecked()) {
        column = MsgListModel::CC;
    } else if (m_actionSortByDate->isChecked()) {
        column = MsgListModel::DATE;
    } else if (m_actionSortByFrom->isChecked()) {
        column = MsgListModel::FROM;
    } else if (m_actionSortBySize->isChecked()) {
        column = MsgListModel::SIZE;
    } else if (m_actionSortBySubject->isChecked()) {
        column = MsgListModel::SUBJECT;
    } else if (m_actionSortByTo->isChecked()) {
        column = MsgListModel::TO;
    } else {
        column = -1;
    }

    rememberMessageListSort(column, order);
    QHeaderView *header = msgListWidget->tree->header();
    // The header may show this sorting already, from before relevance, and would not sort again
    const bool sorted = header->sortIndicatorSection() == column && header->sortIndicatorOrder() == order;
    header->setSortIndicator(column, order);
    if (unifiedRelevance && sorted && showsUnifiedList())
        m_prettyUnifiedModel->sort(column, order);
}

void MainWindow::slotSortingConfirmed(int column, Qt::SortOrder order)
{
    // don't do anything during initialization
    if (!m_actionSortNone)
        return;

    using namespace Imap::Mailbox;
    QAction *action;

    switch (column) {
    case MsgListModel::SEEN:
    case MsgListModel::FLAGGED:
    case MsgListModel::ATTACHMENT:
    case MsgListModel::COLUMN_COUNT:
    case MsgListModel::BCC:
    case -1:
        if (actionThreadMsgList->isChecked() && !showsUnifiedList())
            action = m_actionSortThreading;
        else
            action = m_actionSortNone;
        break;
    case MsgListModel::SUBJECT:
        action = m_actionSortBySubject;
        break;
    case MsgListModel::FROM:
        action = m_actionSortByFrom;
        break;
    case MsgListModel::TO:
        action = m_actionSortByTo;
        break;
    case MsgListModel::CC:
        action = m_actionSortByCc;
        break;
    case MsgListModel::DATE:
        action = m_actionSortByDate;
        break;
    case MsgListModel::RECEIVED_DATE:
        action = m_actionSortByArrival;
        break;
    case MsgListModel::SIZE:
        action = m_actionSortBySize;
        break;
    default:
        action = m_actionSortNone;
    }

    action->setChecked(true);
    // The server sorts again, which ends sorting by relevance
    if (prettyMsgListModel && prettyMsgListModel->isSortingByRelevance())
        prettyMsgListModel->setRelevanceTerms(QStringList(), order);
    if (m_prettyUnifiedModel && m_prettyUnifiedModel->isSortingByRelevance() && showsUnifiedList()) {
        m_prettyUnifiedModel->setRelevanceTerms(QStringList(), order);
        m_prettyUnifiedModel->sort(column, order);
    }
    if (order == Qt::DescendingOrder)
        m_actionSortDescending->setChecked(true);
    else
        m_actionSortAscending->setChecked(true);
}

void MainWindow::slotSearchRequested(const QStringList &searchConditions)
{
    m_lastSearchConditions = searchConditions;
    // The Search Results panel lists the results of every search, so the message list keeps showing its folder
    const bool inPanel = !searchConditions.isEmpty() && wantsSearchInPanel();
    if (!searchConditions.isEmpty() && !inPanel)
        rememberUnnarrowedPosition();
    // Offline, the server cannot filter the folder, but the account's local index can answer like a search across
    const Imap::Mailbox::SearchIndexer *indexer = m_imapAccess ? m_imapAccess->searchIndexer() : nullptr;
    const bool searchIndexOffline = imapModel() && !imapModel()->isNetworkAvailable() && indexer && indexer->isAvailable();
    // The Unified Inbox has no single folder to filter, so its INBOXes are searched like several folders
    if (!searchConditions.isEmpty()
        && (inPanel || m_unifiedInbox || searchIndexOffline
            || msgListWidget->searchPlaces() != UiUtils::SearchScope::Places::CurrentFolder)) {
        startSearchAcross(searchConditions);
#ifdef IMPRESARIO_HAVE_WEBENGINE
        m_messageWidget->messageView->setHighlightTerm(UiUtils::searchHighlightTerm(searchConditions));
#endif
        return;
    }
    leaveSearchAcross();
    Imap::Mailbox::ThreadingMsgListModel * threadingMsgListModel =
            qobject_cast<Imap::Mailbox::ThreadingMsgListModel *>(m_imapAccess->threadingMsgListModel());
    threadingMsgListModel->setUserSearchingSortingPreference(searchConditions, threadingMsgListModel->currentSortCriterium(),
                                                             threadingMsgListModel->currentSortOrder());
    updateRelevanceSorting(searchConditions);
    scheduleMessageListStatus();
#ifdef IMPRESARIO_HAVE_WEBENGINE
    m_messageWidget->messageView->setHighlightTerm(UiUtils::searchHighlightTerm(searchConditions));
#endif
}

void MainWindow::updateRelevanceSorting(const QStringList &searchConditions)
{
    if (!m_actionSortByRelevance || !prettyMsgListModel)
        return;
    if (showsUnifiedList()) {
        // A unified list has its own relevance
        updateUnifiedRelevance();
        return;
    }
    const QStringList terms = UiUtils::relevanceTerms(searchConditions);
    m_folderSearchTerms = terms;
    m_actionSortByRelevance->setEnabled(!terms.isEmpty());
    m_actionSortByRelevance->setToolTip(
        terms.isEmpty() ? i18nc("@info:tooltip", "Relevance sorting needs an active quick search or Show Related filter")
                        : i18nc("@info:tooltip", "Show the messages which match the search best first"));
    if (!terms.isEmpty() && !m_actionSortByRelevance->isChecked() && remembersRelevanceSorting()) {
        // Best matches first, as relevance was chosen during the last search
        m_actionSortAscending->setChecked(true);
        m_actionSortByRelevance->setChecked(true);
    }
    const Qt::SortOrder order = m_actionSortDescending->isChecked() ? Qt::DescendingOrder : Qt::AscendingOrder;
    if (m_actionSortByRelevance->isChecked()) {
        if (!terms.isEmpty()) {
            if (prettyMsgListModel->relevanceTerms() != terms)
                prettyMsgListModel->setRelevanceTerms(terms, order);
        } else {
            // Back to the sorting from before, which the header still shows
            prettyMsgListModel->setRelevanceTerms(QStringList(), order);
            slotSortingConfirmed(msgListWidget->tree->header()->sortIndicatorSection(),
                                 msgListWidget->tree->header()->sortIndicatorOrder());
        }
    } else if (prettyMsgListModel->isSortingByRelevance()) {
        prettyMsgListModel->setRelevanceTerms(QStringList(), order);
    }
    updateSearchSnippets();
}

void MainWindow::updateUnifiedRelevance()
{
    if (!m_actionSortByRelevance || !m_prettyUnifiedModel)
        return;
    QHeaderView *header = msgListWidget->tree->header();
    if (!showsUnifiedList()) {
        if (m_prettyUnifiedModel->isSortingByRelevance())
            m_prettyUnifiedModel->setRelevanceTerms(QStringList(), header->sortIndicatorOrder());
        updateSearchSnippets();
        return;
    }
    const QStringList terms = searchShownInList() ? UiUtils::relevanceTerms(m_searchConditions) : QStringList();
    m_actionSortByRelevance->setEnabled(!terms.isEmpty());
    m_actionSortByRelevance->setToolTip(
        terms.isEmpty() ? i18nc("@info:tooltip", "Relevance sorting needs an active quick search or Show Related filter")
                        : i18nc("@info:tooltip", "Show the messages which match the search best first"));
    if (!terms.isEmpty() && !m_actionSortByRelevance->isChecked() && remembersRelevanceSorting()) {
        m_actionSortAscending->setChecked(true);
        m_actionSortByRelevance->setChecked(true);
    }
    const Qt::SortOrder order = m_actionSortDescending->isChecked() ? Qt::DescendingOrder : Qt::AscendingOrder;
    if (m_actionSortByRelevance->isChecked() && !terms.isEmpty()) {
        if (m_prettyUnifiedModel->relevanceTerms() != terms || m_prettyUnifiedModel->sortOrder() != order)
            m_prettyUnifiedModel->setRelevanceTerms(terms, order);
    } else {
        if (m_prettyUnifiedModel->isSortingByRelevance()) {
            // Back to the sorting from before, which the header still shows
            m_prettyUnifiedModel->setRelevanceTerms(QStringList(), header->sortIndicatorOrder());
            m_prettyUnifiedModel->sort(header->sortIndicatorSection(), header->sortIndicatorOrder());
        }
        if (m_actionSortByRelevance->isChecked())
            slotSortingConfirmed(header->sortIndicatorSection(), header->sortIndicatorOrder());
    }
    updateSearchSnippets();
}

void MainWindow::updateSearchSnippets()
{
    if (!prettyMsgListModel)
        return;
    Imap::Mailbox::SearchSnippets *snippets = prettyMsgListModel->snippets();
    snippets->setModel(imapModel());
    const QStringList terms = currentSearchTerms();
    // The results of a search across have snippets of their own
    snippets->setTerms(showsUnifiedList() ? QStringList() : terms);
    msgListWidget->tree->setSearchSnippets(!terms.isEmpty());
}

QStringList MainWindow::currentSearchTerms() const
{
    if (m_searchAcross)
        return m_searchInPanel ? QStringList() : UiUtils::relevanceTerms(m_searchConditions);
    if (m_unifiedInbox)
        return QStringList();
    return m_folderSearchTerms;
}

bool MainWindow::remembersRelevanceSorting() const
{
    return KConfigGroup(KSharedConfig::openStateConfig(), QStringLiteral("SearchResults")).readEntry("SortByRelevance", false);
}

void MainWindow::rememberRelevanceSorting(bool relevance)
{
    KConfigGroup group(KSharedConfig::openStateConfig(), QStringLiteral("SearchResults"));
    if (group.readEntry("SortByRelevance", false) == relevance)
        return;
    group.writeEntry("SortByRelevance", relevance);
    group.sync();
}

QModelIndex MainWindow::toPrettyIndex(const QModelIndex &viewIndex) const
{
    if (viewIndex.isValid() && m_groupingMsgListModel && viewIndex.model() == m_groupingMsgListModel)
        return m_groupingMsgListModel->mapToSource(viewIndex);
    return viewIndex;
}

QModelIndex MainWindow::fromPrettyIndex(const QModelIndex &prettyIndex) const
{
    if (prettyIndex.isValid() && prettyIndex.model() == prettyMsgListModel && m_groupingMsgListModel &&
        msgListWidget->tree->model() == m_groupingMsgListModel)
        return m_groupingMsgListModel->mapFromSource(prettyIndex);
    return prettyIndex;
}

void MainWindow::connectMessageListSelection()
{
    QItemSelectionModel *selection = msgListWidget->tree->selectionModel();
    if (!selection || !prettyMsgListModel)
        return;
    connect(selection, &QItemSelectionModel::selectionChanged, this, &MainWindow::updateMessageFlags);
    connect(selection, &QItemSelectionModel::selectionChanged, this, &MainWindow::scheduleMessageListStatus);
    connect(selection, &QItemSelectionModel::selectionChanged, this, [this]() { captureFolderPosition(); });
    connect(selection, &QItemSelectionModel::currentChanged, this, [this]() { captureFolderPosition(); });
    if (m_prettyUnifiedModel && msgListWidget->tree->model() == m_prettyUnifiedModel) {
        connect(selection, &QItemSelectionModel::currentChanged, m_prettyUnifiedModel, [this, model = m_prettyUnifiedModel]() {
            QMetaObject::invokeMethod(model, [this, model]() {
                const QModelIndex current = msgListWidget->tree->currentIndex();
                if (current.model() == model)
                    model->setKeptVisibleIndex(current);
            }, Qt::QueuedConnection);
            scheduleMessageListStatus();
        });
        return;
    }
    connect(selection, &QItemSelectionModel::currentChanged, prettyMsgListModel, [this, model = prettyMsgListModel]() {
        // Later, because the current message also changes while the models are busy removing rows
        QMetaObject::invokeMethod(model, [this, model]() {
            const QModelIndex current = msgListWidget->tree->currentIndex();
            // On the header of a group, the message from before stays shown until another one takes its place
            if (!current.data(Imap::Mailbox::RoleIsGroupHeader).toBool() && toPrettyIndex(current).model() == model)
                model->setKeptVisibleIndex(toPrettyIndex(current));
        }, Qt::QueuedConnection);
        scheduleMessageListStatus();
    });
}

void MainWindow::swapMessageListModel(QAbstractItemModel *model)
{
    MsgListView *tree = msgListWidget->tree;
    const QPersistentModelIndex current = toPrettyIndex(tree->currentIndex());
    QList<QPersistentModelIndex> selected;
    QItemSelectionModel *oldSelection = tree->selectionModel();
    if (oldSelection) {
        const QModelIndexList rows = oldSelection->selectedRows(0);
        for (const QModelIndex &index : rows) {
            const QModelIndex pretty = toPrettyIndex(index);
            if (pretty.isValid())
                selected << pretty;
        }
    }
    const bool relevance = !showsUnifiedList() && m_actionSortByRelevance && m_actionSortByRelevance->isChecked();

    tree->setModel(model);
    if (oldSelection && oldSelection != tree->selectionModel())
        oldSelection->deleteLater();
    connectMessageListSelection();
    if (relevance && !m_actionSortByRelevance->isChecked()) {
        // Setting the model sorts it again by the header's column, which ended sorting by relevance
        m_actionSortByRelevance->setChecked(true);
        slotSortingPreferenceChanged();
    }

    QItemSelection selection;
    for (const QPersistentModelIndex &index : std::as_const(selected)) {
        const QModelIndex row = fromPrettyIndex(index);
        // The messages of the Unified Inbox and of a folder are not the same rows
        if (row.isValid() && row.model() == model)
            selection.select(row, row.siblingAtColumn(model->columnCount(row.parent()) - 1));
    }
    if (!selection.isEmpty())
        tree->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
    const QModelIndex newCurrent = fromPrettyIndex(current);
    if (newCurrent.isValid() && newCurrent.model() == model) {
        tree->selectionModel()->setCurrentIndex(newCurrent, QItemSelectionModel::NoUpdate);
        tree->scrollTo(newCurrent);
    }
}

void MainWindow::setMessageListGrouping(Imap::Mailbox::GroupingMsgListModel::Grouping grouping, bool remember)
{
    using Imap::Mailbox::GroupingMsgListModel;
    if (showsUnifiedList() || !m_groupingMsgListModel || !m_groupByActions)
        return;
    MsgListView *tree = msgListWidget->tree;
    auto threadingModel = qobject_cast<Imap::Mailbox::ThreadingMsgListModel *>(m_imapAccess->threadingMsgListModel());
    const bool shown = tree->model() == m_groupingMsgListModel;
    const bool wasGrouped = GroupingMsgListModel::isRealGrouping(m_groupBy) || shown;
    if (grouping != m_groupBy)
        m_collapsedGroups.clear();
    m_groupBy = grouping;

    if (GroupingMsgListModel::isRealGrouping(grouping)) {
        // The groups show a flat list, so that the sorting applies within each group
        {
            const QSignalBlocker blocker(actionThreadMsgList);
            actionThreadMsgList->setChecked(false);
        }
        threadingModel->setUserWantsThreading(false);
        m_actionSortNone->setEnabled(true);
        if (m_actionSortThreading->isChecked())
            m_actionSortNone->trigger();
        m_actionSortThreading->setEnabled(false);
        m_groupingMsgListModel->setGroupOrder(tree->header()->sortIndicatorOrder());
        m_groupingMsgListModel->setGrouping(grouping);
        if (!shown) {
            m_groupingMsgListModel->setSourceModel(prettyMsgListModel);
            swapMessageListModel(m_groupingMsgListModel);
        }
        tree->setRootIsDecorated(true);
        applyGroupExpansion();
    } else {
        if (shown) {
            // First, so that the view finds the PrettyMsgListModel through the groups to let go of it
            swapMessageListModel(prettyMsgListModel);
            m_groupingMsgListModel->setSourceModel(nullptr);
        }
        const bool threaded = grouping == GroupingMsgListModel::Grouping::Conversation;
        const bool wasThreaded = actionThreadMsgList->isChecked();
        {
            const QSignalBlocker blocker(actionThreadMsgList);
            actionThreadMsgList->setChecked(threaded);
        }
        if (wasGrouped || wasThreaded != threaded) {
            if (threaded || !wasGrouped) {
                slotThreadMsgList();
            } else {
                // A flat list again, keeping the sorting of the groups
                m_actionSortNone->setEnabled(true);
                m_actionSortThreading->setEnabled(false);
                tree->setRootIsDecorated(false);
                threadingModel->setUserWantsThreading(false);
            }
        }
    }
    syncGroupByActions();
    if (remember)
        rememberMessageListView();
}

Imap::Mailbox::GroupingMsgListModel::Grouping MainWindow::shownGrouping() const
{
    using Imap::Mailbox::GroupingMsgListModel;
    if (GroupingMsgListModel::isRealGrouping(m_groupBy))
        return m_groupBy;
    return actionThreadMsgList && actionThreadMsgList->isChecked() ? GroupingMsgListModel::Grouping::Conversation
                                                                   : GroupingMsgListModel::Grouping::None;
}

void MainWindow::applyMessageListView()
{
    // The Unified Inbox has a view of its own, see setUnifiedInbox()
    if (showsUnifiedList())
        return;
    using Imap::Mailbox::GroupingMsgListModel;
    const QString mailbox = currentMailboxName();
    m_view = m_viewStore.effectiveView(mailbox);
    m_viewMailbox = mailbox;
    if (!m_groupByActions || !m_groupingMsgListModel || !prettyMsgListModel)
        return;
    GroupingMsgListModel::Grouping grouping = GroupingMsgListModel::groupingFromName(m_view.groupBy);
    if (!GroupingMsgListModel::isRealGrouping(grouping))
        grouping = m_view.threading ? GroupingMsgListModel::Grouping::Conversation : GroupingMsgListModel::Grouping::None;
    ++m_applyingView;
    if (grouping != shownGrouping())
        setMessageListGrouping(grouping, false);
    else
        syncGroupByActions();
    --m_applyingView;
    applyMessageListSort();
    updateMessageListViewActions();
}

void MainWindow::applyMessageListSort()
{
    if (showsUnifiedList() || !m_actionSortNone || !prettyMsgListModel || !msgListWidget->tree->model())
        return;
    MsgListView *tree = msgListWidget->tree;
    QHeaderView *header = tree->header();
    const int column = m_view.sortColumn;
    const Qt::SortOrder order = m_view.sortOrder;
    const bool relevance = m_actionSortByRelevance && m_actionSortByRelevance->isChecked();
    const QAbstractItemModel *threads = prettyMsgListModel->sourceModel();
    ++m_applyingView;
    if (relevance || !threads || !threads->rowCount()) {
        // An empty list refuses to be sorted; the header keeps the sorting to apply later, or to go back to after relevance
        {
            const QSignalBlocker blocker(header);
            header->setSortIndicator(column, order);
        }
        header->viewport()->update();
        if (!relevance) {
            slotSortingConfirmed(column, order);
            if (tree->model() == m_groupingMsgListModel)
                m_groupingMsgListModel->setGroupOrder(order);
        }
    } else if (header->sortIndicatorSection() != column || header->sortIndicatorOrder() != order) {
        // The view sorts its model
        header->setSortIndicator(column, order);
    } else {
        tree->model()->sort(column, order);
    }
    --m_applyingView;
    updateSortButton();
}

void MainWindow::rememberMessageListView()
{
    using Imap::Mailbox::GroupingMsgListModel;
    if (showsUnifiedList() || m_applyingView || !actionThreadMsgList)
        return;
    const bool grouped = GroupingMsgListModel::isRealGrouping(m_groupBy);
    m_view.groupBy = grouped ? GroupingMsgListModel::groupingName(m_groupBy) : QString();
    m_view.threading = !grouped && actionThreadMsgList->isChecked();
    storeMessageListView();
}

void MainWindow::rememberMessageListSort(int column, Qt::SortOrder order)
{
    if (showsUnifiedList() || m_applyingView)
        return;
    m_view.sortColumn = qMax(-1, column);
    m_view.sortOrder = order;
    storeMessageListView();
}

void MainWindow::storeMessageListView()
{
    const QString mailbox = currentMailboxName();
    // Without an open folder, the choice is the account's default
    if (mailbox.isEmpty())
        m_viewStore.setAccountDefault(m_view);
    else
        m_viewStore.setFolderView(mailbox, m_view);
    m_viewMailbox = mailbox;
    saveMessageListViews();
    updateMessageListViewActions();
}

void MainWindow::saveMessageListViews()
{
    m_viewStore.save(*accountSettings());
}

void MainWindow::applyMessageListViewToAllFolders()
{
    const QString mailbox = currentMailboxName();
    if (mailbox.isEmpty())
        return;
    const QString account = m_accounts->displayName(m_accounts->activeAccount());
    QMessageBox box(QMessageBox::Question, i18nc("@title:window", "Use This View for All Folders"),
                    i18nc("@info", "Sort, thread and group all folders of %1 like %2? Folders which have their own view "
                                   "will follow it as well.", account, mailbox),
                    QMessageBox::NoButton, this);
    box.setObjectName(QStringLiteral("messageListViewForAllFoldersConfirmation"));
    QPushButton *apply = box.addButton(i18nc("@action:button", "Use for All Folders"), QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(apply);
    box.exec();
    if (box.clickedButton() != apply)
        return;
    m_viewStore.applyToAllFolders(m_view);
    saveMessageListViews();
    updateMessageListViewActions();
}

void MainWindow::resetMessageListViewForFolder()
{
    const QString mailbox = currentMailboxName();
    if (mailbox.isEmpty() || !m_viewStore.resetFolder(mailbox))
        return;
    saveMessageListViews();
    applyMessageListView();
}

void MainWindow::updateMessageListViewActions()
{
    if (!m_actionResetFolderView || !m_actionViewForAllFolders)
        return;
    if (showsUnifiedList()) {
        m_actionViewForAllFolders->setEnabled(false);
        m_actionResetFolderView->setEnabled(false);
        return;
    }
    const QString mailbox = currentMailboxName();
    m_actionViewForAllFolders->setEnabled(!mailbox.isEmpty());
    m_actionResetFolderView->setEnabled(!mailbox.isEmpty() &&
                                        m_viewStore.scope(mailbox) == UiUtils::ViewStateStore::Scope::Folder);
}

void MainWindow::pruneFolderStates()
{
    UiUtils::FolderPositionStore &positions = folderPositions();
    if (m_viewStore.folders().isEmpty() && positions.folders().isEmpty())
        return;
    // The unfiltered folder tree, so that unsubscribed folders do not look deleted
    const UiUtils::RecentFolders::StateLookup state = UiUtils::RecentFolders::stateFromModel(m_imapAccess->mailboxModel());
    if (m_viewStore.prune(state))
        saveMessageListViews();
    if (positions.prune(state))
        saveFolderPositions();
}

namespace {
const QString folderPositionsGroup = QStringLiteral("MessageListPositions");
}

UiUtils::FolderPositionStore &MainWindow::folderPositions()
{
    const QString account = m_imapAccess->credentialAccountId();
    auto it = m_folderPositions.find(account);
    if (it == m_folderPositions.end()) {
        it = m_folderPositions.insert(account, UiUtils::FolderPositionStore());
        it->loadJson(KConfigGroup(KSharedConfig::openStateConfig(), folderPositionsGroup).readEntry(account, QString()));
    }
    return *it;
}

void MainWindow::captureFolderPosition(bool anchorOnly)
{
    using namespace Imap::Mailbox;
    if (showsUnifiedList() || m_positionMailbox.isEmpty() || !prettyMsgListModel || m_positionMailbox != currentMailboxName())
        return;
    // Until the remembered position is back, the list is not where the user left it
    if (m_messageRestorer && m_messageRestorer->isPending())
        return;
    MsgListView *tree = msgListWidget->tree;
    UiUtils::FolderPosition position = anchorOnly ? m_livePosition : UiUtils::FolderPosition();
    const auto *msgList = qobject_cast<MsgListModel *>(m_imapAccess->msgListModel());
    position.uidValidity = msgList->currentMailbox().data(RoleMailboxUidValidity).toUInt();
    position.anchorUid = 0;
    const int height = tree->viewport()->height();
    for (QModelIndex index = tree->indexAt(QPoint(0, 0)); index.isValid() && tree->visualRect(index).top() < height;
         index = tree->indexBelow(index)) {
        // The header of a group is not a message
        if (const uint uid = index.data(RoleMessageUid).toUInt()) {
            position.anchorUid = uid;
            break;
        }
    }
    if (!anchorOnly) {
        const QModelIndex current = tree->currentIndex();
        position.uid = current.data(RoleIsGroupHeader).toBool() ? 0 : current.data(RoleMessageUid).toUInt();
        if (QItemSelectionModel *selection = tree->selectionModel()) {
            const QModelIndexList rows = selection->selectedRows(0);
            if (rows.size() <= UiUtils::FolderPosition::MaxSelectedUids) {
                for (const QModelIndex &row : rows) {
                    const uint uid = row.data(RoleMessageUid).toUInt();
                    if (uid && uid != position.uid)
                        position.selectedUids << uid;
                }
            }
        }
    }
    // An emptied list says nothing about where the user was
    if (position.isValid())
        m_livePosition = position;
}

void MainWindow::commitFolderPosition()
{
    if (m_positionMailbox.isEmpty() || !m_livePosition.isValid())
        return;
    folderPositions().setPosition(m_positionMailbox, m_livePosition);
    saveFolderPositions();
}

void MainWindow::restoreFolderPosition()
{
    if (showsUnifiedList() || !m_messageRestorer || m_positionMailbox.isEmpty() || m_positionMailbox != currentMailboxName())
        return;
    if (m_pendingReveal.accountId == m_accounts->activeAccount() && m_pendingReveal.mailbox == m_positionMailbox) {
        // The message which revealInFolder() asked for, rather than the one selected before
        const PendingReveal reveal = std::exchange(m_pendingReveal, PendingReveal());
        m_activeReveal = reveal;
        m_messageRestorer->restore(reveal.mailbox, reveal.uidValidity, reveal.uid);
        return;
    }
    // Switching accounts queues a restore of the position, which would replace the revealed message still being selected
    if (isRevealing(m_positionMailbox))
        return;
    m_activeReveal = PendingReveal();
    const auto position = folderPositions().position(m_positionMailbox);
    if (!position)
        return;
    m_messageRestorer->restorePosition(m_positionMailbox, position->uidValidity, position->uid, position->anchorUid,
                                       position->selectedUids);
}

bool MainWindow::isRevealing(const QString &mailbox) const
{
    const QString accountId = m_accounts->activeAccount();
    if (m_pendingReveal.accountId == accountId && m_pendingReveal.mailbox == mailbox)
        return true;
    return m_messageRestorer && m_messageRestorer->isPending() && m_activeReveal.accountId == accountId
        && m_activeReveal.mailbox == mailbox;
}

void MainWindow::saveFolderPositions()
{
    KConfigGroup positions(KSharedConfig::openStateConfig(), folderPositionsGroup);
    for (auto it = m_folderPositions.cbegin(); it != m_folderPositions.cend(); ++it) {
        if (it->folders().isEmpty())
            positions.deleteEntry(it.key());
        else
            positions.writeEntry(it.key(), it->toJson());
    }
    if (!positions.sync())
        qWarning() << "Could not save the positions in the message lists";
}

void MainWindow::updateMessageListScope()
{
    if (!m_imapAccess || !msgListWidget)
        return;
    {
        const QString accountId = m_accounts->activeAccount();
        msgListWidget->setFacetContext({accountId, m_accounts->displayName(accountId), QDate()});
        const QString scope = m_searchAcross                      ? QStringLiteral("search")
                              : (m_unifiedInbox && m_unifiedModel) ? QStringLiteral("unified")
                                                                   : accountId + QLatin1Char('\n') + currentMailboxName();
        // The facets were chosen among the messages of another list
        if (std::exchange(m_facetScope, scope) != scope)
            msgListWidget->clearFacets();
    }
    if (searchShownInList()) {
        msgListWidget->setCustomScope(i18nc("@label", "Search Results"),
                                      i18nc("@info:tooltip list of accounts and folders, one per line", "Searched in:\n%1",
                                            m_searchedPlaces.join(QLatin1Char('\n'))));
        return;
    }
    if (m_unifiedInbox && m_unifiedModel) {
        using State = Imap::Mailbox::UnifiedMsgListModel::AccountState;
        QStringList accounts;
        for (const QString &accountId : m_unifiedModel->accountIds()) {
            const QString name = m_unifiedModel->accountName(accountId);
            switch (m_unifiedModel->accountState(accountId)) {
            case State::Ready:
                accounts << name;
                break;
            case State::Syncing:
                accounts << i18nc("@info:tooltip account name", "%1 (synchronizing)", name);
                break;
            case State::Offline:
                accounts << i18nc("@info:tooltip account name", "%1 (offline)", name);
                break;
            case State::Failed:
                accounts << i18nc("@info:tooltip account name, error", "%1 (failed: %2)", name,
                                  m_unifiedModel->accountStateDetail(accountId));
                break;
            case State::Unavailable:
                accounts << i18nc("@info:tooltip account name", "%1 (unavailable)", name);
                break;
            }
        }
        msgListWidget->setCustomScope(i18nc("@label", "Unified Inbox"),
                                      i18nc("@info:tooltip list of accounts, one per line", "The INBOX of these accounts:\n%1",
                                            accounts.join(QLatin1Char('\n'))));
        return;
    }
    const auto *msgList = qobject_cast<Imap::Mailbox::MsgListModel *>(m_imapAccess->msgListModel());
    const QModelIndex mailbox = msgList ? msgList->currentMailbox() : QModelIndex();
    msgListWidget->setScope(m_accounts->displayName(m_accounts->activeAccount()),
                            mailbox.data(Imap::Mailbox::RoleShortMailboxName).toString(),
                            mailbox.data(Imap::Mailbox::RoleMailboxName).toString());
}

void MainWindow::syncGroupByActions()
{
    using Imap::Mailbox::GroupingMsgListModel;
    if (!m_groupByActions)
        return;
    const bool grouped = GroupingMsgListModel::isRealGrouping(m_groupBy);
    GroupingMsgListModel::Grouping shown = m_groupBy;
    if (!grouped) {
        shown = actionThreadMsgList->isChecked() ? GroupingMsgListModel::Grouping::Conversation
                                                 : GroupingMsgListModel::Grouping::None;
    }
    const QList<QAction *> actions = m_groupByActions->actions();
    for (QAction *action : actions) {
        const auto grouping = static_cast<GroupingMsgListModel::Grouping>(action->data().toInt());
        if (grouping == GroupingMsgListModel::Grouping::Conversation)
            action->setEnabled(actionThreadMsgList->isEnabled());
        if (grouping == shown && !action->isChecked())
            action->setChecked(true);
    }
    m_actionExpandAllGroups->setEnabled(grouped);
    m_actionCollapseAllGroups->setEnabled(grouped);
    m_expandAllConversations->setEnabled(showsThreads());
    m_collapseAllConversations->setEnabled(showsThreads());
}

bool MainWindow::showsThreads() const
{
    return !showsUnifiedList() && !Imap::Mailbox::GroupingMsgListModel::isRealGrouping(m_groupBy) && actionThreadMsgList
        && actionThreadMsgList->isEnabled() && actionThreadMsgList->isChecked();
}

void MainWindow::applyGroupExpansion()
{
    if (!m_groupingMsgListModel || msgListWidget->tree->model() != m_groupingMsgListModel)
        return;
    for (int row = 0, rows = m_groupingMsgListModel->rowCount(); row < rows; ++row) {
        const QModelIndex header = m_groupingMsgListModel->index(row, 0);
        msgListWidget->tree->setExpanded(header, !m_collapsedGroups.contains(m_groupingMsgListModel->groupKey(header)));
    }
}

void MainWindow::slotHideRead()
{
    if (actionHideRead->isChecked())
        rememberUnnarrowedPosition();
    prettyMsgListModel->setHideRead(actionHideRead->isChecked());
    saveMessageListFilters();
}

void MainWindow::applyMessageListFilters()
{
    using Common::SettingsNames;
    using Imap::Mailbox::PrettyMsgListModel;
    const QSettings *account = accountSettings();
    // Before the filters were kept per account, hiding read messages was a global setting
    const QVariant hideRead = m_settings->value(SettingsNames::guiMsgListHideRead, false);
    PrettyMsgListModel::Filters filters;
    filters.setFlag(PrettyMsgListModel::FilterUnread, account->value(SettingsNames::guiMsgListFilterUnread, hideRead).toBool());
    filters.setFlag(PrettyMsgListModel::FilterFlagged, account->value(SettingsNames::guiMsgListFilterFlagged, false).toBool());
    filters.setFlag(PrettyMsgListModel::FilterAttachments,
                    account->value(SettingsNames::guiMsgListFilterAttachments, false).toBool());
    prettyMsgListModel->setFilters(filters);
    syncMessageListFilterUi();
}

void MainWindow::saveMessageListFilters()
{
    using Common::SettingsNames;
    using Imap::Mailbox::PrettyMsgListModel;
    const PrettyMsgListModel::Filters filters = prettyMsgListModel->filters();
    QSettings *account = accountSettings();
    account->setValue(SettingsNames::guiMsgListFilterUnread, filters.testFlag(PrettyMsgListModel::FilterUnread));
    account->setValue(SettingsNames::guiMsgListFilterFlagged, filters.testFlag(PrettyMsgListModel::FilterFlagged));
    account->setValue(SettingsNames::guiMsgListFilterAttachments, filters.testFlag(PrettyMsgListModel::FilterAttachments));
}

void MainWindow::syncMessageListFilterUi()
{
    // The actions do not exist yet when the models are set up for the first time
    if (!m_actionFilterUnread || !prettyMsgListModel)
        return;
    using Imap::Mailbox::PrettyMsgListModel;
    const PrettyMsgListModel::Filters filters = prettyMsgListModel->filters();
    // The filters of the folder apply to the Unified Inbox as well
    if (m_prettyUnifiedModel)
        m_prettyUnifiedModel->setFilters(filters);
    if (m_searchResultsPanel)
        m_searchResultsPanel->prettyResults()->setFilters(filters);
    actionHideRead->setChecked(filters.testFlag(PrettyMsgListModel::FilterUnread));
    m_actionFilterUnread->setChecked(filters.testFlag(PrettyMsgListModel::FilterUnread));
    m_actionFilterFlagged->setChecked(filters.testFlag(PrettyMsgListModel::FilterFlagged));
    m_actionFilterAttachments->setChecked(filters.testFlag(PrettyMsgListModel::FilterAttachments));
    const bool active = filters != PrettyMsgListModel::NoFilter;
    m_actionClearFilters->setEnabled(active);
    m_clearFiltersButton->setVisible(active);
    QStringList names;
    for (const QAction *action : {m_actionFilterUnread, m_actionFilterFlagged, m_actionFilterAttachments}) {
        if (action->isChecked())
            names << KLocalizedString::removeAcceleratorMarker(action->text());
    }
    msgListWidget->setActiveFilters(names);
    scheduleMessageListStatus();
}

void MainWindow::applySearchFacets()
{
    const UiUtils::SearchFacets facets = msgListWidget->facets();
    // The facets narrow the results of the search, wherever they are listed
    const bool inPanel = m_searchAcross && m_searchInPanel;
    const UiUtils::SearchFacets listFacets = inPanel ? UiUtils::SearchFacets() : facets;
    if (prettyMsgListModel)
        prettyMsgListModel->setFacets(listFacets);
    if (m_prettyUnifiedModel)
        m_prettyUnifiedModel->setFacets(listFacets);
    if (m_searchResultsPanel)
        m_searchResultsPanel->prettyResults()->setFacets(inPanel ? facets : UiUtils::SearchFacets());
    scheduleMessageListStatus();
}

bool MainWindow::messageListNarrowed() const
{
    if (searchShownInList() || (prettyMsgListModel && prettyMsgListModel->filters() != Imap::Mailbox::PrettyMsgListModel::NoFilter))
        return true;
    const auto *threading = m_imapAccess ? qobject_cast<Imap::Mailbox::ThreadingMsgListModel *>(m_imapAccess->threadingMsgListModel())
                                         : nullptr;
    return threading && !threading->currentSearchCondition().isEmpty();
}

void MainWindow::rememberUnnarrowedPosition()
{
    if (messageListNarrowed() || showsUnifiedList())
        return;
    captureFolderPosition();
    m_unnarrowedMailbox = m_positionMailbox;
    m_unnarrowedPosition = m_livePosition;
    m_pickedWhileNarrowed = false;
}

void MainWindow::clearSearchAndFilters()
{
    using Imap::Mailbox::PrettyMsgListModel;
    // A search in the Search Results panel does not narrow the list, but is cleared as well
    if (!prettyMsgListModel || (!messageListNarrowed() && !m_searchAcross))
        return;
    const QString mailbox = std::exchange(m_unnarrowedMailbox, QString());
    const UiUtils::FolderPosition before = std::exchange(m_unnarrowedPosition, UiUtils::FolderPosition());
    // A message which the user picked among the results stays current, now among all messages of the folder
    const bool picked = std::exchange(m_pickedWhileNarrowed, false);
    if (picked)
        captureFolderPosition();
    const UiUtils::FolderPosition narrowed = picked && !showsUnifiedList() ? m_livePosition : UiUtils::FolderPosition();
    prettyMsgListModel->setFilters(PrettyMsgListModel::NoFilter);
    saveMessageListFilters();
    msgListWidget->clearSearch();
    const QString shown = currentMailboxName();
    if (showsUnifiedList() || !m_messageRestorer || shown.isEmpty() || shown != m_positionMailbox)
        return;
    if (narrowed.uid && narrowed.uidValidity) {
        m_messageRestorer->restorePosition(shown, narrowed.uidValidity, narrowed.uid, 0, narrowed.selectedUids);
    } else if (before.isValid() && mailbox == shown) {
        m_messageRestorer->restorePosition(shown, before.uidValidity, before.uid, before.anchorUid, before.selectedUids);
    }
}

void MainWindow::scheduleMessageListStatus()
{
    if (m_listStatusTimer)
        m_listStatusTimer->start();
}

void MainWindow::updateMessageListStatus()
{
    if (!prettyMsgListModel)
        return;
    using namespace Imap::Mailbox;
    QStringList parts;
    const auto *msgList = qobject_cast<MsgListModel *>(m_imapAccess->msgListModel());
    const QModelIndex mailbox = msgList ? msgList->currentMailbox() : QModelIndex();
    QString statusDetails;
    const bool searchRunning = m_searchAcross && isSearchAcrossRunning();
    const bool canRetry = m_searchAcross && !searchRunning && !m_searchProblems.isEmpty();
    const bool canSearchServer = m_searchAcross && !searchRunning
        && std::any_of(m_folderSearches.cbegin(), m_folderSearches.cend(),
                       [](const QPointer<FolderSearch> &search) { return search && search->canContinueOnServer(); });
    const bool inList = searchShownInList();
    if (m_actionStopSearch)
        m_actionStopSearch->setVisible(inList && searchRunning);
    if (m_actionRetrySearch)
        m_actionRetrySearch->setVisible(inList && canRetry);
    if (m_actionSearchServer)
        m_actionSearchServer->setVisible(inList && canSearchServer);
    QStringList searchParts;
    QString searchDetails;
    if (m_searchAcross && searchModel()) {
        UnifiedMsgListModel *results = searchModel();
        PrettyUnifiedMsgListModel *prettyResults = searchPrettyModel();
        int folders = 0;
        int searched = 0;
        bool local = false;
        bool localOnline = false;
        bool incomplete = false;
        for (const QPointer<FolderSearch> &search : std::as_const(m_folderSearches)) {
            if (!search)
                continue;
            folders += search->folderCount();
            searched += search->searchedCount();
            const bool online = search->model() && search->model()->isNetworkAvailable();
            local = local || (search->usedLocalIndex() && !online);
            localOnline = localOnline || (search->usedLocalIndex() && online);
            incomplete = incomplete || !search->incompleteFolders().isEmpty() || search->indexPending()
                || search->indexPaused();
        }
        const int found = results->messageCount();
        searchDetails = searchProgressDetails();
        if (searchRunning) {
            searchParts << i18nc("@info:status %1 is a count of folders done, %2 of all folders", "Searching %1 of %2 folders",
                                 std::min(searched + 1, std::max(folders, 1)), std::max(folders, 1));
            searchParts << i18ncp("@info:status", "%1 found", "%1 found", found);
        } else if (m_searchStopped) {
            searchParts << i18nc("@info:status %1 is a count of folders done, %2 of all folders",
                                 "Search stopped after %1 of %2 folders", searched, folders);
            searchParts << i18ncp("@info:status", "%1 found", "%1 found", found);
        } else {
            searchParts << i18ncp("@info:status", "%1 message found", "%1 messages found", found);
        }
        if (prettyResults->filters() != PrettyMsgListModel::NoFilter) {
            searchParts << i18nc("@info:status filters hide some messages", "showing %1 of %2",
                                 prettyResults->visibleMessageCount(), found);
        }
        if (local)
            searchParts << i18nc("@info:status", "offline, in mail indexed on this computer");
        else if (localOnline)
            searchParts << i18nc("@info:status", "in mail indexed on this computer");
        if ((local || localOnline) && incomplete && !searchRunning)
            searchParts << i18nc("@info:status", "results may be incomplete");
        if (!m_searchProblems.isEmpty()) {
            searchParts << i18ncp("@info:status", "%1 place could not be searched", "%1 places could not be searched",
                                  m_searchProblems.size());
        }
        if (m_searchInPanel && m_searchResultsPanel) {
            m_searchResultsPanel->setStatus(searchParts.join(QStringLiteral(" \u00B7 ")), searchDetails);
            m_searchResultsPanel->setSearchActions(searchRunning, canRetry, canSearchServer);
        }
    }
    if (inList) {
        parts = searchParts;
        statusDetails = searchDetails;
    } else if (m_unifiedInbox && m_unifiedModel) {
        const int total = m_unifiedModel->messageCount();
        parts << i18nc("@info:status message count, unread count", "%1, %2",
                       i18ncp("@info:status", "%1 message", "%1 messages", total),
                       i18ncp("@info:status", "%1 unread", "%1 unread", m_unifiedModel->unreadCount()));
        if (m_prettyUnifiedModel->filters() != PrettyMsgListModel::NoFilter) {
            parts << i18nc("@info:status filters hide some messages", "showing %1 of %2",
                           m_prettyUnifiedModel->visibleMessageCount(), total);
        }
        // Their cached messages may still be listed, but nothing new arrives
        QStringList offline;
        QStringList unavailable;
        for (const QString &accountId : m_unifiedModel->accountIds()) {
            switch (m_unifiedModel->accountState(accountId)) {
            case UnifiedMsgListModel::AccountState::Ready:
            case UnifiedMsgListModel::AccountState::Syncing:
                break;
            case UnifiedMsgListModel::AccountState::Offline:
                offline << m_unifiedModel->accountName(accountId);
                break;
            case UnifiedMsgListModel::AccountState::Failed:
            case UnifiedMsgListModel::AccountState::Unavailable:
                unavailable << m_unifiedModel->accountName(accountId);
                break;
            }
        }
        const QString separator = i18nc("@info:status separator of account names", ", ");
        if (!offline.isEmpty())
            parts << i18nc("@info:status list of account names", "offline: %1", offline.join(separator));
        if (!unavailable.isEmpty())
            parts << i18nc("@info:status list of account names", "unavailable: %1", unavailable.join(separator));
    } else if (mailbox.isValid()) {
        const auto *threading = qobject_cast<ThreadingMsgListModel *>(m_imapAccess->threadingMsgListModel());
        const QString failure = threading ? threading->searchFailure() : QString();
        if (!failure.isEmpty()) {
            // The list is empty because the search could not run, not because nothing matched
            parts << i18nc("@info:status", "Search failed: %1", failure);
            statusDetails = failure;
        }
        const QVariant total = mailbox.data(RoleTotalMessageCount);
        const QVariant unread = mailbox.data(RoleUnreadMessageCount);
        if (total.isValid()) {
            QString counts = i18ncp("@info:status", "%1 message", "%1 messages", total.toInt());
            if (unread.isValid()) {
                counts = i18nc("@info:status message count, unread count", "%1, %2", counts,
                               i18ncp("@info:status", "%1 unread", "%1 unread", unread.toInt()));
            }
            parts << counts;
        }
        if (prettyMsgListModel->filters() != PrettyMsgListModel::NoFilter) {
            const int shown = prettyMsgListModel->visibleMessageCount();
            parts << (total.isValid() ? i18nc("@info:status filters hide some messages", "showing %1 of %2", shown, total.toInt())
                                      : i18ncp("@info:status", "showing %1 message", "showing %1 messages", shown));
        }
    }
    const int selected = msgListWidget->tree->selectedTree().size();
    if (selected > 1)
        parts << i18ncp("@info:status", "%1 selected", "%1 selected", selected);
    if (m_clearSearchAndFilters)
        m_clearSearchAndFilters->setEnabled(messageListNarrowed() || m_searchAcross);
    msgListWidget->setListStatus(parts.join(QStringLiteral(" \u00B7 ")), statusDetails);
    updateSelectionActions();
    scheduleSavedSearchCount();
}

void MainWindow::updateSelectionActions()
{
    if (!m_actionSelectThread)
        return;
    const bool threaded = actionThreadMsgList->isEnabled() && actionThreadMsgList->isChecked();
    m_actionSelectThread->setEnabled(threaded && msgListWidget->tree->currentIndex().isValid());
    updateRelatedActions();
}

void MainWindow::slotSelectThread()
{
    QModelIndex root = msgListWidget->tree->currentIndex();
    if (!root.isValid())
        return;
    while (root.parent().isValid())
        root = root.parent();
    QItemSelection selection;
    selectShownRows(msgListWidget->tree, root.siblingAtColumn(0), selection);
    msgListWidget->tree->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
}

void MainWindow::slotInvertSelection()
{
    const QAbstractItemModel *model = msgListWidget->tree->model();
    if (!model)
        return;
    QItemSelection shown;
    for (int row = 0, rows = model->rowCount(); row < rows; ++row)
        selectShownRows(msgListWidget->tree, model->index(row, 0), shown);
    // Not a Toggle: QItemSelectionModel::isRowSelected() ignores the parent of a pending toggle,
    // which confuses rows of different threads
    QItemSelectionModel *selectionModel = msgListWidget->tree->selectionModel();
    QItemSelection inverted;
    for (const QItemSelectionRange &range : std::as_const(shown)) {
        if (!selectionModel->isSelected(range.topLeft()))
            inverted.append(range);
    }
    selectionModel->select(inverted, QItemSelectionModel::ClearAndSelect);
}

void MainWindow::updateRelatedActions()
{
    if (!m_actionRelatedSender)
        return;
    const QModelIndex current = msgListWidget->tree->currentIndex();
    auto known = [&current](UiUtils::FieldFilter field) {
        return current.isValid() && !UiUtils::fieldFilterValue(current, field).isEmpty();
    };
    m_actionRelatedSender->setEnabled(known(UiUtils::FieldFilter::From));
    m_actionRelatedRecipient->setEnabled(known(UiUtils::FieldFilter::To));
    m_actionRelatedSubject->setEnabled(known(UiUtils::FieldFilter::Subject));
    // Only offered for list messages
    const bool list = known(UiUtils::FieldFilter::MailingList);
    m_actionRelatedList->setVisible(list);
    m_actionRelatedList->setEnabled(list);
    m_actionSelectSameSender->setEnabled(m_actionRelatedSender->isEnabled());
    m_actionSelectSameSubject->setEnabled(m_actionRelatedSubject->isEnabled());
    // Show Related searches a folder on the server, and the Unified Inbox has none
    if (showsUnifiedList()) {
        for (QAction *action : {m_actionRelatedSender, m_actionRelatedRecipient, m_actionRelatedSubject, m_actionRelatedList})
            action->setEnabled(false);
    }
    const QAbstractItemModel *model = msgListWidget->tree->model();
    const bool hasRows = model && model->rowCount() > 0;
    m_actionSelectUnread->setEnabled(hasRows);
    m_actionSelectFlagged->setEnabled(hasRows);
}

void MainWindow::showRelatedMessages(UiUtils::FieldFilter field)
{
    if (showsUnifiedList())
        return;
    const QString value = UiUtils::fieldFilterValue(msgListWidget->tree->currentIndex(), field);
    if (value.isEmpty())
        return;
    msgListWidget->showFieldFilter(UiUtils::fieldFilterText(field, value));
}

void MainWindow::selectSimilarMessages(UiUtils::SimilarMessages criterion)
{
    QTreeView *tree = msgListWidget->tree;
    const QModelIndexList matches = UiUtils::findSimilarMessages(tree->model(), tree->currentIndex(), criterion);
    if (matches.isEmpty()) {
        m_notificationBar->showInfo(i18nc("@info", "No loaded message matches."));
        return;
    }
    QItemSelection selection;
    for (const QModelIndex &match : matches) {
        // Shown, and expanded so that the actions do not include the collapsed replies along with it
        for (QModelIndex parent = match.parent(); parent.isValid(); parent = parent.parent())
            tree->expand(parent);
        if (tree->model()->hasChildren(match))
            tree->expand(match);
        selection.select(match.siblingAtColumn(0), match.siblingAtColumn(tree->model()->columnCount(match.parent()) - 1));
    }
    tree->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
    scheduleMessageListStatus();
}

void MainWindow::setDenseProcessingLayout(bool enabled)
{
    const KSharedConfigPtr stateConfig = KSharedConfig::openStateConfig();
    KConfigGroup state(stateConfig, QStringLiteral("DenseProcessing"));
    if (enabled) {
        if (m_densityMenu) {
            state.writeEntry("PreviousComfortable", m_densityMenu->isComfortable());
            m_densityMenu->setComfortable(false);
        }
        // Columns fit more messages on the screen and can be sorted from their header
        if (m_messageRowsMenu) {
            state.writeEntry("PreviousMultiLine", m_messageRowsMenu->isMultiLine());
            m_messageRowsMenu->setMultiLine(false);
        }
        QStringList hidden;
        if (m_embeddedPanelsMenu) {
            for (EmbeddedPanel *panel : std::as_const(m_embeddedPanels)) {
                // Attachments stay, since deciding what to do with a message often depends on them
                // and the Search Results panel keeps the results of the search
                if (panel->descriptor().id == QLatin1String("Attachments")
                    || panel->descriptor().id == QLatin1String("SearchResults"))
                    continue;
                QAction *visible = m_embeddedPanelsMenu->visibleAction(panel);
                if (visible && visible->isChecked()) {
                    visible->setChecked(false);
                    hidden << panel->descriptor().id;
                }
            }
        }
        state.writeEntry("HiddenPanels", hidden);
        state.writeEntry("Enabled", true);
        msgListWidget->tree->setFocus();
    } else {
        // Only undo what the preset changed; a panel shown again meanwhile is left alone
        if (m_densityMenu && state.readEntry("PreviousComfortable", false))
            m_densityMenu->setComfortable(true);
        if (m_messageRowsMenu && state.readEntry("PreviousMultiLine", false))
            m_messageRowsMenu->setMultiLine(true);
        const QStringList hidden = state.readEntry("HiddenPanels", QStringList());
        if (m_embeddedPanelsMenu) {
            for (EmbeddedPanel *panel : std::as_const(m_embeddedPanels)) {
                if (!hidden.contains(panel->descriptor().id))
                    continue;
                if (QAction *visible = m_embeddedPanelsMenu->visibleAction(panel))
                    visible->setChecked(true);
            }
        }
        state.deleteEntry("PreviousComfortable");
        state.deleteEntry("PreviousMultiLine");
        state.deleteEntry("HiddenPanels");
        state.writeEntry("Enabled", false);
    }
    if (!stateConfig->sync())
        qWarning() << "Unable to save the dense processing layout state";
}

void MainWindow::updateSortButton()
{
    if (!m_sortButton)
        return;
    const QAction *column = m_actionSortNone->actionGroup()->checkedAction();
    const bool descending = m_actionSortDescending->isChecked();
    m_sortButton->setIcon(quickActionIcon(descending ? QStringLiteral("view-sort-descending") : QStringLiteral("view-sort-ascending")));
    QString text;
    if (!column || column == m_actionSortNone) {
        text = i18nc("@info:tooltip", "Sorting: not sorted");
    } else if (column == m_actionSortByRelevance) {
        // Ascending puts the best matches first
        text = descending ? i18nc("@info:tooltip", "Sorting: relevance, best matches last")
                          : i18nc("@info:tooltip", "Sorting: relevance, best matches first");
    } else {
        const QString criterion = KLocalizedString::removeAcceleratorMarker(column->text());
        text = descending ? i18nc("@info:tooltip %1 is a sort criterion", "Sorting: %1, descending", criterion)
                          : i18nc("@info:tooltip %1 is a sort criterion", "Sorting: %1, ascending", criterion);
    }
    m_sortButton->setToolTip(text);
    m_sortButton->setAccessibleDescription(text);
}

void MainWindow::slotCapabilitiesUpdated(const QStringList &capabilities)
{
    msgListWidget->tree->header()->viewport()->removeEventFilter(this);
    if (capabilities.contains(QStringLiteral("SORT"))) {
        m_actionSortByDate->actionGroup()->setEnabled(true);
    } else {
        m_actionSortByDate->actionGroup()->setEnabled(false);
        msgListWidget->tree->header()->viewport()->installEventFilter(this);
    }

    msgListWidget->setFuzzySearchSupported(capabilities.contains(QStringLiteral("SEARCH=FUZZY")));

    m_actionShowOnlySubscribed->setEnabled(capabilities.contains(QStringLiteral("LIST-EXTENDED")));
    m_actionShowOnlySubscribed->setChecked(m_actionShowOnlySubscribed->isEnabled() &&
                                           m_settings->value(
                                               Common::SettingsNames::guiMailboxListShowOnlySubscribed, false).toBool());
    m_actionSubscribeMailbox->setEnabled(m_actionShowOnlySubscribed->isEnabled());

    const QStringList supportedCapabilities = Imap::Mailbox::ThreadingMsgListModel::supportedCapabilities();
    Q_FOREACH(const QString &capability, capabilities) {
        if (supportedCapabilities.contains(capability)) {
            actionThreadMsgList->setEnabled(true);
            if (actionThreadMsgList->isChecked()) {
                // Threading resets the sorting, which the folder's sorting replaces again
                ++m_applyingView;
                slotThreadMsgList();
                --m_applyingView;
                applyMessageListSort();
            }
            return;
        }
    }
    actionThreadMsgList->setEnabled(false);
}

void MainWindow::slotShowImapInfo()
{
    QString caps;
    Q_FOREACH(const QString &cap, imapModel()->capabilities()) {
        caps += tr("<li>%1</li>\n").arg(cap);
    }

    QString idString;
    if (!imapModel()->serverId().isEmpty() && imapModel()->capabilities().contains(QStringLiteral("ID"))) {
        QMap<QByteArray,QByteArray> serverId = imapModel()->serverId();

#define IMAP_ID_FIELD(Var, Name) bool has_##Var = serverId.contains(Name); \
    QString Var = has_##Var ? QString::fromUtf8(serverId[Name]).toHtmlEscaped() : tr("Unknown");
        IMAP_ID_FIELD(serverName, "name");
        IMAP_ID_FIELD(serverVersion, "version");
        IMAP_ID_FIELD(os, "os");
        IMAP_ID_FIELD(osVersion, "os-version");
        IMAP_ID_FIELD(vendor, "vendor");
        //IMAP_ID_FIELD(supportUrl, "support-url");
        //IMAP_ID_FIELD(address, "address");
        //IMAP_ID_FIELD(date, "date");
        //IMAP_ID_FIELD(command, "command");
        //IMAP_ID_FIELD(arguments, "arguments");
        //IMAP_ID_FIELD(environment, "environment");
#undef IMAP_ID_FIELD
        if (has_serverName) {
            idString = tr("<p>");
            if (has_serverVersion)
                idString += tr("Server: %1 %2").arg(serverName, serverVersion);
            else
                idString += tr("Server: %1").arg(serverName);

            if (has_vendor) {
                idString += tr(" (%1)").arg(vendor);
            }
            if (has_os) {
                if (has_osVersion)
                    idString += tr(" on %1 %2", "%1 is the operating system of an IMAP server and %2 is its version.").arg(os, osVersion);
                else
                    idString += tr(" on %1", "%1 is the operationg system of an IMAP server.").arg(os);
            }
            idString += tr("</p>");
        } else {
            idString = tr("<p>The IMAP server did not return usable information about itself.</p>");
        }
        QString fullId;
        for (QMap<QByteArray,QByteArray>::const_iterator it = serverId.constBegin(); it != serverId.constEnd(); ++it) {
            fullId += tr("<li>%1: %2</li>").arg(QString::fromUtf8(it.key()).toHtmlEscaped(), QString::fromUtf8(it.value()).toHtmlEscaped());
        }
        idString += tr("<ul>%1</ul>").arg(fullId);
    } else {
        idString = tr("<p>The server has not provided information about its software version.</p>");
    }

    Ui::ImapInfoDialog ui;
    QScopedPointer<QDialog> dialog(new QDialog(this));

    ui.setupUi(dialog.data());
    ui.version->setText(tr("%1\n"
                           "<p>The following capabilities are currently advertised:</p>").arg(idString));
    ui.capabilities->setText(tr("<ul>\n%1</ul>").arg(caps));

    dialog->exec();
}

QSize MainWindow::sizeHint() const
{
    return QSize(1150, 980);
}

void MainWindow::slotUpdateWindowTitle()
{
    QModelIndex mailbox = qobject_cast<Imap::Mailbox::MsgListModel *>(m_imapAccess->msgListModel())->currentMailbox();
    QString profileName = Common::Application::profileName();
    if (!profileName.isEmpty())
        profileName = QLatin1String(" [") + profileName + QLatin1Char(']');
    if (m_accounts->accountIds().size() > 1)
        profileName += QLatin1String(" [") + m_accounts->displayName(m_accounts->activeAccount()) + QLatin1Char(']');
    if (mailbox.isValid()) {
        if (mailbox.data(Imap::Mailbox::RoleUnreadMessageCount).toInt()) {
            //~ singular %1 - %n unread - Plasma Impresario
            //~ plural %1 - %n unread - Plasma Impresario
            setWindowTitle(tr("%1 - %n unread - Plasma Impresario", nullptr, mailbox.data(Imap::Mailbox::RoleUnreadMessageCount).toInt())
                           .arg(mailbox.data(Imap::Mailbox::RoleShortMailboxName).toString()) + profileName);
        } else {
            setWindowTitle(tr("%1 - Plasma Impresario").arg(mailbox.data(Imap::Mailbox::RoleShortMailboxName).toString()) + profileName);
        }
    } else {
        setWindowTitle(tr("Plasma Impresario") + profileName);
    }
    if (m_shellHeader)
        m_shellHeader->setFolder(mailbox.isValid() ? mailbox.data(Imap::Mailbox::RoleShortMailboxName).toString() : QString());
}

void MainWindow::setMailboxSidebarRight(bool right)
{
    m_mailboxSidebarOnRight = right;
    if (!m_shellRow || m_mainStack)
        return;
    QSplitter *target = right ? m_shellRight.widget : m_shellLeft.widget;
    if (m_accountSidebar->parentWidget() == target)
        return;
    // The sidebar keeps its width; whatever else is in the columns stays where it is.
    rememberShellExtents();
    std::swap(m_shellLeft.extent, m_shellRight.extent);
    placeMailboxSidebar();
    placeEmbeddedPanels();
    updateShellRegions(false, true);
}

void MainWindow::createShell()
{
    auto saveLater = [this](QSplitter *splitter) {
        connect(splitter, &QSplitter::splitterMoved, m_delayedStateSaving, static_cast<void (QTimer::*)()>(&QTimer::start));
    };

    m_shellBody = new QSplitter(Qt::Vertical);
    m_shellBody->setObjectName(QStringLiteral("shellBody"));
    m_shellRow = new QSplitter(Qt::Horizontal);
    m_shellRow->setObjectName(QStringLiteral("shellRow"));
    m_mainColumn = new QSplitter(Qt::Vertical);
    m_mainColumn->setObjectName(QStringLiteral("shellMainColumn"));

    m_shellLeft.widget = new QSplitter(Qt::Vertical);
    m_shellLeft.widget->setObjectName(QStringLiteral("shellLeftColumn"));
    m_shellRight.widget = new QSplitter(Qt::Vertical);
    m_shellRight.widget->setObjectName(QStringLiteral("shellRightColumn"));
    m_shellFooter.widget = new QSplitter(Qt::Horizontal);
    m_shellFooter.widget->setObjectName(QStringLiteral("shellFooter"));

    // The tab bar can sit beside the main column, so the column has a wrapper
    m_mainArea = new QWidget;
    m_mainArea->setObjectName(QStringLiteral("shellMainArea"));
    m_mainAreaLayout = new QBoxLayout(QBoxLayout::TopToBottom, m_mainArea);
    m_mainAreaLayout->setContentsMargins(0, 0, 0, 0);
    m_mainAreaLayout->setSpacing(0);
    m_mainAreaLayout->addWidget(m_mainColumn, 1);

    m_shellRow->addWidget(m_shellLeft.widget);
    m_shellRow->addWidget(m_mainArea);
    m_shellRow->addWidget(m_shellRight.widget);
    m_shellRow->setStretchFactor(0, 0);
    m_shellRow->setStretchFactor(1, 1);
    m_shellRow->setStretchFactor(2, 0);
    m_shellRow->setCollapsible(1, false);

    m_shellBody->addWidget(m_shellRow);
    m_shellBody->addWidget(m_shellFooter.widget);
    m_shellBody->setStretchFactor(0, 1);
    m_shellBody->setStretchFactor(1, 0);
    m_shellBody->setCollapsible(0, false);

    for (QSplitter *splitter : {m_shellBody, m_shellRow, m_mainColumn, m_shellLeft.widget, m_shellRight.widget, m_shellFooter.widget})
        saveLater(splitter);
    connect(m_shellRow, &QSplitter::splitterMoved, this, &MainWindow::possiblyLoadMessageOnSplittersChanged);
    for (QSplitter *splitter : {m_shellRow, m_shellBody})
        connect(splitter, &QSplitter::splitterMoved, this, &MainWindow::rememberShellExtents);
    connect(m_mainColumn, &QSplitter::splitterMoved, this, &MainWindow::possiblyLoadMessageOnSplittersChanged);

    // Region sizes can only be applied once the shell has its real size.
    m_shellRow->installEventFilter(this);
    m_shellBody->installEventFilter(this);

    for (QSplitter *region : {m_readerColumn, m_shellLeft.widget, m_shellRight.widget, m_shellFooter.widget}) {
        connect(region, &QSplitter::splitterMoved, this, [this, region]() { rememberPanelExtents(region); });
        region->installEventFilter(this);
    }

    for (QSplitter *region : {m_shellLeft.widget, m_shellRight.widget, m_shellFooter.widget})
        region->hide();

    // The folder tree is grouped by account; the sidebar takes the tree's place in the side columns
    m_accountSidebar = new AccountSidebar(mboxTree);
    connect(m_accountSidebar, &AccountSidebar::accountActivated, this, &MainWindow::switchAccount);
    connect(m_accountSidebar, &AccountSidebar::accountMenuRequested, this, &MainWindow::showAccountMenu);
    m_savedSearchFolders = new SavedSearchFolders;
    m_accountSidebar->setFooter(m_savedSearchFolders);
    connect(m_savedSearchFolders, &SavedSearchFolders::searchActivated, this, &MainWindow::activateSavedSearch);
    connect(m_savedSearchFolders, &SavedSearchFolders::editRequested, this, &MainWindow::editSavedSearch);
    connect(m_savedSearchFolders, &SavedSearchFolders::renameRequested, msgListWidget, &MessageListWidget::renameSavedSearch);
    connect(m_savedSearchFolders, &SavedSearchFolders::deleteRequested, msgListWidget, &MessageListWidget::deleteSavedSearch);
    connect(m_savedSearchFolders, &SavedSearchFolders::ownerChangeRequested, this, &MainWindow::changeSavedSearchOwner);
    connect(m_savedSearchFolders, &SavedSearchFolders::foldersChoiceRequested, this, &MainWindow::chooseSavedSearchFolders);
    connect(m_savedSearchFolders, &SavedSearchFolders::missingFoldersRemovalRequested, this,
            &MainWindow::removeMissingSavedSearchFolders);
    connect(m_savedSearchFolders, &SavedSearchFolders::orderChanged, this, &MainWindow::storeSavedSearchOrder);
    connect(m_savedSearchFolders, &SavedSearchFolders::ruleRequested, this, &MainWindow::createRuleFromSavedSearch);
    m_savedSearchProblemsTimer = new QTimer(this);
    m_savedSearchProblemsTimer->setInterval(500);
    m_savedSearchProblemsTimer->setSingleShot(true);
    connect(m_savedSearchProblemsTimer, &QTimer::timeout, this, &MainWindow::refreshSavedSearchProblems);
    m_savedSearchCountTimer = new QTimer(this);
    m_savedSearchCountTimer->setInterval(300);
    m_savedSearchCountTimer->setSingleShot(true);
    connect(m_savedSearchCountTimer, &QTimer::timeout, this, &MainWindow::countActiveSavedSearch);
    connect(msgListWidget, &MessageListWidget::savedSearchesChanged, this, &MainWindow::reloadSavedSearchFolders);
    connect(msgListWidget, &MessageListWidget::activeSavedSearchChanged, this, [this](const QString &owner, const QString &id) {
        m_savedSearchFolders->setActive(owner, id);
        scheduleSavedSearchCount();
    });
    connect(this, &MainWindow::accountsChanged, this, &MainWindow::reloadSavedSearchFolders);
    reloadSavedSearchFolders();

    // The header region sits above the columns and the footer; it is not resizable
    m_shellHeader = new ShellHeader(KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")));
    auto *shell = new QWidget;
    shell->setObjectName(QStringLiteral("shell"));
    auto *shellLayout = new QVBoxLayout(shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);
    shellLayout->addWidget(m_shellHeader);
    shellLayout->addWidget(m_shellBody, 1);
    m_shellLayout = shellLayout;
    setCentralWidget(shell);

    m_tabBar = new MailTabBar(this);
    m_tabBar->hide();
    connect(m_tabBar, &MailTabBar::tabActivated, this, &MainWindow::activateTab);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabBar, &QTabBar::tabMoved, this, [this](int from, int to) {
        if (from < 0 || to < 0 || from >= m_tabSession.tabs.size() || to >= m_tabSession.tabs.size())
            return;
        m_tabSession.tabs.move(from, to);
        m_tabSession.current = m_tabBar->currentIndex();
        saveTabs();
    });
}

void MainWindow::placeMailboxSidebar()
{
    if (!m_shellRow)
        return;
    // Called before leaving the one-at-a-time layout too, so that deleting its stack does not delete the tree
    QSplitter *column = m_mailboxSidebarOnRight ? m_shellRight.widget : m_shellLeft.widget;
    column->insertWidget(0, m_accountSidebar);
    column->setStretchFactor(0, 1);
    m_accountSidebar->show();
}

void MainWindow::placeEmbeddedPanels()
{
    if (!m_shellRow)
        return;
    QHash<QSplitter *, int> nextIndex;
    for (EmbeddedPanel *panel : std::as_const(m_embeddedPanels)) {
        QSplitter *region = panelRegion(panel);
        if (!nextIndex.contains(region)) {
            // Panels follow the mailbox tree or the reader, in their canonical order.
            const bool afterAnchor = region->count() > 0 && (region->widget(0) == m_accountSidebar || region->widget(0) == m_messageWidget);
            nextIndex.insert(region, afterAnchor ? 1 : 0);
        }
        int &index = nextIndex[region];
        if (region->indexOf(panel) != index)
            region->insertWidget(index, panel);
        region->setStretchFactor(index, 0);
        region->setCollapsible(index, false);
        ++index;
        // Moving a widget between splitters hides it
        panel->setVisible(panel->shouldBeShown());
    }
    fitPanelRegions();
}

QSplitter *MainWindow::panelRegion(const EmbeddedPanel *panel) const
{
    switch (panel->placement()) {
    case EmbeddedPanel::Placement::BelowReader:
        return m_readerColumn;
    case EmbeddedPanel::Placement::LeftColumn:
        return m_shellLeft.widget;
    case EmbeddedPanel::Placement::RightColumn:
        return m_shellRight.widget;
    case EmbeddedPanel::Placement::Footer:
        return m_shellFooter.widget;
    }
    return m_readerColumn;
}

void MainWindow::rememberPanelExtents(QSplitter *region)
{
    const QList<int> sizes = region->sizes();
    for (int i = 0; i < region->count(); ++i) {
        auto *panel = qobject_cast<EmbeddedPanel *>(region->widget(i));
        if (panel && !panel->isHidden() && sizes.at(i) > 0)
            m_panelExtents.insert(panel, sizes.at(i));
    }
}

void MainWindow::fitPanelRegions()
{
    if (!m_shellRow)
        return;
    for (QSplitter *region : {m_readerColumn, m_shellLeft.widget, m_shellRight.widget, m_shellFooter.widget})
        fitPanelRegion(region);
}

bool MainWindow::shellRegionHasContent(const QSplitter *region) const
{
    for (int i = 0; i < region->count(); ++i) {
        QWidget *widget = region->widget(i);
        if (widget == m_accountSidebar || (widget == m_tabBar && m_tabsEnabled))
            return true;
        if (auto panel = qobject_cast<EmbeddedPanel *>(widget); panel && panel->shouldBeShown())
            return true;
    }
    return false;
}

namespace {
int splitterExtent(const QSplitter *splitter, const QWidget *widget)
{
    return splitter->orientation() == Qt::Horizontal ? widget->width() : widget->height();
}

int hintExtent(const QSplitter *splitter, const QWidget *widget)
{
    const QSize hint = widget->sizeHint();
    return splitter->orientation() == Qt::Horizontal ? hint.width() : hint.height();
}

/** @short Pane sizes from QSplitter::saveState(), whose header has not changed since Qt 4 */
QList<int> savedSplitterSizes(const QByteArray &state)
{
    constexpr qint32 splitterMagic = 0xff;
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_5_0);
    qint32 marker = 0;
    qint32 version = 0;
    QList<int> sizes;
    stream >> marker >> version >> sizes;
    if (stream.status() != QDataStream::Ok || marker != splitterMagic)
        return {};
    return sizes;
}
}

void MainWindow::fitPanelRegion(QSplitter *region)
{
    const int available = splitterExtent(region, region);
    if (!region->isVisible() || available <= 0) {
        m_panelRegionsPending.insert(region);
        return;
    }
    m_panelRegionsPending.remove(region);

    int shownPanels = 0;
    const int tabBarIndex = m_tabBar && !m_tabBar->isHidden() ? region->indexOf(m_tabBar) : -1;
    for (int i = 0; i < region->count(); ++i) {
        if (qobject_cast<EmbeddedPanel *>(region->widget(i)) && !region->widget(i)->isHidden())
            ++shownPanels;
    }
    if (!shownPanels && tabBarIndex < 0)
        return;

    // The splitter gives a panel shown after the region was laid out only its minimum size, so size every
    // shown panel explicitly: its remembered size, or its size hint. Side by side, panels share the width.
    const bool horizontal = region->orientation() == Qt::Horizontal;
    QList<int> sizes;
    int anchorIndex = -1;
    int shown = 0;
    int panelTotal = 0;
    int fixed = 0;
    for (int i = 0; i < region->count(); ++i) {
        QWidget *widget = region->widget(i);
        int size = 0;
        if (!widget->isHidden()) {
            ++shown;
            if (i == tabBarIndex) {
                // The tab bar keeps the size of its tabs
                size = hintExtent(region, widget);
                fixed += size;
            } else if (auto *panel = qobject_cast<EmbeddedPanel *>(widget)) {
                size = m_panelExtents.value(panel, 0);
                if (size <= 0)
                    size = horizontal ? std::max(panel->sizeHint().width(), available / shownPanels) : panel->sizeHint().height();
                panelTotal += size;
            } else {
                anchorIndex = i;
            }
        }
        sizes.append(size);
    }
    const int space = available - region->handleWidth() * std::max(0, shown - 1) - fixed;
    if (space <= 0 || (panelTotal <= 0 && anchorIndex < 0))
        return;
    // The mailbox tree or the reader keeps at least a third of its region; without either, the panels fill it.
    const int panelSpace = anchorIndex >= 0 ? std::min(panelTotal, space * 2 / 3) : space;
    for (int i = 0; i < sizes.size(); ++i) {
        if (i != anchorIndex && i != tabBarIndex && sizes.at(i) > 0 && panelTotal > 0)
            sizes[i] = std::max(1, int(qint64(sizes.at(i)) * panelSpace / panelTotal));
    }
    if (anchorIndex >= 0)
        sizes[anchorIndex] = std::max(1, space - panelSpace);
    region->setSizes(sizes);
}

void MainWindow::rememberShellExtents()
{
    for (ShellRegion *region : {&m_shellLeft, &m_shellRight, &m_shellFooter}) {
        if (!region->widget || !region->shown)
            continue;
        auto parent = qobject_cast<QSplitter *>(region->widget->parentWidget());
        const int index = parent->indexOf(region->widget);
        const auto sizes = parent->sizes();
        int total = 0;
        for (int size : sizes)
            total += size;
        if (total > 0 && index >= 0 && sizes.at(index) > 0)
            region->extent = sizes.at(index);
    }
}

void MainWindow::updateShellRegions(bool rememberCurrent, bool force)
{
    if (!m_shellRow)
        return;
    if (rememberCurrent && m_shellRegionsInitialized)
        rememberShellExtents();

    bool rowChanged = force || !m_shellRegionsInitialized;
    bool bodyChanged = rowChanged;
    for (ShellRegion *region : {&m_shellLeft, &m_shellRight, &m_shellFooter}) {
        const bool shown = !m_focusLayout && shellRegionHasContent(region->widget);
        if (shown != region->shown) {
            (region == &m_shellFooter ? bodyChanged : rowChanged) = true;
            region->shown = shown;
        }
        region->widget->setVisible(shown);
    }
    m_shellRegionsInitialized = true;
    m_shellRowSizesPending = m_shellRowSizesPending || rowChanged;
    m_shellBodySizesPending = m_shellBodySizesPending || bodyChanged;
    applyPendingShellSizes();
    fitPanelRegions();
}

void MainWindow::applyPendingShellSizes(QSplitter *only)
{
    auto apply = [](QSplitter *splitter, QWidget *main, const QList<const ShellRegion *> &regions) {
        int available = splitterExtent(splitter, splitter);
        QList<int> sizes;
        int used = 0;
        int visibleCount = 0;
        int mainIndex = -1;
        for (int i = 0; i < splitter->count(); ++i) {
            QWidget *widget = splitter->widget(i);
            int size = 0;
            if (widget == main) {
                mainIndex = i;
                ++visibleCount;
            } else {
                for (const ShellRegion *region : regions) {
                    if (region->widget != widget || !region->shown)
                        continue;
                    ++visibleCount;
                    size = region->extent > 0 ? region->extent : hintExtent(splitter, widget);
                    // A region never takes more than two fifths of the shell
                    size = std::clamp(size, 1, std::max(1, available * 2 / 5));
                }
            }
            used += size;
            sizes.append(size);
        }
        available -= splitter->handleWidth() * std::max(0, visibleCount - 1);
        if (mainIndex >= 0)
            sizes[mainIndex] = std::max(1, available - used);
        splitter->setSizes(sizes);
    };
    // Until a splitter is laid out, its size says nothing about the space its regions will share
    auto ready = [only](QSplitter *splitter) {
        return (!only || only == splitter) && splitter->isVisible() && splitterExtent(splitter, splitter) > 0;
    };
    if (m_shellRowSizesPending && ready(m_shellRow)) {
        m_shellRowSizesPending = false;
        apply(m_shellRow, m_mainArea, {&m_shellLeft, &m_shellRight});
    }
    if (m_shellBodySizesPending && ready(m_shellBody)) {
        m_shellBodySizesPending = false;
        apply(m_shellBody, m_shellRow, {&m_shellFooter});
    }
}

void MainWindow::slotLayoutCompact()
{
    saveSizesAndState();

    m_mainColumn->setOrientation(Qt::Vertical);
    m_mainColumn->insertWidget(0, msgListWidget);
    m_mainColumn->insertWidget(1, m_readerColumn);
    m_mainColumn->setStretchFactor(0, 1);
    m_mainColumn->setStretchFactor(1, 1);
    // The reader shall not collapse
    m_mainColumn->setCollapsible(1, false);
    m_mainColumn->setSizes({msgListWidget->sizeHint().height(), m_readerColumn->sizeHint().height()});

    placeMailboxSidebar();
    msgListWidget->show();
    m_readerColumn->show();
    delete m_mainStack;
    placeEmbeddedPanels();

    m_layoutMode = LAYOUT_COMPACT;
    m_settings->setValue(Common::SettingsNames::guiMainWindowLayout, Common::SettingsNames::guiMainWindowLayoutCompact);
    applySizesAndState();
}

void MainWindow::slotLayoutWide()
{
    saveSizesAndState();

    m_mainColumn->setOrientation(Qt::Horizontal);
    m_mainColumn->insertWidget(0, msgListWidget);
    m_mainColumn->insertWidget(1, m_readerColumn);
    m_mainColumn->setStretchFactor(0, 1);
    m_mainColumn->setStretchFactor(1, 1);
    m_mainColumn->setCollapsible(1, false);
    m_mainColumn->setSizes({1, 1});

    placeMailboxSidebar();
    msgListWidget->show();
    m_readerColumn->show();
    delete m_mainStack;
    placeEmbeddedPanels();

    m_layoutMode = LAYOUT_WIDE;
    m_settings->setValue(Common::SettingsNames::guiMainWindowLayout, Common::SettingsNames::guiMainWindowLayoutWide);
    applySizesAndState();
}

void MainWindow::slotLayoutOneAtTime()
{
    saveSizesAndState();
    if (m_mainStack)
        return;

    rememberShellExtents();
    m_mainStack = new OnePanelAtTimeWidget(this, mboxTree, msgListWidget, m_readerColumn, m_mainToolbar, m_oneAtTimeGoBack,
                                           m_accountSidebar);
    m_mainColumn->addWidget(m_mainStack);
    m_mainStack->show();
    placeEmbeddedPanels();
    updateShellRegions(false);

    m_layoutMode = LAYOUT_ONE_AT_TIME;
    m_settings->setValue(Common::SettingsNames::guiMainWindowLayout, Common::SettingsNames::guiMainWindowLayoutOneAtTime);
    applySizesAndState();
}

void MainWindow::activateRelatedMessage(const QModelIndex &message)
{
    const QModelIndex deproxified = Imap::deproxifiedIndex(message);
    if (!deproxified.isValid())
        return;
    // UIDs repeat across folders and accounts, so only a row of the very same message is selected; a message which the
    // list does not show, such as one of another folder, opens in the reader and leaves the list as it is.
    QAbstractItemModel *model = msgListWidget->tree->model();
    const QModelIndexList candidates = model && model->rowCount() > 0
        ? model->match(model->index(0, 0), Imap::Mailbox::RoleMessageUid, deproxified.data(Imap::Mailbox::RoleMessageUid), -1,
                       Qt::MatchExactly | Qt::MatchRecursive)
        : QModelIndexList();
    const auto match = std::find_if(candidates.cbegin(), candidates.cend(),
                                    [&](const QModelIndex &row) { return Imap::deproxifiedIndex(row) == deproxified; });
    if (match != candidates.cend()) {
        msgListWidget->tree->setCurrentIndex(*match);
        msgListWidget->tree->scrollTo(*match);
        msgListClicked(*match);
        return;
    }
#ifdef IMPRESARIO_HAVE_WEBENGINE
    m_messageWidget->messageView->setMessage(deproxified);
#endif
}

void MainWindow::revealInFolder(const QModelIndex &message)
{
    // The index may belong to a list which is about to be emptied
    const QModelIndex deproxified = Imap::deproxifiedIndex(message);
    if (!deproxified.isValid())
        return;
    revealLocation(accountOfModel(qobject_cast<const Imap::Mailbox::Model *>(deproxified.model())),
                   deproxified.data(Imap::Mailbox::RoleMailboxName).toString(),
                   deproxified.data(Imap::Mailbox::RoleMailboxUidValidity).toUInt(),
                   deproxified.data(Imap::Mailbox::RoleMessageUid).toUInt());
}

void MainWindow::revealLocation(const QString &accountId, const QString &mailbox, uint uidValidity, uint uid)
{
    if (accountId.isEmpty() || mailbox.isEmpty() || !uid)
        return;
    // Results in the message list, or a search of the folder, would hide the other messages of the folder
    if (searchShownInList() || (!m_searchAcross && messageListNarrowed())) {
        leaveSearchAcross();
        msgListWidget->clearSearch();
    }
    if (accountId != m_accounts->activeAccount())
        switchAccount(accountId);
    if (accountId != m_accounts->activeAccount())
        return;
    m_pendingReveal = {accountId, mailbox, uidValidity, uid};
    if (!m_unifiedInbox && currentMailboxName() == mailbox) {
        restoreFolderPosition();
        msgListWidget->tree->setFocus();
    } else {
        goToMailbox(mailbox);
    }
}

void MainWindow::updateAgendaSource()
{
    if (!m_agendaPanel)
        return;
    const bool enabled = m_agendaPanel->isPanelEnabled();
#ifdef IMPRESARIO_HAVE_AKONADI_CALENDAR
    if (enabled && !m_agendaSource) {
        m_agendaSource = new UiUtils::AkonadiAgendaSource(this);
        m_agendaPanel->setCalendarSource(m_agendaSource);
    } else if (!enabled && m_agendaSource) {
        m_agendaPanel->setCalendarSource(nullptr);
        delete std::exchange(m_agendaSource, nullptr);
    }
#endif
    if (enabled) {
        scheduleAgendaMessages();
    } else {
        if (m_agendaMessagesTimer)
            m_agendaMessagesTimer->stop();
        m_agendaPanel->setMessages({});
    }
}

void MainWindow::scheduleAgendaMessages()
{
    if (m_agendaPanel && m_agendaPanel->isPanelEnabled() && m_agendaMessagesTimer && !m_agendaMessagesTimer->isActive())
        m_agendaMessagesTimer->start();
}

void MainWindow::refreshAgendaMessages()
{
    using namespace UiUtils::Agenda;
    if (!m_agendaPanel || !m_agendaPanel->isPanelEnabled())
        return;
    QList<MessageSource> sources;
    for (auto it = m_accountAccess.cbegin(); it != m_accountAccess.cend(); ++it) {
        Imap::Mailbox::SearchIndexer *indexer = it.value() ? it.value()->searchIndexer() : nullptr;
        if (!indexer)
            continue;
        connect(indexer, &Imap::Mailbox::SearchIndexer::finished, this, &MainWindow::scheduleAgendaMessages, Qt::UniqueConnection);
        if (indexer->isAvailable())
            sources.append({it.key(), indexer->index()});
    }

    // The shown folder knows the latest flags of the messages it has loaded, before the index catches up
    ShownFolder shown;
    auto *list = !m_unifiedInbox && m_imapAccess ? qobject_cast<Imap::Mailbox::MsgListModel *>(m_imapAccess->msgListModel()) : nullptr;
    const QModelIndex mailbox = list ? list->currentMailbox() : QModelIndex();
    if (mailbox.isValid()) {
        shown.accountId = m_accounts->activeAccount();
        shown.mailbox = mailbox.data(Imap::Mailbox::RoleMailboxName).toString();
        shown.uidValidity = mailbox.data(Imap::Mailbox::RoleMailboxUidValidity).toUInt();
        for (int row = 0; row < list->rowCount(); ++row) {
            const QModelIndex message = list->index(row, 0);
            const uint uid = message.data(Imap::Mailbox::RoleMessageUid).toUInt();
            // Asking an envelope which is not loaded yet would fetch it
            if (!uid || !message.data(Imap::Mailbox::RoleIsFetched).toBool())
                continue;
            shown.loaded.insert(uid);
            const QStringList flags = message.data(Imap::Mailbox::RoleMessageFlags).toStringList();
            const bool flagged = flags.contains(Imap::Mailbox::FlagNames::flagged, Qt::CaseInsensitive);
            const bool toDo = flags.contains(QStringLiteral("$TODO"), Qt::CaseInsensitive);
            if ((!flagged && !toDo) || flags.contains(Imap::Mailbox::FlagNames::deleted, Qt::CaseInsensitive))
                continue;
            Item item;
            item.kind = Kind::Message;
            item.title = message.data(Imap::Mailbox::RoleMessageSubject).toString();
            item.accountId = shown.accountId;
            item.mailbox = shown.mailbox;
            item.uidValidity = shown.uidValidity;
            item.uid = uid;
            item.messageId = QString::fromUtf8(message.data(Imap::Mailbox::RoleMessageMessageId).toByteArray());
            item.sender = Imap::Message::MailAddress::prettyList(message.data(Imap::Mailbox::RoleMessageFrom).toList(),
                                                                 Imap::Message::MailAddress::FORMAT_JUST_NAME);
            item.sent = message.data(Imap::Mailbox::RoleMessageDate).toDateTime().toLocalTime().date();
            item.toDo = toDo;
            shown.followUps << item;
        }
    }

    QList<Item> messages = followUps(sources, shown, maximumMessages);
    for (Item &item : messages) {
        item.source = i18nc("@item:intext %1 is an account, %2 a folder", "%1, %2", m_accounts->displayName(item.accountId),
                            item.mailbox);
    }
    m_agendaPanel->setMessages(messages);
}

void MainWindow::openAgendaMessage(const UiUtils::Agenda::Item &message)
{
    using Imap::Mailbox::FolderSearch;
    Imap::ImapAccess *access = m_accountAccess.value(message.accountId);
    auto *model = access ? qobject_cast<Imap::Mailbox::Model *>(access->imapModel()) : nullptr;
    if (!model || message.mailbox.isEmpty() || !message.uid)
        return;
    const auto sameMessage = [model, &message](const QModelIndex &candidate) {
        const QModelIndex found = Imap::deproxifiedIndex(candidate);
        if (found.data(Imap::Mailbox::RoleMailboxName).toString() != message.mailbox
            || found.data(Imap::Mailbox::RoleMessageUid).toUInt() != message.uid)
            return false;
        const uint uidValidity = model->knownUidValidity(found);
        return !message.uidValidity || !uidValidity || uidValidity == message.uidValidity;
    };

    if (!m_unifiedInbox && message.accountId == m_accounts->activeAccount() && currentMailboxName() == message.mailbox) {
        if (auto *list = qobject_cast<Imap::Mailbox::MsgListModel *>(access->msgListModel())) {
            for (int row = 0; row < list->rowCount(); ++row) {
                const QModelIndex candidate = list->index(row, 0);
                if (candidate.data(Imap::Mailbox::RoleMessageUid).toUInt() == message.uid && sameMessage(candidate)) {
                    activateRelatedMessage(candidate);
                    return;
                }
            }
        }
    }
    const QByteArray messageId = UiUtils::MessageId::parse(message.messageId).messageId;
    if (!messageId.isEmpty()) {
        const QModelIndexList loaded = model->findLoadedMessagesById(messageId);
        for (const QModelIndex &candidate : loaded) {
            if (sameMessage(candidate)) {
                activateRelatedMessage(candidate);
                return;
            }
        }
    }

    // The message has to be looked up in its folder, which selects that folder on the account's connection
    if (const FolderSearch *search = m_folderSearches.value(message.accountId); search && search->isRunning())
        stopSearchAcross();
    if (m_messageLookup && m_messageLookup->state() == UiUtils::MessageIdLookup::State::SearchingServers) {
        m_messageLookupStopped = true;
        m_messageLookup->cancel();
    }
    cancelAgendaSearch();
    auto *search = new FolderSearch(model, this);
    search->setSearchIndex(access->searchIndexer());
    m_agendaSearch = search;
    m_agendaSearchItem = message;
    connect(search, &FolderSearch::finished, this, &MainWindow::agendaSearchFinished);
    m_agendaBusy = m_notificationBar->showBusy(i18nc("@info", "Opening the message…"));
    search->start({message.mailbox}, false,
                  messageId.isEmpty() ? QStringList{QStringLiteral("UID"), QString::number(message.uid)}
                                      : UiUtils::MessageId::searchConditions(messageId));
    if (m_relatedMessagesPanel && m_relatedMessagesPanel->serverSearch()->isRunning()
        && Imap::deproxifiedIndex(m_relatedMessagesPanel->serverSearch()->message()).model() == model)
        m_relatedMessagesPanel->restartServerSearch();
}

void MainWindow::agendaSearchFinished()
{
    Imap::Mailbox::FolderSearch *search = m_agendaSearch;
    if (!search)
        return;
    const UiUtils::Agenda::Item item = m_agendaSearchItem;
    const QModelIndexList results = search->results()->messages();
    // The local index may not have the message yet
    if (results.isEmpty() && search->canContinueOnServer() && !search->continuedOnServer()) {
        search->continueOnServer();
        return;
    }
    QPersistentModelIndex found;
    for (const QModelIndex &result : results) {
        const QModelIndex message = Imap::deproxifiedIndex(result);
        if (message.data(Imap::Mailbox::RoleMailboxName).toString() != item.mailbox)
            continue;
        // A Message-ID found in the folder names the message even when the index knew it under an older UID
        if (!found.isValid() || message.data(Imap::Mailbox::RoleMessageUid).toUInt() == item.uid)
            found = message;
    }
    const QList<Imap::Mailbox::FolderSearch::Failure> failures = search->failures();
    const QString reason = failures.isEmpty() ? QString() : failures.constFirst().reason;
    m_agendaSearch = nullptr;
    search->deleteLater();
    reselectShownFolder(m_accountAccess.value(item.accountId));
    m_notificationBar->dismissBusy(std::exchange(m_agendaBusy, 0));
    resumeRelatedSearch();
    if (found.isValid()) {
        activateRelatedMessage(found);
        return;
    }
    const QString title = item.title.isEmpty() ? i18nc("@info message without subject", "(no subject)") : item.title;
    const QString text = reason.isEmpty()
        ? i18nc("@info %1 is a subject, %2 a folder", "The message \"%1\" is no longer in %2.", title, item.mailbox)
        : i18nc("@info %1 is a subject, %2 a reason", "The message \"%1\" could not be opened: %2", title, reason);
    m_notificationBar->showError(text, {{i18nc("@action:button", "Show in Folder"), QStringLiteral("folder-open"),
                                         QStringLiteral("agendaShowInFolder"), [this, item]() {
                                             revealLocation(item.accountId, item.mailbox, item.uidValidity, item.uid);
                                         }}});
}

void MainWindow::cancelAgendaSearch()
{
    Imap::Mailbox::FolderSearch *search = m_agendaSearch;
    if (!search)
        return;
    m_agendaSearch = nullptr;
    search->cancel();
    search->deleteLater();
    reselectShownFolder(m_accountAccess.value(m_agendaSearchItem.accountId));
    if (m_agendaBusy)
        m_notificationBar->dismissBusy(std::exchange(m_agendaBusy, 0));
    resumeRelatedSearch();
}

void MainWindow::openCalendarApplication()
{
    // My Day never changes calendars; editing belongs to the KDE PIM application
    const QStringList applications{QStringLiteral("org.kde.korganizer"), QStringLiteral("org.kde.merkuro.calendar")};
    for (const QString &name : applications) {
        if (const KService::Ptr service = KService::serviceByDesktopName(name)) {
            auto *job = new KIO::ApplicationLauncherJob(service);
            job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
            job->start();
            return;
        }
    }
    m_notificationBar->showError(i18nc("@info", "No calendar application was found. Install KOrganizer or Merkuro "
                                                "Calendar to see and edit events and tasks."));
}

void MainWindow::joinMeeting(const QUrl &url)
{
    // Calendar data comes from other people; only links to a meeting are opened, never local files or programs
    static const QStringList schemes{QStringLiteral("https"), QStringLiteral("http"), QStringLiteral("sip"),
                                     QStringLiteral("sips"), QStringLiteral("tel")};
    if (!url.isValid() || !schemes.contains(url.scheme().toLower()))
        return;
    auto *job = new KIO::OpenUrlJob(url);
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
    job->start();
}

void MainWindow::actOnOpenMessage(const std::function<void()> &fn)
{
#ifdef IMPRESARIO_HAVE_WEBENGINE
    const QModelIndex message = Imap::deproxifiedIndex(m_messageWidget->messageView->currentMessage());
    const QString accountId = accountOfModel(qobject_cast<const Imap::Mailbox::Model *>(message.model()));
    if (message.isValid() && !accountId.isEmpty())
        actAs(accountId, {message}, fn);
#else
    Q_UNUSED(fn);
#endif
}

void MainWindow::updateReaderActions()
{
#ifdef IMPRESARIO_HAVE_WEBENGINE
    if (!m_readerActions || !m_deleteMovesToTrash)
        return;
    const QModelIndex message = Imap::deproxifiedIndex(m_messageWidget->messageView->currentMessage());
    bool marks = false;
    bool archive = false;
    bool movesToTrash = true;
    actOnOpenMessage([&]() {
        marks = imapModel()->isNetworkAvailable() && message.data(Imap::Mailbox::RoleMessageUid).toUInt() > 0;
        archive = message.data(Imap::Mailbox::RoleMailboxName).toString() != archiveTarget();
        movesToTrash = !deleteTrashTarget().isEmpty();
    });
    m_readerActions->setMessage(message);
    m_readerActions->setMarksEnabled(marks);
    m_readerActions->setArchiveEnabled(archive);
    m_readerActions->setMovesToTrash(movesToTrash);
#endif
}

void MainWindow::showAttachmentPanel()
{
    if (!m_attachmentPanel)
        return;
    if (!m_attachmentPanel->isPanelEnabled() && m_embeddedPanelsMenu) {
        if (QAction *visible = m_embeddedPanelsMenu->visibleAction(m_attachmentPanel))
            visible->setChecked(true);
    }
    // The panel is shown once its region has been laid out again
    QTimer::singleShot(0, this, [this]() {
        if (m_attachmentPanel->isVisible())
            m_attachmentPanel->focusTarget()->setFocus(Qt::OtherFocusReason);
    });
}

Imap::Mailbox::Model *MainWindow::imapModel() const
{
    return qobject_cast<Imap::Mailbox::Model *>(currentAccess()->imapModel());
}

Imap::ImapAccess *MainWindow::currentAccess() const
{
    return m_acting ? m_actingAccess : m_imapAccess;
}

QString MainWindow::currentAccountId() const
{
    return m_acting ? m_actingAccountId : m_accounts->activeAccount();
}

bool MainWindow::showsModel(const Imap::Mailbox::Model *model) const
{
    if (!model)
        return false;
    if (static_cast<const QObject *>(model) == m_imapAccess->imapModel())
        return true;
    return showsUnifiedList() && m_unifiedModel && m_unifiedModel->accountIds().contains(accountOfModel(model));
}

QString MainWindow::accountOfModel(const Imap::Mailbox::Model *model) const
{
    if (!model)
        return QString();
    for (auto it = m_accountAccess.cbegin(); it != m_accountAccess.cend(); ++it) {
        if (it.value() && it.value()->imapModel() == static_cast<const QObject *>(model))
            return it.key();
    }
    return QString();
}

void MainWindow::actAs(const QString &accountId, const QModelIndexList &selection, const std::function<void()> &fn)
{
    Imap::ImapAccess *access = m_accountAccess.value(accountId);
    if (!access)
        return;
    const bool wasActing = m_acting;
    Imap::ImapAccess *const previousAccess = m_actingAccess;
    const QString previousAccountId = m_actingAccountId;
    const QModelIndexList previousSelection = m_actingSelection;
    m_acting = true;
    m_actingAccess = access;
    m_actingAccountId = accountId;
    m_actingSelection = selection;
    fn();
    m_acting = wasActing;
    m_actingAccess = previousAccess;
    m_actingAccountId = previousAccountId;
    m_actingSelection = previousSelection;
}

QStringList MainWindow::accountIds() const
{
    return m_accounts->accountIds();
}

QString MainWindow::accountName(const QString &accountId) const
{
    return m_accounts->displayName(accountId);
}

QString MainWindow::composerAccountId() const
{
    return currentAccountId();
}

QString MainWindow::messageAccountName(const QModelIndex &message) const
{
    if (!message.isValid() || m_accounts->accountIds().size() < 2)
        return QString();
    const QString named = message.data(Imap::Mailbox::RoleMessageAccountName).toString();
    if (!named.isEmpty())
        return named;
    const QModelIndex source = Imap::deproxifiedIndex(message);
    const QString accountId = accountOfModel(qobject_cast<const Imap::Mailbox::Model *>(source.model()));
    return accountId.isEmpty() ? QString() : m_accounts->displayName(accountId);
}

QVariant MainWindow::messageSetting(const QString &key, const QModelIndex &message) const
{
    QString accountId;
    QString folder;
    if (message.isValid()) {
        const QModelIndex source = Imap::deproxifiedIndex(message);
        accountId = accountOfModel(qobject_cast<const Imap::Mailbox::Model *>(source.model()));
        folder = message.data(Imap::Mailbox::RoleMailboxName).toString();
    }
    if (accountId.isEmpty())
        accountId = currentAccountId();
    const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
    return UiUtils::SettingsStore::value(*config, m_settings, key, accountId, folder);
}

bool MainWindow::withAccount(const QString &accountId, const std::function<void()> &fn)
{
    Imap::ImapAccess *access = m_accountAccess.value(accountId);
    if (!access || !access->imapModel() || !m_accounts->accountIds().contains(accountId))
        return false;
    actAs(accountId, QModelIndexList(), fn);
    return true;
}

bool MainWindow::actOnModel(const Imap::Mailbox::Model *model, const std::function<void()> &fn)
{
    if (!model)
        return false;
    if (static_cast<const QObject *>(model) == currentAccess()->imapModel()) {
        fn();
        return true;
    }
    const QString accountId = accountOfModel(model);
    if (accountId.isEmpty())
        return false;
    actAs(accountId, QModelIndexList(), fn);
    return true;
}

bool MainWindow::routeToMessageAccounts(const std::function<void()> &fn, bool singleAccount, const QModelIndexList &selection)
{
    if (!showsUnifiedList() || m_acting)
        return false;
    const QModelIndexList messages = selection.isEmpty() ? translatedSelection() : selection;
    QStringList order;
    QHash<QString, QModelIndexList> byAccount;
    for (const QModelIndex &message : messages) {
        const QString accountId = accountOfModel(qobject_cast<const Imap::Mailbox::Model *>(message.model()));
        if (accountId.isEmpty())
            continue;
        if (!byAccount.contains(accountId))
            order << accountId;
        byAccount[accountId] << message;
    }
    if (order.isEmpty() || (order.size() == 1 && order.constFirst() == m_accounts->activeAccount()))
        return false;
    if (singleAccount && order.size() > 1) {
        m_notificationBar->showError(i18nc("@info", "Messages of different accounts cannot be moved or copied together. "
                                                    "Select the messages of one account."));
        return true;
    }
    for (const QString &accountId : std::as_const(order))
        actAs(accountId, byAccount.value(accountId), fn);
    return true;
}

QString MainWindow::forAccountOf(const Imap::Mailbox::Model *model, const QString &text) const
{
    if (text.isEmpty() || !showsUnifiedList() || !m_unifiedModel || m_unifiedModel->accountIds().size() < 2)
        return text;
    const QString accountId = accountOfModel(model);
    if (accountId.isEmpty())
        return text;
    return i18nc("@info account name and what happened to its messages", "%1: %2", m_accounts->displayName(accountId), text);
}

QStringList MainWindow::unifiedAccountIds() const
{
    const QStringList excluded = KConfigGroup(KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")), unifiedInboxGroup())
            .readEntry("ExcludedAccounts", QStringList());
    QStringList ids;
    for (const QString &accountId : m_accounts->accountIds()) {
        if (!excluded.contains(accountId))
            ids << accountId;
    }
    return ids;
}

void MainWindow::populateUnifiedAccountsMenu()
{
    m_unifiedAccountsMenu->clear();
    const QStringList included = unifiedAccountIds();
    for (const QString &accountId : m_accounts->accountIds()) {
        QString label = m_accounts->displayName(accountId);
        QAction *action = m_unifiedAccountsMenu->addAction(label.replace(QLatin1Char('&'), QLatin1String("&&")));
        action->setCheckable(true);
        action->setChecked(included.contains(accountId));
        action->setData(accountId);
        // At least one account stays in the list
        action->setEnabled(!(included.size() == 1 && included.contains(accountId)));
        connect(action, &QAction::toggled, this, [this, accountId](bool checked) {
            KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")), unifiedInboxGroup());
            QStringList excluded = group.readEntry("ExcludedAccounts", QStringList());
            excluded.removeAll(accountId);
            if (!checked)
                excluded << accountId;
            group.writeEntry("ExcludedAccounts", excluded);
            if (!group.sync())
                qWarning() << "Could not save the accounts of the Unified Inbox";
            if (m_unifiedInbox)
                syncUnifiedSources();
        });
    }
}

void MainWindow::syncUnifiedSources()
{
    // The search results take the place of the INBOXes until the search is left
    if (!m_unifiedModel || searchShownInList())
        return;
    const QStringList wanted = m_unifiedInbox ? unifiedAccountIds() : QStringList();
    for (const QString &accountId : m_unifiedModel->accountIds()) {
        if (!wanted.contains(accountId))
            m_unifiedModel->removeAccount(accountId);
    }
    for (const QString &accountId : wanted) {
        Imap::ImapAccess *access = m_accountAccess.value(accountId);
        auto *model = access ? qobject_cast<Imap::Mailbox::Model *>(access->imapModel()) : nullptr;
        auto *msgList = access ? qobject_cast<Imap::Mailbox::MsgListModel *>(access->msgListModel()) : nullptr;
        QAbstractItemModel *source = nullptr;
        if (model && msgList) {
            if (!model->property("unifiedInboxHooked").toBool()) {
                model->setProperty("unifiedInboxHooked", true);
                // The INBOX can only be opened once the folders are listed
                connect(model, &QAbstractItemModel::rowsInserted, m_unifiedModel, [this](const QModelIndex &parent) {
                    if (!parent.isValid() && m_unifiedInbox)
                        syncUnifiedSources();
                });
                connect(msgList, &Imap::Mailbox::MsgListModel::mailboxChanged, m_unifiedModel, [this]() {
                    if (!m_unifiedInbox)
                        return;
                    QMetaObject::invokeMethod(m_unifiedModel, [this]() {
                        if (m_unifiedInbox)
                            syncUnifiedSources();
                    }, Qt::QueuedConnection);
                });
            }
            const QModelIndex inbox = model->mailboxIndexByName(QStringLiteral("INBOX"));
            if (inbox.isValid()) {
                if (msgList->currentMailbox() != inbox)
                    msgList->setMailbox(inbox);
                source = msgList;
            }
        }
        const QString name = m_accounts->displayName(accountId);
        m_unifiedModel->setAccountSource(accountId, name, source);
        if (m_unifiedModel->accountName(accountId) != name)
            m_unifiedModel->setAccountName(accountId, name);
    }
    updateUnifiedStates();
}

void MainWindow::updateUnifiedStates()
{
    if (!m_unifiedModel)
        return;
    using Common::AccountStatusKind;
    using State = Imap::Mailbox::UnifiedMsgListModel::AccountState;
    QStringList accountIds = m_unifiedModel->accountIds();
    if (m_searchResultsPanel)
        accountIds += m_searchResultsPanel->results()->accountIds();
    accountIds.removeDuplicates();
    for (const QString &accountId : std::as_const(accountIds)) {
        const Common::AccountStatus status = m_accountStatus->status(accountId);
        Imap::ImapAccess *access = m_accountAccess.value(accountId);
        State state = State::Ready;
        QString detail;
        if (!access || !access->isConfigured() || !access->imapModel()) {
            state = State::Unavailable;
        } else {
            switch (status.kind) {
            case AccountStatusKind::Idle:
            case AccountStatusKind::Connected:
                state = State::Ready;
                break;
            case AccountStatusKind::Connecting:
                state = State::Syncing;
                break;
            case AccountStatusKind::Offline:
                state = State::Offline;
                break;
            case AccountStatusKind::NetworkError:
            case AccountStatusKind::AuthRequired:
            case AccountStatusKind::AuthFailed:
                state = State::Failed;
                detail = status.detail.isEmpty() ? Common::AccountStatusTracker::label(status.kind) : status.detail;
                break;
            }
        }
        if (m_unifiedModel->accountIds().contains(accountId))
            m_unifiedModel->setAccountState(accountId, state, detail);
        if (m_searchResultsPanel && m_searchResultsPanel->results()->accountIds().contains(accountId))
            m_searchResultsPanel->results()->setAccountState(accountId, state, detail);
    }
    if (showsUnifiedList() || m_searchAcross) {
        updateMessageListScope();
        updateMessageFlags();
        scheduleMessageListStatus();
    }
}

void MainWindow::setUnifiedInbox(bool enabled, bool returnToFolder)
{
    if (!m_unifiedModel || !prettyMsgListModel || enabled == m_unifiedInbox) {
        if (m_actionUnifiedInbox && m_actionUnifiedInbox->isChecked() != m_unifiedInbox) {
            const QSignalBlocker blocker(m_actionUnifiedInbox);
            m_actionUnifiedInbox->setChecked(m_unifiedInbox);
        }
        return;
    }
    // A search of the list from before would look like it applies to the other list
    leaveSearchAcross();
    msgListWidget->clearSearch();
    if (enabled) {
        captureFolderPosition();
        commitFolderPosition();
        m_unifiedReturnMailbox = currentMailboxName();
        if (m_messageRestorer)
            m_messageRestorer->cancel();
        m_unifiedInbox = true;
        syncUnifiedSources();
        showUnifiedListModel();
        msgListWidget->setCurrentPlaceText(i18nc("@item search in", "Unified Inbox"));
        if (QItemSelectionModel *folders = mboxTree->selectionModel())
            folders->clearSelection();
    } else {
        m_unifiedInbox = false;
        showFolderListModel();
        msgListWidget->setCurrentPlaceText(QString());
        msgListWidget->setSearchAvailable(true);
        syncUnifiedSources();
        applyMessageListView();
        if (auto *threading = qobject_cast<Imap::Mailbox::ThreadingMsgListModel *>(m_imapAccess->threadingMsgListModel()))
            updateRelevanceSorting(threading->currentSearchCondition());
    }
    {
        const QSignalBlocker blocker(m_actionUnifiedInbox);
        m_actionUnifiedInbox->setChecked(m_unifiedInbox);
    }
    updateMessageListScope();
    updateMessageFlags();
    scheduleMessageListStatus();
    KConfigGroup state(KSharedConfig::openStateConfig(), unifiedInboxGroup());
    state.writeEntry("Enabled", m_unifiedInbox);
    if (!state.sync())
        qWarning() << "Could not save the state of the Unified Inbox";
    if (!enabled) {
        const QString target = m_unifiedReturnMailbox;
        m_unifiedReturnMailbox.clear();
        if (returnToFolder && !target.isEmpty() && target != currentMailboxName()) {
            goToMailbox(target);
        } else {
            restoreFolderPosition();
            // While the Unified Inbox was shown, choosing this folder was not remembered
            const QString mailbox = currentMailboxName();
            if (!mailbox.isEmpty()) {
                KConfigGroup selection(KSharedConfig::openStateConfig(), QStringLiteral("MailboxSelection"));
                KConfigGroup accountSelection = selection.group(m_imapAccess->credentialAccountId());
                accountSelection.writeEntry("Mailbox", mailbox);
                if (!accountSelection.sync())
                    qWarning() << "Could not save mailbox selection";
            }
        }
    }
}

bool MainWindow::showsUnifiedList() const
{
    return m_unifiedInbox || searchShownInList();
}

bool MainWindow::searchShownInList() const
{
    return m_searchAcross && !m_searchInPanel;
}

bool MainWindow::wantsSearchInPanel() const
{
    return m_searchResultsPanel && m_searchResultsPanel->isPanelEnabled()
        && msgListWidget->activeSavedSearchOwner().isEmpty();
}

Imap::Mailbox::UnifiedMsgListModel *MainWindow::searchModel() const
{
    return m_searchInPanel && m_searchResultsPanel ? m_searchResultsPanel->results() : m_unifiedModel;
}

Imap::Mailbox::PrettyUnifiedMsgListModel *MainWindow::searchPrettyModel() const
{
    return m_searchInPanel && m_searchResultsPanel ? m_searchResultsPanel->prettyResults() : m_prettyUnifiedModel;
}

void MainWindow::moveSearchResults()
{
    if (m_lastSearchConditions.isEmpty())
        return;
    const bool inPanel = wantsSearchInPanel();
    // A search of the shown folder alone stays in the list while the panel is off
    if (m_searchAcross ? m_searchInPanel != inPanel : inPanel)
        slotSearchRequested(m_lastSearchConditions);
}

void MainWindow::setFolderViewActionsEnabled(bool enabled)
{
    const QList<QAction *> folderViewActions = {m_groupByMenu ? m_groupByMenu->menuAction() : nullptr,
                                                m_showRelatedMenu ? m_showRelatedMenu->menuAction() : nullptr,
                                                m_actionRelatedSender, m_actionRelatedRecipient, m_actionRelatedSubject,
                                                m_actionRelatedList, m_actionSortByRelevance, m_actionViewForAllFolders,
                                                m_actionResetFolderView, m_actionExpandAllGroups, m_actionCollapseAllGroups};
    for (QAction *action : folderViewActions) {
        if (action)
            action->setEnabled(enabled);
    }
    if (m_groupByActions)
        m_groupByActions->setEnabled(enabled);
}

void MainWindow::showUnifiedListModel()
{
    MsgListView *tree = msgListWidget->tree;
    QHeaderView *header = tree->header();
    m_prettyUnifiedModel->setFilters(prettyMsgListModel->filters());
    swapMessageListModel(m_prettyUnifiedModel);
    tree->setRootIsDecorated(false);
    if (header->sortIndicatorSection() < 0)
        header->setSortIndicator(Imap::Mailbox::MsgListModel::DATE, Qt::DescendingOrder);
    slotSortingConfirmed(header->sortIndicatorSection(), header->sortIndicatorOrder());
    // Neither threads, nor groups, nor a search on the server span several folders
    m_unifiedThreadingEnabled = actionThreadMsgList->isEnabled();
    actionThreadMsgList->setEnabled(false);
    m_actionSortThreading->setEnabled(false);
    m_actionSortNone->setEnabled(true);
    setFolderViewActionsEnabled(false);
}

void MainWindow::showFolderListModel()
{
    using Imap::Mailbox::GroupingMsgListModel;
    MsgListView *tree = msgListWidget->tree;
    QHeaderView *header = tree->header();
    const bool grouped = GroupingMsgListModel::isRealGrouping(m_groupBy);
    {
        // The folder's own sorting, rather than the one of the unified list
        const UiUtils::MessageListView view = m_viewStore.effectiveView(currentMailboxName());
        const QSignalBlocker blocker(header);
        header->setSortIndicator(view.sortColumn, view.sortOrder);
    }
    swapMessageListModel(grouped ? static_cast<QAbstractItemModel *>(m_groupingMsgListModel) : prettyMsgListModel);
    tree->setRootIsDecorated(grouped || actionThreadMsgList->isChecked());
    actionThreadMsgList->setEnabled(m_unifiedThreadingEnabled);
    m_actionSortThreading->setEnabled(!grouped && actionThreadMsgList->isChecked());
    m_actionSortNone->setEnabled(!m_actionSortThreading->isEnabled());
    setFolderViewActionsEnabled(true);
}

namespace {

/** @short Select the account's shown folder again after a search selected other folders */
void reselectShownFolder(Imap::ImapAccess *access)
{
    auto *model = access ? qobject_cast<Imap::Mailbox::Model *>(access->imapModel()) : nullptr;
    auto *msgList = access ? qobject_cast<Imap::Mailbox::MsgListModel *>(access->msgListModel()) : nullptr;
    if (model && msgList && msgList->currentMailbox().isValid())
        model->switchToMailbox(msgList->currentMailbox());
}

}

void MainWindow::startSearchAcross(const QStringList &conditions)
{
    using namespace UiUtils::SearchScope;
    using Imap::Mailbox::FolderSearch;
    if (!m_unifiedModel || !m_prettyUnifiedModel || !prettyMsgListModel)
        return;
    const QString activeAccount = m_accounts->activeAccount();
    QList<Folder> shown;
    if (m_unifiedInbox) {
        for (const QString &accountId : unifiedAccountIds())
            shown << Folder{accountId, QStringLiteral("INBOX")};
    } else if (const QString mailbox = currentMailboxName(); !mailbox.isEmpty()) {
        shown << Folder{activeAccount, mailbox};
    }
    QList<Folder> selected;
    const QStringList selectedKeys = msgListWidget->selectedFolders();
    for (const QString &key : selectedKeys)
        selected << folderFromKey(key);
    const QList<AccountFolders> plan =
        foldersToSearch(msgListWidget->searchPlaces(), activeAccount, shown, selected, m_accounts->accountIds());

    const bool inPanel = wantsSearchInPanel();
    // The results move between the message list and the Search Results panel
    if (m_searchAcross && m_searchInPanel != inPanel)
        leaveSearchAcross();
    const bool entering = !m_searchAcross;
    if (inPanel) {
        // The folder shows all of its messages while the panel lists the results
        auto *threading = qobject_cast<Imap::Mailbox::ThreadingMsgListModel *>(m_imapAccess->threadingMsgListModel());
        if (threading && !threading->currentSearchCondition().isEmpty()) {
            threading->setUserSearchingSortingPreference(QStringList(), threading->currentSortCriterium(),
                                                         threading->currentSortOrder());
        }
        updateRelevanceSorting(QStringList());
    } else if (entering && !m_unifiedInbox) {
        captureFolderPosition();
        commitFolderPosition();
        if (m_messageRestorer)
            m_messageRestorer->cancel();
        // The folder's own search would still hide messages once the folder is shown again
        auto *threading = qobject_cast<Imap::Mailbox::ThreadingMsgListModel *>(m_imapAccess->threadingMsgListModel());
        if (threading && !threading->currentSearchCondition().isEmpty()) {
            threading->setUserSearchingSortingPreference(QStringList(), threading->currentSortCriterium(),
                                                         threading->currentSortOrder());
        }
    }
    cancelFolderSearches();
    m_searchAcross = true;
    m_searchInPanel = inPanel;
    m_searchProblems.clear();
    if (m_notificationBar && m_notificationBar->isVisible() && m_notificationBar->text() == m_searchProblemsNotice)
        m_notificationBar->dismiss();
    m_searchedPlaces.clear();
    Imap::Mailbox::UnifiedMsgListModel *results = searchModel();
    results->setShowsFolders(true);
    const QStringList listed = results->accountIds();
    for (const QString &accountId : listed)
        results->removeAccount(accountId);
    if (inPanel) {
        m_searchResultsPanel->prettyResults()->setFilters(prettyMsgListModel->filters());
        m_searchResultsPanel->setSearchShown(true, UiUtils::relevanceTerms(conditions));
    }

    m_searchConditions = conditions;
    m_searchPlan = plan;
    m_searchStopped = false;
    const QString separator = i18nc("@info:tooltip separator of folder names", ", ");
    for (const AccountFolders &folders : plan) {
        const QString name = m_accounts->displayName(folders.accountId);
        m_searchedPlaces << (folders.allFolders
                                 ? i18nc("@info:tooltip account name", "%1: all folders", name)
                                 : i18nc("@info:tooltip account name, list of folders", "%1: %2", name,
                                         folders.mailboxes.join(separator)));
        searchAccount(folders.accountId, folders.mailboxes, folders.allFolders, false);
    }
    updateUnifiedStates();
    if (entering && !m_unifiedInbox && !inPanel)
        showUnifiedListModel();
    updateUnifiedRelevance();
    updateMessageListScope();
    updateMessageFlags();
    scheduleMessageListStatus();
    if (!isSearchAcrossRunning())
        folderSearchFinished(QString());
}

void MainWindow::cancelFolderSearches()
{
    const auto searches = std::exchange(m_folderSearches, {});
    for (auto it = searches.cbegin(); it != searches.cend(); ++it) {
        Imap::Mailbox::FolderSearch *search = it.value();
        if (!search)
            continue;
        if (m_unifiedModel && m_unifiedModel->accountSource(it.key()) == search->results())
            m_unifiedModel->removeAccount(it.key());
        if (m_searchResultsPanel && m_searchResultsPanel->results()->accountSource(it.key()) == search->results())
            m_searchResultsPanel->results()->removeAccount(it.key());
        const bool running = search->isRunning();
        search->cancel();
        if (running)
            reselectShownFolder(m_accountAccess.value(it.key()));
        search->deleteLater();
    }
}

void MainWindow::searchAccount(const QString &accountId, const QStringList &mailboxes, bool allFolders, bool retrying)
{
    using Common::AccountStatusKind;
    using Imap::Mailbox::FolderSearch;
    const QString name = m_accounts->displayName(accountId);
    Imap::ImapAccess *access = m_accountAccess.value(accountId);
    auto *model = access && access->isConfigured() ? qobject_cast<Imap::Mailbox::Model *>(access->imapModel()) : nullptr;
    Common::AccountStatus status = m_accountStatus->status(accountId);
    Imap::Mailbox::SearchIndexer *indexer = access ? access->searchIndexer() : nullptr;
    if (retrying && model && !model->isNetworkAvailable() && status.kind == AccountStatusKind::NetworkError) {
        // Connect now instead of waiting for the next attempt to reconnect
        auto *watcher = qobject_cast<Imap::Mailbox::NetworkWatcher *>(access->networkWatcher());
        if (watcher && watcher->desiredNetworkPolicy() == Imap::Mailbox::NETWORK_ONLINE)
            watcher->setNetworkOnline();
        else if (watcher && watcher->desiredNetworkPolicy() == Imap::Mailbox::NETWORK_EXPENSIVE)
            watcher->setNetworkExpensive();
        status = m_accountStatus->status(accountId);
    }
    // Without a connection, the mail indexed on this computer is searched instead
    const bool local = model && !model->isNetworkAvailable() && indexer && indexer->isAvailable();
    // Searching on the server only leaves the index out while the server can be reached
    if (!local && model && model->isNetworkAvailable()
        && UiUtils::SettingsStore::value(*KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")), nullptr,
                                         QStringLiteral("Search/UseIndex"))
                           .toString()
                   == QLatin1String("server"))
        indexer = nullptr;
    QString problem;
    if (!model) {
        problem = i18nc("@info account name", "%1 is not set up", name);
    } else if (!local && (status.kind == AccountStatusKind::Offline || !model->isNetworkAvailable())) {
        problem = i18nc("@info account name", "%1 is offline", name);
    } else if (!local && access->protocol() == Imap::AccountProtocol::Imap
               && ((status.kind == AccountStatusKind::NetworkError && !retrying) || status.kind == AccountStatusKind::AuthRequired
                   || status.kind == AccountStatusKind::AuthFailed)) {
        problem = i18nc("@info account name, reason", "%1: %2", name,
                        status.detail.isEmpty() ? Common::AccountStatusTracker::label(status.kind) : status.detail);
    }
    if (!problem.isEmpty()) {
        addSearchProblem(accountId, QString(), problem);
        return;
    }
    // Both searches would select folders on the same connection
    if (m_relatedMessagesPanel && m_relatedMessagesPanel->serverSearch()->isRunning()
        && m_relatedMessagesPanel->serverSearch()->message().model() == model)
        m_relatedMessagesPanel->restartServerSearch();
    if (m_messageLookup && m_messageLookup->state() == UiUtils::MessageIdLookup::State::SearchingServers) {
        m_messageLookupStopped = true;
        m_messageLookup->cancel();
    }
    if (m_agendaSearch && m_agendaSearch->model() == model)
        cancelAgendaSearch();
    if (FolderSearch *search = m_folderSearches.value(accountId); search && search->model() == model) {
        search->setSearchIndex(indexer);
        search->retry(mailboxes, allFolders);
        return;
    }
    auto *search = new FolderSearch(model, this);
    search->setSearchIndex(indexer);
    search->results()->snippets()->setModel(model);
    search->results()->snippets()->setTerms(UiUtils::relevanceTerms(m_searchConditions));
    m_folderSearches.insert(accountId, search);
    connect(search, &FolderSearch::progressChanged, this, &MainWindow::scheduleMessageListStatus);
    connect(search, &FolderSearch::folderFailed, this, [this, accountId, name](const QString &mailbox, const QString &reason) {
        addSearchProblem(accountId, mailbox,
                         mailbox.isEmpty() ? i18nc("@info account name, reason", "%1: %2", name, reason)
                                           : i18nc("@info account name, folder, reason", "%1, %2: %3", name, mailbox, reason));
    });
    connect(search, &FolderSearch::finished, this, [this, accountId]() { folderSearchFinished(accountId); });
    searchModel()->setAccountSource(accountId, name, search->results());
    search->start(mailboxes, allFolders, m_searchConditions);
}

void MainWindow::addSearchProblem(const QString &accountId, const QString &mailbox, const QString &text)
{
    m_searchProblems.append({accountId, mailbox, text});
    scheduleMessageListStatus();
}

bool MainWindow::isSearchAcrossRunning() const
{
    return std::any_of(m_folderSearches.cbegin(), m_folderSearches.cend(),
                       [](const QPointer<Imap::Mailbox::FolderSearch> &search) { return search && search->isRunning(); });
}

void MainWindow::stopSearchAcross()
{
    if (!m_searchAcross || !isSearchAcrossRunning())
        return;
    for (auto it = m_folderSearches.cbegin(); it != m_folderSearches.cend(); ++it) {
        Imap::Mailbox::FolderSearch *search = it.value();
        if (!search || !search->isRunning())
            continue;
        search->cancel();
        reselectShownFolder(m_accountAccess.value(it.key()));
    }
    m_searchStopped = true;
    folderSearchFinished(QString());
}

void MainWindow::retrySearchAcross()
{
    if (!m_searchAcross || isSearchAcrossRunning() || m_searchProblems.isEmpty())
        return;
    const QList<SearchProblem> problems = std::exchange(m_searchProblems, {});
    if (m_notificationBar && m_notificationBar->isVisible() && m_notificationBar->text() == m_searchProblemsNotice)
        m_notificationBar->dismiss();
    m_searchStopped = false;
    for (const UiUtils::SearchScope::AccountFolders &planned : std::as_const(m_searchPlan)) {
        bool whole = false;
        QStringList mailboxes;
        for (const SearchProblem &problem : problems) {
            if (problem.accountId != planned.accountId)
                continue;
            if (problem.mailbox.isEmpty())
                whole = true;
            else if (!mailboxes.contains(problem.mailbox))
                mailboxes << problem.mailbox;
        }
        if (whole)
            searchAccount(planned.accountId, planned.mailboxes, planned.allFolders, true);
        else if (!mailboxes.isEmpty())
            searchAccount(planned.accountId, mailboxes, false, true);
    }
    updateUnifiedStates();
    updateMessageListScope();
    scheduleMessageListStatus();
    if (!isSearchAcrossRunning())
        folderSearchFinished(QString());
}

void MainWindow::continueSearchOnServer()
{
    if (!m_searchAcross || isSearchAcrossRunning())
        return;
    QStringList accounts;
    for (auto it = m_folderSearches.cbegin(); it != m_folderSearches.cend(); ++it) {
        if (it.value() && it.value()->canContinueOnServer())
            accounts << it.key();
    }
    if (accounts.isEmpty())
        return;
    // The server searches these places again, including those which could not be opened from the index
    m_searchProblems.erase(std::remove_if(m_searchProblems.begin(), m_searchProblems.end(),
                                          [&accounts](const SearchProblem &problem) {
                                              return accounts.contains(problem.accountId);
                                          }),
                           m_searchProblems.end());
    if (m_notificationBar && m_notificationBar->isVisible() && m_notificationBar->text() == m_searchProblemsNotice)
        m_notificationBar->dismiss();
    m_searchStopped = false;
    for (const QString &accountId : std::as_const(accounts)) {
        Imap::Mailbox::FolderSearch *search = m_folderSearches.value(accountId);
        // Both searches would select folders on the same connection
        if (m_relatedMessagesPanel && m_relatedMessagesPanel->serverSearch()->isRunning()
            && m_relatedMessagesPanel->serverSearch()->message().model() == search->model())
            m_relatedMessagesPanel->restartServerSearch();
        search->continueOnServer();
    }
    updateUnifiedStates();
    updateMessageListScope();
    scheduleMessageListStatus();
    if (!isSearchAcrossRunning())
        folderSearchFinished(QString());
}

QString MainWindow::searchProgressDetails() const
{
    QStringList lines;
    for (const UiUtils::SearchScope::AccountFolders &planned : std::as_const(m_searchPlan)) {
        const QString name = m_accounts->displayName(planned.accountId);
        const Imap::Mailbox::FolderSearch *search = m_folderSearches.value(planned.accountId);
        const bool failed = std::any_of(m_searchProblems.cbegin(), m_searchProblems.cend(), [&](const SearchProblem &problem) {
            return problem.accountId == planned.accountId && problem.mailbox.isEmpty();
        });
        if (!search) {
            lines << i18nc("@info:tooltip account name", "%1: could not be searched", name);
            continue;
        }
        const int found = search->results()->rowCount();
        const int folders = search->folderCount();
        const int searched = search->searchedCount();
        if (search->isRunning()) {
            const QString folder = search->currentFolder();
            lines << (folder.isEmpty()
                          ? i18nc("@info:tooltip account name", "%1: listing folders", name)
                          : i18nc("@info:tooltip account name, folder name, folders done, all folders, messages found",
                                  "%1: searching %2, %3 of %4 folders, %5 found", name, folder,
                                  std::min(searched + 1, std::max(folders, 1)), std::max(folders, 1), found));
        } else if (failed && found == 0 && searched == 0) {
            lines << i18nc("@info:tooltip account name", "%1: could not be searched", name);
        } else if (m_searchStopped && searched < folders) {
            lines << i18nc("@info:tooltip account name, folders done, all folders, messages found",
                           "%1: stopped after %2 of %3 folders, %4 found", name, searched, folders, found);
        } else if (search->usedLocalIndex()) {
            lines << i18ncp("@info:tooltip %2 is an account name, %1 the messages found",
                            "%2: %1 found in mail indexed on this computer",
                            "%2: %1 found in mail indexed on this computer", found, name);
        } else {
            lines << i18ncp("@info:tooltip %2 is an account name, %1 the messages found",
                            "%2: %1 found", "%2: %1 found", found, name);
        }
        if (!search->isRunning() && search->messagesWithoutText() > 0) {
            lines << i18ncp("@info:tooltip %2 is an account name",
                            "%2: the text of %1 message is not on this computer, so only its headers were searched",
                            "%2: the text of %1 messages is not on this computer, so only their headers were searched",
                            search->messagesWithoutText(), name);
        }
        const int excluded = int(search->excludedFolders().size());
        if (!search->isRunning() && search->incompleteFolders().size() > excluded) {
            lines << i18ncp("@info:tooltip %2 is an account name",
                            "%2: %1 folder is not fully indexed on this computer, so results may be incomplete",
                            "%2: %1 folders are not fully indexed on this computer, so results may be incomplete",
                            int(search->incompleteFolders().size()) - excluded, name);
        }
        if (!search->isRunning() && excluded > 0) {
            lines << i18ncp("@info:tooltip %2 is an account name",
                            "%2: %1 folder is kept out of the index, so only Search Server can search it",
                            "%2: %1 folders are kept out of the index, so only Search Server can search them",
                            excluded, name);
        }
        if (!search->isRunning() && !search->discardedFolders().isEmpty()) {
            lines << i18ncp("@info:tooltip %2 is an account name",
                            "%2: %1 folder was renumbered on the server since it was indexed, so its indexed matches "
                            "were left out",
                            "%2: %1 folders were renumbered on the server since they were indexed, so their indexed "
                            "matches were left out",
                            int(search->discardedFolders().size()), name);
        }
        if (!search->isRunning() && search->canContinueOnServer()) {
            lines << i18nc("@info:tooltip account name", "%1: Search Server adds the messages found on the server",
                           name);
        }
        if (!search->isRunning() && search->indexPaused()) {
            lines << i18nc("@info:tooltip account name", "%1: indexing is paused, so recent changes may be missing",
                           name);
        } else if (!search->isRunning() && search->indexPending()) {
            lines << i18nc("@info:tooltip account name", "%1: the index was still being updated",
                           name);
        }
    }
    if (!m_searchProblems.isEmpty()) {
        lines << i18nc("@info:tooltip followed by places and reasons, one per line", "Could not be searched:");
        for (const SearchProblem &problem : std::as_const(m_searchProblems))
            lines << problem.text;
    }
    return lines.join(QLatin1Char('\n'));
}

void MainWindow::folderSearchFinished(const QString &accountId)
{
    if (!accountId.isEmpty())
        reselectShownFolder(m_accountAccess.value(accountId));
    scheduleMessageListStatus();
    resumeRelatedSearch();
    if (isSearchAcrossRunning() || m_searchProblems.isEmpty() || !m_notificationBar)
        return;
    // A long list of folders would fill the window
    constexpr int shownProblems = 5;
    QStringList lines;
    for (const SearchProblem &problem : std::as_const(m_searchProblems)) {
        if (lines.size() == shownProblems && m_searchProblems.size() > shownProblems + 1) {
            lines << i18ncp("@info", "and %1 more", "and %1 more", int(m_searchProblems.size()) - shownProblems);
            break;
        }
        lines << problem.text;
    }
    m_searchProblemsNotice = i18ncp("@info %2 is a list of places and reasons, one per line",
                                    "This place could not be searched:\n%2", "These places could not be searched:\n%2",
                                    int(m_searchProblems.size()), lines.join(QLatin1Char('\n')));
    m_notificationBar->showError(m_searchProblemsNotice,
                                 {{i18nc("@action:button", "Retry"), QStringLiteral("view-refresh"),
                                   QStringLiteral("searchRetry"), [this]() { retrySearchAcross(); }}});
}

void MainWindow::leaveSearchAcross()
{
    if (!m_searchAcross)
        return;
    cancelFolderSearches();
    resumeRelatedSearch();
    m_searchAcross = false;
    m_searchProblems.clear();
    m_searchedPlaces.clear();
    m_searchConditions.clear();
    m_searchPlan.clear();
    m_searchStopped = false;
    if (m_notificationBar && m_notificationBar->isVisible() && m_notificationBar->text() == m_searchProblemsNotice)
        m_notificationBar->dismiss();
    if (std::exchange(m_searchInPanel, false)) {
        // The message list kept showing its folder
        if (m_searchResultsPanel) {
            Imap::Mailbox::UnifiedMsgListModel *results = m_searchResultsPanel->results();
            const QStringList listed = results->accountIds();
            for (const QString &accountId : listed)
                results->removeAccount(accountId);
            m_searchResultsPanel->setStatus(QString(), QString());
            m_searchResultsPanel->setSearchActions(false, false, false);
            m_searchResultsPanel->setSearchShown(false);
        }
        applySearchFacets();
        updateMessageListScope();
        scheduleMessageListStatus();
        return;
    }
    if (m_unifiedModel) {
        m_unifiedModel->setShowsFolders(false);
        const QStringList listed = m_unifiedModel->accountIds();
        for (const QString &accountId : listed)
            m_unifiedModel->removeAccount(accountId);
    }
    if (m_unifiedInbox) {
        syncUnifiedSources();
        updateUnifiedRelevance();
    } else {
        updateUnifiedRelevance();
        showFolderListModel();
        applyMessageListView();
        if (auto *threading = qobject_cast<Imap::Mailbox::ThreadingMsgListModel *>(m_imapAccess->threadingMsgListModel()))
            updateRelevanceSorting(threading->currentSearchCondition());
        restoreFolderPosition();
    }
    updateMessageListScope();
    updateMessageFlags();
    scheduleMessageListStatus();
}

void MainWindow::chooseSearchFolders()
{
    auto *dialog = new SearchFoldersDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    addSearchFolderAccounts(dialog);
    dialog->setSelectedFolders(msgListWidget->selectedFolders());
    connect(dialog, &QDialog::accepted, this, [this, dialog]() { msgListWidget->setSelectedFolders(dialog->selectedFolders()); });
    dialog->open();
}

void MainWindow::chooseRelatedFolders(const QString &accountId)
{
    if (!m_relatedMessagesPanel)
        return;
    const QString shownAccount = accountId.isEmpty() ? m_accounts->activeAccount() : accountId;
    auto *dialog = new SearchFoldersDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(i18nc("@title:window", "Search Related Messages In"));
    Imap::ImapAccess *access = m_accountAccess.value(shownAccount);
    QAbstractItemModel *mailboxes = nullptr;
    if (access == m_imapAccess)
        mailboxes = prettyMboxModel;
    else if (access && access->mailboxModel())
        mailboxes = new Imap::Mailbox::PrettyMailboxModel(dialog, access->mailboxModel());
    dialog->addAccount(shownAccount, m_accounts->displayName(shownAccount), mailboxes);
    // Folders of the other accounts are kept as they are
    dialog->setSelectedFolders(m_relatedMessagesPanel->chosenFolders());
    connect(dialog, &QDialog::accepted, this, [this, dialog]() {
        m_relatedMessagesPanel->setChosenFolders(dialog->selectedFolders());
        m_relatedMessagesPanel->setServerScope(UiUtils::RelatedSearch::Scope::ChosenFolders);
    });
    dialog->open();
}

void MainWindow::openMessageUrl(const QUrl &url)
{
    openMessageById(url.toString(QUrl::FullyEncoded));
}

void MainWindow::openMessageById(const QString &messageId)
{
    using UiUtils::MessageId::Problem;
    using Lookup = UiUtils::MessageIdLookup;
    showMainWindow();
    if (!m_notificationBar)
        return;
    const UiUtils::MessageId::Parsed parsed = UiUtils::MessageId::parse(messageId);
    QString problem;
    switch (parsed.problem) {
    case Problem::None:
        break;
    case Problem::Empty:
        problem = i18nc("@info", "No Message-ID was given.");
        break;
    case Problem::InvalidCharacter:
        problem = i18nc("@info", "A Message-ID cannot contain spaces, control characters or angle brackets inside it.");
        break;
    case Problem::MissingAt:
        problem = i18nc("@info", "A Message-ID has an \"@\" with text on both sides.");
        break;
    case Problem::TooLong:
        problem = i18nc("@info", "The Message-ID is too long to come from a message.");
        break;
    }
    if (!problem.isEmpty()) {
        m_notificationBar->showError(i18nc("@info %1 is the reason", "Cannot open the message: %1", problem));
        return;
    }
    if (!m_messageLookup) {
        m_messageLookup = new Lookup(this);
        connect(m_messageLookup, &Lookup::finished, this, &MainWindow::messageLookupFinished);
        connect(m_messageLookup, &Lookup::selectionChanged, this,
                [this](Imap::Mailbox::Model *model) { reselectShownFolder(m_accountAccess.value(accountOfModel(model))); });
    }
    m_messageLookupStopped = false;
    QList<Lookup::Account> accounts;
    const QStringList accountIds = m_accounts->accountIds();
    for (const QString &accountId : accountIds) {
        Imap::ImapAccess *access = m_accountAccess.value(accountId);
        if (!access || !access->isConfigured())
            continue;
        accounts << Lookup::Account{accountId, qobject_cast<Imap::Mailbox::Model *>(access->imapModel()), access->searchIndexer()};
    }
    if (m_messageLookup->start(parsed.messageId, accounts) != Lookup::State::NeedsServer)
        return;
    m_notificationBar->showInfo(
        i18nc("@info %1 is a Message-ID", "The message \"%1\" is not on this computer. Search every folder on the mail servers for it?",
              QString::fromUtf8(parsed.messageId)),
        {{i18nc("@action:button", "Search Servers"), QStringLiteral("network-server"), QStringLiteral("searchServersForMessage"),
          [this]() { searchServersForMessage(); }}});
}

QList<UiUtils::MailStatus::Account> MainWindow::mailStatusAccounts() const
{
    namespace MailStatus = UiUtils::MailStatus;
    using Kind = Common::AccountStatusKind;
    QList<MailStatus::Account> result;
    const QStringList accountIds = m_accounts->accountIds();
    for (const QString &accountId : accountIds) {
        MailStatus::Account account;
        account.id = accountId;
        account.name = m_accounts->displayName(accountId);
        account.unread = m_inboxUnread.value(accountId, -1);
        switch (m_accountStatus->status(accountId).kind) {
        case Kind::Idle:
            account.state = MailStatus::State::Idle;
            break;
        case Kind::Connecting:
            account.state = MailStatus::State::Connecting;
            break;
        case Kind::Connected:
            account.state = MailStatus::State::Online;
            break;
        case Kind::Offline:
            account.state = MailStatus::State::Offline;
            break;
        case Kind::NetworkError:
            account.state = MailStatus::State::Failed;
            break;
        case Kind::AuthRequired:
        case Kind::AuthFailed:
            account.state = MailStatus::State::SignInNeeded;
            break;
        }
        result << account;
    }
    return result;
}

QList<UiUtils::MailStatus::Message> MainWindow::mailStatusUnread(const QStringList &accounts, const QStringList &folders,
                                                                 int limit) const
{
    namespace MailStatus = UiUtils::MailStatus;
    QList<MailStatus::Source> sources;
    const QStringList accountIds = m_accounts->accountIds();
    for (const QString &accountId : accountIds) {
        Imap::ImapAccess *access = m_accountAccess.value(accountId);
        if (!access || !access->isConfigured())
            continue;
        // As for Plasma Search, an account whose index is off gives only its count
        const Imap::Mailbox::SearchIndexer *indexer = access->searchIndexer();
        if (indexer && indexer->isAvailable() && indexer->index())
            sources << MailStatus::Source{accountId, m_accounts->displayName(accountId), indexer->index()};
    }
    auto messages = MailStatus::unreadMessages(sources, accounts, folders, limit);
    for (auto &message : messages) {
        if (message.sender.isEmpty())
            message.sender = i18nc("@item a message without a sender", "(Unknown sender)");
        if (message.subject.isEmpty())
            message.subject = i18nc("@item a message without a subject", "(No subject)");
    }
    return messages;
}

void MainWindow::showAccountFromWidget(const QString &accountId)
{
    if (m_accounts->accountIds().contains(accountId))
        switchAccount(accountId);
    showMainWindow();
}

QList<UiUtils::PlasmaSearch::Match> MainWindow::plasmaSearchMatches(const QString &query) const
{
    namespace PlasmaSearch = UiUtils::PlasmaSearch;
    if (!PlasmaSearch::isEnabled(*KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"))))
        return {};
    QList<PlasmaSearch::Source> sources;
    const QStringList accountIds = m_accounts->accountIds();
    for (const QString &accountId : accountIds) {
        Imap::ImapAccess *access = m_accountAccess.value(accountId);
        if (!access || !access->isConfigured())
            continue;
        // An account whose index is off has no indexer, and its mail is never offered
        const Imap::Mailbox::SearchIndexer *indexer = access->searchIndexer();
        if (indexer && indexer->isAvailable() && indexer->index())
            sources << PlasmaSearch::Source{accountId, indexer->index()};
    }
    auto matches = PlasmaSearch::matches(query, sources);
    for (auto &match : matches) {
        match.title = match.subject.isEmpty() ? i18nc("@item a message without a subject", "(No subject)") : match.subject;
        const QString sender = match.sender.isEmpty() ? i18nc("@item a message without a sender", "(Unknown sender)") : match.sender;
        match.subtext = match.sent.isValid()
            ? i18nc("@item %1 is the sender of a message, %2 the date it was sent", "%1, %2", sender,
                    QLocale().toString(match.sent, QLocale::ShortFormat))
            : sender;
    }
    return matches;
}

void MainWindow::openPlasmaSearchMatch(const QString &matchId)
{
    UiUtils::PlasmaSearch::Target target;
    if (!UiUtils::PlasmaSearch::parseMatchId(matchId, &target) || !m_accounts->accountIds().contains(target.accountId))
        return;
    showMainWindow();
    revealLocation(target.accountId, target.mailbox, target.uidValidity, target.uid);
}

void MainWindow::searchServersForMessage()
{
    if (!m_messageLookup || m_messageLookup->state() != UiUtils::MessageIdLookup::State::NeedsServer)
        return;
    // These searches would select folders on the same connections
    if (isSearchAcrossRunning())
        stopSearchAcross();
    if (m_relatedMessagesPanel && m_relatedMessagesPanel->serverSearch()->isRunning())
        m_relatedMessagesPanel->restartServerSearch();
    cancelAgendaSearch();
    if (!m_messageLookup->searchServers())
        return;
    m_notificationBar->showMessage(
        InlineNotificationBar::Kind::Busy,
        i18nc("@info %1 is a Message-ID", "Searching the mail servers for the message \"%1\"...",
              QString::fromUtf8(m_messageLookup->messageId())),
        {{i18nc("@action:button", "Stop"), QStringLiteral("process-stop"), QStringLiteral("stopMessageSearch"), [this]() {
              m_messageLookupStopped = true;
              m_messageLookup->cancel();
          }}});
}

void MainWindow::messageLookupFinished()
{
    const QString messageId = QString::fromUtf8(m_messageLookup->messageId());
    const auto found = m_messageLookup->found();
    if (m_messageLookup->state() == UiUtils::MessageIdLookup::State::Found && !found.isEmpty()) {
        if (m_notificationBar->isBusy())
            m_notificationBar->dismiss();
        const auto &first = found.constFirst();
        revealLocation(first.accountId, first.mailbox, first.uidValidity, first.uid);
        if (found.size() > 1) {
            QStringList others;
            for (auto it = found.cbegin() + 1; it != found.cend(); ++it) {
                others << (it->accountId == first.accountId
                               ? it->mailbox
                               : i18nc("@item account name, folder", "%1: %2", m_accounts->displayName(it->accountId), it->mailbox));
            }
            others.removeDuplicates();
            m_notificationBar->showInfo(i18ncp("@info %2 lists folders", "The message is also in this folder: %2",
                                               "The message is also in these folders: %2", others.size(),
                                               others.join(i18nc("@item separator of folders", ", "))));
        }
        return;
    }
    QString text = m_messageLookupStopped
        ? i18nc("@info %1 is a Message-ID", "The search for the message \"%1\" was stopped.", messageId)
        : i18nc("@info %1 is a Message-ID", "No message with the Message-ID \"%1\" was found.", messageId);
    const QStringList failures = m_messageLookup->failures();
    if (!failures.isEmpty()) {
        text += QLatin1Char(' ')
            + i18nc("@info %1 lists folders and reasons", "These could not be searched: %1",
                    failures.join(i18nc("@item separator of failures", "; ")));
    }
    m_notificationBar->showError(text);
}

void MainWindow::resumeRelatedSearch()
{
    if (m_relatedMessagesPanel && m_relatedMessagesPanel->isServerSearchBlocked() && !isSearchAcrossRunning())
        m_relatedMessagesPanel->restartServerSearch();
}

void MainWindow::desktopGeometryChanged()
{
    m_delayedStateSaving->start();
}

QString MainWindow::settingsKeyForLayout(const LayoutMode layout)
{
    switch (layout) {
    case LAYOUT_COMPACT:
        return Common::SettingsNames::guiSizesInMainWinWhenCompact;
    case LAYOUT_WIDE:
        return Common::SettingsNames::guiSizesInMainWinWhenWide;
    case LAYOUT_ONE_AT_TIME:
        return Common::SettingsNames::guiSizesInaMainWinWhenOneAtATime;
        break;
    }
    return QString();
}

void MainWindow::saveSizesAndState()
{
    if (m_skipSavingOfUI)
        return;

    QRect geometry = screen()->availableGeometry();
    QString key = settingsKeyForLayout(m_layoutMode);
    if (key.isEmpty())
        return;

    m_settings->setValue(Common::SettingsNames::settingsVersion, 2);

    m_settings->beginGroup(key.arg(QString::number(geometry.width())));

    m_settings->setValue(Common::SettingsNames::geometry, saveGeometry());
    m_settings->setValue(Common::SettingsNames::windowState, saveState());

    rememberShellExtents();
    m_settings->setValue(QStringLiteral("shellLeftColumnWidth"), m_shellLeft.extent);
    m_settings->setValue(QStringLiteral("shellRightColumnWidth"), m_shellRight.extent);
    m_settings->setValue(QStringLiteral("shellFooterHeight"), m_shellFooter.extent);
    m_settings->setValue(QStringLiteral("shellMainColumnState"), m_mainColumn->saveState());
    // Only sizes picked by moving a divider are kept; the others follow the panels' size hints.
    for (EmbeddedPanel *panel : std::as_const(m_embeddedPanels)) {
        const QString key = QStringLiteral("embeddedPanel%1Extent").arg(panel->descriptor().id);
        if (const int extent = m_panelExtents.value(panel, 0); extent > 0)
            m_settings->setValue(key, extent);
        else
            m_settings->remove(key);
    }
    m_settings->setValue(QStringLiteral("mailboxSidebarOnRight"), m_mailboxSidebarOnRight);

    // The header keeps the sorting of all accounts rather than that of the open folder
    if (m_globalSort) {
        QHeaderView *header = msgListWidget->tree->header();
        m_settings->setValue(Common::SettingsNames::messageListState,
                             headerStateWithSort(header->saveState(), header->count(), m_globalSort->first, m_globalSort->second));
    }
    m_settings->beginWriteArray(Common::SettingsNames::messageListHeader);
    for (auto i = 0; i < msgListWidget->tree->header()->count(); ++i) {
        m_settings->setArrayIndex(i);
        m_settings->setValue(Common::SettingsNames::columnWidth,
                             QByteArray::number(msgListWidget->tree->header()->sectionSize(i)));
    }
    m_settings->endArray();

    m_settings->setValue(Common::SettingsNames::menubarVisible, menuBar()->isVisible());

    m_settings->endGroup();
}

void MainWindow::flushUiState()
{
    m_delayedStateSaving->stop();
    commitFolderPosition();
    saveSizesAndState();
    m_settings->sync();
    if (m_settings->status() != QSettings::NoError)
        qWarning() << "Could not save window layout";
}

void MainWindow::saveRawStateSetting(bool enabled)
{
    m_settings->setValue(Common::SettingsNames::guiAllowRawSearch, enabled);
}

void MainWindow::applySizesAndState()
{
    QRect geometry = screen()->availableGeometry();
    QString key = settingsKeyForLayout(m_layoutMode);
    if (key.isEmpty())
        return;

    bool versionOk;
    const int version = m_settings->value(Common::SettingsNames::settingsVersion, 0).toInt(&versionOk);

    if (!versionOk  || version < 1) {
        convertSizesAndStateSettings();
    }

    // There are slots connected to various events triggered by both restoreGeometry() and restoreState() which would attempt to
    // save our geometries and state, which is what we must avoid while this method is executing.
    bool skipSaving = m_skipSavingOfUI;
    m_skipSavingOfUI = true;

    m_settings->beginGroup(key.arg(QString::number(geometry.width())));
    const auto g = m_settings->value(Common::SettingsNames::geometry).toByteArray();

    // https://bugreports.qt-project.org/browse/QTBUG-30636
    if (windowState() & Qt::WindowMaximized) {
        // restoreGeometry(.) restores the wrong size for at least maximized window
        // However, QWidget does also not notice that the configure request for this
        // is ignored by many window managers (because users really don't like when windows
        // drop themselves out of maximization) and has a wrong QWidget::geometry() idea from
        // the wrong assumption the request would have been hononred.
        //  So we just "fix" the internal geometry immediately afterwards to prevent
        // mislayouting
        // There's atm. no flicker due to this (and because Qt compresses events)
        // In case it ever occurs, we can frame this in setUpdatesEnabled(false/true)
        QRect oldGeometry = MainWindow::geometry();
        restoreGeometry(g);
        if (windowState() & Qt::WindowMaximized)
            setGeometry(oldGeometry);
        } else {
            restoreGeometry(g);
        if (windowState() & Qt::WindowMaximized) {
            // ensure to try setting the proper geometry and have the WM constrain us
            setGeometry(QGuiApplication::primaryScreen()->availableGeometry());
        }
    }

    restoreState(m_settings->value(Common::SettingsNames::windowState).toByteArray());

    m_panelExtents.clear();
    for (EmbeddedPanel *panel : std::as_const(m_embeddedPanels)) {
        bool ok = false;
        const int extent = m_settings->value(QStringLiteral("embeddedPanel%1Extent").arg(panel->descriptor().id)).toInt(&ok);
        if (ok && extent > 0)
            m_panelExtents.insert(panel, extent);
    }

    {
        const QList<int> legacyMainSizes = migrateLegacyPaneSizes();
        const bool savedOnRight = m_settings->value(QStringLiteral("mailboxSidebarOnRight"), false).toBool();
        const bool swapSides = savedOnRight != m_mailboxSidebarOnRight;
        auto readExtent = [this](const QString &key, int &extent) {
            bool ok = false;
            const int value = m_settings->value(key).toInt(&ok);
            if (ok && value > 0)
                extent = value;
        };
        readExtent(QStringLiteral("shellLeftColumnWidth"), swapSides ? m_shellRight.extent : m_shellLeft.extent);
        readExtent(QStringLiteral("shellRightColumnWidth"), swapSides ? m_shellLeft.extent : m_shellRight.extent);
        readExtent(QStringLiteral("shellFooterHeight"), m_shellFooter.extent);
        const Qt::Orientation mainOrientation = m_mainColumn->orientation();
        const QByteArray mainState = m_settings->value(QStringLiteral("shellMainColumnState")).toByteArray();
        if (!mainState.isEmpty())
            m_mainColumn->restoreState(mainState);
        else if (!legacyMainSizes.isEmpty())
            m_mainColumn->setSizes(legacyMainSizes);
        m_mainColumn->setOrientation(mainOrientation);
        updateShellRegions(false, true);
    }

    QHeaderView *listHeader = msgListWidget->tree->header();
    QByteArray s = m_settings->value(Common::SettingsNames::messageListState).toByteArray();
    const bool firstHeaderRestore = !m_globalSort;
    if (firstHeaderRestore) {
        // The saved sorting is the default of all accounts and folders
        m_globalSort = headerStateSort(s, listHeader->count());
    } else {
        // The list keeps the sorting of the open folder
        s = headerStateWithSort(s, listHeader->count(), listHeader->sortIndicatorSection(), listHeader->sortIndicatorOrder());
    }
    listHeader->restoreState(s);
    if (firstHeaderRestore) {
        UiUtils::MessageListView global = m_viewStore.globalDefault();
        global.sortColumn = m_globalSort->first;
        global.sortOrder = m_globalSort->second;
        m_viewStore.setGlobalDefault(global);
        applyMessageListView();
    }
    // got to manually update the state of the actions which control the visibility state
    msgListWidget->tree->updateActionsAfterRestoredState();
    connect(msgListWidget->tree->header(), &QHeaderView::sectionCountChanged, msgListWidget->tree, &MsgListView::slotHandleNewColumns);

    const auto size = m_settings->beginReadArray(Common::SettingsNames::messageListHeader);
    for (auto i = 0; i < size; ++i) {
        m_settings->setArrayIndex(i);
        bool ok = false;
        const auto sectionSize = m_settings->value(Common::SettingsNames::columnWidth).toInt(&ok);
        if (!ok) {
            continue;
        }

        QHeaderView::ResizeMode resizeMode = msgListWidget->tree->resizeModeForColumn(i);
        if (sectionSize > 0 && resizeMode == QHeaderView::Interactive) {
            // fun fact: user cannot resize by mouse when size <= 0
            msgListWidget->tree->setColumnWidth(i, sectionSize);
        } else {
            msgListWidget->tree->setColumnWidth(i, msgListWidget->tree->sizeHintForColumn(i));
        }
        msgListWidget->tree->header()->setSectionResizeMode(i, resizeMode);

    }
    m_settings->endArray();

    const auto v = m_settings->value(Common::SettingsNames::menubarVisible).toBool();
    menuBar()->setVisible(v);
    showMenuBar->setChecked(v);

    m_settings->endGroup();

    m_skipSavingOfUI = skipSaving;
}

QString MainWindow::layoutToString(const LayoutMode layout) const
{
    switch (layout) {
    case LAYOUT_COMPACT:
        return QStringLiteral("Compact");
    case LAYOUT_WIDE:
        return QStringLiteral("Wide");
    case LAYOUT_ONE_AT_TIME:
        return QStringLiteral("One At a Time");
    }

    Q_ASSERT(false);
    return QString();
}

void MainWindow::convertSizesAndStateSettings()
{
    convertSizesAndStateSetting(LAYOUT_COMPACT);
    convertSizesAndStateSetting(LAYOUT_WIDE);
    convertSizesAndStateSetting(LAYOUT_ONE_AT_TIME);
    m_settings->sync();
}

void MainWindow::convertSizesAndStateSetting(const LayoutMode layout)
{
    QRect geometry = screen()->availableGeometry();
    QString key = settingsKeyForLayout(layout);
    if (key.isEmpty())
        return;

    QByteArray buf = m_settings->value(key.arg(QString::number(geometry.width()))).toByteArray();
    if (buf.isEmpty())
        return;

    qWarning() << "Starting conversion of layout settings for:" << layoutToString(layout);

    int size;
    QDataStream stream(&buf, QIODevice::ReadOnly);
    stream >> size;
    QByteArray item;

    m_settings->beginGroup(key.arg(QString::number(geometry.width())));

    if (size-- && !stream.atEnd()) {
        stream >> item;
        m_settings->setValue(Common::SettingsNames::geometry, item);
    }

    if (size-- && !stream.atEnd()) {
        stream >> item;
        m_settings->setValue(Common::SettingsNames::windowState, item);
    }

    if (size-- && !stream.atEnd()) {
        stream >> item;
        m_settings->setValue(Common::SettingsNames::mainVSplitterState, item);
    }

    if (size-- && !stream.atEnd()) {
        stream >> item;
        m_settings->setValue(Common::SettingsNames::mainHSplitterState, item);
    }

    if (size-- && !stream.atEnd()) {
        stream >> item;
        m_settings->setValue(Common::SettingsNames::messageListState, item);
    }

    if (size-- && !stream.atEnd()) {
        stream >> item;
        bool ok;
        int columns = item.toInt(&ok);
        if (ok) {
            m_settings->beginWriteArray(Common::SettingsNames::messageListHeader);
             for (int i = 0; i < columns && size-- && !stream.atEnd(); ++i) {
                m_settings->setArrayIndex(i);
                stream >> item;
                bool ok = false;
                const int sectionSize = item.toInt(&ok);
                m_settings->setValue(Common::SettingsNames::columnWidth,
                                     ok ? sectionSize : msgListWidget->tree->sizeHintForColumn(i));
            }
            m_settings->endArray();
        }
    }

    if (size-- && !stream.atEnd()) {
        stream >> item;
        bool ok;
        bool visibility = item.toInt(&ok);
        if (ok) {
            m_settings->setValue(Common::SettingsNames::menubarVisible, visibility);
        }
    }

    m_settings->endGroup();
    qWarning() << "Finished conversion of layout settings for:" << layoutToString(layout);
}

/** @short Carry the pane sizes of builds before the shell over to the shell, in the current layout's group

Those builds kept the folder tree and the mail panes in one horizontal splitter, and the compact layout split the
message list and the reader in a vertical one. Returns the message list and reader sizes for the main column, or an
empty list when there is nothing to carry over.
*/
QList<int> MainWindow::migrateLegacyPaneSizes()
{
    const QString outerKey = Common::SettingsNames::mainHSplitterState;
    const QString innerKey = Common::SettingsNames::mainVSplitterState;
    if (!m_settings->contains(outerKey) && !m_settings->contains(innerKey))
        return {};
    QList<int> outer = savedSplitterSizes(m_settings->value(outerKey).toByteArray());
    const QList<int> inner = savedSplitterSizes(m_settings->value(innerKey).toByteArray());
    m_settings->remove(outerKey);
    m_settings->remove(innerKey);
    // Sizes the shell saved since then win
    if (m_settings->contains(QStringLiteral("shellMainColumnState")))
        return {};

    // The folder tree was the first pane, or the last one on the side recorded in the same group
    const bool treeOnRight = m_settings->value(QStringLiteral("mailboxSidebarOnRight"), false).toBool();
    const int tree = outer.isEmpty() ? 0 : (treeOnRight ? outer.takeLast() : outer.takeFirst());
    if (tree > 0)
        m_settings->setValue(treeOnRight ? QStringLiteral("shellRightColumnWidth") : QStringLiteral("shellLeftColumnWidth"), tree);

    const QList<int> &mailPanes = m_layoutMode == LAYOUT_COMPACT ? inner : outer;
    if (m_layoutMode == LAYOUT_ONE_AT_TIME || mailPanes.size() != 2 || mailPanes.at(0) <= 0 || mailPanes.at(1) <= 0)
        return {};
    return mailPanes;
}

void MainWindow::resizeEvent(QResizeEvent *)
{
    m_delayedStateSaving->start();
}

/** @short Make sure that the message gets loaded after the splitters have changed their position */
void MainWindow::possiblyLoadMessageOnSplittersChanged()
{
#ifdef IMPRESARIO_HAVE_WEBENGINE
    if (m_messageWidget->isVisible() && !m_messageWidget->size().isEmpty()) {
        // We do not have to check whether it's a different message; the setMessage() will do this or us
        // and there are multiple proxy models involved anyway
        QModelIndex index = msgListWidget->tree->currentIndex();
        if (index.isValid() && !index.data(Imap::Mailbox::RoleIsGroupHeader).toBool()) {
            // OTOH, setting an invalid QModelIndex would happily assert-fail
            m_messageWidget->messageView->setMessage(msgListWidget->tree->currentIndex());
        }
    }
#endif
}

Imap::ImapAccess *MainWindow::imapAccess() const
{
    return currentAccess();
}

void MainWindow::enableLoggingToDisk()
{
    protocolLogger->slotSetPersistentLogging(true);
}

void MainWindow::slotPluginsChanged()
{
    Plugins::AddressbookPlugin *addressbook = pluginManager()->addressbook();
    if (!addressbook || !(addressbook->features() & Plugins::AddressbookPlugin::FeatureAddressbookWindow))
        m_actionContactEditor->setEnabled(false);
    else
        m_actionContactEditor->setEnabled(true);
    if (m_contactPanel)
        m_contactPanel->setAddressBook(addressbook);
}

/** @short Update the default action to make sure that we show a correct status of the network connection */
void MainWindow::updateNetworkIndication()
{
    if (QAction *action = qobject_cast<QAction*>(sender())) {
        if (action->isChecked()) {
            m_netToolbarDefaultAction->setIcon(action->icon());
        }
    }
}

void MainWindow::updateConnectionStatus(const QString &message)
{
    m_connectionError = false;
    m_connectionStatus->setText(message);
    m_connectionStatus->setToolTip(QString());
    m_connectionStatus->setAccessibleDescription(QString());
}

void MainWindow::showStatusMessage(const QString &message)
{
    networkIndicator->setToolTip(message);
    networkIndicator->setAccessibleDescription(message);
    if (isActiveWindow())
        QToolTip::showText(networkIndicator->mapToGlobal(QPoint(0, 0)), message);
}

void MainWindow::slotMessageModelChanged(QAbstractItemModel *model)
{
    mailMimeTree->setModel(model);
}

void MainWindow::slotFavoriteTagsChanged()
{
    for (int i = 1; i <= m_favoriteTags->rowCount(); ++i) {
        QAction *action = ShortcutHandler::instance()->action(QStringLiteral("action_tag_") + QString::number(i));
        if (action)
            action->setText(tr("Tag with \"%1\"").arg(m_favoriteTags->tagNameByIndex(i - 1)));
    }
}

void MainWindow::registerComposeWindow(ComposeWidget* widget)
{
    connect(widget, &ComposeWidget::logged, this, [this](const Common::LogKind kind, const QString& source, const QString& message) {
        protocolLogger->log(0, Common::LogMessage(QDateTime::currentDateTime(), kind, source, message, 0));
    });
}

}
