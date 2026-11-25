/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QAccessible>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtTest>

#include <KLocalizedString>

#include "Gui/InlineNotificationBar.h"

using Gui::InlineNotificationBar;

class InlineNotificationBarTest : public QObject
{
    Q_OBJECT

private slots:
    void hiddenUntilShown();
    void showsTextAndKind();
    void actionButtonsRunCallbacks();
    void newMessageReplacesButtons();
    void closeButtonDismisses();
    void infoHidesAfterTimeout();
    void infoStaysWhileFocused();
    void errorPersists();
    void zeroTimeoutKeepsInfo();
    void accessibleNames();
};

namespace {

struct Fixture {
    QWidget window;
    InlineNotificationBar *bar;
    QLineEdit *outside;

    Fixture()
    {
        auto *layout = new QVBoxLayout(&window);
        bar = new InlineNotificationBar(&window);
        outside = new QLineEdit(&window);
        layout->addWidget(bar);
        layout->addWidget(outside);
        window.show();
    }
};

}

void InlineNotificationBarTest::hiddenUntilShown()
{
    Fixture f;
    QVERIFY(f.bar->isHidden());
    QVERIFY(!f.bar->isAutoHideActive());
}

void InlineNotificationBarTest::showsTextAndKind()
{
    Fixture f;
    f.bar->showInfo(QStringLiteral("Moved 3 messages to Trash"));
    QVERIFY(f.bar->isVisible());
    QCOMPARE(f.bar->kind(), InlineNotificationBar::Kind::Info);
    QCOMPARE(f.bar->text(), QStringLiteral("Moved 3 messages to Trash"));
    auto *label = f.bar->findChild<QLabel *>(QStringLiteral("inlineNotificationText"));
    QVERIFY(label);
    QCOMPARE(label->text(), QStringLiteral("Moved 3 messages to Trash"));
    QVERIFY(f.bar->actionButtons().isEmpty());

    f.bar->showError(QStringLiteral("Mailbox does not exist"));
    QCOMPARE(f.bar->kind(), InlineNotificationBar::Kind::Error);
    QCOMPARE(f.bar->text(), QStringLiteral("Mailbox does not exist"));
}

void InlineNotificationBarTest::actionButtonsRunCallbacks()
{
    Fixture f;
    int undoCalls = 0;
    int openCalls = 0;
    QSignalSpy dismissed(f.bar, &InlineNotificationBar::dismissed);
    f.bar->showInfo(QStringLiteral("Moved 1 message to Trash"),
                    {{QStringLiteral("&Undo"), QStringLiteral("edit-undo"), QStringLiteral("undoButton"),
                      [&undoCalls]() { ++undoCalls; }},
                     {QStringLiteral("Open Trash"), QString(), QString(), [&openCalls]() { ++openCalls; }}});
    QCOMPARE(f.bar->actionButtons().size(), 2);
    QPushButton *undo = f.bar->findChild<QPushButton *>(QStringLiteral("undoButton"));
    QVERIFY(undo);
    QCOMPARE(undo, f.bar->actionButtons().at(0));
    QVERIFY(undo->isVisible());

    QTest::mouseClick(undo, Qt::LeftButton);
    QCOMPARE(undoCalls, 1);
    QCOMPARE(openCalls, 0);
    // Using an action hides the bar
    QVERIFY(f.bar->isHidden());
    QCOMPARE(dismissed.size(), 1);
}

void InlineNotificationBarTest::newMessageReplacesButtons()
{
    Fixture f;
    bool firstCalled = false;
    f.bar->showInfo(QStringLiteral("first"), {{QStringLiteral("First"), QString(), QString(),
                                                [&firstCalled]() { firstCalled = true; }}});
    QPointer<QPushButton> first = f.bar->actionButtons().value(0);
    QVERIFY(first);

    // A callback that shows the next message must not crash by deleting its own button
    bool secondCalled = false;
    f.bar->showInfo(QStringLiteral("second"), {{QStringLiteral("Second"), QString(), QString(), [&]() {
        secondCalled = true;
        f.bar->showError(QStringLiteral("third"));
    }}});
    QVERIFY(f.bar->actionButtons().size() == 1);
    QPushButton *second = f.bar->actionButtons().at(0);
    QVERIFY(second != first);
    QTest::mouseClick(second, Qt::LeftButton);
    QVERIFY(secondCalled);
    QVERIFY(!firstCalled);
    QVERIFY(f.bar->isVisible());
    QCOMPARE(f.bar->text(), QStringLiteral("third"));
    QVERIFY(f.bar->actionButtons().isEmpty());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(!first);
}

void InlineNotificationBarTest::closeButtonDismisses()
{
    Fixture f;
    QSignalSpy dismissed(f.bar, &InlineNotificationBar::dismissed);
    f.bar->showError(QStringLiteral("Cannot move"));
    QVERIFY(f.bar->closeButton());
    QCOMPARE(f.bar->closeButton()->objectName(), QStringLiteral("inlineNotificationClose"));
    QTest::mouseClick(f.bar->closeButton(), Qt::LeftButton);
    QVERIFY(f.bar->isHidden());
    QCOMPARE(dismissed.size(), 1);

    // Dismissing a hidden bar does not report it again
    f.bar->dismiss();
    QCOMPARE(dismissed.size(), 1);
}

void InlineNotificationBarTest::infoHidesAfterTimeout()
{
    Fixture f;
    QCOMPARE(f.bar->autoHideTimeout(), 8000);
    f.bar->setAutoHideTimeout(50);
    f.outside->setFocus();
    f.bar->showInfo(QStringLiteral("Archived 2 messages"));
    QVERIFY(f.bar->isAutoHideActive());
    QTRY_VERIFY_WITH_TIMEOUT(f.bar->isHidden(), 2000);
}

void InlineNotificationBarTest::infoStaysWhileFocused()
{
    Fixture f;
    QApplication::setActiveWindow(&f.window);
    f.bar->setAutoHideTimeout(50);
    f.bar->showInfo(QStringLiteral("Moved 1 message to Trash"),
                    {{QStringLiteral("Undo"), QString(), QString(), []() {}}});
    f.bar->actionButtons().at(0)->setFocus();
    if (QApplication::focusWidget() != f.bar->actionButtons().at(0))
        QSKIP("The platform does not move the keyboard focus");
    QTest::qWait(300);
    QVERIFY(f.bar->isVisible());
    QVERIFY(f.bar->isAutoHideActive());

    f.outside->setFocus();
    QTRY_VERIFY_WITH_TIMEOUT(f.bar->isHidden(), 2000);
}

void InlineNotificationBarTest::errorPersists()
{
    Fixture f;
    f.bar->setAutoHideTimeout(50);
    f.bar->showError(QStringLiteral("Cannot archive: Mailbox does not exist"));
    QVERIFY(!f.bar->isAutoHideActive());
    QTest::qWait(200);
    QVERIFY(f.bar->isVisible());
}

void InlineNotificationBarTest::zeroTimeoutKeepsInfo()
{
    Fixture f;
    f.bar->setAutoHideTimeout(0);
    f.bar->showInfo(QStringLiteral("Expunge removes them"));
    QVERIFY(!f.bar->isAutoHideActive());
    QTest::qWait(100);
    QVERIFY(f.bar->isVisible());
}

void InlineNotificationBarTest::accessibleNames()
{
    Fixture f;
    f.bar->showInfo(QStringLiteral("Moved 3 messages to Trash"),
                    {{QStringLiteral("&Undo"), QString(), QString(), []() {}}});
    QCOMPARE(f.bar->objectName(), QStringLiteral("inlineNotificationBar"));
    QCOMPARE(f.bar->accessibleName(), QStringLiteral("Notification"));
    QCOMPARE(f.bar->accessibleDescription(), QStringLiteral("Moved 3 messages to Trash"));
    QCOMPARE(f.bar->closeButton()->accessibleName(), QStringLiteral("Close notification"));
    QVERIFY(!f.bar->closeButton()->toolTip().isEmpty());
    QCOMPARE(f.bar->actionButtons().at(0)->accessibleName(), QStringLiteral("Undo"));
}

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
    InlineNotificationBarTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_InlineNotificationBar.moc"
