/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QApplication>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <KLocalizedString>

#include "Gui/MsgListView.h"
#include "Imap/Model/FavoriteTagsModel.h"
#include "Imap/Model/ItemRoles.h"

namespace {

const QStringList names{QStringLiteral("Alpha"), QStringLiteral("Bravo"), QStringLiteral("Charlie"), QStringLiteral("Delta"),
                        QStringLiteral("Echo"), QStringLiteral("Foxtrot"), QStringLiteral("Golf"), QStringLiteral("Hotel")};

}

class MsgListViewSelectionTest : public QObject
{
    Q_OBJECT

    QStandardItemModel *m_model = nullptr;
    Imap::Mailbox::FavoriteTagsModel *m_tags = nullptr;
    Gui::MsgListView *m_view = nullptr;

    /** @short UIDs of the selected rows, in the order of the list */
    QList<uint> selectedUids() const
    {
        QList<uint> uids;
        for (int row = 0; row < m_model->rowCount(); ++row) {
            if (m_view->selectionModel()->isRowSelected(row, QModelIndex()))
                uids << m_model->index(row, 0).data(Imap::Mailbox::RoleMessageUid).toUInt();
        }
        return uids;
    }

    uint currentUid() const
    {
        return m_view->currentIndex().data(Imap::Mailbox::RoleMessageUid).toUInt();
    }

    void key(Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        QTest::keyClick(m_view, key, modifiers);
    }

private slots:
    void init()
    {
        m_model = new QStandardItemModel(this);
        for (int i = 0; i < names.size(); ++i) {
            auto *item = new QStandardItem(names.at(i));
            item->setData(uint(i + 1), Imap::Mailbox::RoleMessageUid);
            m_model->appendRow(item);
        }
        m_tags = new Imap::Mailbox::FavoriteTagsModel(this);
        m_view = new Gui::MsgListView(nullptr, m_tags);
        m_view->setAutoActivateAfterKeyNavigation(false);
        m_view->setModel(m_model);
        m_view->resize(400, 600);
        m_view->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_view));
        m_view->activateWindow();
        m_view->setFocus();
        m_view->setCurrentIndex(m_model->index(2, 0));
        QCOMPARE(selectedUids(), QList<uint>({3}));
    }

    void cleanup()
    {
        delete m_view;
        m_view = nullptr;
        delete m_tags;
        delete m_model;
    }

    void shiftExtendsFromStableAnchor()
    {
        key(Qt::Key_Down, Qt::ShiftModifier);
        key(Qt::Key_Down, Qt::ShiftModifier);
        QCOMPARE(currentUid(), 5u);
        QCOMPARE(selectedUids(), QList<uint>({3, 4, 5}));
        // Going back past the anchor selects the other side of it
        key(Qt::Key_Up, Qt::ShiftModifier);
        key(Qt::Key_Up, Qt::ShiftModifier);
        key(Qt::Key_Up, Qt::ShiftModifier);
        QCOMPARE(currentUid(), 2u);
        QCOMPARE(selectedUids(), QList<uint>({2, 3}));
        key(Qt::Key_End, Qt::ShiftModifier);
        QCOMPARE(currentUid(), 8u);
        QCOMPARE(selectedUids(), QList<uint>({3, 4, 5, 6, 7, 8}));
        key(Qt::Key_Home, Qt::ShiftModifier);
        QCOMPARE(currentUid(), 1u);
        QCOMPARE(selectedUids(), QList<uint>({1, 2, 3}));
        key(Qt::Key_PageDown, Qt::ShiftModifier);
        QVERIFY(currentUid() > 3);
        QCOMPARE(selectedUids().constFirst(), 3u);
        QCOMPARE(selectedUids().constLast(), currentUid());
        // A plain move starts again from one message
        key(Qt::Key_Up);
        QCOMPARE(selectedUids(), QList<uint>({currentUid()}));
    }

    void ctrlMovesAndToggles()
    {
        key(Qt::Key_Down, Qt::ControlModifier);
        QCOMPARE(currentUid(), 4u);
        QCOMPARE(selectedUids(), QList<uint>({3}));
        key(Qt::Key_Space, Qt::ControlModifier);
        QCOMPARE(selectedUids(), QList<uint>({3, 4}));
        key(Qt::Key_Down, Qt::ControlModifier);
        key(Qt::Key_Down, Qt::ControlModifier);
        key(Qt::Key_Space, Qt::ControlModifier);
        QCOMPARE(currentUid(), 6u);
        QCOMPARE(selectedUids(), QList<uint>({3, 4, 6}));
        key(Qt::Key_Up, Qt::ControlModifier);
        key(Qt::Key_Up, Qt::ControlModifier);
        key(Qt::Key_Space, Qt::ControlModifier);
        QCOMPARE(currentUid(), 4u);
        QCOMPARE(selectedUids(), QList<uint>({3, 6}));
        key(Qt::Key_End, Qt::ControlModifier);
        QCOMPARE(currentUid(), 8u);
        QCOMPARE(selectedUids(), QList<uint>({3, 6}));
        key(Qt::Key_Home, Qt::ControlModifier);
        QCOMPARE(currentUid(), 1u);
        QCOMPARE(selectedUids(), QList<uint>({3, 6}));
    }

    void lettersKeepSeveralSelected()
    {
        // With one message selected, typing still jumps to a matching one
        QTest::keyClicks(m_view, QStringLiteral("e"));
        QCOMPARE(currentUid(), 5u);
        QCOMPARE(selectedUids(), QList<uint>({5}));
        key(Qt::Key_Down, Qt::ShiftModifier);
        QTest::qWait(QApplication::keyboardInputInterval() + 50);
        QTest::keyClicks(m_view, QStringLiteral("b"));
        QCOMPARE(currentUid(), 6u);
        QCOMPARE(selectedUids(), QList<uint>({5, 6}));
    }

    void removingTheSelectionSelectsTheNextMessage()
    {
        QSignalSpy replaced(m_view, &Gui::MsgListView::currentReplacedAfterRemoval);
        key(Qt::Key_Down, Qt::ShiftModifier);
        QCOMPARE(selectedUids(), QList<uint>({3, 4}));
        // The cursor is at the end of the block, the next remaining message follows it
        m_model->removeRows(2, 2);
        QTRY_COMPARE(replaced.size(), 1);
        QCOMPARE(currentUid(), 5u);
        QCOMPARE(selectedUids(), QList<uint>({5}));
        QCOMPARE(replaced.constFirst().constFirst().toModelIndex(), m_view->currentIndex());
        QVERIFY(m_view->hasFocus());

        // The cursor at the start of the block
        m_view->setCurrentIndex(m_model->index(3, 0));
        key(Qt::Key_Up, Qt::ShiftModifier);
        QCOMPARE(currentUid(), 5u);
        QCOMPARE(selectedUids(), QList<uint>({5, 6}));
        m_model->removeRows(2, 2);
        QTRY_COMPARE(replaced.size(), 2);
        QCOMPARE(currentUid(), 7u);
        QCOMPARE(selectedUids(), QList<uint>({7}));
    }

    void removingTheLastMessageSelectsThePreviousOne()
    {
        QSignalSpy replaced(m_view, &Gui::MsgListView::currentReplacedAfterRemoval);
        key(Qt::Key_End);
        QCOMPARE(currentUid(), 8u);
        m_model->removeRows(7, 1);
        QTRY_COMPARE(replaced.size(), 1);
        QCOMPARE(currentUid(), 7u);
        QCOMPARE(selectedUids(), QList<uint>({7}));
    }

    void otherRemovalsKeepTheSelection()
    {
        QSignalSpy replaced(m_view, &Gui::MsgListView::currentReplacedAfterRemoval);
        key(Qt::Key_Down, Qt::ShiftModifier);
        m_model->removeRows(6, 1);
        QCoreApplication::processEvents();
        QCOMPARE(replaced.size(), 0);
        QCOMPARE(currentUid(), 4u);
        QCOMPARE(selectedUids(), QList<uint>({3, 4}));

        // Part of the selection goes away, the cursor stays
        m_model->removeRows(2, 1);
        QCoreApplication::processEvents();
        QCOMPARE(replaced.size(), 0);
        QCOMPARE(currentUid(), 4u);
        QCOMPARE(selectedUids(), QList<uint>({4}));
    }

    void emptiedSelectionFallsBackToTheCursor()
    {
        QSignalSpy replaced(m_view, &Gui::MsgListView::currentReplacedAfterRemoval);
        key(Qt::Key_Down, Qt::ShiftModifier);
        key(Qt::Key_Down, Qt::ControlModifier);
        key(Qt::Key_Down, Qt::ControlModifier);
        QCOMPARE(currentUid(), 6u);
        QCOMPARE(selectedUids(), QList<uint>({3, 4}));
        m_model->removeRows(2, 2);
        QTRY_COMPARE(replaced.size(), 1);
        QCOMPARE(currentUid(), 6u);
        QCOMPARE(selectedUids(), QList<uint>({6}));
    }

    void emptiedListSelectsNothing()
    {
        QSignalSpy replaced(m_view, &Gui::MsgListView::currentReplacedAfterRemoval);
        m_view->selectAll();
        m_model->removeRows(0, m_model->rowCount());
        QCoreApplication::processEvents();
        QCOMPARE(replaced.size(), 0);
        QVERIFY(!m_view->currentIndex().isValid());
        QVERIFY(!m_view->selectionModel()->hasSelection());
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
    qputenv("LANGUAGE", "C");
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("plasma-impresario");
    MsgListViewSelectionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_MsgListViewSelection.moc"
