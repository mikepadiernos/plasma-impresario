/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/MailboxPickerDialog.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/PrettyMailboxModel.h"
#include "Imap/Model/SpecialFolder.h"
#include "UiUtils/IconLoader.h"

#include <KLocalizedString>
#include <QApplication>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using Imap::Mailbox::SpecialUse;

class MailboxNavigationTest : public QObject
{
    Q_OBJECT
private:
    static QStandardItem *mailbox(const QString &name, SpecialUse kind = SpecialUse::None, bool selectable = true)
    {
        const QString leaf = name.section(QLatin1Char('/'), -1);
        auto *item = new QStandardItem(leaf);
        item->setData(name, Imap::Mailbox::RoleMailboxName);
        item->setData(leaf, Imap::Mailbox::RoleShortMailboxName);
        item->setData(QStringLiteral("/"), Imap::Mailbox::RoleMailboxSeparator);
        item->setData(selectable, Imap::Mailbox::RoleMailboxIsSelectable);
        item->setData(true, Imap::Mailbox::RoleMailboxIsSubscribed);
        item->setData(static_cast<int>(kind), Imap::Mailbox::RoleMailboxSpecialUse);
        return item;
    }

    static QStringList names(const QAbstractItemModel &model, const QModelIndex &parent = QModelIndex())
    {
        QStringList res;
        for (int row = 0; row < model.rowCount(parent); ++row)
            res << model.index(row, 0, parent).data(Imap::Mailbox::RoleMailboxName).toString();
        return res;
    }

    static void fillTree(QStandardItemModel &source)
    {
        source.appendRow(mailbox(QStringLiteral("INBOX"), SpecialUse::Inbox));
        auto *projects = mailbox(QStringLiteral("Projects"), SpecialUse::None, false);
        projects->appendRow(mailbox(QStringLiteral("Projects/Alpha")));
        projects->appendRow(mailbox(QStringLiteral("Projects/Beta")));
        source.appendRow(projects);
        source.appendRow(mailbox(QStringLiteral("Trash"), SpecialUse::Trash));
        auto *lazy = mailbox(QStringLiteral("Lazy"));
        lazy->setData(false, Imap::Mailbox::RoleIsFetched);
        lazy->appendRow(mailbox(QStringLiteral("Lazy/Hidden")));
        source.appendRow(lazy);
    }

private slots:
    void prettyModelPutsSpecialFoldersFirst()
    {
        QStandardItemModel source;
        source.appendRow(mailbox(QStringLiteral("INBOX"), SpecialUse::Inbox));
        source.appendRow(mailbox(QStringLiteral("Notes")));
        source.appendRow(mailbox(QStringLiteral("Papierkorb"), SpecialUse::Trash));
        source.appendRow(mailbox(QStringLiteral("Archive"), SpecialUse::Archive));
        source.appendRow(mailbox(QStringLiteral("Bills")));
        source.appendRow(mailbox(QStringLiteral("Sent"), SpecialUse::Sent));
        source.appendRow(mailbox(QStringLiteral("Drafts"), SpecialUse::Drafts));
        auto *junk = mailbox(QStringLiteral("Spam"));
        source.appendRow(junk);
        Imap::Mailbox::PrettyMailboxModel model(nullptr, &source);

        QCOMPARE(names(model), (QStringList{QStringLiteral("INBOX"), QStringLiteral("Drafts"), QStringLiteral("Sent"),
                                            QStringLiteral("Archive"), QStringLiteral("Papierkorb"), QStringLiteral("Notes"),
                                            QStringLiteral("Bills"), QStringLiteral("Spam")}));

        // Classification arriving later (e.g. after the whole tree is listed) re-sorts the siblings
        junk->setData(static_cast<int>(SpecialUse::Junk), Imap::Mailbox::RoleMailboxSpecialUse);
        QCOMPARE(names(model), (QStringList{QStringLiteral("INBOX"), QStringLiteral("Drafts"), QStringLiteral("Sent"),
                                            QStringLiteral("Archive"), QStringLiteral("Spam"), QStringLiteral("Papierkorb"),
                                            QStringLiteral("Notes"), QStringLiteral("Bills")}));

        // The subscription filter still applies on top of the order
        source.item(1)->setData(false, Imap::Mailbox::RoleMailboxIsSubscribed); // Notes
        model.setShowOnlySubscribed(true);
        QCOMPARE(names(model), (QStringList{QStringLiteral("INBOX"), QStringLiteral("Drafts"), QStringLiteral("Sent"),
                                            QStringLiteral("Archive"), QStringLiteral("Spam"), QStringLiteral("Papierkorb"),
                                            QStringLiteral("Bills")}));
        model.setShowOnlySubscribed(false);
        QCOMPARE(model.rowCount(), 8);
    }

    void prettyModelUsesSpecialIcons()
    {
        QStandardItemModel source;
        source.appendRow(mailbox(QStringLiteral("INBOX"), SpecialUse::Inbox));
        source.appendRow(mailbox(QStringLiteral("Sent"), SpecialUse::Sent));
        auto *trash = mailbox(QStringLiteral("Trash"), SpecialUse::Trash);
        source.appendRow(trash);
        source.appendRow(mailbox(QStringLiteral("Notes")));
        Imap::Mailbox::PrettyMailboxModel model(nullptr, &source);
        if (!QIcon::hasThemeIcon(QStringLiteral("mail-folder-inbox")))
            QSKIP("The Breeze icon theme is not installed");

        const auto iconAt = [&model](int row) { return model.index(row, 0).data(Qt::DecorationRole).value<QIcon>(); };
        const auto expected = [](SpecialUse kind) { return Imap::Mailbox::PrettyMailboxModel::specialFolderIcon(kind); };
        QVERIFY(!expected(SpecialUse::Inbox).isNull());
        QVERIFY(!expected(SpecialUse::Sent).isNull());
        QVERIFY(!expected(SpecialUse::Trash).isNull());
        QVERIFY(expected(SpecialUse::None).isNull());
        for (const auto kind : Imap::Mailbox::SpecialFolder::navigableKinds())
            QVERIFY2(!expected(kind).isNull(), qPrintable(QString::number(static_cast<int>(kind))));

        QCOMPARE(iconAt(0).cacheKey(), expected(SpecialUse::Inbox).cacheKey());
        QCOMPARE(iconAt(1).cacheKey(), expected(SpecialUse::Sent).cacheKey());
        QCOMPARE(iconAt(2).cacheKey(), expected(SpecialUse::Trash).cacheKey());
        QCOMPARE(iconAt(3).cacheKey(), UiUtils::loadIcon(QStringLiteral("folder")).cacheKey());

        // A folder that is busy loading keeps showing the loading icon
        trash->setData(true, Imap::Mailbox::RoleMailboxItemsAreLoading);
        QCOMPARE(iconAt(2).cacheKey(), UiUtils::loadIcon(QStringLiteral("folder-grey")).cacheKey());
    }

    void pickerOffersSelectableListedMailboxes()
    {
        QStandardItemModel source;
        fillTree(source);
        Gui::MailboxPickerDialog dialog;
        dialog.setMailboxes(&source);
        // Containers that cannot be selected are skipped, unlisted branches are not walked
        QCOMPARE(dialog.offeredMailboxes(), (QStringList{QStringLiteral("INBOX"), QStringLiteral("Projects/Alpha"),
                                                          QStringLiteral("Projects/Beta"), QStringLiteral("Trash"),
                                                          QStringLiteral("Lazy")}));
        QCOMPARE(dialog.selectedMailbox(), QStringLiteral("INBOX"));

        auto *list = dialog.findChild<QListView *>();
        QVERIFY(list);
        QCOMPARE(list->model()->index(1, 0).data().toString(), QStringLiteral("Projects / Alpha"));

        dialog.setMailboxes(&source, {QStringLiteral("INBOX"), QStringLiteral("Trash")});
        QCOMPARE(dialog.offeredMailboxes(), (QStringList{QStringLiteral("Projects/Alpha"), QStringLiteral("Projects/Beta"),
                                                          QStringLiteral("Lazy")}));
        dialog.setCurrentMailbox(QStringLiteral("Lazy"));
        QCOMPARE(dialog.selectedMailbox(), QStringLiteral("Lazy"));
    }

    void pickerFiltersAndAcceptsFromTheKeyboard()
    {
        QStandardItemModel source;
        fillTree(source);
        Gui::MailboxPickerDialog dialog;
        dialog.setPrompt(QStringLiteral("Pick one:"));
        dialog.setMailboxes(&source);
        QSignalSpy accepted(&dialog, &QDialog::accepted);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto *filter = dialog.findChild<QLineEdit *>();
        auto *ok = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        QVERIFY(filter);
        QVERIFY(ok);
        filter->setFocus();

        QTest::keyClicks(filter, QStringLiteral("PROJ"));
        QCOMPARE(dialog.selectedMailbox(), QStringLiteral("Projects/Alpha"));
        QTest::keyClick(filter, Qt::Key_Down);
        QCOMPARE(dialog.selectedMailbox(), QStringLiteral("Projects/Beta"));
        QVERIFY(filter->hasFocus());

        // No match: nothing to accept
        QTest::keyClicks(filter, QStringLiteral("zzz"));
        QVERIFY(dialog.selectedMailbox().isEmpty());
        QVERIFY(!ok->isEnabled());
        QTest::keyClick(filter, Qt::Key_Return);
        QCOMPARE(accepted.size(), 0);

        filter->clear();
        QTest::keyClicks(filter, QStringLiteral("tras"));
        QCOMPARE(dialog.selectedMailbox(), QStringLiteral("Trash"));
        QVERIFY(ok->isEnabled());
        QTest::keyClick(filter, Qt::Key_Return);
        QCOMPARE(accepted.size(), 1);
        QCOMPARE(dialog.selectedMailbox(), QStringLiteral("Trash"));
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
    // The offscreen platform has no desktop theme, and test mode hides the system data directories;
    // point Qt at the installed Plasma theme so the icon chains resolve as they do in a session
    QStringList iconPaths = QIcon::themeSearchPaths();
    QString dataDirs = qEnvironmentVariable("XDG_DATA_DIRS");
    if (dataDirs.isEmpty())
        dataDirs = QStringLiteral("/usr/local/share:/usr/share");
    for (const QString &dir : dataDirs.split(QLatin1Char(':'), Qt::SkipEmptyParts))
        iconPaths << dir + QStringLiteral("/icons");
    QIcon::setThemeSearchPaths(iconPaths);
    QIcon::setThemeName(QStringLiteral("breeze"));
    KLocalizedString::setApplicationDomain("plasma-impresario");
    MailboxNavigationTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_MailboxNavigation.moc"
