/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QHeaderView>
#include <QPointer>
#include <QStandardItemModel>
#include <QtTest>

#include "Gui/MsgListView.h"
#include "Imap/Model/FavoriteTagsModel.h"
#include "Imap/Model/MsgListModel.h"

class MsgListViewTeardownTest : public QObject
{
    Q_OBJECT

private slots:
    /** @short Destroying a shown view must not deliver header signals to the partly destroyed view (#426) */
    void destroyingShownViewIsClean()
    {
        QStandardItemModel model(0, Imap::Mailbox::MsgListModel::COLUMN_COUNT);
        for (int row = 0; row < 8; ++row) {
            QList<QStandardItem *> items;
            for (int column = 0; column < Imap::Mailbox::MsgListModel::COLUMN_COUNT; ++column)
                items << new QStandardItem(QStringLiteral("%1/%2").arg(row).arg(column));
            model.appendRow(items);
        }
        Imap::Mailbox::FavoriteTagsModel tags;
        auto *view = new Gui::MsgListView(nullptr, &tags);
        view->setModel(&model);
        view->resize(400, 600);
        view->show();
        QVERIFY(QTest::qWaitForWindowExposed(view));
        view->resize(500, 300);
        QPointer<QHeaderView> header = view->header();
        delete view;
        QVERIFY(header.isNull());
    }

    /** @short A header that outlives its view stops calling back into it */
    void headerSignalsAfterDestructionAreIgnored()
    {
        Imap::Mailbox::FavoriteTagsModel tags;
        auto *view = new Gui::MsgListView(nullptr, &tags);
        QHeaderView *header = view->header();
        header->setParent(nullptr);
        delete view;
        Q_EMIT header->geometriesChanged();
        Q_EMIT header->sectionCountChanged(0, 1);
        delete header;
    }
};

QTEST_MAIN(MsgListViewTeardownTest)
#include "test_MsgListViewTeardown.moc"
