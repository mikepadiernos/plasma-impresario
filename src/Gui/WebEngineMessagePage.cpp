/* Copyright (C) 2026 Plasma Mail contributors

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License or (at your option)
   version 3 or any later version accepted by the membership of KDE e.V.
   (or its successor approved by the membership of KDE e.V.).
*/
#include "Gui/WebEngineMessagePage.h"
#include "Gui/WebEngineMessageProfile.h"

#include <QWebEngineNewWindowRequest>

namespace Gui
{

WebEngineMessagePage::WebEngineMessagePage(WebEngineMessageProfile *profile, QObject *parent)
    : QWebEnginePage(profile, parent)
{
    connect(this, &QWebEnginePage::newWindowRequested, this, [this](QWebEngineNewWindowRequest &request) {
        if (request.isUserInitiated())
            requestExternalLink(request.requestedUrl());
    });
}

bool WebEngineMessagePage::loadMessagePart(const QUrl &url)
{
    if (!url.isValid() || url.scheme() != QLatin1String("trojita-imap") ||
        url.host() != QLatin1String("msg") || !url.path().startsWith(QLatin1Char('/')) ||
        url.hasQuery() || url.hasFragment() || !url.userInfo().isEmpty() || url.port() != -1) {
        return false;
    }
    m_partUrl = url;
    load(url);
    return true;
}

bool WebEngineMessagePage::acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame)
{
    if (!isMainFrame)
        return false;

    if (type == NavigationTypeLinkClicked) {
        if (url.hasFragment() && !m_partUrl.isEmpty() && url.matches(m_partUrl, QUrl::RemoveFragment))
            return true;
        requestExternalLink(url);
        return false;
    }

    if (type == NavigationTypeFormSubmitted || type == NavigationTypeRedirect)
        return false;
    return !m_partUrl.isEmpty() && url.matches(m_partUrl, QUrl::RemoveFragment);
}

void WebEngineMessagePage::requestExternalLink(const QUrl &url)
{
    if (!url.isValid())
        return;
    const QString scheme = url.scheme().toLower();
    if (((scheme == QLatin1String("http") || scheme == QLatin1String("https")) && !url.host().isEmpty()) ||
        (scheme == QLatin1String("mailto") && !url.path().isEmpty())) {
        emit externalLinkRequested(url);
    }
}

}
