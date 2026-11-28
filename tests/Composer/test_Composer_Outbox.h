/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <QTemporaryDir>
#include <memory>

#include "Composer/Outbox.h"
#include "MSA/FakeMSA.h"

class ComposerOutboxTest : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void testPersistsAcrossRestart();
    void testSendsThenFiles();
    void testFilingFailureNeverSendsAgain();
    void testRestartWhileFilingOnlyFiles();
    void testInterruptedWaitsForTheUser();
    void testRefusedBecomesDraftWithReason();
    void testRefusedWithoutDraftKeepsTheMessage();
    void testTransientFailureRetriesWithBackoff();
    void testOfflineWaitsUntilWoken();
    void testOneAtATimeInOrder();
    void testMissingPasswordRetriesLater();
    void testSilentServerInterrupts();
    void testMoveToDrafts();
    void testDefaultBackoff();

private:
    Composer::Outbox::Message message(const QString &subject = QStringLiteral("Hello")) const;
    Composer::Outbox::Transport transport();
    std::unique_ptr<Composer::Outbox> makeOutbox();

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<MSA::FakeFactory> m_factory;
    bool m_online = true;
    QList<QByteArray> m_filed;
    QList<Composer::Outbox::FilingReply> m_filingReplies;
    /** Answer filing right away with this, or keep the reply in m_filingReplies when empty */
    QList<bool> m_filingAnswers;
};
