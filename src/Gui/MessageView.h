/* Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>
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
#ifndef VIEW_MESSAGEVIEW_H
#define VIEW_MESSAGEVIEW_H

#include <QPersistentModelIndex>
#include <QPointer>
#include <QSet>
#include <QWidget>
#include <functional>
#include "Composer/Recipients.h"
#include "Gui/PartWalker.h"
#include "Imap/Model/FavoriteTagsModel.h"
#include "TagHandler.h"

class QBoxLayout;
class QLabel;
class QLayout;
class QPushButton;
class QSettings;
class QStackedLayout;
class QTimer;
class QUrl;
class QWebEngineView;

namespace Cryptography {
class MessageModel;
}

namespace Imap {
namespace Network {
class MsgPartNetAccessManager;
}
namespace Message {
class Envelope;
}
namespace Mailbox {
class NetworkWatcher;
}
}

namespace Plugins {
class PluginManager;
}

namespace Gui {

class AbstractPartWidget;
class EmbeddedWebView;
class EnvelopeView;
class MainWindow;
class ExternalElementsWidget;
class InlineNotificationBar;
class SimplePartWidget;
class Spinner;
class TagListWidget;
class WebEngineMessageProfile;


/** @short Widget for displaying complete e-mail messages as available from the IMAP server

  Widget which can render a regular message as exported by the Imap::Mailbox::Model model.
  Notably, this class will not render message/rfc822 MIME parts.
*/
class MessageView : public QWidget
{
    Q_OBJECT
public:
    MessageView(QWidget *parent, QSettings *settings, Plugins::PluginManager *pluginManager,
            Imap::Mailbox::FavoriteTagsModel *m_favoriteTags);
    ~MessageView() override;

    void setNetworkWatcher(Imap::Mailbox::NetworkWatcher *netWatcher);
    /** @short Name the account of the shown message in its header; an empty name leaves it out */
    void setAccountResolver(const std::function<QString(const QModelIndex &)> &resolver);
    QModelIndex currentMessage() const;
    /** @short Find text in the shown message; the main window gives Find to it while the reader has focus */
    QAction *findAction() const;
    Plugins::PluginManager *pluginManager() const;
    QSettings* profileSettings() const;
    WebEngineMessageProfile *webEngineProfile() const;
    void quoteText(const std::function<void(const QString &)> &callback) const;
    bool isCryptoMessage() const;
    /** The network manager which serves the parts of the current message */
    Imap::Network::MsgPartNetAccessManager *networkAccessManager() const;
    /** @short Text from the active search to find in the next messages which are opened

    An empty string turns this off. The message which is shown already is left alone. */
    void setHighlightTerm(const QString &term);
    QString highlightTerm() const;
    /** @short Ask again for the current message, or for its parts which could not be loaded */
    void retryLoading();
    /** @short Show the current message again, for example after the preferred rendering changed */
    void reloadMessage();
public slots:
    void setMessage(const QModelIndex &index);
    void setEmpty();
    void stopAutoMarkAsRead();
    void zoomIn();
    void zoomOut();
    void zoomOriginal();
protected:
    void showEvent(QShowEvent *se) override;
    /** @short Forwarded method to the bodyWidget() */
    void searchDialogRequested();
private slots:
    void markAsRead();
    void externalsRequested(const QUrl &url);
    void enableExternalData();
    void newLabelAction(const QString &tag);
    void deleteLabelAction(const QString &tag);
    void partContextMenuRequested(const QPoint &point);
    void partLinkHovered(const QString &link, const QString &title, const QString &textContent);
    void triggerSearchDialogBy(Gui::EmbeddedWebView *w);
    void onWebViewLoadStarted();
    void onWebViewLoadFinished(bool ok);
    void onNetworkPolicyChanged();
signals:
    void messageChanged();
    void messageModelChanged(QAbstractItemModel *model);
    /** The mouse points at a link to @p url whose text is @p text, or at no link if @p url is empty */
    void linkHovered(const QString &url, const QString &text);
    /** An attachment, image or message from the reader was saved to @p fileName */
    void fileSaved(const QString &fileName);
    void searchRequestedBy(Gui::EmbeddedWebView *webView);
    /** @short The first loaded part of the current message which contains the highlight term */
    void highlightTermFound(Gui::EmbeddedWebView *webView, const QString &term);
    void transferError(const QString &errorString);
private:
    bool eventFilter(QObject *object, QEvent *event) override;
    Imap::Message::Envelope envelope() const;
    void showMessageNow();
    AbstractPartWidget *bodyWidget() const;
    void unsetPreviousMessage();
    void clearWaitingConns();
    void findHighlightTerm(EmbeddedWebView *view, int retries);
    void showMessageUnavailable();
    void updateFailedParts();
    /** @short Show the junk banner while the current message is marked as junk */
    void updateJunkBanner();
    /** @short Override the junk mark of the current message */
    void markAsNotJunk();

    bool isOffline() const;

    QStackedLayout *m_stack;
    QWidget *m_homePage;
    /** Shown instead of the message when its structure cannot be loaded */
    QWidget *m_unavailablePage;
    QLabel *m_unavailableIcon;
    QLabel *m_unavailableTitle;
    QLabel *m_unavailableText;
    QPushButton *m_retryMessage;
    /** Reports parts of the shown message which could not be loaded */
    InlineNotificationBar *m_partErrors;
    /** Warns while the shown message is marked as junk, and offers to override the mark */
    InlineNotificationBar *m_junkBanner;
    /** The user closed the junk banner of the shown message */
    bool m_junkBannerClosed = false;
    QList<QPointer<SimplePartWidget>> m_failedParts;

    QWidget *m_messageWidget;
    QBoxLayout *m_msgLayout;
    EnvelopeView *m_envelope;
    ExternalElementsWidget *externalElements;
    TagListWidget *tags;
    QPersistentModelIndex message;
    Cryptography::MessageModel *messageModel;
    WebEngineMessageProfile *m_webEngineProfile;
    Imap::Network::MsgPartNetAccessManager *netAccess;
    QPointer<Imap::Mailbox::NetworkWatcher> m_netWatcher;
    QTimer *markAsReadTimer;
    QWidget *m_bodyWidget;
    QAction *m_zoomIn, *m_zoomOut, *m_zoomOriginal;
    QAction *m_findAction;

    std::unique_ptr<PartWidgetFactory> factory;
    Spinner *m_loadingSpinner;
    QSettings *m_settings;
    Plugins::PluginManager *m_pluginManager;
    TagHandler m_taghandler;
    QSet<QWebEngineView*> m_loadingItems;
    QString m_highlightTerm;
    /** The part of the current message which has been reported through highlightTermFound() */
    QPointer<EmbeddedWebView> m_highlightPart;

    std::vector<QMetaObject::Connection> m_waitingMessageConns;

    MessageView(const MessageView &); // don't implement
    MessageView &operator=(const MessageView &); // don't implement

    friend class SimplePartWidget; // needs access to onWebViewLoadStarted/onWebViewLoadFinished
};

}

#endif // VIEW_MESSAGEVIEW_H
