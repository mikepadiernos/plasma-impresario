/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/InlineNotificationBar.h"
#include "Gui/MessageListWidget.h"
#include "Gui/MsgListView.h"
#include "Gui/SearchBuilderDialog.h"
#include "Gui/SearchFoldersDialog.h"
#include "Imap/Model/ItemRoles.h"
#include "UiUtils/SearchScope.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <KLocalizedString>
#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>

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
        // A preset follows the calendar, so the search is checked again for a new day
        auto *dayChange = widget.findChild<QTimer *>(QStringLiteral("searchDayChange"));
        QVERIFY(dayChange);
        QVERIFY(dayChange->isActive());
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
        QVERIFY(!dayChange->isActive());
        QCOMPARE(widget.searchConditions(), QStringList({QString()}));
        QCOMPARE(query->placeholderText(), QStringLiteral("Quick Search"));

        // Leaving for a list which cannot be searched drops the range with the text
        chooseDates(widget, Dates::Today);
        widget.setSearchAvailable(false);
        QCOMPARE(widget.dateRange(), preset(Dates::AnyTime));
        widget.setSearchAvailable(true);
        QVERIFY(widget.searchConditions().isEmpty());
    }

    void explainsInvalidQueries()
    {
        Imap::Mailbox::FavoriteTagsModel tags;
        Gui::MessageListWidget widget(nullptr, &tags, QStringLiteral("account-q"));
        QStandardItemModel model;
        enableSearch(widget, model);
        auto *query = widget.findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        auto *problem = widget.findChild<Gui::InlineNotificationBar *>(QStringLiteral("searchQueryError"));
        auto *dayChange = widget.findChild<QTimer *>(QStringLiteral("searchDayChange"));
        QVERIFY(query);
        QVERIFY(problem);
        QVERIFY(problem->isHidden());
        QSignalSpy search(&widget, &Gui::MessageListWidget::requestingSearch);
        const auto apply = [&widget]() { QMetaObject::invokeMethod(&widget, "slotApplySearch"); };

        // Fields, operators and flags
        query->setText(QStringLiteral("(from:ann OR from:bob) -subject:report is:unread"));
        apply();
        QCOMPARE(search.size(), 1);
        QCOMPARE(search.takeFirst().at(0).toStringList(),
                 QStringList({QStringLiteral("OR"), QStringLiteral("FROM"), QStringLiteral("ann"), QStringLiteral("FROM"),
                              QStringLiteral("bob"), QStringLiteral("NOT"), QStringLiteral("SUBJECT"),
                              QStringLiteral("report"), QStringLiteral("UNSEEN")}));
        QVERIFY(problem->isHidden());

        // An invalid query is explained and never reaches the server, so the list keeps the previous search
        query->setText(QStringLiteral("from:ann foo:bar"));
        apply();
        QVERIFY(search.isEmpty());
        QVERIFY(!problem->isHidden());
        QCOMPARE(problem->kind(), Gui::InlineNotificationBar::Kind::Error);
        QVERIFY2(problem->text().contains(QStringLiteral("\u201Cfoo\u201D")), qPrintable(problem->text()));
        QCOMPARE(query->selectedText(), QStringLiteral("foo:bar"));
        QVERIFY(widget.searchConditions().isEmpty());

        // Editing the text removes the explanation
        query->setText(QStringLiteral("from:ann"));
        QVERIFY(problem->isHidden());

        // Plain words need a field to search in
        widget.findChild<QAction *>(QStringLiteral("searchInSubject"))->setChecked(false);
        widget.findChild<QAction *>(QStringLiteral("searchInSenders"))->setChecked(false);
        query->setText(QStringLiteral("from:ann lunch"));
        apply();
        QVERIFY(search.isEmpty());
        QVERIFY(problem->text().contains(QStringLiteral("subject:lunch")));
        QCOMPARE(query->selectedText(), QStringLiteral("lunch"));
        widget.findChild<QAction *>(QStringLiteral("searchInSubject"))->setChecked(true);

        // Saving explains the problem rather than asking for a name
        bool asked = false;
        QTimer::singleShot(0, [&asked]() {
            if (auto *dialog = QApplication::activeModalWidget()) {
                asked = true;
                dialog->close();
            }
        });
        query->setText(QStringLiteral("(lunch"));
        widget.findChild<QAction *>(QStringLiteral("saveSearch"))->trigger();
        QCoreApplication::processEvents();
        QVERIFY(!asked);
        QVERIFY(!problem->isHidden());
        QVERIFY(problem->text().contains(QStringLiteral("not closed")));

        // A query for today follows the calendar like the preset ranges
        query->setText(QStringLiteral("date:today"));
        apply();
        QCOMPARE(search.size(), 1);
        QCOMPARE(search.takeFirst().at(0).toStringList(),
                 UiUtils::SearchScope::dateConditions(preset(UiUtils::SearchScope::Dates::Today), QDate::currentDate()));
        QVERIFY(dayChange->isActive());
        query->setText(QStringLiteral("date:2025"));
        apply();
        QCOMPARE(search.size(), 1);
        search.clear();
        QVERIFY(!dayChange->isActive());

        // A blank query is no search at all
        query->setText(QStringLiteral("   "));
        apply();
        QCOMPARE(search.size(), 1);
        QVERIFY(search.takeFirst().at(0).toStringList().isEmpty());
        QVERIFY(problem->isHidden());
    }

    void buildsQueries()
    {
        using UiUtils::SearchQuery::Field;
        Imap::Mailbox::FavoriteTagsModel tags;
        Gui::MessageListWidget widget(nullptr, &tags, QStringLiteral("account-b"));
        QStandardItemModel model;
        enableSearch(widget, model);
        widget.show();
        auto *query = widget.findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        auto *openBuilder = widget.findChild<QAction *>(QStringLiteral("searchBuilder"));
        QVERIFY(query);
        QVERIFY(openBuilder);
        QSignalSpy search(&widget, &Gui::MessageListWidget::requestingSearch);
        const auto open = [&]() -> Gui::SearchBuilderDialog * {
            openBuilder->trigger();
            const auto dialogs = widget.findChildren<Gui::SearchBuilderDialog *>();
            return dialogs.size() == 1 ? dialogs.constFirst() : nullptr;
        };

        // The query in the quick search opens as its conditions
        query->setText(QStringLiteral("from:ann -subject:report"));
        auto *dialog = open();
        QVERIFY(dialog);
        QVERIFY(dialog->isVisible());
        const auto rows = [&dialog]() {
            return dialog->findChildren<QWidget *>(QStringLiteral("searchBuilderCondition"));
        };
        auto *text = dialog->findChild<QLineEdit *>(QStringLiteral("searchBuilderQuery"));
        auto *problem = dialog->findChild<Gui::InlineNotificationBar *>(QStringLiteral("searchBuilderProblem"));
        auto *match = dialog->findChild<QComboBox *>(QStringLiteral("searchBuilderMatch"));
        auto *add = dialog->findChild<QPushButton *>(QStringLiteral("searchBuilderAdd"));
        QPushButton *searchButton = dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        QVERIFY(text && problem && match && add && searchButton);
        QCOMPARE(rows().size(), 2);
        QCOMPARE(text->text(), QStringLiteral("from:ann -subject:report"));
        QVERIFY(problem->isHidden());
        QVERIFY(searchButton->isEnabled());
        QCOMPARE(rows().at(1)->findChild<QComboBox *>(QStringLiteral("searchBuilderComparison"))->currentText(),
                 i18nc("@item:inlistbox search builder comparison", "does not contain"));

        // A condition added from the keyboard gets the focus; an invalid value is explained and cannot be searched for
        dialog->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(dialog));
        add->setFocus();
        QTest::keyClick(add, Qt::Key_Space);
        QCOMPARE(rows().size(), 3);
        QWidget *size = rows().at(2);
        auto *field = size->findChild<QComboBox *>(QStringLiteral("searchBuilderField"));
        QTRY_VERIFY(field->hasFocus());
        field->setCurrentIndex(field->findData(static_cast<int>(Field::Size)));
        auto *comparison = size->findChild<QComboBox *>(QStringLiteral("searchBuilderComparison"));
        QCOMPARE(comparison->currentText(), i18nc("@item:inlistbox search builder size comparison", "is larger than"));
        auto *value = size->findChild<QLineEdit *>(QStringLiteral("searchBuilderValue"));
        QVERIFY(value->isVisibleTo(dialog));
        QTest::keyClicks(value, QStringLiteral("2X"));
        QVERIFY(!problem->isHidden());
        QCOMPARE(problem->kind(), Gui::InlineNotificationBar::Kind::Error);
        QVERIFY2(problem->text().startsWith(QLatin1String("Condition 3: ")), qPrintable(problem->text()));
        QVERIFY(!searchButton->isEnabled());
        value->clear();
        QTest::keyClicks(value, QStringLiteral("2M"));
        QVERIFY(problem->isHidden());
        QCOMPARE(text->text(), QStringLiteral("from:ann -subject:report size>2M"));

        // Any instead of all, a status, and removing a condition
        match->setCurrentIndex(1);
        QCOMPARE(text->text(), QStringLiteral("from:ann OR -subject:report OR size>2M"));
        auto *first = rows().at(0)->findChild<QComboBox *>(QStringLiteral("searchBuilderField"));
        first->setCurrentIndex(first->findData(static_cast<int>(Field::Is)));
        auto *status = rows().at(0)->findChild<QComboBox *>(QStringLiteral("searchBuilderStatus"));
        QVERIFY(status->isVisibleTo(dialog));
        QVERIFY(!rows().at(0)->findChild<QLineEdit *>(QStringLiteral("searchBuilderValue"))->isVisibleTo(dialog));
        status->setCurrentIndex(status->findData(QStringLiteral("flagged")));
        QCOMPARE(text->text(), QStringLiteral("is:flagged OR -subject:report OR size>2M"));
        rows().at(1)->findChild<QToolButton *>(QStringLiteral("searchBuilderRemove"))->click();
        QCOMPARE(rows().size(), 2);
        QCOMPARE(text->text(), QStringLiteral("is:flagged OR size>2M"));

        // Return in a value searches; the query replaces the quick search
        QTest::keyClick(value, Qt::Key_Return);
        QCOMPARE(query->text(), QStringLiteral("is:flagged OR size>2M"));
        QCOMPARE(search.size(), 1);
        QCOMPARE(search.takeFirst().at(0).toStringList(),
                 QStringList({QStringLiteral("OR"), QStringLiteral("FLAGGED"), QStringLiteral("LARGER"), QStringLiteral("2097152")}));
        QTRY_VERIFY(widget.findChildren<Gui::SearchBuilderDialog *>().isEmpty());

        // A query without a builder form is explained, and the dialog starts with an empty condition
        query->setText(QStringLiteral("from:ann (lunch OR dinner)"));
        dialog = open();
        QVERIFY(dialog);
        problem = dialog->findChild<Gui::InlineNotificationBar *>(QStringLiteral("searchBuilderProblem"));
        QVERIFY(!problem->isHidden());
        QCOMPARE(problem->kind(), Gui::InlineNotificationBar::Kind::Info);
        QVERIFY2(problem->text().contains(QStringLiteral("from:ann AND (lunch OR dinner)")), qPrintable(problem->text()));
        QCOMPARE(rows().size(), 1);
        QVERIFY(dialog->findChild<QLineEdit *>(QStringLiteral("searchBuilderQuery"))->text().isEmpty());
        QVERIFY(!dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
        QVERIFY(!rows().at(0)->findChild<QToolButton *>(QStringLiteral("searchBuilderRemove"))->isEnabled());
        dialog->reject();
        QCOMPARE(query->text(), QStringLiteral("from:ann (lunch OR dinner)"));
        QVERIFY(search.isEmpty());
        QTRY_VERIFY(widget.findChildren<Gui::SearchBuilderDialog *>().isEmpty());

        // So is a raw IMAP query
        widget.findChild<QAction *>(QStringLiteral("searchRaw"))->setChecked(true);
        query->setText(QStringLiteral(":=SEEN"));
        dialog = open();
        QVERIFY(dialog);
        problem = dialog->findChild<Gui::InlineNotificationBar *>(QStringLiteral("searchBuilderProblem"));
        QVERIFY2(problem->text().contains(QStringLiteral("Raw IMAP")), qPrintable(problem->text()));
        dialog->reject();
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
        QVERIFY(!widget.findChild<QTimer *>(QStringLiteral("searchDayChange"))->isActive());

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

    void choosesWhereToSearch()
    {
        using UiUtils::SearchScope::Places;
        Imap::Mailbox::FavoriteTagsModel tags;
        Gui::MessageListWidget widget(nullptr, &tags, QStringLiteral("account-i"));
        QStandardItemModel model;
        enableSearch(widget, model);
        auto *query = widget.findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        query->setText(QStringLiteral("invoice"));
        QSignalSpy search(&widget, &Gui::MessageListWidget::requestingSearch);
        QSignalSpy choice(&widget, &Gui::MessageListWidget::foldersChoiceRequested);
        QCOMPARE(widget.searchPlaces(), Places::CurrentFolder);

        // Another place applies the search at once and shows up in the placeholder
        widget.findChild<QAction *>(QStringLiteral("searchPlaces-all"))->trigger();
        QCOMPARE(widget.searchPlaces(), Places::AllAccounts);
        QCOMPARE(search.size(), 1);
        QCOMPARE(query->placeholderText(), QStringLiteral("Quick Search (All Accounts)"));
        widget.setDateRange({UiUtils::SearchScope::Dates::PastWeek, {}, {}});
        QCOMPARE(query->placeholderText(), QStringLiteral("Quick Search (All Accounts, Past Week)"));
        widget.setDateRange({});

        // Picking folders asks for them, and nothing changes until they are chosen
        auto *folders = widget.findChild<QAction *>(QStringLiteral("searchPlaces-folders"));
        folders->trigger();
        QCOMPARE(choice.size(), 1);
        QCOMPARE(widget.searchPlaces(), Places::AllAccounts);
        QVERIFY(widget.findChild<QAction *>(QStringLiteral("searchPlaces-all"))->isChecked());
        search.clear();
        const QStringList keys{QStringLiteral("account-i/INBOX"), QStringLiteral("account-j/Archive")};
        widget.setSelectedFolders(keys);
        QCOMPARE(widget.searchPlaces(), Places::SelectedFolders);
        QCOMPARE(widget.selectedFolders(), keys);
        QVERIFY(folders->isChecked());
        QCOMPARE(search.size(), 1);
        QCOMPARE(query->placeholderText(), QStringLiteral("Quick Search (2 Selected Folders)"));

        // The shown list can be named after what it is
        widget.findChild<QAction *>(QStringLiteral("searchPlaces-current"))->trigger();
        widget.setCurrentPlaceText(QStringLiteral("Unified Inbox"));
        QCOMPARE(widget.findChild<QAction *>(QStringLiteral("searchPlaces-current"))->text(), QStringLiteral("Unified Inbox"));
        QCOMPARE(query->placeholderText(), QStringLiteral("Quick Search"));
    }

    void savesPlaces()
    {
        using UiUtils::SearchScope::Places;
        Imap::Mailbox::FavoriteTagsModel tags;
        Gui::MessageListWidget widget(nullptr, &tags, QStringLiteral("account-k"));
        QStandardItemModel model;
        enableSearch(widget, model);
        auto *query = widget.findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        query->setText(QStringLiteral("invoice"));
        const QStringList keys{QStringLiteral("account-k/INBOX.Receipts")};
        widget.setSelectedFolders(keys);
        saveSearch(widget, QStringLiteral("Receipts"));
        widget.findChild<QAction *>(QStringLiteral("searchPlaces-account"))->trigger();
        saveSearch(widget, QStringLiteral("Everywhere"));
        widget.findChild<QAction *>(QStringLiteral("searchPlaces-current"))->trigger();
        widget.setSelectedFolders({});
        widget.findChild<QAction *>(QStringLiteral("searchPlaces-current"))->trigger();

        KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"))->reparseConfiguration();
        QMenu *receipts = nullptr;
        QMenu *everywhere = nullptr;
        for (auto *action : savedSearches(widget)->actions()) {
            if (action->text() == QStringLiteral("Receipts"))
                receipts = action->menu();
            if (action->text() == QStringLiteral("Everywhere"))
                everywhere = action->menu();
        }
        QVERIFY(receipts);
        QVERIFY(everywhere);
        receipts->findChild<QAction *>(QStringLiteral("runSavedSearch"))->trigger();
        QCOMPARE(widget.searchPlaces(), Places::SelectedFolders);
        QCOMPARE(widget.selectedFolders(), keys);
        QVERIFY(widget.findChild<QAction *>(QStringLiteral("searchPlaces-folders"))->isChecked());
        everywhere->findChild<QAction *>(QStringLiteral("runSavedSearch"))->trigger();
        QCOMPARE(widget.searchPlaces(), Places::Account);
        // Searches saved before places existed search the shown list
        KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")), QStringLiteral("SavedSearches"));
        group = group.group(QStringLiteral("account-k"));
        for (const QString &id : group.groupList())
            group.group(id).deleteEntry("Places");
        group.sync();
        everywhere->findChild<QAction *>(QStringLiteral("runSavedSearch"))->trigger();
        QCOMPARE(widget.searchPlaces(), Places::CurrentFolder);
    }

    void clearingKeepsPlaces()
    {
        using UiUtils::SearchScope::Places;
        Imap::Mailbox::FavoriteTagsModel tags;
        Gui::MessageListWidget widget(nullptr, &tags, QStringLiteral("account-l"));
        QStandardItemModel model;
        enableSearch(widget, model);
        auto *query = widget.findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        widget.findChild<QAction *>(QStringLiteral("searchPlaces-all"))->trigger();
        query->setText(QStringLiteral("invoice"));
        widget.setDateRange({UiUtils::SearchScope::Dates::PastWeek, {}, {}});

        QSignalSpy search(&widget, &Gui::MessageListWidget::requestingSearch);
        widget.clearSearch();
        QCOMPARE(search.size(), 1);
        QVERIFY(search.first().first().toStringList().isEmpty());
        QVERIFY(query->text().isEmpty());
        QVERIFY(!UiUtils::SearchScope::isLimited(widget.dateRange()));
        QCOMPARE(widget.searchPlaces(), Places::AllAccounts);
        // Nothing to clear asks for nothing
        widget.clearSearch();
        QCOMPARE(search.size(), 1);
    }

    void summarizesSearchAndFilters()
    {
        Imap::Mailbox::FavoriteTagsModel tags;
        Gui::MessageListWidget widget(nullptr, &tags, QStringLiteral("account-s"));
        QStandardItemModel model;
        enableSearch(widget, model);
        auto *query = widget.findChild<QLineEdit *>(QStringLiteral("quickSearch"));
        auto *summary = widget.findChild<QWidget *>(QStringLiteral("searchSummary"));
        auto *text = widget.findChild<QLabel *>(QStringLiteral("searchSummaryText"));
        auto *clearAll = widget.findChild<QToolButton *>(QStringLiteral("clearSearchAndFiltersButton"));
        QVERIFY(query);
        QVERIFY(summary);
        QVERIFY(text);
        QVERIFY(clearAll);
        QVERIFY(!text->accessibleName().isEmpty());
        QVERIFY(!clearAll->toolTip().isEmpty());
        QVERIFY(!clearAll->accessibleDescription().isEmpty());
        const auto apply = [&widget]() { QMetaObject::invokeMethod(&widget, "slotApplySearch"); };
        const QString separator = QStringLiteral(" \u00B7 ");

        // Nothing narrows the list
        QVERIFY(summary->isHidden());

        // The query in effect and where it looks
        query->setText(QStringLiteral("  from:ann  "));
        QVERIFY(summary->isHidden());
        apply();
        QVERIFY(!summary->isHidden());
        QCOMPARE(text->text(), QStringLiteral("Search: from:ann") + separator + QStringLiteral("In: Current Folder"));
        QCOMPARE(text->toolTip(), text->text());

        // Dates and places are part of the search
        chooseDates(widget, UiUtils::SearchScope::Dates::PastWeek);
        widget.findChild<QAction *>(QStringLiteral("searchPlaces-all"))->trigger();
        QCOMPARE(text->text(), QStringLiteral("Search: from:ann") + separator + QStringLiteral("Past Week") + separator
                     + QStringLiteral("In: All Accounts"));

        // An invalid query does not replace the search in effect
        query->setText(QStringLiteral("foo:bar"));
        apply();
        QVERIFY(text->text().startsWith(QStringLiteral("Search: from:ann")));

        // The quick filters of the window
        widget.setActiveFilters({QStringLiteral("Unread"), QStringLiteral("Flagged")});
        QVERIFY(text->text().endsWith(separator + QStringLiteral("Filters: Unread, Flagged")));

        // The button asks the window to clear everything
        QSignalSpy clear(&widget, &Gui::MessageListWidget::clearAllRequested);
        QVERIFY(clearAll->isEnabled());
        clearAll->click();
        QCOMPARE(clear.size(), 1);

        // A date range on its own narrows the list as well
        widget.clearSearch();
        QCOMPARE(text->text(), QStringLiteral("Filters: Unread, Flagged"));
        widget.setDateRange(preset(UiUtils::SearchScope::Dates::Today));
        QCOMPARE(text->text(), QStringLiteral("Today") + separator + QStringLiteral("In: All Accounts") + separator
                     + QStringLiteral("Filters: Unread, Flagged"));
        widget.clearSearch();
        widget.setActiveFilters({});
        QVERIFY(summary->isHidden());
        QVERIFY(text->text().isEmpty());

        // A list which cannot be searched has no search to show
        query->setText(QStringLiteral("invoice"));
        apply();
        QVERIFY(!summary->isHidden());
        widget.setSearchAvailable(false);
        QVERIFY(summary->isHidden());
    }

    void choosesFoldersOfSeveralAccounts()
    {
        using namespace Imap::Mailbox;
        const auto folder = [](const QString &name, const QString &shortName, bool selectable) {
            auto *item = new QStandardItem(shortName);
            item->setData(name, RoleMailboxName);
            item->setData(shortName, RoleShortMailboxName);
            item->setData(selectable, RoleMailboxIsSelectable);
            item->setData(true, RoleIsFetched);
            return item;
        };
        QStandardItemModel alpha;
        alpha.appendRow(folder(QStringLiteral("INBOX"), QStringLiteral("INBOX"), true));
        QStandardItem *archive = folder(QStringLiteral("Archive"), QStringLiteral("Archive"), false);
        archive->appendRow(folder(QStringLiteral("Archive.2024"), QStringLiteral("2024"), true));
        alpha.appendRow(archive);
        // A branch which is not listed yet is not walked
        QStandardItem *unlisted = folder(QStringLiteral("Lists"), QStringLiteral("Lists"), true);
        unlisted->setData(false, RoleIsFetched);
        unlisted->appendRow(folder(QStringLiteral("Lists.kde"), QStringLiteral("kde"), true));
        alpha.appendRow(unlisted);
        QStandardItemModel beta;
        beta.appendRow(folder(QStringLiteral("INBOX"), QStringLiteral("INBOX"), true));

        Gui::SearchFoldersDialog dialog;
        dialog.addAccount(QStringLiteral("alpha"), QStringLiteral("Alpha"), &alpha);
        dialog.addAccount(QStringLiteral("beta"), QStringLiteral("Beta"), &beta);
        dialog.addAccount(QStringLiteral("gamma"), QStringLiteral("Gamma"), nullptr);
        QTreeWidget *tree = dialog.tree();
        QCOMPARE(tree->topLevelItemCount(), 3);
        QTreeWidgetItem *alphaItem = tree->topLevelItem(0);
        QCOMPARE(alphaItem->childCount(), 3);
        QCOMPARE(alphaItem->child(0)->text(0), QStringLiteral("INBOX"));
        QCOMPARE(alphaItem->child(1)->text(0), QStringLiteral("Archive / 2024"));
        QCOMPARE(alphaItem->child(2)->text(0), QStringLiteral("Lists"));
        QVERIFY(!(tree->topLevelItem(2)->flags() & Qt::ItemIsEnabled));

        // Chosen folders of accounts which are not listed are kept
        dialog.setSelectedFolders({QStringLiteral("alpha/Archive.2024"), QStringLiteral("gamma/INBOX")});
        QCOMPARE(alphaItem->child(1)->checkState(0), Qt::Checked);
        QCOMPARE(alphaItem->checkState(0), Qt::PartiallyChecked);
        tree->topLevelItem(1)->setCheckState(0, Qt::Checked);
        QCOMPARE(dialog.selectedFolders(),
                 (QStringList{QStringLiteral("alpha/Archive.2024"), QStringLiteral("beta/INBOX"), QStringLiteral("gamma/INBOX")}));
        alphaItem->setCheckState(0, Qt::Checked);
        QCOMPARE(dialog.selectedFolders().size(), 5);
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
