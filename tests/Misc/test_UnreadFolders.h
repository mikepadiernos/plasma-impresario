/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef TEST_UNREADFOLDERS_H
#define TEST_UNREADFOLDERS_H

#include <QObject>

/** @short Verify which folder "Go to Next Unread Folder" picks */
class UnreadFoldersTest : public QObject
{
    Q_OBJECT

private slots:
    void walksTreeOrderAndWraps();
    void skipsUnknownAndUnselectable();
    void doesNotDescendIntoUnlistedBranches();
    void noOtherUnreadFolder();
};

#endif
