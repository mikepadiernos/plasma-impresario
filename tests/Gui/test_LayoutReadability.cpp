/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Gui/AccountSidebar.h"
#include "Gui/DensityDelegate.h"
#include "Gui/DensityMenu.h"
#include "Gui/MailBoxTreeView.h"
#include "Gui/MsgListView.h"
#include "Gui/Window.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "configure.cmake.h"
#ifdef IMPRESARIO_HAVE_WEBENGINE
#include "Gui/WebEngineMessageProfile.h"
#endif
#include "static_plugins.h"

#include <KLocalizedString>
#include <QAction>
#include <QApplication>
#include <QScopeGuard>
#include <QSettings>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

/** @short Compact and comfortable layouts stay usable at common window sizes,
text sizes and display scales

The test runs once at scale 1 and once on a 2560x1600 screen at 200%, as
described in offscreen-hidpi.json; window sizes are logical. Every visible
pane keeps room, rows stay tall enough for their text, and the panes keep
keyboard focus.
*/
class LayoutReadabilityTest : public QObject
{
    Q_OBJECT
private slots:
    void layoutsStayReadable_data()
    {
        QTest::addColumn<QSize>("size");
        QTest::addColumn<QByteArray>("layout");
        QTest::addColumn<bool>("comfortable");
        QTest::addColumn<int>("pointSize");
        const QList<QPair<const char *, QSize>> sizes = {
            {"small", QSize(800, 600)},
            {"laptop", QSize(1366, 768)},
            {"large", QSize(2560, 1440)},
        };
        for (const auto &[sizeName, size] : sizes) {
            for (const char *layout : {"slotLayoutWide", "slotLayoutCompact"}) {
                for (bool comfortable : {false, true}) {
                    for (int pointSize : {10, 16}) {
                        QTest::addRow(
                            "%s-%s-%s-%dpt", sizeName, layout + 10, comfortable ? "comfortable" : "compact", pointSize)
                            << size << QByteArray(layout) << comfortable << pointSize;
                    }
                }
            }
        }
    }

    void layoutsStayReadable()
    {
        QFETCH(QSize, size);
        QFETCH(QByteArray, layout);
        QFETCH(bool, comfortable);
        QFETCH(int, pointSize);

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("legacy.ini")), QSettings::IniFormat);
        settings.setValue(Common::SettingsNames::imapMethodKey, Common::SettingsNames::methodSSL);
        settings.setValue(Common::SettingsNames::imapStartMode, Common::SettingsNames::netOffline);
        settings.setValue(Common::SettingsNames::guiShowSystray, false);
        settings.setValue(Common::SettingsNames::msaMethodKey, Common::SettingsNames::methodSMTP);

        const QFont originalFont = QApplication::font();
        QFont font = originalFont;
        font.setPointSize(pointSize);
        QApplication::setFont(font);
        auto restoreFont = qScopeGuard([&] { QApplication::setFont(originalFont); });

        Gui::MainWindow window(&settings);
        window.resize(size);
        window.show();
        QVERIFY(QMetaObject::invokeMethod(&window, layout.constData()));
        auto *menu = window.findChild<Gui::DensityMenu *>();
        QVERIFY(menu);
        QCOMPARE(menu->actions().size(), 2);
        menu->actions().at(comfortable ? 1 : 0)->trigger();
        QCoreApplication::processEvents();

        auto *mailboxes = window.findChild<Gui::MailBoxTreeView *>();
        auto *messages = window.findChild<Gui::MsgListView *>(QStringLiteral("messageListView"));
        auto *reader = window.findChild<QSplitter *>(QStringLiteral("readerColumn"));
        QVERIFY(mailboxes && messages && reader);

        // Each pane keeps enough room to show a line of text, however the window is
        // sized
        const int lineHeight = QFontMetrics(messages->font()).height();
        const int minimumWidth = QFontMetrics(messages->font()).averageCharWidth() * 8;
        for (QWidget *pane :
            {static_cast<QWidget *>(mailboxes), static_cast<QWidget *>(messages), static_cast<QWidget *>(reader)}) {
            QVERIFY2(pane->isVisible(), pane->metaObject()->className());
            QVERIFY2(pane->width() >= minimumWidth,
                qPrintable(QStringLiteral("%1 is %2 px wide, below %3")
                        .arg(QString::fromLatin1(pane->metaObject()->className()))
                        .arg(pane->width())
                        .arg(minimumWidth)));
            QVERIFY2(pane->height() >= lineHeight * 2,
                qPrintable(QStringLiteral("%1 is %2 px high, below %3")
                        .arg(QString::fromLatin1(pane->metaObject()->className()))
                        .arg(pane->height())
                        .arg(lineHeight * 2)));
            QVERIFY2(pane->focusPolicy() & Qt::TabFocus || pane == reader, pane->metaObject()->className());
        }
        QVERIFY(window.width() <= qMax(size.width(), window.minimumSizeHint().width()));
        const int expectedRatio = qEnvironmentVariableIsSet("IMPRESARIO_TEST_DPR")
            ? qEnvironmentVariableIntValue("IMPRESARIO_TEST_DPR")
            : 1;
        QCOMPARE(qRound(window.devicePixelRatio()), expectedRatio);

        // Rows are never shorter than their text, in either density
        QStandardItemModel model(1, 1);
        model.setData(model.index(0, 0), QStringLiteral("A reasonably long subject line"));
        for (QTreeView *view : {static_cast<QTreeView *>(mailboxes), static_cast<QTreeView *>(messages)}) {
            auto *delegate = qobject_cast<Gui::DensityDelegate *>(view->itemDelegate());
            QVERIFY(delegate);
            QStyleOptionViewItem option;
            option.initFrom(view);
            const int rowHeight = delegate->sizeHint(option, model.index(0, 0)).height();
            const int textHeight = QFontMetrics(view->font()).height();
            QVERIFY2(rowHeight >= textHeight,
                qPrintable(QStringLiteral("%1 rows are %2 px for %3 px text")
                        .arg(view->objectName())
                        .arg(rowHeight)
                        .arg(textHeight)));
        }
        window.close();
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
    qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent/plasma-impresario-test");
    QStandardPaths::setTestModeEnabled(true);
#ifdef IMPRESARIO_HAVE_WEBENGINE
    Gui::WebEngineMessageProfile::registerScheme();
#endif
    Common::registerMetaTypes();
    QApplication app(argc, argv);
    for (auto location :
        {QStandardPaths::GenericConfigLocation, QStandardPaths::AppDataLocation, QStandardPaths::CacheLocation}) {
        if (!QStandardPaths::writableLocation(location).startsWith(home.path() + QLatin1Char('/')))
            return 1;
    }
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(license);
    KLocalizedString::setApplicationDomain("plasma-impresario");
    if (!Imap::Mailbox::SystemNetworkWatcher::init())
        return 1;
    LayoutReadabilityTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_LayoutReadability.moc"
