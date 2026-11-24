/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/MailBoxTreeView.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/PrettyMailboxModel.h"

#include <QApplication>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class MailBoxTreeViewTest : public QObject
{
    Q_OBJECT
private:
    static QStandardItem *mailbox(const QString &name)
    {
        auto *item = new QStandardItem(name);
        item->setData(name, Imap::Mailbox::RoleMailboxName);
        item->setData(QStringLiteral("/"), Imap::Mailbox::RoleMailboxSeparator);
        item->setData(true, Imap::Mailbox::RoleMailboxIsSelectable);
        return item;
    }

private slots:
    void mailboxTooltipDoesNotReportUnknownCountsAsZero()
    {
        QStandardItemModel source;
        auto *item = mailbox(QStringLiteral("Archive"));
        item->setData(QStringLiteral("Archive"), Imap::Mailbox::RoleShortMailboxName);
        source.appendRow(item);
        Imap::Mailbox::PrettyMailboxModel model(nullptr, &source);
        const QModelIndex index = model.index(0, 0);

        const QString unknown = index.data(Qt::ToolTipRole).toString();
        QVERIFY(!unknown.contains(QStringLiteral("0 message")));
        QVERIFY(!unknown.contains(QStringLiteral("0 unread")));
        QVERIFY(!unknown.contains(QStringLiteral("0 recent")));

        item->setData(0, Imap::Mailbox::RoleTotalMessageCount);
        const QString partial = index.data(Qt::ToolTipRole).toString();
        QVERIFY(partial.contains(QStringLiteral("0 message")));
        QVERIFY(!partial.contains(QStringLiteral("0 unread")));
        QVERIFY(!partial.contains(QStringLiteral("0 recent")));

        item->setData(12, Imap::Mailbox::RoleTotalMessageCount);
        item->setData(3, Imap::Mailbox::RoleUnreadMessageCount);
        item->setData(1, Imap::Mailbox::RoleRecentMessageCount);
        const QString known = index.data(Qt::ToolTipRole).toString();
        QVERIFY(known.contains(QStringLiteral("12")));
        QVERIFY(known.contains(QStringLiteral("3")));
        QVERIFY(known.contains(QStringLiteral("1")));
    }

    void restoresWhenMailboxArrives()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        QStandardItemModel model;
        Gui::MailBoxTreeView tree(nullptr, &settings);
        tree.setModel(&model);
        QSignalSpy restored(&tree, &Gui::MailBoxTreeView::mailboxRestored);
        tree.restoreMailbox(QStringLiteral("Projects/Archive"));
        model.appendRow(mailbox(QStringLiteral("INBOX")));
        tree.show();
        tree.setFocus();
        QCoreApplication::processEvents();
        auto *parent = mailbox(QStringLiteral("Projects"));
        model.appendRow(parent);
        QCoreApplication::processEvents();
        QCOMPARE(restored.size(), 0);
        auto *child = mailbox(QStringLiteral("Projects/Archive"));
        parent->appendRow(child);
        QTRY_COMPARE(restored.size(), 1);
        QCOMPARE(tree.currentIndex(), child->index());
        QCOMPARE(restored.at(0).at(0).value<QModelIndex>(), child->index());
        QVERIFY(tree.isExpanded(parent->index()));
    }

    void userSelectionCancelsPendingRestore()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        QStandardItemModel model;
        auto *inbox = mailbox(QStringLiteral("INBOX"));
        model.appendRow(inbox);
        Gui::MailBoxTreeView tree(nullptr, &settings);
        tree.setModel(&model);
        QSignalSpy restored(&tree, &Gui::MailBoxTreeView::mailboxRestored);
        tree.show();
        QCoreApplication::processEvents();
        tree.restoreMailbox(QStringLiteral("Archive"));
        QTest::mouseClick(tree.viewport(), Qt::LeftButton, Qt::NoModifier, tree.visualRect(inbox->index()).center());
        model.appendRow(mailbox(QStringLiteral("Archive")));
        QCoreApplication::processEvents();
        QCOMPARE(restored.size(), 0);
        QCOMPARE(tree.currentIndex(), inbox->index());
    }

    void keyboardNavigationCancelsPendingRestore()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        QStandardItemModel model;
        model.appendRow(mailbox(QStringLiteral("INBOX")));
        Gui::MailBoxTreeView tree(nullptr, &settings);
        tree.setModel(&model);
        tree.show();
        tree.setFocus();
        QCoreApplication::processEvents();
        QSignalSpy restored(&tree, &Gui::MailBoxTreeView::mailboxRestored);
        tree.restoreMailbox(QStringLiteral("Archive"));
        QTest::keyClick(&tree, Qt::Key_Down);
        model.appendRow(mailbox(QStringLiteral("Archive")));
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
    MailBoxTreeViewTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_MailBoxTreeView.moc"
