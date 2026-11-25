/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDialog>
#include <QStringList>

class QAbstractItemModel;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QListView;
class QModelIndex;
class QSortFilterProxyModel;
class QStandardItemModel;

namespace Gui {

/** @short Searchable list for choosing one mailbox of an account

The dialog offers the selectable mailboxes that a mailbox tree model (one using the
Imap::Mailbox roles, such as PrettyMailboxModel) has already listed, in the model's
order and with the model's icons. Typing filters the list, the arrow keys move
through it, and Enter accepts the current row.
*/
class MailboxPickerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MailboxPickerDialog(QWidget *parent = nullptr);

    void setPrompt(const QString &prompt);

    /** @short Fill the list from @arg mailboxes, skipping the mailbox names in @arg excluded

    Only branches whose children are already known are walked, so filling the list never
    makes an IMAP model contact the server.
    */
    void setMailboxes(QAbstractItemModel *mailboxes, const QStringList &excluded = QStringList());

    /** @short Make the given mailbox the current row if it is offered and matches the filter */
    void setCurrentMailbox(const QString &mailbox);

    /** @short Full IMAP name of the current row, or an empty string */
    QString selectedMailbox() const;

    /** @short Names of the offered mailboxes in their display order (regardless of the filter) */
    QStringList offeredMailboxes() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void addMailboxes(QAbstractItemModel *model, const QModelIndex &parent, const QString &parentPath,
                      const QStringList &excluded);
    void ensureCurrentRow();
    void updateAcceptButton();

    QLabel *m_prompt;
    QLineEdit *m_filter;
    QListView *m_list;
    QStandardItemModel *m_items;
    QSortFilterProxyModel *m_filtered;
    QDialogButtonBox *m_buttons;
};

}
