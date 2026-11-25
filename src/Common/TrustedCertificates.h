/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COMMON_TRUSTEDCERTIFICATES_H
#define COMMON_TRUSTEDCERTIFICATES_H

#include <QString>

class QSettings;
class QSslCertificate;

namespace Common {

/**
 * @short Server certificates the user accepted, shared by every account of a profile
 *
 * Each decision is recorded for a host name and the SHA-256 fingerprint of the certificate's public key, so that
 * accounts on the same server, and the IMAP and SMTP services of one host, do not ask again about a key that was
 * already accepted. A different key for the same host is not trusted.
 */
class TrustedCertificates
{
public:
    /** Whether the user accepted the public key of @arg certificate for @arg host */
    static bool isTrusted(const QSettings &settings, const QString &host, const QSslCertificate &certificate);

    /** Remember that the user accepted the public key of @arg certificate for @arg host */
    static void trust(QSettings &settings, const QString &host, const QSslCertificate &certificate);

    /** The hex-encoded SHA-256 digest of the certificate's public key, or an empty string for a null certificate */
    static QString fingerprint(const QSslCertificate &certificate);
};

}

#endif
