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

#ifndef IMPRESARIO_IMAP_SPECIALFLAGNAMES_H
#define IMPRESARIO_IMAP_SPECIALFLAGNAMES_H

#include <QString>
#include <QHash>

namespace Imap
{
namespace Mailbox
{

/** @short Some of the well-known IMAP flag names */
struct FlagNames {
    /** The flag names themselves, with canonical capitalization */
    static const QString answered;
    static const QString seen;
    static const QString deleted;
    static const QString forwarded;
    static const QString recent;
    static const QString flagged;
    static const QString junk;
    static const QString notjunk;
    static const QString mdnsent;
    static const QString submitted;
    static const QString submitpending;
    /** Set by the server's attachment detection, such as Dovecot's, not by the user */
    static const QString hasAttachment;
    static const QString hasNoAttachment;

    /** A mapping from lower-case flag names to canonical ones */
    static const QHash<QString,QString> toCanonical;
};

}
}

#endif // IMPRESARIO_IMAP_SPECIALFLAGNAMES_H
