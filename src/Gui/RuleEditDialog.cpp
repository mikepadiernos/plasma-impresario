/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "RuleEditDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>

#include <KLocalizedString>

#include "StateColors.h"
#include "UiUtils/IconLoader.h"

namespace Gui {

namespace MailRules = UiUtils::MailRules;

namespace {

const QList<MailRules::Field> allFields = {MailRules::Field::Sender, MailRules::Field::Recipients,
                                           MailRules::Field::Subject, MailRules::Field::Attachment,
                                           MailRules::Field::Folder,  MailRules::Field::Tag,
                                           MailRules::Field::Junk};
const QList<MailRules::ActionKind> allActions = {MailRules::ActionKind::Move,     MailRules::ActionKind::Archive,
                                                 MailRules::ActionKind::Delete,   MailRules::ActionKind::Flag,
                                                 MailRules::ActionKind::MarkRead, MailRules::ActionKind::AddTag,
                                                 MailRules::ActionKind::Notify};

QToolButton *removeButton(QWidget *parent, const QString &toolTip)
{
    auto *button = new QToolButton(parent);
    button->setIcon(UiUtils::SymbolicIcon::fromTheme(QStringLiteral("list-remove")));
    button->setToolTip(toolTip);
    button->setAccessibleName(toolTip);
    button->setAutoRaise(true);
    return button;
}

/** @short One condition: the field, the comparison and, for fields which take one, the value */
class ConditionRow : public QWidget
{
public:
    ConditionRow(const MailRules::Condition &condition, const std::function<void()> &changed, QWidget *parent)
        : QWidget(parent)
        , m_field(new QComboBox(this))
        , m_comparison(new QComboBox(this))
        , m_value(new QLineEdit(this))
    {
        setObjectName(QStringLiteral("ruleCondition"));
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        m_field->setObjectName(QStringLiteral("conditionField"));
        m_comparison->setObjectName(QStringLiteral("conditionComparison"));
        m_value->setObjectName(QStringLiteral("conditionValue"));
        m_field->setAccessibleName(i18nc("@label:listbox", "Field"));
        m_comparison->setAccessibleName(i18nc("@label:listbox", "Comparison"));
        m_value->setAccessibleName(i18nc("@label:textbox", "Value"));
        for (MailRules::Field field : allFields)
            m_field->addItem(MailRules::fieldName(field), static_cast<int>(field));
        layout->addWidget(m_field);
        layout->addWidget(m_comparison);
        layout->addWidget(m_value, 1);
        QToolButton *remove = removeButton(this, i18nc("@action:button", "Remove Condition"));
        layout->addWidget(remove);

        m_field->setCurrentIndex(m_field->findData(static_cast<int>(condition.field)));
        fillComparisons(condition.comparison);
        m_value->setText(condition.value);
        connect(m_field, &QComboBox::currentIndexChanged, this, [this, changed]() {
            fillComparisons(MailRules::Comparison::Contains);
            changed();
        });
        connect(m_comparison, &QComboBox::currentIndexChanged, this, changed);
        connect(m_value, &QLineEdit::textChanged, this, changed);
        connect(remove, &QToolButton::clicked, this, [this, changed]() {
            hide();
            deleteLater();
            setParent(nullptr);
            changed();
        });
    }

    MailRules::Condition condition() const
    {
        MailRules::Condition condition;
        condition.field = field();
        condition.comparison = static_cast<MailRules::Comparison>(m_comparison->currentData().toInt());
        condition.value = MailRules::takesValue(condition.field) ? m_value->text().trimmed() : QString();
        return condition;
    }

private:
    MailRules::Field field() const
    {
        return static_cast<MailRules::Field>(m_field->currentData().toInt());
    }

    void fillComparisons(MailRules::Comparison wanted)
    {
        const MailRules::Field current = field();
        QSignalBlocker blocker(m_comparison);
        m_comparison->clear();
        for (MailRules::Comparison comparison : MailRules::comparisonsFor(current))
            m_comparison->addItem(MailRules::comparisonName(current, comparison), static_cast<int>(comparison));
        const int index = m_comparison->findData(static_cast<int>(wanted));
        m_comparison->setCurrentIndex(index < 0 ? 0 : index);
        m_value->setVisible(MailRules::takesValue(current));
        m_value->setPlaceholderText(current == MailRules::Field::Folder ? i18nc("@info:placeholder", "Folder name")
                                    : current == MailRules::Field::Tag  ? i18nc("@info:placeholder", "Tag")
                                                                        : i18nc("@info:placeholder", "Text"));
    }

    QComboBox *m_field;
    QComboBox *m_comparison;
    QLineEdit *m_value;
};

/** @short One action: what to do and, for moving and tagging, the folder or the tag */
class ActionRow : public QWidget
{
public:
    ActionRow(const MailRules::Action &action, const std::function<void()> &changed, QWidget *parent)
        : QWidget(parent)
        , m_kind(new QComboBox(this))
        , m_value(new QLineEdit(this))
    {
        setObjectName(QStringLiteral("ruleAction"));
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        m_kind->setObjectName(QStringLiteral("actionKind"));
        m_value->setObjectName(QStringLiteral("actionValue"));
        m_kind->setAccessibleName(i18nc("@label:listbox", "Action"));
        m_value->setAccessibleName(i18nc("@label:textbox", "Folder or tag"));
        for (MailRules::ActionKind kind : allActions)
            m_kind->addItem(MailRules::actionName(kind), static_cast<int>(kind));
        layout->addWidget(m_kind);
        layout->addWidget(m_value, 1);
        QToolButton *remove = removeButton(this, i18nc("@action:button", "Remove Action"));
        layout->addWidget(remove);
        m_kind->setCurrentIndex(m_kind->findData(static_cast<int>(action.kind)));
        m_value->setText(action.value);
        updateValue();
        connect(m_kind, &QComboBox::currentIndexChanged, this, [this, changed]() {
            updateValue();
            changed();
        });
        connect(m_value, &QLineEdit::textChanged, this, changed);
        connect(remove, &QToolButton::clicked, this, [this, changed]() {
            hide();
            deleteLater();
            setParent(nullptr);
            changed();
        });
    }

    MailRules::Action action() const
    {
        MailRules::Action action;
        action.kind = static_cast<MailRules::ActionKind>(m_kind->currentData().toInt());
        action.value = MailRules::takesValue(action.kind) ? m_value->text().trimmed() : QString();
        return action;
    }

private:
    void updateValue()
    {
        const auto kind = static_cast<MailRules::ActionKind>(m_kind->currentData().toInt());
        m_value->setVisible(MailRules::takesValue(kind));
        m_value->setPlaceholderText(kind == MailRules::ActionKind::Move ? i18nc("@info:placeholder", "Folder name")
                                                                        : i18nc("@info:placeholder", "Tag"));
    }

    QComboBox *m_kind;
    QLineEdit *m_value;
};

}

RuleEditDialog::RuleEditDialog(const QList<QPair<QString, QString>> &accounts, QWidget *parent)
    : QDialog(parent)
    , m_name(new QLineEdit(this))
    , m_enabled(new QCheckBox(i18nc("@option:check", "Rule is on"), this))
    , m_account(new QComboBox(this))
    , m_folders(new QPlainTextEdit(this))
    , m_match(new QComboBox(this))
    , m_stop(new QCheckBox(i18nc("@option:check", "Skip the rules after this one for matching messages"), this))
    , m_summary(new QLabel(this))
    , m_feedback(new QLabel(this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this))
{
    setObjectName(QStringLiteral("ruleEditDialog"));
    setWindowTitle(i18nc("@title:window", "Edit Rule"));
    m_name->setObjectName(QStringLiteral("ruleName"));
    m_enabled->setObjectName(QStringLiteral("ruleEnabled"));
    m_account->setObjectName(QStringLiteral("ruleAccount"));
    m_folders->setObjectName(QStringLiteral("ruleFolders"));
    m_match->setObjectName(QStringLiteral("ruleMatch"));
    m_stop->setObjectName(QStringLiteral("ruleStop"));
    m_summary->setObjectName(QStringLiteral("ruleSummary"));
    m_feedback->setObjectName(QStringLiteral("ruleFeedback"));

    m_account->addItem(i18nc("@item:inlistbox rule scope", "Every account"), QString());
    for (const auto &[id, name] : accounts)
        m_account->addItem(name, id);
    m_folders->setPlaceholderText(i18nc("@info:placeholder", "INBOX"));
    m_folders->setToolTip(i18nc("@info:tooltip", "One folder per line; without any, the rule checks the INBOX"));
    m_folders->setTabChangesFocus(true);
    m_folders->setMaximumHeight(m_folders->fontMetrics().lineSpacing() * 4 + 12);
    m_match->addItem(i18nc("@item:inlistbox", "All conditions match"), static_cast<int>(MailRules::Match::All));
    m_match->addItem(i18nc("@item:inlistbox", "Any condition matches"), static_cast<int>(MailRules::Match::Any));

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:textbox", "Name:"), m_name);
    form->addRow(QString(), m_enabled);
    form->addRow(i18nc("@label:listbox", "Account:"), m_account);
    form->addRow(i18nc("@label:textbox", "New mail in:"), m_folders);
    layout->addLayout(form);

    auto *conditionsBox = new QGroupBox(i18nc("@title:group", "Conditions"), this);
    auto *conditionsLayout = new QVBoxLayout(conditionsBox);
    conditionsLayout->addWidget(m_match);
    m_conditions = new QVBoxLayout;
    conditionsLayout->addLayout(m_conditions);
    auto *addCondition = new QPushButton(QIcon::fromTheme(QStringLiteral("list-add")),
                                         i18nc("@action:button", "Add Condition"), conditionsBox);
    addCondition->setObjectName(QStringLiteral("addCondition"));
    conditionsLayout->addWidget(addCondition, 0, Qt::AlignLeft);
    layout->addWidget(conditionsBox);

    auto *actionsBox = new QGroupBox(i18nc("@title:group", "Actions"), this);
    auto *actionsLayout = new QVBoxLayout(actionsBox);
    m_actions = new QVBoxLayout;
    actionsLayout->addLayout(m_actions);
    auto *addAction = new QPushButton(QIcon::fromTheme(QStringLiteral("list-add")),
                                      i18nc("@action:button", "Add Action"), actionsBox);
    addAction->setObjectName(QStringLiteral("addAction"));
    actionsLayout->addWidget(addAction, 0, Qt::AlignLeft);
    actionsLayout->addWidget(m_stop);
    layout->addWidget(actionsBox);

    m_summary->setWordWrap(true);
    m_feedback->setWordWrap(true);
    m_feedback->setTextFormat(Qt::PlainText);
    layout->addWidget(m_summary);
    layout->addWidget(m_feedback);
    layout->addStretch();
    layout->addWidget(m_buttons);

    connect(addCondition, &QPushButton::clicked, this, [this]() { this->addCondition({}); });
    connect(addAction, &QPushButton::clicked, this, [this]() { this->addAction({}); });
    connect(m_name, &QLineEdit::textChanged, this, &RuleEditDialog::refresh);
    connect(m_folders, &QPlainTextEdit::textChanged, this, &RuleEditDialog::refresh);
    connect(m_account, &QComboBox::currentIndexChanged, this, &RuleEditDialog::refresh);
    connect(m_match, &QComboBox::currentIndexChanged, this, &RuleEditDialog::refresh);
    connect(m_stop, &QCheckBox::toggled, this, &RuleEditDialog::refresh);
    connect(m_buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (canAccept())
            accept();
    });
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    setRule({});
}

void RuleEditDialog::clearRows(QVBoxLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

void RuleEditDialog::setRule(const MailRules::Rule &rule)
{
    m_original = rule;
    m_name->setText(rule.name);
    m_enabled->setChecked(rule.enabled);
    int account = m_account->findData(rule.accountId);
    if (account < 0) {
        // An account which is gone keeps its rule until the user chooses another scope
        m_account->addItem(rule.accountId, rule.accountId);
        account = m_account->count() - 1;
    }
    m_account->setCurrentIndex(account);
    m_folders->setPlainText(rule.folders.join(QLatin1Char('\n')));
    m_match->setCurrentIndex(m_match->findData(static_cast<int>(rule.match)));
    m_stop->setChecked(rule.stop);
    clearRows(m_conditions);
    clearRows(m_actions);
    for (const MailRules::Condition &condition : rule.conditions)
        addCondition(condition);
    for (const MailRules::Action &action : rule.actions)
        addAction(action);
    refresh();
}

void RuleEditDialog::addCondition(const MailRules::Condition &condition)
{
    m_conditions->addWidget(new ConditionRow(condition, [this]() { refresh(); }, this));
    refresh();
}

void RuleEditDialog::addAction(const MailRules::Action &action)
{
    m_actions->addWidget(new ActionRow(action, [this]() { refresh(); }, this));
    refresh();
}

MailRules::Rule RuleEditDialog::rule() const
{
    MailRules::Rule rule = m_original;
    rule.name = m_name->text().trimmed();
    rule.enabled = m_enabled->isChecked();
    rule.accountId = m_account->currentData().toString();
    rule.folders.clear();
    const QStringList lines = m_folders->toPlainText().split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString folder = line.trimmed();
        if (!folder.isEmpty() && !rule.folders.contains(folder))
            rule.folders << folder;
    }
    rule.match = static_cast<MailRules::Match>(m_match->currentData().toInt());
    rule.stop = m_stop->isChecked();
    rule.conditions.clear();
    for (int i = 0; i < m_conditions->count(); ++i) {
        if (auto *row = dynamic_cast<ConditionRow *>(m_conditions->itemAt(i)->widget()))
            rule.conditions << row->condition();
    }
    rule.actions.clear();
    for (int i = 0; i < m_actions->count(); ++i) {
        if (auto *row = dynamic_cast<ActionRow *>(m_actions->itemAt(i)->widget()))
            rule.actions << row->action();
    }
    return rule;
}

QString RuleEditDialog::feedback() const
{
    return m_feedback->text();
}

bool RuleEditDialog::canAccept() const
{
    return MailRules::problems(rule()).isEmpty();
}

void RuleEditDialog::refresh()
{
    const MailRules::Rule current = rule();
    const QStringList problems = MailRules::problems(current);
    const QStringList warnings = MailRules::warnings(current);
    m_summary->setText(current.actions.isEmpty() ? QString() : MailRules::summary(current));
    QStringList lines = problems + warnings;
    if (current.movesMessages() && !current.isReviewed())
        lines << i18nc("@info", "It moves messages, so it runs on new mail by itself only after its matches were "
                                "previewed once.");
    m_feedback->setText(lines.join(QLatin1Char('\n')));
    QPalette palette = this->palette();
    if (!problems.isEmpty())
        palette.setColor(QPalette::WindowText, StateColors::text(StateColors::State::Negative));
    m_feedback->setPalette(palette);
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(problems.isEmpty());
}

}
