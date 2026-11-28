/* Copyright (C) 2026 Plasma Mail contributors

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License or (at your option)
   version 3 or any later version accepted by the membership of KDE e.V.
   (or its successor approved by the membership of KDE e.V.).
*/
#ifndef GUI_WEBENGINEMESSAGEPAGE_H
#define GUI_WEBENGINEMESSAGEPAGE_H

#include <functional>
#include <memory>
#include <QColor>
#include <QWebEnginePage>

namespace Gui
{

class WebEngineMessageProfile;

/** A message part page that delegates safe links instead of navigating to them.
    Its profile must outlive the page. */
class WebEngineMessagePage : public QWebEnginePage
{
    Q_OBJECT
public:
    explicit WebEngineMessagePage(WebEngineMessageProfile *profile, QObject *parent = nullptr);

    bool loadMessagePart(const QUrl &url);

    /** Loads application-generated markup using the message part as its base URL.
     * @param html Markup to display.
     * @param baseUrl URL used to resolve message-part resources.
     */
    /** @returns Whether the markup was accepted for the supplied message part. */
    bool setMessageHtml(const QString &html, const QUrl &baseUrl);

    /** Asynchronously returns selected text, or the rendered page text when nothing is selected.
     * @param callback Receives the text available for quoting. It is not called if the page is destroyed first.
     */
    void quoteText(const std::function<void(const QString &)> &callback);

    /** Which link to prefer when several have the same target. */
    enum class LinkSource {
        /** The link under the mouse pointer */
        Hovered,
        /** The link clicked or opened in a context menu last */
        Clicked,
    };

    /** Asynchronously returns the text of the link to @p href.
     * @param href The link target as the page reports it.
     * @param source Which of several links with this target is meant.
     * @param callback Receives the text, or an empty string if no such link is found. It is not called if the page
     * is destroyed first.
     */
    void linkText(const QString &href, LinkSource source, const std::function<void(const QString &)> &callback);

    /** Colors for the reader's stylesheet. */
    struct Colors {
        QColor background;
        QColor text;
        QColor link;
        QColor visitedLink;
        /** Overrides the message's own colors, such as a forced black-on-white scheme. */
        bool forced = false;
        /** Appended after the color rules. */
        QString extraCss;
    };

    /** Applies the reader's colors to the loaded page and to pages loaded later
     * once they finish.
     *
     * Plain text, generated or raw, uses @p colors. HTML that sets a background of
     * its own keeps its colors. Other HTML uses @p colors on a light palette, but a
     * light page in a dark one, because such mail is written for white paper. The
     * rules are injected from an isolated script world; message scripts stay
     * disabled.
     */
    void setColors(const Colors &colors);

signals:
    /** A link to @p url was clicked or opened in a new window.
     * @param text The link's text when it can be found in the page, otherwise empty. Always
     * empty for mailto: links.
     */
    void externalLinkRequested(const QUrl &url, const QString &text);
    /** Emitted after the colors were applied to a page.
     * @param kind "plain", "own" (the message's own background), "page" (HTML on
     * the reader's page), "forced", or empty while no document exists.
     */
    void colorsApplied(const QString &kind);

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;

private:
    void requestExternalLink(const QUrl &url);
    void applyColors();
    Colors m_colors;
    bool m_hasColors = false;
    QUrl m_partUrl;
    bool m_allowGeneratedMarkup = false;
    bool m_generatedMarkup = false;
    /** Expires before ~QWebEnginePage() runs the pending script callbacks, when a QPointer still looks valid. */
    std::shared_ptr<char> m_alive = std::make_shared<char>();
};

}

#endif
