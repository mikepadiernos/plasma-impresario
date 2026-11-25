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
#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QInputDialog>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QHeaderView>
#include <QItemSelectionModel>
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
#include <QSplitter>
#include <QSslError>
#include <QSslKey>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTimer>
#include <QTextDocument>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include "configure.cmake.h"
#ifdef TROJITA_HAVE_KNOTIFICATIONS
#include <KNotification>
#endif
#include "Common/Application.h"
#include "Common/InvokeMethod.h"
#include "Common/Paths.h"
#include "Common/PortNumbers.h"
#include "Common/SettingsNames.h"
#include "Composer/AccountIdentities.h"
#include "Composer/Mailto.h"
#include "Composer/SenderIdentitiesModel.h"
#ifdef TROJITA_HAVE_CRYPTO_MESSAGES
#  ifdef TROJITA_HAVE_GPGMEPP
#    include "Cryptography/GpgMe++.h"
#  endif
#endif
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
#include "Imap/Model/FavoriteTagsModel.h"
#include "Imap/Model/GroupingMsgListModel.h"
#include "Imap/Model/Utils.h"
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
#include "AttachmentPanel.h"
#include "ContactPanel.h"
#include "DensityMenu.h"
#include "EmbeddedPanelsMenu.h"
#include "InlineNotificationBar.h"
#include "MailBoxTreeView.h"
#include "MailboxPickerDialog.h"
#include "MessageDetailsPanel.h"
#include "MessageListWidget.h"
#include "MessageView.h"
#include "MessageSourceWidget.h"
#include "Gui/MessageHeadersWidget.h"
#include "MsgListView.h"
#include "MessageSelectionRestorer.h"
#include "OnePanelAtTimeWidget.h"
#include "PasswordDialog.h"
#include "RelatedMessagesPanel.h"
#include "ProtocolLoggerWidget.h"
#include "SettingsDialog.h"
#include "ShortcutOverviewDialog.h"
#ifdef TROJITA_HAVE_WEBENGINE
#include "SimplePartWidget.h"
#endif
#include "Streams/SocketFactory.h"
#include "TaskProgressIndicator.h"
#include "Util.h"
#include "Window.h"
#include "ShortcutHandler/ShortcutHandler.h"

#include "ui_CreateMailboxDialog.h"
#include "ui_AboutDialog.h"
#include "ui_ImapInfoDialog.h"

#include "Imap/Model/ModelTest/modeltest.h"
#include "UiUtils/IconLoader.h"
#include "UiUtils/QaimDfsIterator.h"
#include "UiUtils/Relevance.h"

#include <QVBoxLayout>
#include "Imap/Model/AccountCapabilities.h"
#include "OAuth/OAuth.h"
#include "OAuth/Session.h"
#include "AccountSidebar.h"
#include "CapabilityActionGate.h"
#include "ShellHeader.h"

/** @short All user-facing widgets and related classes */
namespace Gui
{

    static const char * const netErrorUnseen = "net_error_unseen";

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

#ifdef TROJITA_HAVE_CRYPTO_MESSAGES
    Plugins::PluginManager::MimePartReplacers replacers;
#ifdef TROJITA_HAVE_GPGMEPP
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

    createWidgets();

    Imap::migrateSettings(m_settings);

    m_senderIdentities = new Composer::SenderIdentitiesModel(this);
    m_senderIdentities->loadFromSettings(*m_settings);
    for (const QString &accountId : m_accounts->accountIds())
        Composer::AccountIdentities::migrateAccount(*m_settings, *m_accounts->settings(accountId));

    if (! accountSettings()->contains(Common::SettingsNames::imapMethodKey)) {
        QTimer::singleShot(0, this, SLOT(slotShowAccountSettings()));
    }


    setupModels();
    connectBackgroundAccounts();
    createActions();
    createMenus();
    applyMessageListView();
    connect(msgListWidget->tree->header(), &QHeaderView::sectionClicked, this, [this]() {
        // Sorting by relevance is never remembered
        if (m_actionSortByRelevance && m_actionSortByRelevance->isChecked())
            return;
        const QHeaderView *header = msgListWidget->tree->header();
        rememberMessageListSort(header->sortIndicatorSection(), header->sortIndicatorOrder());
    });
    connect(msgListWidget->tree->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        captureFolderPosition(true);
    });
    slotToggleSysTray();
    slotPluginsChanged();

    slotFavoriteTagsChanged();
    connect(m_favoriteTags, &QAbstractItemModel::modelReset, this, &MainWindow::slotFavoriteTagsChanged);
    connect(m_favoriteTags, &QAbstractItemModel::layoutChanged, this, &MainWindow::slotFavoriteTagsChanged);
    connect(m_favoriteTags, &QAbstractItemModel::rowsMoved, this, &MainWindow::slotFavoriteTagsChanged);
    connect(m_favoriteTags, &QAbstractItemModel::rowsInserted, this, &MainWindow::slotFavoriteTagsChanged);
    connect(m_favoriteTags, &QAbstractItemModel::rowsRemoved, this, &MainWindow::slotFavoriteTagsChanged);
    connect(m_favoriteTags, &QAbstractItemModel::dataChanged, this, &MainWindow::slotFavoriteTagsChanged);

    // Please note that Qt 4.6.1 really requires passing the method signature this way, *not* using the SLOT() macro
    QDesktopServices::setUrlHandler(QStringLiteral("mailto"), this, "slotComposeMailUrl");
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

void MainWindow::reply(const Composer::ReplyMode mode)
{
#ifdef TROJITA_HAVE_WEBENGINE
    const QModelIndex messageIndex = m_messageWidget->message();
    const bool isCryptoMessage = m_messageWidget->isCryptoMessage();
    QPointer<MainWindow> window(this);

    m_messageWidget->quoteText([window, messageIndex, mode, isCryptoMessage](const QString &text) {
        if (!window || !messageIndex.isValid())
            return;

        if (isCryptoMessage)
            window->m_msgActionHandler.reply(messageIndex, mode, text, MessageActionHandler::CryptoMessage);
        else
            window->m_msgActionHandler.reply(messageIndex, mode, text, MessageActionHandler::Message);
    });
#endif
}

namespace {

/** @short Icon for an action shown at toolbar size, where Plasma prefers the -symbolic variant */
QIcon quickActionIcon(const QString &name)
{
    const QString symbolic = name + QLatin1String("-symbolic");
    return QIcon::hasThemeIcon(symbolic) ? QIcon::fromTheme(symbolic) : UiUtils::loadIcon(name);
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
    shortcutHandler->defineAction(QStringLiteral("action_go_to_folder"), QStringLiteral("go-jump"), i18nc("@action", "Go to &Folder..."), QStringLiteral("Ctrl+J"));
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
    // connect later

    exitAction = ShortcutHandler::instance()->createAction(QStringLiteral("action_application_exit"), qApp, SLOT(quit()), this);
    exitAction->setStatusTip(tr("Exit the application"));

    m_actionShortcutOverview = ShortcutHandler::instance()->createAction(QStringLiteral("action_shortcut_overview"), this);
    connect(m_actionShortcutOverview, &QAction::triggered, this, &MainWindow::slotShowShortcutOverview);

    netOffline = ShortcutHandler::instance()->createAction(QStringLiteral("action_network_offline"));
    netOffline->setCheckable(true);
    // connect later
    netExpensive = ShortcutHandler::instance()->createAction(QStringLiteral("action_network_expensive"));
    netExpensive->setCheckable(true);
    // connect later
    netOnline = ShortcutHandler::instance()->createAction(QStringLiteral("action_network_online"));
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
    connect(configSettings, &QAction::triggered, this, &MainWindow::slotShowSettings);
    m_actionAddAccount = new QAction(UiUtils::loadIcon(QStringLiteral("list-add")), i18n("&Add Account..."), this);
    connect(m_actionAddAccount, &QAction::triggered, this, &MainWindow::slotAddAccount);
    m_actionAccountSettings = new QAction(UiUtils::loadIcon(QStringLiteral("configure")), i18n("A&ccount Settings..."), this);
    m_actionAccountSettings->setObjectName(QStringLiteral("action_account_settings"));
    connect(m_actionAccountSettings, &QAction::triggered, this, &MainWindow::slotShowAccountSettings);
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

#ifdef TROJITA_HAVE_WEBENGINE
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
    m_mainToolbar->addWidget(networkIndicator);

    m_menuFromToolBar = new QToolButton(this);
    m_menuFromToolBar->setIcon(UiUtils::loadIcon(QStringLiteral("menu_new")));
    m_menuFromToolBar->setText(QChar(0x205d)); // Unicode 'TRICOLON'
    m_menuFromToolBar->setPopupMode(QToolButton::MenuButtonPopup);
    connect(m_menuFromToolBar, &QAbstractButton::clicked, m_menuFromToolBar, &QToolButton::showMenu);
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
        ADD_ACTION(applicationMenu, m_actionRenameAccount);
        ADD_ACTION(applicationMenu, m_actionRemoveAccount);
        applicationMenu->addSeparator();
        ADD_ACTION(applicationMenu, configSettings);
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
        QMenu *filterMenu = viewMenu->addMenu(quickActionIcon(QStringLiteral("view-filter")), i18nc("@title:menu", "&Filter"));
        filterMenu->setObjectName(QStringLiteral("messageListFilterMenu"));
            ADD_ACTION(filterMenu, m_actionFilterUnread);
            ADD_ACTION(filterMenu, m_actionFilterFlagged);
            ADD_ACTION(filterMenu, m_actionFilterAttachments);
            filterMenu->addSeparator();
            ADD_ACTION(filterMenu, m_actionClearFilters);
        m_groupByMenu = viewMenu->addMenu(quickActionIcon(QStringLiteral("view-group")), i18nc("@title:menu", "&Group By"));
        m_groupByMenu->setObjectName(QStringLiteral("messageListGroupByMenu"));
            for (QAction *action : m_groupByActions->actions()) {
                ADD_ACTION(m_groupByMenu, action);
            }
            m_groupByMenu->addSeparator();
            ADD_ACTION(m_groupByMenu, m_actionExpandAllGroups);
            ADD_ACTION(m_groupByMenu, m_actionCollapseAllGroups);
        viewMenu->addAction(m_actionViewForAllFolders);
        viewMenu->addAction(m_actionResetFolderView);
        viewMenu->addSeparator();
        ADD_ACTION(viewMenu, m_actionShowOnlySubscribed);

    QMenu *mailboxMenu = menuBar()->addMenu(tr("Mail&box"));
        mailboxMenu->addMenu(m_goToFolderMenu);
        ADD_ACTION(mailboxMenu, m_goToFolder);
        mailboxMenu->addSeparator();
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
#ifdef TROJITA_HAVE_WEBENGINE
        ADD_ACTION(mailboxMenu, m_previousMessage);
        ADD_ACTION(mailboxMenu, m_nextMessage);
        ADD_ACTION(mailboxMenu, m_previousAnyMessage);
        ADD_ACTION(mailboxMenu, m_nextAnyMessage);
        mailboxMenu->addSeparator();
#endif
        ADD_ACTION(mailboxMenu, expunge);
        mailboxMenu->addSeparator();
        ADD_ACTION(mailboxMenu, m_renameMailbox);
        ADD_ACTION(mailboxMenu, m_emptyTrash);
        ADD_ACTION(mailboxMenu, m_emptyJunk);
        connect(mailboxMenu, &QMenu::aboutToShow, this, [this]() {
            m_renameMailbox->setEnabled(canRenameMailbox(mboxTree->currentIndex()));
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
#ifdef TROJITA_HAVE_WEBENGINE
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
    m_backgroundAlerts->setIcon(QIcon::fromTheme(QStringLiteral("dialog-warning-symbolic"),
                                                 QIcon::fromTheme(QStringLiteral("dialog-warning"))));
    m_backgroundAlerts->hide();
    connect(m_backgroundAlerts, &QToolButton::clicked, this, [this]() {
        switchAccount(m_backgroundAlerts->property("accountId").toString());
    });
    // Left of the shown account's connection status
    statusBar()->insertPermanentWidget(0, m_backgroundAlerts);

    m_oauthSignIn = new QToolButton(statusBar());
    m_oauthSignIn->setObjectName(QStringLiteral("oauthSignInButton"));
    m_oauthSignIn->setAutoRaise(true);
    m_oauthSignIn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_oauthSignIn->setIcon(QIcon::fromTheme(QStringLiteral("dialog-password-symbolic"),
                                            QIcon::fromTheme(QStringLiteral("dialog-password"))));
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
    msgListWidget->tree->setContextMenuPolicy(Qt::CustomContextMenu);
    msgListWidget->tree->setAlternatingRowColors(true);
    msgListWidget->setRawSearchEnabled(m_settings->value(Common::SettingsNames::guiAllowRawSearch).toBool());
    connect (msgListWidget, &MessageListWidget::rawSearchSettingChanged, this, &MainWindow::saveRawStateSetting);

    connect(msgListWidget->tree, &QWidget::customContextMenuRequested, this, &MainWindow::showContextMenuMsgListTree);
    connect(msgListWidget->tree, &QAbstractItemView::activated, this, &MainWindow::msgListClicked);
    connect(msgListWidget->tree, &QAbstractItemView::clicked, this, &MainWindow::msgListClicked);
    connect(msgListWidget->tree, &QAbstractItemView::doubleClicked, this, &MainWindow::openCompleteMessageWidget);
    connect(msgListWidget->tree, &MsgListView::currentReplacedAfterRemoval, this, &MainWindow::slotMessageReplacedAfterRemoval);
    connect(msgListWidget, &MessageListWidget::requestingSearch, this, &MainWindow::slotSearchRequested);
    connect(msgListWidget->tree->header(), &QHeaderView::sectionMoved, m_delayedStateSaving, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(msgListWidget->tree->header(), &QHeaderView::sectionResized, m_delayedStateSaving, static_cast<void (QTimer::*)()>(&QTimer::start));

    msgListWidget->tree->installEventFilter(this);

#ifdef TROJITA_HAVE_WEBENGINE
    m_messageWidget = new CompleteMessageWidget(this, m_settings, m_pluginManager, m_favoriteTags);
    connect(m_messageWidget->messageView, &MessageView::messageChanged, this, &MainWindow::scrollMessageUp);
    connect(m_messageWidget->messageView, &MessageView::messageChanged, this, &MainWindow::slotUpdateMessageActions);
    connect(m_messageWidget->messageView, &MessageView::linkHovered, [](const QString &url) {
        if (url.isEmpty()) {
            QToolTip::hideText();
        } else {
            // indirection due to https://bugs.kde.org/show_bug.cgi?id=363783
            QTimer::singleShot(250, [url]() {
                QToolTip::showText(QCursor::pos(), QObject::tr("Link target: %1").arg(UiUtils::Formatting::htmlEscaped(url)));
            });
        }
    });
    connect(m_messageWidget->messageView, &MessageView::transferError, this, &MainWindow::slotDownloadTransferError);

    m_attachmentPanel = new AttachmentPanel();
    m_attachmentPanel->setNetworkAccessManager(m_messageWidget->messageView->networkAccessManager());
    connect(m_messageWidget->messageView, &MessageView::messageModelChanged, m_attachmentPanel, &AttachmentPanel::setMessageModel);
    connect(m_attachmentPanel, &AttachmentPanel::transferError, this, &MainWindow::slotDownloadTransferError);

    m_messageDetailsPanel = new MessageDetailsPanel();
    m_contactPanel = new ContactPanel();
    m_relatedMessagesPanel = new RelatedMessagesPanel();
    for (MessageContextPanel *panel : std::initializer_list<MessageContextPanel *>{m_messageDetailsPanel, m_contactPanel, m_relatedMessagesPanel}) {
        connect(m_messageWidget->messageView, &MessageView::messageChanged, panel, [this, panel]() {
            panel->setMessage(m_messageWidget->messageView->currentMessage());
        });
    }
    connect(m_contactPanel, &ContactPanel::composeRequested, this, &MainWindow::slotComposeMailUrl);
    connect(m_contactPanel, &ContactPanel::manageContactRequested, this, &MainWindow::slotManageContact);
    connect(m_relatedMessagesPanel, &RelatedMessagesPanel::messageActivated, this, &MainWindow::activateRelatedMessage);
    m_embeddedPanels = {m_attachmentPanel, m_messageDetailsPanel, m_contactPanel, m_relatedMessagesPanel};
#else
   m_messageWidget = new QLabel(QStringLiteral("Message Widget placeholder"));
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
#ifdef TROJITA_HAVE_WEBENGINE
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

#ifdef TROJITA_HAVE_WEBENGINE
    m_messageWidget->messageView->setNetworkWatcher(qobject_cast<Imap::Mailbox::NetworkWatcher*>(m_imapAccess->networkWatcher()));
    m_relatedMessagesPanel->setMessageList(m_imapAccess->msgListModel());
#endif

    auto realThreadingModel = qobject_cast<Imap::Mailbox::ThreadingMsgListModel*>(m_imapAccess->threadingMsgListModel());
    Q_ASSERT(realThreadingModel);
    auto realMsgListModel = qobject_cast<Imap::Mailbox::MsgListModel*>(m_imapAccess->msgListModel());
    Q_ASSERT(realMsgListModel);

    prettyMboxModel = new Imap::Mailbox::PrettyMailboxModel(this, qobject_cast<QAbstractItemModel *>(m_imapAccess->mailboxModel()));
    prettyMboxModel->setObjectName(QStringLiteral("prettyMboxModel"));
    connect(realThreadingModel, &Imap::Mailbox::ThreadingMsgListModel::sortingFailed,
            msgListWidget, &MessageListWidget::slotSortingFailed);
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
    }
    applyMessageListFilters();
    connect(prettyMsgListModel, &Imap::Mailbox::PrettyMsgListModel::filtersChanged, this, &MainWindow::syncMessageListFilterUi);
    connect(prettyMsgListModel, &QAbstractItemModel::rowsInserted, this, &MainWindow::scheduleMessageListStatus);
    connect(prettyMsgListModel, &QAbstractItemModel::rowsRemoved, this, &MainWindow::scheduleMessageListStatus);
    connect(prettyMsgListModel, &QAbstractItemModel::modelReset, this, &MainWindow::scheduleMessageListStatus);
    connect(prettyMsgListModel, &QAbstractItemModel::layoutChanged, this, &MainWindow::scheduleMessageListStatus);
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
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::dataChanged, this, &MainWindow::updateMessageFlags);
    // Sorting an empty list does not stick, so the folder's sorting applies once its messages are there
    connect(realMsgListModel, &Imap::Mailbox::MsgListModel::messagesAvailable, this, [this]() {
        if (!m_viewMailbox.isEmpty() && m_viewMailbox == currentMailboxName())
            applyMessageListSort();
    });
    connect(qobject_cast<Imap::Mailbox::MsgListModel*>(m_imapAccess->msgListModel()), &Imap::Mailbox::MsgListModel::messagesAvailable,
            this, &MainWindow::slotScrollToUnseenMessage);
    // When a resynchronization replaces all messages of the folder, the same messages are selected again
    connect(realMsgListModel, &QAbstractItemModel::rowsRemoved, this, [this, realMsgListModel](const QModelIndex &parent) {
        if (parent.isValid() || realMsgListModel->rowCount() || !m_messageRestorer || !m_livePosition.isValid() ||
            m_positionMailbox.isEmpty() || m_positionMailbox != currentMailboxName() || m_messageRestorer->isPending())
            return;
        m_messageRestorer->restorePosition(m_positionMailbox, m_livePosition.uidValidity, m_livePosition.uid,
                                           m_livePosition.anchorUid, m_livePosition.selectedUids);
    });
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::rowsInserted, msgListWidget, &MessageListWidget::slotAutoEnableDisableSearch);
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::rowsRemoved, msgListWidget, &MessageListWidget::slotAutoEnableDisableSearch);
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::rowsRemoved, this, &MainWindow::updateMessageFlags);
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::layoutChanged, msgListWidget, &MessageListWidget::slotAutoEnableDisableSearch);
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::layoutChanged, this, &MainWindow::updateMessageFlags);
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::modelReset, msgListWidget, &MessageListWidget::slotAutoEnableDisableSearch);
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::modelReset, this, &MainWindow::updateMessageFlags);
    connect(realMsgListModel, &Imap::Mailbox::MsgListModel::mailboxChanged, this, &MainWindow::slotMailboxChanged);
    connect(realMsgListModel, &Imap::Mailbox::MsgListModel::mailboxChanged, this, [this]() {
        commitFolderPosition();
        m_positionMailbox = currentMailboxName();
        m_livePosition = UiUtils::FolderPosition();
        pruneFolderStates();
        applyMessageListView();
        updateMessageListScope();
        // Later, after a click on the folder has cancelled restoring another message
        QMetaObject::invokeMethod(this, [this]() { restoreFolderPosition(); }, Qt::QueuedConnection);
    });
    KConfigGroup selection(KSharedConfig::openStateConfig(), QStringLiteral("MailboxSelection"));
    KConfigGroup accountSelection = selection.group(m_imapAccess->credentialAccountId());
    connect(realMsgListModel, &Imap::Mailbox::MsgListModel::mailboxChanged, this,
            [accountSelection](const QModelIndex &mailbox) mutable {
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
    connect(mboxTree, &MailBoxTreeView::mailboxRestored, realMsgListModel,
            static_cast<void (Imap::Mailbox::MsgListModel::*)(const QModelIndex &)>(&Imap::Mailbox::MsgListModel::setMailbox));
    connect(mboxTree, &MailBoxTreeView::mailboxRestored, messageRestorer,
            [messageRestorer, accountSelection](const QModelIndex &mailbox) {
        const QString name = mailbox.data(Imap::Mailbox::RoleMailboxName).toString();
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

    qApp->setQuitOnLastWindowClosed(false);

    m_trayIcon = new QSystemTrayIcon(this);
    handleTrayIconChange();

    QAction* quitAction = new QAction(tr("&Quit"), m_trayIcon);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    QMenu *trayIconMenu = new QMenu(this);
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

    qApp->setQuitOnLastWindowClosed(true);
}

void MainWindow::slotToggleSysTray()
{
    bool showSystray = m_settings->value(Common::SettingsNames::guiShowSystray, QVariant(true)).toBool();
    if (showSystray && !m_trayIcon && QSystemTrayIcon::isSystemTrayAvailable()) {
        createSysTray();
    } else if (!showSystray && m_trayIcon) {
        removeSysTray();
    }
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
    auto profileName = QString::fromUtf8(qgetenv("TROJITA_PROFILE"));
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
        painter.setBrush(isOffline ? Qt::red : Qt::black);
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
    m_trayIcon->setIcon(QIcon(pixmap));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    flushUiState();
    if (m_trayIcon && m_trayIcon->isVisible()) {
        Util::askForSomethingUnlessTold(tr("Plasma Impresario"),
                                        tr("The application will continue in systray. This can be disabled within the settings."),
                                        Common::SettingsNames::guiOnSystrayClose, QMessageBox::Ok, this, m_settings);
        hide();
        event->ignore();
    }
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
#ifdef TROJITA_HAVE_WEBENGINE
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
    setVisible(true);
    activateWindow();
    raise();
}

void MainWindow::saveMessageSelection(const QModelIndex &index)
{
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
#ifdef TROJITA_HAVE_WEBENGINE
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
#ifdef TROJITA_HAVE_WEBENGINE
    // Because it's quite possible that we have switched into another mailbox, make sure that we're in the "current" one so that
    // user will be notified about new arrivals, etc.
    QModelIndex translated = Imap::deproxifiedIndex(index);
    imapModel()->switchToMailbox(translated.parent().parent());

    if (index.column() == Imap::Mailbox::MsgListModel::SEEN) {
        if (!translated.data(Imap::Mailbox::RoleIsFetched).toBool())
            return;
        Imap::Mailbox::FlagsOperation flagOp = translated.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool() ?
                                               Imap::Mailbox::FLAG_REMOVE : Imap::Mailbox::FLAG_ADD;
        const bool read = flagOp == Imap::Mailbox::FLAG_ADD;
        trackFlagChange(imapModel()->markMessagesRead(QModelIndexList() << translated, flagOp), QString(), QString(),
                        [read](const QString &message) {
            return read ? i18ncp("@info", "Could not mark %1 message as read: %2", "Could not mark %1 messages as read: %2", 1, message)
                        : i18ncp("@info", "Could not mark %1 message as unread: %2", "Could not mark %1 messages as unread: %2", 1,
                                 message);
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
        trackFlagChange(imapModel()->setMessageFlags(QModelIndexList() << translated, Imap::Mailbox::FlagNames::flagged, flagOp),
                        QString(), QString(), [flagged](const QString &message) {
            return flagged ? i18ncp("@info", "Could not flag %1 message: %2", "Could not flag %1 messages: %2", 1, message)
                           : i18ncp("@info", "Could not remove the flag from %1 message: %2",
                                    "Could not remove the flag from %1 messages: %2", 1, message);
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
#ifdef TROJITA_HAVE_WEBENGINE
    const QModelIndex index = msgListWidget->tree->currentIndex();

    if (! index.data(Imap::Mailbox::RoleMessageUid).isValid())
        return;

    CompleteMessageWidget *widget = new CompleteMessageWidget(0, m_settings, m_pluginManager, m_favoriteTags);
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
        actionList.append(resyncMbox);
        actionList.append(reloadMboxList);

        actionList.append(m_actionSubscribeMailbox);
        m_actionSubscribeMailbox->setChecked(mboxTree->indexAt(position).data(Imap::Mailbox::RoleMailboxIsSubscribed).toBool());
    } else {
        actionList.append(createTopMailbox);
    }
    actionList.append(reloadAllMailboxes);
    actionList.append(m_actionShowOnlySubscribed);
    QMenu::exec(actionList, mboxTree->mapToGlobal(position), nullptr, this);
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
        actionList.append(viewMsgSource);
        actionList.append(viewMsgHeaders);
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
                   << m_actionFilterAttachments << m_actionClearFilters << &filterSeparator << m_actionMarkMailboxAsRead;
        if (msgListWidget->tree->model() == m_groupingMsgListModel)
            actionList << &relatedSeparator << m_actionExpandAllGroups << m_actionCollapseAllGroups;
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
    dialog->setMailboxes(prettyMboxModel, {current});
    const QStringList recent = UiUtils::RecentFolders::load(*accountSettings(), Common::SettingsNames::imapRecentDestinationFolders)
            .offered(UiUtils::RecentFolders::StateLookup(), {current});
    if (!recent.isEmpty())
        dialog->setCurrentMailbox(recent.constFirst());
    QPointer<Imap::Mailbox::Model> model = imapModel();
    connect(dialog, &QDialog::accepted, this, [this, dialog, model, messages, op]() {
        // The selection may have changed, or the messages may be gone, while the dialog was open
        if (!model || model != imapModel())
            return;
        QModelIndexList stillThere;
        for (const QPersistentModelIndex &index : messages) {
            if (index.isValid())
                stillThere << index;
        }
        copyOrMoveToFolder(stillThere, dialog->selectedMailbox(), op);
    });
    dialog->open();
}

void MainWindow::populateRecentFoldersMenu(QMenu *menu, const Imap::Mailbox::CopyMoveOperation op)
{
    using UiUtils::RecentFolders;
    menu->clear();
    QSettings *settings = accountSettings();
    RecentFolders recent = RecentFolders::load(*settings, Common::SettingsNames::imapRecentDestinationFolders);
    // The unfiltered folder tree, so that unsubscribed folders do not look deleted
    const RecentFolders::StateLookup state = RecentFolders::stateFromModel(m_imapAccess->mailboxModel());
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
    Imap::Mailbox::ImapTask *task = model->copyMoveMessages(mailbox, messages, op);
    if (op == Imap::Mailbox::MOVE)
        trackMove(task, currentMailboxName(), mailbox, messages.size(), MoveKind::Folder);
    else
        trackCopy(task, mailbox, messages.size());
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
    }
}

void MainWindow::slotShowAccountSettings()
{
    SettingsDialog *dialog = new SettingsDialog(this, SettingsDialog::Scope::Account, m_senderIdentities,
                                                accountIdentitiesModel(), m_favoriteTags, m_settings);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(i18nc("@title:window %1 is the account name", "Account Settings: %1",
                                 m_accounts->displayName(m_accounts->activeAccount())));
    if (dialog->exec() == QDialog::Accepted) {
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
    if (method != Common::SettingsNames::methodTCP && method != Common::SettingsNames::methodSSL &&
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
    return m_accounts->settings(m_accounts->activeAccount());
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
    const QString accountId = m_accounts->activeAccount();
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
    connect(m_accountStatus, &Common::AccountStatusTracker::statusChanged, this, [this]() {
        if (m_accountSwitcher)
            refreshAccountSwitcher();
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
    m_imapAccess = accountAccess(m_accounts->activeAccount());
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
        ? QString::fromUtf8(qgetenv("TROJITA_PROFILE")) : accountId;
    auto *access = new Imap::ImapAccess(this, m_accounts->settings(accountId), m_pluginManager, name, m_settings);
    connect(access, &Imap::ImapAccess::cacheError, this, &MainWindow::cacheError);
    connect(access, &Imap::ImapAccess::checkSslPolicy, this, [this, accountId]() { checkSslPolicy(accountId); },
            Qt::QueuedConnection);
    m_accountAccess.insert(accountId, access);
    return access;
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
    }
    m_capabilityGate->apply(Imap::AccountCapabilities::forAccount(*accountSettings()));
    m_accountSidebar->refresh(*m_accounts, *m_accountStatus);
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

void MainWindow::notifyBackgroundAccount(const QString &accountId)
{
    const Common::AccountStatus status = m_accountStatus->status(accountId);
    const QString title = i18nc("@title notification, %1 is the account name", "%1 needs attention",
                                m_accounts->displayName(accountId));
    QString text = Common::AccountStatusTracker::label(status.kind);
    if (!status.detail.isEmpty())
        text += QLatin1String(": ") + status.detail;
#ifdef TROJITA_HAVE_KNOTIFICATIONS
    auto *notification = new KNotification(QStringLiteral("accountNeedsAttention"), KNotification::CloseOnTimeout);
    notification->setComponentName(QStringLiteral("plasma-impresario"));
    notification->setTitle(title);
    notification->setText(text.toHtmlEscaped());
    notification->setWindow(windowHandle());
    KNotificationAction *open = notification->addDefaultAction(i18nc("@action:button", "Open Account"));
    connect(open, &KNotificationAction::activated, this, [this, accountId]() {
        switchAccount(accountId);
        showMainWindow();
    });
    if (status.kind == Common::AccountStatusKind::AuthFailed && usesOAuth(*m_accounts->settings(accountId))) {
        KNotificationAction *signIn = notification->addAction(i18nc("@action:button", "Sign In"));
        connect(signIn, &KNotificationAction::activated, this, [this, accountId]() { signInAccount(accountId); });
    }
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
    using Common::AccountStatusKind;
    connect(model, &Imap::Mailbox::Model::connectionStateChanged, access,
            [this, accountId](uint, const Imap::ConnectionState state) {
        const AccountStatusKind current = m_accountStatus->status(accountId).kind;
        if (state == Imap::CONN_STATE_NONE) {
            if (!Common::AccountStatusTracker::needsAttention(current))
                m_accountStatus->setStatus(accountId, AccountStatusKind::Idle);
        } else if (state >= Imap::CONN_STATE_AUTHENTICATED) {
            m_accountStatus->setStatus(accountId, AccountStatusKind::Connected);
        } else {
            m_accountStatus->setStatus(accountId, AccountStatusKind::Connecting);
        }
    });
    connect(model, &Imap::Mailbox::Model::networkPolicyOffline, access, [this, accountId]() {
        if (!Common::AccountStatusTracker::needsAttention(m_accountStatus->status(accountId).kind))
            m_accountStatus->setStatus(accountId, AccountStatusKind::Offline);
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
    if (accountId == m_accounts->activeAccount() || !m_accounts->accountIds().contains(accountId))
        return;
    commitFolderPosition();
    detachAccountModels();
    nukeModels();
    m_accounts->setActiveAccount(accountId);
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
    if (status.kind == Common::AccountStatusKind::AuthRequired) {
        m_ignoreStoredPassword = !status.detail.isEmpty();
        authenticationRequested();
    }
}

void MainWindow::slotAddAccount()
{
    bool accepted = false;
    const QString name = QInputDialog::getText(this, i18n("Add Account"), i18n("Account name:"),
                                               QLineEdit::Normal, QString(), &accepted).trimmed();
    if (!accepted || name.isEmpty())
        return;
    const QString accountId = m_accounts->addAccount(name);
    Composer::AccountIdentities::initializeNewAccount(*m_accounts->settings(accountId));
    switchAccount(accountId);
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
#ifdef TROJITA_HAVE_WEBENGINE
    m_messageWidget->messageView->setFocus();
#else
    m_messageWidget->setFocus();
#endif
}

void MainWindow::slotFocusNextRegion()
{
    cycleFocusRegion(1);
}

void MainWindow::slotFocusPreviousRegion()
{
    cycleFocusRegion(-1);
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
#ifdef TROJITA_HAVE_WEBENGINE
    regions.append(Region{m_messageWidget, m_messageWidget->messageView});
#else
    regions.append(Region{m_messageWidget, m_messageWidget});
#endif
    for (EmbeddedPanel *panel : std::as_const(m_embeddedPanels))
        regions.append(Region{panel, panel->focusTarget()});

    const int count = regions.size();
    QWidget *focus = focusWidget();
    int current = -1;
    for (int i = 0; i < count && focus; ++i) {
        if (regions.at(i).container == focus || regions.at(i).container->isAncestorOf(focus)) {
            current = i;
            break;
        }
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
    refreshAccountSwitcher();
    updateMessageListScope();
    slotUpdateWindowTitle();
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
    QString question = i18n("Remove account \"%1\" and its settings? Messages on the server are not affected.",
                            m_accounts->displayName(accountId));
    const QSettings &settings = *m_accounts->settings(accountId);
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
    Imap::ImapAccess *access = accountAccess(accountId);
    const QSettings &removed = *m_accounts->settings(accountId);
    const bool revokeOAuth = usesOAuth(removed) && !sharesOAuthGrant(accountId);
    const OAuth::AccountConfig oauthConfig = OAuth::AccountConfig::fromSettings(removed, access->credentialAccountId());
    if (Plugins::PasswordPlugin *password = pluginManager()->password()) {
        QStringList types{QStringLiteral("imap"), QStringLiteral("smtp")};
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
    // Deleting the account's models also closes its cache database, so the files can be removed.
    delete m_accountAccess.take(accountId);
    if (!Imap::removeAccountCache(cacheDirectory, Common::writablePath(Common::LOCATION_CACHE)))
        qWarning() << "Could not delete the offline cache of the removed account in" << cacheDirectory;
    // Open composers may still list this account's identities, so the model lives until the window closes.
    m_accountIdentities.remove(accountId);
    m_accountStatus->forget(accountId);
    m_accounts->removeAccount(accountId);
    refreshAccountSwitcher();
    slotUpdateWindowTitle();
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
#ifdef TROJITA_HAVE_WEBENGINE
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
    path = QFileDialog::getOpenFileName(this, tr("Edit draft"), path, tr("Drafts") + QLatin1String(" (*.draft)"));
    if (!path.isNull()) {
        ComposeWidget::warnIfMsaNotConfigured(ComposeWidget::createDraft(this, path), this);
    }
}

QModelIndexList MainWindow::translatedSelection() const
{
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
#ifdef TROJITA_HAVE_WEBENGINE
    const QModelIndexList translatedIndexes = translatedSelection();
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleMarkAsRead: no valid messages";
    } else {
        const int count = translatedIndexes.size();
        // The change of a single message is visible in the list
        const QString done = count < 2 ? QString()
            : value ? i18ncp("@info", "Marked %1 message as read", "Marked %1 messages as read", count)
                    : i18ncp("@info", "Marked %1 message as unread", "Marked %1 messages as unread", count);
        trackFlagChange(imapModel()->markMessagesRead(translatedIndexes, value ? Imap::Mailbox::FLAG_ADD : Imap::Mailbox::FLAG_REMOVE),
                        value ? i18ncp("@info", "Marking %1 message as read…", "Marking %1 messages as read…", count)
                              : i18ncp("@info", "Marking %1 message as unread…", "Marking %1 messages as unread…", count),
                        done, [value, count](const QString &message) {
            return value ? i18ncp("@info", "Could not mark %1 message as read: %2", "Could not mark %1 messages as read: %2", count,
                                  message)
                         : i18ncp("@info", "Could not mark %1 message as unread: %2", "Could not mark %1 messages as unread: %2",
                                  count, message);
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
#ifdef TROJITA_HAVE_WEBENGINE
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
#ifdef TROJITA_HAVE_WEBENGINE
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
#ifdef TROJITA_HAVE_WEBENGINE
    m_messageWidget->messageView->setMessage(target);
#endif
    tree->setCurrentIndex(target);
}

void MainWindow::slotPreviousUnread()
{
#ifdef TROJITA_HAVE_WEBENGINE
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
        trackFlagChange(imapModel()->setMessageFlags(translatedIndexes, tagName, checked ? Imap::Mailbox::FLAG_ADD : Imap::Mailbox::FLAG_REMOVE),
                        checked ? i18ncp("@info", "Adding the tag %2 to %1 message…", "Adding the tag %2 to %1 messages…", count, tagName)
                                : i18ncp("@info", "Removing the tag %2 from %1 message…", "Removing the tag %2 from %1 messages…", count,
                                         tagName),
                        done, [checked, count, tagName](const QString &message) {
            return checked ? i18ncp("@info", "Could not add the tag %2 to %1 message: %3", "Could not add the tag %2 to %1 messages: %3",
                                    count, tagName, message)
                           : i18ncp("@info", "Could not remove the tag %2 from %1 message: %3",
                                    "Could not remove the tag %2 from %1 messages: %3", count, tagName, message);
        });
    }
}

void MainWindow::handleMarkAsDeleted(bool value)
{
    const QModelIndexList translatedIndexes = translatedSelection();
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleMarkAsDeleted: no valid messages";
    } else if (const QString trash = deleteTrashTarget(); !trash.isEmpty()) {
        auto task = imapModel()->copyMoveMessages(trash, translatedIndexes, Imap::Mailbox::CopyMoveOperation::MOVE);
        trackMove(task, currentMailboxName(), trash, translatedIndexes.size(), MoveKind::Trash);
    } else {
        // A shortcut can fire while the action still says "Move to Trash", which is not a toggle
        if (!markAsDeleted->isCheckable())
            value = true;
        const int count = translatedIndexes.size();
        const QString done = count < 2 ? QString()
            : value ? i18ncp("@info", "Marked %1 message as deleted", "Marked %1 messages as deleted", count)
                    : i18ncp("@info", "Removed the deleted mark from %1 message", "Removed the deleted mark from %1 messages", count);
        trackFlagChange(imapModel()->markMessagesDeleted(translatedIndexes, value ? Imap::Mailbox::FLAG_ADD : Imap::Mailbox::FLAG_REMOVE),
                        value ? i18ncp("@info", "Marking %1 message as deleted…", "Marking %1 messages as deleted…", count)
                              : i18ncp("@info", "Removing the deleted mark from %1 message…",
                                       "Removing the deleted mark from %1 messages…", count),
                        done, [value, count](const QString &message) {
            return value ? i18ncp("@info", "Could not mark %1 message as deleted: %2", "Could not mark %1 messages as deleted: %2",
                                  count, message)
                         : i18ncp("@info", "Could not remove the deleted mark from %1 message: %2",
                                  "Could not remove the deleted mark from %1 messages: %2", count, message);
        });
    }
}

void MainWindow::handleMarkAsFlagged(const bool value)
{
    const QModelIndexList translatedIndexes = translatedSelection();
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleMarkAsFlagged: no valid messages";
    } else {
        const int count = translatedIndexes.size();
        const QString done = count < 2 ? QString()
            : value ? i18ncp("@info", "Flagged %1 message", "Flagged %1 messages", count)
                    : i18ncp("@info", "Removed the flag from %1 message", "Removed the flag from %1 messages", count);
        trackFlagChange(imapModel()->setMessageFlags(translatedIndexes, Imap::Mailbox::FlagNames::flagged,
                                                     value ? Imap::Mailbox::FLAG_ADD : Imap::Mailbox::FLAG_REMOVE),
                        value ? i18ncp("@info", "Flagging %1 message…", "Flagging %1 messages…", count)
                              : i18ncp("@info", "Removing the flag from %1 message…", "Removing the flag from %1 messages…", count),
                        done, [value, count](const QString &message) {
            return value ? i18ncp("@info", "Could not flag %1 message: %2", "Could not flag %1 messages: %2", count, message)
                         : i18ncp("@info", "Could not remove the flag from %1 message: %2",
                                  "Could not remove the flag from %1 messages: %2", count, message);
        });
    }
}

void MainWindow::handleMarkAsJunk(const bool value)
{
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
            trackFlagChange(imapModel()->setMessageFlags(translatedIndexes, Imap::Mailbox::FlagNames::notjunk, Imap::Mailbox::FLAG_REMOVE),
                            QString(), QString(), failed);
        }
        const QString done = count < 2 ? QString()
            : value ? i18ncp("@info", "Marked %1 message as junk", "Marked %1 messages as junk", count)
                    : i18ncp("@info", "Removed the junk mark from %1 message", "Removed the junk mark from %1 messages", count);
        trackFlagChange(imapModel()->setMessageFlags(translatedIndexes, Imap::Mailbox::FlagNames::junk,
                                                     value ? Imap::Mailbox::FLAG_ADD : Imap::Mailbox::FLAG_REMOVE),
                        value ? i18ncp("@info", "Marking %1 message as junk…", "Marking %1 messages as junk…", count)
                              : i18ncp("@info", "Removing the junk mark from %1 message…", "Removing the junk mark from %1 messages…",
                                       count),
                        done, failed);
    }
}

void MainWindow::handleMarkAsNotJunk(const bool value)
{
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
            trackFlagChange(imapModel()->setMessageFlags(translatedIndexes, Imap::Mailbox::FlagNames::junk, Imap::Mailbox::FLAG_REMOVE),
                            QString(), QString(), failed);
        }
        const QString done = count < 2 ? QString()
            : value ? i18ncp("@info", "Marked %1 message as not junk", "Marked %1 messages as not junk", count)
                    : i18ncp("@info", "Removed the not-junk mark from %1 message", "Removed the not-junk mark from %1 messages",
                             count);
        trackFlagChange(imapModel()->setMessageFlags(translatedIndexes, Imap::Mailbox::FlagNames::notjunk,
                                                     value ? Imap::Mailbox::FLAG_ADD : Imap::Mailbox::FLAG_REMOVE),
                        value ? i18ncp("@info", "Marking %1 message as not junk…", "Marking %1 messages as not junk…", count)
                              : i18ncp("@info", "Removing the not-junk mark from %1 message…",
                                       "Removing the not-junk mark from %1 messages…", count),
                        done, failed);
    }
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
    const QModelIndexList translatedIndexes = translatedSelection();
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleMoveToArchive: no valid messages";
    } else {
        const QString archiveFolderName = archiveTarget();
        auto copyMoveMessagesTask = imapModel()->copyMoveMessages(archiveFolderName, translatedIndexes,
                                                                  Imap::Mailbox::CopyMoveOperation::MOVE);
        trackMove(copyMoveMessagesTask, currentMailboxName(), archiveFolderName, translatedIndexes.size(), MoveKind::Archive);
    }
}

void MainWindow::handleDeletePermanently()
{
    using UiUtils::DestructiveActions::Confirmation;
    const QModelIndexList translatedIndexes = translatedSelection();
    if (translatedIndexes.isEmpty() || !imapModel()->isNetworkAvailable())
        return;

    // The model can change while the question is shown
    const QList<QPersistentModelIndex> selected(translatedIndexes.cbegin(), translatedIndexes.cend());
    const int count = selected.size();
    if (!confirmPermanentAction(Confirmation::DeletePermanently, i18nc("@title:window", "Delete Permanently"),
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

    bool expungeScheduled = false;
    Imap::Mailbox::ImapTask *task = imapModel()->deleteMessagesPermanently(messages, &expungeScheduled);
    if (!task) {
        m_notificationBar->showError(i18nc("@info", "Could not delete the messages because the account is offline."));
        return;
    }
    QPointer<Imap::Mailbox::Model> model = imapModel();
    const int deleted = messages.size();
    connect(task, &Imap::Mailbox::ImapTask::completed, this, [this, model, deleted, expungeScheduled]() {
        if (!model || model != imapModel())
            return;
        if (expungeScheduled) {
            m_notificationBar->showInfo(i18ncp("@info", "Permanently deleted %1 message", "Permanently deleted %1 messages", deleted));
            return;
        }
        // Without UIDPLUS, only an EXPUNGE of the whole folder can remove them
        InlineNotificationBar::Action expungeNow{i18nc("@action:button", "Expunge"), QStringLiteral("trash-empty"),
                                                 QStringLiteral("expungeMailbox"), [this, model]() {
            if (model && model == imapModel())
                slotExpunge();
        }};
        m_notificationBar->showInfo(i18ncp("@info",
                                           "Marked %1 message as deleted. The server cannot remove single messages; "
                                           "Expunge removes all messages marked as deleted in this folder.",
                                           "Marked %1 messages as deleted. The server cannot remove single messages; "
                                           "Expunge removes all messages marked as deleted in this folder.", deleted),
                                    {expungeNow});
    });
    connect(task, &Imap::Mailbox::ImapTask::failed, this, [this, model, deleted](const QString &message) {
        if (model && model == imapModel())
            m_notificationBar->showError(i18ncp("@info", "Could not delete %1 message permanently: %2",
                                                "Could not delete %1 messages permanently: %2", deleted, message));
    });
    showBusyWhileRunning(task, i18ncp("@info", "Deleting %1 message permanently…", "Deleting %1 messages permanently…", deleted));
}

void MainWindow::setConfirmationPrompt(const ConfirmationPrompt &prompt)
{
    m_confirmationPrompt = prompt;
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
    auto *model = qobject_cast<Imap::Mailbox::MsgListModel *>(m_imapAccess->msgListModel());
    return model ? model->currentMailbox().data(Imap::Mailbox::RoleMailboxName).toString() : QString();
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
    return chooseDeleteAction(deleteMovesToTrash(), trash, currentMailboxName()) == DeleteAction::MoveToTrash ? trash : QString();
}

void MainWindow::updateDeleteActionMode()
{
    // The models are set up before the actions exist
    if (!m_deleteMovesToTrash)
        return;
    const QString trash = deleteTrashTarget();
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
                  << i18nc("@title:group", "General") << i18nc("@title:group", "Other Windows");
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
    return entries;
}

void MainWindow::trackMove(Imap::Mailbox::ImapTask *task, const QString &source, const QString &target, const int count,
                           const MoveKind kind)
{
    if (!task)
        return;
    QPointer<Imap::Mailbox::Model> model = imapModel();
    connect(task, &Imap::Mailbox::ImapTask::completed, this,
            [this, model, source, target, count, kind](Imap::Mailbox::ImapTask *finished) {
        if (!model || model != imapModel())
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
        if (copyMove && copyMove->copyUidValidity() && !copyMove->copyUidDestinationUids().isEmpty() && !source.isEmpty()) {
            const uint uidValidity = copyMove->copyUidValidity();
            const Imap::Uids uids = copyMove->copyUidDestinationUids();
            action = {i18nc("@action:button", "Undo"), QStringLiteral("edit-undo"), QStringLiteral("undoMove"),
                      [this, model, source, target, uidValidity, uids]() {
                if (model && model == imapModel())
                    undoMove(source, target, uidValidity, uids);
            }};
        } else {
            // Without COPYUID (RFC 4315) there is no telling which messages in the target are the moved ones
            action = {openText, QStringLiteral("go-jump"), QStringLiteral("openMoveTarget"), [this, model, target]() {
                if (model && model == imapModel())
                    goToMailbox(target);
            }};
        }
        m_notificationBar->showInfo(text, {action});
    });
    connect(task, &Imap::Mailbox::ImapTask::failed, this, [this, model, target, count, kind](const QString &message) {
        if (!model || model != imapModel())
            return;
        switch (kind) {
        case MoveKind::Archive:
            slotMoveToArchiveFailed(message, count);
            break;
        case MoveKind::Trash:
            m_notificationBar->showError(i18ncp("@info", "Could not move %1 message to Trash: %2",
                                                "Could not move %1 messages to Trash: %2", count, message));
            break;
        case MoveKind::Folder:
            m_notificationBar->showError(i18ncp("@info", "Could not move %1 message to %2: %3", "Could not move %1 messages to %2: %3",
                                                count, target, message));
            break;
        }
    });
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
    showBusyWhileRunning(task, busy);
}

void MainWindow::trackCopy(Imap::Mailbox::ImapTask *task, const QString &target, const int count)
{
    if (!task)
        return;
    QPointer<Imap::Mailbox::Model> model = imapModel();
    connect(task, &Imap::Mailbox::ImapTask::completed, this, [this, model, target, count]() {
        if (!model || model != imapModel())
            return;
        const InlineNotificationBar::Action open{i18nc("@action:button", "Open Folder"), QStringLiteral("go-jump"),
                                                 QStringLiteral("openCopyTarget"), [this, model, target]() {
            if (model && model == imapModel())
                goToMailbox(target);
        }};
        m_notificationBar->showInfo(i18ncp("@info", "Copied %1 message to %2", "Copied %1 messages to %2", count, target),
                                    {open});
    });
    connect(task, &Imap::Mailbox::ImapTask::failed, this, [this, model, target, count](const QString &message) {
        if (!model || model != imapModel())
            return;
        m_notificationBar->showError(i18ncp("@info", "Could not copy %1 message to %2: %3", "Could not copy %1 messages to %2: %3",
                                            count, target, message));
    });
    showBusyWhileRunning(task, i18ncp("@info", "Copying %1 message to %2…", "Copying %1 messages to %2…", count, target));
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
        if (model && model == imapModel())
            *busyId = m_notificationBar->showBusy(busyText);
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

void MainWindow::trackFlagChange(Imap::Mailbox::ImapTask *task, const QString &busyText, const QString &doneText,
                                 const std::function<QString(const QString &)> &failedText)
{
    if (!task)
        return;
    QPointer<Imap::Mailbox::Model> model = imapModel();
    if (!doneText.isEmpty()) {
        connect(task, &Imap::Mailbox::ImapTask::completed, this, [this, model, doneText]() {
            if (model && model == imapModel())
                m_notificationBar->showInfo(doneText);
        });
    }
    connect(task, &Imap::Mailbox::ImapTask::failed, this, [this, model, failedText](const QString &message) {
        if (model && model == imapModel())
            m_notificationBar->showError(failedText(message));
    });
    showBusyWhileRunning(task, busyText);
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
        if (!guard || guard != imapModel())
            return;
        m_notificationBar->showInfo(i18ncp("@info", "Moved %1 message back to %2", "Moved %1 messages back to %2", count, source));
        reopenCurrentMailbox(target);
    });
    connect(task, &Imap::Mailbox::ImapTask::failed, this, [this, guard, source, target](const QString &message) {
        if (!guard || guard != imapModel())
            return;
        m_notificationBar->showError(i18nc("@info", "Could not move the messages back to %1: %2", source, message));
        reopenCurrentMailbox(target);
    });
    showBusyWhileRunning(task, i18ncp("@info", "Moving %1 message back to %2…", "Moving %1 messages back to %2…", count, source));
}

void MainWindow::reopenCurrentMailbox(const QString &usedMailbox)
{
    // Only one mailbox is selected per connection; selecting another one leaves the message list stale
    auto *msgList = qobject_cast<Imap::Mailbox::MsgListModel *>(m_imapAccess->msgListModel());
    const QModelIndex current = msgList ? msgList->currentMailbox() : QModelIndex();
    if (current.isValid() && current.data(Imap::Mailbox::RoleMailboxName).toString() != usedMailbox)
        imapModel()->switchToMailbox(current);
}

void MainWindow::slotExpunge()
{
    auto *msgList = qobject_cast<Imap::Mailbox::MsgListModel *>(m_imapAccess->msgListModel());
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

void MainWindow::updateMessageFlagsOf(const QModelIndex &index)
{
    QModelIndexList indexes = index.isValid() ? QModelIndexList() << index : translatedSelection();
    const bool isValid = !indexes.isEmpty() &&
                         // either we operate on the -already valided- selection or the index must be valid
                         (!index.isValid() || index.data(Imap::Mailbox::RoleMessageUid).toUInt() > 0);
    const bool okToModify = imapModel()->isNetworkAvailable() && isValid;

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
#ifdef TROJITA_HAVE_WEBENGINE
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

    m_forwardAsAttachment->setEnabled(m_messageWidget->messageView->currentMessage().isValid());
    m_resend->setEnabled(m_messageWidget->messageView->currentMessage().isValid());
#endif
}

void MainWindow::scrollMessageUp()
{
#ifdef TROJITA_HAVE_WEBENGINE
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
#ifdef TROJITA_HAVE_WEBENGINE
    const QModelIndex messageIndex = m_messageWidget->message();
    m_msgActionHandler.forward(messageIndex, Composer::ForwardMode::FORWARD_AS_ATTACHMENT);
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

    ComposeWidget::warnIfMsaNotConfigured(
                ComposeWidget::createFromReadOnly(this, index, recipients),
                this);

}

void MainWindow::slotComposeMailUrl(const QUrl &url)
{
    ComposeWidget::warnIfMsaNotConfigured(ComposeWidget::createFromUrl(this, url), this);
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
    Gui::Util::messageBoxWarning(this, tr("Can't open mailbox"),
                                 tr("Opening mailbox \"%1\" failed with the following message:\n%2").arg(mailbox, msg));
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
    Q_UNUSED(parserId);
    static Imap::ConnectionState previousState = Imap::ConnectionState::CONN_STATE_NONE;
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
    previousState = state;
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
#ifdef TROJITA_HAVE_CRYPTO_MESSAGES
            true
#else
            false
#endif
                       );
    features.emplace_back(tr("IMAP compression"),
#ifdef TROJITA_HAVE_ZLIB
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
    Gui::Util::messageBoxCritical(this, tr("Can't save into file"),
                                  tr("Unable to save into file. Error:\n%1").arg(errorString));
}

void MainWindow::slotDownloadMessageFileNameRequested(QString *fileName)
{
    *fileName = QFileDialog::getSaveFileName(this, tr("Save Message"), *fileName, QString(), nullptr,
                                             QFileDialog::HideNameFilterDetails);
}

void MainWindow::slotViewMsgSource()
{
#ifdef TROJITA_HAVE_WEBENGINE

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
#ifdef TROJITA_HAVE_WEBENGINE
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
#ifdef TROJITA_HAVE_WEBENGINE
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
    if (Imap::Mailbox::GroupingMsgListModel::isRealGrouping(m_groupBy))
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
        const QStringList terms = UiUtils::relevanceTerms(
            qobject_cast<ThreadingMsgListModel *>(m_imapAccess->threadingMsgListModel())->currentSearchCondition());
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
    msgListWidget->tree->header()->setSortIndicator(column, order);
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
        if (actionThreadMsgList->isChecked())
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
    if (order == Qt::DescendingOrder)
        m_actionSortDescending->setChecked(true);
    else
        m_actionSortAscending->setChecked(true);
}

void MainWindow::slotSearchRequested(const QStringList &searchConditions)
{
    Imap::Mailbox::ThreadingMsgListModel * threadingMsgListModel =
            qobject_cast<Imap::Mailbox::ThreadingMsgListModel *>(m_imapAccess->threadingMsgListModel());
    threadingMsgListModel->setUserSearchingSortingPreference(searchConditions, threadingMsgListModel->currentSortCriterium(),
                                                             threadingMsgListModel->currentSortOrder());
    updateRelevanceSorting(searchConditions);
}

void MainWindow::updateRelevanceSorting(const QStringList &searchConditions)
{
    if (!m_actionSortByRelevance || !prettyMsgListModel)
        return;
    const QStringList terms = UiUtils::relevanceTerms(searchConditions);
    m_actionSortByRelevance->setEnabled(!terms.isEmpty());
    m_actionSortByRelevance->setToolTip(
        terms.isEmpty() ? i18nc("@info:tooltip", "Relevance sorting needs an active quick search or Show Related filter")
                        : i18nc("@info:tooltip", "Show the messages which match the search best first"));
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
}

QModelIndex MainWindow::toPrettyIndex(const QModelIndex &viewIndex) const
{
    if (viewIndex.isValid() && m_groupingMsgListModel && viewIndex.model() == m_groupingMsgListModel)
        return m_groupingMsgListModel->mapToSource(viewIndex);
    return viewIndex;
}

QModelIndex MainWindow::fromPrettyIndex(const QModelIndex &prettyIndex) const
{
    if (prettyIndex.isValid() && m_groupingMsgListModel && msgListWidget->tree->model() == m_groupingMsgListModel)
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
    connect(selection, &QItemSelectionModel::currentChanged, prettyMsgListModel, [this, model = prettyMsgListModel]() {
        // Later, because the current message also changes while the models are busy removing rows
        QMetaObject::invokeMethod(model, [this, model]() {
            const QModelIndex current = msgListWidget->tree->currentIndex();
            // On the header of a group, the message from before stays shown until another one takes its place
            if (!current.data(Imap::Mailbox::RoleIsGroupHeader).toBool())
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
    const bool relevance = m_actionSortByRelevance && m_actionSortByRelevance->isChecked();

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
        if (row.isValid())
            selection.select(row, row.siblingAtColumn(model->columnCount(row.parent()) - 1));
    }
    if (!selection.isEmpty())
        tree->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
    const QModelIndex newCurrent = fromPrettyIndex(current);
    if (newCurrent.isValid()) {
        tree->selectionModel()->setCurrentIndex(newCurrent, QItemSelectionModel::NoUpdate);
        tree->scrollTo(newCurrent);
    }
}

void MainWindow::setMessageListGrouping(Imap::Mailbox::GroupingMsgListModel::Grouping grouping, bool remember)
{
    using Imap::Mailbox::GroupingMsgListModel;
    if (!m_groupingMsgListModel || !m_groupByActions)
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
    if (!m_actionSortNone || !prettyMsgListModel || !msgListWidget->tree->model())
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
    if (m_applyingView || !actionThreadMsgList)
        return;
    const bool grouped = GroupingMsgListModel::isRealGrouping(m_groupBy);
    m_view.groupBy = grouped ? GroupingMsgListModel::groupingName(m_groupBy) : QString();
    m_view.threading = !grouped && actionThreadMsgList->isChecked();
    storeMessageListView();
}

void MainWindow::rememberMessageListSort(int column, Qt::SortOrder order)
{
    if (m_applyingView)
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
    if (m_positionMailbox.isEmpty() || !prettyMsgListModel || m_positionMailbox != currentMailboxName())
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
    if (!m_messageRestorer || m_positionMailbox.isEmpty() || m_positionMailbox != currentMailboxName())
        return;
    const auto position = folderPositions().position(m_positionMailbox);
    if (!position)
        return;
    m_messageRestorer->restorePosition(m_positionMailbox, position->uidValidity, position->uid, position->anchorUid,
                                       position->selectedUids);
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
    actionHideRead->setChecked(filters.testFlag(PrettyMsgListModel::FilterUnread));
    m_actionFilterUnread->setChecked(filters.testFlag(PrettyMsgListModel::FilterUnread));
    m_actionFilterFlagged->setChecked(filters.testFlag(PrettyMsgListModel::FilterFlagged));
    m_actionFilterAttachments->setChecked(filters.testFlag(PrettyMsgListModel::FilterAttachments));
    const bool active = filters != PrettyMsgListModel::NoFilter;
    m_actionClearFilters->setEnabled(active);
    m_clearFiltersButton->setVisible(active);
    scheduleMessageListStatus();
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
    if (mailbox.isValid()) {
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
    msgListWidget->setListStatus(parts.join(QStringLiteral(" \u00B7 ")));
    updateSelectionActions();
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
    const QAbstractItemModel *model = msgListWidget->tree->model();
    const bool hasRows = model && model->rowCount() > 0;
    m_actionSelectUnread->setEnabled(hasRows);
    m_actionSelectFlagged->setEnabled(hasRows);
}

void MainWindow::showRelatedMessages(UiUtils::FieldFilter field)
{
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
        QStringList hidden;
        if (m_embeddedPanelsMenu) {
            for (EmbeddedPanel *panel : std::as_const(m_embeddedPanels)) {
                // Attachments stay, since deciding what to do with a message often depends on them
                if (panel->descriptor().id == QLatin1String("Attachments"))
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
    QString profileName = QString::fromUtf8(qgetenv("TROJITA_PROFILE"));
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

    m_shellRow->addWidget(m_shellLeft.widget);
    m_shellRow->addWidget(m_mainColumn);
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

    // The header region sits above the columns and the footer; it is not resizable
    m_shellHeader = new ShellHeader(KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")));
    auto *shell = new QWidget;
    shell->setObjectName(QStringLiteral("shell"));
    auto *shellLayout = new QVBoxLayout(shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);
    shellLayout->addWidget(m_shellHeader);
    shellLayout->addWidget(m_shellBody, 1);
    setCentralWidget(shell);
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
        if (widget == m_accountSidebar)
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
    for (int i = 0; i < region->count(); ++i) {
        if (qobject_cast<EmbeddedPanel *>(region->widget(i)) && !region->widget(i)->isHidden())
            ++shownPanels;
    }
    if (!shownPanels)
        return;

    // The splitter gives a panel shown after the region was laid out only its minimum size, so size every
    // shown panel explicitly: its remembered size, or its size hint. Side by side, panels share the width.
    const bool horizontal = region->orientation() == Qt::Horizontal;
    QList<int> sizes;
    int anchorIndex = -1;
    int shown = 0;
    int panelTotal = 0;
    for (int i = 0; i < region->count(); ++i) {
        QWidget *widget = region->widget(i);
        int size = 0;
        if (!widget->isHidden()) {
            ++shown;
            if (auto *panel = qobject_cast<EmbeddedPanel *>(widget)) {
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
    const int space = available - region->handleWidth() * std::max(0, shown - 1);
    if (space <= 0 || panelTotal <= 0)
        return;
    // The mailbox tree or the reader keeps at least a third of its region; without either, the panels fill it.
    const int panelSpace = anchorIndex >= 0 ? std::min(panelTotal, space * 2 / 3) : space;
    for (int i = 0; i < sizes.size(); ++i) {
        if (i != anchorIndex && sizes.at(i) > 0)
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
        const bool shown = shellRegionHasContent(region->widget);
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
        apply(m_shellRow, m_mainColumn, {&m_shellLeft, &m_shellRight});
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
    QAbstractItemModel *model = msgListWidget->tree->model();
    const QModelIndexList matches = model && model->rowCount() > 0
        ? model->match(model->index(0, 0), Imap::Mailbox::RoleMessageUid, deproxified.data(Imap::Mailbox::RoleMessageUid), 1,
                       Qt::MatchExactly | Qt::MatchRecursive)
        : QModelIndexList();
    if (!matches.isEmpty()) {
        msgListWidget->tree->setCurrentIndex(matches.constFirst());
        msgListWidget->tree->scrollTo(matches.constFirst());
        msgListClicked(matches.constFirst());
        return;
    }
#ifdef TROJITA_HAVE_WEBENGINE
    m_messageWidget->messageView->setMessage(deproxified);
#endif
}

Imap::Mailbox::Model *MainWindow::imapModel() const
{
    return qobject_cast<Imap::Mailbox::Model *>(m_imapAccess->imapModel());
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
#ifdef TROJITA_HAVE_WEBENGINE
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
    return m_imapAccess;
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
        m_contactPanel->setAddressBookAvailable(addressbook != nullptr);
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
