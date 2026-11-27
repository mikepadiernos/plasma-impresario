/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QApplication>
#include <QDeadlineTimer>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include <memory>
#include <optional>

#include <KConfig>
#include <KSharedConfig>

#include "Gui/MailSafetyPrompt.h"
#include "Imap/Model/ItemRoles.h"
#include "UiUtils/ActivityLog.h"
#include "UiUtils/MailSafetySettings.h"

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

    QTemporaryDir m_dir;

    /** The application's settings, inside the test mode's configuration folder */
    static KSharedConfigPtr appConfig()
    {
        return KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
    }

    QString writeFile(const QString &name, const QByteArray &content)
    {
        QFile file(m_dir.filePath(name));
        if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size())
            return QString();
        return file.fileName();
    }

    /** @short Run scanBeforeOpening() and wait for its answer */
    static std::optional<bool> scanAndWait(const QString &file, const QString &fileName)
    {
        auto result = std::make_shared<std::optional<bool>>();
        QWidget parent;
        Prompt::scanBeforeOpening(&parent, file, fileName, [result](bool open) { *result = open; });
        QDeadlineTimer deadline(5000);
        while (!result->has_value() && !deadline.hasExpired())
            QTest::qWait(10);
        return *result;
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
        QFile::remove(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                      + QLatin1String("/plasma-impresariorc"));
        appConfig()->reparseConfiguration();
    }

    void safetyHooksAreOffByDefault()
    {
        KConfig config(m_dir.filePath(QStringLiteral("safetyrc")), KConfig::SimpleConfig);
        QVERIFY(!UiUtils::MailSafetySettings::scansAttachments(config));
        QVERIFY(!UiUtils::MailSafetySettings::movesJunk(config));
        QVERIFY(UiUtils::MailSafetySettings::scannerSocket(config).isEmpty());

        UiUtils::MailSafetySettings::setScansAttachments(config, true);
        UiUtils::MailSafetySettings::setMovesJunk(config, true);
        UiUtils::MailSafetySettings::setScannerSocket(config, QStringLiteral(" /run/custom/clamd.sock "));
        QVERIFY(UiUtils::MailSafetySettings::scansAttachments(config));
        QVERIFY(UiUtils::MailSafetySettings::movesJunk(config));
        // A configured socket is used even when it does not exist, so that the failure is reported
        QCOMPARE(UiUtils::MailSafetySettings::scannerSocketToUse(config), QStringLiteral("/run/custom/clamd.sock"));
    }

    void scanningOffOpensAtOnce()
    {
        UiUtils::MailSafetySettings::setScansAttachments(*appConfig(), false);
        Answer answer;
        bool called = false;
        bool opened = false;
        Prompt::scanBeforeOpening(nullptr, m_dir.filePath(QStringLiteral("never-read.pdf")), QStringLiteral("a.pdf"),
                                  [&called, &opened](bool open) {
            called = true;
            opened = open;
        });
        QVERIFY(called);
        QVERIFY(opened);
        QTest::qWait(50);
        QVERIFY(answer.objectName.isEmpty());
    }

    void unreachableScannerAsksFirst()
    {
        UiUtils::MailSafetySettings::setScansAttachments(*appConfig(), true);
        UiUtils::MailSafetySettings::setScannerSocket(*appConfig(), m_dir.filePath(QStringLiteral("nobody.sock")));
        const QString file = writeFile(QStringLiteral("report.doc"), QByteArrayLiteral("data"));
        Answer answer;
        const std::optional<bool> opened = scanAndWait(file, QStringLiteral("report.doc"));
        QVERIFY(opened.has_value());
        QVERIFY(!*opened);
        QCOMPARE(answer.objectName, QStringLiteral("attachmentConfirmation"));
        QVERIFY2(answer.text.contains(QLatin1String("not available")), qPrintable(answer.text));
    }

    void foundThreatIsBlockedAndNamed()
    {
        // Answers once the command, one chunk of the four bytes and the end of the stream have arrived
        QLocalServer clamd;
        const QString socket = m_dir.filePath(QStringLiteral("clamd.sock"));
        QVERIFY(clamd.listen(socket));
        auto received = std::make_shared<QByteArray>();
        connect(&clamd, &QLocalServer::newConnection, &clamd, [&clamd, received]() {
            QLocalSocket *client = clamd.nextPendingConnection();
            connect(client, &QLocalSocket::readyRead, client, [client, received]() {
                *received += client->readAll();
                if (received->size() == 10 + 8 + 4)
                    client->write(QByteArray("stream: Eicar-Signature FOUND\0", 30));
            });
        });
        UiUtils::MailSafetySettings::setScansAttachments(*appConfig(), true);
        UiUtils::MailSafetySettings::setScannerSocket(*appConfig(), socket);
        const QString file = writeFile(QStringLiteral("eicar.com"), QByteArrayLiteral("data"));

        UiUtils::ActivityLog log;
        Prompt::setActivityLog(&log);
        Answer answer(QStringLiteral("openAttachmentAnyway"));
        const std::optional<bool> opened = scanAndWait(file, QStringLiteral("eicar.com"));
        QVERIFY(opened.has_value());
        QVERIFY(!*opened);
        QCOMPARE(answer.objectName, QStringLiteral("attachmentBlocked"));
        QVERIFY2(answer.text.contains(QLatin1String("Eicar-Signature")), qPrintable(answer.text));
        QVERIFY(received->startsWith(QByteArray("zINSTREAM\0", 10)));
        // The scan is in the activity log as well, as one entry which ends failed with the threat
        QCOMPARE(log.count(), 1);
        const UiUtils::ActivityLog::Entry entry = log.entry(0);
        QCOMPARE(entry.kind, UiUtils::ActivityLog::Kind::Scan);
        QCOMPARE(entry.state, UiUtils::ActivityLog::State::Failed);
        QVERIFY2(entry.error.contains(QLatin1String("Eicar-Signature")), qPrintable(entry.error));
        Prompt::setActivityLog(nullptr);
        UiUtils::MailSafetySettings::setScansAttachments(*appConfig(), false);
    }

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
