/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "ActivityDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <KLocalizedString>

#include "StateColors.h"
#include "UiUtils/IconLoader.h"

namespace Gui {

using UiUtils::ActivityLog;

namespace {

QIcon stateIcon(ActivityLog::State state)
{
    switch (state) {
    case ActivityLog::State::Running:
        return UiUtils::SymbolicIcon::fromTheme(QStringLiteral("view-refresh"));
    case ActivityLog::State::Waiting:
        return UiUtils::SymbolicIcon::fromTheme(QStringLiteral("dialog-information"));
    case ActivityLog::State::Done:
        return UiUtils::SymbolicIcon::fromTheme(QStringLiteral("dialog-ok"));
    case ActivityLog::State::Failed:
        return UiUtils::SymbolicIcon::fromTheme(QStringLiteral("dialog-error"));
    }
    return {};
}

QString when(const QDateTime &time)
{
    if (!time.isValid())
        return {};
    const QDateTime local = time.toLocalTime();
    if (local.date() == QDate::currentDate())
        return QLocale().toString(local.time(), QLocale::ShortFormat);
    return QLocale().toString(local, QLocale::ShortFormat);
}

}

ActivityDialog::ActivityDialog(ActivityLog *log, QWidget *parent)
    : QDialog(parent)
    , m_log(log)
    , m_tree(new QTreeWidget(this))
    , m_action(new QPushButton(this))
    , m_clear(new QPushButton(UiUtils::loadIcon(QStringLiteral("edit-clear-history")),
                              i18nc("@action:button", "Clear Finished"), this))
{
    setObjectName(QStringLiteral("activityDialog"));
    setWindowTitle(i18nc("@title:window", "Activity"));
    m_tree->setObjectName(QStringLiteral("activityTree"));
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->setWordWrap(true);
    m_tree->setHeaderLabels({i18nc("@title:column", "State"), i18nc("@title:column", "Activity"),
                             i18nc("@title:column", "Details"), i18nc("@title:column", "Time")});
    m_tree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tree->setAccessibleName(i18nc("@label", "Background activity, newest first"));
    m_action->setObjectName(QStringLiteral("activityAction"));
    m_clear->setObjectName(QStringLiteral("activityClear"));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_tree, 1);
    auto *row = new QHBoxLayout;
    row->addWidget(m_action);
    row->addStretch();
    row->addWidget(m_clear);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    row->addWidget(box);
    layout->addLayout(row);
    resize(720, 420);

    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_clear, &QPushButton::clicked, m_log, &ActivityLog::clearFinished);
    connect(m_action, &QPushButton::clicked, this, &ActivityDialog::triggerSelected);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, &ActivityDialog::updateButtons);
    connect(m_tree, &QTreeWidget::itemActivated, this, &ActivityDialog::triggerSelected);
    connect(m_log, &QAbstractItemModel::modelReset, this, &ActivityDialog::rebuild);
    connect(m_log, &QAbstractItemModel::layoutChanged, this, &ActivityDialog::rebuild);
    connect(m_log, &QAbstractItemModel::rowsInserted, this, &ActivityDialog::rebuild);
    connect(m_log, &QAbstractItemModel::rowsRemoved, this, &ActivityDialog::rebuild);
    connect(m_log, &QAbstractItemModel::rowsMoved, this, &ActivityDialog::rebuild);
    connect(m_log, &QAbstractItemModel::dataChanged, this, &ActivityDialog::rebuild);
    rebuild();
}

QTreeWidget *ActivityDialog::tree() const
{
    return m_tree;
}

void ActivityDialog::rebuild()
{
    const QString selectedKey = m_tree->currentItem() ? m_tree->currentItem()->data(0, Qt::UserRole).toString()
                                                      : QString();
    const int selectedRow = m_tree->currentItem() ? m_tree->indexOfTopLevelItem(m_tree->currentItem()) : -1;
    QSignalBlocker blocker(m_tree);
    m_tree->clear();
    QTreeWidgetItem *current = nullptr;
    for (int row = 0; row < m_log->count(); ++row) {
        const ActivityLog::Entry entry = m_log->entry(row);
        auto *item = new QTreeWidgetItem(m_tree);
        item->setIcon(0, stateIcon(entry.state));
        QString state = ActivityLog::stateName(entry.state);
        if (entry.state == ActivityLog::State::Running && entry.progress >= 0)
            state = i18nc("@info %1 state, %2 percent", "%1 (%2%)", state, entry.progress);
        item->setText(0, state);
        item->setText(1, entry.title);
        item->setText(2, entry.error.isEmpty() ? entry.detail : entry.error);
        item->setText(3, when(entry.time));
        item->setToolTip(1, m_log->data(m_log->index(row), Qt::ToolTipRole).toString());
        item->setToolTip(2, item->text(2));
        item->setData(0, Qt::UserRole, entry.key);
        item->setData(0, Qt::UserRole + 1, row);
        if (entry.state == ActivityLog::State::Failed) {
            const QColor negative = StateColors::text(StateColors::State::Negative);
            item->setForeground(0, negative);
            item->setForeground(2, negative);
        }
        if (!selectedKey.isEmpty() && entry.key == selectedKey)
            current = item;
    }
    if (!current && selectedRow >= 0 && m_tree->topLevelItemCount() > 0)
        current = m_tree->topLevelItem(std::min(selectedRow, m_tree->topLevelItemCount() - 1));
    if (current)
        m_tree->setCurrentItem(current);
    blocker.unblock();
    m_clear->setEnabled(m_log->count() > m_log->runningCount() + m_log->failedCount());
    updateButtons();
}

void ActivityDialog::updateButtons()
{
    QTreeWidgetItem *item = m_tree->currentItem();
    ActivityLog::Action action = ActivityLog::Action::None;
    if (item) {
        const int row = item->data(0, Qt::UserRole + 1).toInt();
        if (row >= 0 && row < m_log->count())
            action = m_log->entry(row).action;
    }
    const QString name = ActivityLog::actionName(action);
    m_action->setText(name.isEmpty() ? i18nc("@action:button", "No Action Needed") : name);
    m_action->setEnabled(action != ActivityLog::Action::None);
}

void ActivityDialog::triggerSelected()
{
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item)
        return;
    const int row = item->data(0, Qt::UserRole + 1).toInt();
    if (row < 0 || row >= m_log->count())
        return;
    const ActivityLog::Entry entry = m_log->entry(row);
    if (entry.action == ActivityLog::Action::None)
        return;
    emit actionRequested(entry);
}

ActivityButton::ActivityButton(ActivityLog *log, QWidget *parent)
    : QToolButton(parent)
    , m_log(log)
{
    setObjectName(QStringLiteral("activityButton"));
    setAutoRaise(true);
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(m_log, &ActivityLog::countsChanged, this, &ActivityButton::refresh);
    refresh();
}

void ActivityButton::refresh()
{
    const int failed = m_log->failedCount();
    const int running = m_log->runningCount();
    QStringList parts;
    if (running > 0)
        parts << i18ncp("@info:status", "%1 running", "%1 running", running);
    if (failed > 0)
        parts << i18ncp("@info:status", "%1 failed", "%1 failed", failed);
    setText(parts.join(i18nc("@info list separator", ", ")));
    setIcon(UiUtils::SymbolicIcon::fromTheme(failed > 0 ? QStringLiteral("dialog-error")
                                                         : running > 0 ? QStringLiteral("view-refresh")
                                                                       : QStringLiteral("view-history")));
    setToolTip(parts.isEmpty() ? i18nc("@info:tooltip", "Show the background activity")
                               : i18nc("@info:tooltip %1 counts", "Background activity: %1", text()));
    setAccessibleName(toolTip());
}

}
