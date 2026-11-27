/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QCoreApplication>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <KLocalizedString>

#include "Gui/LinkTarget.h"

class LinkTargetTest : public QObject
{
    Q_OBJECT

private:
    /** The description without its markup and wrapping points, as it reads */
    static QString plain(const QString &html)
    {
        QString text = html;
        text.remove(QChar(0x200B));
        static const QRegularExpression tag(QStringLiteral("<[^>]*>"));
        text.replace(tag, QStringLiteral(" "));
        return text.replace(QLatin1String("&amp;"), QLatin1String("&"));
    }

    static QString describe(const QString &url, const QString &text = QString())
    {
        return Gui::linkTargetDescription(QUrl(url), text);
    }

private slots:
    void showsTheFullTargetWithoutWarnings()
    {
        const QString html = describe(QStringLiteral("https://example.org/news?id=7&lang=en#top"), QStringLiteral("Read"));
        QVERIFY2(plain(html).contains(QLatin1String("Link target")), qPrintable(html));
        QVERIFY2(plain(html).contains(QLatin1String("https://example.org/news?id=7&lang=en#top")), qPrintable(html));
        QVERIFY(html.contains(QLatin1String("&amp;")) && !html.contains(QLatin1String("&lang")));
        QVERIFY(!html.contains(QLatin1String("<ul>")));
        QVERIFY(Gui::linkWarningReasons(QUrl(QStringLiteral("https://example.org/")), QStringLiteral("example.org")).isEmpty());
        // Long addresses can wrap after punctuation
        QVERIFY(html.contains(QStringLiteral("example.") + QChar(0x200B) + QStringLiteral("org/") + QChar(0x200B)));
    }

    void pointsOutPunycodeHosts()
    {
        const QString html = describe(QString::fromUtf8("https://p\xd0\xb0ypal.example/login"), QStringLiteral("Pay now"));
        const QString text = plain(html);
        // The address shows the host as it is contacted, and the warning shows both forms
        QVERIFY2(text.contains(QLatin1String("https://xn--pypal-4ve.example/login")), qPrintable(text));
        QVERIFY2(text.contains(QLatin1String("This link may not go where it appears to")), qPrintable(text));
        QVERIFY2(text.contains(QString::fromUtf8("p\xd0\xb0ypal.example")), qPrintable(text));
        QVERIFY2(text.contains(QLatin1String("written  xn--pypal-4ve.example")), qPrintable(text));
    }

    void pointsOutUserNamesAndPasswords()
    {
        const QString html = describe(QStringLiteral("https://bank.example.com:secret@evil.test/"), QStringLiteral("Sign in"));
        const QString text = plain(html);
        QVERIFY2(text.contains(QLatin1String("https://bank.example.com:secret@evil.test/")), qPrintable(text));
        QVERIFY2(text.contains(QLatin1String("user name  bank.example.com  before the site  evil.test")), qPrintable(text));
        QVERIFY2(text.contains(QLatin1String("also contains a password")), qPrintable(text));
        const QStringList reasons =
            Gui::linkWarningReasons(QUrl(QStringLiteral("https://user@evil.test/")), QStringLiteral("Sign in"));
        QCOMPARE(reasons.size(), 1);
        QVERIFY(!reasons.first().contains(QLatin1String("password")));
    }

    void pointsOutTextShowingAnotherAddress()
    {
        QString text = plain(describe(QStringLiteral("https://evil.test/login"), QStringLiteral("https://bank.example.com/")));
        QVERIFY2(text.contains(QLatin1String("shows  bank.example.com , but the link goes to  evil.test")), qPrintable(text));
        text = plain(describe(QStringLiteral("mailto:collect@evil.test"), QStringLiteral("help@bank.example.com")));
        QVERIFY2(text.contains(QLatin1String("mailto:collect@evil.test")), qPrintable(text));
        QVERIFY2(text.contains(QLatin1String("shows  help@bank.example.com , but the message is written to  collect@evil.test")),
                 qPrintable(text));
        // Flagged, but such links still open; only http and https links ask first
        QVERIFY(plain(describe(QStringLiteral("mailto:help@example.org"), QStringLiteral("help@example.org")))
                    .indexOf(QLatin1String("may not go")) < 0);
    }

    void escapesAndShortensLongTargets()
    {
        const QString tail = QStringLiteral("/end-of-the-address");
        const QString url = QStringLiteral("https://example.org/") + QString(3000, QLatin1Char('a')) + tail;
        const QString text = plain(describe(url));
        QVERIFY(text.contains(QLatin1String("https://example.org/aaa")));
        QVERIFY(text.contains(QChar(0x2026)));
        QVERIFY(text.endsWith(tail + QLatin1Char(' ')));
        QVERIFY(text.size() < 1200);
        const QString html = describe(QStringLiteral("https://example.org/\"><script>x</script>"));
        QVERIFY2(!html.contains(QLatin1String("<script>")), qPrintable(html));
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir home;
    qputenv("HOME", home.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", home.filePath(QStringLiteral("config")).toUtf8());
    qputenv("XDG_DATA_HOME", home.filePath(QStringLiteral("data")).toUtf8());
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("plasma-impresario");
    LinkTargetTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_LinkTarget.moc"
