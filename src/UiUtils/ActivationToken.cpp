/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QProcessEnvironment>

#include "UiUtils/ActivationToken.h"

namespace UiUtils {

QString ActivationToken::fromEnvironment(const QProcessEnvironment &environment)
{
    for (const char *name : {"XDG_ACTIVATION_TOKEN", "DESKTOP_STARTUP_ID"}) {
        const QString token = environment.value(QLatin1String(name));
        if (isAcceptable(token))
            return token;
    }
    return QString();
}

bool ActivationToken::isAcceptable(const QString &token)
{
    if (token.isEmpty() || token.size() > maximumLength)
        return false;
    for (const QChar c : token) {
        if (c.unicode() <= 0x20 || c.unicode() >= 0x7f)
            return false;
    }
    return true;
}

void ActivationToken::handOver(const QString &token, const QDateTime &now)
{
    if (!isAcceptable(token))
        return;
    m_token = token;
    m_handedOver = now;
}

QString ActivationToken::take(const QDateTime &now)
{
    QString token;
    std::swap(token, m_token);
    if (token.isEmpty() || !m_handedOver.isValid())
        return QString();
    const qint64 age = m_handedOver.msecsTo(now);
    if (age < 0 || age > lifetimeMs)
        return QString();
    return token;
}

bool ActivationToken::isPending() const
{
    return !m_token.isEmpty();
}

}
