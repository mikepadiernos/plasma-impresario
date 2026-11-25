/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QSettings>
#include <QSslCertificate>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>
#include "test_TrustedCertificates.h"
#include "Common/SettingsNames.h"
#include "Common/TrustedCertificates.h"

using Common::TrustedCertificates;

namespace {

/** @short A self-signed certificate for fake.example.test */
const QByteArray serverPem = QByteArrayLiteral(
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBjzCCATWgAwIBAgIUHXfIolb9MD+LIRAPE6YYNqam5sUwCgYIKoZIzj0EAwIw\n"
    "HDEaMBgGA1UEAwwRZmFrZS5leGFtcGxlLnRlc3QwIBcNMjYxMTI1MTUyNjU3WhgP\n"
    "MjEyNjExMDExNTI2NTdaMBwxGjAYBgNVBAMMEWZha2UuZXhhbXBsZS50ZXN0MFkw\n"
    "EwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE9I9ZgooTMc2guwHUUfvH0z+J1jt1jCow\n"
    "zLhYaisWdQZQJwqVerPFxc52doJ80amEJBR0DhpR3fS5WOxpdVbYbKNTMFEwHQYD\n"
    "VR0OBBYEFNafphxjnPL/GloSBDWLKmeuCVUKMB8GA1UdIwQYMBaAFNafphxjnPL/\n"
    "GloSBDWLKmeuCVUKMA8GA1UdEwEB/wQFMAMBAf8wCgYIKoZIzj0EAwIDSAAwRQIg\n"
    "WK80Z+14xvDKXdnhC+PLr3bj4vhz2PSk9a1Sr+noQi8CIQCVERIG3ekwZ/sjh0ok\n"
    "7aymUG+iqeSvUXNV3HW27qtx5A==\n"
    "-----END CERTIFICATE-----\n");

/** @short A certificate re-issued for another name with the same key as serverPem */
const QByteArray renewedPem = QByteArrayLiteral(
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBlTCCATugAwIBAgIUYUjqZZGdtNtZtWPVpwhX9c3xKk4wCgYIKoZIzj0EAwIw\n"
    "HzEdMBsGA1UEAwwUcmVuZXdlZC5leGFtcGxlLnRlc3QwIBcNMjYxMTI1MTY1OTM4\n"
    "WhgPMjEyNjExMDExNjU5MzhaMB8xHTAbBgNVBAMMFHJlbmV3ZWQuZXhhbXBsZS50\n"
    "ZXN0MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE9I9ZgooTMc2guwHUUfvH0z+J\n"
    "1jt1jCowzLhYaisWdQZQJwqVerPFxc52doJ80amEJBR0DhpR3fS5WOxpdVbYbKNT\n"
    "MFEwHQYDVR0OBBYEFNafphxjnPL/GloSBDWLKmeuCVUKMB8GA1UdIwQYMBaAFNaf\n"
    "phxjnPL/GloSBDWLKmeuCVUKMA8GA1UdEwEB/wQFMAMBAf8wCgYIKoZIzj0EAwID\n"
    "SAAwRQIgFfrjrEFuLIiOygh1MYQbxq92X/szwKSV4CeSjtFDpDcCIQCaTf9MJGa5\n"
    "aAKUr6y7Nc9RowT6DgvpEfV9VVrtZUdtDg==\n"
    "-----END CERTIFICATE-----\n");

/** @short A certificate with a different key */
const QByteArray otherPem = QByteArrayLiteral(
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBkDCCATegAwIBAgIUMBztGDgKDz9wE1yj9WlRFKguh60wCgYIKoZIzj0EAwIw\n"
    "HTEbMBkGA1UEAwwSb3RoZXIuZXhhbXBsZS50ZXN0MCAXDTI2MTEyNTE2NTkzOFoY\n"
    "DzIxMjYxMTAxMTY1OTM4WjAdMRswGQYDVQQDDBJvdGhlci5leGFtcGxlLnRlc3Qw\n"
    "WTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAATY9E/p0dD8H4k0tZH/cy0D1riFx6nJ\n"
    "NxNGMR4yIVUnEllET/7ffbkZ/LHrZSRj2QreCN0FnSpFpYfXC4kQ2ZdOo1MwUTAd\n"
    "BgNVHQ4EFgQU8WVOC01+mV9cUcsUCi/DKvF48/gwHwYDVR0jBBgwFoAU8WVOC01+\n"
    "mV9cUcsUCi/DKvF48/gwDwYDVR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAgNHADBE\n"
    "AiBT5Upmt3x3h+0157TGq4kuloiP1ErNcPQMpDOEdmIVawIgKcL6Ng0z0mdbXjoE\n"
    "zl1WWPkqar1HSpd1uH1YCczlDFQ=\n"
    "-----END CERTIFICATE-----\n");

QSslCertificate certificate(const QByteArray &pem)
{
    return QSslCertificate(pem, QSsl::Pem);
}

struct Profile {
    QTemporaryDir dir;
    std::unique_ptr<QSettings> settings;

    Profile()
        : settings(std::make_unique<QSettings>(dir.path() + QStringLiteral("/profile.conf"), QSettings::IniFormat))
    {
    }
};

}

void TrustedCertificatesTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(!certificate(serverPem).isNull());
    QVERIFY(!certificate(renewedPem).isNull());
    QVERIFY(!certificate(otherPem).isNull());
}

void TrustedCertificatesTest::trustsAnAcceptedKeyForItsHost()
{
    Profile profile;
    const QSslCertificate server = certificate(serverPem);
    QVERIFY(!TrustedCertificates::isTrusted(*profile.settings, QStringLiteral("mail.example.test"), server));
    TrustedCertificates::trust(*profile.settings, QStringLiteral("mail.example.test"), server);
    QVERIFY(TrustedCertificates::isTrusted(*profile.settings, QStringLiteral("mail.example.test"), server));

    // Another account of the same profile reads the decision from disk
    profile.settings->sync();
    QSettings reread(profile.settings->fileName(), QSettings::IniFormat);
    QVERIFY(TrustedCertificates::isTrusted(reread, QStringLiteral("mail.example.test"), server));
}

void TrustedCertificatesTest::matchesHostNamesCaseInsensitively()
{
    Profile profile;
    const QSslCertificate server = certificate(serverPem);
    TrustedCertificates::trust(*profile.settings, QStringLiteral(" Mail.Example.TEST "), server);
    QVERIFY(TrustedCertificates::isTrusted(*profile.settings, QStringLiteral("mail.example.test"), server));
}

void TrustedCertificatesTest::doesNotTrustAnotherHost()
{
    Profile profile;
    const QSslCertificate server = certificate(serverPem);
    TrustedCertificates::trust(*profile.settings, QStringLiteral("mail.example.test"), server);
    QVERIFY(!TrustedCertificates::isTrusted(*profile.settings, QStringLiteral("smtp.example.test"), server));
}

void TrustedCertificatesTest::doesNotTrustAnotherKey()
{
    Profile profile;
    TrustedCertificates::trust(*profile.settings, QStringLiteral("mail.example.test"), certificate(serverPem));
    QVERIFY(!TrustedCertificates::isTrusted(*profile.settings, QStringLiteral("mail.example.test"),
                                            certificate(otherPem)));
    QVERIFY(TrustedCertificates::fingerprint(certificate(serverPem))
            != TrustedCertificates::fingerprint(certificate(otherPem)));
}

void TrustedCertificatesTest::trustFollowsTheKeyAcrossRenewals()
{
    Profile profile;
    TrustedCertificates::trust(*profile.settings, QStringLiteral("mail.example.test"), certificate(serverPem));
    QVERIFY(certificate(serverPem) != certificate(renewedPem));
    QCOMPARE(TrustedCertificates::fingerprint(certificate(renewedPem)),
             TrustedCertificates::fingerprint(certificate(serverPem)));
    QVERIFY(TrustedCertificates::isTrusted(*profile.settings, QStringLiteral("mail.example.test"),
                                           certificate(renewedPem)));
}

void TrustedCertificatesTest::ignoresNullCertificatesAndEmptyHosts()
{
    Profile profile;
    QVERIFY(TrustedCertificates::fingerprint(QSslCertificate()).isEmpty());
    TrustedCertificates::trust(*profile.settings, QStringLiteral("mail.example.test"), QSslCertificate());
    TrustedCertificates::trust(*profile.settings, QStringLiteral("  "), certificate(serverPem));
    QVERIFY(!profile.settings->contains(Common::SettingsNames::sslTrustedPublicKeys));
    QVERIFY(!TrustedCertificates::isTrusted(*profile.settings, QStringLiteral("mail.example.test"), QSslCertificate()));
    QVERIFY(!TrustedCertificates::isTrusted(*profile.settings, QString(), certificate(serverPem)));
}

void TrustedCertificatesTest::doesNotDuplicateEntries()
{
    Profile profile;
    TrustedCertificates::trust(*profile.settings, QStringLiteral("mail.example.test"), certificate(serverPem));
    TrustedCertificates::trust(*profile.settings, QStringLiteral("MAIL.example.test"), certificate(renewedPem));
    TrustedCertificates::trust(*profile.settings, QStringLiteral("mail.example.test"), certificate(otherPem));
    const QStringList entries = profile.settings->value(Common::SettingsNames::sslTrustedPublicKeys).toStringList();
    QCOMPARE(entries.size(), 2);
    QVERIFY(entries.first().startsWith(QLatin1String("mail.example.test ")));
}

QTEST_GUILESS_MAIN(TrustedCertificatesTest)
