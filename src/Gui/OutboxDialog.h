/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDialog>
#include <functional>

class QLabel;
class QPushButton;
class QTreeWidget;

namespace Composer {
class Outbox;
}

namespace Gui {

/** @short The messages waiting to be sent, and what to do with those which wait
for a decision

Each message shows its subject, its account, its state and when it is tried next
or why it failed. A queued message can be sent now, an interrupted or refused
one sent again, and any message which the server does not have yet taken back
into the drafts. A message which the server already accepted only waits for its
copy in Sent; giving that up forgets the message.
*/
class OutboxDialog : public QDialog
{
    Q_OBJECT
public:
    using AccountName = std::function<QString(const QString &accountId)>;
    /** @short Ask the user; receives the title and the question, and answers whether to go ahead */
    using Confirmation = std::function<bool(const QString &title, const QString &question)>;

    OutboxDialog(Composer::Outbox *outbox, const AccountName &accountName, QWidget *parent = nullptr);

    QTreeWidget *tree() const;
    /** Replace the confirmation dialog, for example in tests */
    void setConfirmation(const Confirmation &confirm);

    void sendNow();
    void sendAgain();
    void moveToDrafts();
    void forget();

signals:
    /** @short The selected message is now the draft or saved message at @p path */
    void draftMoved(const QString &path);

private:
    void rebuild();
    void updateButtons();
    QString selectedId() const;
    bool confirm(const QString &title, const QString &question);

    Composer::Outbox *m_outbox;
    AccountName m_accountName;
    Confirmation m_confirm;
    QTreeWidget *m_tree;
    QLabel *m_problem;
    QPushButton *m_sendNow;
    QPushButton *m_sendAgain;
    QPushButton *m_moveToDrafts;
    QPushButton *m_forget;
};

} // namespace Gui
