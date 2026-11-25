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
#include "ExternalElementsWidget.h"

#include <KLocalizedString>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include "UiUtils/LinkSafety.h"

namespace Gui
{

ExternalElementsWidget::ExternalElementsWidget(QWidget *parent):
    QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    m_message = new QLabel(this);
    m_message->setObjectName(QStringLiteral("remoteContentMessage"));
    m_message->setWordWrap(true);
    m_message->setTextFormat(Qt::PlainText);
    loadStuffButton = new QPushButton(i18nc("@action:button", "Load Remote Content"), this);
    loadStuffButton->setObjectName(QStringLiteral("loadRemoteContent"));
    loadStuffButton->setToolTip(i18nc("@info:tooltip", "Load the remote content of this message only"));
    layout->addWidget(m_message, 1);
    layout->addWidget(loadStuffButton);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(loadStuffButton, &QAbstractButton::clicked, this, &ExternalElementsWidget::loadingEnabled);
    updateMessage();

    // Consent is all or nothing for the open message: once given, every remote request of the message is
    // allowed until another message is shown. Scripts stay disabled, so the message cannot start new
    // requests on its own.
}

void ExternalElementsWidget::addBlockedUrl(const QUrl &url)
{
    bool changed = false;
    const QStringList hosts = UiUtils::remoteHosts({url});
    for (const QString &host : hosts) {
        if (!m_hosts.contains(host)) {
            m_hosts << host;
            changed = true;
        }
    }
    if (changed)
        updateMessage();
}

void ExternalElementsWidget::clear()
{
    m_hosts.clear();
    updateMessage();
}

QStringList ExternalElementsWidget::blockedHosts() const
{
    return m_hosts;
}

QString ExternalElementsWidget::message() const
{
    return m_message->text();
}

void ExternalElementsWidget::updateMessage()
{
    constexpr int shown = 3;
    QString from;
    if (m_hosts.size() > shown) {
        from = i18ncp("@info %2 is a list of servers", "%2 and %1 more server", "%2 and %1 more servers",
                      int(m_hosts.size() - shown), QStringList(m_hosts.mid(0, shown)).join(QLatin1String(", ")));
    } else {
        from = m_hosts.join(QLatin1String(", "));
    }
    const QString why = i18nc("@info", "Loading it can tell the sender when you read this message.");
    if (from.isEmpty()) {
        m_message->setText(i18nc("@info", "Remote content is blocked to protect your privacy.") + QLatin1Char(' ') + why);
    } else {
        m_message->setText(i18nc("@info %1 is a list of servers", "Remote content from %1 is blocked to protect your privacy.", from)
                           + QLatin1Char(' ') + why);
    }
    m_message->setToolTip(m_hosts.join(QLatin1Char('\n')));
    setAccessibleName(m_message->text());
}

}
