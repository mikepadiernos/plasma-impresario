/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_AccountIdentities.h"
#include "Common/SettingsNames.h"
#include "Composer/AccountIdentities.h"
#include "Composer/SenderIdentitiesModel.h"
#include "Imap/Parser/MailAddress.h"
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
    QCOMPARE(AccountIdentities::source(f.account), AccountIdentities::Source::Own);
}

void AccountIdentitiesTest::newAccountStartsWithOwnEmptyIdentities()
{
    Fixture f;
    store(f.shared, { work });
    AccountIdentities::initializeNewAccount(f.account);
    QCOMPARE(AccountIdentities::source(f.account), AccountIdentities::Source::Own);
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
    QCOMPARE(AccountIdentities::source(f.account), AccountIdentities::Source::Own);
}

void AccountIdentitiesTest::noIdentityCanBeChosen()
{
    Fixture f;
    store(f.account, { home });
    AccountIdentities::setSource(f.account, AccountIdentities::Source::None);
    QCOMPARE(AccountIdentities::source(f.account), AccountIdentities::Source::None);
    QVERIFY(!AccountIdentities::usesSharedIdentities(f.account));
    // The account's own identities are kept for when it switches back.
    QCOMPARE(addresses(f.account), QStringList { QStringLiteral("home@example.test") });
    // Choosing no identity is a recorded choice, so migration leaves it alone.
    QVERIFY(!AccountIdentities::migrateAccount(f.shared, f.account));
    QCOMPARE(AccountIdentities::source(f.account), AccountIdentities::Source::None);

    AccountIdentities::setSource(f.account, AccountIdentities::Source::Shared);
    QCOMPARE(AccountIdentities::source(f.account), AccountIdentities::Source::Shared);
    AccountIdentities::setSource(f.account, AccountIdentities::Source::Own);
    QCOMPARE(AccountIdentities::source(f.account), AccountIdentities::Source::Own);
    QCOMPARE(addresses(f.account), QStringList { QStringLiteral("home@example.test") });
}

void AccountIdentitiesTest::earlierSharedChoiceIsKept()
{
    Fixture f;
    // Settings written before "no identity" existed have only the shared flag.
    f.account.setValue(Common::SettingsNames::composerUseSharedIdentitiesKey, true);
    QCOMPARE(AccountIdentities::source(f.account), AccountIdentities::Source::Shared);
    f.account.setValue(Common::SettingsNames::composerUseSharedIdentitiesKey, false);
    QCOMPARE(AccountIdentities::source(f.account), AccountIdentities::Source::Own);
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

void AccountIdentitiesTest::loginAddressIsAPlainEmailAddress()
{
    QFETCH(QVariant, user);
    QFETCH(QString, expected);
    Fixture f;
    if (user.isValid())
        f.account.setValue(Common::SettingsNames::imapUserKey, user);
    const QList<Imap::Message::MailAddress> result = AccountIdentities::loginAddresses(f.account);
    if (expected.isEmpty()) {
        QVERIFY(result.isEmpty());
    } else {
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.first().mailbox + QLatin1Char('@') + result.first().host, expected);
        QVERIFY(result.first().name.isEmpty());
    }
}

void AccountIdentitiesTest::loginAddressIsAPlainEmailAddress_data()
{
    QTest::addColumn<QVariant>("user");
    QTest::addColumn<QString>("expected");
    QTest::newRow("unset") << QVariant() << QString();
    QTest::newRow("empty") << QVariant(QString()) << QString();
    QTest::newRow("email") << QVariant(QStringLiteral("alice@example.test")) << QStringLiteral("alice@example.test");
    QTest::newRow("padded") << QVariant(QStringLiteral(" alice@example.test ")) << QStringLiteral("alice@example.test");
    QTest::newRow("local name") << QVariant(QStringLiteral("alice")) << QString();
    QTest::newRow("no local part") << QVariant(QStringLiteral("@example.test")) << QString();
    QTest::newRow("no domain") << QVariant(QStringLiteral("alice@")) << QString();
    QTest::newRow("two at signs") << QVariant(QStringLiteral("alice@corp@example.test")) << QString();
    QTest::newRow("space") << QVariant(QStringLiteral("alice smith@example.test")) << QString();
}

QTEST_GUILESS_MAIN(AccountIdentitiesTest)
