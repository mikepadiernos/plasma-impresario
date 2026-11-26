/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "SearchFoldersDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <KLocalizedString>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/PrettyMailboxModel.h"
#include "UiUtils/IconLoader.h"
#include "UiUtils/SearchScope.h"

namespace Gui {

namespace {
constexpr int RoleFolderKey = Qt::UserRole + 1;
}

SearchFoldersDialog::SearchFoldersDialog(QWidget *parent)
    : QDialog(parent)
    , m_tree(new QTreeWidget(this))
{
    setWindowTitle(i18nc("@title:window", "Search In Folders"));
    auto *prompt = new QLabel(i18nc("@label", "Choose the folders to search, from any account:"), this);
    prompt->setWordWrap(true);
    m_tree->setObjectName(QStringLiteral("searchFoldersTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setUniformRowHeights(true);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    m_ok->setText(i18nc("@action:button", "Search"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_tree, &QTreeWidget::itemChanged, this, &SearchFoldersDialog::updateAcceptButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(prompt);
    layout->addWidget(m_tree, 1);
    layout->addWidget(buttons);
    resize(420, 480);
    updateAcceptButton();
}

void SearchFoldersDialog::addAccount(const QString &accountId, const QString &accountName, QAbstractItemModel *mailboxes)
{
    {
        const QSignalBlocker blocker(m_tree);
        auto *account = new QTreeWidgetItem(m_tree, {accountName});
        account->setIcon(0, UiUtils::loadIcon(QStringLiteral("folder-mail")));
        if (mailboxes)
            addFolders(account, accountId, mailboxes, QModelIndex(), QString());
        if (account->childCount() > 0) {
            // Checking the account checks all of its folders, and it shows whether some of them are checked
            account->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
            account->setCheckState(0, Qt::Unchecked);
        } else {
            account->setFlags(Qt::NoItemFlags);
            account->setToolTip(0, i18nc("@info:tooltip", "The folders of this account are not known yet"));
        }
        account->setExpanded(true);
    }
    updateAcceptButton();
}

void SearchFoldersDialog::addFolders(QTreeWidgetItem *account, const QString &accountId, QAbstractItemModel *model,
                                     const QModelIndex &parent, const QString &parentPath)
{
    using namespace Imap::Mailbox;
    const int rows = model->rowCount(parent);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        const QString name = index.data(RoleMailboxName).toString();
        QString shortName = index.data(RoleShortMailboxName).toString();
        if (shortName.isEmpty())
            shortName = name;
        const QString path = parentPath.isEmpty() ? shortName
                                                  : i18nc("@item folder path, %1 parent, %2 child", "%1 / %2", parentPath, shortName);
        if (!name.isEmpty() && index.data(RoleMailboxIsSelectable).toBool()) {
            QIcon icon = PrettyMailboxModel::specialFolderIcon(static_cast<SpecialUse>(index.data(RoleMailboxSpecialUse).toInt()));
            if (icon.isNull())
                icon = UiUtils::loadIcon(QStringLiteral("folder"));
            auto *item = new QTreeWidgetItem(account, {path});
            item->setIcon(0, icon);
            item->setToolTip(0, name);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            item->setCheckState(0, Qt::Unchecked);
            item->setData(0, RoleFolderKey, UiUtils::SearchScope::folderKey({accountId, name}));
        }
        // Do not descend into branches that have not been listed yet; that would send LIST commands
        const QVariant fetched = index.data(RoleIsFetched);
        if ((!fetched.isValid() || fetched.toBool()) && model->hasChildren(index))
            addFolders(account, accountId, model, index, path);
    }
}

void SearchFoldersDialog::setSelectedFolders(const QStringList &keys)
{
    QStringList unlisted = keys;
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
        const QString key = (*it)->data(0, RoleFolderKey).toString();
        if (key.isEmpty())
            continue;
        // Not blocked, so that the accounts follow their folders
        (*it)->setCheckState(0, keys.contains(key) ? Qt::Checked : Qt::Unchecked);
        unlisted.removeAll(key);
    }
    m_unlisted = unlisted;
    updateAcceptButton();
}

QStringList SearchFoldersDialog::selectedFolders() const
{
    QStringList keys;
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
        const QString key = (*it)->data(0, RoleFolderKey).toString();
        if (!key.isEmpty() && (*it)->checkState(0) == Qt::Checked)
            keys << key;
    }
    for (const QString &key : m_unlisted) {
        if (!keys.contains(key))
            keys << key;
    }
    return keys;
}

QTreeWidget *SearchFoldersDialog::tree() const
{
    return m_tree;
}

void SearchFoldersDialog::updateAcceptButton()
{
    if (m_ok)
        m_ok->setEnabled(!selectedFolders().isEmpty());
}

}
