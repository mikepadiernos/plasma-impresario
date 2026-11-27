/* Copyright (C) 2026 Plasma Impresario contributors

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

#ifndef AKONADIADDRESSBOOKCONTACTJOB_H
#define AKONADIADDRESSBOOKCONTACTJOB_H

#include "AkonadiAddressbook.h"

#include <Akonadi/ContactSearchJob>

/** @short Look up the name, organization and stored photo of the first Akonadi contact with an address */
class AkonadiAddressbookContactJob : public AddressbookContactJob {
    Q_OBJECT

public:
    AkonadiAddressbookContactJob(const QString &email, AkonadiAddressbook *parent);

public slots:
    void doStart() override;
    void doStop() override;

private slots:
    void searchResult(KJob *job);

private:
    QString m_email;
    Akonadi::ContactSearchJob *m_job;
};

#endif
