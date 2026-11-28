/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

#include <KLocalizedString>

#include "Composer/Outbox.h"
#include "Gui/OutboxDialog.h"
#include "MSA/FakeMSA.h"

using Composer::Outbox;
using Gui::OutboxDialog;

class OutboxDialogTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();
    void listsMessagesWithAccountNames();
    void queuedMessageCanBeSentNow();
    void interruptedMessageAsksBeforeSendingAgain();
    void refusedMessageMovesToDrafts();

private:
    Outbox::Message message(const QString &subject) const;
    Outbox::Transport transport();
    QPushButton *button(OutboxDialog &dialog, const char *name) const;

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<MSA::FakeFactory> m_factory;
    bool m_online = false;
};

void OutboxDialogTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    KLocalizedString::setApplicationDomain("plasma-impresario");
}

void OutboxDialogTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_factory = std::make_unique<MSA::FakeFactory>();
    m_online = false;
}

void OutboxDialogTest::cleanup()
{
    m_factory.reset();
    m_dir.reset();
}

Outbox::Message OutboxDialogTest::message(const QString &subject) const
{
    Outbox::Message message;
    message.accountId = QStringLiteral("work");
    message.subject = subject;
    message.from = "me@example.org";
    message.recipients = {"you@example.org"};
    message.data = "Subject: " + subject.toUtf8() + "\r\n\r\nBody\r\n";
    message.timestamp = QDateTime::currentDateTime();
    message.draft = "draft of " + subject.toUtf8();
    return message;
}

Outbox::Transport OutboxDialogTest::transport()
{
    Outbox::Transport transport;
    MSA::FakeFactory *factory = m_factory.get();
    transport.createMsa = [factory](QObject *parent) { return factory->create(parent); };
    transport.online = [this]() { return m_online; };
    return transport;
}

QPushButton *OutboxDialogTest::button(OutboxDialog &dialog, const char *name) const
{
    return dialog.findChild<QPushButton *>(QLatin1String(name));
}

void OutboxDialogTest::listsMessagesWithAccountNames()
{
    Outbox outbox(m_dir->filePath(QStringLiteral("Outbox")), m_dir->filePath(QStringLiteral("Drafts")));
    QVERIFY(!outbox.enqueue(message(QStringLiteral("First"))).isEmpty());
    OutboxDialog dialog(&outbox, [](const QString &accountId) { return QStringLiteral("Account ") + accountId; });
    QCOMPARE(dialog.tree()->topLevelItemCount(), 1);
    QCOMPARE(dialog.tree()->topLevelItem(0)->text(0), QStringLiteral("First"));
    QCOMPARE(dialog.tree()->topLevelItem(0)->text(1), QStringLiteral("Account work"));

    // The list follows the outbox
    QVERIFY(!outbox.enqueue(message(QString())).isEmpty());
    QCOMPARE(dialog.tree()->topLevelItemCount(), 2);
    QVERIFY(!dialog.tree()->topLevelItem(1)->text(0).isEmpty());
}

void OutboxDialogTest::queuedMessageCanBeSentNow()
{
    Outbox outbox(m_dir->filePath(QStringLiteral("Outbox")), m_dir->filePath(QStringLiteral("Drafts")));
    outbox.setTransport(QStringLiteral("work"), transport());
    const QString id = outbox.enqueue(message(QStringLiteral("Queued")));
    OutboxDialog dialog(&outbox, {});
    dialog.tree()->setCurrentItem(dialog.tree()->topLevelItem(0));
    QVERIFY(button(dialog, "outboxSendNow")->isEnabled());
    QVERIFY(!button(dialog, "outboxSendAgain")->isEnabled());
    QVERIFY(!button(dialog, "outboxForget")->isEnabled());
    QVERIFY(button(dialog, "outboxMoveToDrafts")->isEnabled());

    QSignalSpy requested(m_factory.get(), &MSA::FakeFactory::requestedSending);
    m_online = true;
    dialog.sendNow();
    QVERIFY(requested.wait(1000));
    QCOMPARE(outbox.entry(id).state, Outbox::State::Sending);
    // Nothing can be done to a message while the server has it
    QVERIFY(!button(dialog, "outboxSendNow")->isEnabled());
    QVERIFY(!button(dialog, "outboxMoveToDrafts")->isEnabled());

    m_factory->doEmitSent();
    QTRY_COMPARE(dialog.tree()->topLevelItemCount(), 0);
}

void OutboxDialogTest::interruptedMessageAsksBeforeSendingAgain()
{
    const QString folder = m_dir->filePath(QStringLiteral("Outbox"));
    const QString drafts = m_dir->filePath(QStringLiteral("Drafts"));
    QString id;
    {
        // The application ends while the server has the message
        Outbox outbox(folder, drafts);
        outbox.setTransport(QStringLiteral("work"), transport());
        m_online = true;
        QSignalSpy requested(m_factory.get(), &MSA::FakeFactory::requestedSending);
        id = outbox.enqueue(message(QStringLiteral("Maybe sent")));
        QVERIFY(requested.wait(1000));
    }
    Outbox outbox(folder, drafts);
    QCOMPARE(outbox.entry(id).state, Outbox::State::Interrupted);
    OutboxDialog dialog(&outbox, {});
    dialog.tree()->setCurrentItem(dialog.tree()->topLevelItem(0));
    QVERIFY(!button(dialog, "outboxSendNow")->isEnabled());
    QVERIFY(button(dialog, "outboxSendAgain")->isEnabled());
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("outboxProblem"))->isVisibleTo(&dialog));

    int asked = 0;
    bool answer = false;
    dialog.setConfirmation([&](const QString &, const QString &) {
        ++asked;
        return answer;
    });
    dialog.sendAgain();
    QCOMPARE(asked, 1);
    QCOMPARE(outbox.entry(id).state, Outbox::State::Interrupted);
    answer = true;
    dialog.sendAgain();
    QCOMPARE(asked, 2);
    QCOMPARE(outbox.entry(id).state, Outbox::State::Queued);
}

void OutboxDialogTest::refusedMessageMovesToDrafts()
{
    const QString drafts = m_dir->filePath(QStringLiteral("Drafts"));
    Outbox outbox(m_dir->filePath(QStringLiteral("Outbox")), drafts);
    outbox.setTransport(QStringLiteral("work"), transport());
    const QString id = outbox.enqueue(message(QStringLiteral("Kept")));
    OutboxDialog dialog(&outbox, {});
    dialog.tree()->setCurrentItem(dialog.tree()->topLevelItem(0));
    QSignalSpy moved(&dialog, &OutboxDialog::draftMoved);
    dialog.moveToDrafts();
    QCOMPARE(moved.count(), 1);
    const QString path = moved.first().first().toString();
    QVERIFY(path.startsWith(drafts));
    QVERIFY(path.endsWith(QLatin1String(".draft")));
    QVERIFY(QFile::exists(path));
    QCOMPARE(outbox.count(), 0);
    QCOMPARE(dialog.tree()->topLevelItemCount(), 0);
    QVERIFY(outbox.entry(id).id.isEmpty());
}

QTEST_MAIN(OutboxDialogTest)
#include "test_OutboxDialog.moc"
