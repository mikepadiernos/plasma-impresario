/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class KConfigBase;

namespace UiUtils {

/** @short Running with the window closed and starting after login

Both are off by default. Outside a sandbox, starting after login writes an XDG autostart entry, which Plasma runs at
login; inside a Flatpak or Snap sandbox the autostart folder is not the host's, so the XDG Background portal is asked
instead. The window and the portal call live in the GUI; this namespace holds the decisions and the file handling.
*/
namespace BackgroundRunning {

/** The command-line option which starts the application without showing its window */
QString backgroundOption();

/** Whether closing the main window keeps the application running, as stored in @arg config */
bool runsInBackground(const KConfigBase &config);
void setRunsInBackground(KConfigBase &config, bool enabled);
/** Whether the application starts after login, as stored in @arg config */
bool startsAfterLogin(const KConfigBase &config);
void setStartsAfterLogin(KConfigBase &config, bool enabled);

/** Whether the application runs in a Flatpak or Snap sandbox

@arg flatpakInfo is the path of the file which Flatpak places in every sandbox; @arg snap is the value of $SNAP.
*/
bool isSandboxed(const QString &flatpakInfo, const QByteArray &snap);
/** Whether this process runs in a sandbox */
bool isSandboxed();

/** The command which starts the application in the background, with its profile when there is one */
QStringList commandLine(const QString &program, const QString &profile);
/** One argument of a desktop entry's Exec key, quoted and escaped as the Desktop Entry Specification asks */
QString quoteExecArgument(const QString &argument);

/** Where the autostart entry for @arg desktopFileName goes, in the user's autostart folder */
QString autostartPath(const QString &desktopFileName);
/** The contents of the autostart entry */
QByteArray autostartEntry(const QString &name, const QString &iconName, const QStringList &commandLine);
/** Whether the file at @arg path is an autostart entry which this application wrote */
bool isOwnAutostartEntry(const QString &path);
/** Write the autostart entry, replacing an earlier one; @arg error describes a failure */
bool writeAutostartEntry(const QString &path, const QByteArray &contents, QString *error);
/** Remove the autostart entry, but only one this application wrote; a missing entry is not an error */
bool removeAutostartEntry(const QString &path, QString *error);

/** The options of the Background portal's RequestBackground call */
QVariantMap portalOptions(bool autostart, const QStringList &commandLine, const QString &reason);

}

}
