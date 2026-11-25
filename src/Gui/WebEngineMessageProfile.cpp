/* Copyright (C) 2026 Plasma Mail contributors

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License or (at your option)
   version 3 or any later version accepted by the membership of KDE e.V.
   (or its successor approved by the membership of KDE e.V.).
*/
#include "Gui/WebEngineMessageProfile.h"

#include "Imap/Network/MsgPartNetAccessManager.h"

#include <QBuffer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTimer>
#include <QWebEngineSettings>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>
#include <QWebEngineUrlSchemeHandler>

#include <atomic>
#include <memory>

namespace Gui
{

class MessageRequestInterceptor : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT
public:
    using QWebEngineUrlRequestInterceptor::QWebEngineUrlRequestInterceptor;

    void interceptRequest(QWebEngineUrlRequestInfo &info) override
    {
        const QUrl url = info.requestUrl();
        const QString scheme = url.scheme();
        if ((scheme == QLatin1String("trojita-imap") &&
             (url.host() == QLatin1String("msg") || url.host() == QLatin1String("cid"))) ||
            scheme == QLatin1String("cid") ||
            scheme == QLatin1String("data") || url == QUrl(QStringLiteral("about:blank"))) {
            return;
        }
        if (info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeImage &&
            (scheme == QLatin1String("http") || scheme == QLatin1String("https"))) {
            if (m_remoteImagesAllowed.load())
                return;
            emit remoteImageBlocked(url);
        }
        info.block(true);
    }

    std::atomic<bool> m_remoteImagesAllowed{false};

signals:
    void remoteImageBlocked(const QUrl &url);
};

class MessageSchemeHandler : public QWebEngineUrlSchemeHandler
{
public:
    MessageSchemeHandler(Imap::Network::MsgPartNetAccessManager *manager, QObject *parent)
        : QWebEngineUrlSchemeHandler(parent), m_manager(manager)
    {
    }

    void requestStarted(QWebEngineUrlRequestJob *job) override
    {
        const QUrl url = job->requestUrl();
        QUrl partUrl;
        if (url.scheme() == QLatin1String("cid") ||
            (url.scheme() == QLatin1String("trojita-imap") && url.host() == QLatin1String("msg"))) {
            partUrl = url;
        } else if (url.scheme() == QLatin1String("trojita-imap") &&
                   url.host() == QLatin1String("cid") && url.path().startsWith(QLatin1Char('/'))) {
            const QByteArray cidUrl = QByteArrayLiteral("cid:") + url.path(QUrl::FullyEncoded).mid(1).toLatin1();
            partUrl = QUrl::fromEncoded(cidUrl);
        }
        if (!m_manager || job->requestMethod() != QByteArrayLiteral("GET") || !partUrl.isValid() || partUrl.isEmpty()) {
            job->fail(QWebEngineUrlRequestJob::RequestDenied);
            return;
        }

        QNetworkReply *reply = m_manager->get(QNetworkRequest(partUrl));
        auto pending = std::make_shared<bool>(true);
        connect(job, &QObject::destroyed, reply, [reply, pending] {
            if (*pending) {
                *pending = false;
                reply->abort();
                reply->deleteLater();
            }
        });
        connect(reply, &QObject::destroyed, job, [job, pending] {
            if (*pending) {
                *pending = false;
                job->fail(QWebEngineUrlRequestJob::RequestAborted);
            }
        });
        auto finish = [job, reply, pending] {
            if (!*pending)
                return;
            *pending = false;
            if (reply->error() != QNetworkReply::NoError) {
                job->fail(QWebEngineUrlRequestJob::RequestFailed);
            } else {
                auto *buffer = new QBuffer(job);
                buffer->setData(reply->readAll());
                buffer->open(QIODevice::ReadOnly);
                job->reply(reply->header(QNetworkRequest::ContentTypeHeader).toByteArray(), buffer);
            }
            reply->deleteLater();
        };
        connect(reply, &QNetworkReply::finished, job, finish);
        if (reply->isFinished())
            QTimer::singleShot(0, job, finish);
    }

private:
    QPointer<Imap::Network::MsgPartNetAccessManager> m_manager;
};

void WebEngineMessageProfile::registerScheme()
{
    QWebEngineUrlScheme scheme(QByteArrayLiteral("trojita-imap"));
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme);
    QWebEngineUrlScheme::registerScheme(scheme);
    QWebEngineUrlScheme cidScheme(QByteArrayLiteral("cid"));
    cidScheme.setSyntax(QWebEngineUrlScheme::Syntax::Path);
    cidScheme.setFlags(QWebEngineUrlScheme::SecureScheme);
    QWebEngineUrlScheme::registerScheme(cidScheme);
    // Chromium otherwise handles mailto externally before the page can delegate the link.
    QWebEngineUrlScheme mailtoScheme(QByteArrayLiteral("mailto"));
    mailtoScheme.setSyntax(QWebEngineUrlScheme::Syntax::Path);
    QWebEngineUrlScheme::registerScheme(mailtoScheme);
}

WebEngineMessageProfile::WebEngineMessageProfile(const QModelIndex &message, QObject *parent)
    : QWebEngineProfile(parent), m_interceptor(new MessageRequestInterceptor(this))
{
    auto *manager = new Imap::Network::MsgPartNetAccessManager(this);
    manager->setModelMessage(message);
    auto *handler = new MessageSchemeHandler(manager, this);
    installUrlSchemeHandler(QByteArrayLiteral("trojita-imap"), handler);
    installUrlSchemeHandler(QByteArrayLiteral("cid"), handler);
    installUrlSchemeHandler(QByteArrayLiteral("mailto"), handler);
    setUrlRequestInterceptor(m_interceptor);
    connect(m_interceptor, &MessageRequestInterceptor::remoteImageBlocked, this, [this](const QUrl &url) {
        if (!m_interceptor->m_remoteImagesAllowed.load())
            emit remoteImageBlocked(url);
    });
    setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    setHttpCacheType(QWebEngineProfile::NoCache);
    settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, false);
    settings()->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, false);
    settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, false);
    settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    settings()->setAttribute(QWebEngineSettings::HyperlinkAuditingEnabled, false);
    settings()->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, false);
    // A part which cannot be loaded is reported by the message view, not by a Chromium error page
    settings()->setAttribute(QWebEngineSettings::ErrorPageEnabled, false);
    settings()->setUnknownUrlSchemePolicy(QWebEngineSettings::DisallowUnknownUrlSchemes);
}

void WebEngineMessageProfile::setRemoteImagesAllowed(bool allowed)
{
    m_interceptor->m_remoteImagesAllowed.store(allowed);
}

}

#include "WebEngineMessageProfile.moc"
