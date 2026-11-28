/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <KLocalizedString>

#include "Composer/SenderIdentitiesModel.h"
#include "Gui/HeaderInputGuard.h"
#include "Gui/SettingsDialog.h"

using Gui::HeaderInputGuard;

class HeaderInputGuardTest : public QObject
{
    Q_OBJECT

private slots:
    void pastedLineBreaksBecomeSpaces();
    void cursorStaysAfterPastedText();
    void plainTextIsLeftAlone();
    void programTextIsLeftAlone();
    void accessibleNameNamesTheField();
    void identityEditorPointsOutCleanedText();

private:
    static void paste(QLineEdit *edit, const QString &text);
};

void HeaderInputGuardTest::paste(QLineEdit *edit, const QString &text)
{
    QApplication::clipboard()->setText(text);
    edit->paste();
}

void HeaderInputGuardTest::pastedLineBreaksBecomeSpaces()
{
    QLineEdit edit;
    HeaderInputGuard guard;
    guard.watch(&edit, QStringLiteral("Subject"));
    QSignalSpy cleaned(&guard, &HeaderInputGuard::cleaned);

    paste(&edit, QStringLiteral("Quarterly\r\nBcc: someone@example.org\u2028report\tfinal"));

    QCOMPARE(edit.text(), QStringLiteral("Quarterly Bcc: someone@example.org report final"));
    QCOMPARE(cleaned.size(), 1);
    QCOMPARE(cleaned.at(0).at(0).value<QLineEdit *>(), &edit);
    QCOMPARE(cleaned.at(0).at(1).toString(), HeaderInputGuard::noticeText(QStringLiteral("Subject")));
    QVERIFY(cleaned.at(0).at(1).toString().contains(QLatin1String("Subject")));
}

void HeaderInputGuardTest::cursorStaysAfterPastedText()
{
    QLineEdit edit;
    edit.setText(QStringLiteral("before after"));
    HeaderInputGuard guard;
    guard.watch(&edit, QStringLiteral("Subject"));
    edit.setCursorPosition(7);

    paste(&edit, QStringLiteral("one\n\ntwo "));

    QCOMPARE(edit.text(), QStringLiteral("before one two after"));
    QCOMPARE(edit.cursorPosition(), 15);
}

void HeaderInputGuardTest::plainTextIsLeftAlone()
{
    QLineEdit edit;
    HeaderInputGuard guard;
    guard.watch(&edit, QStringLiteral("Subject"));
    QSignalSpy cleaned(&guard, &HeaderInputGuard::cleaned);

    paste(&edit, QStringLiteral("Lunch on Friday?"));
    QTest::keyClicks(&edit, QStringLiteral(" Yes"));

    QCOMPARE(edit.text(), QStringLiteral("Lunch on Friday? Yes"));
    QCOMPARE(cleaned.size(), 0);
}

void HeaderInputGuardTest::programTextIsLeftAlone()
{
    QLineEdit edit;
    HeaderInputGuard guard;
    guard.watch(&edit, QStringLiteral("Subject"));
    QSignalSpy cleaned(&guard, &HeaderInputGuard::cleaned);

    edit.setText(QStringLiteral("restored\ndraft"));

    QCOMPARE(edit.text(), QStringLiteral("restored\ndraft"));
    QCOMPARE(cleaned.size(), 0);
}

void HeaderInputGuardTest::accessibleNameNamesTheField()
{
    QLineEdit edit;
    edit.setAccessibleName(QStringLiteral("Bcc address"));
    HeaderInputGuard guard;
    guard.watch(&edit);
    QSignalSpy cleaned(&guard, &HeaderInputGuard::cleaned);

    paste(&edit, QStringLiteral("a@example.org\nb@example.org"));

    QCOMPARE(edit.text(), QStringLiteral("a@example.org b@example.org"));
    QCOMPARE(cleaned.size(), 1);
    QCOMPARE(cleaned.at(0).at(1).toString(), HeaderInputGuard::noticeText(QStringLiteral("Bcc address")));
}

void HeaderInputGuardTest::identityEditorPointsOutCleanedText()
{
    Composer::SenderIdentitiesModel identities;
    identities.appendIdentity(
        Composer::ItemSenderIdentity(QStringLiteral("Ann"), QStringLiteral("ann@example.org"), QString(), QString()));
    Gui::EditIdentity dialog(nullptr, &identities, identities.index(0, 0));
    dialog.show();
    auto *note = dialog.findChild<QLabel *>(QStringLiteral("identityCleanedNote"));
    QVERIFY(note);
    QVERIFY(!note->isVisible());
    auto *organization = dialog.findChild<QLineEdit *>(QStringLiteral("organisationLineEdit"));
    QVERIFY(organization);

    organization->selectAll();
    paste(organization, QStringLiteral("Example\nOrganization: Other"));

    QCOMPARE(organization->text(), QStringLiteral("Example Organization: Other"));
    QVERIFY(note->isVisible());
    QVERIFY(note->text().contains(QLatin1String("Organization")));
}

int main(int argc, char *argv[])
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
    qRegisterMetaType<QLineEdit *>();
    HeaderInputGuardTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_HeaderInputGuard.moc"
