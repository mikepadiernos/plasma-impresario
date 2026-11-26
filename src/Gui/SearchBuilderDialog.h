/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDialog>
#include <QList>

#include "UiUtils/SearchBuilder.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QToolButton;
class QVBoxLayout;

namespace Gui {

class InlineNotificationBar;

/** @short Build a quick search query from conditions of which all or any must match

Each row is one condition: a field, a comparison and a value. The equivalent query text is shown below the conditions
as they change, and a condition which cannot be searched for is explained there. An existing query opens as its
conditions when it has a builder form; otherwise the dialog explains why and starts with an empty condition.
*/
class SearchBuilderDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SearchBuilderDialog(QWidget *parent = nullptr);
    ~SearchBuilderDialog() override;

    /** @short Show the query @arg text as conditions, or explain why it cannot be shown */
    void setQuery(const QString &text);
    /** @short Start with an empty condition and explain with @arg problem why the current query is not shown */
    void explainQuery(const QString &problem);

    void setConditions(const UiUtils::SearchBuilder::Conditions &conditions);
    UiUtils::SearchBuilder::Conditions conditions() const;
    /** @short The query text of the conditions */
    QString query() const;

    /** @short Add an empty condition and move the focus to it */
    void addCondition();

private:
    struct Row;

    Row *appendRow(const UiUtils::SearchBuilder::Condition &condition);
    void removeRow(Row *row);
    void showField(Row *row, const UiUtils::SearchBuilder::Condition &condition);
    UiUtils::SearchBuilder::Condition conditionOf(const Row *row) const;
    void updateRows();
    void updateQuery();

    QComboBox *m_match;
    QVBoxLayout *m_rowLayout;
    QPushButton *m_add;
    QLineEdit *m_query;
    InlineNotificationBar *m_problem;
    QPushButton *m_search;
    QList<Row *> m_rows;
    /** Why the query which the dialog was opened with is not shown */
    QString m_queryProblem;
};

}
