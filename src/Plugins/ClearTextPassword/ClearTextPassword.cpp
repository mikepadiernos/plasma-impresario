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

#include <QRegularExpression>
#include <QString>
#include <QSettings>

#include "ClearTextPassword.h"

namespace {

const QString imapPassKey = QStringLiteral("imap.auth.pass");
const QString smtpPassKey = QStringLiteral("msa.smtp.auth.pass");

/**
 * Return the settings key that holds a password, or an empty string for an unsupported account type.
 * Accounts created by the account registry ("account-1" and up) get their own key; the default and
 * named profiles keep the original keys.
 */
QString passwordKey(const QString &accountId, const QString &accountType)
{
    if (accountType != QLatin1String("imap") && accountType != QLatin1String("smtp"))
        return QString();
    static const QRegularExpression registryAccount(QStringLiteral("^account-[1-9][0-9]*$"));
    if (registryAccount.match(accountId).hasMatch())
        return QStringLiteral("accountPasswords/%1/%2").arg(accountId, accountType);
    return accountType == QLatin1String("imap") ? imapPassKey : smtpPassKey;
}

}

ClearTextPasswordJob::ClearTextPasswordJob(const QString &accountId, const QString &accountType, const QString &password,
    enum Type type, QObject *parent, QSettings *settings) :
    PasswordJob(parent), m_accountId(accountId), m_accountType(accountType), m_password(password), m_type(type), m_settings(settings)
{
}

void ClearTextPasswordJob::doStart()
{
    const QString key = passwordKey(m_accountId, m_accountType);
    if (key.isEmpty()) {
        emit error(PasswordJob::UnknownError, tr("This plugin only supports storing of IMAP and SMTP passwords"));
        return;
    }
    switch (m_type) {
    case Request: {
        const QVariant password = m_settings->value(key);
        if (password.typeId() != QMetaType::QString || password.toString().isEmpty()) {
            emit error(PasswordJob::NoSuchPassword, QString());
        } else {
            emit passwordAvailable(password.toString());
        }
        break;
    }
    case Store:
        m_settings->setValue(key, m_password);
        emit passwordStored();
        break;
    case Delete:
        m_settings->remove(key);
        emit passwordDeleted();
        break;
    }
}

void ClearTextPasswordJob::doStop()
{
    emit error(PasswordJob::Stopped, QString());
}

ClearTextPassword::ClearTextPassword(QObject *parent, QSettings *settings) : PasswordPlugin(parent), m_settings(settings)
{
}

PasswordPlugin::Features ClearTextPassword::features() const
{
    return PasswordPlugin::Feature();
}

PasswordJob *ClearTextPassword::requestPassword(const QString &accountId, const QString &accountType)
{
    return new ClearTextPasswordJob(accountId, accountType, QString(), ClearTextPasswordJob::Request, this, m_settings);
}

PasswordJob *ClearTextPassword::storePassword(const QString &accountId, const QString &accountType, const QString &password)
{
    return new ClearTextPasswordJob(accountId, accountType, password, ClearTextPasswordJob::Store, this, m_settings);
}

PasswordJob *ClearTextPassword::deletePassword(const QString &accountId, const QString &accountType)
{
    return new ClearTextPasswordJob(accountId, accountType, QString(), ClearTextPasswordJob::Delete, this, m_settings);
}

QString trojita_plugin_ClearTextPasswordPlugin::name() const
{
    return QStringLiteral("cleartextpassword");
}

QString trojita_plugin_ClearTextPasswordPlugin::description() const
{
    return tr("Plasma Impresario's settings");
}

Plugins::PasswordPlugin *trojita_plugin_ClearTextPasswordPlugin::create(QObject *parent, QSettings *settings)
{
    Q_ASSERT(settings);
    return new ClearTextPassword(parent, settings);
}

// vim: set et ts=4 sts=4 sw=4
