/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Jmap/Settings.h"

#include <QHostAddress>
#include <QSettings>

#include "Common/SettingsNames.h"

namespace Jmap {

QUrl Settings::sessionUrlFor(const QString &entered)
{
    const QString text = entered.trimmed();
    if (text.isEmpty())
        return {};
    QUrl url = QUrl::fromUserInput(text.contains(QLatin1String("://")) ? text : QLatin1String("https://") + text);
    if (!url.isValid() || url.host().isEmpty())
        return {};
    if (url.path().isEmpty() || url.path() == QLatin1String("/"))
        url.setPath(QStringLiteral("/.well-known/jmap"));
    return url;
}

bool Settings::isAcceptable(const QUrl &url)
{
    if (url.scheme() == QLatin1String("https"))
        return true;
    if (url.scheme() != QLatin1String("http"))
        return false;
    const QString host = url.host();
    return host == QLatin1String("localhost") || QHostAddress(host).isLoopback();
}

QUrl Settings::sessionUrl() const
{
    return sessionUrlFor(url);
}

QString Settings::host() const
{
    return sessionUrl().host();
}

QByteArray Settings::authorization() const
{
    if (authentication == Authentication::Bearer)
        return QByteArrayLiteral("Bearer ") + password.toUtf8();
    return QByteArrayLiteral("Basic ") + (user + QLatin1Char(':') + password).toUtf8().toBase64();
}

QString Settings::authenticationName(Authentication authentication)
{
    return authentication == Authentication::Bearer ? QStringLiteral("bearer") : QStringLiteral("basic");
}

Authentication Settings::authenticationFromName(const QString &name)
{
    return name == QLatin1String("bearer") ? Authentication::Bearer : Authentication::Basic;
}

QString Settings::sendingName(Sending sending)
{
    return sending == Sending::Smtp ? QStringLiteral("smtp") : QStringLiteral("jmap");
}

Sending Settings::sendingFromName(const QString &name)
{
    return name == QLatin1String("smtp") ? Sending::Smtp : Sending::Jmap;
}

Settings Settings::fromSettings(const QSettings &account)
{
    using Common::SettingsNames;
    Settings settings;
    settings.url = account.value(SettingsNames::jmapUrlKey).toString();
    settings.user = account.value(SettingsNames::jmapUserKey).toString();
    settings.authentication = authenticationFromName(account.value(SettingsNames::jmapAuthKey).toString());
    settings.interval = qMax(0, account.value(SettingsNames::jmapIntervalKey, 10).toInt());
    settings.sending = sendingFromName(account.value(SettingsNames::jmapSendingKey).toString());
    settings.canSubmit = account.value(SettingsNames::jmapCanSubmitKey, true).toBool();
    return settings;
}

void Settings::save(QSettings &account) const
{
    using Common::SettingsNames;
    account.setValue(SettingsNames::jmapUrlKey, url);
    account.setValue(SettingsNames::jmapUserKey, user);
    account.setValue(SettingsNames::jmapAuthKey, authenticationName(authentication));
    account.setValue(SettingsNames::jmapIntervalKey, interval);
    account.setValue(SettingsNames::jmapSendingKey, sendingName(sending));
    account.setValue(SettingsNames::jmapCanSubmitKey, canSubmit);
}

}
