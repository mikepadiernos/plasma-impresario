/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef TEST_OAUTH_H
#define TEST_OAUTH_H

#include <QObject>

/** @short OAuth 2.0 sign-in, token renewal and SASL authentication without real credentials or network

The provider's token endpoint, the browser which returns to the loopback redirect, the credential storage and the
SMTP server are all fakes on the loopback interface.
*/
class OAuthTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void saslInitialResponses();
    void saslChallengeAndMechanismChoice();
    void pkce();
    void providerTable();
    void authorizationRequest();
    void tokenResponses_data();
    void tokenResponses();
    void accountSettings();
    void revocationRequest();
    void grantRoundTrip();

    void authorizationSucceeds();
    void consentDenied();
    void authorizationIsCancelled();
    void authorizationTimesOut();
    void browserUnavailable();
    void refreshRenewsAndRotatesTokens();
    void refreshFailures_data();
    void refreshFailures();
    void credentialStorageUnavailable();
    void accountsAreIsolated();
    void signOutRemovesTheCredentials();
    void signOutRevocation_data();
    void signOutRevocation();
    void clientSecretIsStoredWithTheGrant();
    void clientSecretMigratesFromTheSettings();

    void smtpRetriesOnceWithAFreshToken();
    void smtpGivesUpAfterTheRetry();
    void smtpWithoutOAuthMechanism();
    void smtpTokenFailure();
};

#endif
