/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/ReaderActionBar.h"
#include "Imap/Model/ItemRoles.h"

#include <KLocalizedString>
#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTest>
#include <QToolButton>

using Gui::ReaderActionBar;

class ReaderActionBarTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void withoutMessageEverythingIsOff();
    void followsTheMarksOfTheMessage();
    void requestsActOnTheMessage();
    void deleteFollowsTheTrashMode();
    void attachmentChipShowsTheCount();
    void plainTextPreference();
    void replyKeepsItsMenu();
    void removedMessageTurnsTheBarOff();

private:
    static QToolButton *button(ReaderActionBar &bar, const char *name)
    {
        auto *found = bar.findChild<QToolButton *>(QLatin1String(name));
        Q_ASSERT(found);
        return found;
    }
    static QStandardItem *addMessage(QStandardItemModel &model, bool read, bool flagged)
    {
        auto *item = new QStandardItem(QStringLiteral("message"));
        item->setData(read, Imap::Mailbox::RoleMessageIsMarkedRead);
        item->setData(flagged, Imap::Mailbox::RoleMessageIsMarkedFlagged);
        item->setData(false, Imap::Mailbox::RoleMessageIsMarkedDeleted);
        model.appendRow(item);
        return item;
    }
};

void ReaderActionBarTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    KLocalizedString::setApplicationDomain("plasma-impresario");
}

void ReaderActionBarTest::withoutMessageEverythingIsOff()
{
    ReaderActionBar bar;
    QCOMPARE(bar.objectName(), QStringLiteral("readerActions"));
    bar.setMarksEnabled(true);
    bar.setArchiveEnabled(true);
    bar.setAttachmentCount(3);
    for (const char *name : {"readerArchive", "readerTrash", "readerRead", "readerFlag", "readerZoomIn", "readerZoomOut",
                             "readerZoomReset", "readerPlainText"})
        QVERIFY2(!button(bar, name)->isEnabled(), name);
    QVERIFY(!bar.isAttachmentChipShown());
}

void ReaderActionBarTest::followsTheMarksOfTheMessage()
{
    QStandardItemModel model;
    addMessage(model, false, false);
    QStandardItem *item = addMessage(model, false, true);
    ReaderActionBar bar;
    bar.setMessage(item->index());
    QCOMPARE(bar.message(), item->index());

    // Marks need a connection; the preview controls do not
    QVERIFY(!button(bar, "readerRead")->isEnabled());
    QVERIFY(button(bar, "readerZoomIn")->isEnabled());
    bar.setMarksEnabled(true);
    QVERIFY(button(bar, "readerRead")->isEnabled());
    QVERIFY(!button(bar, "readerArchive")->isEnabled());
    bar.setArchiveEnabled(true);
    QVERIFY(button(bar, "readerArchive")->isEnabled());

    QVERIFY(!button(bar, "readerRead")->isChecked());
    QVERIFY(button(bar, "readerFlag")->isChecked());

    // Changes made elsewhere, also to other messages
    item->setData(true, Imap::Mailbox::RoleMessageIsMarkedRead);
    QVERIFY(button(bar, "readerRead")->isChecked());
    model.item(0)->setData(true, Imap::Mailbox::RoleMessageIsMarkedFlagged);
    item->setData(false, Imap::Mailbox::RoleMessageIsMarkedFlagged);
    QVERIFY(!button(bar, "readerFlag")->isChecked());
    QCOMPARE(button(bar, "readerRead")->toolTip(), i18nc("@info:tooltip", "Mark this message as unread"));
}

void ReaderActionBarTest::requestsActOnTheMessage()
{
    QStandardItemModel model;
    QStandardItem *item = addMessage(model, false, false);
    ReaderActionBar bar;
    bar.setMessage(item->index());
    bar.setMarksEnabled(true);
    bar.setArchiveEnabled(true);

    QSignalSpy read(&bar, &ReaderActionBar::markReadRequested);
    QSignalSpy flag(&bar, &ReaderActionBar::flagRequested);
    QSignalSpy archive(&bar, &ReaderActionBar::archiveRequested);
    QSignalSpy trash(&bar, &ReaderActionBar::trashRequested);
    QSignalSpy zoomIn(&bar, &ReaderActionBar::zoomInRequested);
    QSignalSpy zoomOut(&bar, &ReaderActionBar::zoomOutRequested);
    QSignalSpy zoomReset(&bar, &ReaderActionBar::zoomResetRequested);

    button(bar, "readerRead")->click();
    QCOMPARE(read.size(), 1);
    QCOMPARE(read.at(0).at(0).toBool(), true);
    button(bar, "readerFlag")->click();
    QCOMPARE(flag.size(), 1);
    QCOMPARE(flag.at(0).at(0).toBool(), true);
    button(bar, "readerArchive")->click();
    button(bar, "readerTrash")->click();
    button(bar, "readerZoomIn")->click();
    button(bar, "readerZoomOut")->click();
    button(bar, "readerZoomReset")->click();
    QCOMPARE(archive.size(), 1);
    QCOMPARE(trash.size(), 1);
    QCOMPARE(zoomIn.size(), 1);
    QCOMPARE(zoomOut.size(), 1);
    QCOMPARE(zoomReset.size(), 1);

    // The buttons show the marks of the message, not what was asked for
    QVERIFY(!button(bar, "readerFlag")->isChecked());
    item->setData(true, Imap::Mailbox::RoleMessageIsMarkedFlagged);
    QVERIFY(button(bar, "readerFlag")->isChecked());
}

void ReaderActionBarTest::deleteFollowsTheTrashMode()
{
    QStandardItemModel model;
    QStandardItem *item = addMessage(model, true, false);
    ReaderActionBar bar;
    bar.setMessage(item->index());
    QCOMPARE(button(bar, "readerTrash")->text(), i18nc("@action:button", "Move to Trash"));
    bar.setMovesToTrash(false);
    QCOMPARE(button(bar, "readerTrash")->text(), i18nc("@action:button", "Delete"));
    item->setData(true, Imap::Mailbox::RoleMessageIsMarkedDeleted);
    QCOMPARE(button(bar, "readerTrash")->text(), i18nc("@action:button", "Undelete"));
    QCOMPARE(button(bar, "readerTrash")->accessibleName(), i18nc("@action:button", "Undelete"));
}

void ReaderActionBarTest::attachmentChipShowsTheCount()
{
    QStandardItemModel model;
    QStandardItem *item = addMessage(model, true, false);
    ReaderActionBar bar;
    bar.setMessage(item->index());
    QToolButton *chip = button(bar, "readerAttachments");
    QVERIFY(!bar.isAttachmentChipShown());
    bar.setAttachmentCount(2);
    QVERIFY(bar.isAttachmentChipShown());
    QCOMPARE(chip->text(), i18ncp("@action:button", "%1 attachment", "%1 attachments", 2));
    QCOMPARE(bar.attachmentCount(), 2);
    QSignalSpy requested(&bar, &ReaderActionBar::attachmentsRequested);
    chip->click();
    QCOMPARE(requested.size(), 1);
    bar.setAttachmentCount(0);
    QVERIFY(!bar.isAttachmentChipShown());
}

void ReaderActionBarTest::plainTextPreference()
{
    QStandardItemModel model;
    QStandardItem *item = addMessage(model, true, false);
    ReaderActionBar bar;
    bar.setMessage(item->index());
    QSignalSpy toggled(&bar, &ReaderActionBar::preferPlainTextToggled);
    bar.setPreferPlainText(true);
    QVERIFY(button(bar, "readerPlainText")->isChecked());
    QCOMPARE(toggled.size(), 0);
    button(bar, "readerPlainText")->click();
    QCOMPARE(toggled.size(), 1);
    QCOMPARE(toggled.at(0).at(0).toBool(), false);
}

void ReaderActionBarTest::replyKeepsItsMenu()
{
    ReaderActionBar bar;
    QMenu menu;
    QAction privateReply(QStringLiteral("Private Reply"));
    QAction listReply(QStringLiteral("Reply to List"));
    menu.addAction(&privateReply);
    menu.addAction(&listReply);
    bar.setReplyMenu(&menu);
    bar.setReplyAction(&privateReply);
    QToolButton *reply = button(bar, "readerReply");
    QCOMPARE(reply->defaultAction(), &privateReply);
    bar.setReplyAction(&listReply);
    QCOMPARE(reply->defaultAction(), &listReply);
    QCOMPARE(reply->menu(), &menu);
    // Clicking replies; holding the button opens the menu, which a corner marker shows instead of an arrow
    QCOMPARE(reply->popupMode(), QToolButton::DelayedPopup);
    QVERIFY(reply->property("plasmaImpresarioMenuMarker").toBool());
    // The window disables reply actions when there is nobody to reply to
    listReply.setEnabled(false);
    QVERIFY(!reply->isEnabled());
}

void ReaderActionBarTest::removedMessageTurnsTheBarOff()
{
    QStandardItemModel model;
    QStandardItem *item = addMessage(model, true, false);
    ReaderActionBar bar;
    bar.setMessage(item->index());
    bar.setMarksEnabled(true);
    bar.setAttachmentCount(1);
    QVERIFY(button(bar, "readerRead")->isEnabled());
    model.removeRow(0);
    QVERIFY(!bar.message().isValid());
    QVERIFY(!button(bar, "readerRead")->isEnabled());
    QVERIFY(!bar.isAttachmentChipShown());
}

QTEST_MAIN(ReaderActionBarTest)
#include "test_ReaderActionBar.moc"
