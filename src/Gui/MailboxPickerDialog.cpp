/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "MailboxPickerDialog.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <KLocalizedString>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/PrettyMailboxModel.h"
#include "UiUtils/IconLoader.h"

namespace Gui {

namespace {
constexpr int RoleMailbox = Qt::UserRole + 1;
}

MailboxPickerDialog::MailboxPickerDialog(QWidget *parent)
    : QDialog(parent)
    , m_prompt(new QLabel(this))
    , m_filter(new QLineEdit(this))
    , m_list(new QListView(this))
    , m_items(new QStandardItemModel(this))
    , m_filtered(new QSortFilterProxyModel(this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this))
{
    setWindowTitle(i18nc("@title:window", "Choose Folder"));

    m_prompt->setText(i18nc("@label:textbox", "Folder:"));
    m_prompt->setBuddy(m_filter);
    m_filter->setPlaceholderText(i18nc("@info:placeholder", "Type to filter folders"));
    m_filter->setClearButtonEnabled(true);
    m_filter->installEventFilter(this);

    m_filtered->setSourceModel(m_items);
    m_filtered->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_list->setModel(m_filtered);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setUniformItemSizes(true);
    m_list->setAccessibleName(i18nc("@label", "Folders"));

    // Enter is handled by the default (OK) button, both in the filter and in the list
    connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_filtered->setFilterFixedString(text);
        if (m_filtered->rowCount() > 0)
            m_list->setCurrentIndex(m_filtered->index(0, 0));
        updateAcceptButton();
    });
    connect(m_list, &QAbstractItemView::doubleClicked, this, [this](const QModelIndex &index) {
        if (index.isValid())
            accept();
    });
    connect(m_list->selectionModel(), &QItemSelectionModel::currentChanged, this, &MailboxPickerDialog::updateAcceptButton);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_prompt);
    layout->addWidget(m_filter);
    layout->addWidget(m_list);
    layout->addWidget(m_buttons);

    m_filter->setFocus();
    updateAcceptButton();
}

void MailboxPickerDialog::setPrompt(const QString &prompt)
{
    m_prompt->setText(prompt);
}

void MailboxPickerDialog::setMailboxes(QAbstractItemModel *mailboxes, const QStringList &excluded)
{
    m_items->clear();
    if (mailboxes)
        addMailboxes(mailboxes, QModelIndex(), QString(), excluded);
    ensureCurrentRow();
}

void MailboxPickerDialog::addMailboxes(QAbstractItemModel *model, const QModelIndex &parent, const QString &parentPath,
                                       const QStringList &excluded)
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
        if (!name.isEmpty() && index.data(RoleMailboxIsSelectable).toBool() && !excluded.contains(name)) {
            QIcon icon = PrettyMailboxModel::specialFolderIcon(static_cast<SpecialUse>(index.data(RoleMailboxSpecialUse).toInt()));
            if (icon.isNull())
                icon = UiUtils::loadIcon(QStringLiteral("folder"));
            auto *item = new QStandardItem(icon, path);
            item->setData(name, RoleMailbox);
            item->setToolTip(name);
            item->setEditable(false);
            m_items->appendRow(item);
        }
        // Do not descend into branches that have not been listed yet; that would send LIST commands
        const QVariant fetched = index.data(RoleIsFetched);
        if ((!fetched.isValid() || fetched.toBool()) && model->hasChildren(index))
            addMailboxes(model, index, path, excluded);
    }
}

void MailboxPickerDialog::setCurrentMailbox(const QString &mailbox)
{
    for (int row = 0; row < m_filtered->rowCount(); ++row) {
        const QModelIndex index = m_filtered->index(row, 0);
        if (index.data(RoleMailbox).toString() == mailbox) {
            m_list->setCurrentIndex(index);
            m_list->scrollTo(index);
            return;
        }
    }
}

QString MailboxPickerDialog::selectedMailbox() const
{
    return m_list->currentIndex().data(RoleMailbox).toString();
}

QStringList MailboxPickerDialog::offeredMailboxes() const
{
    QStringList res;
    for (int row = 0; row < m_items->rowCount(); ++row)
        res << m_items->item(row)->data(RoleMailbox).toString();
    return res;
}

bool MailboxPickerDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_filter && event->type() == QEvent::KeyPress) {
        switch (static_cast<QKeyEvent *>(event)->key()) {
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            // Let the user move through the matches without leaving the filter
            QCoreApplication::sendEvent(m_list, event);
            return true;
        default:
            break;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void MailboxPickerDialog::ensureCurrentRow()
{
    if (!m_list->currentIndex().isValid() && m_filtered->rowCount() > 0)
        m_list->setCurrentIndex(m_filtered->index(0, 0));
    updateAcceptButton();
}

void MailboxPickerDialog::updateAcceptButton()
{
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(m_list->currentIndex().isValid());
}

}
