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

#include <QObject>
#include <QtGlobal>
#include "Application.h"

#include "plasma-impresario-version.h"

namespace Common
{

/* default values, they can be changed by other backends, e.g. Kontact or AppVersion::setGitVersion() */
QString Application::name = QStringLiteral("trojita");
QString Application::version = QStringLiteral(IMPRESARIO_VERSION);
QString Application::organization = QStringLiteral("flaska.net");

namespace {
constexpr auto profileVariable = "PLASMA_IMPRESARIO_PROFILE";
constexpr auto legacyProfileVariable = "TROJITA_PROFILE";
}

QString Application::profileName()
{
    const QByteArray chosen = qgetenv(profileVariable);
    return QString::fromUtf8(chosen.isEmpty() ? qgetenv(legacyProfileVariable) : chosen);
}

void Application::setProfileName(const QString &name)
{
    // The environment is a hidden global, but it reaches the helper processes this one starts
    qunsetenv(legacyProfileVariable);
    if (name.isEmpty())
        qunsetenv(profileVariable);
    else
        qputenv(profileVariable, name.toUtf8());
}

}
