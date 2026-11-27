/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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
#ifndef GUI_ENVELOPEVIEW_H
#define GUI_ENVELOPEVIEW_H

#include <QModelIndex>
#include <QUrl>
#include <QWidget>
#include <functional>

#include "UiUtils/MailingLists.h"

namespace Imap {
namespace Message {
class MailAddress;
}
}

namespace Plugins {
    class PluginManager;
}

namespace Gui {

/** @short Widget displaying the message envelope */
class EnvelopeView : public QWidget
{
    Q_OBJECT
public:
    EnvelopeView(QWidget *parent, Plugins::PluginManager *pluginManager);

    void setMessage(const QModelIndex &index);
    /** @short Name the account of a message in an Account row; an empty name hides the row */
    void setAccountResolver(const std::function<QString(const QModelIndex &)> &resolver);
    /** @short Follow the URL of a confirmed mailing list action; QDesktopServices::openUrl() by default */
    void setUrlOpener(const std::function<void(const QUrl &)> &opener);

    /** @short The mailing list actions offered for the shown message */
    QList<UiUtils::MailingLists::ActionTarget> listActions() const { return m_listActions; }

public slots:
    /** @short Ask whether to follow the list action at @arg position of listActions(), and follow it if confirmed */
    void requestListAction(int position);

private:
    QString headerText(const QModelIndex &index);

    QString htmlizeAddresses(const QList<Imap::Message::MailAddress> &addresses);

    Plugins::PluginManager *m_pluginManager;
    std::function<QString(const QModelIndex &)> m_accountName;
    std::function<void(const QUrl &)> m_openUrl;
    UiUtils::MailingList m_list;
    QList<UiUtils::MailingLists::ActionTarget> m_listActions;

    EnvelopeView(const EnvelopeView &); // don't implement
    EnvelopeView &operator=(const EnvelopeView &); // don't implement
};

}

#endif // GUI_ENVELOPEVIEW_H
