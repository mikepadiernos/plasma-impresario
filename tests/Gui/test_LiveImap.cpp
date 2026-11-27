/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Composer/SenderIdentitiesModel.h"
#include "Gui/MailBoxTreeView.h"
#include "Gui/MessageListWidget.h"
#include "Gui/MsgListView.h"
#include "Gui/Window.h"
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/NetworkWatcher.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "configure.cmake.h"
#ifdef TROJITA_HAVE_WEBENGINE
#include "Gui/MessageView.h"
#include "Gui/WebEngineMessageProfile.h"
#endif
#include "static_plugins.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

class LiveImapTest : public QObject
{
    Q_OBJECT
private slots:
    void authenticationFailureAndRecovery()
    {
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("account.ini")), QSettings::IniFormat);
        settings.setValue(Common::SettingsNames::imapMethodKey, Common::SettingsNames::methodSSL);
        settings.setValue(Common::SettingsNames::imapHostKey, QStringLiteral("not-configured.invalid"));
        settings.setValue(Common::SettingsNames::imapStartMode, Common::SettingsNames::netOffline);
        settings.setValue(Common::SettingsNames::imapNeedsNetwork, false);
        settings.setValue(Common::SettingsNames::imapUseSystemProxy, false);
        settings.setValue(Common::SettingsNames::cacheOfflineKey, Common::SettingsNames::cacheOfflineNone);
        settings.setValue(Common::SettingsNames::guiShowSystray, false);
        settings.setValue(Common::SettingsNames::msaMethodKey, Common::SettingsNames::methodSENDMAIL);

        bool haveUid = false;
        bool haveValidity = false;
        const uint savedUid = qgetenv("PLASMA_TEST_IMAP_RESTORE_UID").toUInt(&haveUid);
        const uint savedValidity = qgetenv("PLASMA_TEST_IMAP_RESTORE_VALIDITY").toUInt(&haveValidity);
        const QString restartPhase = qEnvironmentVariable("PLASMA_TEST_IMAP_RESTART_PHASE");
        QVERIFY(restartPhase.isEmpty() || restartPhase == QLatin1String("write") || restartPhase == QLatin1String("read"));
        QVERIFY(haveUid == haveValidity);
        QVERIFY(restartPhase.isEmpty() || haveUid);
        if (haveUid) {
            QVERIFY(savedUid && savedValidity);
            KConfigGroup selection(KSharedConfig::openStateConfig(), QStringLiteral("MailboxSelection"));
            KConfigGroup account = selection.group(QStringLiteral("account-0"));
            if (restartPhase.isEmpty()) {
                account.writeEntry("Mailbox", QStringLiteral("INBOX"));
                account.writeEntry("MessageMailbox", QStringLiteral("INBOX"));
                account.writeEntry("MessageUidValidity", savedValidity);
                account.writeEntry("MessageUid", savedUid);
                QVERIFY(account.sync());
            } else if (restartPhase == QLatin1String("read")) {
                QCOMPARE(account.readEntry("Mailbox", QString()), QStringLiteral("INBOX"));
                QCOMPARE(account.readEntry("MessageMailbox", QString()), QStringLiteral("INBOX"));
                QCOMPARE(account.readEntry("MessageUidValidity", 0u), savedValidity);
                QCOMPARE(account.readEntry("MessageUid", 0u), savedUid);
            }
        }

        int passwordsEntered = 0;
        bool certificateAccepted = false;
        bool mailboxSelected = false;
        Gui::MainWindow window(&settings);
        window.senderIdentitiesModel()->appendIdentity(Composer::ItemSenderIdentity(
            QStringLiteral("Synthetic User"), QStringLiteral("synthetic@example.test"), QString(), QString()));
        bool setupSubmitted = false;
        QTimer dialogs;
        connect(&dialogs, &QTimer::timeout, this, [&] {
            for (QWidget *widget : QApplication::topLevelWidgets()) {
                if (!widget->isVisible())
                    continue;
                if (auto *password = qobject_cast<QDialog *>(widget)) {
                    auto *host = password->findChild<QLineEdit *>(QStringLiteral("imapHost"));
                    auto *portInput = password->findChild<QLineEdit *>(QStringLiteral("imapPort"));
                    auto *user = password->findChild<QLineEdit *>(QStringLiteral("imapUser"));
                    auto *input = password->findChild<QLineEdit *>(QStringLiteral("passwordLineEdit"));
                    if (host && portInput && user && !setupSubmitted) {
                        host->setText(QStringLiteral("127.0.0.1"));
                        portInput->setText(QString::number(port));
                        user->setText(QStringLiteral("synthetic@example.test"));
                        auto *buttons = password->findChild<QDialogButtonBox *>();
                        if (buttons && buttons->button(QDialogButtonBox::Save)) {
                            setupSubmitted = true;
                            buttons->button(QDialogButtonBox::Save)->click();
                        }
                    } else if (input) {
                        input->setText(passwordsEntered++ == 0 ? QStringLiteral("incorrect-synthetic-password")
                                                               : QStringLiteral("synthetic-test-only"));
                        password->accept();
                    } else if (auto *message = qobject_cast<QMessageBox *>(password);
                               message && message->windowTitle() == window.imapAccess()->sslInfoTitle() &&
                               message->standardButtons() == (QMessageBox::Yes | QMessageBox::No)) {
                        certificateAccepted = true;
                        message->button(QMessageBox::Yes)->click();
                    }
                }
            }
        });
        dialogs.start(20);

        window.show();
        QCoreApplication::processEvents();
        QVERIFY(QMetaObject::invokeMethod(&window, "slotShowAccountSettings", Qt::DirectConnection));
        QVERIFY(setupSubmitted);
        // Since accounts were introduced, the dialog saves to the active account's own settings.
        QSettings *account = window.accountSettings();
        QCOMPARE(account->value(Common::SettingsNames::imapHostKey).toString(), QStringLiteral("127.0.0.1"));
        QCOMPARE(account->value(Common::SettingsNames::imapPortKey).toInt(), port);
        QCOMPARE(account->value(Common::SettingsNames::imapUserKey).toString(), QStringLiteral("synthetic@example.test"));
        QCoreApplication::processEvents();
        QSignalSpy failures(window.imapModel(), &Imap::Mailbox::Model::authAttemptFailed);
        QSignalSpy errors(window.imapModel(), &Imap::Mailbox::Model::imapError);
        connect(window.imapModel(), &Imap::Mailbox::Model::connectionStateChanged, this,
                [&mailboxSelected](uint, Imap::ConnectionState state) {
            if (state == Imap::CONN_STATE_SELECTED)
                mailboxSelected = true;
        });
        auto *watcher = qobject_cast<Imap::Mailbox::NetworkWatcher *>(window.imapAccess()->networkWatcher());
        QVERIFY(watcher);
        watcher->setNetworkOnline();
        auto *tree = window.findChild<Gui::MailBoxTreeView *>();
        QVERIFY(tree);
        QTRY_VERIFY_WITH_TIMEOUT(tree->model()->rowCount() > 0, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(passwordsEntered >= 2, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!failures.isEmpty(), 10000);
        QVERIFY(certificateAccepted);
        QTRY_VERIFY_WITH_TIMEOUT(tree->model()->match(tree->model()->index(0, 0), Imap::Mailbox::RoleMailboxName,
                                                     QStringLiteral("INBOX"), 1).size() == 1, 10000);
        const QModelIndex inbox = tree->model()->match(tree->model()->index(0, 0), Imap::Mailbox::RoleMailboxName,
                                                       QStringLiteral("INBOX"), 1).first();
        if (haveUid && restartPhase != QLatin1String("write")) {
            auto *messages = window.findChild<Gui::MsgListView *>(QStringLiteral("messageListView"));
            QVERIFY(messages);
            QTRY_COMPARE_WITH_TIMEOUT(messages->currentIndex().data(Imap::Mailbox::RoleMessageUid).toUInt(), savedUid, 10000);
            QCOMPARE(messages->currentIndex().data(Imap::Mailbox::RoleMailboxUidValidity).toUInt(), savedValidity);
#ifdef TROJITA_HAVE_WEBENGINE
            auto *reader = window.findChild<Gui::MessageView *>();
            QVERIFY(reader);
            QTRY_COMPARE_WITH_TIMEOUT(reader->currentMessage().data(Imap::Mailbox::RoleMessageUid).toUInt(), savedUid, 10000);
#endif
        }
        QVERIFY2(errors.isEmpty(), qPrintable(errors.isEmpty() ? QString() : errors.first().first().toString()));
        tree->scrollTo(inbox);
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualRect(inbox).center());
        QTRY_VERIFY_WITH_TIMEOUT(mailboxSelected, 10000);
        QVERIFY2(errors.isEmpty(), qPrintable(errors.isEmpty() ? QString() : errors.first().first().toString()));
        const QString searchSubject = qEnvironmentVariable("PLASMA_TEST_IMAP_SEARCH_SUBJECT");
        if (!searchSubject.isEmpty()) {
            QVERIFY(haveUid);
            QTRY_COMPARE_WITH_TIMEOUT(inbox.data(Imap::Mailbox::RoleTotalMessageCount).toInt(), 2, 10000);
            QTRY_COMPARE_WITH_TIMEOUT(inbox.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 2, 10000);
            QVERIFY(inbox.data(Qt::ToolTipRole).toString().contains(QStringLiteral("2")));
            auto *searchWidget = window.findChild<Gui::MessageListWidget *>();
            auto *messages = window.findChild<Gui::MsgListView *>(QStringLiteral("messageListView"));
            QVERIFY(searchWidget);
            QVERIFY(messages);
            QTRY_VERIFY_WITH_TIMEOUT(messages->model()->rowCount() >= 2, 10000);
            auto *query = searchWidget->findChild<QLineEdit *>(QStringLiteral("quickSearch"));
            QVERIFY(query);
            searchWidget->findChild<QAction *>(QStringLiteral("searchInSenders"))->setChecked(false);
            query->setText(searchSubject);
            QTimer::singleShot(0, []() {
                if (auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget())) {
                    dialog->setTextValue(QStringLiteral("Synthetic search"));
                    dialog->accept();
                }
            });
            searchWidget->findChild<QAction *>(QStringLiteral("saveSearch"))->trigger();
            query->setText(QStringLiteral("nonmatching-synthetic-search-92731"));
            QTest::keyClick(query, Qt::Key_Return);
            QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 0, 10000);
            auto *menu = searchWidget->findChild<QMenu *>(QStringLiteral("savedSearches"));
            QVERIFY(menu);
            QVERIFY(QMetaObject::invokeMethod(menu, "aboutToShow"));
            QCOMPARE(menu->actions().size(), 1);
            menu->actions().first()->menu()->findChild<QAction *>(QStringLiteral("runSavedSearch"))->trigger();
            QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 1, 10000);
            QCOMPARE(messages->model()->index(0, 0).data(Imap::Mailbox::RoleMessageUid).toUInt(), savedUid);
            query->clear();
            QTRY_VERIFY_WITH_TIMEOUT(messages->model()->rowCount() >= 2, 10000);
        }
        if (restartPhase == QLatin1String("write")) {
            auto *messages = window.findChild<Gui::MsgListView *>(QStringLiteral("messageListView"));
            QVERIFY(messages);
            QTRY_VERIFY_WITH_TIMEOUT(messages->model()->rowCount() > 0, 10000);
            QModelIndexList matches;
            QTRY_VERIFY_WITH_TIMEOUT(!(matches = messages->model()->match(messages->model()->index(0, 0),
                                     Imap::Mailbox::RoleMessageUid, savedUid, 1, Qt::MatchExactly | Qt::MatchRecursive)).isEmpty(), 10000);
            messages->scrollTo(matches.first());
            QTest::mouseClick(messages->viewport(), Qt::LeftButton, Qt::NoModifier, messages->visualRect(matches.first()).center());
            KConfigGroup selection(KSharedConfig::openStateConfig(), QStringLiteral("MailboxSelection"));
            const KConfigGroup account = selection.group(QStringLiteral("account-0"));
            QCOMPARE(account.readEntry("MessageUid", 0u), savedUid);
            QCOMPARE(account.readEntry("MessageUidValidity", 0u), savedValidity);
        }
        watcher->setNetworkOffline();
        QCOMPARE(watcher->effectiveNetworkPolicy(), Imap::Mailbox::NETWORK_OFFLINE);
        mailboxSelected = false;
        watcher->setNetworkOnline();
        QTRY_VERIFY_WITH_TIMEOUT(mailboxSelected, 10000);
        QVERIFY2(errors.isEmpty(), qPrintable(errors.isEmpty() ? QString() : errors.first().first().toString()));
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir home;
    if (!home.isValid())
        return 1;
    QString homePath = home.path();
    const QString restartHome = qEnvironmentVariable("PLASMA_TEST_IMAP_RESTART_HOME");
    const QString restartPhase = qEnvironmentVariable("PLASMA_TEST_IMAP_RESTART_PHASE");
    if (restartHome.isEmpty() != restartPhase.isEmpty())
        return 1;
    if (!restartHome.isEmpty()) {
        homePath = QFileInfo(restartHome).canonicalFilePath();
        if (!homePath.startsWith(QDir::tempPath() + QStringLiteral("/plasma-impresario-imap-restart-")))
            return 1;
    }
    qputenv("HOME", homePath.toUtf8());
    qputenv("XDG_CONFIG_HOME", (homePath + QStringLiteral("/config")).toUtf8());
    qputenv("XDG_DATA_HOME", (homePath + QStringLiteral("/data")).toUtf8());
    qputenv("XDG_CACHE_HOME", (homePath + QStringLiteral("/cache")).toUtf8());
    qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent/plasma-impresario-live-test");
    qunsetenv("TROJITA_PROFILE");
    QStandardPaths::setTestModeEnabled(true);
#ifdef TROJITA_HAVE_WEBENGINE
    Gui::WebEngineMessageProfile::registerScheme();
#endif
    Common::registerMetaTypes();
    QApplication app(argc, argv);
    for (auto location : {QStandardPaths::GenericConfigLocation, QStandardPaths::AppDataLocation, QStandardPaths::CacheLocation}) {
        if (!QStandardPaths::writableLocation(location).startsWith(homePath + QLatin1Char('/')))
            return 1;
    }
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(license);
    KLocalizedString::setApplicationDomain("plasma-impresario");
    if (!Imap::Mailbox::SystemNetworkWatcher::init())
        return 1;
    LiveImapTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_LiveImap.moc"
