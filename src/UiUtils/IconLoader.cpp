/* Copyright (C) 2006 - 2015 Jan Kundrát <jkt@kde.org>
   Copyright (C) 2016 Luís Pereira <luis.artur.pereira@gmail.com>

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

#include "IconLoader.h"
#include <QCoreApplication>
#include <QFile>
#include <QHash>

namespace UiUtils {

static void cleanup_icon_cache();
namespace {
    struct Cache : public QHash<QString, QIcon>
    {
        Cache()
        {
            qAddPostRoutine(cleanup_icon_cache);
        }
    };
}

Q_GLOBAL_STATIC(Cache, iconDict)

static void cleanup_icon_cache()
{
    iconDict()->clear();
}


/** @short Wrapper around the QIcon::fromTheme with sane fallback
 *
 * The active icon theme, which Plasma sets, always wins. The bundled copy under
 * :/icons is used only for names the theme does not have.
 * */
QIcon loadIcon(const QString &name)
{
    auto it = iconDict()->constFind(name);
    if (it != iconDict()->constEnd())
        return *it;

    // A QIcon(QString) constructor creates non-null QIcons, even though the resulting icon will
    // not ever return a valid and usable pixmap. This means that we have to actually look at the
    // icon's pixmap to find out what to return.
    // If we do not do that, the GUI shows empty pixmaps instead of a text fallback, which is
    // clearly suboptimal.
    const QString bundled = QStringLiteral(":/icons/%1.svg").arg(name);
    QIcon res = QIcon::fromTheme(name, QFile::exists(bundled) ? QIcon(bundled) : QIcon());
    if (res.pixmap(QSize(16, 16)).isNull()) {
        res = QIcon();
    }
    iconDict()->insert(name, res);
    return res;
}

QIcon SymbolicIcon::fromTheme(const QString &name, const QIcon &fallback)
{
    const QString symbolic = name + QLatin1String("-symbolic");
    if (QIcon::hasThemeIcon(symbolic))
        return QIcon::fromTheme(symbolic);
    const QIcon icon = loadIcon(name);
    return icon.isNull() ? fallback : icon;
}

}
