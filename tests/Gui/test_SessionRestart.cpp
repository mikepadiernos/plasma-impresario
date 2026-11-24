/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Gui/DensityMenu.h"
#include "Gui/MailBoxTreeView.h"
#include "Gui/Window.h"
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "configure.cmake.h"
#ifdef TROJITA_HAVE_WEBENGINE
#include "Gui/WebEngineMessageProfile.h"
#endif
#include "static_plugins.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

class SessionRestartTest : public QObject
{
    Q_OBJECT
private slots:
    void persistsAfterExit_data()
    {
        QTest::addColumn<QString>("exitMode");
        QTest::newRow("close") << QStringLiteral("close");
        QTest::newRow("quit") << QStringLiteral("quit");
        QTest::newRow("hidden-quit") << QStringLiteral("hidden-quit");
    }

    void persistsAfterExit()
    {
        QFETCH(QString, exitMode);
        QTemporaryDir home(QDir::tempPath() + QStringLiteral("/plasma-impresario-restart-XXXXXX"));
        QVERIFY(home.isValid());
        for (const QString &phase : {exitMode, QStringLiteral("read")}) {
            QProcess child;
            auto env = QProcessEnvironment::systemEnvironment();
            env.insert(QStringLiteral("HOME"), home.path());
            env.insert(QStringLiteral("XDG_CONFIG_HOME"), home.filePath(QStringLiteral("config")));
            env.insert(QStringLiteral("XDG_DATA_HOME"), home.filePath(QStringLiteral("data")));
            env.insert(QStringLiteral("XDG_CACHE_HOME"), home.filePath(QStringLiteral("cache")));
            env.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("unix:path=/nonexistent/plasma-restart-test"));
            env.insert(QStringLiteral("PLASMA_TEST_RESTART_PHASE"), phase);
            env.remove(QStringLiteral("TROJITA_PROFILE"));
            child.setProcessEnvironment(env);
            child.start(QCoreApplication::applicationFilePath());
            QVERIFY(child.waitForStarted());
            QVERIFY(child.waitForFinished(30000));
            const QString output = QString::fromUtf8(child.readAllStandardOutput() + child.readAllStandardError());
            const QString failure = QStringLiteral("Phase %1 exited with code %2:\n%3").arg(phase).arg(child.exitCode()).arg(output);
            QVERIFY2(child.exitStatus() == QProcess::NormalExit && child.exitCode() == 0, qPrintable(failure));
        }
    }
};

int runChild(QApplication &app, const QString &phase)
{
    QSettings settings(QDir::home().filePath(QStringLiteral("account.ini")), QSettings::IniFormat);
    settings.setValue(Common::SettingsNames::imapMethodKey, Common::SettingsNames::methodSSL);
    settings.setValue(Common::SettingsNames::imapStartMode, Common::SettingsNames::netOffline);
    settings.setValue(Common::SettingsNames::guiShowSystray, false);
    settings.setValue(Common::SettingsNames::msaMethodKey, Common::SettingsNames::methodSENDMAIL);
    Gui::MainWindow window(&settings);
    window.resize(1000, 700);
    window.show();
    QCoreApplication::processEvents();
    auto *mailboxes = window.findChild<Gui::MailBoxTreeView *>();
    auto *menu = window.findChild<Gui::DensityMenu *>();
    if (!mailboxes || !menu)
        return 1;
    if (phase != QLatin1String("read")) {
        if (!QMetaObject::invokeMethod(&window, "slotLayoutWide"))
            return 2;
        auto *right = window.findChild<QAction *>(QStringLiteral("mailboxSidebarRight"));
        if (!right)
            return 3;
        right->trigger();
        menu->actions().at(1)->trigger();
        QStandardItemModel messages(1, 1);
        const QModelIndex message = messages.index(0, 0);
        messages.setData(message, QStringLiteral("INBOX"), Imap::Mailbox::RoleMailboxName);
        messages.setData(message, 123u, Imap::Mailbox::RoleMailboxUidValidity);
        messages.setData(message, 42u, Imap::Mailbox::RoleMessageUid);
        if (!QMetaObject::invokeMethod(&window, "saveMessageSelection", Q_ARG(QModelIndex, message)))
            return 9;
        auto *messageModel = qobject_cast<Imap::Mailbox::MsgListModel *>(window.imapAccess()->msgListModel());
        if (!messageModel)
            return 10;
        messageModel->mailboxChanged(message);
        auto *splitter = qobject_cast<QSplitter *>(mailboxes->parentWidget());
        if (!splitter || splitter->count() != 3)
            return 4;
        splitter->setSizes({450, 250, 150});
        splitter->splitterMoved(450, 1);
        QSettings expected(QDir::home().filePath(QStringLiteral("expected.ini")), QSettings::IniFormat);
        const auto sizes = splitter->sizes();
        for (int i = 0; i < sizes.size(); ++i)
            expected.setValue(QString::number(i), sizes.at(i));
        expected.sync();
        if (expected.status() != QSettings::NoError)
            return 5;
        QTimer::singleShot(0, &app, [&window, &app, phase] {
            if (phase == QLatin1String("close"))
                window.close();
            else if (phase == QLatin1String("hidden-quit"))
                window.hide();
            app.quit();
        });
        return app.exec();
    }

    auto *splitter = qobject_cast<QSplitter *>(mailboxes->parentWidget());
    if (!splitter || splitter->count() != 3 || splitter->indexOf(mailboxes) != 2 || !menu->actions().at(1)->isChecked())
        return 6;
    KConfigGroup selection(KSharedConfig::openStateConfig(), QStringLiteral("MailboxSelection"));
    const KConfigGroup account = selection.group(window.imapAccess()->credentialAccountId());
    if (account.readEntry("Mailbox", QString()) != QLatin1String("INBOX") ||
        account.readEntry("MessageMailbox", QString()) != QLatin1String("INBOX") ||
        account.readEntry("MessageUidValidity", 0u) != 123u || account.readEntry("MessageUid", 0u) != 42u)
        return 11;
    QSettings expected(QDir::home().filePath(QStringLiteral("expected.ini")), QSettings::IniFormat);
    for (int i = 0; i < 3; ++i) {
        bool ok = false;
        const int size = expected.value(QString::number(i)).toInt(&ok);
        if (!ok)
            return 7;
        if (qAbs(splitter->sizes().at(i) - size) > 2)
            return 8;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const QString phase = qEnvironmentVariable("PLASMA_TEST_RESTART_PHASE");
    if (phase.isEmpty()) {
        QCoreApplication app(argc, argv);
        SessionRestartTest test;
        return QTest::qExec(&test, argc, argv);
    }
    const QString home = QFileInfo(QDir::homePath()).canonicalFilePath();
    const QString prefix = QDir::tempPath() + QStringLiteral("/plasma-impresario-restart-");
    if (!home.startsWith(prefix) ||
        qEnvironmentVariable("XDG_CONFIG_HOME") != home + QStringLiteral("/config") ||
        qEnvironmentVariable("XDG_DATA_HOME") != home + QStringLiteral("/data") ||
        qEnvironmentVariable("XDG_CACHE_HOME") != home + QStringLiteral("/cache"))
        return 1;
    QStandardPaths::setTestModeEnabled(true);
#ifdef TROJITA_HAVE_WEBENGINE
    Gui::WebEngineMessageProfile::registerScheme();
#endif
    Common::registerMetaTypes();
    QApplication app(argc, argv);
    for (auto location : {QStandardPaths::GenericConfigLocation, QStandardPaths::AppDataLocation, QStandardPaths::CacheLocation}) {
        if (!QStandardPaths::writableLocation(location).startsWith(home + QLatin1Char('/')))
            return 1;
    }
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(license);
    KLocalizedString::setApplicationDomain("plasma-impresario");
    if (!Imap::Mailbox::SystemNetworkWatcher::init())
        return 1;
    return runChild(app, phase);
}

#include "test_SessionRestart.moc"
