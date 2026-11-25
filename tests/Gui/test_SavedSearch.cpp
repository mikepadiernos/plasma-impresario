/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/MessageListWidget.h"
#include "Gui/MsgListView.h"

#include <KSharedConfig>
#include <KLocalizedString>
#include <QAbstractButton>
#include <QApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

class SavedSearchTest : public QObject
{
    Q_OBJECT
private:
    static void enableSearch(Gui::MessageListWidget &widget, QStandardItemModel &model)
    {
        model.setRowCount(1);
        widget.tree->setModel(&model);
        QMetaObject::invokeMethod(&widget, "slotAutoEnableDisableSearch");
    }

    static void saveSearch(Gui::MessageListWidget &widget, const QString &name)
    {
        QTimer::singleShot(0, [name]() {
            auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            if (dialog) {
                dialog->setTextValue(name);
                dialog->accept();
            }
        });
        widget.findChild<QAction *>(QStringLiteral("saveSearch"))->trigger();
    }

    static QMenu *savedSearches(Gui::MessageListWidget &widget)
    {
        auto *menu = widget.findChild<QMenu *>(QStringLiteral("savedSearches"));
        QMetaObject::invokeMethod(menu, "aboutToShow");
        return menu;
    }

private slots:
    void persistsRunsAndDeletesPerAccount()
    {
        Imap::Mailbox::FavoriteTagsModel tags;
        {
            Gui::MessageListWidget widget(nullptr, &tags, QStringLiteral("account-a"));
            QStandardItemModel model;
            enableSearch(widget, model);
            widget.findChild<QLineEdit *>(QStringLiteral("quickSearch"))->setText(QStringLiteral("quarterly"));
            widget.findChild<QAction *>(QStringLiteral("searchInSubject"))->setChecked(false);
            widget.findChild<QAction *>(QStringLiteral("searchInSenders"))->setChecked(false);
            widget.findChild<QAction *>(QStringLiteral("searchInBody"))->setChecked(true);
            saveSearch(widget, QStringLiteral("Reports"));
        }
        KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"))->reparseConfiguration();
        {
            Gui::MessageListWidget other(nullptr, &tags, QStringLiteral("account-b"));
            QCOMPARE(savedSearches(other)->actions().size(), 0);
            QStandardItemModel model;
            enableSearch(other, model);
            other.findChild<QLineEdit *>(QStringLiteral("quickSearch"))->setText(QStringLiteral("private"));
            saveSearch(other, QStringLiteral("Other"));
        }
        {
            Gui::MessageListWidget widget(nullptr, &tags, QStringLiteral("account-a"));
            QStandardItemModel model;
            enableSearch(widget, model);
            QMenu *menu = savedSearches(widget);
            QCOMPARE(menu->actions().size(), 1);
            QCOMPARE(menu->actions().first()->text(), QStringLiteral("Reports"));
            auto *entry = menu->actions().first()->menu();
            QVERIFY(entry);
            QSignalSpy search(&widget, &Gui::MessageListWidget::requestingSearch);
            entry->findChild<QAction *>(QStringLiteral("runSavedSearch"))->trigger();
            QCOMPARE(search.size(), 1);
            QCOMPARE(search.takeFirst().at(0).toStringList(), QStringList({QStringLiteral("BODY"), QStringLiteral("quarterly")}));
            QCOMPARE(widget.findChild<QLineEdit *>(QStringLiteral("quickSearch"))->text(), QStringLiteral("quarterly"));
            QTimer::singleShot(0, []() {
                auto *dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                if (dialog)
                    dialog->button(QMessageBox::Yes)->click();
            });
            entry->findChild<QAction *>(QStringLiteral("deleteSavedSearch"))->trigger();
        }
        KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"))->reparseConfiguration();
        Gui::MessageListWidget reopened(nullptr, &tags, QStringLiteral("account-a"));
        QCOMPARE(savedSearches(reopened)->actions().size(), 0);
        Gui::MessageListWidget other(nullptr, &tags, QStringLiteral("account-b"));
        QCOMPARE(savedSearches(other)->actions().size(), 1);
        QCOMPARE(savedSearches(other)->actions().first()->text(), QStringLiteral("Other"));
    }

    void preservesRawAndFuzzySearches()
    {
        Imap::Mailbox::FavoriteTagsModel tags;
        Gui::MessageListWidget widget(nullptr, &tags, QStringLiteral("account-c"));
        QStandardItemModel model;
        enableSearch(widget, model);
        auto *query = widget.findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        auto *raw = widget.findChild<QAction *>(QStringLiteral("searchRaw"));
        widget.setFuzzySearchSupported(true);
        raw->setChecked(true);
        query->setText(QStringLiteral(":=UNSEEN"));
        saveSearch(widget, QStringLiteral("Raw"));
        raw->setChecked(false);
        widget.findChild<QAction *>(QStringLiteral("searchInSenders"))->setChecked(false);
        query->setText(QStringLiteral("quarterly"));
        saveSearch(widget, QStringLiteral("Fuzzy"));

        auto *menu = savedSearches(widget);
        QMenu *rawEntry = nullptr;
        QMenu *fuzzyEntry = nullptr;
        for (auto *action : menu->actions()) {
            if (action->text() == QStringLiteral("Raw"))
                rawEntry = action->menu();
            if (action->text() == QStringLiteral("Fuzzy"))
                fuzzyEntry = action->menu();
        }
        QVERIFY(rawEntry);
        QVERIFY(fuzzyEntry);
        QSignalSpy search(&widget, &Gui::MessageListWidget::requestingSearch);
        widget.setFuzzySearchSupported(false);
        rawEntry->findChild<QAction *>(QStringLiteral("runSavedSearch"))->trigger();
        QCOMPARE(search.takeFirst().at(0).toStringList(), QStringList({QStringLiteral("UNSEEN")}));
        fuzzyEntry->findChild<QAction *>(QStringLiteral("runSavedSearch"))->trigger();
        QCOMPARE(search.size(), 0);
        for (QWidget *window : QApplication::topLevelWidgets()) {
            if (auto *message = qobject_cast<QMessageBox *>(window))
                message->close();
        }
        widget.setFuzzySearchSupported(true);
        fuzzyEntry->findChild<QAction *>(QStringLiteral("runSavedSearch"))->trigger();
        QCOMPARE(search.takeFirst().at(0).toStringList(),
                 QStringList({QStringLiteral("FUZZY"), QStringLiteral("SUBJECT"), QStringLiteral("quarterly")}));
    }

    void rejectsDuplicateNames()
    {
        Imap::Mailbox::FavoriteTagsModel tags;
        Gui::MessageListWidget widget(nullptr, &tags, QStringLiteral("account-d"));
        QStandardItemModel model;
        enableSearch(widget, model);
        auto *query = widget.findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        query->setText(QStringLiteral("first"));
        saveSearch(widget, QStringLiteral("Reports"));
        query->setText(QStringLiteral("second"));
        saveSearch(widget, QStringLiteral("Reports"));
        QCOMPARE(savedSearches(widget)->actions().size(), 1);
        QSignalSpy search(&widget, &Gui::MessageListWidget::requestingSearch);
        savedSearches(widget)->actions().first()->menu()->findChild<QAction *>(QStringLiteral("runSavedSearch"))->trigger();
        QVERIFY(search.takeFirst().at(0).toStringList().contains(QStringLiteral("first")));
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
    KLocalizedString::setApplicationDomain("plasma-impresario");
    SavedSearchTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_SavedSearch.moc"
