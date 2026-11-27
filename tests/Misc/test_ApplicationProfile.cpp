/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QtTest>

#include "Common/Application.h"

class ApplicationProfileTest : public QObject
{
    Q_OBJECT

private slots:
    void cleanup()
    {
        qunsetenv("PLASMA_IMPRESARIO_PROFILE");
        qunsetenv("TROJITA_PROFILE");
    }

    void defaultProfileIsEmpty()
    {
        cleanup();
        QVERIFY(Common::Application::profileName().isEmpty());
    }

    void chosenProfileReachesHelpers()
    {
        Common::Application::setProfileName(QStringLiteral("work"));
        QCOMPARE(qgetenv("PLASMA_IMPRESARIO_PROFILE"), QByteArray("work"));
        QCOMPARE(Common::Application::profileName(), QStringLiteral("work"));
        Common::Application::setProfileName(QString());
        QVERIFY(!qEnvironmentVariableIsSet("PLASMA_IMPRESARIO_PROFILE"));
        QVERIFY(Common::Application::profileName().isEmpty());
    }

    void legacyNameIsReadAndReplaced()
    {
        // A launcher from Trojitá's time sets the old name
        qputenv("TROJITA_PROFILE", "home");
        QCOMPARE(Common::Application::profileName(), QStringLiteral("home"));
        // The new name wins where both are set
        qputenv("PLASMA_IMPRESARIO_PROFILE", "work");
        QCOMPARE(Common::Application::profileName(), QStringLiteral("work"));
        // Choosing a profile leaves only the new name behind
        Common::Application::setProfileName(QStringLiteral("travel"));
        QVERIFY(!qEnvironmentVariableIsSet("TROJITA_PROFILE"));
        QCOMPARE(Common::Application::profileName(), QStringLiteral("travel"));
    }

    void profileNamesKeepUnicode()
    {
        Common::Application::setProfileName(QStringLiteral("Zürich"));
        QCOMPARE(Common::Application::profileName(), QStringLiteral("Zürich"));
    }
};

QTEST_GUILESS_MAIN(ApplicationProfileTest)

#include "test_ApplicationProfile.moc"
