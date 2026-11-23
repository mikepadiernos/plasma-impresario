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
     * @param callback Receives the text available for quoting.
     */
    void quoteText(const std::function<void(const QString &)> &callback);

signals:
    void externalLinkRequested(const QUrl &url);

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;

private:
    void requestExternalLink(const QUrl &url);
    QUrl m_partUrl;
    bool m_allowGeneratedMarkup = false;
    bool m_generatedMarkup = false;
};

}

#endif
