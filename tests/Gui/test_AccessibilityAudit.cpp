/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Gui/SearchBuilderDialog.h"
#include "Gui/Window.h"
#include "Imap/Model/SystemNetworkWatcher.h"
#include "configure.cmake.h"
#ifdef IMPRESARIO_HAVE_WEBENGINE
#include "Gui/WebEngineMessageProfile.h"
#endif
#include "static_plugins.h"

#include <KLocalizedString>
#include <KPageWidget>
#include <KPageWidgetModel>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QSettings>
#include <QScrollBar>
#include <QSet>
#include <QShortcut>
#include <QShortcutEvent>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>

namespace {

QString describe(const QWidget *widget)
{
    QStringList path;
    for (const QWidget *w = widget; w; w = w->parentWidget()) {
        path.prepend(w->objectName().isEmpty()
                ? QString::fromLatin1(w->metaObject()->className())
                : QStringLiteral("%1#%2").arg(QString::fromLatin1(w->metaObject()->className()), w->objectName()));
        if (w->isWindow())
            break;
    }
    return path.join(QLatin1String(" > "));
}

bool isControl(const QWidget *widget)
{
    return qobject_cast<const QAbstractButton *>(widget) || qobject_cast<const QLineEdit *>(widget)
        || qobject_cast<const QComboBox *>(widget) || qobject_cast<const QAbstractItemView *>(widget)
        || qobject_cast<const QAbstractSpinBox *>(widget) || qobject_cast<const QAbstractSlider *>(widget)
        || qobject_cast<const QTextEdit *>(widget);
}

/** @short Visible controls of @p root which a screen reader would announce without a name */
QStringList unnamedControls(QWidget *root)
{
    QStringList found;
    QList<QWidget *> widgets = root->findChildren<QWidget *>();
    widgets.prepend(root);
    for (QWidget *widget : std::as_const(widgets)) {
        if (!widget->isVisible() || !isControl(widget) || widget->window() != root)
            continue;
        // Parts of a composite control, which Orca announces through the control itself
        if (qobject_cast<QScrollBar *>(widget) || qobject_cast<QHeaderView *>(widget))
            continue;
        QWidget *parent = widget->parentWidget();
        if (qobject_cast<QAbstractSpinBox *>(parent) || qobject_cast<QLineEdit *>(parent)
            || qobject_cast<QComboBox *>(parent))
            continue;
        QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(widget);
        if (!accessible || accessible->text(QAccessible::Name).trimmed().isEmpty())
            found.append(describe(widget) + QStringLiteral(" [tooltip: %1]").arg(widget->toolTip()));
    }
    return found;
}

/** @short Unnamed controls of the window @p root on each of its settings pages and tabs */
QStringList auditWindow(QWidget *root)
{
    QStringList found = unnamedControls(root);
    const auto pageWidgets = root->findChildren<KPageWidget *>();
    for (KPageWidget *pages : pageWidgets) {
        auto *model = qobject_cast<KPageWidgetModel *>(pages->model());
        if (!model)
            continue;
        QList<QModelIndex> pending{QModelIndex()};
        while (!pending.isEmpty()) {
            const QModelIndex parent = pending.takeFirst();
            for (int row = 0; row < model->rowCount(parent); ++row) {
                const QModelIndex index = model->index(row, 0, parent);
                pending.append(index);
                if (KPageWidgetItem *item = model->item(index)) {
                    pages->setCurrentPage(item);
                    QCoreApplication::processEvents();
                    found += unnamedControls(root);
                }
            }
        }
    }
    const auto tabWidgets = root->findChildren<QTabWidget *>();
    for (QTabWidget *tabs : tabWidgets) {
        for (int i = 0; i < tabs->count(); ++i) {
            if (!tabs->isTabEnabled(i))
                continue;
            tabs->setCurrentIndex(i);
            QCoreApplication::processEvents();
            if (tabs->isVisible())
                found += unnamedControls(root);
        }
    }
    found.removeDuplicates();
    return found;
}

/** @short Run @p open, audit the window which it opens and close that window again */
QStringList auditOpenedWindow(const std::function<void()> &open, QString *title)
{
    const QWidgetList before = QApplication::topLevelWidgets();
    QStringList found;
    bool seen = false;
    QTimer poll;
    poll.setInterval(50);
    QObject::connect(&poll, &QTimer::timeout, [&] {
        const QWidgetList now = QApplication::topLevelWidgets();
        for (QWidget *window : now) {
            if (seen || before.contains(window) || !window->isVisible() || qobject_cast<QMenu *>(window))
                continue;
            seen = true;
            *title = window->windowTitle();
            found = auditWindow(window);
            if (auto *dialog = qobject_cast<QDialog *>(window))
                dialog->reject();
            else
                window->close();
        }
    });
    poll.start();
    open();
    (void)QTest::qWaitFor([&] { return seen; }, 5000);
    poll.stop();
    if (!seen)
        found.append(QStringLiteral("no window opened"));
    return found;
}

}

/** @short Records ambiguous shortcut events and swallows every shortcut so that no action runs */
class ShortcutRecorder : public QObject
{
public:
    QStringList ambiguous;
    int delivered = 0;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() != QEvent::Shortcut)
            return false;
        ++delivered;
        if (static_cast<QShortcutEvent *>(event)->isAmbiguous()) {
            QString name = watched->objectName();
            if (auto *action = qobject_cast<QAction *>(watched); action && name.isEmpty())
                name = action->text();
            ambiguous.append(name.isEmpty() ? QString::fromLatin1(watched->metaObject()->className()) : name);
        }
        return true;
    }
};

/** @short Screen readers find a name for every control, and the keyboard reaches them

The main window and the dialogs opened from its menus are walked through their
accessibility interfaces, as Orca would see them through AT-SPI.
*/
class AccessibilityAuditTest : public QObject
{
    Q_OBJECT
private slots:
    void init()
    {
        QVERIFY(m_directory.isValid());
        m_settings
            = std::make_unique<QSettings>(m_directory.filePath(QStringLiteral("legacy.ini")), QSettings::IniFormat);
        m_settings->setValue(Common::SettingsNames::imapMethodKey, Common::SettingsNames::methodSSL);
        m_settings->setValue(Common::SettingsNames::imapStartMode, Common::SettingsNames::netOffline);
        m_settings->setValue(Common::SettingsNames::guiShowSystray, false);
        m_settings->setValue(Common::SettingsNames::msaMethodKey, Common::SettingsNames::methodSMTP);
    }

    void mainWindowControlsHaveNames()
    {
        Gui::MainWindow window(m_settings.get());
        window.resize(1366, 768);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const QStringList unnamed = unnamedControls(&window);
        for (const QString &line : unnamed)
            qWarning().noquote() << "unnamed:" << line;
        QVERIFY2(unnamed.isEmpty(), qPrintable(QString::number(unnamed.size())));
    }

    void dialogControlsHaveNames_data()
    {
        QTest::addColumn<QString>("action");
        for (const char *name :
            {"action_settings", "action_add_account", "action_search_index_status", "action_mail_rules",
                "action_activity", "action_configure_notifications", "action_find_saved_files", "action_go_to_folder",
                "action_compose_mail", "action_shortcut_overview", "action_configure_shortcuts", "action_about"}) {
            QTest::newRow(name) << QString::fromLatin1(name);
        }
        QTest::newRow("outbox") << QStringLiteral("outbox");
        QTest::newRow("searchBuilder") << QStringLiteral("searchBuilder");
    }

    void dialogControlsHaveNames()
    {
        QFETCH(QString, action);
        Gui::MainWindow window(m_settings.get());
        window.resize(1366, 768);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        std::function<void()> open;
        if (action == QLatin1String("outbox")) {
            open = [&window]() { window.showOutbox(); };
        } else if (action == QLatin1String("searchBuilder")) {
            // The builder opens from the search field once a folder is shown
            open = [&window]() {
                auto *dialog = new Gui::SearchBuilderDialog(&window);
                dialog->setAttribute(Qt::WA_DeleteOnClose);
                dialog->show();
            };
        } else {
            auto *trigger = window.findChild<QAction *>(action);
            QVERIFY2(trigger, qPrintable(action));
            QVERIFY(trigger->isEnabled());
            open = [trigger]() { trigger->trigger(); };
        }
        QString title;
        const QStringList unnamed = auditOpenedWindow(open, &title);
        for (const QString &line : unnamed)
            qWarning().noquote() << "unnamed in" << title << ":" << line;
        QVERIFY2(
            unnamed.isEmpty(), qPrintable(QStringLiteral("%1 unnamed controls in %2").arg(unnamed.size()).arg(title)));
    }

    void shortcutsAreUnambiguous()
    {
        Gui::MainWindow window(m_settings.get());
        window.resize(1366, 768);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QApplication::setActiveWindow(&window);

        ShortcutRecorder recorder;
        QList<QKeySequence> sequences;
        const auto actions = window.findChildren<QAction *>();
        for (QAction *action : actions) {
            action->installEventFilter(&recorder);
            if (!action->isEnabled())
                continue;
            for (const QKeySequence &sequence : action->shortcuts()) {
                if (!sequence.isEmpty() && !sequences.contains(sequence))
                    sequences.append(sequence);
            }
        }
        const auto shortcuts = window.findChildren<QShortcut *>();
        for (QShortcut *shortcut : shortcuts) {
            shortcut->installEventFilter(&recorder);
            if (!shortcut->isEnabled())
                continue;
            for (const QKeySequence &sequence : shortcut->keys()) {
                if (!sequence.isEmpty() && !sequences.contains(sequence))
                    sequences.append(sequence);
            }
        }
        QVERIFY(sequences.size() > 20);
        auto *messages = window.findChild<QAbstractItemView *>(QStringLiteral("messageListView"));
        QVERIFY(messages);

        QStringList ambiguous;
        for (const QKeySequence &sequence : std::as_const(sequences)) {
            recorder.ambiguous.clear();
            // Start each shortcut from the message list, where keyboard triage happens
            messages->setFocus();
            const int before = recorder.delivered;
            QTest::keySequence(messages, sequence);
            QCoreApplication::processEvents();
            if (recorder.delivered == before)
                qInfo().noquote() << "not handled as a shortcut from the message list:" << sequence.toString();
            if (!recorder.ambiguous.isEmpty())
                ambiguous.append(
                    QStringLiteral("%1: %2").arg(sequence.toString(), recorder.ambiguous.join(QLatin1String(", "))));
        }
        qInfo("%d of %lld shortcuts reached an action", recorder.delivered, static_cast<long long>(sequences.size()));
        // Every shortcut must arrive somewhere, or the test checks nothing
        QVERIFY(recorder.delivered >= sequences.size() / 2);
        for (const QString &line : std::as_const(ambiguous))
            qWarning().noquote() << "ambiguous:" << line;
        QVERIFY2(ambiguous.isEmpty(), qPrintable(QString::number(ambiguous.size())));
    }

    void tabReachesVisibleControls()
    {
        Gui::MainWindow window(m_settings.get());
        window.resize(1366, 768);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QApplication::setActiveWindow(&window);
        QVERIFY(QTest::qWaitForWindowActive(&window));

        QSet<QWidget *> reached;
        for (int i = 0; i < 300; ++i) {
            QWidget *focus = QApplication::focusWidget();
            if (!focus)
                break;
            reached.insert(focus);
            QTest::keyClick(focus, Qt::Key_Tab);
        }
        qInfo("Tab reached %lld widgets", static_cast<long long>(reached.size()));
        QVERIFY(reached.size() > 5);

        QStringList unreachable;
        const auto widgets = window.findChildren<QWidget *>();
        for (QWidget *widget : widgets) {
            if (!widget->isVisible() || !widget->isEnabled() || !(widget->focusPolicy() & Qt::TabFocus)
                || widget->window() != &window)
                continue;
            QWidget *target = widget;
            while (target->focusProxy())
                target = target->focusProxy();
            if (reached.contains(target) || reached.contains(widget))
                continue;
            // Parts of composite controls which take focus through their owner
            QWidget *parent = widget->parentWidget();
            if (qobject_cast<QAbstractSpinBox *>(parent) || qobject_cast<QComboBox *>(parent)
                || qobject_cast<QScrollBar *>(widget))
                continue;
            unreachable.append(describe(widget));
        }
        for (const QString &line : std::as_const(unreachable))
            qWarning().noquote() << "not reachable by Tab:" << line;
        QVERIFY2(unreachable.isEmpty(), qPrintable(QString::number(unreachable.size())));
    }

private:
    QTemporaryDir m_directory;
    std::unique_ptr<QSettings> m_settings;
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
    AccessibilityAuditTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_AccessibilityAudit.moc"
