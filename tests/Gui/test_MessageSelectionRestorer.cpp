/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/MessageSelectionRestorer.h"
#include "Imap/Model/ItemRoles.h"

#include <QApplication>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeView>

class MessageSelectionRestorerTest : public QObject
{
    Q_OBJECT
private:
    static QStandardItem *message(uint uid, uint validity)
    {
        auto *item = new QStandardItem(QStringLiteral("Message"));
        item->setData(QStringLiteral("INBOX"), Imap::Mailbox::RoleMailboxName);
        item->setData(uid, Imap::Mailbox::RoleMessageUid);
        item->setData(validity, Imap::Mailbox::RoleMailboxUidValidity);
        return item;
    }

private slots:
    void restoresAsThreadedMessageArrives()
    {
        QStandardItemModel model;
        QTreeView view;
        view.setModel(&model);
        Gui::MessageSelectionRestorer restorer(&view, &view);
        QSignalSpy restored(&restorer, &Gui::MessageSelectionRestorer::messageRestored);
        restorer.restore(QStringLiteral("INBOX"), 123, 42);
        auto *thread = message(5, 123);
        model.appendRow(thread);
        auto *saved = message(42, 123);
        thread->appendRow(saved);
        QTRY_COMPARE(restored.size(), 1);
        QCOMPARE(view.currentIndex(), saved->index());
        QCOMPARE(restored.at(0).at(0).value<QModelIndex>(), saved->index());
    }

    void refusesChangedUidValidity()
    {
        QStandardItemModel model;
        QTreeView view;
        view.setModel(&model);
        Gui::MessageSelectionRestorer restorer(&view, &view);
        QSignalSpy restored(&restorer, &Gui::MessageSelectionRestorer::messageRestored);
        restorer.restore(QStringLiteral("INBOX"), 123, 42);
        auto *reusedUid = message(42, 124);
        model.appendRow(reusedUid);
        QCoreApplication::processEvents();
        QCOMPARE(restored.size(), 0);
        reusedUid->setData(123u, Imap::Mailbox::RoleMailboxUidValidity);
        QCoreApplication::processEvents();
        QCOMPARE(restored.size(), 0);
    }

    void userNavigationCancelsRestore()
    {
        QStandardItemModel model;
        model.appendRow(message(5, 123));
        QTreeView view;
        view.setModel(&model);
        view.show();
        view.setFocus();
        QCoreApplication::processEvents();
        Gui::MessageSelectionRestorer restorer(&view, &view);
        QSignalSpy restored(&restorer, &Gui::MessageSelectionRestorer::messageRestored);
        restorer.restore(QStringLiteral("INBOX"), 123, 42);
        QTest::keyClick(&view, Qt::Key_Down);
        model.appendRow(message(42, 123));
        QCoreApplication::processEvents();
        QCOMPARE(restored.size(), 0);
    }

    void waitsForUidValidity()
    {
        QStandardItemModel model;
        auto *saved = message(42, 0);
        model.appendRow(saved);
        QTreeView view;
        view.setModel(&model);
        Gui::MessageSelectionRestorer restorer(&view, &view);
        QSignalSpy restored(&restorer, &Gui::MessageSelectionRestorer::messageRestored);
        restorer.restore(QStringLiteral("INBOX"), 123, 42);
        QCoreApplication::processEvents();
        QCOMPARE(restored.size(), 0);
        saved->setData(123u, Imap::Mailbox::RoleMailboxUidValidity);
        QTRY_COMPARE(restored.size(), 1);
    }

    void ignoresOtherMailboxesAndMissingSavedValidity()
    {
        QStandardItemModel model;
        auto *other = message(42, 123);
        other->setData(QStringLiteral("Archive"), Imap::Mailbox::RoleMailboxName);
        model.appendRow(other);
        QTreeView view;
        view.setModel(&model);
        Gui::MessageSelectionRestorer restorer(&view, &view);
        QSignalSpy restored(&restorer, &Gui::MessageSelectionRestorer::messageRestored);
        restorer.restore(QStringLiteral("INBOX"), 123, 42);
        QCoreApplication::processEvents();
        QCOMPARE(restored.size(), 0);
        auto *saved = message(42, 123);
        model.appendRow(saved);
        QTRY_COMPARE(restored.size(), 1);
        QCOMPARE(view.currentIndex(), saved->index());
        restorer.restore(QStringLiteral("INBOX"), 0, 42);
        QCoreApplication::processEvents();
        QCOMPARE(restored.size(), 1);
    }

    void cancellationStopsPendingRestore()
    {
        QStandardItemModel model;
        QTreeView view;
        view.setModel(&model);
        Gui::MessageSelectionRestorer restorer(&view, &view);
        QSignalSpy restored(&restorer, &Gui::MessageSelectionRestorer::messageRestored);
        restorer.restore(QStringLiteral("INBOX"), 123, 42);
        restorer.cancel();
        model.appendRow(message(42, 123));
        QCoreApplication::processEvents();
        QCOMPARE(restored.size(), 0);
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
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    MessageSelectionRestorerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_MessageSelectionRestorer.moc"
