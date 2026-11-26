/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "SearchBuilderDialog.h"

#include <QComboBox>
#include <QDate>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include <KLocalizedString>

#include "Gui/InlineNotificationBar.h"
#include "UiUtils/IconLoader.h"

namespace Gui {

using UiUtils::SearchBuilder::Condition;
using UiUtils::SearchBuilder::Conditions;
using UiUtils::SearchBuilder::Match;
using UiUtils::SearchBuilder::Operator;
using UiUtils::SearchQuery::Comparison;
using UiUtils::SearchQuery::Field;

struct SearchBuilderDialog::Row {
    QWidget *widget = nullptr;
    QComboBox *field = nullptr;
    QComboBox *comparison = nullptr;
    QLineEdit *value = nullptr;
    /** The value of is: conditions */
    QComboBox *status = nullptr;
    QToolButton *remove = nullptr;
};

namespace {

struct FieldItem {
    Field field;
    QString text;
};

QList<FieldItem> fieldItems()
{
    return {
        {Field::Text, i18nc("@item:inlistbox search builder field", "Any Chosen Field")},
        {Field::From, i18nc("@item:inlistbox search builder field", "From")},
        {Field::To, i18nc("@item:inlistbox search builder field", "To")},
        {Field::Cc, i18nc("@item:inlistbox search builder field", "Cc")},
        {Field::Bcc, i18nc("@item:inlistbox search builder field", "Bcc")},
        {Field::Subject, i18nc("@item:inlistbox search builder field", "Subject")},
        {Field::Body, i18nc("@item:inlistbox search builder field", "Body")},
        {Field::Message, i18nc("@item:inlistbox search builder field", "Whole Message")},
        {Field::List, i18nc("@item:inlistbox search builder field", "Mailing List")},
        {Field::Is, i18nc("@item:inlistbox search builder field", "Status")},
        {Field::Tag, i18nc("@item:inlistbox search builder field", "Tag")},
        {Field::Has, i18nc("@item:inlistbox search builder field", "Attachment")},
        {Field::Date, i18nc("@item:inlistbox search builder field", "Date")},
        {Field::Size, i18nc("@item:inlistbox search builder field", "Size")},
    };
}

QString operatorText(const Field field, const Operator &op)
{
    switch (field) {
    case Field::Date:
        switch (op.comparison) {
        case Comparison::Less:
            return i18nc("@item:inlistbox search builder date comparison", "is before");
        case Comparison::LessOrEqual:
            return i18nc("@item:inlistbox search builder date comparison", "is in or before");
        case Comparison::Greater:
            return i18nc("@item:inlistbox search builder date comparison", "is after");
        case Comparison::GreaterOrEqual:
            return i18nc("@item:inlistbox search builder date comparison", "is in or after");
        default:
            return op.negated ? i18nc("@item:inlistbox search builder date comparison", "is not in")
                              : i18nc("@item:inlistbox search builder date comparison", "is in");
        }
    case Field::Size:
        switch (op.comparison) {
        case Comparison::Greater:
            return i18nc("@item:inlistbox search builder size comparison", "is larger than");
        case Comparison::GreaterOrEqual:
            return i18nc("@item:inlistbox search builder size comparison", "is at least");
        case Comparison::Less:
            return i18nc("@item:inlistbox search builder size comparison", "is smaller than");
        default:
            return i18nc("@item:inlistbox search builder size comparison", "is at most");
        }
    case Field::Is:
    case Field::Tag:
        return op.negated ? i18nc("@item:inlistbox search builder comparison", "is not")
                          : i18nc("@item:inlistbox search builder comparison", "is");
    case Field::Has:
        return op.negated ? i18nc("@item:inlistbox search builder attachment", "is not present")
                          : i18nc("@item:inlistbox search builder attachment", "is present");
    default:
        return op.negated ? i18nc("@item:inlistbox search builder comparison", "does not contain")
                          : i18nc("@item:inlistbox search builder comparison", "contains");
    }
}

QString placeholder(const Field field)
{
    switch (field) {
    case Field::Text:
        return i18nc("@info:placeholder", "Word or phrase");
    case Field::From:
    case Field::To:
    case Field::Cc:
    case Field::Bcc:
        return i18nc("@info:placeholder", "Name or address");
    case Field::List:
        return i18nc("@info:placeholder", "List address");
    case Field::Tag:
        return i18nc("@info:placeholder", "Tag name, such as $label1");
    case Field::Date:
        return i18nc("@info:placeholder date formats stay in English", "YYYY-MM-DD, YYYY-MM, YYYY, today or yesterday");
    case Field::Size:
        return i18nc("@info:placeholder", "Such as 500K, 10M or 2G");
    default:
        return QString();
    }
}

QList<std::pair<QString, QString>> statusItems()
{
    return {
        {QStringLiteral("unread"), i18nc("@item:inlistbox message status", "Unread")},
        {QStringLiteral("read"), i18nc("@item:inlistbox message status", "Read")},
        {QStringLiteral("flagged"), i18nc("@item:inlistbox message status", "Flagged")},
        {QStringLiteral("unflagged"), i18nc("@item:inlistbox message status", "Not Flagged")},
        {QStringLiteral("answered"), i18nc("@item:inlistbox message status", "Answered")},
        {QStringLiteral("unanswered"), i18nc("@item:inlistbox message status", "Not Answered")},
        {QStringLiteral("draft"), i18nc("@item:inlistbox message status", "Draft")},
        {QStringLiteral("deleted"), i18nc("@item:inlistbox message status", "Deleted")},
        {QStringLiteral("forwarded"), i18nc("@item:inlistbox message status", "Forwarded")},
    };
}

}

SearchBuilderDialog::SearchBuilderDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18nc("@title:window", "Search Builder"));

    m_match = new QComboBox(this);
    m_match->setObjectName(QStringLiteral("searchBuilderMatch"));
    m_match->addItem(i18nc("@item:inlistbox search builder match", "all of the conditions"), static_cast<int>(Match::All));
    m_match->addItem(i18nc("@item:inlistbox search builder match", "any of the conditions"), static_cast<int>(Match::Any));
    auto *matchLabel = new QLabel(i18nc("@label:listbox", "&Find messages matching:"), this);
    matchLabel->setBuddy(m_match);
    auto *matchRow = new QHBoxLayout();
    matchRow->addWidget(matchLabel);
    matchRow->addWidget(m_match);
    matchRow->addStretch(1);
    connect(m_match, &QComboBox::currentIndexChanged, this, &SearchBuilderDialog::updateQuery);

    auto *rows = new QWidget(this);
    m_rowLayout = new QVBoxLayout(rows);
    m_rowLayout->setContentsMargins(0, 0, 0, 0);
    m_rowLayout->addStretch(1);
    auto *scroll = new QScrollArea(this);
    scroll->setWidget(rows);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    m_add = new QPushButton(UiUtils::loadIcon(QStringLiteral("list-add")), i18nc("@action:button", "&Add Condition"), this);
    m_add->setObjectName(QStringLiteral("searchBuilderAdd"));
    connect(m_add, &QPushButton::clicked, this, &SearchBuilderDialog::addCondition);
    auto *addRow = new QHBoxLayout();
    addRow->addWidget(m_add);
    addRow->addStretch(1);

    m_query = new QLineEdit(this);
    m_query->setObjectName(QStringLiteral("searchBuilderQuery"));
    m_query->setReadOnly(true);
    m_query->setPlaceholderText(i18nc("@info:placeholder", "Add a condition with a value"));
    m_query->setToolTip(i18nc("@info:tooltip", "The same search as quick search text; it replaces the text in the quick search"));
    auto *queryLabel = new QLabel(i18nc("@label:textbox", "&Query:"), this);
    queryLabel->setBuddy(m_query);
    auto *queryRow = new QHBoxLayout();
    queryRow->addWidget(queryLabel);
    queryRow->addWidget(m_query, 1);

    m_problem = new InlineNotificationBar(this);
    m_problem->setObjectName(QStringLiteral("searchBuilderProblem"));
    m_problem->setAccessibleName(i18nc("@info:accessible", "Search builder problem"));
    m_problem->closeButton()->hide();
    m_problem->setAutoHideTimeout(0);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_search = buttons->button(QDialogButtonBox::Ok);
    m_search->setText(i18nc("@action:button", "Search"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(matchRow);
    layout->addWidget(scroll, 1);
    layout->addLayout(addRow);
    layout->addLayout(queryRow);
    layout->addWidget(m_problem);
    layout->addWidget(buttons);
    resize(640, 360);

    appendRow(Condition{});
    updateRows();
}

SearchBuilderDialog::~SearchBuilderDialog()
{
    qDeleteAll(m_rows);
}

void SearchBuilderDialog::setQuery(const QString &text)
{
    const UiUtils::SearchBuilder::Result result = UiUtils::SearchBuilder::fromQuery(text, QDate::currentDate());
    if (!result.isValid()) {
        explainQuery(result.problem);
        return;
    }
    m_queryProblem.clear();
    setConditions(result.conditions);
}

void SearchBuilderDialog::explainQuery(const QString &problem)
{
    m_queryProblem = problem;
    setConditions({});
}

void SearchBuilderDialog::setConditions(const Conditions &conditions)
{
    while (!m_rows.isEmpty())
        removeRow(m_rows.constLast());
    {
        const QSignalBlocker blocker(m_match);
        m_match->setCurrentIndex(m_match->findData(static_cast<int>(conditions.match)));
    }
    for (const Condition &condition : conditions.conditions)
        appendRow(condition);
    if (m_rows.isEmpty())
        appendRow(Condition{});
    updateRows();
}

Conditions SearchBuilderDialog::conditions() const
{
    Conditions result;
    result.match = static_cast<Match>(m_match->currentData().toInt());
    for (const Row *row : m_rows)
        result.conditions << conditionOf(row);
    return result;
}

QString SearchBuilderDialog::query() const
{
    return UiUtils::SearchBuilder::queryText(conditions());
}

void SearchBuilderDialog::addCondition()
{
    Row *row = appendRow(Condition{});
    updateRows();
    row->field->setFocus(Qt::OtherFocusReason);
}

SearchBuilderDialog::Row *SearchBuilderDialog::appendRow(const Condition &condition)
{
    auto *row = new Row;
    row->widget = new QWidget(m_rowLayout->parentWidget());
    row->widget->setObjectName(QStringLiteral("searchBuilderCondition"));
    row->field = new QComboBox(row->widget);
    row->field->setObjectName(QStringLiteral("searchBuilderField"));
    for (const FieldItem &item : fieldItems())
        row->field->addItem(item.text, static_cast<int>(item.field));
    row->field->setItemData(0, i18nc("@info:tooltip", "The fields chosen in the search options"), Qt::ToolTipRole);
    row->comparison = new QComboBox(row->widget);
    row->comparison->setObjectName(QStringLiteral("searchBuilderComparison"));
    row->value = new QLineEdit(row->widget);
    row->value->setObjectName(QStringLiteral("searchBuilderValue"));
    row->value->setClearButtonEnabled(true);
    row->status = new QComboBox(row->widget);
    row->status->setObjectName(QStringLiteral("searchBuilderStatus"));
    for (const auto &[key, text] : statusItems())
        row->status->addItem(text, key);
    row->remove = new QToolButton(row->widget);
    row->remove->setObjectName(QStringLiteral("searchBuilderRemove"));
    row->remove->setIcon(UiUtils::loadIcon(QStringLiteral("list-remove")));
    row->remove->setFocusPolicy(Qt::StrongFocus);

    auto *layout = new QHBoxLayout(row->widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(row->field);
    layout->addWidget(row->comparison);
    layout->addWidget(row->value, 1);
    layout->addWidget(row->status, 1);
    layout->addWidget(row->remove);
    // Before the stretch, which keeps the rows at the top
    m_rowLayout->insertWidget(m_rowLayout->count() - 1, row->widget);
    // The layout would show it only once the event loop runs, too late to move the focus to it
    row->widget->show();
    m_rows << row;

    showField(row, condition);
    connect(row->field, &QComboBox::currentIndexChanged, this, [this, row]() {
        Condition condition = conditionOf(row);
        const QList<Operator> offered = UiUtils::SearchBuilder::operators(condition.field);
        // Keep "does not" when the new field offers it; otherwise start with its first comparison
        const bool kept = std::any_of(offered.cbegin(), offered.cend(), [&](const Operator &op) {
            return op.comparison == condition.comparison && op.negated == condition.negated;
        });
        if (!kept) {
            condition.comparison = offered.constFirst().comparison;
            condition.negated = offered.constFirst().negated;
        }
        showField(row, condition);
        updateQuery();
    });
    connect(row->comparison, &QComboBox::currentIndexChanged, this, &SearchBuilderDialog::updateQuery);
    connect(row->value, &QLineEdit::textChanged, this, &SearchBuilderDialog::updateQuery);
    connect(row->value, &QLineEdit::returnPressed, this, [this]() {
        if (m_search->isEnabled())
            accept();
    });
    connect(row->status, &QComboBox::currentIndexChanged, this, &SearchBuilderDialog::updateQuery);
    connect(row->remove, &QToolButton::clicked, this, [this, row]() {
        const int index = m_rows.indexOf(row);
        removeRow(row);
        updateRows();
        if (!m_rows.isEmpty())
            m_rows.at(std::min<int>(index, m_rows.size() - 1))->field->setFocus(Qt::OtherFocusReason);
    });
    return row;
}

void SearchBuilderDialog::removeRow(Row *row)
{
    m_rows.removeOne(row);
    delete row->widget;
    delete row;
}

void SearchBuilderDialog::showField(Row *row, const Condition &condition)
{
    const QSignalBlocker fieldBlocker(row->field);
    const QSignalBlocker comparisonBlocker(row->comparison);
    const QSignalBlocker statusBlocker(row->status);
    row->field->setCurrentIndex(row->field->findData(static_cast<int>(condition.field)));
    row->comparison->clear();
    const QList<Operator> offered = UiUtils::SearchBuilder::operators(condition.field);
    for (int i = 0; i < offered.size(); ++i) {
        row->comparison->addItem(operatorText(condition.field, offered[i]), i);
        if (offered[i].comparison == condition.comparison && offered[i].negated == condition.negated)
            row->comparison->setCurrentIndex(i);
    }
    const bool status = condition.field == Field::Is;
    row->status->setVisible(status);
    row->value->setVisible(!status && UiUtils::SearchBuilder::needsValue(condition.field));
    if (status) {
        const int index = row->status->findData(condition.value);
        row->status->setCurrentIndex(std::max(index, 0));
    } else if (row->value->isVisibleTo(row->widget)) {
        row->value->setPlaceholderText(placeholder(condition.field));
        if (row->value->text() != condition.value) {
            const QSignalBlocker valueBlocker(row->value);
            row->value->setText(condition.value);
        }
    }
}

Condition SearchBuilderDialog::conditionOf(const Row *row) const
{
    Condition condition;
    condition.field = static_cast<Field>(row->field->currentData().toInt());
    const QList<Operator> offered = UiUtils::SearchBuilder::operators(condition.field);
    const int index = row->comparison->currentData().toInt();
    if (index >= 0 && index < offered.size()) {
        condition.comparison = offered[index].comparison;
        condition.negated = offered[index].negated;
    }
    if (condition.field == Field::Is)
        condition.value = row->status->currentData().toString();
    else if (UiUtils::SearchBuilder::needsValue(condition.field))
        condition.value = row->value->text();
    return condition;
}

void SearchBuilderDialog::updateRows()
{
    for (int i = 0; i < m_rows.size(); ++i) {
        Row *row = m_rows[i];
        const int number = i + 1;
        row->field->setAccessibleName(i18nc("@label:listbox", "Field of condition %1", number));
        row->comparison->setAccessibleName(i18nc("@label:listbox", "Comparison of condition %1", number));
        row->value->setAccessibleName(i18nc("@label:textbox", "Value of condition %1", number));
        row->status->setAccessibleName(i18nc("@label:listbox", "Status of condition %1", number));
        row->remove->setToolTip(i18nc("@info:tooltip", "Remove condition %1", number));
        row->remove->setAccessibleName(row->remove->toolTip());
        // A single condition stays, so that there is always a row to fill in
        row->remove->setEnabled(m_rows.size() > 1);
    }
    // New rows are created after the buttons below them; keep the tab order in the order shown
    QWidget *previous = m_match;
    for (const Row *row : std::as_const(m_rows)) {
        for (QWidget *widget : {static_cast<QWidget *>(row->field), static_cast<QWidget *>(row->comparison),
                                static_cast<QWidget *>(row->value), static_cast<QWidget *>(row->status),
                                static_cast<QWidget *>(row->remove)}) {
            setTabOrder(previous, widget);
            previous = widget;
        }
    }
    setTabOrder(previous, m_add);
    setTabOrder(m_add, m_query);
    updateQuery();
}

void SearchBuilderDialog::updateQuery()
{
    const QDate today = QDate::currentDate();
    QString problem;
    bool empty = true;
    for (int i = 0; i < m_rows.size() && problem.isEmpty(); ++i) {
        const QString text = UiUtils::SearchBuilder::conditionText(conditionOf(m_rows[i]));
        if (text.isEmpty())
            continue;
        empty = false;
        const UiUtils::SearchQuery::Query parsed = UiUtils::SearchQuery::Query::parse(text, today);
        if (!parsed.isValid())
            problem = i18nc("@info condition number, then why it cannot be searched for", "Condition %1: %2", i + 1, parsed.error().message);
    }
    m_query->setText(query());
    m_query->setCursorPosition(0);
    m_search->setEnabled(!empty && problem.isEmpty());
    using Kind = InlineNotificationBar::Kind;
    const Kind kind = problem.isEmpty() ? Kind::Info : Kind::Error;
    const QString message = problem.isEmpty() ? m_queryProblem : problem;
    if (message.isEmpty()) {
        m_problem->hide();
    } else if (m_problem->isHidden() || m_problem->kind() != kind || m_problem->text() != message) {
        // Showing a message alerts screen readers, so only a new one is shown
        m_problem->showMessage(kind, message);
    }
}

}
