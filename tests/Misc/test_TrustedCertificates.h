/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_TRUSTEDCERTIFICATES_H
#define TEST_TRUSTEDCERTIFICATES_H

#include <QObject>

/** @short Verify that accepted server keys are shared across accounts by host and public key */
class TrustedCertificatesTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void trustsAnAcceptedKeyForItsHost();
    void matchesHostNamesCaseInsensitively();
    void doesNotTrustAnotherHost();
    void doesNotTrustAnotherKey();
    void trustFollowsTheKeyAcrossRenewals();
    void ignoresNullCertificatesAndEmptyHosts();
    void doesNotDuplicateEntries();
};

#endif
