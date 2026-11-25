/* Copyright (C) 2026 Plasma Mail contributors

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License or (at your option)
   version 3 or any later version accepted by the membership of KDE e.V.
   (or its successor approved by the membership of KDE e.V.).
*/
#include "Gui/WebEngineMessagePage.h"
#include "Gui/WebEngineMessageProfile.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QWebEngineNewWindowRequest>
#include <QWebEngineScript>

namespace Gui
{

namespace
{

QString jsString(const QString &text)
{
    const QByteArray encoded = QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(encoded.mid(1, encoded.size() - 2));
}

QString colorRules(const QColor &background, const QColor &text, const QColor &link, const QColor &visitedLink)
{
    // :where() keeps the rules at zero specificity, so any color the message sets
    // for an element still wins.
    const bool dark = background.lightness() < text.lightness();
    return QStringLiteral(":where(html){background-color:%1;color:%2;color-scheme:%3}"
                          ":where(a:link){color:%4}:where(a:visited){color:%5}")
        .arg(background.name(), text.name(), dark ? QStringLiteral("dark") : QStringLiteral("light"), link.name(),
             visitedLink.name());
}

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
    connect(this, &QWebEnginePage::loadFinished, this, [this](bool ok) {
        if (ok)
            applyColors();
    });
}

void WebEngineMessagePage::setColors(const Colors &colors)
{
    m_colors = colors;
    m_hasColors = true;
    // Shown until the page paints its own; avoids a light flash in dark schemes.
    setBackgroundColor(colors.forced ? QColor(Qt::white) : colors.background);
    applyColors();
}

void WebEngineMessagePage::applyColors()
{
    if (!m_hasColors)
        return;

    const bool dark = m_colors.background.lightness() < m_colors.text.lightness();
    const QString paletteRules = colorRules(m_colors.background, m_colors.text, m_colors.link, m_colors.visitedLink);
    QString forcedRules;
    if (m_colors.forced) {
        forcedRules = QStringLiteral("html,body{background-color:white !important;color:black !important;"
                                     "color-scheme:light}");
    }
    // HTML without a background of its own is written for white paper: in a dark
    // palette, its text colors would be unreadable on the dark page.
    const QString pageRules = dark ? QStringLiteral(":where(html){background-color:#ffffff;color:#000000;color-scheme:light}")
                                   : paletteRules;

    // Runs in the application's isolated world: the page's own scripts are
    // disabled, and this script only reads computed styles and adds one style
    // element; the message source is not changed.
    const QString script = QStringLiteral(R"JS((() => {
        const d = document;
        if (!d.documentElement)
            return "";
        let s = d.getElementById("trojita-user-style");
        if (!s) {
            s = d.createElement("style");
            s.id = "trojita-user-style";
        }
        s.textContent = "";
        (d.head || d.documentElement).appendChild(s);
        const clear = c => c === "transparent" || /^rgba\(.*,\s*0\)$/.test(c);
        const ownBackground = e => {
            if (!e)
                return false;
            const style = getComputedStyle(e);
            return !clear(style.backgroundColor) || style.backgroundImage !== "none";
        };
        const plain = %1 || d.contentType === "text/plain";
        const own = !plain && (ownBackground(d.documentElement) || ownBackground(d.body));
        const kind = %2 ? "forced" : plain ? "plain" : own ? "own" : "page";
        const rules = {forced: %3, plain: %4, own: "", page: %5};
        s.textContent = rules[kind] + %6;
        return kind;
    })())JS")
                               .arg(m_generatedMarkup ? QStringLiteral("true") : QStringLiteral("false"),
                                    m_colors.forced ? QStringLiteral("true") : QStringLiteral("false"),
                                    jsString(forcedRules), jsString(paletteRules), jsString(pageRules),
                                    jsString(m_colors.extraCss));
    const QPointer<WebEngineMessagePage> page(this);
    runJavaScript(script, QWebEngineScript::ApplicationWorld, [page](const QVariant &kind) {
        if (page)
            emit page->colorsApplied(kind.toString());
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
