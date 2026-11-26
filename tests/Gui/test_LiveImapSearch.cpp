/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Gui/InlineNotificationBar.h"
#include "Gui/MailBoxTreeView.h"
#include "Gui/MessageListWidget.h"
#include "Gui/MsgListView.h"
#include "Gui/SearchBuilderDialog.h"
#include "Gui/SearchFoldersDialog.h"
#include "Gui/Window.h"
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "UiUtils/SearchScope.h"
#include "configure.cmake.h"
#ifdef TROJITA_HAVE_WEBENGINE
#include "Gui/WebEngineMessageProfile.h"
#endif
#include "static_plugins.h"

#include "Plugins/PluginManager.h"

#include <KLocalizedString>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>

#include <memory>

namespace {

// Synthetic credentials for a disposable loopback server; never point this test at a real account.
const QString syntheticPassword = QStringLiteral("synthetic-test-only");

void configureAccount(const QString &fileName, int port, const QString &user)
{
    QSettings account(fileName, QSettings::IniFormat);
    account.setValue(Common::SettingsNames::imapMethodKey, Common::SettingsNames::methodSSL);
    account.setValue(Common::SettingsNames::imapHostKey, QStringLiteral("127.0.0.1"));
    account.setValue(Common::SettingsNames::imapPortKey, port);
    account.setValue(Common::SettingsNames::imapUserKey, user);
    account.setValue(Common::SettingsNames::imapStartMode, Common::SettingsNames::netOnline);
    account.setValue(Common::SettingsNames::imapNeedsNetwork, false);
    account.setValue(Common::SettingsNames::imapUseSystemProxy, false);
    account.setValue(Common::SettingsNames::cacheOfflineKey, Common::SettingsNames::cacheOfflineNone);
    account.setValue(Common::SettingsNames::msaMethodKey, Common::SettingsNames::methodSENDMAIL);
    account.sync();
}

QModelIndex findMailbox(const QAbstractItemModel *model, const QString &name)
{
    if (!model || model->rowCount() == 0)
        return {};
    const QModelIndexList matches = model->match(model->index(0, 0), Imap::Mailbox::RoleMailboxName, name, 1, Qt::MatchExactly);
    return matches.isEmpty() ? QModelIndex() : QModelIndex(matches.first());
}

QComboBox *accountSwitcher(Gui::MainWindow &window)
{
    for (auto *combo : window.findChildren<QComboBox *>()) {
        if (combo->accessibleName() == i18n("Account"))
            return combo;
    }
    return nullptr;
}

/** @short "Subject | row label" of each shown message whose envelope is loaded, sorted */
QStringList shownRows(const QAbstractItemModel *model)
{
    QStringList rows;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        const QString subject = index.data(Imap::Mailbox::RoleMessageSubject).toString();
        if (subject.isEmpty())
            return {QStringLiteral("(not loaded)")};
        rows << subject + QStringLiteral(" | ") + index.data(Imap::Mailbox::RoleMessageRowLabel).toString();
    }
    rows.sort();
    return rows;
}

QString describe(const QStringList &rows, Gui::MessageListWidget *widget)
{
    auto *status = widget->findChild<QLabel *>(QStringLiteral("messageListStatus"));
    return rows.join(QStringLiteral("; ")) + QStringLiteral(" | status: ") + (status ? status->text() : QString())
        + QStringLiteral(" | details: ") + (status ? status->toolTip() : QString());
}

/** @short Accept the server's certificate and dismiss password prompts while a test runs */
void answerDialogs()
{
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (!widget->isVisible())
            continue;
        if (auto *message = qobject_cast<QMessageBox *>(widget);
            message && message->standardButtons() == (QMessageBox::Yes | QMessageBox::No)) {
            message->button(QMessageBox::Yes)->click();
        } else if (auto *dialog = qobject_cast<QDialog *>(widget);
                   dialog && dialog->findChild<QLineEdit *>(QStringLiteral("passwordLineEdit"))) {
            dialog->reject();
        }
    }
}

/** @short Forwards loopback connections to the server, and can hold its answers or cut the connections

Holding the answers makes the server look slow; cutting closes the open connections and refuses new ones, like a
server which went away. The TLS stream passes through unchanged.
*/
class TcpProxy : public QObject
{
public:
    explicit TcpProxy(quint16 serverPort)
        : m_serverPort(serverPort)
    {
        connect(&m_listener, &QTcpServer::newConnection, this, &TcpProxy::accept);
        m_listener.listen(QHostAddress::LocalHost);
    }

    bool isListening() const { return m_listener.isListening(); }
    quint16 port() const { return m_listener.serverPort(); }

    void hold() { m_holding = true; }
    void release()
    {
        m_holding = false;
        for (const auto &link : std::as_const(m_links)) {
            if (link->client && !link->held.isEmpty())
                link->client->write(link->held);
            link->held.clear();
        }
    }
    void cut()
    {
        m_refusing = true;
        m_holding = false;
        const auto links = std::exchange(m_links, {});
        for (const auto &link : links) {
            if (link->client)
                link->client->abort();
            if (link->server)
                link->server->abort();
        }
    }
    void restore()
    {
        m_refusing = false;
        m_holding = false;
    }

private:
    struct Link {
        QPointer<QTcpSocket> client;
        QPointer<QTcpSocket> server;
        QByteArray held;
    };

    void accept()
    {
        while (QTcpSocket *client = m_listener.nextPendingConnection()) {
            if (m_refusing) {
                client->abort();
                client->deleteLater();
                continue;
            }
            auto link = std::make_shared<Link>();
            auto *server = new QTcpSocket(this);
            client->setParent(this);
            link->client = client;
            link->server = server;
            m_links << link;
            connect(client, &QTcpSocket::readyRead, server, [client, server]() { server->write(client->readAll()); });
            connect(server, &QTcpSocket::readyRead, client, [this, link, client, server]() {
                const QByteArray data = server->readAll();
                if (m_holding)
                    link->held += data;
                else
                    client->write(data);
            });
            connect(client, &QTcpSocket::disconnected, server, &QTcpSocket::disconnectFromHost);
            connect(server, &QTcpSocket::disconnected, client, &QTcpSocket::disconnectFromHost);
            server->connectToHost(QHostAddress::LocalHost, m_serverPort);
        }
    }

    QTcpServer m_listener;
    quint16 m_serverPort;
    bool m_holding = false;
    bool m_refusing = false;
    QList<std::shared_ptr<Link>> m_links;
};

}

class LiveImapSearchTest : public QObject
{
    Q_OBJECT
private slots:
    /**
     * Expects a loopback IMAPS server on PLASMA_TEST_IMAPS_PORT with users sharing the synthetic password, each
     * message with the subject "Synthetic NAME": alpha@example.test with invoice-alpha-inbox and lunch in INBOX,
     * invoice-alpha-archive in Archive and invoice-alpha-old in Archive/2024; beta@example.test with invoice-beta in
     * INBOX and invoice-beta-receipt in Receipts. PLASMA_TEST_ALPHA_MAILDIR names alpha's Maildir. See
     * tools/dev-container/live-imap-search.sh.
     */
    void searchesAcrossFoldersAndAccounts()
    {
        using UiUtils::SearchScope::Places;
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
        const QString alphaMaildir = qEnvironmentVariable("PLASMA_TEST_ALPHA_MAILDIR");
        QVERIFY(!alphaMaildir.isEmpty());
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        settings.setValue(Common::SettingsNames::guiShowSystray, false);
        settings.setValue(Common::SettingsNames::passwordPlugin, QStringLiteral("cleartextpassword"));
        settings.setValue(QStringLiteral("accounts/ids"),
                          QStringList{QStringLiteral("account-1"), QStringLiteral("account-2"), QStringLiteral("account-3")});
        settings.setValue(QStringLiteral("accounts/active"), QStringLiteral("account-1"));
        settings.setValue(QStringLiteral("accounts/next"), 4);
        settings.setValue(QStringLiteral("accounts/names/account-1"), QStringLiteral("Alpha"));
        settings.setValue(QStringLiteral("accounts/names/account-2"), QStringLiteral("Beta"));
        settings.setValue(QStringLiteral("accounts/names/account-3"), QStringLiteral("Broken"));
        settings.setValue(QStringLiteral("accountPasswords/account-1/imap"), syntheticPassword);
        settings.setValue(QStringLiteral("accountPasswords/account-2/imap"), syntheticPassword);
        settings.setValue(QStringLiteral("accountPasswords/account-3/imap"), QStringLiteral("incorrect-synthetic-password"));
        settings.sync();
        configureAccount(directory.filePath(QStringLiteral("main-account-1.conf")), port, QStringLiteral("alpha@example.test"));
        configureAccount(directory.filePath(QStringLiteral("main-account-2.conf")), port, QStringLiteral("beta@example.test"));
        configureAccount(directory.filePath(QStringLiteral("main-account-3.conf")), port, QStringLiteral("beta@example.test"));

        QTimer dialogs;
        connect(&dialogs, &QTimer::timeout, this, &answerDialogs);
        dialogs.start(20);

        Gui::MainWindow window(&settings);
        window.resize(1000, 700);
        window.show();
        auto *switcher = accountSwitcher(window);
        auto *tree = window.findChild<Gui::MailBoxTreeView *>();
        auto *searchWidget = window.findChild<Gui::MessageListWidget *>();
        auto *messages = window.findChild<Gui::MsgListView *>();
        QVERIFY(switcher && tree && searchWidget && messages);
        auto *query = searchWidget->findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        auto *scope = searchWidget->findChild<QLabel *>(QStringLiteral("messageListScope"));
        auto *status = searchWidget->findChild<QLabel *>(QStringLiteral("messageListStatus"));
        QVERIFY(query && scope && status);

        QModelIndex inbox;
        QTRY_VERIFY_WITH_TIMEOUT((inbox = findMailbox(tree->model(), QStringLiteral("INBOX"))).isValid(), 20000);
        tree->scrollTo(inbox);
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualRect(inbox).center());
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 2, 10000);
        // The account with a rejected password is known to have failed before searching everywhere
        QTRY_VERIFY_WITH_TIMEOUT(switcher->itemText(2).startsWith(QLatin1String("Broken (")), 20000);

        // The current folder is searched by default, as before
        QCOMPARE(searchWidget->searchPlaces(), Places::CurrentFolder);
        query->setText(QStringLiteral("invoice"));
        emit query->returnPressed();
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 1, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->index(0, 0).data(Imap::Mailbox::RoleMessageSubject).toString(),
                                  QStringLiteral("Synthetic invoice-alpha-inbox"), 10000);

        // Every folder of the shown account, with the folder of each result
        QStringList rows;
        searchWidget->setSearchPlaces(Places::Account);
        QTRY_COMPARE_WITH_TIMEOUT(scope->text(), QStringLiteral("Search Results"), 5000);
        QTRY_VERIFY2_WITH_TIMEOUT((rows = shownRows(messages->model())).size() == 3 && status->text().contains(QLatin1String("3 messages found")),
                                  qPrintable(describe(rows, searchWidget)), 30000);
        QCOMPARE(rows, (QStringList{QStringLiteral("Synthetic invoice-alpha-archive | Alpha \u00B7 Archive"),
                                    QStringLiteral("Synthetic invoice-alpha-inbox | Alpha \u00B7 Inbox"),
                                    QStringLiteral("Synthetic invoice-alpha-old | Alpha \u00B7 Archive/2024")}));
        QVERIFY(!status->text().contains(QLatin1String("could not be searched")));

        // All accounts; the one which cannot log in is reported
        searchWidget->setSearchPlaces(Places::AllAccounts);
        QTRY_VERIFY2_WITH_TIMEOUT((rows = shownRows(messages->model())).size() == 5 && status->text().contains(QLatin1String("5 messages found")),
                                  qPrintable(describe(rows, searchWidget)), 30000);
        QCOMPARE(rows, (QStringList{QStringLiteral("Synthetic invoice-alpha-archive | Alpha \u00B7 Archive"),
                                    QStringLiteral("Synthetic invoice-alpha-inbox | Alpha \u00B7 Inbox"),
                                    QStringLiteral("Synthetic invoice-alpha-old | Alpha \u00B7 Archive/2024"),
                                    QStringLiteral("Synthetic invoice-beta | Beta \u00B7 Inbox"),
                                    QStringLiteral("Synthetic invoice-beta-receipt | Beta \u00B7 Receipts")}));
        QVERIFY2(status->text().contains(QLatin1String("1 place could not be searched")), qPrintable(status->text()));
        QVERIFY2(scope->toolTip().contains(QLatin1String("Broken")), qPrintable(scope->toolTip()));

        // Folders of both accounts, chosen in the dialog
        searchWidget->findChild<QAction *>(QStringLiteral("searchPlaces-folders"))->trigger();
        Gui::SearchFoldersDialog *chooser = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((chooser = window.findChild<Gui::SearchFoldersDialog *>()) && chooser->isVisible(), 5000);
        QTreeWidget *folders = chooser->tree();
        QCOMPARE(folders->topLevelItemCount(), 3);
        const auto check = [folders](int account, const QString &path) {
            QTreeWidgetItem *parent = folders->topLevelItem(account);
            for (int child = 0; child < parent->childCount(); ++child) {
                if (parent->child(child)->text(0) == path) {
                    parent->child(child)->setCheckState(0, Qt::Checked);
                    return true;
                }
            }
            return false;
        };
        QVERIFY(check(0, QStringLiteral("Archive / 2024")));
        QVERIFY(check(1, QStringLiteral("Receipts")));
        chooser->accept();
        QCOMPARE(searchWidget->searchPlaces(), Places::SelectedFolders);
        QCOMPARE(searchWidget->selectedFolders(),
                 (QStringList{QStringLiteral("account-1/Archive/2024"), QStringLiteral("account-2/Receipts")}));
        QTRY_VERIFY2_WITH_TIMEOUT((rows = shownRows(messages->model())).size() == 2 && status->text().contains(QLatin1String("2 messages found")),
                                  qPrintable(describe(rows, searchWidget)), 30000);
        QCOMPARE(rows, (QStringList{QStringLiteral("Synthetic invoice-alpha-old | Alpha \u00B7 Archive/2024"),
                                    QStringLiteral("Synthetic invoice-beta-receipt | Beta \u00B7 Receipts")}));

        // A missing folder is reported, and the others are still searched
        searchWidget->setSelectedFolders({QStringLiteral("account-1/Archive"), QStringLiteral("account-1/Gone")});
        QTRY_VERIFY2_WITH_TIMEOUT((rows = shownRows(messages->model())).size() == 1 && status->text().contains(QLatin1String("1 message found")),
                                  qPrintable(describe(rows, searchWidget)), 30000);
        QVERIFY2(status->text().contains(QLatin1String("1 place could not be searched")), qPrintable(status->text()));

        // Leaving the results shows the folder from before, which is selected on the server again
        searchWidget->clearSearch();
        QTRY_VERIFY_WITH_TIMEOUT(scope->text() != QStringLiteral("Search Results"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 2, 10000);
        QFile late(alphaMaildir + QStringLiteral("/new/alpha-late.synthetic"));
        QVERIFY(late.open(QIODevice::WriteOnly));
        late.write("From: Synthetic Sender <sender@example.test>\r\nTo: alpha@example.test\r\n"
                   "Subject: Synthetic alpha-late\r\nDate: Tue, 24 Nov 2026 13:00:00 +0000\r\n"
                   "Message-ID: <alpha-late@example.test>\r\n\r\nSynthetic body.\r\n");
        late.close();
        late.setPermissions(late.permissions() | QFileDevice::ReadOther | QFileDevice::ReadGroup);
        if (auto *checkMail = window.findChild<QAction *>(QStringLiteral("action_check_mail")))
            checkMail->trigger();
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 3, 20000);

        // In the Unified Inbox, the shown list is the INBOX of each account
        searchWidget->setSearchPlaces(Places::CurrentFolder);
        auto *unified = window.findChild<QAction *>(QStringLiteral("action_unified_inbox"));
        QVERIFY(unified);
        unified->trigger();
        QVERIFY(unified->isChecked());
        QVERIFY(query->isEnabled() || messages->model()->rowCount() == 0);
        QTRY_VERIFY_WITH_TIMEOUT(query->isEnabled(), 20000);
        QCOMPARE(searchWidget->findChild<QAction *>(QStringLiteral("searchPlaces-current"))->text(), QStringLiteral("Unified Inbox"));
        query->setText(QStringLiteral("invoice"));
        emit query->returnPressed();
        QTRY_VERIFY2_WITH_TIMEOUT((rows = shownRows(messages->model())).size() == 2 && status->text().contains(QLatin1String("2 messages found")),
                                  qPrintable(describe(rows, searchWidget)), 30000);
        QCOMPARE(rows, (QStringList{QStringLiteral("Synthetic invoice-alpha-inbox | Alpha \u00B7 Inbox"),
                                    QStringLiteral("Synthetic invoice-beta | Beta \u00B7 Inbox")}));
        query->clear();
        emit query->returnPressed();
        QTRY_VERIFY2_WITH_TIMEOUT(shownRows(messages->model()).size() >= 4 && !status->text().contains(QLatin1String("found")),
                                  qPrintable(describe(shownRows(messages->model()), searchWidget)), 20000);
        QCOMPARE(scope->text(), QStringLiteral("Unified Inbox"));

        // Leaving the Unified Inbox clears its search and shows the folder again
        query->setText(QStringLiteral("invoice"));
        emit query->returnPressed();
        QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QLatin1String("found")), 30000);
        unified->trigger();
        QVERIFY(!unified->isChecked());
        QVERIFY(query->text().isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 3, 10000);
        QCOMPARE(searchWidget->findChild<QAction *>(QStringLiteral("searchPlaces-current"))->text(), QStringLiteral("Current Folder"));
    }

    /**
     * The same server and users as searchesAcrossFoldersAndAccounts(), with Beta reached through a proxy which can
     * hold the server's answers or cut the connection: a slow search can be stopped, nothing arrives from a stopped
     * search, a connection lost during a search is reported with a retry action, and retrying finds the rest.
     */
    void stopsSlowSearchesAndRetriesFailedPlaces()
    {
        using UiUtils::SearchScope::Places;
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
        TcpProxy proxy{quint16(port)};
        QVERIFY(proxy.isListening());
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        settings.setValue(Common::SettingsNames::guiShowSystray, false);
        settings.setValue(Common::SettingsNames::passwordPlugin, QStringLiteral("cleartextpassword"));
        settings.setValue(QStringLiteral("accounts/ids"), QStringList{QStringLiteral("account-1"), QStringLiteral("account-2")});
        settings.setValue(QStringLiteral("accounts/active"), QStringLiteral("account-1"));
        settings.setValue(QStringLiteral("accounts/next"), 3);
        settings.setValue(QStringLiteral("accounts/names/account-1"), QStringLiteral("Alpha"));
        settings.setValue(QStringLiteral("accounts/names/account-2"), QStringLiteral("Beta"));
        settings.setValue(QStringLiteral("accountPasswords/account-1/imap"), syntheticPassword);
        settings.setValue(QStringLiteral("accountPasswords/account-2/imap"), syntheticPassword);
        settings.sync();
        configureAccount(directory.filePath(QStringLiteral("main-account-1.conf")), port, QStringLiteral("alpha@example.test"));
        configureAccount(directory.filePath(QStringLiteral("main-account-2.conf")), proxy.port(), QStringLiteral("beta@example.test"));

        QTimer dialogs;
        connect(&dialogs, &QTimer::timeout, this, &answerDialogs);
        dialogs.start(20);

        Gui::MainWindow window(&settings);
        window.resize(1000, 700);
        window.show();
        auto *tree = window.findChild<Gui::MailBoxTreeView *>();
        auto *searchWidget = window.findChild<Gui::MessageListWidget *>();
        auto *messages = window.findChild<Gui::MsgListView *>();
        QVERIFY(tree && searchWidget && messages);
        auto *query = searchWidget->findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        auto *status = searchWidget->findChild<QLabel *>(QStringLiteral("messageListStatus"));
        auto *stop = searchWidget->findChild<QToolButton *>(QStringLiteral("stopSearchButton"));
        auto *retry = searchWidget->findChild<QToolButton *>(QStringLiteral("retrySearchButton"));
        QVERIFY(query && status && stop && retry);

        QModelIndex inbox;
        QTRY_VERIFY_WITH_TIMEOUT((inbox = findMailbox(tree->model(), QStringLiteral("INBOX"))).isValid(), 20000);
        tree->scrollTo(inbox);
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualRect(inbox).center());
        QTRY_VERIFY_WITH_TIMEOUT(messages->model()->rowCount() >= 2, 10000);

        const QStringList alphaRows{QStringLiteral("Synthetic invoice-alpha-archive | Alpha \u00B7 Archive"),
                                    QStringLiteral("Synthetic invoice-alpha-inbox | Alpha \u00B7 Inbox"),
                                    QStringLiteral("Synthetic invoice-alpha-old | Alpha \u00B7 Archive/2024")};
        const QStringList allRows = alphaRows
            + QStringList{QStringLiteral("Synthetic invoice-beta | Beta \u00B7 Inbox"),
                          QStringLiteral("Synthetic invoice-beta-receipt | Beta \u00B7 Receipts")};

        // Both accounts answer: nothing to stop or retry afterwards
        QStringList rows;
        query->setText(QStringLiteral("invoice"));
        searchWidget->setSearchPlaces(Places::AllAccounts);
        QTRY_VERIFY2_WITH_TIMEOUT((rows = shownRows(messages->model())) == allRows && status->text().contains(QLatin1String("5 messages found")),
                                  qPrintable(describe(rows, searchWidget)), 30000);
        QVERIFY(stop->isHidden());
        QVERIFY(retry->isHidden());
        QVERIFY2(status->toolTip().contains(QLatin1String("Alpha: 3 found")) && status->toolTip().contains(QLatin1String("Beta: 2 found")),
                 qPrintable(status->toolTip()));

        // A slow server does not hold up the others; their results arrive while it is still being searched
        proxy.hold();
        emit query->returnPressed();
        QTRY_VERIFY2_WITH_TIMEOUT((rows = shownRows(messages->model())) == alphaRows && status->text().contains(QLatin1String("Searching"))
                                      && status->toolTip().contains(QLatin1String("Alpha: 3 found"))
                                      && status->toolTip().contains(QLatin1String("Beta: searching")),
                                  qPrintable(describe(rows, searchWidget)), 30000);
        QVERIFY(stop->isVisible());
        QVERIFY(retry->isHidden());

        // Stopping keeps what was found, and the slow server's answers which arrive afterwards are not shown
        stop->click();
        QTRY_VERIFY2_WITH_TIMEOUT(status->text().contains(QLatin1String("Search stopped after")), qPrintable(describe(rows, searchWidget)), 5000);
        QVERIFY2(status->text().contains(QLatin1String("3 found")), qPrintable(status->text()));
        QVERIFY2(status->toolTip().contains(QLatin1String("Beta: stopped after 0 of 2 folders")), qPrintable(status->toolTip()));
        QVERIFY(stop->isHidden());
        QCOMPARE(shownRows(messages->model()), alphaRows);
        proxy.release();
        QTest::qWait(3000);
        QCOMPARE(shownRows(messages->model()), alphaRows);
        QVERIFY2(status->text().contains(QLatin1String("Search stopped after")), qPrintable(status->text()));

        // The connection to a server is lost during the search: the results of the others stay, and it is reported
        proxy.hold();
        emit query->returnPressed();
        QTRY_VERIFY2_WITH_TIMEOUT((rows = shownRows(messages->model())) == alphaRows && status->toolTip().contains(QLatin1String("Beta: searching")),
                                  qPrintable(describe(rows, searchWidget)), 30000);
        proxy.cut();
        QTRY_VERIFY2_WITH_TIMEOUT(!status->text().contains(QLatin1String("Searching"))
                                      && status->text().contains(QLatin1String("could not be searched")),
                                  qPrintable(describe(shownRows(messages->model()), searchWidget)), 30000);
        QCOMPARE(shownRows(messages->model()), alphaRows);
        QVERIFY(retry->isVisible());
        QPushButton *notified = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((notified = window.findChild<QPushButton *>(QStringLiteral("searchRetry"))) && notified->isVisible(), 5000);

        // Once the server is back, retrying searches only what is missing
        proxy.restore();
        notified->click();
        QTRY_VERIFY2_WITH_TIMEOUT((rows = shownRows(messages->model())) == allRows && status->text().contains(QLatin1String("5 messages found"))
                                      && !status->text().contains(QLatin1String("could not be searched")),
                                  qPrintable(describe(rows, searchWidget)), 60000);
        QVERIFY(retry->isHidden());
        QVERIFY(stop->isHidden());
    }

    /**
     * The same server and alpha user as searchesAcrossFoldersAndAccounts(): searches the server cannot run are
     * explained in the status instead of looking like an empty result, while the field search and raw IMAP search
     * still work. PLASMA_TEST_EXPECT_ESEARCH says whether the server advertises ESEARCH (1) or only SEARCH (0); the
     * account claims X-NO-UTF8-SEARCH, so plain SEARCH sends ASCII only. Pass the slot name to run only this test.
     */
    void explainsSearchesTheServerCannotRun()
    {
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
        const bool expectEsearch = qEnvironmentVariable("PLASMA_TEST_EXPECT_ESEARCH", QStringLiteral("1")) != QLatin1String("0");
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        settings.setValue(Common::SettingsNames::guiShowSystray, false);
        settings.setValue(Common::SettingsNames::passwordPlugin, QStringLiteral("cleartextpassword"));
        settings.setValue(QStringLiteral("accounts/ids"), QStringList{QStringLiteral("account-1")});
        settings.setValue(QStringLiteral("accounts/active"), QStringLiteral("account-1"));
        settings.setValue(QStringLiteral("accounts/next"), 2);
        settings.setValue(QStringLiteral("accounts/names/account-1"), QStringLiteral("Alpha"));
        settings.setValue(QStringLiteral("accountPasswords/account-1/imap"), syntheticPassword);
        settings.sync();
        const QString accountFile = directory.filePath(QStringLiteral("main-account-1.conf"));
        configureAccount(accountFile, port, QStringLiteral("alpha@example.test"));
        {
            QSettings account(accountFile, QSettings::IniFormat);
            account.setValue(Common::SettingsNames::imapBlacklistedCapabilities, QStringList{QStringLiteral("X-NO-UTF8-SEARCH")});
            account.sync();
        }

        QTimer dialogs;
        connect(&dialogs, &QTimer::timeout, this, &answerDialogs);
        dialogs.start(20);

        Gui::MainWindow window(&settings);
        window.resize(1000, 700);
        window.show();
        auto *tree = window.findChild<Gui::MailBoxTreeView *>();
        auto *searchWidget = window.findChild<Gui::MessageListWidget *>();
        auto *messages = window.findChild<Gui::MsgListView *>();
        QVERIFY(tree && searchWidget && messages);
        auto *query = searchWidget->findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        auto *status = searchWidget->findChild<QLabel *>(QStringLiteral("messageListStatus"));
        auto *raw = searchWidget->findChild<QAction *>(QStringLiteral("searchRaw"));
        QVERIFY(query && status && raw);

        QModelIndex inbox;
        QTRY_VERIFY_WITH_TIMEOUT((inbox = findMailbox(tree->model(), QStringLiteral("INBOX"))).isValid(), 20000);
        tree->scrollTo(inbox);
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualRect(inbox).center());
        // searchesAcrossFoldersAndAccounts() may have added a message to INBOX
        QTRY_VERIFY_WITH_TIMEOUT(messages->model()->rowCount() >= 2, 10000);
        const int inboxSize = messages->model()->rowCount();
        Imap::Mailbox::Model *model = window.imapModel();
        QVERIFY(model);
        QTRY_VERIFY_WITH_TIMEOUT(model->capabilities().contains(QLatin1String("IMAP4rev1"), Qt::CaseInsensitive), 10000);
        QVERIFY2(model->capabilities().contains(QLatin1String("ESEARCH"), Qt::CaseInsensitive) == expectEsearch, qPrintable(model->capabilities().join(u' ')));

        const auto search = [&](const QString &text) {
            query->setText(text);
            emit query->returnPressed();
        };
        const auto failed = [&]() { return status->text().contains(QLatin1String("Search failed")); };

        // The field search finds the message as before
        search(QStringLiteral("invoice"));
        QTRY_VERIFY2_WITH_TIMEOUT(messages->model()->rowCount() == 1 && !failed(),
                                  qPrintable(describe(shownRows(messages->model()), searchWidget)), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->index(0, 0).data(Imap::Mailbox::RoleMessageSubject).toString(),
                                  QStringLiteral("Synthetic invoice-alpha-inbox"), 10000);

        // A raw query with a key the server does not know shows the server's reason instead of an empty list
        raw->setChecked(true);
        search(QStringLiteral(":=XFOO bar"));
        QTRY_VERIFY2_WITH_TIMEOUT(failed() && status->toolTip().contains(QLatin1String("Unknown argument XFOO")),
                                  qPrintable(describe(shownRows(messages->model()), searchWidget)), 10000);
        QCOMPARE(messages->model()->rowCount(), 0);
        QVERIFY2(status->text().contains(QLatin1String("did not understand the IMAP search query")), qPrintable(status->text()));

        // A key which needs a capability the server does not advertise is named before anything is sent
        search(QStringLiteral(":=FUZZY SUBJECT invoice"));
        QTRY_VERIFY2_WITH_TIMEOUT(failed() && status->text().contains(QLatin1String("SEARCH=FUZZY")),
                                  qPrintable(describe(shownRows(messages->model()), searchWidget)), 10000);
        QCOMPARE(messages->model()->rowCount(), 0);

        // A valid raw query works again and clears the explanation
        search(QStringLiteral(":=SUBJECT invoice"));
        QTRY_VERIFY2_WITH_TIMEOUT(messages->model()->rowCount() == 1 && !failed(),
                                  qPrintable(describe(shownRows(messages->model()), searchWidget)), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->index(0, 0).data(Imap::Mailbox::RoleMessageSubject).toString(),
                                  QStringLiteral("Synthetic invoice-alpha-inbox"), 10000);

        // Text beyond ASCII: ESEARCH always searches in UTF-8; plain SEARCH on an ASCII-only account explains why not
        search(QStringLiteral("caf\u00E9"));
        if (expectEsearch) {
            QTest::qWait(1500);
            QVERIFY2(!failed(), qPrintable(describe(shownRows(messages->model()), searchWidget)));
            QCOMPARE(messages->model()->rowCount(), 0);
        } else {
            QTRY_VERIFY2_WITH_TIMEOUT(failed() && status->text().contains(QLatin1String("plain ASCII")),
                                      qPrintable(describe(shownRows(messages->model()), searchWidget)), 10000);
        }

        // Clearing the search shows the whole folder again
        search(QString());
        QTRY_VERIFY2_WITH_TIMEOUT(messages->model()->rowCount() == inboxSize && !failed(),
                                  qPrintable(describe(shownRows(messages->model()), searchWidget)), 10000);
    }

    /**
     * The same server with gamma@example.test, whose INBOX holds "Release plan" from Ann (seen, keyword $label1, 20
     * November 2026), "Lunch on Friday" from Bob (flagged, unread, 21 November 2026), "Quarterly report" from Ann
     * (seen, multipart/mixed with an attachment of about 3 KB, 10 June 2025) and "[dev] Release notes" from Carol
     * (unread, List-Post dev@lists.example.test, 22 November 2026). Queries in the search syntax find the expected
     * messages on the server; invalid queries are explained without a search. Pass the slot name to run only this
     * test.
     */
    void searchesWithQuerySyntax()
    {
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        settings.setValue(Common::SettingsNames::guiShowSystray, false);
        settings.setValue(Common::SettingsNames::passwordPlugin, QStringLiteral("cleartextpassword"));
        settings.setValue(QStringLiteral("accounts/ids"), QStringList{QStringLiteral("account-1")});
        settings.setValue(QStringLiteral("accounts/active"), QStringLiteral("account-1"));
        settings.setValue(QStringLiteral("accounts/next"), 2);
        settings.setValue(QStringLiteral("accounts/names/account-1"), QStringLiteral("Gamma"));
        settings.setValue(QStringLiteral("accountPasswords/account-1/imap"), syntheticPassword);
        settings.sync();
        configureAccount(directory.filePath(QStringLiteral("main-account-1.conf")), port, QStringLiteral("gamma@example.test"));

        QTimer dialogs;
        connect(&dialogs, &QTimer::timeout, this, &answerDialogs);
        dialogs.start(20);

        Gui::MainWindow window(&settings);
        window.resize(1000, 700);
        window.show();
        auto *tree = window.findChild<Gui::MailBoxTreeView *>();
        auto *searchWidget = window.findChild<Gui::MessageListWidget *>();
        auto *messages = window.findChild<Gui::MsgListView *>();
        QVERIFY(tree && searchWidget && messages);
        auto *query = searchWidget->findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        auto *status = searchWidget->findChild<QLabel *>(QStringLiteral("messageListStatus"));
        auto *problem = searchWidget->findChild<Gui::InlineNotificationBar *>(QStringLiteral("searchQueryError"));
        QVERIFY(query && status && problem);

        QModelIndex inbox;
        QTRY_VERIFY_WITH_TIMEOUT((inbox = findMailbox(tree->model(), QStringLiteral("INBOX"))).isValid(), 20000);
        tree->scrollTo(inbox);
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualRect(inbox).center());
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 4, 10000);

        const auto subjects = [&]() {
            QStringList result;
            for (int row = 0; row < messages->model()->rowCount(); ++row) {
                const QString subject = messages->model()->index(row, 0).data(Imap::Mailbox::RoleMessageSubject).toString();
                if (subject.isEmpty())
                    return QStringList{QStringLiteral("(not loaded)")};
                result << subject;
            }
            result.sort();
            return result;
        };
        const auto failed = [&]() { return status->text().contains(QLatin1String("Search failed")); };
        const auto search = [&](const QString &text) {
            query->setText(text);
            emit query->returnPressed();
        };
        const auto expectFound = [&](const QString &text, QStringList expected) {
            search(text);
            expected.sort();
            QTRY_VERIFY2_WITH_TIMEOUT(subjects() == expected && !failed(),
                                      qPrintable(text + QStringLiteral(" -> ") + describe(subjects(), searchWidget)), 10000);
            QVERIFY2(problem->isHidden(), qPrintable(text + QStringLiteral(": ") + problem->text()));
        };
        const QString plan = QStringLiteral("Release plan");
        const QString lunch = QStringLiteral("Lunch on Friday");
        const QString report = QStringLiteral("Quarterly report");
        const QString notes = QStringLiteral("[dev] Release notes");

        // Fields, negation, OR and grouping
        expectFound(QStringLiteral("from:ann"), {plan, report});
        expectFound(QStringLiteral("from:ann -subject:report"), {plan});
        expectFound(QStringLiteral("from:bob OR from:carol"), {lunch, notes});
        expectFound(QStringLiteral("(from:ann OR from:carol) is:unread"), {notes});
        expectFound(QStringLiteral("(from:ann subject:release) OR is:flagged"), {plan, lunch});
        expectFound(QStringLiteral("NOT (from:ann OR from:bob)"), {notes});
        // Flags, keywords, attachments and mailing lists
        expectFound(QStringLiteral("is:flagged"), {lunch});
        expectFound(QStringLiteral("is:read"), {plan, report});
        expectFound(QStringLiteral("tag:$label1"), {plan});
        expectFound(QStringLiteral("has:attachment"), {report});
        expectFound(QStringLiteral("list:dev@lists.example.test"), {notes});
        // Dates and sizes
        expectFound(QStringLiteral("date:2025-06"), {report});
        expectFound(QStringLiteral("date<2026"), {report});
        expectFound(QStringLiteral("date>=2026-11-21"), {lunch, notes});
        expectFound(QStringLiteral("date:2026-11-20"), {plan});
        expectFound(QStringLiteral("size>2K"), {report});
        expectFound(QStringLiteral("size<=2K"), {plan, lunch, notes});
        // Plain words must all match in the chosen fields; quotes keep a phrase together
        expectFound(QStringLiteral("release notes"), {notes});
        expectFound(QStringLiteral("\"release plan\""), {plan});
        expectFound(QStringLiteral("Re: plan"), {});

        // An invalid query is explained, never sent, and the list keeps the previous search
        expectFound(QStringLiteral("is:flagged"), {lunch});
        for (const QString &text : {QStringLiteral("from:"), QStringLiteral("(from:ann"), QStringLiteral("AND lunch"),
                                    QStringLiteral("foo:bar")}) {
            search(text);
            QVERIFY2(!problem->isHidden(), qPrintable(text));
            QVERIFY2(problem->text().startsWith(QLatin1String("Invalid search: ")), qPrintable(problem->text()));
            QTest::qWait(300);
            QCOMPARE(subjects(), QStringList{lunch});
            QVERIFY2(!failed(), qPrintable(describe(subjects(), searchWidget)));
        }

        // A valid query replaces the explanation; clearing shows the whole folder again
        expectFound(QStringLiteral("from:carol"), {notes});

        // The search builder, opened from the main window, writes a query which the server runs like a typed one
        QAction *openBuilder = nullptr;
        for (QAction *action : window.findChildren<QAction *>()) {
            if (action->shortcut() == QKeySequence(QStringLiteral("Ctrl+Shift+B")))
                openBuilder = action;
        }
        QVERIFY(openBuilder);
        openBuilder->trigger();
        auto *builder = searchWidget->findChild<Gui::SearchBuilderDialog *>();
        QVERIFY(builder);
        QCOMPARE(builder->query(), QStringLiteral("from:carol"));
        using UiUtils::SearchQuery::Comparison;
        using UiUtils::SearchQuery::Field;
        UiUtils::SearchBuilder::Conditions built;
        built.match = UiUtils::SearchBuilder::Match::Any;
        built.conditions = {{Field::From, Comparison::Contains, false, QStringLiteral("carol")},
                            {Field::Size, Comparison::Greater, false, QStringLiteral("2K")}};
        builder->setConditions(built);
        builder->accept();
        QCOMPARE(query->text(), QStringLiteral("from:carol OR size>2K"));
        QTRY_VERIFY2_WITH_TIMEOUT(subjects() == QStringList({report, notes}) && !failed(),
                                  qPrintable(describe(subjects(), searchWidget)), 10000);
        search(QString());
        QTRY_VERIFY2_WITH_TIMEOUT(messages->model()->rowCount() == 4 && !failed(),
                                  qPrintable(describe(subjects(), searchWidget)), 10000);
    }

    void clearsSearchAndFiltersInOneStep()
    {
        using UiUtils::SearchScope::Places;
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        settings.setValue(Common::SettingsNames::guiShowSystray, false);
        settings.setValue(Common::SettingsNames::passwordPlugin, QStringLiteral("cleartextpassword"));
        settings.setValue(QStringLiteral("accounts/ids"), QStringList{QStringLiteral("account-1")});
        settings.setValue(QStringLiteral("accounts/active"), QStringLiteral("account-1"));
        settings.setValue(QStringLiteral("accounts/next"), 2);
        settings.setValue(QStringLiteral("accounts/names/account-1"), QStringLiteral("Gamma"));
        settings.setValue(QStringLiteral("accountPasswords/account-1/imap"), syntheticPassword);
        // Clicking messages must not change their flags, which the later searches of the same folder rely on
        settings.setValue(Common::SettingsNames::autoMarkReadEnabled, false);
        settings.sync();
        configureAccount(directory.filePath(QStringLiteral("main-account-1.conf")), port, QStringLiteral("gamma@example.test"));

        QTimer dialogs;
        connect(&dialogs, &QTimer::timeout, this, &answerDialogs);
        dialogs.start(20);

        Gui::MainWindow window(&settings);
        window.resize(1000, 700);
        window.show();
        auto *tree = window.findChild<Gui::MailBoxTreeView *>();
        auto *searchWidget = window.findChild<Gui::MessageListWidget *>();
        auto *messages = window.findChild<Gui::MsgListView *>();
        QVERIFY(tree && searchWidget && messages);
        auto *query = searchWidget->findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        auto *scope = searchWidget->findChild<QLabel *>(QStringLiteral("messageListScope"));
        auto *summary = searchWidget->findChild<QWidget *>(QStringLiteral("searchSummary"));
        auto *summaryText = searchWidget->findChild<QLabel *>(QStringLiteral("searchSummaryText"));
        auto *unread = window.findChild<QAction *>(QStringLiteral("messageListFilterUnread"));
        auto *clearAll = window.findChild<QAction *>(QStringLiteral("action_clear_search_and_filters"));
        QVERIFY(query && scope && summary && summaryText && unread && clearAll);

        QModelIndex inbox;
        QTRY_VERIFY_WITH_TIMEOUT((inbox = findMailbox(tree->model(), QStringLiteral("INBOX"))).isValid(), 20000);
        tree->scrollTo(inbox);
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualRect(inbox).center());

        const auto rowOf = [messages](const QString &subject) {
            const QAbstractItemModel *model = messages->model();
            for (int row = 0; row < model->rowCount(); ++row) {
                const QModelIndex index = model->index(row, 0);
                if (index.data(Imap::Mailbox::RoleMessageSubject).toString() == subject)
                    return index;
            }
            return QModelIndex();
        };
        const auto currentSubject = [messages]() {
            return messages->currentIndex().data(Imap::Mailbox::RoleMessageSubject).toString();
        };
        const auto search = [&](const QString &text) {
            query->setText(text);
            emit query->returnPressed();
        };
        const QString plan = QStringLiteral("Release plan");
        const QString notes = QStringLiteral("[dev] Release notes");
        const QString lunch = QStringLiteral("Lunch on Friday");
        // Opening a message with the mouse, as the user does, and not by moving the current index of the view
        const auto click = [&](const QString &subject) {
            const QModelIndex index = rowOf(subject);
            QVERIFY(index.isValid());
            messages->scrollTo(index);
            QTest::mouseClick(messages->viewport(), Qt::LeftButton, Qt::NoModifier, messages->visualRect(index).center());
            QTRY_COMPARE(currentSubject(), subject);
        };
        QTRY_VERIFY_WITH_TIMEOUT(messages->model()->rowCount() == 4 && rowOf(plan).isValid(), 10000);
        QVERIFY(summary->isHidden());

        // A filter and a search hide the selected message; one step brings back the whole folder and the selection
        click(plan);
        unread->trigger();
        search(QStringLiteral("from:carol"));
        QTRY_VERIFY_WITH_TIMEOUT(messages->model()->rowCount() == 1 && rowOf(notes).isValid(), 10000);
        QVERIFY(!summary->isHidden());
        QCOMPARE(summaryText->text(), QStringLiteral("Search: from:carol \u00B7 In: Current Folder \u00B7 Filters: Unread"));
        QTRY_VERIFY(clearAll->isEnabled());
        clearAll->trigger();
        QVERIFY(query->text().isEmpty());
        QVERIFY(!unread->isChecked());
        QVERIFY(summary->isHidden());
        QTRY_VERIFY2_WITH_TIMEOUT(messages->model()->rowCount() == 4 && currentSubject() == plan,
                                  qPrintable(currentSubject()), 10000);
        QCOMPARE(messages->selectionModel()->selectedRows().size(), 1);

        // A message picked among the results stays selected among all messages
        search(QStringLiteral("from:carol"));
        const auto rows = [messages]() {
            QStringList subjects;
            for (int row = 0; row < messages->model()->rowCount(); ++row)
                subjects << messages->model()->index(row, 0).data(Imap::Mailbox::RoleMessageSubject).toString();
            return subjects.join(QLatin1String(" | "));
        };
        QTRY_VERIFY2_WITH_TIMEOUT(messages->model()->rowCount() == 1 && rowOf(notes).isValid(), qPrintable(rows()), 10000);
        click(notes);
        QTRY_VERIFY(clearAll->isEnabled());
        clearAll->trigger();
        QTRY_VERIFY2_WITH_TIMEOUT(messages->model()->rowCount() == 4 && currentSubject() == notes,
                                  qPrintable(currentSubject()), 10000);

        // After a search of the whole account, the folder comes back where it was; the places stay chosen
        click(lunch);
        searchWidget->setSearchPlaces(Places::Account);
        search(QStringLiteral("from:ann"));
        QTRY_COMPARE_WITH_TIMEOUT(scope->text(), QStringLiteral("Search Results"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(messages->model()->rowCount() == 2, 30000);
        QVERIFY(summaryText->text().contains(QLatin1String("In: Current Account")));
        QTRY_VERIFY(clearAll->isEnabled());
        clearAll->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(scope->text() != QStringLiteral("Search Results"), 5000);
        QTRY_VERIFY2_WITH_TIMEOUT(messages->model()->rowCount() == 4 && currentSubject() == lunch,
                                  qPrintable(currentSubject()), 10000);
        QCOMPARE(searchWidget->searchPlaces(), Places::Account);
        QVERIFY(summary->isHidden());
        QTRY_VERIFY(!clearAll->isEnabled());
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir home;
    if (!home.isValid())
        return 1;
    const QString homePath = home.path();
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
    LiveImapSearchTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_LiveImapSearch.moc"
