/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "test_Color.h"

#include <QColor>
#include <QTest>

#include "UiUtils/Color.h"

void ColorTest::luminanceSpansBlackToWhite()
{
    QCOMPARE(UiUtils::relativeLuminance(QColor(Qt::black)), 0.0);
    QCOMPARE(UiUtils::relativeLuminance(QColor(Qt::white)), 1.0);
    QVERIFY(UiUtils::relativeLuminance(QColor(0x80, 0x80, 0x80)) < 0.25);
    // Green weighs most, blue least
    QVERIFY(UiUtils::relativeLuminance(QColor(Qt::green)) > UiUtils::relativeLuminance(QColor(Qt::red)));
    QVERIFY(UiUtils::relativeLuminance(QColor(Qt::red)) > UiUtils::relativeLuminance(QColor(Qt::blue)));
}

void ColorTest::contrastMatchesWcag()
{
    QCOMPARE(UiUtils::contrastRatio(QColor(Qt::black), QColor(Qt::white)), 21.0);
    QCOMPARE(UiUtils::contrastRatio(QColor(Qt::white), QColor(Qt::black)), 21.0);
    QCOMPARE(UiUtils::contrastRatio(QColor(0x12, 0x34, 0x56), QColor(0x12, 0x34, 0x56)), 1.0);
    // #767676 is the lightest grey that passes AA on white; #777777 is not
    QVERIFY(UiUtils::contrastRatio(QColor(0x76, 0x76, 0x76), QColor(Qt::white)) >= 4.5);
    QVERIFY(UiUtils::contrastRatio(QColor(0x77, 0x77, 0x77), QColor(Qt::white)) < 4.5);
}

void ColorTest::readableTextPicksTheStrongerOfBlackAndWhite()
{
    QCOMPARE(UiUtils::readableTextColor(QColor(Qt::yellow)), QColor(Qt::black));
    QCOMPARE(UiUtils::readableTextColor(QColor(Qt::white)), QColor(Qt::black));
    QCOMPARE(UiUtils::readableTextColor(QColor(0x00, 0x00, 0x80)), QColor(Qt::white));
    QCOMPARE(UiUtils::readableTextColor(QColor(0x23, 0x26, 0x29)), QColor(Qt::white));
}

void ColorTest::ensureContrastKeepsSufficientColors()
{
    const QColor grey(0x76, 0x76, 0x76);
    QCOMPARE(UiUtils::ensureContrast(grey, QColor(Qt::white)), grey);
    QCOMPARE(UiUtils::ensureContrast(QColor(Qt::white), QColor(0x00, 0x00, 0x80)), QColor(Qt::white));
}

void ColorTest::ensureContrastKeepsTheHue()
{
    // Breeze's positive green is too light for text on white
    const QColor green(0x27, 0xae, 0x60);
    const QColor white(Qt::white);
    QVERIFY(UiUtils::contrastRatio(green, white) < 4.5);
    const QColor darker = UiUtils::ensureContrast(green, white);
    QVERIFY(UiUtils::contrastRatio(darker, white) >= 4.5);
    QVERIFY(qAbs(darker.hslHueF() - green.hslHueF()) < 0.02);
    QVERIFY(darker.lightnessF() < green.lightnessF());

    // ... and too dark on Breeze Dark's view background
    const QColor dark(0x1b, 0x1e, 0x20);
    const QColor red(0x99, 0x22, 0x22);
    const QColor lighter = UiUtils::ensureContrast(red, dark);
    QVERIFY(UiUtils::contrastRatio(lighter, dark) >= 4.5);
    QVERIFY(lighter.lightnessF() > red.lightnessF());
    QVERIFY(qAbs(lighter.hslHueF() - red.hslHueF()) < 0.02);
}

void ColorTest::ensureContrastAlwaysReachesTheMinimum()
{
    const QList<QColor> backgrounds = {Qt::white, Qt::black, Qt::yellow, QColor(0x80, 0x80, 0x80), QColor(0x3d, 0xae, 0xe9)};
    const QList<QColor> preferred = {Qt::white, Qt::black, Qt::red, QColor(0x80, 0x80, 0x80), QColor(0xf6, 0x74, 0x00)};
    for (const QColor &background : backgrounds) {
        for (const QColor &color : preferred) {
            const QColor result = UiUtils::ensureContrast(color, background);
            // Mid grey cannot reach 4.5 with both black and white, so the better of the two is the floor
            const qreal floor = qMin<qreal>(4.5, UiUtils::contrastRatio(UiUtils::readableTextColor(background), background));
            QVERIFY2(UiUtils::contrastRatio(result, background) >= floor,
                     qPrintable(color.name() + QLatin1String(" on ") + background.name() + QLatin1String(" -> ") + result.name()));
        }
    }
}

QTEST_GUILESS_MAIN(ColorTest)
