/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/DensityDelegate.h"
#include "Gui/DensityMenu.h"
#include "Gui/ColoredItemDelegate.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeView>

class DensityMenuTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("plasma-impresario");
    }

    void changesBothViewsWithoutChangingSelection()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        auto config = KSharedConfig::openConfig(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        QStandardItemModel model(2, 1);
        model.setData(model.index(0, 0), QStringLiteral("Inbox"));
        model.setData(model.index(1, 0), QStringLiteral("A message"));
        QTreeView mailboxes;
        QTreeView messages;
        for (auto *view : {&mailboxes, &messages}) {
            view->setModel(&model);
            if (view == &messages)
                view->setItemDelegate(new Gui::ColoredItemDelegate(view));
            else
                view->setItemDelegate(new Gui::DensityDelegate(view));
            view->setUniformRowHeights(true);
            view->setCurrentIndex(model.index(1, 0));
            view->show();
        }
        Gui::DensityMenu menu(&mailboxes, &messages, config);
        QCOMPARE(menu.actions().size(), 2);
        QCoreApplication::processEvents();
        const int compactMailboxHeight = mailboxes.visualRect(model.index(0, 0)).height();
        const int compactMessageHeight = messages.visualRect(model.index(0, 0)).height();
        QVERIFY(compactMailboxHeight > 0);
        QVERIFY(compactMessageHeight > 0);

        menu.actions().at(1)->trigger();
        QCoreApplication::processEvents();
        QVERIFY(mailboxes.visualRect(model.index(0, 0)).height() > compactMailboxHeight);
        QVERIFY(messages.visualRect(model.index(0, 0)).height() > compactMessageHeight);
        QCOMPARE(mailboxes.currentIndex(), model.index(1, 0));
        QCOMPARE(messages.currentIndex(), model.index(1, 0));

        menu.actions().at(0)->trigger();
        QCoreApplication::processEvents();
        QCOMPARE(mailboxes.visualRect(model.index(0, 0)).height(), compactMailboxHeight);
        QCOMPARE(messages.visualRect(model.index(0, 0)).height(), compactMessageHeight);
    }

    void restoresDensityAcrossMenus()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        auto config = KSharedConfig::openConfig(directory.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        QTreeView mailboxes;
        QTreeView messages;
        mailboxes.setItemDelegate(new Gui::DensityDelegate(&mailboxes));
        messages.setItemDelegate(new Gui::DensityDelegate(&messages));
        {
            Gui::DensityMenu menu(&mailboxes, &messages, config);
            QCOMPARE(menu.actions().size(), 2);
            QVERIFY(menu.actions().at(0)->isChecked());
            menu.actions().at(1)->trigger();
        }
        QVERIFY(config->sync());
        config->reparseConfiguration();
        Gui::DensityMenu restored(&mailboxes, &messages, config);
        QVERIFY(restored.actions().at(1)->isChecked());
    }
};

QTEST_MAIN(DensityMenuTest)
#include "test_DensityMenu.moc"
