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

#include "AkonadiAddressbookContactJob.h"

AkonadiAddressbookContactJob::AkonadiAddressbookContactJob(const QString &email, AkonadiAddressbook *parent) :
    AddressbookContactJob(parent), m_email(email), m_job(nullptr)
{
}

void AkonadiAddressbookContactJob::doStart()
{
    Q_ASSERT(!m_job);
    m_job = new Akonadi::ContactSearchJob(this);
    m_job->setQuery(Akonadi::ContactSearchJob::Email, m_email, Akonadi::ContactSearchJob::ExactMatch);
    m_job->setLimit(1);
    connect(m_job, &KJob::result, this, &AkonadiAddressbookContactJob::searchResult);
}

void AkonadiAddressbookContactJob::doStop()
{
    if (m_job) {
        disconnect(m_job, &KJob::result, this, &AkonadiAddressbookContactJob::searchResult);
        // Kills and deletes the job immediately
        m_job->kill();
        m_job = nullptr;
    }
    emit error(AddressbookJob::Stopped);
    finished();
}

void AkonadiAddressbookContactJob::searchResult(KJob *job)
{
    m_job = nullptr;
    auto searchJob = qobject_cast<Akonadi::ContactSearchJob *>(job);
    Q_ASSERT(searchJob);
    if (job->error()) {
        qWarning() << "AkonadiAddressbookContactJob::searchResult:" << job->errorString();
        emit error(AddressbookJob::UnknownError);
        finished();
        return;
    }

    ContactDetails details;
    details.email = m_email;
    const auto contacts = searchJob->contacts();
    if (!contacts.isEmpty()) {
        const KContacts::Addressee &contact = contacts.first();
        details.known = true;
        details.name = contact.realName();
        if (details.name.isEmpty())
            details.name = contact.formattedName();
        details.organization = contact.organization();
        // A photo which only refers to a URL is not fetched
        const KContacts::Picture photo = contact.photo();
        if (!photo.isEmpty() && photo.isIntern())
            details.photo = photo.rawData();
    }
    emit contactDetailsAvailable(details);
    finished();
}
