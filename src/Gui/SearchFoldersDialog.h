/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDialog>
#include <QStringList>

class QAbstractItemModel;
class QModelIndex;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace Gui {

/** @short Checkable tree for choosing the folders which the quick search looks at

Each account is a top-level item with its folders below it, shown by their path, as far as its mailbox tree model (one
using the Imap::Mailbox roles, such as PrettyMailboxModel) has listed them already. Folders are identified by the keys
of UiUtils::SearchScope::folderKey(), so that folders of several accounts can be chosen together.
*/
class SearchFoldersDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SearchFoldersDialog(QWidget *parent = nullptr);

    /** @short Offer the selectable folders of @arg mailboxes under the account @arg accountName

    Only branches whose children are already known are walked, so filling the tree never makes an IMAP model contact
    the server. Without a model, or before its folders are listed, the account is shown without folders.
    */
    void addAccount(const QString &accountId, const QString &accountName, QAbstractItemModel *mailboxes);

    /** @short Check the folders of @arg keys; keys of folders which are not offered are kept as they are */
    void setSelectedFolders(const QStringList &keys);
    /** @short Keys of the checked folders, in the order of the tree, followed by kept keys of unlisted folders */
    QStringList selectedFolders() const;

    QTreeWidget *tree() const;

private:
    void addFolders(QTreeWidgetItem *account, const QString &accountId, QAbstractItemModel *model,
                    const QModelIndex &parent, const QString &parentPath);
    void updateAcceptButton();

    QTreeWidget *m_tree;
    QPushButton *m_ok = nullptr;
    /** Chosen folders which are not in the tree, such as those of accounts which are not connected */
    QStringList m_unlisted;
};

}
