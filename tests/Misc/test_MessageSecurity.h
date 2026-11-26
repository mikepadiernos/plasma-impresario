/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

class MessageSecurityTest : public QObject
{
    Q_OBJECT
private slots:
    void unprotectedMessage();
    void mimeStructureIsNeverVerified();
    void mimeStructureIsNeverVerified_data();
    void readerSignatureResults();
    void readerSignatureResults_data();
    void readerDecryptionResults();
    void readerDecryptionResults_data();
    void outermostResultWins();
    void inlinePgpInLoadedText();
    void inlinePgpNeverDownloads();
    void attachedMessagesAreIgnored();
};
