/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "RulesDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <KLocalizedString>

#include "RuleEditDialog.h"
#include "UiUtils/IconLoader.h"

namespace Gui {

namespace MailRules = UiUtils::MailRules;

namespace {

QPushButton *button(QWidget *parent, const QString &name, const QString &icon, const QString &text)
{
    auto *result = new QPushButton(UiUtils::loadIcon(icon), text, parent);
    result->setObjectName(name);
    return result;
}

QString folderLabel(const QString &folder)
{
    return folder.compare(QLatin1String("INBOX"), Qt::CaseInsensitive) == 0 ? i18nc("@item the INBOX folder", "Inbox")
                                                                            : folder;
}

}

RulesDialog::RulesDialog(const Accounts &accounts, QWidget *parent)
    : QDialog(parent)
    , m_accounts(accounts)
    , m_list(new QListWidget(this))
    , m_add(button(this, QStringLiteral("ruleAdd"), QStringLiteral("list-add"), i18nc("@action:button", "Add…")))
    , m_edit(button(this, QStringLiteral("ruleEdit"), QStringLiteral("document-edit"),
                    i18nc("@action:button", "Edit…")))
    , m_remove(button(this, QStringLiteral("ruleRemove"), QStringLiteral("list-remove"),
                      i18nc("@action:button", "Remove")))
    , m_up(button(this, QStringLiteral("ruleUp"), QStringLiteral("go-up"), i18nc("@action:button", "Move Up")))
    , m_down(button(this, QStringLiteral("ruleDown"), QStringLiteral("go-down"), i18nc("@action:button", "Move Down")))
    , m_preview(button(this, QStringLiteral("rulePreview"), QStringLiteral("view-preview"),
                       i18nc("@action:button", "Preview")))
    , m_apply(button(this, QStringLiteral("ruleApply"), QStringLiteral("system-run"),
                     i18nc("@action:button", "Apply to Folder…")))
    , m_previewSummary(new QLabel(this))
    , m_previewList(new QTreeWidget(this))
{
    setObjectName(QStringLiteral("rulesDialog"));
    setWindowTitle(i18nc("@title:window", "Mail Rules"));
    m_list->setObjectName(QStringLiteral("rulesList"));
    m_list->setAccessibleName(i18nc("@label", "Rules, in the order they run"));
    m_list->setWordWrap(true);
    m_list->setAlternatingRowColors(true);
    m_previewSummary->setObjectName(QStringLiteral("rulePreviewSummary"));
    m_previewSummary->setWordWrap(true);
    m_previewList->setObjectName(QStringLiteral("rulePreviewList"));
    m_previewList->setRootIsDecorated(false);
    m_previewList->setHeaderLabels({i18nc("@title:column", "Subject"), i18nc("@title:column", "From"),
                                    i18nc("@title:column", "Received")});
    m_previewList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_previewList->setAccessibleName(i18nc("@label", "Messages the rule matches"));

    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(i18nc("@info", "Rules run on new mail in their order. A rule which moves messages runs by "
                                            "itself only after its matches were previewed once."),
                             this);
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto *row = new QHBoxLayout;
    row->addWidget(m_list, 1);
    auto *buttons = new QVBoxLayout;
    for (QPushButton *b : {m_add, m_edit, m_remove, m_up, m_down})
        buttons->addWidget(b);
    buttons->addSpacing(12);
    buttons->addWidget(m_preview);
    buttons->addWidget(m_apply);
    buttons->addStretch();
    row->addLayout(buttons);
    layout->addLayout(row, 2);
    layout->addWidget(m_previewSummary);
    layout->addWidget(m_previewList, 1);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(box);
    resize(720, 560);

    m_confirmation = [this](const QString &question) {
        return QMessageBox::question(this, i18nc("@title:window", "Apply Rule"), question) == QMessageBox::Yes;
    };
    m_editor = [this](MailRules::Rule &rule) {
        RuleEditDialog editor(m_accounts, this);
        editor.setRule(rule);
        if (editor.exec() != QDialog::Accepted)
            return false;
        rule = editor.rule();
        return true;
    };

    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_list, &QListWidget::currentRowChanged, this, [this]() {
        m_previewList->clear();
        m_previewSummary->clear();
        updateButtons();
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, &RulesDialog::editSelected);
    connect(m_list, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        const int row = m_list->row(item);
        if (row < 0 || row >= m_rules.size())
            return;
        m_rules[row].enabled = item->checkState() == Qt::Checked;
    });
    connect(m_add, &QPushButton::clicked, this, [this]() {
        MailRules::Rule rule;
        rule.name = i18nc("@item default name of a new rule", "New Rule");
        if (!m_accountId.isEmpty())
            rule.accountId = m_accountId;
        addRule(rule);
    });
    connect(m_edit, &QPushButton::clicked, this, &RulesDialog::editSelected);
    connect(m_remove, &QPushButton::clicked, this, &RulesDialog::removeSelected);
    connect(m_up, &QPushButton::clicked, this, [this]() { moveSelected(-1); });
    connect(m_down, &QPushButton::clicked, this, [this]() { moveSelected(1); });
    connect(m_preview, &QPushButton::clicked, this, &RulesDialog::previewSelected);
    connect(m_apply, &QPushButton::clicked, this, &RulesDialog::applySelected);
    updateButtons();
}

void RulesDialog::setRules(const QList<MailRules::Rule> &rules)
{
    m_rules = rules;
    fillList(rules.isEmpty() ? -1 : 0);
}

QList<MailRules::Rule> RulesDialog::rules() const
{
    return m_rules;
}

void RulesDialog::setShownFolder(const QString &accountId, const QString &folder, const MessageSource &messages)
{
    m_accountId = accountId;
    m_folder = folder;
    m_messages = messages;
    updateButtons();
}

void RulesDialog::setApplier(const Applier &applier)
{
    m_applier = applier;
    updateButtons();
}

void RulesDialog::setConfirmation(const Confirmation &confirmation)
{
    m_confirmation = confirmation;
}

void RulesDialog::setEditor(const Editor &editor)
{
    m_editor = editor;
}

QListWidget *RulesDialog::list() const
{
    return m_list;
}

QTreeWidget *RulesDialog::previewList() const
{
    return m_previewList;
}

QString RulesDialog::previewSummary() const
{
    return m_previewSummary->text();
}

int RulesDialog::selectedRow() const
{
    const int row = m_list->currentRow();
    return row >= 0 && row < m_rules.size() ? row : -1;
}

void RulesDialog::fillList(int current)
{
    QSignalBlocker blocker(m_list);
    m_list->clear();
    for (const MailRules::Rule &rule : std::as_const(m_rules)) {
        QString scope;
        if (!rule.accountId.isEmpty()) {
            const auto account = std::find_if(m_accounts.cbegin(), m_accounts.cend(),
                                              [&rule](const auto &pair) { return pair.first == rule.accountId; });
            scope = account == m_accounts.cend() ? rule.accountId : account->second;
        }
        QStringList folders;
        for (const QString &folder : rule.checkedFolders())
            folders << folderLabel(folder);
        const QString folderList = folders.join(i18nc("@info list separator", ", "));
        const QString where = scope.isEmpty()
            ? i18nc("@info %1 folders", "New mail in %1 of every account", folderList)
            : i18nc("@info %1 folders, %2 account", "New mail in %1 of %2", folderList, scope);
        QString text = rule.name + QLatin1Char('\n') + MailRules::summary(rule) + QLatin1Char('\n') + where;
        auto *item = new QListWidgetItem(text, m_list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(rule.enabled ? Qt::Checked : Qt::Unchecked);
        if (rule.needsReview()) {
            item->setIcon(UiUtils::SymbolicIcon::fromTheme(QStringLiteral("dialog-warning")));
            item->setToolTip(i18nc("@info:tooltip", "Waits for a preview before it moves new mail by itself"));
        }
    }
    m_list->setCurrentRow(std::min(current, static_cast<int>(m_rules.size()) - 1));
    blocker.unblock();
    m_previewList->clear();
    m_previewSummary->clear();
    updateButtons();
}

void RulesDialog::updateButtons()
{
    const int row = selectedRow();
    const bool selected = row >= 0;
    m_edit->setEnabled(selected);
    m_remove->setEnabled(selected);
    m_up->setEnabled(selected && row > 0);
    m_down->setEnabled(selected && row + 1 < m_rules.size());
    const bool folderShown = !m_folder.isEmpty() && m_messages;
    m_preview->setEnabled(selected && folderShown);
    m_apply->setEnabled(selected && folderShown && m_applier);
    const QString where = folderShown ? i18nc("@info:tooltip %1 folder", "Uses the loaded messages of “%1”",
                                              folderLabel(m_folder))
                                      : i18nc("@info:tooltip", "Open a folder in the window to preview rules on it");
    m_preview->setToolTip(where);
    m_apply->setToolTip(where);
}

void RulesDialog::selectRule(const QString &id)
{
    for (int row = 0; row < m_rules.size(); ++row) {
        if (m_rules.at(row).id == id) {
            m_list->setCurrentRow(row);
            return;
        }
    }
}

void RulesDialog::addRule(const MailRules::Rule &rule)
{
    MailRules::Rule edited = rule;
    QStringList taken;
    for (const MailRules::Rule &existing : std::as_const(m_rules))
        taken << existing.id;
    if (edited.id.isEmpty() || taken.contains(edited.id))
        edited.id = MailRules::newId(taken);
    if (!m_editor || !m_editor(edited))
        return;
    m_rules << edited;
    fillList(static_cast<int>(m_rules.size()) - 1);
    // What the new rule matches is shown before it is saved
    previewSelected();
}

void RulesDialog::editSelected()
{
    const int row = selectedRow();
    if (row < 0 || !m_editor)
        return;
    MailRules::Rule edited = m_rules.at(row);
    if (!m_editor(edited))
        return;
    m_rules[row] = edited;
    fillList(row);
}

void RulesDialog::removeSelected()
{
    const int row = selectedRow();
    if (row < 0)
        return;
    m_rules.removeAt(row);
    fillList(row);
}

void RulesDialog::moveSelected(int by)
{
    const int row = selectedRow();
    const int target = row + by;
    if (row < 0 || target < 0 || target >= m_rules.size())
        return;
    m_rules.move(row, target);
    fillList(target);
}

QList<MailRules::Message> RulesDialog::matches(const MailRules::Rule &rule) const
{
    QList<MailRules::Message> result;
    if (!m_messages)
        return result;
    MailRules::Rule single = rule;
    single.enabled = true;
    // The preview looks at the folder shown, whichever folders the rule checks on arrival
    single.folders = {m_folder};
    single.accountId.clear();
    const QList<MailRules::Message> messages = m_messages();
    for (const MailRules::Message &message : messages) {
        if (MailRules::evaluate({single}, message, MailRules::Mode::Manual).matched())
            result << message;
    }
    return result;
}

void RulesDialog::previewSelected()
{
    const int row = selectedRow();
    if (row < 0 || !m_messages)
        return;
    MailRules::Rule &rule = m_rules[row];
    const QList<MailRules::Message> loaded = m_messages();
    const QList<MailRules::Message> matched = matches(rule);
    m_previewList->clear();
    const QLocale locale;
    for (const MailRules::Message &message : matched) {
        auto *item = new QTreeWidgetItem(m_previewList);
        item->setText(0, message.subject.isEmpty() ? i18nc("@item", "(no subject)") : message.subject);
        item->setText(1, message.senders.value(0));
        if (message.received.isValid())
            item->setText(2, locale.toString(message.received.toLocalTime(), QLocale::ShortFormat));
    }
    QStringList actions;
    for (const MailRules::Action &action : std::as_const(rule.actions))
        actions << MailRules::describe(action);
    m_previewSummary->setText(
        i18ncp("@info %2 folder, %3 count of loaded messages, %4 actions",
               "%1 of the %3 loaded messages in “%2” matches. The rule would %4.",
               "%1 of the %3 loaded messages in “%2” match. The rule would %4.", static_cast<int>(matched.size()),
               folderLabel(m_folder), static_cast<int>(loaded.size()),
               actions.join(i18nc("@info list separator", ", "))));
    // Seen once, the rule may move new mail by itself
    const bool wasReviewed = rule.isReviewed();
    rule.markReviewed();
    if (!wasReviewed) {
        QListWidgetItem *item = m_list->item(row);
        item->setIcon(QIcon());
        item->setToolTip(QString());
    }
}

void RulesDialog::applySelected()
{
    const int row = selectedRow();
    if (row < 0 || !m_applier || !m_messages)
        return;
    const MailRules::Rule rule = m_rules.at(row);
    const QList<MailRules::Message> matched = matches(rule);
    if (matched.isEmpty()) {
        m_previewSummary->setText(i18nc("@info %1 folder", "No loaded message in “%1” matches the rule.",
                                        folderLabel(m_folder)));
        return;
    }
    const QString question = i18ncp("@info %2 rule name, %3 folder", "Apply “%2” to %1 message in “%3” now?",
                                    "Apply “%2” to %1 messages in “%3” now?", static_cast<int>(matched.size()),
                                    rule.name, folderLabel(m_folder));
    if (!m_confirmation || !m_confirmation(question))
        return;
    MailRules::Rule single = rule;
    single.enabled = true;
    single.accountId.clear();
    single.folders = {m_folder};
    const int changed = m_applier({single}, m_accountId, m_folder, matched);
    m_rules[row].markReviewed();
    fillList(row);
    m_previewSummary->setText(
        i18ncp("@info", "The rule changed %1 message.", "The rule changed %1 messages.", changed));
}

}
