/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDialog>
#include <QPair>

#include "UiUtils/MailRules.h"

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QVBoxLayout;

namespace Gui {

/** @short Edit one mail rule: its name, scope, conditions and actions

The dialog cannot be accepted while UiUtils::MailRules::problems() reports something; warnings are shown without
stopping it. The rule keeps its ID and its preview fingerprint, so that only a change of what it does asks for a new
preview.
*/
class RuleEditDialog : public QDialog
{
    Q_OBJECT
public:
    /** @arg accounts are the IDs and the names of the accounts offered as the scope */
    explicit RuleEditDialog(const QList<QPair<QString, QString>> &accounts, QWidget *parent = nullptr);

    void setRule(const UiUtils::MailRules::Rule &rule);
    UiUtils::MailRules::Rule rule() const;

    /** Add a condition row, as the Add Condition button does */
    void addCondition(const UiUtils::MailRules::Condition &condition);
    /** Add an action row, as the Add Action button does */
    void addAction(const UiUtils::MailRules::Action &action);
    /** The text of the problems and warnings shown under the rule */
    QString feedback() const;
    /** Whether the rule can be saved now */
    bool canAccept() const;

private:
    void refresh();
    void clearRows(QVBoxLayout *layout);

    UiUtils::MailRules::Rule m_original;
    QLineEdit *m_name;
    QCheckBox *m_enabled;
    QComboBox *m_account;
    QPlainTextEdit *m_folders;
    QComboBox *m_match;
    QVBoxLayout *m_conditions;
    QVBoxLayout *m_actions;
    QCheckBox *m_stop;
    QLabel *m_summary;
    QLabel *m_feedback;
    QDialogButtonBox *m_buttons;
};

}
