/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_COMPOSER_HEADERSAFETY_H
#define TEST_COMPOSER_HEADERSAFETY_H

#include <QObject>

/** @short Verify that composed headers cannot be split, that Bcc stays private and that Message-IDs are anonymous */
class ComposerHeaderSafetyTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void text_data();
    void text();
    void strippedAddressParts();
    void messageIdKeepsVisibleAscii();
    void subjectCannotAddHeaders();
    void organizationCannotAddHeaders();
    void addressesCannotAddHeaders();
    void referencesCannotAddHeaders();
    void attachmentNameCannotAddHeaders();
    void smtpEnvelopeHasNoControls();
    void mailtoSubjectIsOneLine();
    void bccOmittedByDefault();
    void bccKeptOnRequest();
    void bccInCatenateData();
    void messageIdUsesSenderDomain();
    void messageIdWithoutDomain();
    void messageIdIsRandom();
    void messageIdOmitsHostName();
};

#endif
