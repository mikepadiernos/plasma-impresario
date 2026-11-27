/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_DESKTOPFEEDBACK_H
#define TEST_DESKTOPFEEDBACK_H

#include <QObject>
#include <QTemporaryDir>

/** @short Verify the setting, the total and the task manager signal for the unread count */
class DesktopFeedbackTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void shownByDefault();
    void totalSkipsUnknownCounts();
    void propertiesHideNothingUnread();
    void updateNamesTheDesktopEntry();
    void badgeSendsOnlyChanges();
    void signalReachesAListener();

private:
    QTemporaryDir m_dir;
};

#endif
