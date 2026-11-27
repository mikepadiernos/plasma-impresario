/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDialog>
#include <QPair>

#include "UiUtils/NewMail.h"

class QCheckBox;
class QComboBox;
class QPlainTextEdit;
class QSpinBox;

namespace Gui {

/** @short How new mail is announced: per account, per folder, and while the window is in use

Each account is announced one message at a time in a single notification which is updated as mail arrives, in a
summary at most every few minutes, or not at all. Its INBOX is announced unless turned off; other folders are listed
by name. The dialog edits a copy of the settings; the window saves them when it is accepted.
*/
class NotificationsDialog : public QDialog
{
    Q_OBJECT
public:
    using Accounts = QList<QPair<QString, QString>>;

    NotificationsDialog(const Accounts &accounts, const UiUtils::NewMail::Settings &settings,
                        QWidget *parent = nullptr);

    /** The settings as edited so far */
    UiUtils::NewMail::Settings settings() const;
    /** Show the settings of @arg accountId */
    void selectAccount(const QString &accountId);

private:
    void storeAccount() const;
    void loadAccount();
    void updateEnabled();

    Accounts m_accounts;
    mutable UiUtils::NewMail::Settings m_settings;
    QString m_shownAccount;
    QComboBox *m_account;
    QComboBox *m_mode;
    QCheckBox *m_inbox;
    QPlainTextEdit *m_folders;
    QSpinBox *m_summaryMinutes;
    QCheckBox *m_quietWhileActive;
    QCheckBox *m_showContent;
};

}
