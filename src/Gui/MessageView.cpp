/* Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>
   Copyright (C) 2014        Luke Dashjr <luke+trojita@dashjr.org>
   Copyright (C) 2014 - 2015 Stephan Platz <trojita@paalsteek.de>

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

#include <KLocalizedString>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QStackedLayout>
#include <QStyle>
#include <QTimer>
#include <QWebEngineFindTextResult>

#include "Common/InvokeMethod.h"
#include "Common/SettingsNames.h"
#include "Cryptography/MessageModel.h"
#include "Gui/MessageView.h"
#include "Gui/ComposeWidget.h"
#include "Gui/EnvelopeView.h"
#include "Gui/ExternalElementsWidget.h"
#include "Gui/InlineNotificationBar.h"
#include "Gui/OverlayWidget.h"
#include "Gui/PartWidgetFactoryVisitor.h"
#include "Gui/ShortcutHandler/ShortcutHandler.h"
#include "Gui/SimplePartWidget.h"
#include "Gui/Spinner.h"
#include "Gui/TagListWidget.h"
#include "Gui/WebEngineMessageProfile.h"
#include "Gui/Window.h"
#include "Imap/Model/DecryptedTextIndexer.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/NetworkWatcher.h"
#include "Imap/Model/SearchIndexer.h"
#include "Imap/Model/Utils.h"
#include "Imap/Network/MsgPartNetAccessManager.h"
#include "Plugins/PluginManager.h"
#include "UiUtils/IconLoader.h"

namespace Gui
{

MessageView::MessageView(QWidget *parent, QSettings *settings, Plugins::PluginManager *pluginManager,
        Imap::Mailbox::FavoriteTagsModel *m_favoriteTags)
    : QWidget(parent)
    , m_stack(new QStackedLayout(this))
    , m_homePage(new QWidget(this))
    , messageModel(0)
    , m_webEngineProfile(nullptr)
    , netAccess(new Imap::Network::MsgPartNetAccessManager(this))
    , factory(new PartWidgetFactory(netAccess, this,
                                    std::unique_ptr<PartWidgetFactoryVisitor>(new PartWidgetFactoryVisitor())))
    , m_settings(settings)
    , m_pluginManager(pluginManager)
{
    connect(netAccess, &Imap::Network::MsgPartNetAccessManager::requestingExternal, this, &MessageView::externalsRequested);


    setBackgroundRole(QPalette::Base);
    setForegroundRole(QPalette::Text);
    setAutoFillBackground(true);
    setFocusPolicy(Qt::StrongFocus); // not by the wheel


    m_zoomIn = ShortcutHandler::instance()->createAction(QStringLiteral("action_zoom_in"), this);
    addAction(m_zoomIn);
    connect(m_zoomIn, &QAction::triggered, this, &MessageView::zoomIn);

    m_zoomOut = ShortcutHandler::instance()->createAction(QStringLiteral("action_zoom_out"), this);
    addAction(m_zoomOut);
    connect(m_zoomOut, &QAction::triggered, this, &MessageView::zoomOut);

    m_zoomOriginal = ShortcutHandler::instance()->createAction(QStringLiteral("action_zoom_original"), this);
    addAction(m_zoomOriginal);
    connect(m_zoomOriginal, &QAction::triggered, this, &MessageView::zoomOriginal);

    m_findAction = new QAction(UiUtils::loadIcon(QStringLiteral("edit-find")), tr("Search..."), this);
    m_findAction->setShortcut(tr("Ctrl+F"));
    // We can search only in one part of the message at a time. If searching is initiated by shortcut,
    // there is no better solution than ask Part Widgets in visible order to handle this request. A
    // callback method called MessageView::triggerSearchDialogBy is provided for this purpose.
    // Other parts are searchable via the context menu only.
    connect(m_findAction, &QAction::triggered, this, &MessageView::searchDialogRequested);
    addAction(m_findAction);

    m_stack->addWidget(m_homePage);

    // Shown when the message itself cannot be loaded, rather than waiting for it forever
    m_unavailablePage = new QWidget(this);
    m_unavailablePage->setObjectName(QStringLiteral("messageUnavailable"));
    auto unavailableLayout = new QVBoxLayout(m_unavailablePage);
    unavailableLayout->addStretch(1);
    m_unavailableIcon = new QLabel(m_unavailablePage);
    m_unavailableIcon->setAlignment(Qt::AlignCenter);
    unavailableLayout->addWidget(m_unavailableIcon);
    m_unavailableTitle = new QLabel(m_unavailablePage);
    m_unavailableTitle->setObjectName(QStringLiteral("messageUnavailableTitle"));
    m_unavailableTitle->setAlignment(Qt::AlignCenter);
    m_unavailableTitle->setWordWrap(true);
    QFont titleFont = m_unavailableTitle->font();
    titleFont.setBold(true);
    m_unavailableTitle->setFont(titleFont);
    unavailableLayout->addWidget(m_unavailableTitle);
    m_unavailableText = new QLabel(m_unavailablePage);
    m_unavailableText->setObjectName(QStringLiteral("messageUnavailableText"));
    m_unavailableText->setAlignment(Qt::AlignCenter);
    m_unavailableText->setWordWrap(true);
    unavailableLayout->addWidget(m_unavailableText);
    m_retryMessage = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")),
                                     i18nc("@action:button", "Try Again"), m_unavailablePage);
    m_retryMessage->setObjectName(QStringLiteral("retryMessage"));
    connect(m_retryMessage, &QPushButton::clicked, this, &MessageView::retryLoading);
    unavailableLayout->addWidget(m_retryMessage, 0, Qt::AlignHCenter);
    unavailableLayout->addStretch(2);
    m_stack->addWidget(m_unavailablePage);

    // The actual widget for the actual message
    m_messageWidget = new QWidget(this);
    auto fullMsgLayout = new QVBoxLayout(m_messageWidget);
    m_stack->addWidget(m_messageWidget);

    m_envelope = new EnvelopeView(m_messageWidget, pluginManager);
    fullMsgLayout->addWidget(m_envelope, 1);

    tags = new TagListWidget(m_messageWidget, m_favoriteTags);
    connect(tags, &TagListWidget::tagAdded, this, &MessageView::newLabelAction);
    connect(tags, &TagListWidget::tagRemoved, this, &MessageView::deleteLabelAction);
    fullMsgLayout->addWidget(tags, 3);

    externalElements = new ExternalElementsWidget(this);
    externalElements->hide();
    connect(externalElements, &ExternalElementsWidget::loadingEnabled, this, &MessageView::enableExternalData);
    fullMsgLayout->addWidget(externalElements, 1);

    m_partErrors = new InlineNotificationBar(m_messageWidget);
    m_partErrors->setObjectName(QStringLiteral("partLoadErrors"));
    m_partErrors->hide();
    fullMsgLayout->addWidget(m_partErrors);

    // put the actual messages into an extra horizontal view
    // this allows us easy usage of the trailing stretch and also to indent the message a bit
    m_msgLayout = new QHBoxLayout;
    m_msgLayout->setContentsMargins(6,6,6,0);
    fullMsgLayout->addLayout(m_msgLayout, 1);
    // add a strong stretch to squeeze header and message to the top
    // possibly passing a large stretch factor to the message could be enough...
    fullMsgLayout->addStretch(1000);


    markAsReadTimer = new QTimer(this);
    markAsReadTimer->setSingleShot(true);
    connect(markAsReadTimer, &QTimer::timeout, this, &MessageView::markAsRead);

    m_loadingSpinner = new Spinner(this);
    m_loadingSpinner->setText(tr("Fetching\nMessage"));
    m_loadingSpinner->setType(Spinner::Sun);
}

MessageView::~MessageView()
{
    // Destroy the WebEngine views before the network manager because pending replies are owned by that manager.
    unsetPreviousMessage();
}

Imap::Network::MsgPartNetAccessManager *MessageView::networkAccessManager() const
{
    return netAccess;
}

void MessageView::unsetPreviousMessage()
{
    clearWaitingConns();
    m_loadingItems.clear();
    m_highlightPart = nullptr;
    m_failedParts.clear();
    m_partErrors->hide();
    message = QModelIndex();
    markAsReadTimer->stop();
    if (auto w = bodyWidget()) {
        m_stack->removeWidget(dynamic_cast<QWidget *>(w));
        delete w;
    }
    delete m_webEngineProfile;
    m_webEngineProfile = nullptr;
    m_envelope->setMessage(QModelIndex());
    delete messageModel;
    messageModel = nullptr;
}

void MessageView::setEmpty()
{
    unsetPreviousMessage();
    m_loadingSpinner->stop();
    m_stack->setCurrentWidget(m_homePage);
    emit messageChanged();
}

AbstractPartWidget *MessageView::bodyWidget() const
{
    if (m_msgLayout->itemAt(0) && m_msgLayout->itemAt(0)->widget()) {
        return dynamic_cast<AbstractPartWidget *>(m_msgLayout->itemAt(0)->widget());
    } else {
        return nullptr;
    }
}

void MessageView::setMessage(const QModelIndex &index)
{
    Q_ASSERT(index.isValid());
    QModelIndex messageIndex = Imap::deproxifiedIndex(index);
    Q_ASSERT(messageIndex.isValid());

    if (message == messageIndex) {
        // This is a duplicate call, let's do nothing.
        // It might not be our fat-fingered user, but also just a side-effect of our duplicate invocation through
        // QAbstractItemView::clicked() and activated().
        return;
    }

    unsetPreviousMessage();

    message = messageIndex;
    messageModel = new Cryptography::MessageModel(this, message);
    messageModel->setObjectName(QStringLiteral("cryptoMessageModel-%1-%2")
                                .arg(message.data(Imap::Mailbox::RoleMailboxName).toString(),
                                     message.data(Imap::Mailbox::RoleMessageUid).toString()));
    for (const auto &module: m_pluginManager->mimePartReplacers()) {
        messageModel->registerPartHandler(module);
    }
    if (auto *indexer = Imap::Mailbox::SearchIndexer::forModel(message.model())) {
        // Offers the text of an encrypted message to the index once it is decrypted; the index decides whether to keep it
        new Imap::Mailbox::DecryptedTextIndexer(messageModel, indexer,
                                                message.data(Imap::Mailbox::RoleMailboxName).toString(),
                                                message.data(Imap::Mailbox::RoleMessageUid).toUInt());
    }
    emit messageModelChanged(messageModel);

    // The data might be available from the local cache, so let's try to save a possible roundtrip here
    // by explicitly requesting the data
    message.data(Imap::Mailbox::RolePartData);

    if (!message.data(Imap::Mailbox::RoleIsFetched).toBool()) {
        // This happens when the message placeholder is already available in the GUI, but the actual message data haven't been
        // loaded yet. This is especially common with the threading model, but also with bigger unsynced mailboxes.
        // Note that the data might be already available in the cache, it's just that it isn't in the mailbox tree yet.
        m_waitingMessageConns.emplace_back(
                    connect(messageModel, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &topLeft){
            if (topLeft.data(Imap::Mailbox::RoleIsFetched).toBool()) {
                // OK, message is fully fetched now
                showMessageNow();
            } else if (message.data(Imap::Mailbox::RoleIsUnavailable).toBool()) {
                showMessageUnavailable();
            }
        }));
        if (message.data(Imap::Mailbox::RoleIsUnavailable).toBool()) {
            showMessageUnavailable();
        } else {
            if (m_stack->currentWidget() == m_unavailablePage)
                m_stack->setCurrentWidget(m_homePage);
            m_loadingSpinner->setText(tr("Waiting\nfor\nMessage..."));
            m_loadingSpinner->start();
        }
    } else {
        showMessageNow();
    }
}

/** @short Implementation of the "hey, let's really display the message, its BODYSTRUCTURE is available now" */
void MessageView::showMessageNow()
{
    Q_ASSERT(message.data(Imap::Mailbox::RoleIsFetched).toBool());

    clearWaitingConns();

    QModelIndex rootPartIndex = messageModel->index(0,0);
    Q_ASSERT(messageModel->index(0, 0, rootPartIndex).isValid());

    netAccess->setExternalsEnabled(false);
    externalElements->hide();
    externalElements->clear();

    netAccess->setModelMessage(rootPartIndex);
    m_webEngineProfile = new WebEngineMessageProfile(rootPartIndex, this);
    connect(m_webEngineProfile, &WebEngineMessageProfile::remoteImageBlocked,
            this, &MessageView::externalsRequested);

    m_loadingItems.clear();
    m_loadingSpinner->stop();

    m_envelope->setMessage(message);

    auto updateTagList = [this]() {
        tags->setTagList(m_taghandler.tags(message));
    };
    connect(messageModel, &QAbstractItemModel::dataChanged, this, updateTagList);
    updateTagList();

    UiUtils::PartLoadingOptions loadingMode;
    if (m_settings->value(Common::SettingsNames::guiPreferPlaintextRendering, QVariant(true)).toBool())
        loadingMode |= UiUtils::PART_PREFER_PLAINTEXT_OVER_HTML;
    auto viewer = factory->walk(messageModel->index(0,0, rootPartIndex), 0, loadingMode);
    viewer->setParent(this);
    m_msgLayout->addWidget(viewer);
    m_msgLayout->setAlignment(viewer, Qt::AlignTop|Qt::AlignLeft);
    viewer->show();
    // We want to propagate the QWheelEvent to upper layers
    viewer->installEventFilter(this);
    m_stack->setCurrentWidget(m_messageWidget);

    if (m_netWatcher && m_netWatcher->effectiveNetworkPolicy() != Imap::Mailbox::NETWORK_OFFLINE
            && m_settings->value(Common::SettingsNames::autoMarkReadEnabled, QVariant(true)).toBool()) {
        // No additional delay is needed here because the MsgListView won't open a message while the user keeps scrolling,
        // which was AFAIK the original intention
        markAsReadTimer->start(m_settings->value(Common::SettingsNames::autoMarkReadSeconds, QVariant(0)).toUInt() * 1000);
    }

    emit messageChanged();
}

/** @short There's no point in waiting for the message to appear */
void MessageView::clearWaitingConns()
{
    for (auto &conn: m_waitingMessageConns) {
        disconnect(conn);
    }
    m_waitingMessageConns.clear();
}

void MessageView::markAsRead()
{
    if (!message.isValid())
        return;
    Imap::Mailbox::Model *model = const_cast<Imap::Mailbox::Model *>(dynamic_cast<const Imap::Mailbox::Model *>(message.model()));
    Q_ASSERT(model);
    if (!model->isNetworkAvailable())
        return;
    if (!message.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool())
        model->markMessagesRead(QModelIndexList() << message, Imap::Mailbox::FLAG_ADD);
}

/** @short Inhibit the automatic marking of the current message as already read

The user might have e.g. explicitly marked a previously read message as unread again immediately after navigating back to it
in the message listing. In that situation, the message viewer shall respect this decision and inhibit the helper which would
otherwise mark the current message as read after a short timeout.
*/
void MessageView::stopAutoMarkAsRead()
{
    markAsReadTimer->stop();
}

bool MessageView::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        if (static_cast<QWheelEvent *>(event)->modifiers() == Qt::ControlModifier) {
            if (static_cast<QWheelEvent *>(event)->angleDelta().y() > 0) {
                zoomIn();
            } else {
                zoomOut();
            }
        } else {
            // while the containing scrollview has Qt::StrongFocus, the event forwarding breaks that
            // -> completely disable focus for the following wheel event ...
            parentWidget()->setFocusPolicy(Qt::NoFocus);
            MessageView::event(event);
            // ... set reset it
            parentWidget()->setFocusPolicy(Qt::StrongFocus);
        }
        return true;
    } else if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key()) {
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            MessageView::event(event);
            return true;
        case Qt::Key_Home:
        case Qt::Key_End:
            return false;
        default:
            return QObject::eventFilter(object, event);
        }
    } else {
        return QObject::eventFilter(object, event);
    }
}

void MessageView::quoteText(const std::function<void(const QString &)> &callback) const
{
    if (auto w = bodyWidget()) {
        w->quoteMeAsync(callback);
    } else {
        callback(QString());
    }
}

bool MessageView::isCryptoMessage() const
{
    QModelIndex index = messageModel->index(0, 0); /* fake message root */
    if (messageModel->index(0, 0, index)  /* first MIME part */.data(Imap::Mailbox::RolePartDecryptionSupported).toBool()) {
        // This is just an UX improvement shortcut: real filtering for CVE-2019-10734 is in
        // MultipartSignedEncryptedWidget::quoteMe().
        // That is required because the encrypted part might not be the root part of the message.
        return true;
    }

    return false;
}

#define FORWARD_METHOD(METHOD) \
void MessageView::METHOD() \
{ \
    if (auto w = bodyWidget()) { \
        w->METHOD(); \
    } \
}
FORWARD_METHOD(zoomIn)
FORWARD_METHOD(zoomOut)
FORWARD_METHOD(zoomOriginal)
FORWARD_METHOD(searchDialogRequested)

void MessageView::setAccountResolver(const std::function<QString(const QModelIndex &)> &resolver)
{
    m_envelope->setAccountResolver(resolver);
}

void MessageView::setNetworkWatcher(Imap::Mailbox::NetworkWatcher *netWatcher)
{
    if (m_netWatcher)
        disconnect(m_netWatcher, nullptr, this, nullptr);
    m_netWatcher = netWatcher;
    factory->setNetworkWatcher(netWatcher);
    if (m_netWatcher) {
        connect(m_netWatcher, &Imap::Mailbox::NetworkWatcher::effectiveNetworkPolicyChanged,
                this, &MessageView::onNetworkPolicyChanged);
    }
}

bool MessageView::isOffline() const
{
    if (message.isValid())
        return message.data(Imap::Mailbox::RoleIsNetworkOffline).toBool();
    return m_netWatcher && m_netWatcher->effectiveNetworkPolicy() == Imap::Mailbox::NETWORK_OFFLINE;
}

void MessageView::showMessageUnavailable()
{
    m_loadingSpinner->stop();
    const bool offline = isOffline();
    const QString iconName = offline ? QStringLiteral("network-disconnect") : QStringLiteral("dialog-warning");
    const int iconSize = style()->pixelMetric(QStyle::PM_LargeIconSize) * 2;
    m_unavailableIcon->setPixmap(QIcon::fromTheme(iconName).pixmap(iconSize, iconSize));
    if (offline) {
        m_unavailableTitle->setText(i18nc("@info", "This message is not available offline"));
        m_unavailableText->setText(i18nc("@info", "It has not been downloaded yet. It loads when you go online."));
    } else {
        m_unavailableTitle->setText(i18nc("@info", "This message could not be loaded"));
        m_unavailableText->setText(i18nc("@info", "The server did not send it. Try again, or check the connection."));
    }
    m_retryMessage->setVisible(!offline);
    m_stack->setCurrentWidget(m_unavailablePage);
}

void MessageView::reloadMessage()
{
    const QPersistentModelIndex current = message;
    if (!current.isValid())
        return;
    unsetPreviousMessage();
    setMessage(current);
}

void MessageView::retryLoading()
{
    if (!message.isValid())
        return;

    if (!m_waitingMessageConns.empty()) {
        // The structure of the message is not here yet
        message.data(Imap::Mailbox::RoleRetryIfUnavailable);
        if (m_waitingMessageConns.empty()) {
            // The data arrived synchronously, for example from the cache, and the message is shown already
            return;
        }
        if (message.data(Imap::Mailbox::RoleIsFetched).toBool()) {
            showMessageNow();
        } else if (message.data(Imap::Mailbox::RoleIsUnavailable).toBool()) {
            showMessageUnavailable();
        } else {
            m_stack->setCurrentWidget(m_homePage);
            m_loadingSpinner->setText(tr("Waiting\nfor\nMessage..."));
            m_loadingSpinner->start();
        }
        return;
    }

    const auto failed = m_failedParts;
    m_failedParts.clear();
    updateFailedParts();
    for (const auto &part : failed) {
        if (part)
            part->retryLoading();
    }
}

void MessageView::updateFailedParts()
{
    m_failedParts.removeAll(QPointer<SimplePartWidget>());
    if (m_failedParts.isEmpty()) {
        m_partErrors->hide();
        return;
    }
    const int count = m_failedParts.size();
    if (isOffline()) {
        m_partErrors->showError(i18ncp("@info",
                                       "Part of this message is not available offline. It loads when you go online.",
                                       "%1 parts of this message are not available offline. They load when you go online.",
                                       count));
    } else {
        m_partErrors->showError(i18ncp("@info",
                                       "Part of this message could not be loaded.",
                                       "%1 parts of this message could not be loaded.",
                                       count),
                                {{i18nc("@action:button", "Try Again"), QStringLiteral("view-refresh"),
                                  QStringLiteral("retryParts"), [this]() { retryLoading(); }}});
    }
}

void MessageView::onNetworkPolicyChanged()
{
    if (!message.isValid())
        return;
    if (isOffline()) {
        if (m_stack->currentWidget() == m_unavailablePage)
            showMessageUnavailable();
        updateFailedParts();
    } else if (m_stack->currentWidget() == m_unavailablePage || !m_failedParts.isEmpty()) {
        retryLoading();
    }
}

void MessageView::externalsRequested(const QUrl &url)
{
    externalElements->addBlockedUrl(url);
    externalElements->show();
}

void MessageView::enableExternalData()
{
    netAccess->setExternalsEnabled(true);
    if (m_webEngineProfile)
        m_webEngineProfile->setRemoteImagesAllowed(true);
    externalElements->hide();
    if (auto w = bodyWidget()) {
        w->reloadContents();
    }
}

void MessageView::newLabelAction(const QString &tag)
{
    m_taghandler.addTag(message, tag);
}

void MessageView::deleteLabelAction(const QString &tag)
{
    m_taghandler.removeTag(message, tag);
}

void MessageView::showEvent(QShowEvent *se)
{
    QWidget::showEvent(se);
    // The Oxygen style reset the attribute - since we're gonna cause an update() here anyway, it's
    // a good moment to stress that "we know better, Hugo ;-)" -- Thomas
    setAutoFillBackground(true);
}

void MessageView::partContextMenuRequested(const QPoint &point)
{
    if (SimplePartWidget *w = qobject_cast<SimplePartWidget *>(sender())) {
        QMenu menu(w);
        w->buildContextMenu(point, menu);
        menu.exec(w->mapToGlobal(point));
    }
}

void MessageView::partLinkHovered(const QString &link, const QString &title, const QString &textContent)
{
    Q_UNUSED(title);
    Q_UNUSED(textContent);
    emit linkHovered(link);
}

QSettings* MessageView::profileSettings() const
{
    return m_settings;
}

WebEngineMessageProfile *MessageView::webEngineProfile() const
{
    return m_webEngineProfile;
}

QModelIndex MessageView::currentMessage() const
{
    return message;
}

void MessageView::onWebViewLoadStarted()
{
    QWebEngineView *wv = qobject_cast<QWebEngineView*>(sender());
    Q_ASSERT(wv);

    if (m_netWatcher && m_netWatcher->effectiveNetworkPolicy() != Imap::Mailbox::NETWORK_OFFLINE) {
        m_loadingItems << wv;
        m_loadingSpinner->start(250);
    }
}

void MessageView::onWebViewLoadFinished(bool ok)
{
    QWebEngineView *wv = qobject_cast<QWebEngineView*>(sender());
    Q_ASSERT(wv);
    m_loadingItems.remove(wv);
    if (m_loadingItems.isEmpty())
        m_loadingSpinner->stop();

    if (auto part = qobject_cast<SimplePartWidget *>(wv)) {
        const QPointer<SimplePartWidget> ptr(part);
        if (!ok && part->isUnavailable()) {
            if (!m_failedParts.contains(ptr))
                m_failedParts << ptr;
            updateFailedParts();
        } else if (ok && m_failedParts.removeAll(ptr)) {
            updateFailedParts();
        }
    }

    // A part which reloads, such as plain text getting its markup, is searched again
    if (auto view = qobject_cast<EmbeddedWebView *>(wv))
        findHighlightTerm(view, 3);
}

void MessageView::findHighlightTerm(EmbeddedWebView *view, int retries)
{
    if (m_highlightTerm.isEmpty() || (m_highlightPart && m_highlightPart != view))
        return;
    // Searching the rendered part is local; it never enables remote content
    const QString term = m_highlightTerm;
    QPointer<MessageView> self(this);
    QPointer<EmbeddedWebView> part(view);
    // A plain text part loads twice, the second time with its markup. Repeating a search for the same text
    // would continue the earlier one, which the reload has cancelled, so start afresh.
    view->findText(QString());
    view->findText(term, {}, [self, part, term, retries](const QWebEngineFindTextResult &result) {
        if (!self || !part || self->m_highlightTerm != term)
            return;
        if (result.numberOfMatches() == 0) {
            // A search which overlaps a reload is cancelled and reports no matches, so try again shortly
            if (retries > 0) {
                QTimer::singleShot(200, part, [self, part, retries]() {
                    if (self && part)
                        self->findHighlightTerm(part, retries - 1);
                });
            }
            return;
        }
        if (self->m_highlightPart && self->m_highlightPart != part) {
            // Another part of this message was first
            part->findText(QString());
            return;
        }
        self->m_highlightPart = part;
        emit self->highlightTermFound(part, term);
    });
}

void MessageView::setHighlightTerm(const QString &term)
{
    m_highlightTerm = term;
}

QString MessageView::highlightTerm() const
{
    return m_highlightTerm;
}

Plugins::PluginManager *MessageView::pluginManager() const
{
    return m_pluginManager;
}

/** @short Callback for AbstractPartWidget */
QAction *MessageView::findAction() const
{
    return m_findAction;
}

void MessageView::triggerSearchDialogBy(EmbeddedWebView *w)
{
    emit searchRequestedBy(w);
};

}
