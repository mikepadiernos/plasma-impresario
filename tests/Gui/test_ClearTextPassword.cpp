/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Plugins/ClearTextPassword/ClearTextPassword.h"

#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

void run(PasswordJob *job)
{
    QSignalSpy finished(job, &PluginJob::destroyed);
    job->setAutoDelete(true);
    job->start();
    QVERIFY(finished.wait(1000));
}

QString request(ClearTextPassword &plugin, const QString &accountId, const QString &type)
{
    PasswordJob *job = plugin.requestPassword(accountId, type);
    QString password;
    bool missing = false;
    QObject::connect(job, &PasswordJob::passwordAvailable, [&](const QString &value) { password = value; });
    QObject::connect(job, &PasswordJob::error, [&](PasswordJob::Error error, const QString &) {
        missing = error == PasswordJob::NoSuchPassword;
    });
    run(job);
    return missing ? QStringLiteral("<missing>") : password;
}

}

class ClearTextPasswordTest : public QObject
{
    Q_OBJECT
private slots:
    void accountsDoNotShareCleartextPasswords()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        ClearTextPassword plugin(nullptr, &settings);
        run(plugin.storePassword(QStringLiteral("account-1"), QStringLiteral("imap"), QStringLiteral("first")));
        run(plugin.storePassword(QStringLiteral("account-2"), QStringLiteral("imap"), QStringLiteral("second")));
        run(plugin.storePassword(QStringLiteral("account-2"), QStringLiteral("smtp"), QStringLiteral("second-smtp")));
        QCOMPARE(request(plugin, QStringLiteral("account-1"), QStringLiteral("imap")), QStringLiteral("first"));
        QCOMPARE(request(plugin, QStringLiteral("account-2"), QStringLiteral("imap")), QStringLiteral("second"));
        QCOMPARE(request(plugin, QStringLiteral("account-1"), QStringLiteral("smtp")), QStringLiteral("<missing>"));
        QCOMPARE(request(plugin, QStringLiteral("account-2"), QStringLiteral("smtp")), QStringLiteral("second-smtp"));
    }

    void deleteRemovesOnlyThatAccount()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        ClearTextPassword plugin(nullptr, &settings);
        run(plugin.storePassword(QStringLiteral("account-1"), QStringLiteral("imap"), QStringLiteral("first")));
        run(plugin.storePassword(QStringLiteral("account-2"), QStringLiteral("imap"), QStringLiteral("second")));
        run(plugin.deletePassword(QStringLiteral("account-1"), QStringLiteral("imap")));
        QCOMPARE(request(plugin, QStringLiteral("account-1"), QStringLiteral("imap")), QStringLiteral("<missing>"));
        QCOMPARE(request(plugin, QStringLiteral("account-2"), QStringLiteral("imap")), QStringLiteral("second"));
    }

    void legacyProfilesKeepTheOriginalKeys_data()
    {
        QTest::addColumn<QString>("accountId");
        QTest::newRow("default profile") << QStringLiteral("account-0");
        QTest::newRow("named profile") << QStringLiteral("work-profile");
    }

    void legacyProfilesKeepTheOriginalKeys()
    {
        QFETCH(QString, accountId);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        settings.setValue(QStringLiteral("imap.auth.pass"), QStringLiteral("saved-before"));
        ClearTextPassword plugin(nullptr, &settings);
        QCOMPARE(request(plugin, accountId, QStringLiteral("imap")), QStringLiteral("saved-before"));
        run(plugin.storePassword(accountId, QStringLiteral("smtp"), QStringLiteral("smtp-secret")));
        QCOMPARE(settings.value(QStringLiteral("msa.smtp.auth.pass")).toString(), QStringLiteral("smtp-secret"));
    }
};

QTEST_GUILESS_MAIN(ClearTextPasswordTest)

#include "test_ClearTextPassword.moc"
