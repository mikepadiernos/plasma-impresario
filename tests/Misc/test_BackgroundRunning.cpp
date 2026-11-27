/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QFile>
#include <QStandardPaths>
#include <QTest>

#include <KConfig>
#include <KConfigGroup>
#include <KDesktopFile>

#include "test_BackgroundRunning.h"
#include "UiUtils/BackgroundRunning.h"

namespace Background = UiUtils::BackgroundRunning;

void BackgroundRunningTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(m_dir.isValid());
}

void BackgroundRunningTest::offByDefault()
{
    KConfig config(m_dir.filePath(QStringLiteral("settingsrc")), KConfig::SimpleConfig);
    QVERIFY(!Background::runsInBackground(config));
    QVERIFY(!Background::startsAfterLogin(config));
    Background::setRunsInBackground(config, true);
    QVERIFY(Background::runsInBackground(config));
    QVERIFY(!Background::startsAfterLogin(config));
    Background::setStartsAfterLogin(config, true);

    KConfig reread(m_dir.filePath(QStringLiteral("settingsrc")), KConfig::SimpleConfig);
    QVERIFY(Background::runsInBackground(reread));
    QVERIFY(Background::startsAfterLogin(reread));
    Background::setRunsInBackground(reread, false);
    QVERIFY(!Background::runsInBackground(reread));
}

void BackgroundRunningTest::sandboxDetection()
{
    QVERIFY(!Background::isSandboxed(m_dir.filePath(QStringLiteral("missing")), QByteArray()));
    QVERIFY(!Background::isSandboxed(QString(), QByteArray()));
    QVERIFY(Background::isSandboxed(QString(), QByteArrayLiteral("/snap/plasma-impresario/12")));
    const QString info = m_dir.filePath(QStringLiteral("flatpak-info"));
    QFile file(info);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("[Application]\nname=io.github.mikepadiernos.plasma_impresario\n");
    file.close();
    QVERIFY(Background::isSandboxed(info, QByteArray()));
}

void BackgroundRunningTest::commandLineCarriesTheProfile()
{
    QCOMPARE(Background::commandLine(QStringLiteral("/usr/bin/plasma-impresario"), QString()),
             QStringList({QStringLiteral("/usr/bin/plasma-impresario"), QStringLiteral("--background")}));
    QCOMPARE(Background::commandLine(QStringLiteral("plasma-impresario"), QStringLiteral("work")),
             QStringList({QStringLiteral("plasma-impresario"), QStringLiteral("--background"), QStringLiteral("--profile"),
                          QStringLiteral("work")}));
}

void BackgroundRunningTest::quotesExecArguments_data()
{
    QTest::addColumn<QString>("argument");
    QTest::addColumn<QString>("expected");
    QTest::newRow("plain") << QStringLiteral("/usr/bin/plasma-impresario") << QStringLiteral("/usr/bin/plasma-impresario");
    QTest::newRow("option") << QStringLiteral("--background") << QStringLiteral("--background");
    QTest::newRow("space") << QStringLiteral("/opt/My Apps/plasma-impresario")
                           << QStringLiteral("\"/opt/My Apps/plasma-impresario\"");
    QTest::newRow("quote and dollar") << QStringLiteral("a\"b$c") << QStringLiteral("\"a\\\"b\\$c\"");
    QTest::newRow("backslash") << QStringLiteral("a\\b") << QStringLiteral("\"a\\\\b\"");
    QTest::newRow("percent") << QStringLiteral("50%") << QStringLiteral("50%%");
    QTest::newRow("empty") << QString() << QStringLiteral("\"\"");
}

void BackgroundRunningTest::quotesExecArguments()
{
    QFETCH(QString, argument);
    QFETCH(QString, expected);
    QCOMPARE(Background::quoteExecArgument(argument), expected);
}

void BackgroundRunningTest::autostartEntryContents()
{
    const QStringList command{QStringLiteral("/opt/My Apps/plasma-impresario"), QStringLiteral("--background"),
                              QStringLiteral("--profile"), QStringLiteral("work")};
    const QByteArray entry = Background::autostartEntry(QStringLiteral("Plasma Impresario"),
                                                        QStringLiteral("io.github.mikepadiernos.plasma_impresario"), command);
    const QString path = m_dir.filePath(QStringLiteral("contents.desktop"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(entry);
    file.close();

    // Read back as Plasma reads it
    KDesktopFile desktop(path);
    QCOMPARE(desktop.readType(), QStringLiteral("Application"));
    QCOMPARE(desktop.readName(), QStringLiteral("Plasma Impresario"));
    QCOMPARE(desktop.readIcon(), QStringLiteral("io.github.mikepadiernos.plasma_impresario"));
    QCOMPARE(desktop.desktopGroup().readEntry("Exec"),
             QStringLiteral("\"/opt/My Apps/plasma-impresario\" --background --profile work"));
    QVERIFY(desktop.noDisplay());
    QVERIFY(Background::isOwnAutostartEntry(path));
}

void BackgroundRunningTest::autostartPathIsInTheUserFolder()
{
    const QString path = Background::autostartPath(QStringLiteral("io.github.mikepadiernos.plasma_impresario"));
    QVERIFY2(path.startsWith(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)), qPrintable(path));
    QVERIFY2(path.endsWith(QLatin1String("/autostart/io.github.mikepadiernos.plasma_impresario.desktop")), qPrintable(path));
}

void BackgroundRunningTest::writesAndRemovesTheEntry()
{
    const QString path = m_dir.filePath(QStringLiteral("config/autostart/app.desktop"));
    QString error;
    // Removing an entry which is not there is not an error
    QVERIFY(Background::removeAutostartEntry(path, &error));
    const QByteArray first = Background::autostartEntry(QStringLiteral("App"), QString(), {QStringLiteral("app")});
    QVERIFY2(Background::writeAutostartEntry(path, first, &error), qPrintable(error));
    QVERIFY(QFile::exists(path));
    // Writing again replaces the entry
    const QByteArray second =
            Background::autostartEntry(QStringLiteral("App"), QString(), {QStringLiteral("app"), QStringLiteral("--background")});
    QVERIFY2(Background::writeAutostartEntry(path, second, &error), qPrintable(error));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), second);
    file.close();
    QVERIFY2(Background::removeAutostartEntry(path, &error), qPrintable(error));
    QVERIFY(!QFile::exists(path));
}

void BackgroundRunningTest::keepsEntriesOfOthers()
{
    // An entry the user or another tool made for the same application is left alone
    const QString path = m_dir.filePath(QStringLiteral("config/autostart/manual.desktop"));
    QString error;
    QVERIFY(Background::writeAutostartEntry(path, "[Desktop Entry]\nType=Application\nExec=app\n", &error));
    QVERIFY(!Background::isOwnAutostartEntry(path));
    QVERIFY(Background::removeAutostartEntry(path, &error));
    QVERIFY(QFile::exists(path));
    // So is one whose marker was turned off
    QVERIFY(Background::writeAutostartEntry(path, "[Desktop Entry]\nExec=app\nX-Plasma-Impresario-Autostart=false\n", &error));
    QVERIFY(!Background::isOwnAutostartEntry(path));
    QVERIFY(Background::removeAutostartEntry(path, &error));
    QVERIFY(QFile::exists(path));
}

void BackgroundRunningTest::portalOptions()
{
    const QStringList command{QStringLiteral("plasma-impresario"), QStringLiteral("--background")};
    const QVariantMap on = Background::portalOptions(true, command, QStringLiteral("Check mail"));
    QCOMPARE(on.value(QStringLiteral("reason")).toString(), QStringLiteral("Check mail"));
    QCOMPARE(on.value(QStringLiteral("autostart")).toBool(), true);
    QCOMPARE(on.value(QStringLiteral("commandline")).toStringList(), command);
    QCOMPARE(on.value(QStringLiteral("dbus-activatable")).toBool(), false);
    const QVariantMap off = Background::portalOptions(false, command, QStringLiteral("Check mail"));
    QCOMPARE(off.value(QStringLiteral("autostart")).toBool(), false);
    QVERIFY(!off.contains(QStringLiteral("commandline")));
}

QTEST_GUILESS_MAIN(BackgroundRunningTest)
