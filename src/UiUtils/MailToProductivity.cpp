/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/MailToProductivity.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QUrl>

namespace UiUtils {

namespace MailToProductivity {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("UiUtils::MailToProductivity", text);
}

QString subjectOrPlaceholder(const Message &message)
{
    const QString subject = message.subject.simplified();
    return subject.isEmpty() ? tr("(no subject)") : subject;
}

QByteArray utcStamp(const QDateTime &time)
{
    return time.toUTC().toString(QStringLiteral("yyyyMMdd'T'HHmmss'Z'")).toLatin1();
}

}

QString escapeText(const QString &text)
{
    QString result;
    result.reserve(text.size());
    QString normalized = text;
    normalized.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    for (const QChar ch : std::as_const(normalized)) {
        if (ch == QLatin1Char('\\'))
            result += QLatin1String("\\\\");
        else if (ch == QLatin1Char(';'))
            result += QLatin1String("\\;");
        else if (ch == QLatin1Char(','))
            result += QLatin1String("\\,");
        else if (ch == QLatin1Char('\n'))
            result += QLatin1String("\\n");
        else if (ch != QLatin1Char('\r'))
            result += ch;
    }
    return result;
}

QByteArray fold(const QByteArray &line)
{
    QByteArray result;
    qsizetype start = 0;
    int limit = 75;
    while (line.size() - start > limit) {
        qsizetype end = start + limit;
        // Never split a UTF-8 sequence: continuation bytes are 10xxxxxx
        while (end > start && (static_cast<unsigned char>(line.at(end)) & 0xC0) == 0x80)
            --end;
        result += line.mid(start, end - start);
        result += "\r\n ";
        start = end;
        limit = 74;
    }
    result += line.mid(start);
    return result;
}

QDateTime suggestedStart(const QDateTime &now)
{
    QDateTime start = now;
    start.setTime(QTime(now.time().hour(), 0));
    return start.addSecs(3600);
}

QByteArray calendarEvent(const Message &message, const QDateTime &start, int minutes, const QString &uid,
                         const QDateTime &stamp)
{
    QString description = tr("From: %1").arg(message.from);
    if (message.date.isValid())
        description += QLatin1Char('\n') + tr("Date: %1").arg(message.date.toString(Qt::ISODate));
    if (!message.text.trimmed().isEmpty())
        description += QLatin1String("\n\n") + message.text.trimmed();
    QList<QByteArray> lines = {
        "BEGIN:VCALENDAR",
        "VERSION:2.0",
        "PRODID:-//Plasma Impresario//Mail to calendar//EN",
        "BEGIN:VEVENT",
        "UID:" + escapeText(uid).toUtf8(),
        "DTSTAMP:" + utcStamp(stamp),
        "DTSTART:" + utcStamp(start),
        "DTEND:" + utcStamp(start.addSecs(qint64(std::max(minutes, 1)) * 60)),
        "SUMMARY:" + escapeText(subjectOrPlaceholder(message)).toUtf8(),
        "DESCRIPTION:" + escapeText(description).toUtf8(),
    };
    if (!message.messageId.isEmpty())
        lines << "URL:mid:" + QUrl::toPercentEncoding(message.messageId, "@!$&'()*+,;=-._~");
    lines << "END:VEVENT" << "END:VCALENDAR";
    QByteArray result;
    for (const QByteArray &line : std::as_const(lines))
        result += fold(line) + "\r\n";
    return result;
}

QString noteText(const Message &message)
{
    QString text = QLatin1String("# ") + subjectOrPlaceholder(message) + QLatin1String("\n\n");
    text += QLatin1String("- ") + tr("From: %1").arg(message.from) + QLatin1Char('\n');
    if (!message.to.isEmpty())
        text += QLatin1String("- ") + tr("To: %1").arg(message.to.join(QLatin1String(", "))) + QLatin1Char('\n');
    if (message.date.isValid())
        text += QLatin1String("- ") + tr("Date: %1").arg(message.date.toString(Qt::ISODate)) + QLatin1Char('\n');
    if (!message.accountName.isEmpty() || !message.folder.isEmpty())
        text += QLatin1String("- ") + tr("Folder: %1, %2").arg(message.accountName, message.folder) + QLatin1Char('\n');
    if (!message.messageId.isEmpty())
        text += QLatin1String("- ") + tr("Message: <mid:%1>").arg(message.messageId) + QLatin1Char('\n');
    const QString body = message.text.trimmed();
    if (!body.isEmpty()) {
        text += QLatin1Char('\n');
        // A quote keeps headings and lists of the message from becoming part of the note's structure
        for (const QString &line : body.split(QLatin1Char('\n')))
            text += (line.isEmpty() ? QStringLiteral(">") : QString(QLatin1String("> ") + line)) + QLatin1Char('\n');
    }
    return text;
}

QString noteFileName(const Message &message, const QStringList &existing)
{
    QString subject = subjectOrPlaceholder(message);
    static const QString forbidden = QStringLiteral("/\\:*?\"<>|");
    for (QChar &ch : subject) {
        if (forbidden.contains(ch) || ch.category() == QChar::Other_Control)
            ch = QLatin1Char('-');
    }
    subject = subject.simplified();
    while (subject.startsWith(QLatin1Char('.')))
        subject.remove(0, 1);
    if (subject.size() > 80)
        subject = subject.left(80).trimmed();
    const QDate date = (message.date.isValid() ? message.date : QDateTime::currentDateTime()).date();
    const QString base = date.toString(Qt::ISODate) + QLatin1Char(' ') + subject;
    QString name = base + QLatin1String(".md");
    for (int n = 2; existing.contains(name, Qt::CaseInsensitive); ++n)
        name = QStringLiteral("%1 (%2).md").arg(base).arg(n);
    return name;
}

QString saveNote(const QString &folder, const Message &message, QString *error)
{
    const QDir dir(folder);
    if (folder.isEmpty() || !dir.exists()) {
        if (error)
            *error = tr("The notes folder \"%1\" does not exist.").arg(folder);
        return QString();
    }
    const QString path = dir.filePath(noteFileName(message, dir.entryList(QDir::Files | QDir::Hidden)));
    QSaveFile file(path);
    // QSaveFile replaces files; a note which appeared since the folder was listed is kept
    if (QFileInfo::exists(path) || !file.open(QIODevice::WriteOnly) || file.write(noteText(message).toUtf8()) < 0
        || !file.commit()) {
        if (error)
            *error = tr("The note could not be saved to \"%1\": %2").arg(path, file.errorString());
        return QString();
    }
    return path;
}

QString saveEvent(const QString &directory, const QByteArray &ics, const QString &uid, const QDateTime &now,
                  QString *error)
{
    QDir dir(directory);
    if (!dir.mkpath(QStringLiteral("."))) {
        if (error)
            *error = tr("The folder \"%1\" could not be created.").arg(directory);
        return QString();
    }
    const QDateTime cutoff = now.addDays(-1);
    const QFileInfoList old = dir.entryInfoList({QStringLiteral("*.ics")}, QDir::Files);
    for (const QFileInfo &info : old) {
        if (info.lastModified() < cutoff)
            QFile::remove(info.absoluteFilePath());
    }
    QString name = uid;
    for (QChar &ch : name) {
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('-'))
            ch = QLatin1Char('-');
    }
    const QString path = dir.filePath(name + QLatin1String(".ics"));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(ics) < 0 || !file.commit()) {
        if (error)
            *error = tr("The event could not be written to \"%1\": %2").arg(path, file.errorString());
        return QString();
    }
    return path;
}

}

}
