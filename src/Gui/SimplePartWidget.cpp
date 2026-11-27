/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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
#include <QDir>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFontDatabase>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QNetworkReply>
#include <QWebEngineContextMenuRequest>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWheelEvent>
#include <QWidgetAction>

#include "SimplePartWidget.h"
#include "Common/MetaTypes.h"
#include "Common/Paths.h"
#include "Common/PrivateFiles.h"
#include "Composer/Mailto.h"
#include "Gui/MessageView.h" // so that the compiler knows that it's a QObject
#include "Gui/LinkTarget.h"
#include "Gui/WebEngineMessagePage.h"
#include "Gui/Util.h"
#include "Imap/Encoders.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "Imap/Network/FileDownloadManager.h"
#include "Imap/Model/Utils.h"
#include "UiUtils/Color.h"
#include "UiUtils/IconLoader.h"

namespace Gui
{

SimplePartWidget::SimplePartWidget(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager,
                                   const QModelIndex &partIndex, MessageView *messageView):
    EmbeddedWebView(parent, messageView->webEngineProfile(), messageView->profileSettings()),
    m_partIndex(partIndex), m_messageView(messageView), m_netAccessManager(manager)
{
    Q_ASSERT(partIndex.isValid());
    Q_ASSERT(messageView);

    if (m_messageView) {
        connect(this, &QWebEngineView::loadStarted, m_messageView, &MessageView::onWebViewLoadStarted);
        connect(this, &QWebEngineView::loadFinished, m_messageView, &MessageView::onWebViewLoadFinished);
    }

    m_partUrl.setScheme(QStringLiteral("trojita-imap"));
    m_partUrl.setHost(QStringLiteral("msg"));
    m_partUrl.setPath(partIndex.data(Imap::Mailbox::RolePartPathToPart).toString());
    if (partIndex.data(Imap::Mailbox::RolePartMimeType).toString() == QLatin1String("text/plain")) {
        if (partIndex.data(Imap::Mailbox::RolePartOctets).toULongLong() < 100 * 1024) {
            connect(this, &QWebEngineView::loadFinished, this, &SimplePartWidget::slotMarkupPlainText);
        } else {
            QFont font(QFontDatabase::systemFont(QFontDatabase::FixedFont));
            setStaticWidth(QFontMetrics(font).maxWidth()*90);
            addCustomStylesheet(QStringLiteral("pre{word-wrap:normal !important;white-space:pre !important;}"));
            settings()->setFontFamily(QWebEngineSettings::StandardFont, font.family());
        }
    }
    if (!messagePage()->loadMessagePart(m_partUrl))
        qWarning() << "Cannot load message part" << m_partUrl;

    m_savePart = new QAction(UiUtils::loadIcon(QStringLiteral("document-save")), tr("Save this message part..."), this);
    connect(m_savePart, &QAction::triggered, this, &SimplePartWidget::slotDownloadPart);
    this->addAction(m_savePart);

    m_saveMessage = new QAction(UiUtils::loadIcon(QStringLiteral("document-save-all")), tr("Save whole message..."), this);
    connect(m_saveMessage, &QAction::triggered, this, &SimplePartWidget::slotDownloadMessage);
    this->addAction(m_saveMessage);

    m_findAction = new QAction(UiUtils::loadIcon(QStringLiteral("edit-find")), tr("Search..."), this);
    connect(m_findAction, &QAction::triggered, this, &SimplePartWidget::searchDialogRequested);

    m_copyMail = new QAction(UiUtils::loadIcon(QStringLiteral("edit-copy")), tr("Copy e-mail address"), this);
    connect(m_copyMail, &QAction::triggered, this, [this](){
        QGuiApplication::clipboard()->setText(m_copyMail->data().toString());
    });
    this->addAction(m_copyMail);

    setContextMenuPolicy(Qt::CustomContextMenu);

    // It is actually OK to construct this widget without any connection to a messageView -- this is often used when
    // displaying message source or message headers. Let's silence the QObject::connect warning.
    if (m_messageView) {
        connect(this, &QWidget::customContextMenuRequested, m_messageView, &MessageView::partContextMenuRequested);
        // The targets expect the sender() of the signal to be a SimplePartWidget, not a QWebPage,
        // which means we have to do this indirection
        connect(page(), &QWebEnginePage::linkHovered, this, [this](const QString &link) {
            m_hoveredLink = link;
            if (link.isEmpty()) {
                emit linkHovered(link, QString(), QString());
                return;
            }
            // The link's text is looked up first, so that a text which shows another address can be pointed out
            QPointer<SimplePartWidget> self(this);
            messagePage()->linkText(link, WebEngineMessagePage::LinkSource::Hovered, [self, link](const QString &text) {
                if (self && self->m_hoveredLink == link)
                    emit self->linkHovered(link, QString(), text);
            });
        });
        connect(this, &SimplePartWidget::linkHovered, m_messageView, &MessageView::partLinkHovered);

        installEventFilter(m_messageView);
    }
}

void SimplePartWidget::slotMarkupPlainText(bool success)
{
    // A failed load keeps the markup pending for a later retry
    if (!success || !m_partIndex.isValid() || !m_partIndex.data(Imap::Mailbox::RoleIsFetched).toBool())
        return;

    disconnect(this, &QWebEngineView::loadFinished, this, &SimplePartWidget::slotMarkupPlainText);

    QPalette palette = QApplication::palette();

    // and finally set the marked up page.
    const QString html = UiUtils::htmlizedTextPart(m_partIndex, QFontDatabase::systemFont(QFontDatabase::FixedFont),
                                                   palette.base().color(), palette.text().color(),
                                                   palette.link().color(), palette.linkVisited().color());
    if (!messagePage()->setMessageHtml(html, m_partUrl))
        qWarning() << "Cannot display generated markup for" << m_partUrl;
}

void SimplePartWidget::slotFileNameRequested(QString *fileName)
{
    *fileName = QFileDialog::getSaveFileName(this, tr("Save Attachment"),
                QDir(Gui::Util::savedFilesFolder()).filePath(Common::PrivateFiles::safeFileName(*fileName)), QString(),
                0, QFileDialog::HideNameFilterDetails
                                            );
    Gui::Util::rememberSavedFile(*fileName);
}

QString SimplePartWidget::quoteMe() const
{
    return selectedText();
}

void SimplePartWidget::quoteMeAsync(const QuoteCallback &callback) const
{
    messagePage()->quoteText(callback);
}

bool SimplePartWidget::isUnavailable() const
{
    return !m_partIndex.isValid() || m_partIndex.data(Imap::Mailbox::RoleIsUnavailable).toBool();
}

void SimplePartWidget::retryLoading()
{
    if (!m_partIndex.isValid())
        return;
    m_partIndex.data(Imap::Mailbox::RoleRetryIfUnavailable);
    if (!messagePage()->loadMessagePart(m_partUrl))
        qWarning() << "Cannot load message part" << m_partUrl;
}

void SimplePartWidget::reloadContents()
{
    EmbeddedWebView::reload();
}

const auto zoomConstant = 1.1;

void SimplePartWidget::zoomIn()
{
    setZoomFactor(zoomFactor() * zoomConstant);
    constrainSize();
}

void SimplePartWidget::zoomOut()
{
    setZoomFactor(zoomFactor() / zoomConstant);
    constrainSize();
}

void SimplePartWidget::zoomOriginal()
{
    setZoomFactor(1);
    constrainSize();
}

bool SimplePartWidget::searchDialogRequested()
{
    m_messageView->triggerSearchDialogBy(this);
    return true;
}

void SimplePartWidget::buildContextMenu(const QPoint &point, QMenu &menu)
{
    menu.addAction(m_findAction);
    auto a = pageAction(QWebEnginePage::Copy);
    a->setIcon(UiUtils::loadIcon(QStringLiteral("edit-copy")));
    menu.addAction(a);
    a = pageAction(QWebEnginePage::SelectAll);
    a->setIcon(UiUtils::loadIcon(QStringLiteral("edit-select-all")));
    menu.addAction(a);
    const QWebEngineContextMenuRequest *request = lastContextMenuRequest();
    const QUrl linkUrl = request ? request->linkUrl() : QUrl();
    if (!linkUrl.isEmpty()) {
        menu.addSeparator();
        // The full target and any reasons for caution, before the link is opened or copied
        auto *target = new QLabel(linkTargetDescription(linkUrl, request->linkText()), &menu);
        target->setObjectName(QStringLiteral("linkTarget"));
        target->setTextFormat(Qt::RichText);
        target->setWordWrap(true);
        target->setMaximumWidth(target->fontMetrics().averageCharWidth() * 60);
        const int margin = target->fontMetrics().height() / 2;
        target->setContentsMargins(margin, margin / 2, margin, margin / 2);
        auto *targetAction = new QWidgetAction(&menu);
        targetAction->setDefaultWidget(target);
        targetAction->setEnabled(false);
        menu.addAction(targetAction);
        const QString scheme = linkUrl.scheme().toLower();
        if (scheme == QLatin1String("http") || scheme == QLatin1String("https") || scheme == QLatin1String("mailto")) {
            a = menu.addAction(UiUtils::loadIcon(QStringLiteral("internet-services")), tr("Open Link"));
            a->setObjectName(QStringLiteral("openLink"));
            connect(a, &QAction::triggered, this, [this, linkUrl, text = request->linkText()] {
                slotLinkClicked(linkUrl, text);
            });
        }
        auto oneMail = Composer::extractOneMailAddress(linkUrl);
        if (!oneMail.isEmpty()) {
            a = m_copyMail;
            a->setData(oneMail);
        } else {
            a = pageAction(QWebEnginePage::CopyLinkToClipboard);
            a->setIcon(UiUtils::loadIcon(QStringLiteral("edit-copy")));
        }
        menu.addAction(a);
    }
    menu.addSeparator();
    menu.addAction(m_savePart);
    menu.addAction(m_saveMessage);
    if (request && request->mediaType() == QWebEngineContextMenuRequest::MediaTypeImage) {
        a = menu.addAction(UiUtils::loadIcon(QStringLiteral("download")), tr("Download Image..."));
        connect(a, &QAction::triggered, this, [this, url = request->mediaUrl()] {
            slotDownloadImage(url);
        });
    }
    menu.addSeparator();
    QMenu *colorSchemeMenu = menu.addMenu(UiUtils::loadIcon(QStringLiteral("colorneg")), tr("Color scheme"));
    QActionGroup *ag = new QActionGroup(colorSchemeMenu);
    for (auto item: supportedColorSchemes()) {
        QAction *a = colorSchemeMenu->addAction(item.second);
        connect(a, &QAction::triggered, this, [this, item](){
           changeColorScheme(item.first);
        });
        a->setCheckable(true);
        if (item.first == m_colorScheme) {
            a->setChecked(true);
        }
        a->setActionGroup(ag);
    }

    auto zoomMenu = menu.addMenu(UiUtils::loadIcon(QStringLiteral("zoom")), tr("Zoom"));
    if (m_messageView) {
        zoomMenu->addAction(m_messageView->m_zoomIn);
        zoomMenu->addAction(m_messageView->m_zoomOut);
        zoomMenu->addAction(m_messageView->m_zoomOriginal);
    } else {
        auto zoomIn = zoomMenu->addAction(UiUtils::loadIcon(QStringLiteral("zoom-in")), tr("Zoom In"));
        zoomIn->setShortcut(QKeySequence::ZoomIn);
        connect(zoomIn, &QAction::triggered, this, &SimplePartWidget::zoomIn);

        auto zoomOut = zoomMenu->addAction(UiUtils::loadIcon(QStringLiteral("zoom-out")), tr("Zoom Out"));
        zoomOut->setShortcut(QKeySequence::ZoomOut);
        connect(zoomOut, &QAction::triggered, this, &SimplePartWidget::zoomOut);

        auto zoomOriginal = zoomMenu->addAction(UiUtils::loadIcon(QStringLiteral("zoom-original")), tr("Original Size"));
        connect(zoomOriginal, &QAction::triggered, this, &SimplePartWidget::zoomOriginal);
    }
}

void SimplePartWidget::slotDownloadPart()
{
    Imap::Network::FileDownloadManager *manager = new Imap::Network::FileDownloadManager(this, m_netAccessManager, m_partIndex);
    connect(manager, &Imap::Network::FileDownloadManager::fileNameRequested, this, &SimplePartWidget::slotFileNameRequested);
    connect(manager, &Imap::Network::FileDownloadManager::transferError, m_messageView, &MessageView::transferError);
    connect(manager, &Imap::Network::FileDownloadManager::transferError, manager, &QObject::deleteLater);
    connect(manager, &Imap::Network::FileDownloadManager::succeeded, manager, &QObject::deleteLater);
    manager->downloadPart();
}

void SimplePartWidget::slotDownloadMessage()
{
    QModelIndex index = m_partIndex.data(Imap::Mailbox::RolePartMessageIndex).toModelIndex();
    Imap::Network::FileDownloadManager *manager = new Imap::Network::FileDownloadManager(this, m_netAccessManager, index);
    connect(manager, &Imap::Network::FileDownloadManager::fileNameRequested, this, &SimplePartWidget::slotFileNameRequested);
    connect(manager, &Imap::Network::FileDownloadManager::transferError, m_messageView, &MessageView::transferError);
    connect(manager, &Imap::Network::FileDownloadManager::transferError, manager, &QObject::deleteLater);
    connect(manager, &Imap::Network::FileDownloadManager::succeeded, manager, &QObject::deleteLater);
    manager->downloadMessage();
}

void SimplePartWidget::slotDownloadImage(const QUrl &url)
{
    Imap::Network::FileDownloadManager *manager = new Imap::Network::FileDownloadManager(this, m_netAccessManager, url, m_partIndex.parent());
    connect(manager, &Imap::Network::FileDownloadManager::fileNameRequested, this, &SimplePartWidget::slotFileNameRequested);
    connect(manager, &Imap::Network::FileDownloadManager::transferError, m_messageView, &MessageView::transferError);
    connect(manager, &Imap::Network::FileDownloadManager::transferError, manager, &QObject::deleteLater);
    connect(manager, &Imap::Network::FileDownloadManager::succeeded, manager, &QObject::deleteLater);
    manager->downloadPart();
}

}

