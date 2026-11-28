/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <KLocalizedString>

#include "Gui/InlineNotificationBar.h"
#include "Gui/MessageView.h"
#include "Gui/ShortcutHandler/ShortcutHandler.h"
#include "Gui/WebEngineMessageProfile.h"
#include "Imap/Model/FavoriteTagsModel.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Plugins/PluginManager.h"
#include "Streams/FakeSocket.h"
#include "Utils/LibMailboxSync.h"

namespace {

const char *const metadataItems = "UID FETCH 333 (" FETCH_METADATA_ITEMS ")\r\n";

/** Everything a plain text message needs before the model counts it as loaded
 */
const QByteArray metadata = "* 1 FETCH (UID 333 RFC822.SIZE 89 INTERNALDATE "
                            "\"17-Jul-2026 02:44:25 +0000\" "
                            "ENVELOPE (NIL \"Hello\" ((\"Ann\" NIL \"ann\" "
                            "\"example.org\")) NIL NIL NIL NIL NIL NIL NIL) "
                            "BODYSTRUCTURE (\"text\" \"plain\" () NIL NIL NIL "
                            "19 2 NIL NIL NIL NIL))\r\n";

} // namespace

/** @short The reader's pages and bars for messages and parts the server does
 * not send */
class MessageViewTest : public LibMailboxSync
{
    Q_OBJECT
private slots:
    void initTestCase() override;
    void init() override;
    void cleanup() override;

    void refusedMessageIsTriedAgainOnce();
    void refusedPartIsTriedAgainOnce();

private:
    /** Show folder "b" with the single message UID 333, and open it in a new
     * reader */
    void openOneMessage();
    /** Wait until the client sends @arg command, and nothing else */
    void expectCommand(const QByteArray &command);
    /** The client sends nothing for a while */
    void expectSilence();
    QWidget *unavailablePage() const;
    QString unavailableText() const;

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<QWidget> m_window;
    std::unique_ptr<QSettings> m_settings;
    Plugins::PluginManager *m_plugins = nullptr;
    Imap::Mailbox::FavoriteTagsModel *m_tags = nullptr;
    Gui::MessageView *m_view = nullptr;
    QPersistentModelIndex m_message;
};

void MessageViewTest::initTestCase()
{
    KLocalizedString::setApplicationDomain("plasma-impresario");
    LibMailboxSync::initTestCase();
}

void MessageViewTest::init()
{
    LibMailboxSync::init();
    model->setProperty("impresario-imap-delayed-fetch-part", 0);
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_settings = std::make_unique<QSettings>(m_dir->filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    m_window = std::make_unique<QWidget>();
    auto *shortcuts = new Gui::ShortcutHandler(m_window.get());
    shortcuts->defineAction(QStringLiteral("action_zoom_in"), QStringLiteral("zoom-in"), QStringLiteral("Zoom In"));
    shortcuts->defineAction(QStringLiteral("action_zoom_out"), QStringLiteral("zoom-out"), QStringLiteral("Zoom Out"));
    shortcuts->defineAction(
        QStringLiteral("action_zoom_original"), QStringLiteral("zoom-original"), QStringLiteral("Original Size"));
    m_plugins = new Plugins::PluginManager(m_window.get(), m_settings.get(), QString(), QString(), QString());
    m_tags = new Imap::Mailbox::FavoriteTagsModel(m_window.get());
    m_view = new Gui::MessageView(m_window.get(), m_settings.get(), m_plugins, m_tags);
    m_view->setProperty("impresario-auto-retry-ms", 50);
    m_window->resize(800, 600);
    m_view->resize(800, 600);
    m_window->show();
}

void MessageViewTest::cleanup()
{
    // The reader and its pages go before the model they show
    m_window.reset();
    m_view = nullptr;
    m_message = QPersistentModelIndex();
    m_plugins = nullptr;
    m_tags = nullptr;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    m_settings.reset();
    m_dir.reset();
    LibMailboxSync::cleanup();
}

void MessageViewTest::openOneMessage()
{
    helperSyncBNoMessages();
    cServer("* 1 EXISTS\r\n");
    cClient(t.mk("UID FETCH 1:* (FLAGS)\r\n"));
    cServer("* 1 FETCH (UID 333 FLAGS ())\r\n" + t.last("OK fetched\r\n"));
    m_message = msgListB.model()->index(0, 0, msgListB);
    QVERIFY(m_message.isValid());
    m_view->setMessage(m_message);
}

void MessageViewTest::expectCommand(const QByteArray &command)
{
    QByteArray written;
    QTRY_VERIFY_WITH_TIMEOUT((written += SOCK->writtenStuff()).size() >= command.size(), 15000);
    QCOMPARE(QString::fromUtf8(written), QString::fromUtf8(command));
}

void MessageViewTest::expectSilence()
{
    QTest::qWait(300);
    QCOMPARE(QString::fromUtf8(SOCK->writtenStuff()), QString());
}

QWidget *MessageViewTest::unavailablePage() const
{
    return m_view->findChild<QWidget *>(QStringLiteral("messageUnavailable"));
}

QString MessageViewTest::unavailableText() const
{
    return m_view->findChild<QLabel *>(QStringLiteral("messageUnavailableText"))->text();
}

/** @short A message whose structure the server refuses gets a page, one retry
 * of its own, then Try Again */
void MessageViewTest::refusedMessageIsTriedAgainOnce()
{
    openOneMessage();
    const QPersistentModelIndex message = m_message;
    expectCommand(t.mk(metadataItems));
    QVERIFY(!unavailablePage()->isVisible());
    cServer(t.last("NO refused\r\n"));
    QTRY_VERIFY(unavailablePage()->isVisible());
    QCOMPARE(m_view->findChild<QLabel *>(QStringLiteral("messageUnavailableTitle"))->text(),
        i18nc("@info", "This message could not be loaded"));
    QCOMPARE(unavailableText(), i18nc("@info", "The server did not send it. Trying again…"));

    // Tried once more on its own, which the server refuses again
    expectCommand(t.mk(metadataItems));
    cServer(t.last("NO still refused\r\n"));
    QTRY_VERIFY(unavailablePage()->isVisible());
    QCOMPARE(unavailableText(), i18nc("@info", "The server did not send it. Try again, or check the connection."));
    expectSilence();

    // Try Again asks once more, and the message is shown when it arrives
    auto *retry = m_view->findChild<QPushButton *>(QStringLiteral("retryMessage"));
    QVERIFY(retry->isVisible());
    retry->click();
    expectCommand(t.mk(metadataItems));
    cServer(metadata + t.last("OK fetched\r\n"));
    QVERIFY(message.data(Imap::Mailbox::RoleIsFetched).toBool());
    QTRY_VERIFY(!unavailablePage()->isVisible());
    QCOMPARE(m_view->currentMessage(), QModelIndex(message));
}

/** @short A part the server refuses is reported in a bar, tried again once on
 * its own, then offered Try Again */
void MessageViewTest::refusedPartIsTriedAgainOnce()
{
    openOneMessage();
    expectCommand(t.mk(metadataItems));
    cServer(metadata + t.last("OK fetched\r\n"));
    QTRY_VERIFY(!unavailablePage()->isVisible());

    auto *bar = m_view->findChild<Gui::InlineNotificationBar *>(QStringLiteral("partLoadErrors"));
    QVERIFY(bar);
    expectCommand(t.mk("UID FETCH 333 (BODY.PEEK[1])\r\n"));
    cServer(t.last("NO part refused\r\n"));
    QTRY_VERIFY_WITH_TIMEOUT(bar->isVisible(), 15000);
    QCOMPARE(bar->text(),
        i18ncp("@info", "Part of this message could not be loaded. Trying again…",
            "%1 parts of this message could not be loaded. Trying again…", 1));

    // Tried once more on its own, which the server refuses again
    expectCommand(t.mk("UID FETCH 333 (BODY.PEEK[1])\r\n"));
    cServer(t.last("NO part still refused\r\n"));
    QTRY_COMPARE_WITH_TIMEOUT(bar->text(),
        i18ncp(
            "@info", "Part of this message could not be loaded.", "%1 parts of this message could not be loaded.", 1),
        15000);
    QVERIFY(bar->isVisible());
    expectSilence();

    // Try Again asks once more, and the bar goes when the part arrives
    auto *retry = bar->findChild<QPushButton *>(QStringLiteral("retryParts"));
    QVERIFY(retry);
    retry->click();
    expectCommand(t.mk("UID FETCH 333 (BODY.PEEK[1])\r\n"));
    cServer("* 1 FETCH (UID 333 BODY[1] \"Hello there\")\r\n" + t.last("OK fetched\r\n"));
    QTRY_VERIFY_WITH_TIMEOUT(!bar->isVisible(), 15000);
}

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    Gui::WebEngineMessageProfile::registerScheme();
    QApplication app(argc, argv);
    MessageViewTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_MessageView.moc"
