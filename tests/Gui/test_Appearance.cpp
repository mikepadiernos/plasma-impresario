/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/AppearanceManager.h"
#include "Gui/AppearanceStyle.h"
#include "Gui/MailTabBar.h"
#include "UiUtils/Color.h"
#include "UiUtils/SettingsStore.h"
#include "UiUtils/Theme.h"

#include <KConfig>
#include <KLocalizedString>
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

using Gui::AppearanceManager;
using Gui::AppearanceStyle;
namespace Theme = UiUtils::Theme;

class AppearanceTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanup();
    void defaultsLeavePlasmasLook();
    void accentColoursSelectionAndGoesBack();
    void fontsReachTheirWidgets();
    void menuMarkerReplacesTheArrow();
    void selectionStyles();
    void spacingScalesTheStyle();
    void animationsCanBeTurnedOff();
    void loadReadsTheConfigAndWorkspacesCanBeIgnored();
    void tabsShowTheirColourShapeAndIndicator();
    void statusBarStyles();

private:
    static Theme::Values with(std::initializer_list<std::pair<const char *, QVariant>> changes)
    {
        Theme::Values values = Theme::defaults();
        for (const auto &change : changes)
            values.insert(QLatin1String(change.first), change.second);
        return values;
    }
    static void apply(const Theme::Values &values)
    {
        AppearanceManager::instance()->apply(values, UiUtils::FolderColors::Colors());
    }
    QTemporaryDir m_home;
};

void AppearanceTest::initTestCase()
{
    KLocalizedString::setApplicationDomain("plasma-impresario");
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(m_home.isValid());
    AppearanceStyle::ensureInstalled();
    QVERIFY(qobject_cast<AppearanceStyle *>(QApplication::style()));
}

void AppearanceTest::cleanup()
{
    apply(Theme::defaults());
}

void AppearanceTest::defaultsLeavePlasmasLook()
{
    const QPalette before = QApplication::palette();
    apply(Theme::defaults());
    QCOMPARE(QApplication::palette(), before);
    QVERIFY(qApp->styleSheet().isEmpty());
    QVERIFY(!AppearanceManager::instance()->effectsSuspended());
    QCOMPARE(AppearanceManager::instance()->cornerRadius(), -1);
}

void AppearanceTest::accentColoursSelectionAndGoesBack()
{
    const QColor plasmaHighlight = QApplication::palette().color(QPalette::Highlight);
    apply(with({{"AccentColor", QStringLiteral("#c0392b")}}));
    QCOMPARE(QApplication::palette().color(QPalette::Active, QPalette::Highlight), QColor(0xc0, 0x39, 0x2b));
    QCOMPARE(AppearanceManager::instance()->accent(), QColor(0xc0, 0x39, 0x2b));
    apply(Theme::defaults());
    QCOMPARE(QApplication::palette().color(QPalette::Highlight), plasmaHighlight);
}

void AppearanceTest::fontsReachTheirWidgets()
{
    QWidget list;
    QWidget tree;
    AppearanceManager::instance()->registerFont(&list, AppearanceManager::FontTarget::MessageList);
    AppearanceManager::instance()->registerFont(&tree, AppearanceManager::FontTarget::FolderTree);
    QFont chosen(QStringLiteral("Serif"));
    chosen.setPointSize(17);
    apply(with({{"MessageListFont", chosen.toString()}}));
    QCOMPARE(list.font().pointSize(), 17);
    QVERIFY(tree.font().pointSize() != 17 || QApplication::font().pointSize() == 17);
    // Back to the default, the list follows its parent and Plasma again
    apply(Theme::defaults());
    QCOMPARE(list.font().resolveMask(), QFont().resolveMask());
}

void AppearanceTest::menuMarkerReplacesTheArrow()
{
    QMenu menu;
    menu.addAction(QStringLiteral("Other"));
    QAction main(QStringLiteral("Reply"));
    QToolButton arrow;
    QToolButton marked;
    for (QToolButton *button : {&arrow, &marked}) {
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setPopupMode(QToolButton::MenuButtonPopup);
        button->setMenu(&menu);
        button->setDefaultAction(&main);
    }
    AppearanceStyle::addMenuMarker(&marked);
    QCOMPARE(marked.popupMode(), QToolButton::DelayedPopup);
    QCOMPARE(marked.menu(), &menu);
    QVERIFY(!marked.accessibleDescription().isEmpty());
    // No room is kept for an arrow beside the button
    QVERIFY(marked.sizeHint().width() < arrow.sizeHint().width());
    // A button which only opens its menu keeps opening it at once
    QToolButton instant;
    instant.setPopupMode(QToolButton::InstantPopup);
    AppearanceStyle::addMenuMarker(&instant);
    QCOMPARE(instant.popupMode(), QToolButton::InstantPopup);
}

void AppearanceTest::selectionStyles()
{
    QVERIFY(AppearanceStyle::selectionIsFilled());
    apply(with({{"SelectionStyle", QStringLiteral("Outlined")}}));
    QVERIFY(!AppearanceStyle::selectionIsFilled());
    apply(with({{"SelectionStyle", QStringLiteral("AccentBar")}}));
    QVERIFY(!AppearanceStyle::selectionIsFilled());
    apply(with({{"SelectionStyle", QStringLiteral("Rounded")}}));
    QVERIFY(AppearanceStyle::selectionIsFilled());
}

void AppearanceTest::spacingScalesTheStyle()
{
    QStyle *style = QApplication::style();
    const int normal = style->pixelMetric(QStyle::PM_LayoutLeftMargin);
    QVERIFY(normal > 0);
    apply(with({{"SpacingScale", 150}}));
    QCOMPARE(style->pixelMetric(QStyle::PM_LayoutLeftMargin), Theme::scaled(normal, 150));
    apply(Theme::defaults());
    QCOMPARE(style->pixelMetric(QStyle::PM_LayoutLeftMargin), normal);
}

void AppearanceTest::animationsCanBeTurnedOff()
{
    QVERIFY(AppearanceManager::instance()->animationDuration(250) > 0);
    apply(with({{"Animations", QStringLiteral("Off")}}));
    QCOMPARE(AppearanceManager::instance()->animationDuration(250), 0);
}

void AppearanceTest::loadReadsTheConfigAndWorkspacesCanBeIgnored()
{
    KConfig config(m_home.filePath(QStringLiteral("plasma-impresariorc")), KConfig::SimpleConfig);
    UiUtils::SettingsStore::setTheme(config, with({{"StripeFolderTree", true}}));
    AppearanceManager::instance()->load(config);
    QVERIFY(AppearanceManager::instance()->value(QStringLiteral("StripeFolderTree")).toBool());
    QVERIFY(AppearanceManager::instance()->problems().isEmpty());
    AppearanceManager::instance()->setWorkspaceAppearanceDisabled(true);
    QVERIFY(AppearanceManager::instance()->workspaceAppearanceDisabled());
    AppearanceManager::instance()->load(config);
    QVERIFY(AppearanceManager::instance()->value(QStringLiteral("StripeFolderTree")).toBool());
    AppearanceManager::instance()->setWorkspaceAppearanceDisabled(false);
}

void AppearanceTest::tabsShowTheirColourShapeAndIndicator()
{
    Gui::MailTabBar bar;
    const QColor folder(0xf6, 0x74, 0x00);
    bar.setEntries({{QStringLiteral("Inbox"), QString(), folder}, {QStringLiteral("Archive"), QString(), QColor()}}, 0);
    bar.resize(320, bar.sizeHint().height());
    bar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&bar));
    QCOMPARE(bar.tabColor(0), folder);
    QVERIFY(!bar.tabColor(1).isValid());
    QVERIFY(!bar.tabColor(5).isValid());

    // Coloured text by default, readable on the window, and the palette's text without a colour
    const QColor window = bar.palette().color(QPalette::Window);
    QVERIFY(bar.tabTextColor(0).isValid());
    QVERIFY(UiUtils::contrastRatio(bar.tabTextColor(0), window) >= 4.4);
    QVERIFY(!bar.tabTextColor(1).isValid());
    const QImage plain = bar.grab().toImage();

    apply(with({{"TabColorStrength", QStringLiteral("Tint")}}));
    QVERIFY(!bar.tabTextColor(0).isValid());
    const QImage tinted = bar.grab().toImage();
    QVERIFY(tinted != plain);

    apply(with({{"TabColorStrength", QStringLiteral("Strong")}}));
    QCOMPARE(bar.tabTextColor(0), UiUtils::readableTextColor(folder));
    QVERIFY(bar.grab().toImage() != tinted);

    // A shape of the app's own, with each indicator
    QImage previous = plain;
    for (const char *shape : {"Rounded", "Square", "Flat"}) {
        apply(with({{"TabShape", QString::fromLatin1(shape)}}));
        const QImage line = bar.grab().toImage();
        QVERIFY2(line != previous, shape);
        apply(with({{"TabShape", QString::fromLatin1(shape)}, {"TabIndicator", QStringLiteral("ThickLine")}}));
        const QImage thick = bar.grab().toImage();
        QVERIFY2(thick != line, shape);
        apply(with({{"TabShape", QString::fromLatin1(shape)}, {"TabIndicator", QStringLiteral("None")}}));
        QVERIFY2(bar.grab().toImage() != line, shape);
        previous = line;
    }

    // Back to the defaults, the style draws the tabs as before
    apply(Theme::defaults());
    QCOMPARE(bar.grab().toImage(), plain);
}

void AppearanceTest::statusBarStyles()
{
    QStatusBar status;
    status.addWidget(new QLabel(QStringLiteral("Online")));
    status.addPermanentWidget(new QLabel(QStringLiteral("3 unread")));
    status.addPermanentWidget(new QLabel(QStringLiteral("Synced")));
    status.resize(360, status.sizeHint().height() + 6);
    status.show();
    QVERIFY(QTest::qWaitForWindowExposed(&status));
    const QImage style = status.grab().toImage();
    apply(with({{"StatusBarStyle", QStringLiteral("Separated")}}));
    const QImage separated = status.grab().toImage();
    QVERIFY(separated != style);
    apply(with({{"StatusBarStyle", QStringLiteral("Shaded")}}));
    const QImage shaded = status.grab().toImage();
    QVERIFY(shaded != style);
    QVERIFY(shaded != separated);
    apply(Theme::defaults());
    QCOMPARE(status.grab().toImage(), style);
}

QTEST_MAIN(AppearanceTest)
#include "test_Appearance.moc"
