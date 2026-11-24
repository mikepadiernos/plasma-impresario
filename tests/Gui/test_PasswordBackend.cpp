/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/SettingsNames.h"
#include "Plugins/PluginManager.h"

#include <QApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class PasswordBackendTest : public QObject
{
    Q_OBJECT
private slots:
    void selectsBackendForNewAndExistingProfiles_data()
    {
        QTest::addColumn<QString>("explicitBackend");
        QTest::addColumn<QString>("legacyKey");
        QTest::addColumn<QString>("expected");
        QTest::newRow("new profile") << QString() << QString() << QStringLiteral("qtkeychainpassword");
        QTest::newRow("legacy IMAP password") << QString() << QStringLiteral("imap.auth.pass") << QStringLiteral("cleartextpassword");
        QTest::newRow("legacy SMTP password") << QString() << QStringLiteral("msa.smtp.auth.pass") << QStringLiteral("cleartextpassword");
        QTest::newRow("explicit cleartext") << QStringLiteral("cleartextpassword") << QString() << QStringLiteral("cleartextpassword");
        QTest::newRow("explicit secure with legacy password") << QStringLiteral("qtkeychainpassword")
                                                              << QStringLiteral("imap.auth.pass") << QStringLiteral("qtkeychainpassword");
        QTest::newRow("explicit disabled") << QStringLiteral("none") << QString() << QStringLiteral("none");
    }

    void selectsBackendForNewAndExistingProfiles()
    {
        QFETCH(QString, explicitBackend);
        QFETCH(QString, legacyKey);
        QFETCH(QString, expected);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        if (!explicitBackend.isEmpty())
            settings.setValue(Common::SettingsNames::passwordPlugin, explicitBackend);
        if (!legacyKey.isEmpty())
            settings.setValue(legacyKey, QStringLiteral("synthetic-test-only"));
        Plugins::PluginManager manager(nullptr, &settings, Common::SettingsNames::addressbookPlugin,
                                       Common::SettingsNames::passwordPlugin, Common::SettingsNames::spellcheckerPlugin);
        QCOMPARE(manager.passwordPlugin(), expected);
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir home;
    if (!home.isValid())
        return 1;
    qputenv("HOME", home.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", home.filePath(QStringLiteral("config")).toUtf8());
    qputenv("XDG_DATA_HOME", home.filePath(QStringLiteral("data")).toUtf8());
    qputenv("XDG_CACHE_HOME", home.filePath(QStringLiteral("cache")).toUtf8());
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    PasswordBackendTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_PasswordBackend.moc"
