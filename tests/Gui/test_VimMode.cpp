/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/VimMode.h"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTest>
#include <QTreeView>
#include <QVBoxLayout>

using UiUtils::Keymap::Context;

namespace {

/** A window with a list Vim mode handles and a field it leaves alone */
struct Window {
    QWidget window;
    QTreeView *tree = new QTreeView(&window);
    QLineEdit *field = new QLineEdit(&window);
    QStandardItemModel model;

    Window()
    {
        auto *layout = new QVBoxLayout(&window);
        layout->addWidget(tree);
        layout->addWidget(field);
        for (int i = 0; i < 5; ++i)
            model.appendRow(new QStandardItem(QStringLiteral("Message %1").arg(i)));
        tree->setModel(&model);
        tree->setCurrentIndex(model.index(0, 0));
    }

    bool show()
    {
        window.show();
        window.activateWindow();
        if (!QTest::qWaitForWindowActive(&window))
            return false;
        tree->setFocus();
        return QTest::qWaitFor([this]() { return tree->hasFocus(); });
    }

    Gui::VimMode::ContextResolver resolver()
    {
        return [this](QWidget *focus) -> std::optional<Context> {
            if (focus == tree)
                return Context::MessageList;
            return std::nullopt;
        };
    }
};

}

class VimModeTest : public QObject
{
    Q_OBJECT
private slots:
    void offByDefault()
    {
        Window w;
        Gui::VimMode vim(&w.window, w.resolver());
        QVERIFY(!vim.isEnabled());
        QSignalSpy commands(&vim, &Gui::VimMode::commandTriggered);
        QVERIFY(w.show());
        QTest::keyClick(w.tree, Qt::Key_J);
        QCOMPARE(commands.count(), 0);
    }

    void singleKeysRunCommands()
    {
        Window w;
        Gui::VimMode vim(&w.window, w.resolver());
        vim.setEnabled(true);
        QSignalSpy commands(&vim, &Gui::VimMode::commandTriggered);
        QVERIFY(w.show());
        QTest::keyClick(w.tree, Qt::Key_J);
        QCOMPARE(commands.count(), 1);
        QCOMPARE(commands.first().at(0).toString(), QStringLiteral("move.down"));
        QCOMPARE(commands.first().at(1).value<Context>(), Context::MessageList);
        // Shift+G is "G"
        QTest::keyClick(w.tree, 'G', Qt::ShiftModifier);
        QCOMPARE(commands.count(), 2);
        QCOMPARE(commands.last().at(0).toString(), QStringLiteral("move.bottom"));
        QTest::keyClick(w.tree, Qt::Key_D, Qt::ControlModifier);
        QCOMPARE(commands.last().at(0).toString(), QStringLiteral("page.down"));
    }

    void sequencesWaitForTheirNextKey()
    {
        Window w;
        Gui::VimMode vim(&w.window, w.resolver());
        vim.setEnabled(true);
        vim.setHints(false, 0);
        QSignalSpy commands(&vim, &Gui::VimMode::commandTriggered);
        QSignalSpy pending(&vim, &Gui::VimMode::pendingChanged);
        QVERIFY(w.show());
        QTest::keyClick(w.tree, Qt::Key_G);
        QCOMPARE(commands.count(), 0);
        QCOMPARE(vim.pending(), QStringLiteral("g"));
        QCOMPARE(pending.last().first().toString(), QStringLiteral("g"));
        QTest::keyClick(w.tree, Qt::Key_G);
        QCOMPARE(commands.count(), 1);
        QCOMPARE(commands.first().first().toString(), QStringLiteral("move.top"));
        QVERIFY(vim.pending().isEmpty());
        QVERIFY(pending.last().first().toString().isEmpty());

        // A key which continues nothing ends the sequence and is used up by it
        QTest::keyClick(w.tree, Qt::Key_G);
        QTest::keyClick(w.tree, Qt::Key_Z);
        QCOMPARE(commands.count(), 1);
        QVERIFY(vim.pending().isEmpty());

        // Escape drops an incomplete sequence
        QTest::keyClick(w.tree, Qt::Key_G);
        QTest::keyClick(w.tree, Qt::Key_Escape);
        QVERIFY(vim.pending().isEmpty());
        QTest::keyClick(w.tree, Qt::Key_G);
        QTest::keyClick(w.tree, Qt::Key_T);
        QCOMPARE(commands.last().first().toString(), QStringLiteral("tab.next"));
    }

    void incompleteSequencesTimeOut()
    {
        Window w;
        Gui::VimMode vim(&w.window, w.resolver());
        vim.setEnabled(true);
        vim.setHints(false, 0);
        vim.setTimeout(50);
        QSignalSpy commands(&vim, &Gui::VimMode::commandTriggered);
        QVERIFY(w.show());
        QTest::keyClick(w.tree, Qt::Key_G);
        QCOMPARE(vim.pending(), QStringLiteral("g"));
        QTRY_VERIFY(vim.pending().isEmpty());
        // The next g starts again rather than completing gg
        QTest::keyClick(w.tree, Qt::Key_G);
        QCOMPARE(commands.count(), 0);
    }

    void hintsListWhatMayFollow()
    {
        Window w;
        Gui::VimMode vim(&w.window, w.resolver());
        vim.setEnabled(true);
        vim.setTimeout(0);
        vim.setHints(true, 0);
        QVERIFY(w.show());
        auto *hints = w.window.findChild<QLabel *>(QStringLiteral("vimHints"));
        QVERIFY(hints);
        QVERIFY(!hints->isVisible());
        QTest::keyClick(w.tree, Qt::Key_G);
        QTRY_VERIFY(hints->isVisible());
        QVERIFY(hints->text().contains(QStringLiteral("gg")));
        QVERIFY(hints->text().contains(UiUtils::Keymap::description(QStringLiteral("move.top"))));
        QVERIFY(hints->text().contains(QStringLiteral("gi")));
        QTest::keyClick(w.tree, Qt::Key_Escape);
        QVERIFY(!hints->isVisible());
    }

    void textFieldsKeepTheirKeys()
    {
        Window w;
        Gui::VimMode vim(&w.window, w.resolver());
        vim.setEnabled(true);
        QSignalSpy commands(&vim, &Gui::VimMode::commandTriggered);
        QVERIFY(w.show());
        w.field->setFocus();
        QTRY_VERIFY(w.field->hasFocus());
        QTest::keyClicks(w.field, QStringLiteral("jggk"));
        QCOMPARE(w.field->text(), QStringLiteral("jggk"));
        QCOMPARE(commands.count(), 0);
        QVERIFY(Gui::VimMode::takesText(w.field));
        QVERIFY(!Gui::VimMode::takesText(w.tree));
    }

    void commandsComeBeforeTheWindowsShortcuts()
    {
        Window w;
        auto *archive = new QAction(&w.window);
        archive->setShortcut(QKeySequence(Qt::Key_A));
        archive->setShortcutContext(Qt::WindowShortcut);
        w.window.addAction(archive);
        QSignalSpy archived(archive, &QAction::triggered);
        Gui::VimMode vim(&w.window, w.resolver());
        QSignalSpy commands(&vim, &Gui::VimMode::commandTriggered);
        QVERIFY(w.show());

        QTest::keyClick(w.tree, Qt::Key_A);
        QCOMPARE(archived.count(), 1);
        QCOMPARE(commands.count(), 0);

        vim.setEnabled(true);
        QTest::keyClick(w.tree, Qt::Key_A);
        QCOMPARE(archived.count(), 1);
        QCOMPARE(commands.count(), 1);
        QCOMPARE(commands.first().first().toString(), QStringLiteral("message.archive"));

        // A key with no command keeps its shortcut
        UiUtils::Keymap::Bindings bindings = UiUtils::Keymap::defaultBindings();
        bindings.insert(QStringLiteral("message.archive"), QString());
        vim.setBindings(bindings);
        QTest::keyClick(w.tree, Qt::Key_A);
        QCOMPARE(archived.count(), 2);
        QCOMPARE(commands.count(), 1);
    }

    void sentKeysAreNotCommands()
    {
        Window w;
        Gui::VimMode vim(&w.window, w.resolver());
        vim.setEnabled(true);
        connect(&vim, &Gui::VimMode::commandTriggered, &vim, [&vim, &w](const QString &command) {
            if (command == QLatin1String("move.down"))
                vim.sendKey(w.tree, Qt::Key_Down);
        });
        QSignalSpy commands(&vim, &Gui::VimMode::commandTriggered);
        QVERIFY(w.show());
        QTest::keyClick(w.tree, Qt::Key_J);
        QCOMPARE(w.tree->currentIndex().row(), 1);
        QTest::keyClick(w.tree, Qt::Key_J);
        QCOMPARE(w.tree->currentIndex().row(), 2);
        QCOMPARE(commands.count(), 2);
    }
};

QTEST_MAIN(VimModeTest)

#include "test_VimMode.moc"
