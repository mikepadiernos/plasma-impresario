/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Gui/EnvelopeView.h"
#include "Gui/InlineNotificationBar.h"
#include "Gui/MailBoxTreeView.h"
#include "Gui/MessageListWidget.h"
#include "Gui/MsgListView.h"
#include "Gui/SavedSearchFolders.h"
#include "Gui/SearchBuilderDialog.h"
#include "Gui/SearchFoldersDialog.h"
#include "Gui/SearchIndexDialog.h"
#include "Gui/Window.h"
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/SearchIndex.h"
#include "Imap/Model/SearchIndexer.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/NetworkWatcher.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "UiUtils/MailingLists.h"
#include "UiUtils/SavedSearches.h"
#include "UiUtils/SearchScope.h"
#include "configure.cmake.h"
#ifdef TROJITA_HAVE_WEBENGINE
#include "Gui/WebEngineMessageProfile.h"
#endif
#include "static_plugins.h"

#include "Plugins/PluginManager.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
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

#include <functional>
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

    /**
     * gamma@example.test with the messages of searchesWithQuerySyntax, kept offline and indexed. The index first holds
     * only headers; the text of the messages is downloaded on request, after which the account goes offline and the
     * index answers searches of words in the text as well as of headers and flags.
     */
    void searchesIndexedMailOffline()
    {
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        settings.setValue(Common::SettingsNames::guiShowSystray, false);
        settings.setValue(Common::SettingsNames::autoMarkReadEnabled, false);
        settings.setValue(Common::SettingsNames::passwordPlugin, QStringLiteral("cleartextpassword"));
        settings.setValue(QStringLiteral("accounts/ids"), QStringList{QStringLiteral("account-1")});
        settings.setValue(QStringLiteral("accounts/active"), QStringLiteral("account-1"));
        settings.setValue(QStringLiteral("accounts/next"), 2);
        settings.setValue(QStringLiteral("accounts/names/account-1"), QStringLiteral("Gamma"));
        settings.setValue(QStringLiteral("accountPasswords/account-1/imap"), syntheticPassword);
        settings.sync();
        const QString accountFile = directory.filePath(QStringLiteral("main-account-1.conf"));
        configureAccount(accountFile, port, QStringLiteral("gamma@example.test"));
        {
            QSettings account(accountFile, QSettings::IniFormat);
            account.setValue(Common::SettingsNames::cacheOfflineKey, Common::SettingsNames::cacheOfflineAll);
            account.setValue(Common::SettingsNames::searchIndexKey, true);
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
        auto *download = window.findChild<QAction *>(QStringLiteral("action_download_search_text"));
        QVERIFY(tree && searchWidget && messages && download);
        auto *query = searchWidget->findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        auto *status = searchWidget->findChild<QLabel *>(QStringLiteral("messageListStatus"));
        QVERIFY(query && status);
        Imap::Mailbox::SearchIndexer *indexer = window.imapAccess()->searchIndexer();
        QVERIFY(indexer);
        QVERIFY2(indexer->isAvailable(), qPrintable(indexer->unavailableReason()));
        // Rather than the usual pause, which gathers a burst of changes into one pass
        indexer->setTimings(200, 20, 200);

        QModelIndex inbox;
        QTRY_VERIFY_WITH_TIMEOUT((inbox = findMailbox(tree->model(), QStringLiteral("INBOX"))).isValid(), 20000);
        tree->scrollTo(inbox);
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualRect(inbox).center());
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 4, 10000);

        const auto withText = [&]() {
            int count = 0;
            for (const auto &entry : indexer->index()->entries(QStringLiteral("INBOX")))
                count += entry.hasText ? 1 : 0;
            return count;
        };
        // Headers are indexed as they arrive; nothing extra is downloaded for the index
        QTRY_COMPARE_WITH_TIMEOUT(indexer->index()->entries(QStringLiteral("INBOX")).size(), 4, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(!indexer->isRunning(), 20000);
        QVERIFY(withText() < 4);

        // On request, after a question, the text of the rest is downloaded and indexed
        download->setData(QStringLiteral("INBOX"));
        download->setEnabled(true);
        download->trigger();
        QTRY_COMPARE_WITH_TIMEOUT(withText(), 4, 30000);

        auto *watcher = qobject_cast<Imap::Mailbox::NetworkWatcher *>(window.imapAccess()->networkWatcher());
        QVERIFY(watcher);
        watcher->setNetworkOffline();
        QTRY_VERIFY_WITH_TIMEOUT(!window.imapModel()->isNetworkAvailable(), 10000);

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
        const auto expectFound = [&](const QString &text, QStringList expected) {
            query->setText(text);
            emit query->returnPressed();
            expected.sort();
            QTRY_VERIFY2_WITH_TIMEOUT(subjects() == expected
                                          && status->text().contains(QLatin1String("indexed on this computer")),
                                      qPrintable(text + QStringLiteral(" -> ") + describe(subjects(), searchWidget)), 15000);
        };
        const QString plan = QStringLiteral("Release plan");
        const QString lunch = QStringLiteral("Lunch on Friday");
        const QString report = QStringLiteral("Quarterly report");
        const QString notes = QStringLiteral("[dev] Release notes");
        // Words of the text, headers and flags, all without a connection
        expectFound(QStringLiteral("body:noon"), {lunch});
        expectFound(QStringLiteral("text:attached"), {report});
        expectFound(QStringLiteral("from:ann"), {plan, report});
        expectFound(QStringLiteral("is:flagged"), {lunch});
        expectFound(QStringLiteral("release -from:ann"), {notes});
        expectFound(QStringLiteral("text:nothing-like-this"), {});
        QVERIFY(!status->text().contains(QLatin1String("could not be searched")));

        // The index shows which folders it covers, and INBOX is confirmed as indexed
        auto *indexAction = window.findChild<QAction *>(QStringLiteral("action_search_index_status"));
        QVERIFY(indexAction);
        indexAction->trigger();
        auto *indexDialog = window.findChild<Gui::SearchIndexDialog *>();
        QVERIFY(indexDialog);
        QCOMPARE(indexDialog->indexer(), indexer);
        const auto inboxRows = indexDialog->findChild<QTreeWidget *>(QStringLiteral("indexFolders"))
                                   ->findItems(QStringLiteral("INBOX"), Qt::MatchExactly, 0);
        QCOMPARE(inboxRows.size(), 1);
        QCOMPARE(inboxRows.first()->data(1, Qt::UserRole).toInt(),
                 int(Imap::Mailbox::SearchIndexer::FolderCoverage::State::Indexed));
        // While indexing is paused, offline results say that they may be incomplete
        auto *pause = indexDialog->findChild<QPushButton *>(QStringLiteral("indexPause"));
        pause->click();
        QVERIFY(indexer->isPaused());
        expectFound(QStringLiteral("body:noon"), {lunch});
        QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QLatin1String("results may be incomplete")), 5000);
        pause->click();
        QVERIFY(!indexer->isPaused());
        indexDialog->close();

        query->setText(QString());
        emit query->returnPressed();
        watcher->setNetworkOnline();
        QTRY_VERIFY_WITH_TIMEOUT(window.imapModel()->isNetworkAvailable(), 10000);
    }

    /**
     * gamma@example.test, whose "[dev] Release notes" names its list in List-Id "Developers
     * <dev.lists.example.test>" and offers List-Post, List-Unsubscribe and List-Archive. The list is offered as a view
     * of the loaded messages, shows the message on the server, and can be pinned as a saved search of every account;
     * the reader names the list and follows its actions only after the user confirms. Pass the slot name to run only
     * this test.
     */
    void showsMailingLists()
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
        // The second run of the query syntax test expects the release notes to stay unread
        settings.setValue(Common::SettingsNames::autoMarkReadEnabled, false);
        settings.sync();
        configureAccount(directory.filePath(QStringLiteral("main-account-1.conf")), port, QStringLiteral("gamma@example.test"));
        auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
        KConfigGroup searches(config, QStringLiteral("SavedSearches"));
        searches.deleteGroup();
        QVERIFY(config->sync());

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
        auto *menu = searchWidget->findChild<QMenu *>(QStringLiteral("mailingLists"));
        QVERIFY(query && menu);

        QModelIndex inbox;
        QTRY_VERIFY_WITH_TIMEOUT((inbox = findMailbox(tree->model(), QStringLiteral("INBOX"))).isValid(), 20000);
        tree->scrollTo(inbox);
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualRect(inbox).center());
        QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), 4, 10000);
        const auto shownSubjects = [&]() {
            QStringList result;
            for (int row = 0; row < messages->model()->rowCount(); ++row)
                result << messages->model()->index(row, 0).data(Imap::Mailbox::RoleMessageSubject).toString();
            return result;
        };
        const QString notes = QStringLiteral("[dev] Release notes");

        // Once the headers of the loaded messages arrive, their one list is offered
        const QString entry = QStringLiteral("Developers <dev.lists.example.test> (1 message)");
        const auto menuTexts = [&]() {
            QMetaObject::invokeMethod(menu, "aboutToShow");
            QStringList texts;
            for (QAction *action : menu->actions())
                texts << action->text();
            return texts;
        };
        QTRY_VERIFY2_WITH_TIMEOUT(menuTexts().value(0) == entry, qPrintable(menuTexts().join(QStringLiteral(" | "))), 10000);
        QCOMPARE(menu->actions().size(), 3);
        menu->actions().first()->trigger();
        QCOMPARE(query->text(),
                 UiUtils::fieldFilterText(UiUtils::FieldFilter::MailingList, QStringLiteral("dev.lists.example.test")));
        QTRY_VERIFY2_WITH_TIMEOUT(shownSubjects() == QStringList{notes},
                                  qPrintable(describe(shownSubjects(), searchWidget)), 10000);

        // Pinning the shown list saves a search of every account, once
        QMetaObject::invokeMethod(menu, "aboutToShow");
        QVERIFY(menu->actions().first()->isChecked());
        auto *pin = searchWidget->findChild<QAction *>(QStringLiteral("pinMailingList"));
        QVERIFY(pin && pin->isEnabled());
        pin->trigger();
        QMetaObject::invokeMethod(menu, "aboutToShow");
        searchWidget->findChild<QAction *>(QStringLiteral("pinMailingList"))->trigger();
        const auto saved = UiUtils::SavedSearches::load(Gui::MessageListWidget::savedSearchesRoot(),
                                                        {QStringLiteral("account-1")});
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.first().name, QStringLiteral("Developers"));
        QCOMPARE(saved.first().places, UiUtils::SearchScope::Places::AllAccounts);
        QTRY_VERIFY2_WITH_TIMEOUT(shownSubjects() == QStringList{notes},
                                  qPrintable(describe(shownSubjects(), searchWidget)), 10000);

        // The reader names the list, and its actions ask before following a link
        QList<QUrl> opened;
        for (auto *envelope : window.findChildren<Gui::EnvelopeView *>())
            envelope->setUrlOpener([&opened](const QUrl &url) { opened << url; });
        const QModelIndex message = messages->model()->index(0, 0);
        messages->scrollTo(message);
        QTest::mouseClick(messages->viewport(), Qt::LeftButton, Qt::NoModifier, messages->visualRect(message).center());
        Gui::EnvelopeView *envelope = nullptr;
        const auto shownEnvelope = [&]() -> Gui::EnvelopeView * {
            for (auto *view : window.findChildren<Gui::EnvelopeView *>()) {
                if (view->isVisible() && !view->listActions().isEmpty())
                    return view;
            }
            return nullptr;
        };
        QTRY_VERIFY_WITH_TIMEOUT((envelope = shownEnvelope()), 10000);
        QLabel *row = nullptr;
        for (auto *label : envelope->findChildren<QLabel *>(QStringLiteral("envelopeMailingList"))) {
            if (label->isVisible())
                row = label;
        }
        QVERIFY(row);
        QVERIFY2(row->text().contains(QStringLiteral("Developers (dev.lists.example.test)")), qPrintable(row->text()));
        int unsubscribe = -1;
        const auto actions = envelope->listActions();
        QCOMPARE(actions.size(), 3);
        for (qsizetype i = 0; i < actions.size(); ++i) {
            if (actions.at(i).action == UiUtils::MailingLists::Action::Unsubscribe)
                unsubscribe = int(i);
        }
        QVERIFY(unsubscribe >= 0);
        const QUrl leave(QStringLiteral("mailto:dev-leave@lists.example.test"));
        QCOMPARE(actions.at(unsubscribe).url, leave);
        emit row->linkActivated(QStringLiteral("list-action:%1").arg(unsubscribe));
        auto *box = envelope->findChild<QMessageBox *>(QStringLiteral("listActionConfirmation"));
        QVERIFY(box);
        QVERIFY(opened.isEmpty());
        box->findChild<QPushButton *>(QStringLiteral("followListAction"))->click();
        QCOMPARE(opened, QList<QUrl>{leave});

        searches.deleteGroup();
        QVERIFY(config->sync());
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

    /** Uses alpha's and beta's invoices and gamma's INBOX, as above; gamma's flags are left as they were */
    void showsSavedSearchesAsVirtualFolders()
    {
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
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
        settings.setValue(QStringLiteral("accounts/names/account-3"), QStringLiteral("Gamma"));
        for (const QString &id : {QStringLiteral("account-1"), QStringLiteral("account-2"), QStringLiteral("account-3")})
            settings.setValue(QStringLiteral("accountPasswords/%1/imap").arg(id), syntheticPassword);
        settings.setValue(Common::SettingsNames::autoMarkReadEnabled, false);
        settings.sync();
        configureAccount(directory.filePath(QStringLiteral("main-account-1.conf")), port, QStringLiteral("alpha@example.test"));
        configureAccount(directory.filePath(QStringLiteral("main-account-2.conf")), port, QStringLiteral("beta@example.test"));
        configureAccount(directory.filePath(QStringLiteral("main-account-3.conf")), port, QStringLiteral("gamma@example.test"));
        auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
        KConfigGroup searches(config, QStringLiteral("SavedSearches"));
        searches.deleteGroup();
        KConfigGroup counts(KSharedConfig::openStateConfig(), QStringLiteral("SavedSearchCounts"));
        counts.deleteGroup();
        QVERIFY(config->sync());

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
        auto *panel = window.findChild<Gui::SavedSearchFolders *>();
        auto *markAsRead = window.findChild<QAction *>(QStringLiteral("action_mark_as_read"));
        QVERIFY(switcher && tree && searchWidget && messages && panel && markAsRead);
        auto *query = searchWidget->findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        auto *status = searchWidget->findChild<QLabel *>(QStringLiteral("messageListStatus"));
        auto *view = panel->findChild<QTreeWidget *>(QStringLiteral("savedSearchFolders"));
        QVERIFY(query && status && view);
        QVERIFY(panel->isHidden());

        // A message count of zero waits for any message; an earlier test may have delivered more to alpha
        const auto openInbox = [&](int messageCount) {
            QModelIndex inbox;
            QTRY_VERIFY_WITH_TIMEOUT((inbox = findMailbox(tree->model(), QStringLiteral("INBOX"))).isValid(), 20000);
            tree->scrollTo(inbox);
            QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualRect(inbox).center());
            if (messageCount)
                QTRY_COMPARE_WITH_TIMEOUT(messages->model()->rowCount(), messageCount, 10000);
            else
                QTRY_VERIFY_WITH_TIMEOUT(messages->model()->rowCount() > 0, 10000);
        };
        const auto switchTo = [&](int index) {
            switcher->setCurrentIndex(index);
            emit switcher->activated(index);
        };
        // Gamma's INBOX is the folder which its search of the shown folder looks in
        switchTo(2);
        openInbox(4);
        switchTo(0);
        openInbox(0);

        const QString alphaKey = window.imapAccess()->credentialAccountId();
        KConfigGroup everywhere = searches.group(alphaKey).group(QStringLiteral("everywhere"));
        everywhere.writeEntry("Name", QStringLiteral("Invoices everywhere"));
        everywhere.writeEntry("Query", QStringLiteral("invoice"));
        everywhere.writeEntry("Subject", true);
        everywhere.writeEntry("Places", QStringLiteral("all"));
        KConfigGroup releases = searches.group(QStringLiteral("account-3")).group(QStringLiteral("releases"));
        releases.writeEntry("Name", QStringLiteral("Releases"));
        releases.writeEntry("Query", QStringLiteral("release"));
        releases.writeEntry("Subject", true);
        QVERIFY(config->sync());
        emit window.accountsChanged();
        QVERIFY(!panel->isHidden());
        QCOMPARE(view->topLevelItemCount(), 2);
        QTreeWidgetItem *everywhereItem = view->topLevelItem(0);
        QTreeWidgetItem *releasesItem = view->topLevelItem(1);
        QCOMPARE(everywhereItem->text(0), QStringLiteral("Invoices everywhere"));
        QCOMPARE(releasesItem->text(0), QStringLiteral("Releases"));
        QVERIFY(everywhereItem->text(1).isEmpty());

        const auto clickSearch = [&](QTreeWidgetItem *item) {
            view->scrollToItem(item);
            QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->visualItemRect(item).center());
        };
        const auto unreadShown = [messages]() {
            int unread = 0;
            const std::function<void(const QModelIndex &)> walk = [&](const QModelIndex &parent) {
                for (int row = 0; row < messages->model()->rowCount(parent); ++row) {
                    const QModelIndex index = messages->model()->index(row, 0, parent);
                    const QVariant read = index.data(Imap::Mailbox::RoleMessageIsMarkedRead);
                    if (read.isValid() && !read.toBool())
                        ++unread;
                    walk(index);
                }
            };
            walk(QModelIndex());
            return unread;
        };
        const auto storedCounts = [&counts](const QString &owner, const QString &id) {
            return counts.readEntry(UiUtils::SavedSearches::countsKey(owner, id), QList<int>());
        };

        // A search of every account counts what it found once every folder was searched
        clickSearch(everywhereItem);
        QStringList rows;
        QTRY_VERIFY2_WITH_TIMEOUT((rows = shownRows(messages->model())).size() == 5 && status->text().contains(QLatin1String("5 messages found")),
                                  qPrintable(describe(rows, searchWidget)), 30000);
        QCOMPARE(query->text(), QStringLiteral("invoice"));
        QCOMPARE(view->currentItem(), everywhereItem);
        const int invoicesUnread = unreadShown();
        QTRY_COMPARE_WITH_TIMEOUT(everywhereItem->text(1), QStringLiteral("%1 / 5").arg(invoicesUnread), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(storedCounts(alphaKey, QStringLiteral("everywhere")), (QList<int>{5, invoicesUnread}), 5000);

        // A search of the shown folder of another account switches to it
        clickSearch(releasesItem);
        QTRY_COMPARE_WITH_TIMEOUT(window.imapAccess()->credentialAccountId(), QStringLiteral("account-3"), 5000);
        QTRY_VERIFY2_WITH_TIMEOUT((rows = shownRows(messages->model())).size() == 2, qPrintable(describe(rows, searchWidget)), 10000);
        QCOMPARE(query->text(), QStringLiteral("release"));
        QCOMPARE(view->currentItem(), releasesItem);
        QTRY_COMPARE_WITH_TIMEOUT(releasesItem->text(1), QStringLiteral("1 / 2"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(storedCounts(QStringLiteral("account-3"), QStringLiteral("releases")), (QList<int>{2, 1}), 5000);

        // Reading a message found changes the count while the virtual folder is shown
        QModelIndex notes;
        for (int row = 0; row < messages->model()->rowCount(); ++row) {
            const QModelIndex index = messages->model()->index(row, 0);
            if (index.data(Imap::Mailbox::RoleMessageSubject).toString() == QLatin1String("[dev] Release notes"))
                notes = index;
        }
        QVERIFY(notes.isValid());
        messages->scrollTo(notes);
        QTest::mouseClick(messages->viewport(), Qt::LeftButton, Qt::NoModifier, messages->visualRect(notes).center());
        QTRY_COMPARE(messages->currentIndex().data(Imap::Mailbox::RoleMessageSubject).toString(), QStringLiteral("[dev] Release notes"));
        QVERIFY(!markAsRead->isChecked());
        markAsRead->trigger();
        QTRY_COMPARE_WITH_TIMEOUT(releasesItem->text(1), QStringLiteral("0 / 2"), 10000);
        QVERIFY(!releasesItem->font(0).bold());
        QTRY_VERIFY(markAsRead->isChecked());
        markAsRead->trigger();
        QTRY_COMPARE_WITH_TIMEOUT(releasesItem->text(1), QStringLiteral("1 / 2"), 10000);
        QVERIFY(releasesItem->font(0).bold());
        QTRY_COMPARE_WITH_TIMEOUT(storedCounts(QStringLiteral("account-3"), QStringLiteral("releases")), (QList<int>{2, 1}), 5000);

        // Choosing a folder leaves the virtual folder and shows the whole folder
        openInbox(4);
        QVERIFY(query->text().isEmpty());
        QVERIFY(searchWidget->activeSavedSearchId().isEmpty());
        QVERIFY(!view->currentItem());

        // The counts stay known, as when the application starts again
        emit window.accountsChanged();
        QCOMPARE(view->topLevelItem(0)->text(1), QStringLiteral("%1 / 5").arg(invoicesUnread));
        QCOMPARE(view->topLevelItem(1)->text(1), QStringLiteral("1 / 2"));

        // A chosen folder which the server does not list is reported once the account's folders were listed
        KConfigGroup chosen = searches.group(alphaKey).group(QStringLiteral("chosen"));
        chosen.writeEntry("Name", QStringLiteral("Chosen folders"));
        chosen.writeEntry("Query", QStringLiteral("invoice"));
        chosen.writeEntry("Subject", true);
        chosen.writeEntry("Places", QStringLiteral("folders"));
        chosen.writeEntry("Folders", QStringList{QStringLiteral("account-1/INBOX"), QStringLiteral("account-1/NoSuchFolder"),
                                                 QStringLiteral("account-3/INBOX")});
        QVERIFY(config->sync());
        emit window.accountsChanged();
        QCOMPARE(view->topLevelItemCount(), 3);
        QTRY_COMPARE_WITH_TIMEOUT(panel->problems(alphaKey, QStringLiteral("chosen")).missingFolders,
                                  QStringList{QStringLiteral("account-1/NoSuchFolder")}, 10000);
        const auto chosenItem = [view]() { return view->findItems(QStringLiteral("Chosen folders"), Qt::MatchExactly).value(0); };
        QVERIFY(chosenItem());
        QVERIFY(chosenItem()->toolTip(0).contains(QStringLiteral("NoSuchFolder")));
        QVERIFY(chosenItem()->data(0, Qt::AccessibleTextRole).toString().endsWith(QLatin1String(", needs attention")));
        // The search is kept as it was until the user decides
        QCOMPARE(chosen.readEntry("Folders", QStringList()).size(), 3);
        panel->menuFor(chosenItem());
        auto *removeMissing = panel->findChild<QAction *>(QStringLiteral("savedSearchRemoveMissing"));
        QVERIFY(removeMissing && removeMissing->isVisible());
        removeMissing->trigger();
        QCOMPARE(chosen.readEntry("Folders", QStringList()),
                 (QStringList{QStringLiteral("account-1/INBOX"), QStringLiteral("account-3/INBOX")}));
        QVERIFY(panel->problems(alphaKey, QStringLiteral("chosen")).isEmpty());

        // Editing previews the changed search in the list before it is saved
        panel->menuFor(chosenItem());
        panel->findChild<QAction *>(QStringLiteral("savedSearchEdit"))->trigger();
        QCOMPARE(searchWidget->editedSavedSearchId(), QStringLiteral("chosen"));
        QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QLatin1String("found")), 30000);
        query->setText(QStringLiteral("release"));
        QMetaObject::invokeMethod(searchWidget, "slotApplySearch");
        QTRY_VERIFY2_WITH_TIMEOUT((rows = shownRows(messages->model())).size() >= 2
                                      && rows.filter(QStringLiteral("Release notes")).size() >= 1,
                                  qPrintable(describe(rows, searchWidget)), 30000);
        QCOMPARE(chosen.readEntry("Query", QString()), QStringLiteral("invoice"));
        auto *save = searchWidget->findChild<QPushButton *>(QStringLiteral("saveSavedSearchEdit"));
        QVERIFY(save && save->isVisible());
        save->click();
        QVERIFY(searchWidget->editedSavedSearchId().isEmpty());
        QCOMPARE(KConfigGroup(KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")), QStringLiteral("SavedSearches"))
                     .group(alphaKey).group(QStringLiteral("chosen")).readEntry("Query", QString()),
                 QStringLiteral("release"));
        QCOMPARE(searchWidget->activeSavedSearchId(), QStringLiteral("chosen"));
        QTRY_COMPARE_WITH_TIMEOUT(view->currentItem(), chosenItem(), 5000);

        searches.deleteGroup();
        counts.deleteGroup();
        QVERIFY(config->sync());
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
