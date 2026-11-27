/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_BACKGROUNDRUNNING_H
#define TEST_BACKGROUNDRUNNING_H

#include <QObject>
#include <QTemporaryDir>

/** @short Verify the settings, autostart entry and portal request for running in the background */
class BackgroundRunningTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void offByDefault();
    void sandboxDetection();
    void commandLineCarriesTheProfile();
    void quotesExecArguments_data();
    void quotesExecArguments();
    void autostartEntryContents();
    void autostartPathIsInTheUserFolder();
    void writesAndRemovesTheEntry();
    void keepsEntriesOfOthers();
    void portalOptions();

private:
    QTemporaryDir m_dir;
};

#endif
