/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Composer/SenderIdentitiesModel.h"
#include "Gui/MailBoxTreeView.h"
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
#include <QLineEdit>
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
        QVERIFY(haveUid == haveValidity);
        if (haveUid) {
            QVERIFY(savedUid && savedValidity);
            KConfigGroup selection(KSharedConfig::openStateConfig(), QStringLiteral("MailboxSelection"));
            KConfigGroup account = selection.group(QStringLiteral("account-0"));
            account.writeEntry("Mailbox", QStringLiteral("INBOX"));
            account.writeEntry("MessageMailbox", QStringLiteral("INBOX"));
            account.writeEntry("MessageUidValidity", savedValidity);
            account.writeEntry("MessageUid", savedUid);
            QVERIFY(account.sync());
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
        QVERIFY(QMetaObject::invokeMethod(&window, "slotShowSettings", Qt::DirectConnection));
        QVERIFY(setupSubmitted);
        QCOMPARE(settings.value(Common::SettingsNames::imapHostKey).toString(), QStringLiteral("127.0.0.1"));
        QCOMPARE(settings.value(Common::SettingsNames::imapPortKey).toInt(), port);
        QCOMPARE(settings.value(Common::SettingsNames::imapUserKey).toString(), QStringLiteral("synthetic@example.test"));
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
        if (haveUid) {
            auto *messages = window.findChild<Gui::MsgListView *>();
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
    qputenv("HOME", home.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", home.filePath(QStringLiteral("config")).toUtf8());
    qputenv("XDG_DATA_HOME", home.filePath(QStringLiteral("data")).toUtf8());
    qputenv("XDG_CACHE_HOME", home.filePath(QStringLiteral("cache")).toUtf8());
    qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent/plasma-impresario-live-test");
    qunsetenv("TROJITA_PROFILE");
    QStandardPaths::setTestModeEnabled(true);
#ifdef TROJITA_HAVE_WEBENGINE
    Gui::WebEngineMessageProfile::registerScheme();
#endif
    Common::registerMetaTypes();
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(license);
    KLocalizedString::setApplicationDomain("plasma-impresario");
    if (!Imap::Mailbox::SystemNetworkWatcher::init())
        return 1;
    LiveImapTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_LiveImap.moc"
