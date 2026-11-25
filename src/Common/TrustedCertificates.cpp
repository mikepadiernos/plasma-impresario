/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "TrustedCertificates.h"

#include <QCryptographicHash>
#include <QSettings>
#include <QSslCertificate>
#include <QSslKey>
#include <QStringList>

#include "Common/SettingsNames.h"

namespace Common {

namespace {

QString entry(const QString &host, const QSslCertificate &certificate)
{
    const QString key = TrustedCertificates::fingerprint(certificate);
    const QString name = host.trimmed().toLower();
    if (key.isEmpty() || name.isEmpty())
        return {};
    return name + QLatin1Char(' ') + key;
}

}

bool TrustedCertificates::isTrusted(const QSettings &settings, const QString &host, const QSslCertificate &certificate)
{
    const QString wanted = entry(host, certificate);
    return !wanted.isEmpty() && settings.value(SettingsNames::sslTrustedPublicKeys).toStringList().contains(wanted);
}

void TrustedCertificates::trust(QSettings &settings, const QString &host, const QSslCertificate &certificate)
{
    const QString accepted = entry(host, certificate);
    if (accepted.isEmpty())
        return;
    QStringList entries = settings.value(SettingsNames::sslTrustedPublicKeys).toStringList();
    if (entries.contains(accepted))
        return;
    entries << accepted;
    settings.setValue(SettingsNames::sslTrustedPublicKeys, entries);
}

QString TrustedCertificates::fingerprint(const QSslCertificate &certificate)
{
    if (certificate.isNull())
        return {};
    const QByteArray key = certificate.publicKey().toDer();
    if (key.isEmpty())
        return {};
    return QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex());
}

}
