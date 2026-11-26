/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/MessageListWidget.h"
#include "Gui/MsgListView.h"
#include "UiUtils/SearchScope.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <KLocalizedString>
#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QDateEdit>
#include <QDialog>
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

    static void chooseDates(Gui::MessageListWidget &widget, UiUtils::SearchScope::Dates dates)
    {
        widget.findChild<QAction *>(QLatin1String("searchDates-") + UiUtils::SearchScope::configName(dates))->trigger();
    }

    static UiUtils::SearchScope::DateRange preset(UiUtils::SearchScope::Dates dates)
    {
        return {dates, {}, {}};
    }

    static QStringList dateKeys(const UiUtils::SearchScope::DateRange &range)
    {
        return UiUtils::SearchScope::dateConditions(range, QDate::currentDate());
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

    void datesLimitEveryKindOfSearch()
    {
        using UiUtils::SearchScope::Dates;
        Imap::Mailbox::FavoriteTagsModel tags;
        Gui::MessageListWidget widget(nullptr, &tags, QStringLiteral("account-e"));
        QStandardItemModel model;
        enableSearch(widget, model);
        auto *query = widget.findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        query->setText(QStringLiteral("plan"));
        QSignalSpy search(&widget, &Gui::MessageListWidget::requestingSearch);

        // Choosing a range applies the search at once
        chooseDates(widget, Dates::PastWeek);
        QCOMPARE(search.size(), 1);
        const QStringList week = dateKeys(preset(Dates::PastWeek));
        QCOMPARE(search.takeFirst().at(0).toStringList(),
                 QStringList({QStringLiteral("OR"), QStringLiteral("SUBJECT"), QStringLiteral("plan"), QStringLiteral("FROM"),
                              QStringLiteral("plan")}) + week);
        QVERIFY(query->placeholderText().contains(QStringLiteral("Past Week")));
        QVERIFY(query->toolTip().contains(QStringLiteral("Past Week")));
        QCOMPARE(widget.dateRange(), preset(Dates::PastWeek));

        // The range alone is a search, and keeps the field enabled when nothing matches
        query->clear();
        QCOMPARE(widget.searchConditions(), week);
        model.setRowCount(0);
        QMetaObject::invokeMethod(&widget, "slotAutoEnableDisableSearch");
        QVERIFY(query->isEnabled());
        model.setRowCount(1);

        // Field filters and raw queries are narrowed too
        query->setText(QStringLiteral("from:ann@example.org"));
        QCOMPARE(widget.searchConditions(), QStringList({QStringLiteral("FROM"), QStringLiteral("ann@example.org")}) + week);
        widget.findChild<QAction *>(QStringLiteral("searchRaw"))->setChecked(true);
        query->setText(QStringLiteral(":=UNSEEN"));
        QCOMPARE(widget.searchConditions(), QStringList({QStringLiteral("UNSEEN ") + week.join(QLatin1Char(' '))}));
        query->setText(QStringLiteral(":="));
        QCOMPARE(widget.searchConditions(), week);

        chooseDates(widget, Dates::AnyTime);
        QCOMPARE(widget.searchConditions(), QStringList({QString()}));
        QCOMPARE(query->placeholderText(), QStringLiteral("Quick Search"));

        // Leaving for a list which cannot be searched drops the range with the text
        chooseDates(widget, Dates::Today);
        widget.setSearchAvailable(false);
        QCOMPARE(widget.dateRange(), preset(Dates::AnyTime));
        widget.setSearchAvailable(true);
        QVERIFY(widget.searchConditions().isEmpty());
    }

    void choosesCustomRanges()
    {
        using UiUtils::SearchScope::Dates;
        Imap::Mailbox::FavoriteTagsModel tags;
        Gui::MessageListWidget widget(nullptr, &tags, QStringLiteral("account-f"));
        QStandardItemModel model;
        enableSearch(widget, model);
        chooseDates(widget, Dates::PastMonth);

        // Only a start: everything since that day
        QTimer::singleShot(0, []() {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->findChild<QDateEdit *>(QStringLiteral("dateFrom"))->setDate(QDate(2026, 1, 10));
            dialog->findChild<QCheckBox *>(QStringLiteral("dateToEnabled"))->setChecked(false);
            dialog->accept();
        });
        chooseDates(widget, Dates::Custom);
        QCOMPARE(widget.dateRange(), (UiUtils::SearchScope::DateRange{Dates::Custom, QDate(2026, 1, 10), QDate()}));
        QCOMPARE(widget.searchConditions(), QStringList({QStringLiteral("SENTSINCE"), QStringLiteral("10-Jan-2026")}));

        // Cancelling keeps the range in effect
        QTimer::singleShot(0, []() {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget()))
                dialog->reject();
        });
        chooseDates(widget, Dates::Custom);
        QCOMPARE(widget.dateRange().from, QDate(2026, 1, 10));
        QVERIFY(widget.findChild<QAction *>(QStringLiteral("searchDates-custom"))->isChecked());

        // Neither end means any time
        QTimer::singleShot(0, []() {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->findChild<QCheckBox *>(QStringLiteral("dateFromEnabled"))->setChecked(false);
            dialog->findChild<QCheckBox *>(QStringLiteral("dateToEnabled"))->setChecked(false);
            dialog->accept();
        });
        chooseDates(widget, Dates::Custom);
        QCOMPARE(widget.dateRange(), preset(Dates::AnyTime));
        QVERIFY(widget.findChild<QAction *>(QStringLiteral("searchDates-any"))->isChecked());
    }

    void savesDateRanges()
    {
        using UiUtils::SearchScope::Dates;
        Imap::Mailbox::FavoriteTagsModel tags;
        Gui::MessageListWidget widget(nullptr, &tags, QStringLiteral("account-g"));
        QStandardItemModel model;
        enableSearch(widget, model);
        auto *query = widget.findChild<QLineEdit *>(QStringLiteral("quickSearch"));

        // A range without text can be saved, and presets stay relative
        chooseDates(widget, Dates::PastMonth);
        saveSearch(widget, QStringLiteral("Recent"));
        query->setText(QStringLiteral("plan"));
        widget.setDateRange({Dates::Custom, QDate(2026, 1, 10), QDate(2026, 1, 20)});
        saveSearch(widget, QStringLiteral("January"));
        widget.setDateRange({});
        query->clear();

        KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"))->reparseConfiguration();
        QMenu *recent = nullptr;
        QMenu *january = nullptr;
        for (auto *action : savedSearches(widget)->actions()) {
            if (action->text() == QStringLiteral("Recent"))
                recent = action->menu();
            if (action->text() == QStringLiteral("January"))
                january = action->menu();
        }
        QVERIFY(recent);
        QVERIFY(january);
        QSignalSpy search(&widget, &Gui::MessageListWidget::requestingSearch);
        recent->findChild<QAction *>(QStringLiteral("runSavedSearch"))->trigger();
        QCOMPARE(search.takeLast().at(0).toStringList(), dateKeys(preset(Dates::PastMonth)));
        QCOMPARE(widget.dateRange(), preset(Dates::PastMonth));
        january->findChild<QAction *>(QStringLiteral("runSavedSearch"))->trigger();
        QCOMPARE(search.takeLast().at(0).toStringList(),
                 QStringList({QStringLiteral("OR"), QStringLiteral("SUBJECT"), QStringLiteral("plan"), QStringLiteral("FROM"),
                              QStringLiteral("plan"), QStringLiteral("SENTSINCE"), QStringLiteral("10-Jan-2026"),
                              QStringLiteral("SENTBEFORE"), QStringLiteral("21-Jan-2026")}));
        QCOMPARE(query->text(), QStringLiteral("plan"));

        // Searches saved before dates existed run over any time
        Gui::MessageListWidget older(nullptr, &tags, QStringLiteral("account-h"));
        QStandardItemModel olderModel;
        enableSearch(older, olderModel);
        older.findChild<QLineEdit *>(QStringLiteral("quickSearch"))->setText(QStringLiteral("old"));
        saveSearch(older, QStringLiteral("Old"));
        {
            KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")), QStringLiteral("SavedSearches"));
            group = group.group(QStringLiteral("account-h"));
            for (const QString &id : group.groupList()) {
                KConfigGroup entry = group.group(id);
                entry.deleteEntry("Dates");
                entry.deleteEntry("DateFrom");
                entry.deleteEntry("DateTo");
            }
            group.sync();
        }
        older.setDateRange(preset(Dates::Today));
        QSignalSpy olderSearch(&older, &Gui::MessageListWidget::requestingSearch);
        savedSearches(older)->actions().first()->menu()->findChild<QAction *>(QStringLiteral("runSavedSearch"))->trigger();
        QCOMPARE(older.dateRange(), preset(Dates::AnyTime));
        QCOMPARE(olderSearch.takeLast().at(0).toStringList(),
                 QStringList({QStringLiteral("OR"), QStringLiteral("SUBJECT"), QStringLiteral("old"), QStringLiteral("FROM"),
                              QStringLiteral("old")}));
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
