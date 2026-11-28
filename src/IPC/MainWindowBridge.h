/* Copyright (C) 2013 Pali Rohár <pali.rohar@gmail.com>

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

#ifndef IPC_MAINWINDOWBRIDGE_H
#define IPC_MAINWINDOWBRIDGE_H

#include <QObject>

namespace Gui {
    class MainWindow;
}

namespace IPC {

class MainWindowBridge : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.flaska.trojita")

public:
    MainWindowBridge(Gui::MainWindow *window);

public slots:
    /** The token which lets the window shown for the next request come to the front */
    void setActivationToken(const QString &token);
    void showMainWindow();
    void showAddressbookWindow();
    void composeMail(const QString &url);
    /** Compose from a mailto: @arg url, or a blank message, attaching @arg files given on the command line */
    void composeMailWithFiles(const QString &url, const QStringList &files);
    void openMessage(const QString &messageId);
    /** Show a Nextcloud share link handed over by another application, such as Plasma Opus, for review; only after
     * the user accepts does it go into a new draft */
    void openNextcloudShare(const QString &url);

private:
    Gui::MainWindow *m_window;
};

}

#endif
