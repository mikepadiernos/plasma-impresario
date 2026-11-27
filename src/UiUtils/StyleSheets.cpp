/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/StyleSheets.h"

#include <QCoreApplication>
#include <QFile>
#include <QSaveFile>

namespace UiUtils {

namespace StyleSheets {

namespace {

QString tr(const char *text, const char *comment = nullptr)
{
    return QCoreApplication::translate("UiUtils::StyleSheets", text, comment);
}

QString atLine(int line, const QString &problem)
{
    return tr("Line %1: %2").arg(line).arg(problem);
}

/** @short Whether @arg word starts at @arg index, not inside a longer
 * identifier */
bool wordAt(const QString &sheet, qsizetype index, QLatin1StringView word)
{
    if (index > 0) {
        const QChar previous = sheet.at(index - 1);
        if (previous.isLetterOrNumber() || previous == QLatin1Char('-') || previous == QLatin1Char('_'))
            return false;
    }
    return QStringView(sheet).mid(index, word.size()).compare(word, Qt::CaseInsensitive) == 0;
}

int lineBreaks(QStringView text)
{
    return static_cast<int>(text.count(QLatin1Char('\n')));
}

} // namespace

QString check(const QString &sheet)
{
    if (sheet.size() > MaximumSize)
        return tr("The stylesheet is longer than %1 characters.").arg(MaximumSize);
    int line = 1;
    int depth = 0;
    int parentheses = 0;
    int rules = 0;
    int openedAt = 0;
    const qsizetype size = sheet.size();
    for (qsizetype i = 0; i < size; ++i) {
        const QChar ch = sheet.at(i);
        if (ch == QLatin1Char('\n')) {
            ++line;
            continue;
        }
        if (ch.category() == QChar::Other_Control && ch != QLatin1Char('\t') && ch != QLatin1Char('\r') &&
            ch != QLatin1Char('\f'))
            return atLine(line, tr("Control characters are not allowed."));
        if (ch == QLatin1Char('/') && i + 1 < size && sheet.at(i + 1) == QLatin1Char('*')) {
            const qsizetype end = sheet.indexOf(QLatin1String("*/"), i + 2);
            if (end < 0)
                return atLine(line, tr("A comment is not closed with */."));
            line += lineBreaks(QStringView(sheet).mid(i, end - i));
            i = end + 1;
            continue;
        }
        if (ch == QLatin1Char('"') || ch == QLatin1Char('\'')) {
            qsizetype end = i + 1;
            bool closed = false;
            for (; end < size && sheet.at(end) != QLatin1Char('\n'); ++end) {
                if (sheet.at(end) == QLatin1Char('\\')) {
                    ++end;
                } else if (sheet.at(end) == ch) {
                    closed = true;
                    break;
                }
            }
            if (!closed)
                return atLine(line, tr("A quoted string is not closed."));
            i = end;
            continue;
        }
        if (ch == QLatin1Char('@'))
            return atLine(line, tr("At-rules such as @import are not supported."));
        if (wordAt(sheet, i, QLatin1StringView("qproperty-")))
            return atLine(line,
                          tr("qproperty- declarations change how widgets "
                             "behave and are not allowed."));
        if (wordAt(sheet, i, QLatin1StringView("url("))) {
            const qsizetype close = sheet.indexOf(QLatin1Char(')'), i + 4);
            if (close < 0)
                return atLine(line, tr("A url( is not closed."));
            QString target = sheet.mid(i + 4, close - i - 4).trimmed();
            if (target.size() >= 2 && (target.front() == QLatin1Char('"') || target.front() == QLatin1Char('\'')) &&
                target.back() == target.front())
                target = target.mid(1, target.size() - 2).trimmed();
            if (!target.startsWith(QLatin1String(":/")) &&
                !target.startsWith(QLatin1String("qrc:/"), Qt::CaseInsensitive))
                return atLine(line,
                              tr("Loading %1 is blocked; only images built into "
                                 "the application (:/...) "
                                 "can be used.")
                                  .arg(target.left(80)));
            line += lineBreaks(QStringView(sheet).mid(i, close - i));
            i = close;
            continue;
        }
        switch (ch.unicode()) {
        case '{':
            if (++depth > 1)
                return atLine(line, tr("Rules cannot be nested."));
            openedAt = line;
            if (++rules > MaximumRules)
                return atLine(line, tr("A stylesheet can have at most %1 rules.").arg(MaximumRules));
            break;
        case '}':
            if (--depth < 0)
                return atLine(line, tr("A } has no matching {."));
            break;
        case '(':
            ++parentheses;
            break;
        case ')':
            if (--parentheses < 0)
                return atLine(line, tr("A ) has no matching (."));
            break;
        default:
            break;
        }
    }
    if (depth > 0)
        return atLine(openedAt, tr("A rule is not closed with }."));
    if (parentheses > 0)
        return atLine(line, tr("A ( is not closed."));
    return {};
}

std::optional<QString> readFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = tr("Could not open %1: %2").arg(path, file.errorString());
        return std::nullopt;
    }
    const QByteArray data = file.read(MaximumSize + 1);
    if (data.size() > MaximumSize) {
        if (error)
            *error = tr("The stylesheet is larger than %1 bytes.").arg(MaximumSize);
        return std::nullopt;
    }
    const QString sheet = QString::fromUtf8(data);
    if (sheet.contains(QChar::ReplacementCharacter) && !data.contains("\xEF\xBF\xBD")) {
        if (error)
            *error = tr("The stylesheet is not UTF-8 text.");
        return std::nullopt;
    }
    return sheet;
}

QString writeFile(const QString &path, const QString &sheet)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(sheet.toUtf8()) < 0 || !file.commit())
        return tr("Could not write %1: %2").arg(path, file.errorString());
    return {};
}

} // namespace StyleSheets

} // namespace UiUtils
