/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_AccountIdentities.h"
#include "Common/SettingsNames.h"
#include "Composer/AccountIdentities.h"
#include "Composer/SenderIdentitiesModel.h"
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

using namespace Composer;

namespace {

struct Fixture {
    QTemporaryDir directory;
    QSettings shared { directory.filePath(QStringLiteral("shared.ini")), QSettings::IniFormat };
    QSettings account { directory.filePath(QStringLiteral("account.ini")), QSettings::IniFormat };
};

void store(QSettings &settings, const QList<ItemSenderIdentity> &items)
{
    SenderIdentitiesModel model;
    for (const ItemSenderIdentity &item : items)
        model.appendIdentity(item);
    model.saveToSettings(settings);
}

QStringList addresses(QSettings &settings)
{
    SenderIdentitiesModel model;
    model.loadFromSettings(settings);
    QStringList result;
    for (int row = 0; row < model.rowCount(); ++row)
        result << model.index(row, SenderIdentitiesModel::COLUMN_EMAIL).data().toString();
    return result;
}

const ItemSenderIdentity work(QStringLiteral("Work"), QStringLiteral("work@example.test"), QString(), QString());
const ItemSenderIdentity home(QStringLiteral("Home"), QStringLiteral("home@example.test"), QString(), QString());

} // namespace

void AccountIdentitiesTest::accountsUseTheirOwnIdentitiesByDefault()
{
    Fixture f;
    QVERIFY(!AccountIdentities::usesSharedIdentities(f.account));
}

void AccountIdentitiesTest::newAccountStartsWithOwnEmptyIdentities()
{
    Fixture f;
    store(f.shared, { work });
    AccountIdentities::initializeNewAccount(f.account);
    QVERIFY(!AccountIdentities::usesSharedIdentities(f.account));
    QVERIFY(addresses(f.account).isEmpty());
    // A new account has a recorded choice, so it does not later inherit the
    // shared identities.
    QVERIFY(!AccountIdentities::migrateAccount(f.shared, f.account));
    QVERIFY(addresses(f.account).isEmpty());
}

void AccountIdentitiesTest::sharedIdentitiesCanBeChosen()
{
    Fixture f;
    AccountIdentities::setUsesSharedIdentities(f.account, true);
    QVERIFY(AccountIdentities::usesSharedIdentities(f.account));
    AccountIdentities::setUsesSharedIdentities(f.account, false);
    QVERIFY(!AccountIdentities::usesSharedIdentities(f.account));
}

void AccountIdentitiesTest::migrationCopiesSharedIdentities()
{
    Fixture f;
    store(f.shared, { work, home });
    QVERIFY(AccountIdentities::migrateAccount(f.shared, f.account));
    QVERIFY(!AccountIdentities::usesSharedIdentities(f.account));
    QCOMPARE(addresses(f.account),
        (QStringList { QStringLiteral("work@example.test"), QStringLiteral("home@example.test") }));
}

void AccountIdentitiesTest::migrationKeepsExistingOwnIdentities()
{
    Fixture f;
    store(f.shared, { work });
    store(f.account, { home });
    QVERIFY(AccountIdentities::migrateAccount(f.shared, f.account));
    QCOMPARE(addresses(f.account), QStringList { QStringLiteral("home@example.test") });
}

void AccountIdentitiesTest::migrationRunsOnlyOnce()
{
    Fixture f;
    store(f.shared, { work });
    QVERIFY(AccountIdentities::migrateAccount(f.shared, f.account));
    store(f.account, { });
    store(f.shared, { home });
    QVERIFY(!AccountIdentities::migrateAccount(f.shared, f.account));
    QVERIFY(addresses(f.account).isEmpty());

    Fixture shared;
    AccountIdentities::setUsesSharedIdentities(shared.account, true);
    QVERIFY(!AccountIdentities::migrateAccount(shared.shared, shared.account));
    QVERIFY(AccountIdentities::usesSharedIdentities(shared.account));
}

void AccountIdentitiesTest::migrationLeavesSharedIdentitiesUnchanged()
{
    Fixture f;
    store(f.shared, { work });
    AccountIdentities::migrateAccount(f.shared, f.account);
    store(f.account, { home });
    QCOMPARE(addresses(f.shared), QStringList { QStringLiteral("work@example.test") });
}

QTEST_GUILESS_MAIN(AccountIdentitiesTest)
