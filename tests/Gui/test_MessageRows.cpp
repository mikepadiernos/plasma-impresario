/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QSet>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <KConfigGroup>
#include <KSharedConfig>

#include "Gui/MessageRowsMenu.h"
#include "Gui/MsgItemDelegate.h"
#include "Gui/MsgListView.h"
#include "Imap/Model/FavoriteTagsModel.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/PrettyMsgListModel.h"

using Imap::Mailbox::MsgListModel;

namespace {

struct Message
{
    QString from;
    QString subject;
    bool read = false;
    bool flagged = false;
    bool attachments = false;
};

void appendMessage(QStandardItemModel &model, const Message &message)
{
    using namespace Imap::Mailbox;
    QList<QStandardItem *> row;
    for (int column = 0; column < MsgListModel::COLUMN_COUNT; ++column) {
        auto *item = new QStandardItem;
        item->setData(true, RoleIsFetched);
        item->setData(uint(model.rowCount() + 1), RoleMessageUid);
        item->setData(message.subject, RoleMessageSubject);
        item->setData(message.read, RoleMessageIsMarkedRead);
        item->setData(message.flagged, RoleMessageIsMarkedFlagged);
        item->setData(message.attachments, RoleMessageHasAttachments);
        switch (column) {
        case MsgListModel::SUBJECT:
            item->setText(message.subject);
            break;
        case MsgListModel::FROM:
            item->setText(message.from);
            break;
        case MsgListModel::DATE:
            item->setText(QStringLiteral("10:42"));
            break;
        }
        row << item;
    }
    model.appendRow(row);
}

}

class MessageRowsTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_home;
    QStandardItemModel *m_model = nullptr;
    Imap::Mailbox::FavoriteTagsModel *m_tags = nullptr;
    Gui::MsgListView *m_view = nullptr;

    KSharedConfigPtr config() const
    {
        return KSharedConfig::openConfig(m_home.filePath(QStringLiteral("plasma-impresariorc")), KConfig::SimpleConfig);
    }

    /** @short Columns that a click reaches anywhere in the row of @arg row */
    QSet<int> clickableColumns(int row) const
    {
        QSet<int> columns;
        const QRect rect = m_view->visualRect(m_model->index(row, 0));
        for (int y = rect.top(); y <= rect.bottom(); ++y) {
            for (int x = rect.left(); x <= rect.right(); ++x) {
                const QModelIndex index = m_view->indexAt(QPoint(x, y));
                if (index.isValid() && index.row() == row)
                    columns << index.column();
            }
        }
        return columns;
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_home.isValid());
    }

    void init()
    {
        m_model = new QStandardItemModel(0, MsgListModel::COLUMN_COUNT, this);
        appendMessage(*m_model, {QStringLiteral("Jane Doe"), QStringLiteral("Quarterly report"), false, true, true});
        appendMessage(*m_model, {QStringLiteral("bob@example.org"), QStringLiteral("Lunch?"), true, false, false});
        m_tags = new Imap::Mailbox::FavoriteTagsModel(this);
        m_view = new Gui::MsgListView(nullptr, m_tags);
        m_view->setAutoActivateAfterKeyNavigation(false);
        m_view->setModel(m_model);
        m_view->resize(420, 300);
        m_view->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_view));
    }

    void cleanup()
    {
        delete m_view;
        m_view = nullptr;
        delete m_tags;
        delete m_model;
        KConfigGroup(config(), QStringLiteral("Appearance")).deleteGroup();
        KConfigGroup(config(), QStringLiteral("Theme")).deleteGroup();
        config()->sync();
    }

    void initials_data()
    {
        QTest::addColumn<QString>("sender");
        QTest::addColumn<QString>("initials");
        QTest::newRow("full name") << QStringLiteral("Jane Doe") << QStringLiteral("JD");
        QTest::newRow("three names") << QStringLiteral("Jane Q. Public") << QStringLiteral("JP");
        QTest::newRow("one name") << QStringLiteral("jane") << QStringLiteral("J");
        QTest::newRow("address") << QStringLiteral("bob@example.org") << QStringLiteral("B");
        QTest::newRow("list of names") << QStringLiteral("Ann, Bob") << QStringLiteral("AB");
        QTest::newRow("punctuation") << QStringLiteral("\"Ann\" (Sales)") << QStringLiteral("AS");
        QTest::newRow("outside the BMP") << QStringLiteral("\U0001D49C\U0001D49E") << QStringLiteral("\U0001D49C");
        QTest::newRow("no letters") << QStringLiteral("- !") << QString();
        QTest::newRow("empty") << QString() << QString();
    }

    void initials()
    {
        QFETCH(QString, sender);
        QFETCH(QString, initials);
        QCOMPARE(Gui::MsgItemDelegate::senderInitials(sender), initials);
    }

    void menuDefaultsToTwoLinesAndSavesTheChoice()
    {
        auto *menu = new Gui::MessageRowsMenu(m_view, config(), m_view);
        QVERIFY(menu->isMultiLine());
        QVERIFY(m_view->multiLineRows());
        QVERIFY(m_view->isHeaderHidden());
        QVERIFY(menu->senderInitialsAction()->isChecked());
        QVERIFY(menu->senderInitialsAction()->isEnabled());

        menu->setMultiLine(false);
        QVERIFY(!m_view->multiLineRows());
        QVERIFY(!m_view->isHeaderHidden());
        // Initials exist only in two-line rows
        QVERIFY(!menu->senderInitialsAction()->isEnabled());
        menu->setMultiLine(true);
        menu->senderInitialsAction()->trigger();
        menu->setMultiLine(false);

        config()->reparseConfiguration();
        KConfigGroup settings(config(), QStringLiteral("Appearance"));
        QCOMPARE(settings.readEntry("MessageListRows", QString()), QStringLiteral("columns"));
        QCOMPARE(settings.readEntry("MessageListSenderInitials", true), false);

        // Another window starts with the saved choice
        Gui::MsgListView other(nullptr, m_tags);
        other.setModel(m_model);
        Gui::MessageRowsMenu restored(&other, config());
        QVERIFY(!restored.isMultiLine());
        QVERIFY(!other.multiLineRows());
        QVERIFY(!restored.senderInitialsAction()->isChecked());
    }

    void twoLineRowsAreTallerAndSpanTheView()
    {
        const QModelIndex first = m_model->index(0, 0);
        const int singleLine = m_view->visualRect(first).height();
        m_view->setMultiLineRows(true);
        QTRY_VERIFY(m_view->visualRect(first).height() >= 2 * m_view->fontMetrics().height());
        QVERIFY(m_view->visualRect(first).height() > singleLine);
        // Any column is painted, and repainted, as the whole row
        const QRect date = m_view->visualRect(m_model->index(0, MsgListModel::DATE));
        QCOMPARE(date.left(), 0);
        QCOMPARE(date.width(), m_view->viewport()->width());
        QVERIFY(!m_view->viewport()->grab().isNull());

        m_view->setMultiLineRows(false);
        QTRY_COMPARE(m_view->visualRect(first).height(), singleLine);
    }

    void cardsAreTallerTwoLineRowsAndSaveTheStyle()
    {
        auto *menu = new Gui::MessageRowsMenu(m_view, config(), m_view);
        QSignalSpy appearance(menu, &Gui::MessageRowsMenu::appearanceChanged);
        const QModelIndex first = m_model->index(0, 0);
        QVERIFY(!menu->isCards());
        QTRY_VERIFY(m_view->visualRect(first).height() > 0);
        const int rowHeight = m_view->visualRect(first).height();

        menu->setCards(true);
        QVERIFY(menu->isCards());
        QVERIFY(menu->isMultiLine());
        QVERIFY(m_view->cards());
        QVERIFY(m_view->multiLineRows());
        QVERIFY(menu->senderInitialsAction()->isEnabled());
        // A card has a margin and a frame around the two lines
        QTRY_VERIFY(m_view->visualRect(first).height() > rowHeight);
        QVERIFY(!m_view->viewport()->grab().isNull());
        QCOMPARE(appearance.count(), 1);
        config()->reparseConfiguration();
        QCOMPARE(KConfigGroup(config(), QStringLiteral("Theme")).readEntry("MessageListStyle", QString()),
                 QStringLiteral("Cards"));
        QCOMPARE(KConfigGroup(config(), QStringLiteral("Appearance")).readEntry("MessageListRows", QString()),
                 QStringLiteral("multiline"));

        // Columns keep the saved style, and two-line rows replace it
        menu->setMultiLine(false);
        QVERIFY(!m_view->cards());
        QCOMPARE(appearance.count(), 1);
        config()->reparseConfiguration();
        QCOMPARE(KConfigGroup(config(), QStringLiteral("Theme")).readEntry("MessageListStyle", QString()),
                 QStringLiteral("Cards"));
        menu->setMultiLine(true);
        QVERIFY(!menu->isCards());
        QCOMPARE(appearance.count(), 2);
        config()->reparseConfiguration();
        // Rows are the default style, which is not written
        QVERIFY(!KConfigGroup(config(), QStringLiteral("Theme")).hasKey("MessageListStyle"));
        QTRY_COMPARE(m_view->visualRect(first).height(), rowHeight);
    }

    void indicatorsActLikeTheirColumns()
    {
        m_view->setMultiLineRows(true);
        // Without columns, a click anywhere else in the row reaches the message, and the indicators reach their columns
        const QSet<int> columns = clickableColumns(0);
        QCOMPARE(columns, QSet<int>({MsgListModel::SUBJECT, MsgListModel::SEEN, MsgListModel::FLAGGED}));

        QPoint flag;
        const QRect rect = m_view->visualRect(m_model->index(0, 0));
        for (int x = rect.right(); x >= rect.left() && flag.isNull(); --x) {
            for (int y = rect.top(); y <= rect.bottom(); ++y) {
                if (m_view->indexAt(QPoint(x, y)).column() == MsgListModel::FLAGGED) {
                    flag = QPoint(x, y);
                    break;
                }
            }
        }
        QVERIFY(!flag.isNull());
        // The flag is near the trailing edge, after the subject's space
        QVERIFY(flag.x() > rect.center().x());
        QSignalSpy clicked(m_view, &QAbstractItemView::clicked);
        QTest::mouseClick(m_view->viewport(), Qt::LeftButton, {}, flag);
        QCOMPARE(clicked.size(), 1);
        QCOMPARE(clicked.constFirst().constFirst().toModelIndex().column(), int(MsgListModel::FLAGGED));

        // Beyond the columns, which may be narrower than the view, rows still respond
        m_view->header()->resizeSection(MsgListModel::SUBJECT, 20);
        QCOMPARE(m_view->indexAt(QPoint(rect.center().x(), rect.center().y())).row(), 0);

        m_view->setMultiLineRows(false);
        QCOMPARE(m_view->indexAt(QPoint(m_view->header()->sectionViewportPosition(MsgListModel::DATE) + 1, rect.center().y())).column(),
                 int(MsgListModel::DATE));
    }

    void rightToLeftMirrorsTheIndicators()
    {
        m_view->setLayoutDirection(Qt::RightToLeft);
        m_view->setMultiLineRows(true);
        const QRect rect = m_view->visualRect(m_model->index(0, 0));
        int flagX = -1;
        for (int x = rect.left(); x <= rect.right() && flagX < 0; ++x) {
            for (int y = rect.top(); y <= rect.bottom(); ++y) {
                if (m_view->indexAt(QPoint(x, y)).column() == MsgListModel::FLAGGED) {
                    flagX = x;
                    break;
                }
            }
        }
        QVERIFY(flagX >= 0);
        QVERIFY(flagX < rect.center().x());
    }

    void indicatorColumnsHaveAccessibleNames()
    {
        Imap::Mailbox::PrettyMsgListModel pretty(this);
        pretty.setSourceModel(m_model);
        QCOMPARE(pretty.index(0, MsgListModel::SEEN).data(Qt::AccessibleTextRole).toString(), QStringLiteral("Unread"));
        QCOMPARE(pretty.index(0, MsgListModel::FLAGGED).data(Qt::AccessibleTextRole).toString(), QStringLiteral("Flagged"));
        QCOMPARE(pretty.index(0, MsgListModel::ATTACHMENT).data(Qt::AccessibleTextRole).toString(), QStringLiteral("Has attachments"));
        QCOMPARE(pretty.index(1, MsgListModel::SEEN).data(Qt::AccessibleTextRole).toString(), QStringLiteral("Read"));
        QCOMPARE(pretty.index(1, MsgListModel::FLAGGED).data(Qt::AccessibleTextRole).toString(), QStringLiteral("Not flagged"));
        QVERIFY(!pretty.index(1, MsgListModel::ATTACHMENT).data(Qt::AccessibleTextRole).isValid());
    }

    void junkRowsAreMarked()
    {
        const auto setFlags = [this](int row, const QStringList &flags) {
            for (int column = 0; column < MsgListModel::COLUMN_COUNT; ++column)
                m_model->item(row, column)->setData(flags, Imap::Mailbox::RoleMessageFlags);
        };
        Imap::Mailbox::PrettyMsgListModel pretty(this);
        pretty.setSourceModel(m_model);
        const auto description = [&pretty](int row) {
            return pretty.index(row, MsgListModel::SUBJECT).data(Qt::AccessibleDescriptionRole).toString();
        };
        QVERIFY(description(0).isEmpty());

        setFlags(0, {QStringLiteral("$Junk")});
        QCOMPARE(description(0), QStringLiteral("Marked as junk"));
        QVERIFY(description(1).isEmpty());
        // Both row styles paint the mark
        m_view->setMultiLineRows(true);
        QVERIFY(!m_view->viewport()->grab().isNull());
        m_view->setMultiLineRows(false);
        QVERIFY(!m_view->viewport()->grab().isNull());

        // The user's override wins
        setFlags(0, {QStringLiteral("$Junk"), QStringLiteral("$NotJunk")});
        QVERIFY(description(0).isEmpty());
    }
};

QTEST_MAIN(MessageRowsTest)
#include "test_MessageRows.moc"
