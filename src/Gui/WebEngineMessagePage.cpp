/* Copyright (C) 2026 Plasma Mail contributors

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License or (at your option)
   version 3 or any later version accepted by the membership of KDE e.V.
   (or its successor approved by the membership of KDE e.V.).
*/
#include "Gui/WebEngineMessagePage.h"
#include "Gui/WebEngineMessageProfile.h"

#include <QPointer>
#include <QWebEngineNewWindowRequest>
#include <QWebEngineScript>

namespace Gui
{

namespace
{

bool isMessagePartUrl(const QUrl &url)
{
    return url.isValid() && url.scheme() == QLatin1String("trojita-imap") &&
        url.host() == QLatin1String("msg") && url.path().startsWith(QLatin1Char('/')) &&
        !url.hasQuery() && !url.hasFragment() && url.userInfo().isEmpty() && url.port() == -1;
}

}

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
    if (!isMessagePartUrl(url)) {
        return false;
    }
    m_partUrl = url;
    m_allowGeneratedMarkup = false;
    m_generatedMarkup = false;
    load(url);
    return true;
}

bool WebEngineMessagePage::setMessageHtml(const QString &html, const QUrl &baseUrl)
{
    if (!isMessagePartUrl(baseUrl))
        return false;

    m_partUrl = baseUrl;
    m_allowGeneratedMarkup = true;
    m_generatedMarkup = true;
    setHtml(html, baseUrl);
    return true;
}

void WebEngineMessagePage::quoteText(const std::function<void(const QString &)> &callback)
{
    const QString selected = selectedText();
    if (!selected.isEmpty()) {
        callback(selected);
        return;
    }

    if (!m_generatedMarkup) {
        toPlainText(callback);
        return;
    }

    const QPointer<WebEngineMessagePage> page(this);
    runJavaScript(QStringLiteral(R"JS((() => {
        const root = document.querySelector(".plain-text");
        if (!root)
            return null;
        const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
        let text = "";
        let previousQuote = null;
        while (walker.nextNode()) {
            let parent = walker.currentNode.parentElement;
            let hidden = false;
            while (parent && parent !== root) {
                if (getComputedStyle(parent).display === "none") {
                    hidden = true;
                    break;
                }
                parent = parent.parentElement;
            }
            if (hidden)
                continue;
            let quote = walker.currentNode.parentElement;
            while (quote && quote !== root && quote.tagName !== "BLOCKQUOTE")
                quote = quote.parentElement;
            if (quote === root)
                quote = null;
            const nodeText = walker.currentNode.nodeValue;
            if (text && quote !== previousQuote && !text.endsWith("\n") && !nodeText.startsWith("\n"))
                text += "\n";
            text += nodeText;
            previousQuote = quote;
        }
        return text;
    })())JS"), QWebEngineScript::ApplicationWorld, [page, callback](const QVariant &text) {
        if (text.isNull() && page) {
            page->toPlainText(callback);
        } else {
            callback(text.toString());
        }
    });
}

bool WebEngineMessagePage::acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame)
{
    if (!isMainFrame)
        return false;

    if (m_allowGeneratedMarkup && type == NavigationTypeTyped && url.scheme() == QLatin1String("data")) {
        m_allowGeneratedMarkup = false;
        return true;
    }

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
