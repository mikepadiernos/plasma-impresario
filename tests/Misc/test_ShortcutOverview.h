/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef TEST_SHORTCUTOVERVIEW_H
#define TEST_SHORTCUTOVERVIEW_H

#include <QObject>

/** @short Verify the grouping and searching behind the keyboard shortcut overview */
class ShortcutOverviewTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void plainText_data();
    void plainText();
    void shortcutText();
    void groupsFollowTheAreaOrder();
    void filterMatchesTextShortcutAndArea();
    void duplicateShortcuts();
};

#endif
