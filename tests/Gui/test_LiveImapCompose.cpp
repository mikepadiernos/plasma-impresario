/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/Paths.h"
#include "Common/PrivateFiles.h"
#include "Common/SettingsNames.h"
#include "Composer/SenderIdentitiesModel.h"
#include "Gui/AttachmentPanel.h"
#include "Gui/CompleteMessageWidget.h"
#include "Gui/ComposeWidget.h"
#include "Gui/EmbeddedWebView.h"
#include "Gui/InlineNotificationBar.h"
#include "Gui/MailBoxTreeView.h"
#include "Gui/MessageDetailsPanel.h"
#include "Gui/MsgListView.h"
#include "Gui/ReaderActionBar.h"
#include "IPC/MainWindowBridge.h"
#include "Gui/Window.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "Imap/Model/Utils.h"
#include "configure.cmake.h"
#ifdef TROJITA_HAVE_WEBENGINE
#include "Gui/WebEngineMessageProfile.h"
#endif
#include "static_plugins.h"

#include <KLocalizedString>
#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTextEdit>
#include <QTimer>
#include <QUrlQuery>
#include <QWebEnginePage>
#include <algorithm>
#include <memory>

namespace {

// Synthetic credentials for a disposable loopback server; never point this test at a real account.
const QString syntheticPassword = QStringLiteral("synthetic-test-only");
const QString userAddress = QStringLiteral("panels@example.test");

QModelIndex findMailbox(const QAbstractItemModel *model, const QString &name)
{
    if (!model || model->rowCount() == 0)
        return {};
    const QModelIndexList matches = model->match(model->index(0, 0), Imap::Mailbox::RoleMailboxName, name, 1, Qt::MatchExactly);
    return matches.isEmpty() ? QModelIndex() : QModelIndex(matches.first());
}

QModelIndex findMessage(Gui::MsgListView *view, const QString &subject)
{
    QAbstractItemModel *model = view->model();
    if (!model || model->rowCount() == 0)
        return {};
    const QModelIndexList matches = model->match(model->index(0, 0), Imap::Mailbox::RoleMessageSubject, subject, 1,
                                                 Qt::MatchExactly | Qt::MatchRecursive);
    return matches.isEmpty() ? QModelIndex() : QModelIndex(matches.first());
}

/** The number of messages which the server stores in a Maildir folder, such as ".Sent" */
int storedMessages(const QString &folder)
{
    const QDir maildir(qEnvironmentVariable("PLASMA_TEST_MAILDIR") + QLatin1Char('/') + folder);
    return int(QDir(maildir.filePath(QStringLiteral("cur"))).entryList(QDir::Files).size()
               + QDir(maildir.filePath(QStringLiteral("new"))).entryList(QDir::Files).size());
}

/** The newest message which the server stores in a Maildir folder */
QByteArray newestStoredMessage(const QString &folder)
{
    const QDir maildir(qEnvironmentVariable("PLASMA_TEST_MAILDIR") + QLatin1Char('/') + folder);
    QFileInfoList files = QDir(maildir.filePath(QStringLiteral("cur"))).entryInfoList(QDir::Files)
                          + QDir(maildir.filePath(QStringLiteral("new"))).entryInfoList(QDir::Files);
    if (files.isEmpty())
        return {};
    std::sort(files.begin(), files.end(), [](const QFileInfo &a, const QFileInfo &b) {
        return a.lastModified() == b.lastModified() ? a.fileName() < b.fileName() : a.lastModified() < b.lastModified();
    });
    QFile file(files.last().filePath());
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QStringList autosavedDrafts()
{
    return QDir(Common::writablePath(Common::LOCATION_CACHE) + QLatin1String("Drafts/"))
        .entryList({QStringLiteral("*.draft")}, QDir::Files);
}

/** A profile with one account which files sent mail in "Sent" and sends through a script */
class Profile
{
public:
    Profile(QTemporaryDir &directory, int port)
        : m_directory(directory)
        , settings(directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat)
    {
        settings.setValue(Common::SettingsNames::guiShowSystray, false);
        settings.setValue(Common::SettingsNames::passwordPlugin, QStringLiteral("cleartextpassword"));
        settings.setValue(QStringLiteral("accounts/ids"), QStringList{QStringLiteral("account-1")});
        settings.setValue(QStringLiteral("accounts/active"), QStringLiteral("account-1"));
        settings.setValue(QStringLiteral("accounts/next"), 2);
        settings.setValue(QStringLiteral("accounts/names/account-1"), QStringLiteral("Panels"));
        settings.setValue(QStringLiteral("accountPasswords/account-1/imap"), syntheticPassword);
        settings.sync();

        // The script refuses to send while the "fail" file exists, and keeps the last message it sent.
        const QString script = directory.filePath(QStringLiteral("sendmail.sh"));
        QFile file(script);
        if (file.open(QIODevice::WriteOnly)) {
            file.write("if [ -e '" + failFile().toUtf8() + "' ]; then echo 'synthetic refusal' >&2; exit 75; fi\n"
                       "printf '%s\\n' \"$@\" > '" + argumentsFile().toUtf8() + "'\n"
                       "cat > '" + sentFile().toUtf8() + "'\n");
        }
        file.close();

        QSettings account(directory.filePath(QStringLiteral("main-account-1.conf")), QSettings::IniFormat);
        account.setValue(Common::SettingsNames::imapMethodKey, Common::SettingsNames::methodSSL);
        account.setValue(Common::SettingsNames::imapHostKey, QStringLiteral("127.0.0.1"));
        account.setValue(Common::SettingsNames::imapPortKey, port);
        account.setValue(Common::SettingsNames::imapUserKey, userAddress);
        account.setValue(Common::SettingsNames::imapStartMode, Common::SettingsNames::netOnline);
        account.setValue(Common::SettingsNames::imapNeedsNetwork, false);
        account.setValue(Common::SettingsNames::imapUseSystemProxy, false);
        account.setValue(Common::SettingsNames::cacheOfflineKey, Common::SettingsNames::cacheOfflineNone);
        account.setValue(Common::SettingsNames::msaMethodKey, Common::SettingsNames::methodSENDMAIL);
        account.setValue(Common::SettingsNames::sendmailKey, QString(QStringLiteral("/bin/sh ") + script));
        account.setValue(Common::SettingsNames::composerSaveToImapKey, true);
        account.setValue(Common::SettingsNames::composerImapSentKey, QStringLiteral("Sent"));
        account.sync();
    }

    QString failFile() const { return m_directory.filePath(QStringLiteral("fail")); }
    QString sentFile() const { return m_directory.filePath(QStringLiteral("sent.eml")); }
    QString argumentsFile() const { return m_directory.filePath(QStringLiteral("sendmail.args")); }

    /** The command-line arguments of the last send, which end with the recipients */
    QStringList lastArguments() const
    {
        QFile file(argumentsFile());
        return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts)
                                              : QStringList();
    }

    QByteArray lastSent() const
    {
        QFile file(sentFile());
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    void setFailing(bool failing) const
    {
        if (failing) {
            QFile file(failFile());
            QVERIFY(file.open(QIODevice::WriteOnly));
        } else {
            QFile::remove(failFile());
        }
    }

private:
    QTemporaryDir &m_directory;

public:
    QSettings settings;
};

/** Answers the dialogs which a test cannot click through itself */
class Dialogs : public QObject
{
public:
    QStringList errors;
    int passwordPrompts = 0;
    /** The button for "Save Draft?" */
    QMessageBox::StandardButton draftAnswer = QMessageBox::Discard;
    /** The file to choose in the next file dialog */
    QString nextFile;

    Dialogs()
    {
        connect(&m_timer, &QTimer::timeout, this, [this] { answer(); });
        m_timer.start(20);
    }

private:
    void answer()
    {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (!widget->isVisible())
                continue;
            if (auto *message = qobject_cast<QMessageBox *>(widget)) {
                const auto buttons = message->standardButtons();
                if (buttons == (QMessageBox::Yes | QMessageBox::No)) {
                    // The certificate of the synthetic server
                    message->button(QMessageBox::Yes)->click();
                } else if (buttons & QMessageBox::Discard) {
                    message->button(draftAnswer)->click();
                } else {
                    errors << message->text();
                    message->reject();
                }
            } else if (auto *files = qobject_cast<QFileDialog *>(widget)) {
                if (nextFile.isEmpty()) {
                    files->reject();
                } else {
                    // Choose the folder, then type the name over the suggested one
                    const QFileInfo chosen(nextFile);
                    files->setDirectory(chosen.absolutePath());
                    auto *name = files->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"));
                    if (name)
                        name->setText(chosen.fileName());
                    else
                        files->selectFile(nextFile);
                    nextFile.clear();
                    // QFileDialog::accept() is protected; it still runs through the QDialog interface
                    static_cast<QDialog *>(files)->accept();
                }
            } else if (auto *dialog = qobject_cast<QDialog *>(widget);
                       dialog && dialog->findChild<QLineEdit *>(QStringLiteral("passwordLineEdit"))) {
                ++passwordPrompts;
                dialog->reject();
            }
        }
    }

    QTimer m_timer;
};

QPointer<Gui::ComposeWidget> visibleComposer()
{
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (auto *composer = qobject_cast<Gui::ComposeWidget *>(widget); composer && composer->isVisible())
            return composer;
    }
    return {};
}

/** The kind of each address field of a composer, such as Composer::ADDRESS_BCC, from the top */
QList<int> recipientKinds(Gui::ComposeWidget *composer)
{
    QList<int> result;
    auto *envelope = composer->findChild<QFormLayout *>(QStringLiteral("envelopeLayout"));
    if (!envelope)
        return result;
    for (int row = 0; row < envelope->rowCount(); ++row) {
        QLayoutItem *item = envelope->itemAt(row, QFormLayout::LabelRole);
        auto *combo = item ? qobject_cast<QComboBox *>(item->widget()) : nullptr;
        if (combo && combo != composer->findChild<QComboBox *>(QStringLiteral("sender")))
            result << combo->currentData().toInt();
    }
    return result;
}

/** The address fields of a composer, from the top */
QList<QLineEdit *> recipientFields(Gui::ComposeWidget *composer)
{
    QList<QLineEdit *> result;
    auto *envelope = composer->findChild<QFormLayout *>(QStringLiteral("envelopeLayout"));
    if (!envelope)
        return result;
    for (int row = 0; row < envelope->rowCount(); ++row) {
        QLayoutItem *item = envelope->itemAt(row, QFormLayout::FieldRole);
        auto *edit = item ? qobject_cast<QLineEdit *>(item->widget()) : nullptr;
        if (edit && edit->objectName() != QLatin1String("subject"))
            result << edit;
    }
    return result;
}

bool addIdentity(Gui::MainWindow &window)
{
    return window.withAccount(QStringLiteral("account-1"), [&]() {
        if (window.senderIdentitiesModel()->rowCount() == 0)
            window.senderIdentitiesModel()->appendIdentity(
                Composer::ItemSenderIdentity(QStringLiteral("Panels User"), userAddress, QString(), QString()));
    });
}

/** The unfolded header field NAME of a message, such as "Cc: a@example.test, b@example.test" */
QString headerLine(const QByteArray &message, const QByteArray &name)
{
    QByteArray field;
    for (const QByteArray &line : message.split('\n')) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.isEmpty())
            break;
        if (!field.isEmpty()) {
            if (line.startsWith(' ') || line.startsWith('\t')) {
                field += ' ' + trimmed;
                continue;
            }
            break;
        }
        if (line.startsWith(QByteArray(name + ": ")))
            field = trimmed;
    }
    return QString::fromUtf8(field);
}

/** A mailto: URL built the way KIO's KEMailClientLauncherJob hands files from a file manager to a mail client */
QUrl launcherUrl(const QString &query, const QStringList &attachments)
{
    QUrl url(QStringLiteral("mailto:carol@example.test"));
    QUrlQuery items(query);
    for (const QString &attachment : attachments) {
        const QString value = attachment.startsWith(QLatin1Char('/')) ? QUrl::fromLocalFile(attachment).toString() : attachment;
        items.addQueryItem(QStringLiteral("attach"), QString::fromUtf8(QUrl::toPercentEncoding(value)));
    }
    url.setQuery(items);
    return url;
}

/** The names of the files attached in a composer */
QStringList attachmentNames(Gui::ComposeWidget *composer)
{
    QStringList result;
    auto *view = composer->findChild<QAbstractItemView *>(QStringLiteral("attachmentsView"));
    if (!view || !view->model())
        return result;
    for (int row = 0; row < view->model()->rowCount(); ++row)
        result << view->model()->index(row, 0).data().toString();
    return result;
}

/** Serves files over HTTP on the loopback interface, standing in for a file server on another computer */
class FileServer : public QObject
{
public:
    /** The content of each path; others are not found */
    QHash<QByteArray, QByteArray> files;
    /** A path whose response starts and then stalls, so that fetching it can be cancelled */
    QByteArray stalledPath;
    /** The requested paths, in order */
    QList<QByteArray> requests;

    bool listen()
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket *socket = m_server.nextPendingConnection()) {
                socket->setParent(this);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { handle(socket); });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
        return m_server.listen(QHostAddress::LocalHost);
    }

    QString url(const QString &userInfo, const QString &path) const
    {
        return QLatin1String("http://") + userInfo + QLatin1String("127.0.0.1:") + QString::number(m_server.serverPort()) + path;
    }

private:
    void handle(QTcpSocket *socket)
    {
        QByteArray &buffer = m_buffers[socket];
        buffer += socket->readAll();
        if (!buffer.contains("\r\n\r\n"))
            return;
        const QByteArray path = buffer.split(' ').value(1).split('?').value(0);
        m_buffers.remove(socket);
        requests << path;
        if (path == stalledPath) {
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: 1000000\r\n\r\nstarted");
            return;
        }
        const auto file = files.constFind(path);
        if (file == files.constEnd()) {
            socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        } else {
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + QByteArray::number(file->size())
                          + "\r\nConnection: close\r\n\r\n" + *file);
        }
        socket->disconnectFromHost();
    }

    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
};

/** The entries in the folder of files fetched to be attached */
QStringList fetchedCopies()
{
    return QDir(Common::PrivateFiles::fetchedAttachmentsFolder()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
}

bool writeFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

}

class LiveImapComposeTest : public QObject
{
    Q_OBJECT
private slots:
    /**
     * Expects a loopback IMAPS server on PLASMA_TEST_IMAPS_PORT with the user panels@example.test, using the synthetic
     * password, whose INBOX holds the messages of tools/dev-container/seed-panel-mail.py and which has a Sent folder.
     * PLASMA_TEST_MAILDIR is that user's Maildir. See tools/dev-container/live-imap-compose.sh.
     */
    void readingFlowToResponses()
    {
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Profile profile(directory, port);
        Dialogs dialogs;

        Gui::MainWindow window(&profile.settings);
        window.resize(1400, 900);
        window.show();
        QVERIFY(addIdentity(window));
        auto *tree = window.findChild<Gui::MailBoxTreeView *>();
        auto *messages = window.findChild<Gui::MsgListView *>(QStringLiteral("messageListView"));
        auto *details = window.findChild<Gui::MessageDetailsPanel *>();
        auto *attachments = window.findChild<Gui::AttachmentPanel *>();
        auto *reader = window.findChild<Gui::CompleteMessageWidget *>();
        auto *bar = window.findChild<Gui::ReaderActionBar *>(QStringLiteral("readerActions"));
        QVERIFY(tree && messages && details && attachments && reader && bar);
        const int sentBefore = storedMessages(QStringLiteral(".Sent"));

        // Choose the folder and the message with the mouse, as a user would
        QModelIndex inbox;
        QTRY_VERIFY_WITH_TIMEOUT((inbox = findMailbox(tree->model(), QStringLiteral("INBOX"))).isValid(), 20000);
        tree->scrollTo(inbox);
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualRect(inbox).center());
        QModelIndex plan;
        QTRY_VERIFY_WITH_TIMEOUT((plan = findMessage(messages, QStringLiteral("Quarterly plan"))).isValid(), 20000);
        messages->expandAll();
        messages->scrollTo(plan);
        QTest::mouseClick(messages->viewport(), Qt::LeftButton, Qt::NoModifier,
                          messages->visualRect(plan).intersected(messages->viewport()->rect()).center());
        QTRY_COMPARE_WITH_TIMEOUT(details->message().data(Imap::Mailbox::RoleMessageSubject).toString(),
                                  QStringLiteral("Quarterly plan"), 10000);
        const QPersistentModelIndex original = Imap::deproxifiedIndex(details->message());
        QVERIFY(original.isValid());
        QVERIFY(!original.data(Imap::Mailbox::RoleMessageIsMarkedReplied).toBool());
        QVERIFY(!original.data(Imap::Mailbox::RoleMessageIsMarkedForwarded).toBool());

        // The reader shows the body, and the attachments panel lists both files
        const auto shownText = std::make_shared<QString>();
        const auto readShown = [&]() {
            for (auto *view : reader->findChildren<Gui::EmbeddedWebView *>()) {
                if (view->isVisible())
                    view->page()->toPlainText([shownText](const QString &text) { *shownText += text; });
            }
            return *shownText;
        };
        QTRY_VERIFY_WITH_TIMEOUT(readShown().contains(QLatin1String("The draft plan and the budget figures are attached")),
                                 15000);
        QTRY_COMPARE_WITH_TIMEOUT(attachments->attachmentCount(), 2, 15000);
        QTRY_COMPARE_WITH_TIMEOUT(bar->attachmentCount(), 2, 15000);

        // Saving an attachment writes the decoded file
        auto *list = qobject_cast<QListWidget *>(attachments->listView());
        QVERIFY(list);
        const auto budget = list->findItems(QStringLiteral("budget.csv"), Qt::MatchContains);
        QCOMPARE(budget.size(), 1);
        list->setCurrentItem(budget.first());
        const QString saved = directory.filePath(QStringLiteral("budget.csv"));
        dialogs.nextFile = saved;
        attachments->saveCurrent();
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(saved).size() > 0, 15000);
        {
            QFile file(saved);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), QByteArray("quarter,amount\nQ1,1200\nQ2,1350\nQ3,990\nQ4,1500\n"));
        }

        // Only after it was saved, the file can be shown in the file manager, which selects it in its folder
        auto *notice = window.findChild<Gui::InlineNotificationBar *>(QStringLiteral("messageNotificationBar"));
        QVERIFY(notice);
        QList<QUrl> shown;
        QString launchError;
        window.setFileManagerLauncher([&](const QUrl &url, const std::function<void(const QString &)> &failed) {
            shown << url;
            if (!launchError.isEmpty())
                failed(launchError);
        });
        const auto showInFolder = [notice]() -> QPushButton * {
            const auto buttons = notice->actionButtons();
            for (QPushButton *button : buttons) {
                if (button->objectName() == QLatin1String("showSavedFileInFolder") && button->isVisible())
                    return button;
            }
            return nullptr;
        };
        QTRY_VERIFY_WITH_TIMEOUT(notice->isVisible() && showInFolder(), 5000);
        QCOMPARE(notice->kind(), Gui::InlineNotificationBar::Kind::Info);
        QVERIFY2(notice->text().contains(QLatin1String("budget.csv")), qPrintable(notice->text()));
        QVERIFY(shown.isEmpty());
        showInFolder()->click();
        QCOMPARE(shown, QList<QUrl>{QUrl::fromLocalFile(QFileInfo(saved).absoluteFilePath())});

        // A file manager which cannot be reached is reported
        launchError = QStringLiteral("The name org.freedesktop.FileManager1 was not provided by any .service files");
        notice->dismiss();
        // Saved anew rather than over itself, which would ask to replace it
        QVERIFY(QFile::remove(saved));
        dialogs.nextFile = saved;
        attachments->saveCurrent();
        QTRY_VERIFY_WITH_TIMEOUT(notice->isVisible() && showInFolder(), 15000);
        showInFolder()->click();
        QCOMPARE(shown.size(), 2);
        QCOMPARE(notice->kind(), Gui::InlineNotificationBar::Kind::Error);
        QVERIFY2(notice->text().contains(QLatin1String("FileManager1")), qPrintable(notice->text()));

        // A file removed since is reported instead of being shown
        launchError.clear();
        notice->dismiss();
        // Saved anew rather than over itself, which would ask to replace it
        QVERIFY(QFile::remove(saved));
        dialogs.nextFile = saved;
        attachments->saveCurrent();
        QTRY_VERIFY_WITH_TIMEOUT(notice->isVisible() && showInFolder(), 15000);
        QVERIFY(QFile::remove(saved));
        showInFolder()->click();
        QCOMPARE(shown.size(), 2);
        QCOMPARE(notice->kind(), Gui::InlineNotificationBar::Kind::Error);
        QVERIFY2(notice->text().contains(QLatin1String("moved or deleted")), qPrintable(notice->text()));
        notice->dismiss();
        window.setFileManagerLauncher({});

        // Reply to all but me: the sender in To, the other recipient in Cc, the original quoted
        auto *replyAll = window.findChild<QAction *>(QStringLiteral("action_reply_all_but_me"));
        QVERIFY(replyAll);
        QTRY_VERIFY_WITH_TIMEOUT(replyAll->isEnabled(), 10000);
        replyAll->trigger();
        QPointer<Gui::ComposeWidget> composer;
        QTRY_VERIFY_WITH_TIMEOUT((composer = visibleComposer()), 15000);
        QCOMPARE(composer->findChild<QLineEdit *>(QStringLiteral("subject"))->text(), QStringLiteral("Re: Quarterly plan"));
        auto *body = composer->findChild<QTextEdit *>(QStringLiteral("mailText"));
        QVERIFY(body);
        QVERIFY2(body->toPlainText().contains(QLatin1String("> The draft plan")), qPrintable(body->toPlainText()));
        body->append(QStringLiteral("Synthetic reply to all."));
        QVERIFY(QMetaObject::invokeMethod(composer, "send"));
        QTRY_VERIFY2_WITH_TIMEOUT(!composer, qPrintable(dialogs.errors.join(QLatin1Char('\n'))), 20000);
        QVERIFY2(dialogs.errors.isEmpty(), qPrintable(dialogs.errors.join(QLatin1Char('\n'))));
        QByteArray sent = profile.lastSent();
        QVERIFY2(headerLine(sent, "From").contains(userAddress), sent.constData());
        QVERIFY2(headerLine(sent, "To").contains(QLatin1String("alice@example.test")), sent.constData());
        QVERIFY2(headerLine(sent, "Cc").contains(QLatin1String("bob@example.test")), sent.constData());
        QVERIFY2(!headerLine(sent, "Cc").contains(userAddress), sent.constData());
        QCOMPARE(headerLine(sent, "In-Reply-To"), QStringLiteral("In-Reply-To: <plan-1@example.test>"));
        QVERIFY2(sent.contains("Synthetic reply to all."), sent.constData());
        QTRY_VERIFY_WITH_TIMEOUT(original.data(Imap::Mailbox::RoleMessageIsMarkedReplied).toBool(), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(storedMessages(QStringLiteral(".Sent")), sentBefore + 1, 10000);

        // Forward the message, attachments included, as an attached message
        auto *forward = window.findChild<QAction *>(QStringLiteral("action_forward_attachment"));
        QVERIFY(forward && forward->isEnabled());
        forward->trigger();
        QTRY_VERIFY_WITH_TIMEOUT((composer = visibleComposer()), 15000);
        auto *attached = composer->findChild<QAbstractItemView *>(QStringLiteral("attachmentsView"));
        QVERIFY(attached && attached->model());
        QCOMPARE(attached->model()->rowCount(), 1);
        const QList<QLineEdit *> recipients = recipientFields(composer);
        QVERIFY(!recipients.isEmpty());
        recipients.first()->setText(QStringLiteral("Carol Example <carol@example.test>"));
        // The forwarded message downloads in the background, and sending before it arrives is refused; try again
        // as a user would. See CAVEATS.md.
        for (int attempt = 0; composer && attempt < 40; ++attempt) {
            dialogs.errors.clear();
            QVERIFY(QMetaObject::invokeMethod(composer, "send"));
            QTRY_VERIFY_WITH_TIMEOUT(!composer || !dialogs.errors.isEmpty(), 20000);
            if (composer) {
                QVERIFY2(dialogs.errors.first().contains(QLatin1String("not available")), qPrintable(dialogs.errors.first()));
                QTest::qWait(250);
            }
        }
        QVERIFY2(!composer, qPrintable(dialogs.errors.join(QLatin1Char('\n'))));
        QVERIFY2(dialogs.errors.isEmpty(), qPrintable(dialogs.errors.join(QLatin1Char('\n'))));
        sent = profile.lastSent();
        QVERIFY2(headerLine(sent, "To").contains(QLatin1String("carol@example.test")), sent.constData());
        QVERIFY2(headerLine(sent, "Subject").contains(QLatin1String("Quarterly plan")), sent.constData());
        QVERIFY2(sent.contains("message/rfc822"), sent.constData());
        QVERIFY2(sent.contains("Message-ID: <plan-1@example.test>"), sent.constData());
        QVERIFY2(sent.contains("filename=\"budget.csv\""), sent.constData());
        QTRY_VERIFY_WITH_TIMEOUT(original.data(Imap::Mailbox::RoleMessageIsMarkedForwarded).toBool(), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(storedMessages(QStringLiteral(".Sent")), sentBefore + 2, 10000);
        QCOMPARE(dialogs.passwordPrompts, 0);
    }

    void composeSendAndRecover()
    {
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Profile profile(directory, port);
        Dialogs dialogs;
        QVERIFY(autosavedDrafts().isEmpty());

        auto window = std::make_unique<Gui::MainWindow>(&profile.settings);
        window->resize(1400, 900);
        window->show();
        QVERIFY(addIdentity(*window));
        auto *tree = window->findChild<Gui::MailBoxTreeView *>();
        QVERIFY(tree);
        QTRY_VERIFY_WITH_TIMEOUT(findMailbox(tree->model(), QStringLiteral("INBOX")).isValid(), 20000);
        const int sentBefore = storedMessages(QStringLiteral(".Sent"));

        auto *compose = window->findChild<QAction *>(QStringLiteral("action_compose_mail"));
        QVERIFY(compose);
        const auto fill = [](Gui::ComposeWidget *composer, const QString &subject) {
            recipientFields(composer).first()->setText(QStringLiteral("Carol Example <carol@example.test>"));
            // Typed, because only edits by the user make an unsent message worth keeping
            QTest::keyClicks(composer->findChild<QLineEdit *>(QStringLiteral("subject")), subject);
            composer->findChild<QTextEdit *>(QStringLiteral("mailText"))->setPlainText(subject + QLatin1String(" body."));
        };

        // A new message which the outgoing server refuses keeps its composer open for another try
        compose->trigger();
        QPointer<Gui::ComposeWidget> composer;
        QTRY_VERIFY_WITH_TIMEOUT((composer = visibleComposer()), 10000);
        QVERIFY(!recipientFields(composer).isEmpty());
        fill(composer, QStringLiteral("Synthetic new message"));
        auto *buttons = composer->findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
        QVERIFY(buttons);
        profile.setFailing(true);
        QVERIFY(QMetaObject::invokeMethod(composer, "send"));
        QTRY_COMPARE_WITH_TIMEOUT(dialogs.errors.size(), 1, 20000);
        QVERIFY2(dialogs.errors.first().contains(QLatin1String("synthetic refusal")), qPrintable(dialogs.errors.first()));
        QVERIFY(composer);
        QTRY_VERIFY(buttons->isEnabled());
        QVERIFY(profile.lastSent().isEmpty());
        // Until it is sent, the message is kept as an automatic draft
        QCOMPARE(autosavedDrafts().size(), 1);
        // The copy for the Sent folder is stored before the outgoing server is asked; see CAVEATS.md
        QTRY_COMPARE_WITH_TIMEOUT(storedMessages(QStringLiteral(".Sent")), sentBefore + 1, 10000);

        profile.setFailing(false);
        dialogs.errors.clear();
        QVERIFY(QMetaObject::invokeMethod(composer, "send"));
        QTRY_VERIFY2_WITH_TIMEOUT(!composer, qPrintable(dialogs.errors.join(QLatin1Char('\n'))), 20000);
        QVERIFY2(dialogs.errors.isEmpty(), qPrintable(dialogs.errors.join(QLatin1Char('\n'))));
        const QByteArray sent = profile.lastSent();
        QCOMPARE(headerLine(sent, "Subject"), QStringLiteral("Subject: Synthetic new message"));
        QVERIFY2(headerLine(sent, "To").contains(QLatin1String("carol@example.test")), sent.constData());
        QVERIFY2(sent.contains("Synthetic new message body."), sent.constData());
        QTRY_COMPARE_WITH_TIMEOUT(storedMessages(QStringLiteral(".Sent")), sentBefore + 2, 10000);
        QVERIFY(autosavedDrafts().isEmpty());

        // Closing an unsent message offers to save it as a draft, which opens again later
        compose->trigger();
        QTRY_VERIFY_WITH_TIMEOUT((composer = visibleComposer()), 10000);
        fill(composer, QStringLiteral("Synthetic saved draft"));
        const QString draft = directory.filePath(QStringLiteral("saved.draft"));
        dialogs.draftAnswer = QMessageBox::Save;
        dialogs.nextFile = draft;
        composer->close();
        QTRY_VERIFY_WITH_TIMEOUT(!composer, 10000);
        QVERIFY(QFile::exists(draft));
        QVERIFY(autosavedDrafts().isEmpty());
        composer = Gui::ComposeWidget::createDraft(window.get(), draft);
        QVERIFY(composer);
        QCOMPARE(composer->findChild<QLineEdit *>(QStringLiteral("subject"))->text(), QStringLiteral("Synthetic saved draft"));
        QCOMPARE(recipientFields(composer).first()->text(), QStringLiteral("Carol Example <carol@example.test>"));
        QCOMPARE(composer->findChild<QTextEdit *>(QStringLiteral("mailText"))->toPlainText(),
                 QStringLiteral("Synthetic saved draft body."));
        composer->close();
        QTRY_VERIFY_WITH_TIMEOUT(!composer, 10000);
        QVERIFY(QFile::exists(draft));

        // A message left open when the application ends unexpectedly opens again at the next start
        dialogs.draftAnswer = QMessageBox::Discard;
        compose->trigger();
        QTRY_VERIFY_WITH_TIMEOUT((composer = visibleComposer()), 10000);
        fill(composer, QStringLiteral("Synthetic recovered message"));
        QVERIFY(QMetaObject::invokeMethod(composer, "autoSaveDraft"));
        QCOMPARE(autosavedDrafts().size(), 1);
        // Deleting skips the close event, like a crash would
        delete composer;
        window.reset();
        QCOMPARE(autosavedDrafts().size(), 1);

        window = std::make_unique<Gui::MainWindow>(&profile.settings);
        window->resize(1400, 900);
        window->show();
        QVERIFY(addIdentity(*window));
        QTRY_VERIFY_WITH_TIMEOUT((composer = visibleComposer()), 15000);
        QCOMPARE(composer->findChild<QLineEdit *>(QStringLiteral("subject"))->text(),
                 QStringLiteral("Synthetic recovered message"));
        QCOMPARE(recipientFields(composer).first()->text(), QStringLiteral("Carol Example <carol@example.test>"));
        QVERIFY(QMetaObject::invokeMethod(composer, "send"));
        QTRY_VERIFY2_WITH_TIMEOUT(!composer, qPrintable(dialogs.errors.join(QLatin1Char('\n'))), 20000);
        QVERIFY2(dialogs.errors.isEmpty(), qPrintable(dialogs.errors.join(QLatin1Char('\n'))));
        QCOMPARE(headerLine(profile.lastSent(), "Subject"), QStringLiteral("Subject: Synthetic recovered message"));
        QVERIFY(autosavedDrafts().isEmpty());
        QCOMPARE(dialogs.passwordPrompts, 0);
    }

    /** Bcc reaches its recipient and the Sent copy, never the other recipients, and a pasted line break cannot add a
     * header */
    void bccAndHeaderSafety()
    {
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Profile profile(directory, port);
        Dialogs dialogs;

        Gui::MainWindow window(&profile.settings);
        window.resize(1400, 900);
        window.show();
        QVERIFY(addIdentity(window));
        auto *tree = window.findChild<Gui::MailBoxTreeView *>();
        QVERIFY(tree);
        QTRY_VERIFY_WITH_TIMEOUT(findMailbox(tree->model(), QStringLiteral("INBOX")).isValid(), 20000);
        const int sentBefore = storedMessages(QStringLiteral(".Sent"));

        // A link whose subject tries to add a Bcc header of its own
        const QUrl link(QStringLiteral("mailto:carol@example.test?bcc=dave@example.test"
                                       "&subject=Synthetic%20hidden%20copy%0D%0ABcc:%20eve@example.test"
                                       "&body=Synthetic%20hidden%20copy%20body."));
        QPointer<Gui::ComposeWidget> composer = Gui::ComposeWidget::createFromUrl(&window, link);
        QVERIFY(composer);
        const QString subject = QStringLiteral("Synthetic hidden copy Bcc: eve@example.test");
        QCOMPARE(composer->findChild<QLineEdit *>(QStringLiteral("subject"))->text(), subject);
        QVERIFY(recipientKinds(composer).contains(Composer::ADDRESS_BCC));

        // A draft keeps the Bcc recipient
        const QString draft = directory.filePath(QStringLiteral("hidden.draft"));
        dialogs.draftAnswer = QMessageBox::Save;
        dialogs.nextFile = draft;
        QVERIFY(QMetaObject::invokeMethod(composer, "setMessageUpdated"));
        composer->close();
        QTRY_VERIFY_WITH_TIMEOUT(!composer, 10000);
        QVERIFY(QFile::exists(draft));
        composer = Gui::ComposeWidget::createDraft(&window, draft);
        QVERIFY(composer);
        const QList<QLineEdit *> fields = recipientFields(composer);
        const QList<int> kinds = recipientKinds(composer);
        QCOMPARE(kinds.size(), fields.size());
        bool draftHasBcc = false;
        for (int i = 0; i < fields.size(); ++i) {
            if (fields[i]->text().contains(QLatin1String("dave@example.test")))
                draftHasBcc = kinds[i] == Composer::ADDRESS_BCC;
        }
        QVERIFY(draftHasBcc);

        dialogs.draftAnswer = QMessageBox::Discard;
        QVERIFY(QMetaObject::invokeMethod(composer, "send"));
        QTRY_VERIFY2_WITH_TIMEOUT(!composer, qPrintable(dialogs.errors.join(QLatin1Char('\n'))), 20000);
        QVERIFY2(dialogs.errors.isEmpty(), qPrintable(dialogs.errors.join(QLatin1Char('\n'))));

        // The delivered message names no hidden recipient, but the outgoing server still sends to it
        const QByteArray sent = profile.lastSent();
        QCOMPARE(headerLine(sent, "Subject"), QStringLiteral("Subject: ") + subject);
        QVERIFY2(headerLine(sent, "Bcc").isEmpty(), sent.constData());
        QVERIFY2(!sent.contains("dave@example.test"), sent.constData());
        QVERIFY2(headerLine(sent, "To").contains(QLatin1String("carol@example.test")), sent.constData());
        const QStringList arguments = profile.lastArguments();
        QVERIFY2(arguments.contains(QLatin1String("dave@example.test")), qPrintable(arguments.join(QLatin1Char(' '))));
        QVERIFY2(!arguments.contains(QLatin1String("eve@example.test")), qPrintable(arguments.join(QLatin1Char(' '))));

        // The Sent copy lists it, and both copies share a Message-ID in the sender's domain
        QTRY_COMPARE_WITH_TIMEOUT(storedMessages(QStringLiteral(".Sent")), sentBefore + 1, 10000);
        const QByteArray stored = newestStoredMessage(QStringLiteral(".Sent"));
        QVERIFY2(headerLine(stored, "Bcc").contains(QLatin1String("dave@example.test")), stored.constData());
        QCOMPARE(headerLine(stored, "Subject"), QStringLiteral("Subject: ") + subject);
        const QString messageId = headerLine(sent, "Message-ID");
        QVERIFY2(messageId.endsWith(QLatin1String("@example.test>")), qPrintable(messageId));
        QCOMPARE(headerLine(stored, "Message-ID"), messageId);
        QCOMPARE(dialogs.passwordPrompts, 0);
    }

    /** Files which a file manager hands over, through a mailto: URL or the command line, are attached and named in a
     * notice; refused ones are listed, and a link in a message cannot attach any */
    void filesFromOtherApplications()
    {
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Profile profile(directory, port);
        Dialogs dialogs;
        // A composer left open by an earlier failure would be taken for the ones opened here
        while (QPointer<Gui::ComposeWidget> leftover = visibleComposer())
            delete leftover;

        Gui::MainWindow window(&profile.settings);
        window.resize(1400, 900);
        window.show();
        QVERIFY(addIdentity(window));
        auto *tree = window.findChild<Gui::MailBoxTreeView *>();
        QVERIFY(tree);
        QTRY_VERIFY_WITH_TIMEOUT(findMailbox(tree->model(), QStringLiteral("INBOX")).isValid(), 20000);
        const int sentBefore = storedMessages(QStringLiteral(".Sent"));

        const QString notes = directory.filePath(QStringLiteral("files/handoff-notes.txt"));
        const QString spaced = directory.filePath(QStringLiteral("files/100% plan & budget.txt"));
        const QString chosen = directory.filePath(QStringLiteral("files/chosen-on-command-line.txt"));
        const QString hidden = directory.filePath(QStringLiteral("files/.hidden-secret"));
        QVERIFY(writeFile(notes, "Synthetic handoff notes\n"));
        QVERIFY(writeFile(spaced, "Synthetic plan and budget\n"));
        QVERIFY(writeFile(chosen, "Synthetic command line file\n"));
        QVERIFY(writeFile(hidden, "Synthetic secret\n"));
        const auto noticeOf = [](Gui::ComposeWidget *composer) {
            return composer->findChild<Gui::InlineNotificationBar *>(QStringLiteral("composerHandoffNotice"));
        };

        // A file manager's request: the files are attached, the hidden and remote ones refused and listed, and the
        // file also named on the command line attached once
        const QUrl request = launcherUrl(QStringLiteral("subject=Synthetic%20handoff&body=Files%20attached."),
                                         {notes, spaced, hidden, QStringLiteral("sftp://localhost/files/remote.txt"), chosen});
        window.composeMailFromOtherApplication(QUrl::fromEncoded(request.toEncoded()), {chosen});
        QPointer<Gui::ComposeWidget> composer;
        QTRY_VERIFY_WITH_TIMEOUT((composer = visibleComposer()), 10000);
        QCOMPARE(composer->findChild<QLineEdit *>(QStringLiteral("subject"))->text(), QStringLiteral("Synthetic handoff"));
        QCOMPARE(attachmentNames(composer),
                 (QStringList{QStringLiteral("chosen-on-command-line.txt"), QStringLiteral("handoff-notes.txt"),
                              QStringLiteral("100% plan & budget.txt")}));
        auto *notice = noticeOf(composer);
        QVERIFY(notice);
        QVERIFY(notice->isVisible());
        QCOMPARE(notice->kind(), Gui::InlineNotificationBar::Kind::Error);
        for (const QString &expected : {QStringLiteral(".hidden-secret"), QStringLiteral("sftp://localhost/files/remote.txt"),
                                        QStringLiteral("handoff-notes.txt"), QStringLiteral("chosen-on-command-line.txt")})
            QVERIFY2(notice->text().contains(expected), qPrintable(notice->text()));
        QVERIFY2(notice->text().contains(QLatin1String("2 requested files could not be attached")), qPrintable(notice->text()));
        // It stays until dismissed, for the files to be checked before sending
        QTest::qWait(500);
        QVERIFY(notice->isVisible());

        // The attached files are sent
        QVERIFY(QMetaObject::invokeMethod(composer, "send"));
        QTRY_VERIFY2_WITH_TIMEOUT(!composer, qPrintable(dialogs.errors.join(QLatin1Char('\n'))), 20000);
        QVERIFY2(dialogs.errors.isEmpty(), qPrintable(dialogs.errors.join(QLatin1Char('\n'))));
        const QByteArray sent = profile.lastSent();
        QCOMPARE(headerLine(sent, "Subject"), QStringLiteral("Subject: Synthetic handoff"));
        QVERIFY2(sent.contains("filename=handoff-notes.txt"), sent.constData());
        QVERIFY2(sent.contains("filename=chosen-on-command-line.txt"), sent.constData());
        QVERIFY2(sent.contains("filename*=utf-8''100%25%20plan%20%26%20budget.txt"), sent.constData());
        QVERIFY2(!sent.contains("hidden-secret"), sent.constData());
        QVERIFY2(!sent.contains("remote.txt"), sent.constData());
        QTRY_COMPARE_WITH_TIMEOUT(storedMessages(QStringLiteral(".Sent")), sentBefore + 1, 10000);

        // Through D-Bus, as a second start of the application forwards it, everything requested is attached
        IPC::MainWindowBridge bridge(&window);
        bridge.composeMailWithFiles(QString::fromUtf8(launcherUrl(QString(), {notes}).toEncoded()), {chosen});
        QTRY_VERIFY_WITH_TIMEOUT((composer = visibleComposer()), 10000);
        QCOMPARE(attachmentNames(composer),
                 (QStringList{QStringLiteral("chosen-on-command-line.txt"), QStringLiteral("handoff-notes.txt")}));
        notice = noticeOf(composer);
        QVERIFY(notice && notice->isVisible());
        QCOMPARE(notice->kind(), Gui::InlineNotificationBar::Kind::Info);
        QVERIFY2(notice->text().contains(QLatin1String("Attached 2 files")), qPrintable(notice->text()));
        QVERIFY2(notice->text().contains(QLatin1String("handoff-notes.txt")), qPrintable(notice->text()));
        composer->close();
        QTRY_VERIFY_WITH_TIMEOUT(!composer, 10000);

        // Files alone open a blank message with them
        bridge.composeMailWithFiles(QString(), {notes});
        QTRY_VERIFY_WITH_TIMEOUT((composer = visibleComposer()), 10000);
        QCOMPARE(attachmentNames(composer), QStringList{QStringLiteral("handoff-notes.txt")});
        QCOMPARE(noticeOf(composer)->kind(), Gui::InlineNotificationBar::Kind::Info);
        composer->close();
        QTRY_VERIFY_WITH_TIMEOUT(!composer, 10000);

        // Nothing at all opens a blank message without a notice
        bridge.composeMailWithFiles(QString(), {});
        QTRY_VERIFY_WITH_TIMEOUT((composer = visibleComposer()), 10000);
        QVERIFY(attachmentNames(composer).isEmpty());
        QVERIFY(!noticeOf(composer) || !noticeOf(composer)->isVisible());
        composer->close();
        QTRY_VERIFY_WITH_TIMEOUT(!composer, 10000);

        // A link in a message may not pick files to send, whatever it asks
        composer = Gui::ComposeWidget::createFromUrl(&window, request);
        QVERIFY(composer);
        QCOMPARE(composer->findChild<QLineEdit *>(QStringLiteral("subject"))->text(), QStringLiteral("Synthetic handoff"));
        QVERIFY(attachmentNames(composer).isEmpty());
        notice = noticeOf(composer);
        QVERIFY(notice && notice->isVisible());
        QCOMPARE(notice->kind(), Gui::InlineNotificationBar::Kind::Error);
        QVERIFY2(notice->text().contains(QLatin1String("5 files")), qPrintable(notice->text()));
        composer->close();
        QTRY_VERIFY_WITH_TIMEOUT(!composer, 10000);
        QCOMPARE(dialogs.passwordPrompts, 0);
        QVERIFY2(dialogs.errors.isEmpty(), qPrintable(dialogs.errors.join(QLatin1Char('\n'))));
    }

    /** Files on other computers are fetched through KIO into private copies, with progress and cancellation; failures
     * are reported and removed, credentials in their URLs are never shown, and the copies go with the composer */
    void remoteFiles()
    {
        bool ok = false;
        const int port = qEnvironmentVariableIntValue("PLASMA_TEST_IMAPS_PORT", &ok);
        QVERIFY(ok && port > 0 && port <= 65535);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Profile profile(directory, port);
        Dialogs dialogs;
        while (QPointer<Gui::ComposeWidget> leftover = visibleComposer())
            delete leftover;

        FileServer server;
        server.files.insert("/files/report.txt", "Synthetic remote report\n");
        server.stalledPath = "/files/stalled.bin";
        QVERIFY(server.listen());
        const QString credentials = QStringLiteral("alice:hunter2@");
        const auto showsCredentials = [](const QString &text) {
            return text.contains(QLatin1String("hunter2")) || text.contains(QLatin1String("alice"));
        };

        Gui::MainWindow window(&profile.settings);
        window.resize(1400, 900);
        window.show();
        QVERIFY(addIdentity(window));
        auto *tree = window.findChild<Gui::MailBoxTreeView *>();
        QVERIFY(tree);
        QTRY_VERIFY_WITH_TIMEOUT(findMailbox(tree->model(), QStringLiteral("INBOX")).isValid(), 20000);
        QVERIFY(fetchedCopies().isEmpty());

        // Handed over on the command line, which may name this computer; it is fetched and named
        IPC::MainWindowBridge bridge(&window);
        bridge.composeMailWithFiles(QStringLiteral("mailto:carol@example.test?subject=Remote"),
                                    {server.url(credentials, QStringLiteral("/files/report.txt"))});
        QPointer<Gui::ComposeWidget> composer;
        QTRY_VERIFY_WITH_TIMEOUT((composer = visibleComposer()), 10000);
        auto *handoff = composer->findChild<Gui::InlineNotificationBar *>(QStringLiteral("composerHandoffNotice"));
        auto *fetching = composer->findChild<Gui::InlineNotificationBar *>(QStringLiteral("composerFetchNotice"));
        QVERIFY(handoff);
        QVERIFY(fetching);
        QVERIFY2(handoff->text().contains(server.url(QString(), QStringLiteral("/files/report.txt"))), qPrintable(handoff->text()));
        QVERIFY2(!showsCredentials(handoff->text()), qPrintable(handoff->text()));
        QTRY_COMPARE_WITH_TIMEOUT(attachmentNames(composer), QStringList{QStringLiteral("report.txt")}, 20000);
        QTRY_COMPARE_WITH_TIMEOUT(fetching->kind(), Gui::InlineNotificationBar::Kind::Info, 5000);
        QVERIFY(fetching->isVisible());
        QVERIFY2(fetching->text().contains(QLatin1String("report.txt")), qPrintable(fetching->text()));
        QVERIFY2(fetching->text().contains(QLatin1String("127.0.0.1")), qPrintable(fetching->text()));
        QVERIFY2(!showsCredentials(fetching->text()), qPrintable(fetching->text()));
        auto *view = composer->findChild<QAbstractItemView *>(QStringLiteral("attachmentsView"));
        QVERIFY(view);
        const QString tooltip = view->model()->index(0, 0).data(Qt::ToolTipRole).toString();
        QVERIFY2(tooltip.contains(QLatin1String("127.0.0.1")), qPrintable(tooltip));
        QVERIFY2(!showsCredentials(tooltip), qPrintable(tooltip));
        QCOMPARE(fetchedCopies().size(), 1);

        // A web link pasted into the text stays a link
        auto *text = composer->findChild<QTextEdit *>(QStringLiteral("mailText"));
        QVERIFY(text);
        auto *link = new QMimeData;
        link->setUrls({QUrl(server.url(QString(), QStringLiteral("/files/page.html")))});
        link->setText(server.url(QString(), QStringLiteral("/files/page.html")));
        QApplication::clipboard()->setMimeData(link);
        text->paste();
        QVERIFY2(text->toPlainText().contains(QLatin1String("/files/page.html")), qPrintable(text->toPlainText()));
        QCOMPARE(attachmentNames(composer).size(), 1);

        // A missing file is reported and removed; the next one waits for it, then can be cancelled
        const QList<QUrl> more{QUrl(server.url(credentials, QStringLiteral("/files/missing.txt"))),
                               QUrl(server.url(QString(), QStringLiteral("/files/stalled.bin")))};
        QVERIFY(QMetaObject::invokeMethod(composer, "slotAttachFiles", Q_ARG(QList<QUrl>, more)));
        QTRY_VERIFY_WITH_TIMEOUT(fetching->isBusy() && fetching->text().contains(QLatin1String("stalled.bin")), 20000);
        QVERIFY2(fetching->text().contains(QLatin1String("2 of 2")), qPrintable(fetching->text()));
        QCOMPARE(server.requests.count("/files/missing.txt"), 1);
        QTRY_VERIFY_WITH_TIMEOUT(attachmentNames(composer).size() == 2, 5000);
        QVERIFY2(attachmentNames(composer).last().contains(QLatin1String("fetching")), qPrintable(attachmentNames(composer).join(u',')));

        // Not sent while a file is missing
        QVERIFY(QMetaObject::invokeMethod(composer, "send"));
        QTRY_COMPARE_WITH_TIMEOUT(dialogs.errors.size(), 1, 5000);
        QVERIFY2(dialogs.errors.first().contains(QLatin1String("fetched")), qPrintable(dialogs.errors.first()));
        dialogs.errors.clear();
        QVERIFY(composer);

        auto *cancel = fetching->findChild<QAbstractButton *>(QStringLiteral("cancelRemoteAttachments"));
        QVERIFY(cancel);
        cancel->click();
        QCOMPARE(attachmentNames(composer), QStringList{QStringLiteral("report.txt")});
        QCOMPARE(fetching->kind(), Gui::InlineNotificationBar::Kind::Error);
        for (const QString &expected : {QStringLiteral("missing.txt"), QStringLiteral("stalled.bin"), QStringLiteral("cancelled")})
            QVERIFY2(fetching->text().contains(expected), qPrintable(fetching->text()));
        QVERIFY2(!showsCredentials(fetching->text()), qPrintable(fetching->text()));
        QCOMPARE(fetchedCopies().size(), 1);

        // The fetched copy is sent, and goes with the composer
        QVERIFY(QMetaObject::invokeMethod(composer, "send"));
        QTRY_VERIFY2_WITH_TIMEOUT(!composer, qPrintable(dialogs.errors.join(QLatin1Char('\n'))), 20000);
        const QByteArray sent = profile.lastSent();
        QVERIFY2(sent.contains("filename=report.txt"), sent.constData());
        QVERIFY2(sent.contains("Synthetic remote report") || sent.contains(QByteArray("Synthetic remote report\n").toBase64()),
                 sent.constData());
        QVERIFY2(!sent.contains("hunter2"), sent.constData());
        QVERIFY2(!sent.contains("stalled.bin"), sent.constData());
        QTRY_VERIFY_WITH_TIMEOUT(fetchedCopies().isEmpty(), 5000);
        QCOMPARE(dialogs.passwordPrompts, 0);
        QVERIFY2(dialogs.errors.isEmpty(), qPrintable(dialogs.errors.join(QLatin1Char('\n'))));
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
    LiveImapComposeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_LiveImapCompose.moc"
