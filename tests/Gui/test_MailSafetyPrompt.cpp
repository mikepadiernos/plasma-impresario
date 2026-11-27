/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTimer>
#include <QtTest>

#include "Gui/MailSafetyPrompt.h"
#include "Imap/Model/ItemRoles.h"

using namespace UiUtils::MailSafety;
namespace Prompt = Gui::MailSafetyPrompt;

namespace {

/** @short What the next message box said; it is answered with @arg button, or Cancel/Close when empty */
struct Answer {
    QString objectName;
    QString text;
    QTimer timer;

    explicit Answer(const QString &button = QString())
    {
        QObject::connect(&timer, &QTimer::timeout, &timer, [this, button]() {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!box)
                return;
            timer.stop();
            objectName = box->objectName();
            text = box->text() + QLatin1Char('\n') + box->informativeText();
            if (auto *custom = box->findChild<QPushButton *>(button); !button.isEmpty() && custom)
                custom->click();
            else if (auto *cancel = box->button(QMessageBox::Cancel))
                cancel->click();
            else
                box->button(QMessageBox::Close)->click();
        });
        timer.start(10);
    }
};

}

class MailSafetyPromptTest : public QObject
{
    Q_OBJECT

private slots:
    void verdictOfPartComesFromItsMessage()
    {
        // Shaped like the reader's part model: index(0, 0) is the message and its children are parts
        QStandardItemModel model;
        auto *message = new QStandardItem(QStringLiteral("message"));
        auto *mixed = new QStandardItem(QStringLiteral("multipart/mixed"));
        auto *file = new QStandardItem(QStringLiteral("application/pdf"));
        mixed->appendRow(file);
        message->appendRow(mixed);
        model.appendRow(message);
        QCOMPARE(Prompt::junkVerdictOfPart(file->index()), JunkVerdict::Unclassified);

        message->setData(QStringList{QStringLiteral("\\Seen"), QStringLiteral("$Junk")}, Imap::Mailbox::RoleMessageFlags);
        QCOMPARE(Prompt::junkVerdictOfPart(file->index()), JunkVerdict::Junk);
        message->setData(QStringList{QStringLiteral("$Junk"), QStringLiteral("$NotJunk")}, Imap::Mailbox::RoleMessageFlags);
        QCOMPARE(Prompt::junkVerdictOfPart(file->index()), JunkVerdict::NotJunk);
        QCOMPARE(Prompt::junkVerdictOfPart(QModelIndex()), JunkVerdict::Unclassified);
    }

    void everyScanResultButNotScannedIsExplained()
    {
        const QString name = QStringLiteral("invoice.zip");
        QVERIFY(Prompt::scanResultText(ScanResult::NotScanned, name).isEmpty());
        for (ScanResult scan : {ScanResult::Clean, ScanResult::Infected, ScanResult::Failed, ScanResult::Unavailable})
            QVERIFY(Prompt::scanResultText(scan, name).contains(name));
        // Neither an unavailable nor a failed scan reads as a clean one
        QVERIFY(Prompt::scanResultText(ScanResult::Unavailable, name) != Prompt::scanResultText(ScanResult::Clean, name));
        QVERIFY(Prompt::scanResultText(ScanResult::Failed, name).contains(QLatin1String("not been found safe")));
        QVERIFY(!Prompt::junkBannerText().isEmpty());
    }

    void opensOrdinaryMailWithoutAsking()
    {
        Answer answer;
        QVERIFY(Prompt::confirmOpening(nullptr, JunkVerdict::Unclassified, ScanResult::NotScanned, QStringLiteral("a.pdf")));
        QVERIFY(Prompt::confirmOpening(nullptr, JunkVerdict::NotJunk, ScanResult::Clean, QStringLiteral("a.pdf")));
        QTest::qWait(50);
        QVERIFY(answer.objectName.isEmpty());
    }

    void junkAsksAndCancelKeepsItClosed()
    {
        Answer answer;
        QVERIFY(!Prompt::confirmOpening(nullptr, JunkVerdict::Junk, ScanResult::NotScanned, QStringLiteral("invoice.pdf")));
        QCOMPARE(answer.objectName, QStringLiteral("attachmentConfirmation"));
        QVERIFY2(answer.text.contains(QLatin1String("invoice.pdf")) && answer.text.contains(QLatin1String("junk")),
                 qPrintable(answer.text));
    }

    void junkOpensAfterOpenAnyway()
    {
        Answer answer(QStringLiteral("openAttachmentAnyway"));
        QVERIFY(Prompt::confirmOpening(nullptr, JunkVerdict::Junk, ScanResult::NotScanned, QStringLiteral("invoice.pdf")));
        QCOMPARE(answer.objectName, QStringLiteral("attachmentConfirmation"));
    }

    void unavailableScannerAsksAndSaysWhy()
    {
        Answer answer(QStringLiteral("openAttachmentAnyway"));
        QVERIFY(Prompt::confirmOpening(nullptr, JunkVerdict::Unclassified, ScanResult::Unavailable, QStringLiteral("a.doc")));
        QCOMPARE(answer.objectName, QStringLiteral("attachmentConfirmation"));
        QVERIFY2(answer.text.contains(QLatin1String("not available")), qPrintable(answer.text));
        QVERIFY(!answer.text.contains(QLatin1String("junk")));
    }

    void infectedIsBlocked()
    {
        Answer answer(QStringLiteral("openAttachmentAnyway"));
        QVERIFY(!Prompt::confirmOpening(nullptr, JunkVerdict::NotJunk, ScanResult::Infected, QStringLiteral("a.exe")));
        QCOMPARE(answer.objectName, QStringLiteral("attachmentBlocked"));
        QVERIFY2(answer.text.contains(QLatin1String("a.exe")), qPrintable(answer.text));
    }
};

QTEST_MAIN(MailSafetyPromptTest)
#include "test_MailSafetyPrompt.moc"
