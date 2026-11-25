/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_ACCOUNTCACHE_H
#define TEST_ACCOUNTCACHE_H

#include <QObject>

/** @short Verify that removing an account deletes only its own offline cache */
class AccountCacheTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void removesTheAccountDirectory();
    void keepsTheSharedCacheOfTheMigratedAccount();
    void refusesDirectoriesOutsideTheCache();
};

#endif
