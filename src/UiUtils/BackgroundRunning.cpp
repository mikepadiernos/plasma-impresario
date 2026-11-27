/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/BackgroundRunning.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

#include <KConfigGroup>

namespace UiUtils {

namespace BackgroundRunning {

namespace {

const char groupName[] = "Background";
const char runKey[] = "RunInBackground";
const char loginKey[] = "StartAfterLogin";
/** Marks the autostart entries which this application wrote, so that it never removes one the user made */
const QByteArray ownerKey = QByteArrayLiteral("X-Plasma-Impresario-Autostart");

/** Escape a desktop entry string value */
QString escapeValue(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    escaped.replace(QLatin1Char('\n'), QLatin1String("\\n"));
    escaped.replace(QLatin1Char('\t'), QLatin1String("\\t"));
    escaped.replace(QLatin1Char('\r'), QLatin1String("\\r"));
    return escaped;
}

}

QString backgroundOption()
{
    return QStringLiteral("background");
}

bool runsInBackground(const KConfigBase &config)
{
    return config.group(QLatin1String(groupName)).readEntry(runKey, false);
}

void setRunsInBackground(KConfigBase &config, bool enabled)
{
    KConfigGroup group = config.group(QLatin1String(groupName));
    group.writeEntry(runKey, enabled);
    group.sync();
}

bool startsAfterLogin(const KConfigBase &config)
{
    return config.group(QLatin1String(groupName)).readEntry(loginKey, false);
}

void setStartsAfterLogin(KConfigBase &config, bool enabled)
{
    KConfigGroup group = config.group(QLatin1String(groupName));
    group.writeEntry(loginKey, enabled);
    group.sync();
}

bool isSandboxed(const QString &flatpakInfo, const QByteArray &snap)
{
    return (!flatpakInfo.isEmpty() && QFileInfo::exists(flatpakInfo)) || !snap.isEmpty();
}

bool isSandboxed()
{
    return isSandboxed(QStringLiteral("/.flatpak-info"), qgetenv("SNAP"));
}

QStringList commandLine(const QString &program, const QString &profile)
{
    QStringList command{program, QLatin1String("--") + backgroundOption()};
    if (!profile.isEmpty())
        command << QStringLiteral("--profile") << profile;
    return command;
}

QString quoteExecArgument(const QString &argument)
{
    // Reserved characters of the Exec key; an argument with any of them is quoted
    static const QString reserved = QStringLiteral(" \t\n\"'\\><~|&;$*?#()`");
    QString escaped = argument;
    // A literal percent sign would start a field code
    escaped.replace(QLatin1Char('%'), QLatin1String("%%"));
    bool needsQuotes = argument.isEmpty();
    for (const QChar c : argument) {
        if (reserved.contains(c)) {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes)
        return escaped;
    QString quoted;
    for (const QChar c : std::as_const(escaped)) {
        if (c == QLatin1Char('"') || c == QLatin1Char('`') || c == QLatin1Char('$') || c == QLatin1Char('\\'))
            quoted += QLatin1Char('\\');
        quoted += c;
    }
    return QLatin1Char('"') + quoted + QLatin1Char('"');
}

QString autostartPath(const QString &desktopFileName)
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
            .filePath(QStringLiteral("autostart/%1.desktop").arg(desktopFileName));
}

QByteArray autostartEntry(const QString &name, const QString &iconName, const QStringList &commandLine)
{
    QStringList exec;
    for (const QString &argument : commandLine)
        exec << quoteExecArgument(argument);
    QByteArray entry;
    entry += "[Desktop Entry]\n";
    entry += "Type=Application\n";
    entry += "Name=" + escapeValue(name).toUtf8() + '\n';
    if (!iconName.isEmpty())
        entry += "Icon=" + escapeValue(iconName).toUtf8() + '\n';
    entry += "Exec=" + escapeValue(exec.join(QLatin1Char(' '))).toUtf8() + '\n';
    entry += "Terminal=false\n";
    entry += "NoDisplay=true\n";
    entry += "X-GNOME-Autostart-enabled=true\n";
    entry += ownerKey + "=true\n";
    return entry;
}

bool isOwnAutostartEntry(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        const qsizetype equals = line.indexOf('=');
        if (equals > 0 && line.left(equals).trimmed() == ownerKey)
            return line.mid(equals + 1).trimmed() == "true";
    }
    return false;
}

bool writeAutostartEntry(const QString &path, const QByteArray &contents, QString *error)
{
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error)
            *error = QStringLiteral("Cannot create the folder %1").arg(info.absolutePath());
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(contents);
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

bool removeAutostartEntry(const QString &path, QString *error)
{
    if (!QFileInfo::exists(path) || !isOwnAutostartEntry(path))
        return true;
    QFile file(path);
    if (!file.remove()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

QVariantMap portalOptions(bool autostart, const QStringList &commandLine, const QString &reason)
{
    QVariantMap options;
    options.insert(QStringLiteral("reason"), reason);
    options.insert(QStringLiteral("autostart"), autostart);
    options.insert(QStringLiteral("dbus-activatable"), false);
    if (autostart)
        options.insert(QStringLiteral("commandline"), commandLine);
    return options;
}

}

}
