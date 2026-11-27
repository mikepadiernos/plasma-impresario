/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_COLOR_H
#define TEST_COLOR_H

#include <QObject>

/** @short Verify the WCAG contrast helpers used for custom colors */
class ColorTest : public QObject
{
    Q_OBJECT

private slots:
    void luminanceSpansBlackToWhite();
    void contrastMatchesWcag();
    void readableTextPicksTheStrongerOfBlackAndWhite();
    void ensureContrastKeepsSufficientColors();
    void ensureContrastKeepsTheHue();
    void ensureContrastAlwaysReachesTheMinimum();
};

#endif
