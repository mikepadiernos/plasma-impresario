/* Copyright (C) 2026 Plasma Mail contributors

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License or (at your option)
   version 3 or any later version accepted by the membership of KDE e.V.
   (or its successor approved by the membership of KDE e.V.).
*/
#ifndef GUI_WEBENGINEMESSAGEPROFILE_H
#define GUI_WEBENGINEMESSAGEPROFILE_H

#include <QModelIndex>
#include <QWebEngineProfile>

namespace Gui
{

class MessageRequestInterceptor;

/** A private WebEngine profile bound to one IMAP message for its entire lifetime. */
class WebEngineMessageProfile : public QWebEngineProfile
{
    Q_OBJECT
public:
    /** Register message and mailto schemes before constructing QApplication. */
    static void registerScheme();
    explicit WebEngineMessageProfile(const QModelIndex &message, QObject *parent = nullptr);

    /** Controls new image requests. Destroy the profile to cancel pending requests and discard loaded content. */
    void setRemoteImagesAllowed(bool allowed);

signals:
    void remoteImageBlocked(const QUrl &url);

private:
    MessageRequestInterceptor *m_interceptor;
};

}

#endif
