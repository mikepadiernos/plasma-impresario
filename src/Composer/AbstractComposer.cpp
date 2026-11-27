/* Copyright (C) 2006 - 2017 Jan Kundrát <jkt@kde.org>

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

#include <QModelIndex>
#include <QUrl>
#include <QUuid>
#include "Composer/AbstractComposer.h"
#include "Composer/HeaderSafety.h"

namespace Composer {

AbstractComposer::~AbstractComposer() = default;

void AbstractComposer::setPreloadEnabled(const bool preload)
{
}

QModelIndex AbstractComposer::replyingToMessage() const
{
    return QModelIndex();
}

QModelIndex AbstractComposer::forwardingMessage() const
{
    return QModelIndex();
}

/** @short A random Message-ID in the sender's domain

The left part is a random UUID and the right part is the domain of the sender's address, so the identifier never
reveals the computer's host name. Without a usable domain, "localhost" is used instead.
*/
QByteArray AbstractComposer::generateMessageId(const Imap::Message::MailAddress &fromAddress)
{
    const QString host = HeaderSafety::stripped(fromAddress.host);
    QByteArray domain;
    if (!host.startsWith(QLatin1Char('['))) {
        domain = QUrl::toAce(host);
    }
    domain = HeaderSafety::messageId(domain);
    if (domain.isEmpty()) {
        domain = QByteArrayLiteral("localhost");
    }
    return QUuid::createUuid().toByteArray(QUuid::WithoutBraces) + "@" + domain;
}

}
