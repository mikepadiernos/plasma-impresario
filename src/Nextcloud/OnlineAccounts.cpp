/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Nextcloud/OnlineAccounts.h"

#include <QUrl>

namespace Nextcloud {

OnlineAccounts::OnlineAccounts(QObject *parent)
    : QObject(parent)
{
}

OnlineAccounts::~OnlineAccounts() = default;

QString onlineAccountKey(quint32 onlineAccountId)
{
    return QLatin1String("online-") + QString::number(onlineAccountId);
}

Account onlineAccount(quint32 onlineAccountId, const QString &displayName, const QVariantMap &settings)
{
    QString text = settings.value(QStringLiteral("server")).toString().trimmed();
    if (text.isEmpty()) {
        const QString host = settings.value(QStringLiteral("dav/host")).toString().trimmed();
        if (!host.isEmpty())
            text = QLatin1String("https://") + host;
    }
    QString error;
    const QUrl server = text.isEmpty() ? QUrl() : normalizedServer(text, &error);
    if (!server.isValid() || !isAcceptable(server))
        return {};

    const QString user = displayName.trimmed();
    QString userId;
    const QString storagePath = settings.value(QStringLiteral("dav/storagePath")).toString();
    const QLatin1String files("/remote.php/dav/files/");
    const qsizetype at = storagePath.indexOf(files);
    if (at >= 0) {
        QString rest = storagePath.mid(at + files.size());
        while (rest.endsWith(QLatin1Char('/')))
            rest.chop(1);
        // Only the user's own folder; a path below it is not where the files start
        if (!rest.contains(QLatin1Char('/')))
            userId = QUrl::fromPercentEncoding(rest.toUtf8());
    }
    if (userId.isEmpty())
        userId = user;

    Account account;
    account.id = onlineAccountKey(onlineAccountId);
    account.server = server;
    account.loginName = user;
    account.userId = userId;
    account.displayName = user;
    account.source = Account::Source::OnlineAccounts;
    account.onlineAccountId = onlineAccountId;
    return account.isValid() ? account : Account();
}

} // namespace Nextcloud
