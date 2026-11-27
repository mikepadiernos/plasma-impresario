/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

#include "UiUtils/MailToProductivity.h"

using namespace UiUtils::MailToProductivity;

class MailToProductivityTest : public QObject
{
    Q_OBJECT

private:
    static Message sample()
    {
        Message message;
        message.subject = QStringLiteral("Budget; review, Q4");
        message.from = QStringLiteral("Ana <ana@example.org>");
        message.to = {QStringLiteral("me@example.org"), QStringLiteral("bo@example.org")};
        message.date = QDateTime(QDate(2026, 11, 27), QTime(9, 30), QTimeZone::UTC);
        message.messageId = QStringLiteral("abc@example.org");
        message.accountName = QStringLiteral("Work");
        message.folder = QStringLiteral("INBOX");
        message.text = QStringLiteral("# Agenda\nLine two\n\nBye");
        return message;
    }

private slots:
    void escaping()
    {
        QCOMPARE(escapeText(QStringLiteral("a;b,c\\d\r\ne")), QStringLiteral("a\\;b\\,c\\\\d\\ne"));
    }

    void folding()
    {
        const QByteArray shortLine(75, 'a');
        QCOMPARE(fold(shortLine), shortLine);
        const QByteArray longLine(200, 'b');
        const QList<QByteArray> parts = fold(longLine).split('\n');
        QCOMPARE(parts.size(), 3);
        QCOMPARE(parts.at(0).size(), 76); // 75 octets and the carriage return
        QVERIFY(parts.at(1).startsWith(' '));
        QVERIFY(parts.at(1).size() <= 76);
        QByteArray unfolded = fold(longLine);
        unfolded.replace("\r\n ", "");
        QCOMPARE(unfolded, longLine);
        // A UTF-8 sequence is never split
        const QByteArray accented = QStringLiteral("é").repeated(60).toUtf8();
        for (const QByteArray &part : fold(accented).split('\n')) {
            const QString decoded = QString::fromUtf8(part.trimmed());
            QVERIFY(!decoded.contains(QChar::ReplacementCharacter));
        }
    }

    void startsOnTheNextHour()
    {
        const QDateTime now(QDate(2026, 11, 27), QTime(14, 20, 5));
        QCOMPARE(suggestedStart(now), QDateTime(QDate(2026, 11, 27), QTime(15, 0)));
        QCOMPARE(suggestedStart(QDateTime(QDate(2026, 11, 27), QTime(23, 59))), QDateTime(QDate(2026, 11, 28), QTime(0, 0)));
    }

    void event()
    {
        const QDateTime start(QDate(2026, 11, 27), QTime(15, 0), QTimeZone::UTC);
        const QDateTime stamp(QDate(2026, 11, 27), QTime(14, 0), QTimeZone::UTC);
        const QByteArray ics = calendarEvent(sample(), start, 30, QStringLiteral("uid-1"), stamp);
        QVERIFY(ics.startsWith("BEGIN:VCALENDAR\r\nVERSION:2.0\r\n"));
        QVERIFY(ics.endsWith("END:VEVENT\r\nEND:VCALENDAR\r\n"));
        QVERIFY(ics.contains("DTSTART:20261127T150000Z\r\n"));
        QVERIFY(ics.contains("DTEND:20261127T153000Z\r\n"));
        QVERIFY(ics.contains("DTSTAMP:20261127T140000Z\r\n"));
        QVERIFY(ics.contains("SUMMARY:Budget\\; review\\, Q4\r\n"));
        QVERIFY(ics.contains("URL:mid:abc@example.org\r\n"));
        QVERIFY(ics.contains("UID:uid-1\r\n"));
        // Every line, folded or not, is at most 75 octets
        for (const QByteArray &line : ics.split('\n'))
            QVERIFY(line.size() <= 76);
        Message bare;
        const QByteArray minimal = calendarEvent(bare, start, 0, QStringLiteral("u"), stamp);
        QVERIFY(minimal.contains("DTEND:20261127T150100Z"));
        QVERIFY(!minimal.contains("URL:"));
    }

    void note()
    {
        const QString text = noteText(sample());
        QVERIFY(text.startsWith(QLatin1String("# Budget; review, Q4\n\n")));
        QVERIFY(text.contains(QLatin1String("Ana <ana@example.org>")));
        QVERIFY(text.contains(QLatin1String("me@example.org, bo@example.org")));
        QVERIFY(text.contains(QLatin1String("<mid:abc@example.org>")));
        // The message's own heading is quoted rather than becoming one of the note
        QVERIFY(text.contains(QLatin1String("> # Agenda\n> Line two\n>\n> Bye\n")));
    }

    void fileNames()
    {
        Message message = sample();
        QCOMPARE(noteFileName(message, {}), QStringLiteral("2026-11-27 Budget; review, Q4.md"));
        QCOMPARE(noteFileName(message, {QStringLiteral("2026-11-27 budget; review, q4.md")}),
                 QStringLiteral("2026-11-27 Budget; review, Q4 (2).md"));
        message.subject = QStringLiteral("../a/b:c*?\"<>|");
        QCOMPARE(noteFileName(message, {}), QStringLiteral("2026-11-27 -a-b-c------.md"));
        message.subject = QString(200, QLatin1Char('x'));
        QCOMPARE(noteFileName(message, {}).size(), 11 + 80 + 3);
        message.subject.clear();
        QVERIFY(noteFileName(message, {}).endsWith(QLatin1String(".md")));
    }

    void notesAreNewFilesInTheFolder()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QString error;
        const QString first = saveNote(folder.path(), sample(), &error);
        QVERIFY2(!first.isEmpty(), qPrintable(error));
        QCOMPARE(QFileInfo(first).fileName(), QStringLiteral("2026-11-27 Budget; review, Q4.md"));
        QFile written(first);
        QVERIFY(written.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(written.readAll()), noteText(sample()));
        // A second note about the same message does not replace the first
        const QString second = saveNote(folder.path(), sample(), &error);
        QCOMPARE(QFileInfo(second).fileName(), QStringLiteral("2026-11-27 Budget; review, Q4 (2).md"));

        QVERIFY(saveNote(folder.filePath(QStringLiteral("missing")), sample(), &error).isEmpty());
        QVERIFY(error.contains(QLatin1String("missing")));
        QVERIFY(saveNote(QString(), sample()).isEmpty());
    }

    void eventsAreWrittenAndOldOnesRemoved()
    {
        QTemporaryDir cache;
        QVERIFY(cache.isValid());
        const QString directory = cache.filePath(QStringLiteral("events"));
        const QDateTime now = QDateTime::currentDateTime();
        QString error;
        const QString old = saveEvent(directory, "BEGIN:VCALENDAR\r\n", QStringLiteral("old@x"), now, &error);
        QVERIFY2(!old.isEmpty(), qPrintable(error));
        QCOMPARE(QFileInfo(old).fileName(), QStringLiteral("old-x.ics"));
        QFile oldFile(old);
        QVERIFY(oldFile.open(QIODevice::ReadWrite));
        QVERIFY(oldFile.setFileTime(now.addDays(-2), QFileDevice::FileModificationTime));
        oldFile.close();

        const QString recent = saveEvent(directory, "BEGIN:VCALENDAR\r\n", QStringLiteral("recent/../x"), now, &error);
        QCOMPARE(QFileInfo(recent).fileName(), QStringLiteral("recent----x.ics"));
        QCOMPARE(QFileInfo(recent).absolutePath(), QDir(directory).absolutePath());
        const QString next = saveEvent(directory, "BEGIN:VCALENDAR\r\n", QStringLiteral("next"), now.addSecs(60), &error);
        QVERIFY(!next.isEmpty());
        QVERIFY(!QFileInfo::exists(old));
        QVERIFY(QFileInfo::exists(recent));
    }
};

QTEST_GUILESS_MAIN(MailToProductivityTest)
#include "test_MailToProductivity.moc"
