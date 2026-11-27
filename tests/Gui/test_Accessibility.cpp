/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Common/SettingsNames.h"
#include "Gui/AddressRowWidget.h"
#include "Gui/ColoredItemDelegate.h"
#include "Gui/OneEnvelopeAddress.h"
#include "Gui/StateColors.h"
#include "Gui/Util.h"
#include "Imap/Parser/MailAddress.h"
#include "Plugins/PluginManager.h"
#include "UiUtils/Color.h"
#include "UiUtils/IconLoader.h"

#include <QAccessible>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QLineEdit>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QStyleOptionViewItem>
#include <QTemporaryDir>
#include <QTest>

#include <KColorScheme>
#include <KLocalizedString>

/** @short Theme icons, color scheme states, contrast and keyboard access (#103) */
namespace {

class SelectionProbe : public Gui::ColoredItemDelegate
{
public:
    using Gui::ColoredItemDelegate::applyForegroundToSelection;
};

}

class AccessibilityTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_icons;

    void addIcon(const QString &name)
    {
        QImage image(16, 16, QImage::Format_ARGB32);
        image.fill(Qt::darkGray);
        QVERIFY(image.save(m_icons.filePath(QStringLiteral("breeze/actions/16/%1.png").arg(name))));
    }

private slots:
    void initTestCase()
    {
        // A theme named like Plasma's, holding only what each test needs
        QVERIFY(m_icons.isValid());
        QVERIFY(QDir(m_icons.path()).mkpath(QStringLiteral("breeze/actions/16")));
        QFile index(m_icons.filePath(QStringLiteral("breeze/index.theme")));
        QVERIFY(index.open(QIODevice::WriteOnly));
        index.write("[Icon Theme]\nName=Breeze\nDirectories=actions/16\n\n[actions/16]\nSize=16\nType=Fixed\nContext=Actions\n");
        index.close();
        addIcon(QStringLiteral("acc-both"));
        addIcon(QStringLiteral("acc-both-symbolic"));
        addIcon(QStringLiteral("acc-full"));
        addIcon(QStringLiteral("mail-reply-sender"));
        addIcon(QStringLiteral("mail-flagged"));
        QIcon::setThemeSearchPaths({m_icons.path()});
        QIcon::setFallbackSearchPaths({});
        QIcon::setThemeName(QStringLiteral("breeze"));
        QIcon::setFallbackThemeName(QStringLiteral("breeze"));
    }

    void symbolicVariantWinsAtSmallSizes()
    {
        QCOMPARE(UiUtils::SymbolicIcon::fromTheme(QStringLiteral("acc-both")).name(), QStringLiteral("acc-both-symbolic"));
        QCOMPARE(UiUtils::SymbolicIcon::fromTheme(QStringLiteral("acc-full")).name(), QStringLiteral("acc-full"));
    }

    void activeThemeWinsOverBundledCopies()
    {
        // The bundled Breeze copies used to replace these in the Breeze theme
        QVERIFY(!QFile::exists(QStringLiteral(":/icons/breeze/mail-reply-sender.svg")));
        QCOMPARE(UiUtils::loadIcon(QStringLiteral("mail-reply-sender")).name(), QStringLiteral("mail-reply-sender"));
        QVERIFY(QFile::exists(QStringLiteral(":/icons/mail-flagged.svg")));
        QCOMPARE(UiUtils::loadIcon(QStringLiteral("mail-flagged")).name(), QStringLiteral("mail-flagged"));
    }

    void bundledIconFillsGapsInTheTheme()
    {
        QVERIFY(!QIcon::hasThemeIcon(QStringLiteral("mail-attachment")));
        const QIcon bundled = UiUtils::loadIcon(QStringLiteral("mail-attachment"));
        QVERIFY(!bundled.isNull());
        QVERIFY(!bundled.pixmap(16, 16).isNull());
        QVERIFY(!UiUtils::SymbolicIcon::fromTheme(QStringLiteral("mail-attachment")).pixmap(16, 16).isNull());
    }

    void fallbackWhenNothingMatches()
    {
        QIcon fallback(m_icons.filePath(QStringLiteral("breeze/actions/16/acc-full.png")));
        QVERIFY(UiUtils::loadIcon(QStringLiteral("acc-missing")).isNull());
        QCOMPARE(UiUtils::SymbolicIcon::fromTheme(QStringLiteral("acc-missing"), fallback).cacheKey(), fallback.cacheKey());
    }

    void fieldStatesFollowTheColorScheme()
    {
        using Gui::StateColors::State;
        QLineEdit field;
        const QColor normal = field.palette().color(QPalette::Base);
        Gui::StateColors::applyToField(&field, State::Negative);
        QCOMPARE(field.palette().color(QPalette::Active, QPalette::Base), Gui::StateColors::fieldBackground(State::Negative));
        QCOMPARE(Gui::StateColors::fieldBackground(State::Negative),
                 KColorScheme(QPalette::Active, KColorScheme::View).background(KColorScheme::NegativeBackground).color());
        Gui::StateColors::applyToField(&field, State::Positive);
        QCOMPARE(field.palette().color(QPalette::Active, QPalette::Base), Gui::StateColors::fieldBackground(State::Positive));
        QVERIFY(Gui::StateColors::fieldBackground(State::Positive) != Gui::StateColors::fieldBackground(State::Negative));
        Gui::StateColors::applyToField(&field, State::Normal);
        QVERIFY(!field.testAttribute(Qt::WA_SetPalette));
        QCOMPARE(field.palette().color(QPalette::Base), normal);
    }

    void warningLabelsFollowTheColorScheme()
    {
        const KColorScheme scheme(QPalette::Active, KColorScheme::Window);
        const QString sheet = Gui::Util::cssWarningBorder();
        QVERIFY(sheet.contains(scheme.foreground(KColorScheme::NegativeText).color().name()));
        QVERIFY(sheet.contains(scheme.background(KColorScheme::NeutralBackground).color().name()));
        QVERIFY(!sheet.contains(QLatin1String("red")));
        QCOMPARE(Gui::StateColors::text(Gui::StateColors::State::Negative), scheme.foreground(KColorScheme::NegativeText).color());
    }

    void selectedTagTextStaysReadable_data()
    {
        QTest::addColumn<QColor>("highlightedText");
        QTest::addColumn<QColor>("tag");
        QTest::newRow("white on yellow") << QColor(Qt::white) << QColor(0xf0, 0xe0, 0x00);
        QTest::newRow("white on light blue") << QColor(Qt::white) << QColor(0x3d, 0xae, 0xe9);
        QTest::newRow("black on navy") << QColor(Qt::black) << QColor(0x00, 0x00, 0x80);
        QTest::newRow("white on navy") << QColor(Qt::white) << QColor(0x00, 0x00, 0x80);
    }

    void selectedTagTextStaysReadable()
    {
        QFETCH(QColor, highlightedText);
        QFETCH(QColor, tag);
        QStyleOptionViewItem option;
        option.palette.setColor(QPalette::Active, QPalette::HighlightedText, highlightedText);
        SelectionProbe::applyForegroundToSelection(option, tag);
        QCOMPARE(option.palette.color(QPalette::Active, QPalette::Highlight), tag);
        const QColor text = option.palette.color(QPalette::Active, QPalette::HighlightedText);
        QVERIFY2(UiUtils::contrastRatio(text, tag) >= 4.5, qPrintable(text.name()));
        if (UiUtils::contrastRatio(highlightedText, tag) >= 4.5)
            QCOMPARE(text, highlightedText);
    }

    void expanderIsAKeyboardButton()
    {
        Gui::Expander expander(nullptr, 3);
        QCOMPARE(expander.focusPolicy(), Qt::StrongFocus);
        QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(&expander);
        QVERIFY(accessible);
        QCOMPARE(accessible->role(), QAccessible::Button);
        QCOMPARE(accessible->text(QAccessible::Name),
                 i18ncp("@action:button", "Show %1 more address", "Show %1 more addresses", 3));
        QCOMPARE(expander.toolTip(), accessible->text(QAccessible::Name));

        expander.show();
        QVERIFY(QTest::qWaitForWindowExposed(&expander));
        QSignalSpy clicked(&expander, &QAbstractButton::clicked);
        QTest::keyClick(&expander, Qt::Key_Space);
        QTest::keyClick(&expander, Qt::Key_Return);
        QTest::keyClick(&expander, Qt::Key_Enter);
        QCOMPARE(clicked.count(), 3);

        expander.setExpanding(0);
        QCOMPARE(accessible->text(QAccessible::Name), i18nc("@action:button", "Show fewer addresses"));
    }

    void expanderShowsItsFocus()
    {
        QWidget window;
        auto *field = new QLineEdit(&window);
        auto *expander = new Gui::Expander(&window, 2);
        expander->setGeometry(0, field->sizeHint().height() + 4, expander->sizeHint().width() + 8, expander->sizeHint().height() + 8);
        window.show();
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        field->setFocus();
        QTRY_VERIFY(field->hasFocus());
        const QImage unfocused = expander->grab().toImage();
        QTest::keyClick(field, Qt::Key_Tab);
        QTRY_VERIFY(expander->hasFocus());
        QVERIFY(expander->grab().toImage() != unfocused);
    }

    void addressLinksTakeTheKeyboard()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        settings.setValue(Common::SettingsNames::addressbookPlugin, QStringLiteral("none"));
        Plugins::PluginManager plugins(nullptr, &settings, Common::SettingsNames::addressbookPlugin,
                                       Common::SettingsNames::passwordPlugin, Common::SettingsNames::spellcheckerPlugin);
        const Imap::Message::MailAddress address(QStringLiteral("Jane Doe"), QString(), QStringLiteral("jane"),
                                                 QStringLiteral("example.org"));
        Gui::OneEnvelopeAddress label(nullptr, address, &plugins, Gui::OneEnvelopeAddress::Position::Last);
        QVERIFY(label.textInteractionFlags() & Qt::LinksAccessibleByKeyboard);
        QVERIFY(label.focusPolicy() & Qt::TabFocus);
        QCOMPARE(label.accessibleName(), address.prettyName(Imap::Message::MailAddress::FORMAT_READABLE));
        QVERIFY(label.accessibleName().contains(QLatin1String("jane@example.org")));
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
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(icons);
    KLocalizedString::setApplicationDomain("plasma-impresario");
    AccessibilityTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_Accessibility.moc"
