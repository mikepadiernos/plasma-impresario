/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include "test_AccountRegistry.h"
#include "Common/AccountRegistry.h"

using Common::AccountRegistry;

namespace {

struct Fixture {
    QTemporaryDir directory;
    QSettings index{directory.filePath(QStringLiteral("index.ini")), QSettings::IniFormat};

    AccountRegistry::SettingsFactory factory()
    {
        return [this](const QString &id) {
            return std::make_unique<QSettings>(directory.filePath(id + QStringLiteral(".ini")), QSettings::IniFormat);
        };
    }
};

}

void AccountRegistryTest::startsEmpty()
{
    Fixture f;
    AccountRegistry registry(&f.index, f.factory());
    QVERIFY(registry.accountIds().isEmpty());
    QVERIFY(registry.activeAccount().isEmpty());
}

void AccountRegistryTest::addsAccountsInOrderWithUniqueIds()
{
    Fixture f;
    AccountRegistry registry(&f.index, f.factory());
    const QString work = registry.addAccount(QStringLiteral("Work"));
    const QString home = registry.addAccount(QStringLiteral("Home"));
    QVERIFY(!work.isEmpty());
    QVERIFY(work != home);
    QCOMPARE(registry.accountIds(), (QStringList{work, home}));
    QCOMPARE(registry.displayName(work), QStringLiteral("Work"));
    QCOMPARE(registry.displayName(home), QStringLiteral("Home"));
}

void AccountRegistryTest::firstAccountBecomesActive()
{
    Fixture f;
    AccountRegistry registry(&f.index, f.factory());
    const QString first = registry.addAccount(QStringLiteral("First"));
    registry.addAccount(QStringLiteral("Second"));
    QCOMPARE(registry.activeAccount(), first);
}

void AccountRegistryTest::neverReusesRemovedIds()
{
    Fixture f;
    AccountRegistry registry(&f.index, f.factory());
    const QString first = registry.addAccount(QStringLiteral("First"));
    registry.removeAccount(first);
    QVERIFY(registry.addAccount(QStringLiteral("Second")) != first);
}

void AccountRegistryTest::settingsAreIsolatedPerAccount()
{
    Fixture f;
    AccountRegistry registry(&f.index, f.factory());
    const QString a = registry.addAccount(QStringLiteral("A"));
    const QString b = registry.addAccount(QStringLiteral("B"));
    registry.settings(a)->setValue(QStringLiteral("imap.host"), QStringLiteral("a.example.test"));
    registry.settings(b)->setValue(QStringLiteral("imap.host"), QStringLiteral("b.example.test"));
    QCOMPARE(registry.settings(a)->value(QStringLiteral("imap.host")).toString(), QStringLiteral("a.example.test"));
    QCOMPARE(registry.settings(b)->value(QStringLiteral("imap.host")).toString(), QStringLiteral("b.example.test"));
    QCOMPARE(registry.settings(a), registry.settings(a));
}

void AccountRegistryTest::removeDeletesStoredConfiguration()
{
    Fixture f;
    AccountRegistry registry(&f.index, f.factory());
    const QString id = registry.addAccount(QStringLiteral("Gone"));
    registry.settings(id)->setValue(QStringLiteral("imap.host"), QStringLiteral("gone.example.test"));
    registry.settings(id)->sync();
    const QString path = f.directory.filePath(id + QStringLiteral(".ini"));
    QVERIFY(QFile::exists(path));
    registry.removeAccount(id);
    QVERIFY(!QFile::exists(path));
    QVERIFY(registry.accountIds().isEmpty());
}

void AccountRegistryTest::removingActiveAccountActivatesFirstRemaining()
{
    Fixture f;
    AccountRegistry registry(&f.index, f.factory());
    const QString a = registry.addAccount(QStringLiteral("A"));
    const QString b = registry.addAccount(QStringLiteral("B"));
    const QString c = registry.addAccount(QStringLiteral("C"));
    registry.setActiveAccount(b);
    registry.removeAccount(b);
    QCOMPARE(registry.activeAccount(), a);
    registry.removeAccount(c);
    QCOMPARE(registry.activeAccount(), a);
}

void AccountRegistryTest::removingLastAccountClearsActive()
{
    Fixture f;
    AccountRegistry registry(&f.index, f.factory());
    registry.removeAccount(registry.addAccount(QStringLiteral("Only")));
    QVERIFY(registry.activeAccount().isEmpty());
}

void AccountRegistryTest::persistsAccountsNamesAndActiveAccount()
{
    Fixture f;
    QString a, b;
    {
        AccountRegistry registry(&f.index, f.factory());
        a = registry.addAccount(QStringLiteral("A"));
        b = registry.addAccount(QStringLiteral("B"));
        registry.setDisplayName(b, QStringLiteral("Renamed"));
        registry.setActiveAccount(b);
        registry.settings(a)->setValue(QStringLiteral("imap.host"), QStringLiteral("a.example.test"));
    }
    AccountRegistry reloaded(&f.index, f.factory());
    QCOMPARE(reloaded.accountIds(), (QStringList{a, b}));
    QCOMPARE(reloaded.displayName(b), QStringLiteral("Renamed"));
    QCOMPARE(reloaded.activeAccount(), b);
    QCOMPARE(reloaded.settings(a)->value(QStringLiteral("imap.host")).toString(), QStringLiteral("a.example.test"));
    QVERIFY(reloaded.addAccount(QStringLiteral("C")) != a);
}

void AccountRegistryTest::emitsLifecycleSignals()
{
    Fixture f;
    AccountRegistry registry(&f.index, f.factory());
    QSignalSpy added(&registry, &AccountRegistry::accountAdded);
    QSignalSpy removed(&registry, &AccountRegistry::accountRemoved);
    QSignalSpy active(&registry, &AccountRegistry::activeAccountChanged);
    const QString a = registry.addAccount(QStringLiteral("A"));
    const QString b = registry.addAccount(QStringLiteral("B"));
    QCOMPARE(added.count(), 2);
    QCOMPARE(active.count(), 1);
    registry.setActiveAccount(b);
    registry.setActiveAccount(b);
    QCOMPARE(active.count(), 2);
    registry.removeAccount(b);
    QCOMPARE(removed.count(), 1);
    QCOMPARE(removed.at(0).at(0).toString(), b);
    QCOMPARE(active.count(), 3);
    QCOMPARE(active.at(2).at(0).toString(), a);
}

void AccountRegistryTest::unknownIdsAreHandled()
{
    Fixture f;
    AccountRegistry registry(&f.index, f.factory());
    const QString a = registry.addAccount(QStringLiteral("A"));
    QVERIFY(registry.displayName(QStringLiteral("missing")).isEmpty());
    QVERIFY(registry.settings(QStringLiteral("missing")) == nullptr);
    registry.setActiveAccount(QStringLiteral("missing"));
    registry.removeAccount(QStringLiteral("missing"));
    QCOMPARE(registry.activeAccount(), a);
    QCOMPARE(registry.accountIds(), QStringList{a});
}

QTEST_GUILESS_MAIN(AccountRegistryTest)
