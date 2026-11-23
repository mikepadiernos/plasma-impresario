/* Copyright (C) 2026 Plasma Mail contributors

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License or (at your option)
   version 3 or any later version accepted by the membership of KDE e.V.
   (or its successor approved by the membership of KDE e.V.).
*/
#ifndef GUI_WEBENGINEMESSAGEPAGE_H
#define GUI_WEBENGINEMESSAGEPAGE_H

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

signals:
    void externalLinkRequested(const QUrl &url);

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;

private:
    void requestExternalLink(const QUrl &url);
    QUrl m_partUrl;
};

}

#endif
