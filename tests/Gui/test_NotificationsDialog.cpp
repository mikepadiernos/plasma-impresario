/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTest>

#include <KLocalizedString>

#include "Gui/NotificationsDialog.h"

using Gui::NotificationsDialog;
using UiUtils::NewMail::Mode;
using UiUtils::NewMail::Settings;

class NotificationsDialogTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void unchangedKeepsSettings();
    void editsPerAccount();
    void general();
};

namespace {

const NotificationsDialog::Accounts accounts{{QStringLiteral("work"), QStringLiteral("Work")},
                                             {QStringLiteral("home"), QStringLiteral("Home")}};

template<typename T>
T *child(QWidget *dialog, const char *name)
{
    return dialog->findChild<T *>(QLatin1String(name));
}

}

void NotificationsDialogTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    KLocalizedString::setApplicationDomain("plasma-impresario");
}

void NotificationsDialogTest::unchangedKeepsSettings()
{
    Settings settings;
    settings.accountModes.insert(QStringLiteral("home"), Mode::Summary);
    settings.setAnnounces(QStringLiteral("work"), QStringLiteral("Lists/KDE"), true);
    settings.setAnnounces(QStringLiteral("home"), QStringLiteral("INBOX"), false);
    NotificationsDialog dialog(accounts, settings);
    dialog.selectAccount(QStringLiteral("home"));
    dialog.selectAccount(QStringLiteral("work"));
    QCOMPARE(dialog.settings(), settings);
}

void NotificationsDialogTest::editsPerAccount()
{
    NotificationsDialog dialog(accounts, Settings{});
    auto *mode = child<QComboBox>(&dialog, "notifyMode");
    auto *inbox = child<QCheckBox>(&dialog, "notifyInbox");
    auto *folders = child<QPlainTextEdit>(&dialog, "notifyFolders");
    QVERIFY(inbox->isChecked());
    QVERIFY(folders->toPlainText().isEmpty());

    folders->setPlainText(QStringLiteral("Lists/KDE\n\n  Team  \nINBOX\nTeam"));
    dialog.selectAccount(QStringLiteral("home"));
    QVERIFY(folders->toPlainText().isEmpty());
    mode->setCurrentIndex(mode->findData(static_cast<int>(Mode::Off)));
    QVERIFY(!inbox->isEnabled());
    QVERIFY(!folders->isEnabled());

    dialog.selectAccount(QStringLiteral("work"));
    QCOMPARE(folders->toPlainText(), QStringLiteral("Lists/KDE\nTeam"));
    inbox->setChecked(false);
    folders->setPlainText(QStringLiteral("Team"));

    const Settings result = dialog.settings();
    QCOMPARE(result.mode(QStringLiteral("home")), Mode::Off);
    QCOMPARE(result.mode(QStringLiteral("work")), Mode::Each);
    QCOMPARE(result.extraFolders(QStringLiteral("work")), QStringList{QStringLiteral("Team")});
    QVERIFY(!result.announces(QStringLiteral("work"), QStringLiteral("INBOX")));
    QVERIFY(!result.announces(QStringLiteral("work"), QStringLiteral("Lists/KDE")));
    QVERIFY(result.announces(QStringLiteral("home"), QStringLiteral("INBOX")));
}

void NotificationsDialogTest::general()
{
    NotificationsDialog dialog(accounts, Settings{});
    child<QSpinBox>(&dialog, "notifySummaryMinutes")->setValue(30);
    child<QCheckBox>(&dialog, "notifyQuietWhileActive")->setChecked(false);
    child<QCheckBox>(&dialog, "notifyShowContent")->setChecked(false);
    const Settings result = dialog.settings();
    QCOMPARE(result.summaryMinutes, 30);
    QVERIFY(!result.quietWhileActive);
    QVERIFY(!result.showContent);
}

QTEST_MAIN(NotificationsDialogTest)
#include "test_NotificationsDialog.moc"
