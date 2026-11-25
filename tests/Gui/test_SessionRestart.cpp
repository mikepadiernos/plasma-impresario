/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Gui/AccountSidebar.h"
#include "Gui/DensityMenu.h"
#include "Gui/MailBoxTreeView.h"
#include "Gui/OnePanelAtTimeWidget.h"
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
#include <QScreen>
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
        QTest::addColumn<QString>("layout");
        QTest::newRow("wide-close") << QStringLiteral("close") << QStringLiteral("wide");
        QTest::newRow("wide-quit") << QStringLiteral("quit") << QStringLiteral("wide");
        QTest::newRow("wide-hidden-quit") << QStringLiteral("hidden-quit") << QStringLiteral("wide");
        QTest::newRow("compact-close") << QStringLiteral("close") << QStringLiteral("compact");
        QTest::newRow("compact-hidden-quit") << QStringLiteral("hidden-quit") << QStringLiteral("compact");
        QTest::newRow("one-close") << QStringLiteral("close") << QStringLiteral("one");
        QTest::newRow("one-hidden-quit") << QStringLiteral("hidden-quit") << QStringLiteral("one");
    }

    void persistsAfterExit()
    {
        QFETCH(QString, exitMode);
        QFETCH(QString, layout);
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
            env.insert(QStringLiteral("PLASMA_TEST_RESTART_LAYOUT"), layout);
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

void saveSplitterSizes(QSettings &settings, QSplitter *splitter, const QString &prefix)
{
    const auto sizes = splitter->sizes();
    for (int i = 0; i < sizes.size(); ++i)
        settings.setValue(prefix + QString::number(i), sizes.at(i));
}

bool matchesSplitterSizes(QSettings &settings, QSplitter *splitter, const QString &prefix)
{
    QList<int> savedSizes;
    int savedTotal = 0;
    int restoredTotal = 0;
    const auto restoredSizes = splitter->sizes();
    for (int i = 0; i < restoredSizes.size(); ++i) {
        bool ok = false;
        const int size = settings.value(prefix + QString::number(i)).toInt(&ok);
        if (!ok)
            return false;
        savedSizes.append(size);
        savedTotal += size;
        restoredTotal += restoredSizes.at(i);
    }
    if (!savedTotal || !restoredTotal)
        return false;
    for (int i = 0; i < restoredSizes.size(); ++i) {
        const qint64 delta = qAbs(qint64(restoredSizes.at(i)) * savedTotal - qint64(savedSizes.at(i)) * restoredTotal);
        if (delta > qint64(savedTotal) * restoredTotal / 100) {
            fprintf(stderr, "%s splitter section %d saved %d/%d restored %d/%d\n",
                    qPrintable(prefix), i, savedSizes.at(i), savedTotal, restoredSizes.at(i), restoredTotal);
            return false;
        }
    }
    return true;
}

int runChild(QApplication &app, const QString &phase)
{
    const QString layout = qEnvironmentVariable("PLASMA_TEST_RESTART_LAYOUT");
    if (layout != QLatin1String("wide") && layout != QLatin1String("compact") && layout != QLatin1String("one"))
        return 12;
    QSettings settings(QDir::home().filePath(QStringLiteral("account.ini")), QSettings::IniFormat);
    settings.setValue(Common::SettingsNames::imapMethodKey, Common::SettingsNames::methodSSL);
    settings.setValue(Common::SettingsNames::imapStartMode, Common::SettingsNames::netOffline);
    settings.setValue(Common::SettingsNames::guiShowSystray, false);
    settings.setValue(Common::SettingsNames::msaMethodKey, Common::SettingsNames::methodSENDMAIL);
    Gui::MainWindow window(&settings);
    if (phase != QLatin1String("read"))
        window.resize(window.screen()->availableGeometry().size() - QSize(40, 40));
    window.show();
    QCoreApplication::processEvents();
    auto *mailboxes = window.findChild<Gui::MailBoxTreeView *>();
    auto *menu = window.findChild<Gui::DensityMenu *>();
    // The folder tree is placed in the shell inside the account sidebar
    auto *sidebar = window.findChild<Gui::AccountSidebar *>();
    if (!mailboxes || !menu || !sidebar || !sidebar->isAncestorOf(mailboxes))
        return 1;
    if (phase != QLatin1String("read")) {
        const char *layoutSlot = layout == QLatin1String("wide") ? "slotLayoutWide" :
                                 layout == QLatin1String("compact") ? "slotLayoutCompact" : "slotLayoutOneAtTime";
        if (!QMetaObject::invokeMethod(&window, layoutSlot))
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
        QSettings expected(QDir::home().filePath(QStringLiteral("expected.ini")), QSettings::IniFormat);
        if (layout != QLatin1String("one")) {
            auto *row = window.findChild<QSplitter *>(QStringLiteral("shellRow"));
            auto *mainColumn = window.findChild<QSplitter *>(QStringLiteral("shellMainColumn"));
            if (!row || !mainColumn || sidebar->parentWidget() != window.findChild<QSplitter *>(QStringLiteral("shellRightColumn")))
                return 4;
            row->setSizes({0, 700, 200});
            row->splitterMoved(700, 2);
            saveSplitterSizes(expected, row, QStringLiteral("row"));
            if (mainColumn->count() != 2)
                return 13;
            mainColumn->setSizes({400, 200});
            mainColumn->splitterMoved(400, 1);
            saveSplitterSizes(expected, mainColumn, QStringLiteral("main"));
        }
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

    if (!menu->actions().at(1)->isChecked())
        return 6;
    if (layout == QLatin1String("one")) {
        if (!window.findChild<Gui::OnePanelAtTimeWidget *>() || !qobject_cast<Gui::OnePanelAtTimeWidget *>(sidebar->parentWidget()))
            return 6;
    } else {
        auto *row = window.findChild<QSplitter *>(QStringLiteral("shellRow"));
        auto *mainColumn = window.findChild<QSplitter *>(QStringLiteral("shellMainColumn"));
        auto *rightColumn = window.findChild<QSplitter *>(QStringLiteral("shellRightColumn"));
        if (!row || !mainColumn || !rightColumn || sidebar->parentWidget() != rightColumn)
            return 6;
        if (mainColumn->orientation() != (layout == QLatin1String("wide") ? Qt::Horizontal : Qt::Vertical))
            return 7;
        QSettings expected(QDir::home().filePath(QStringLiteral("expected.ini")), QSettings::IniFormat);
        if (!matchesSplitterSizes(expected, row, QStringLiteral("row")))
            return 8;
        if (!matchesSplitterSizes(expected, mainColumn, QStringLiteral("main")))
            return 8;
    }
    KConfigGroup selection(KSharedConfig::openStateConfig(), QStringLiteral("MailboxSelection"));
    const KConfigGroup account = selection.group(window.imapAccess()->credentialAccountId());
    if (account.readEntry("Mailbox", QString()) != QLatin1String("INBOX") ||
        account.readEntry("MessageMailbox", QString()) != QLatin1String("INBOX") ||
        account.readEntry("MessageUidValidity", 0u) != 123u || account.readEntry("MessageUid", 0u) != 42u)
        return 11;
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
