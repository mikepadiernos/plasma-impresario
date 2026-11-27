/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <functional>

#include <QDialog>
#include <QPair>

#include "UiUtils/MailRules.h"

class QLabel;
class QListWidget;
class QPushButton;
class QTreeWidget;

namespace Gui {

/** @short The mail rules in their order, with editing, a preview on the shown folder and running them by hand

A rule's preview lists which of the loaded messages of the shown folder it matches and what it would do to them; it
records that the rule was previewed, which a rule that moves messages needs before it runs on new mail by itself.
Applying a rule to the folder asks first and then acts on those messages. The rules are saved when the dialog is
accepted; applying acts at once.
*/
class RulesDialog : public QDialog
{
    Q_OBJECT
public:
    using Accounts = QList<QPair<QString, QString>>;
    using MessageSource = std::function<QList<UiUtils::MailRules::Message>()>;
    /** Carry out rules on messages of a folder; returns how many messages changed */
    using Applier = std::function<int(const QList<UiUtils::MailRules::Rule> &, const QString &accountId,
                                      const QString &folder, const QList<UiUtils::MailRules::Message> &)>;
    /** Ask the user a question; true for yes. Replaced in tests */
    using Confirmation = std::function<bool(const QString &question)>;
    /** Edit a rule in place; true when it was changed. Replaced in tests */
    using Editor = std::function<bool(UiUtils::MailRules::Rule &rule)>;

    explicit RulesDialog(const Accounts &accounts, QWidget *parent = nullptr);

    void setRules(const QList<UiUtils::MailRules::Rule> &rules);
    QList<UiUtils::MailRules::Rule> rules() const;

    /** @short The folder shown in the window, whose loaded messages are previewed; an empty folder turns preview off */
    void setShownFolder(const QString &accountId, const QString &folder, const MessageSource &messages);
    void setApplier(const Applier &applier);
    void setConfirmation(const Confirmation &confirmation);
    void setEditor(const Editor &editor);

    /** Select the rule @arg id, as when the activity log asks to review it */
    void selectRule(const QString &id);
    /** @short Offer @arg rule as a new rule in the editor, as made from a search; it is added when the editor accepts */
    void addRule(const UiUtils::MailRules::Rule &rule);

    /** Preview the selected rule on the shown folder */
    void previewSelected();
    /** Apply the selected rule to the shown folder after asking */
    void applySelected();

    QListWidget *list() const;
    QTreeWidget *previewList() const;
    QString previewSummary() const;

private:
    void fillList(int current);
    void updateButtons();
    void editSelected();
    void removeSelected();
    void moveSelected(int by);
    int selectedRow() const;
    /** The loaded messages which @arg rule matches, as it would act on them now */
    QList<UiUtils::MailRules::Message> matches(const UiUtils::MailRules::Rule &rule) const;

    Accounts m_accounts;
    QList<UiUtils::MailRules::Rule> m_rules;
    QString m_accountId;
    QString m_folder;
    MessageSource m_messages;
    Applier m_applier;
    Confirmation m_confirmation;
    Editor m_editor;
    QListWidget *m_list;
    QPushButton *m_add;
    QPushButton *m_edit;
    QPushButton *m_remove;
    QPushButton *m_up;
    QPushButton *m_down;
    QPushButton *m_preview;
    QPushButton *m_apply;
    QLabel *m_previewSummary;
    QTreeWidget *m_previewList;
};

}
