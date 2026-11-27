/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/ActivityLog.h"

#include <algorithm>

#include <QCoreApplication>

namespace UiUtils {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("UiUtils::ActivityLog", text);
}

}

ActivityLog::ActivityLog(QObject *parent)
    : QAbstractListModel(parent)
{
}

ActivityLog::~ActivityLog() = default;

void ActivityLog::record(Entry entry)
{
    const int failedBefore = failedCount();
    const int runningBefore = runningCount();
    if (!entry.time.isValid())
        entry.time = QDateTime::currentDateTime();
    if (entry.action != Action::Retry)
        forgetRetry(entry.key);
    const int existing = entry.key.isEmpty() ? -1 : find(entry.key);
    if (existing == 0) {
        m_entries[0] = entry;
        emit dataChanged(index(0), index(0));
    } else {
        if (existing > 0) {
            beginRemoveRows({}, existing, existing);
            m_entries.removeAt(existing);
            endRemoveRows();
        }
        beginInsertRows({}, 0, 0);
        m_entries.prepend(entry);
        endInsertRows();
        trim();
    }
    notifyCounts(failedBefore, runningBefore);
}

bool ActivityLog::finish(const QString &key, State state, const QString &error)
{
    const int row = find(key);
    if (row < 0)
        return false;
    Entry entry = m_entries.at(row);
    entry.state = state;
    entry.error = error;
    entry.time = QDateTime();
    if (state == State::Done)
        entry.progress = entry.progress >= 0 ? 100 : -1;
    record(entry);
    return true;
}

void ActivityLog::trim()
{
    // The oldest entries go first, failures only when nothing else is left to drop
    while (m_entries.size() > capacity) {
        int drop = -1;
        for (int row = static_cast<int>(m_entries.size()) - 1; row >= 0; --row) {
            if (m_entries.at(row).state != State::Failed) {
                drop = row;
                break;
            }
        }
        if (drop < 0)
            drop = static_cast<int>(m_entries.size()) - 1;
        beginRemoveRows({}, drop, drop);
        forgetRetry(m_entries.at(drop).key);
        m_entries.removeAt(drop);
        endRemoveRows();
    }
}

void ActivityLog::clearFinished()
{
    const int failedBefore = failedCount();
    const int runningBefore = runningCount();
    for (int row = static_cast<int>(m_entries.size()) - 1; row >= 0; --row) {
        if (m_entries.at(row).state != State::Done)
            continue;
        beginRemoveRows({}, row, row);
        m_entries.removeAt(row);
        endRemoveRows();
    }
    notifyCounts(failedBefore, runningBefore);
}

void ActivityLog::removeAccount(const QString &accountId)
{
    const int failedBefore = failedCount();
    const int runningBefore = runningCount();
    for (int row = static_cast<int>(m_entries.size()) - 1; row >= 0; --row) {
        if (m_entries.at(row).accountId != accountId)
            continue;
        beginRemoveRows({}, row, row);
        forgetRetry(m_entries.at(row).key);
        m_entries.removeAt(row);
        endRemoveRows();
    }
    notifyCounts(failedBefore, runningBefore);
}

void ActivityLog::clear()
{
    const int failedBefore = failedCount();
    const int runningBefore = runningCount();
    beginResetModel();
    m_entries.clear();
    m_retries.clear();
    endResetModel();
    notifyCounts(failedBefore, runningBefore);
}

bool ActivityLog::setRetry(const QString &key, const std::function<void()> &handler)
{
    const int row = find(key);
    if (row < 0 || !handler)
        return false;
    m_retries.insert(key, handler);
    m_entries[row].action = Action::Retry;
    emit dataChanged(index(row), index(row));
    return true;
}

bool ActivityLog::canRetry(const QString &key) const
{
    return !key.isEmpty() && m_retries.contains(key);
}

bool ActivityLog::retry(const QString &key)
{
    const int row = find(key);
    if (row < 0 || !m_retries.contains(key))
        return false;
    const std::function<void()> handler = m_retries.take(key);
    Entry entry = m_entries.at(row);
    entry.state = State::Running;
    entry.error.clear();
    entry.action = Action::None;
    entry.detail = tr("Trying again");
    entry.time = QDateTime();
    record(entry);
    handler();
    return true;
}

int ActivityLog::retryAll()
{
    QStringList keys;
    for (const Entry &entry : std::as_const(m_entries)) {
        if (entry.state == State::Failed && m_retries.contains(entry.key))
            keys << entry.key;
    }
    int retried = 0;
    for (const QString &key : std::as_const(keys))
        retried += retry(key) ? 1 : 0;
    return retried;
}

int ActivityLog::retryableCount() const
{
    return static_cast<int>(std::count_if(m_entries.cbegin(), m_entries.cend(),
        [this](const Entry &entry) { return entry.state == State::Failed && m_retries.contains(entry.key); }));
}

void ActivityLog::forgetRetry(const QString &key)
{
    if (!key.isEmpty())
        m_retries.remove(key);
}

void ActivityLog::notifyCounts(int failedBefore, int runningBefore)
{
    if (failedBefore != failedCount() || runningBefore != runningCount())
        emit countsChanged();
}

int ActivityLog::count() const
{
    return static_cast<int>(m_entries.size());
}

ActivityLog::Entry ActivityLog::entry(int row) const
{
    return m_entries.value(row);
}

int ActivityLog::find(const QString &key) const
{
    if (key.isEmpty())
        return -1;
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).key == key)
            return row;
    }
    return -1;
}

int ActivityLog::failedCount() const
{
    return static_cast<int>(std::count_if(m_entries.cbegin(), m_entries.cend(),
                                          [](const Entry &entry) { return entry.state == State::Failed; }));
}

int ActivityLog::runningCount() const
{
    return static_cast<int>(std::count_if(m_entries.cbegin(), m_entries.cend(),
                                          [](const Entry &entry) { return entry.state == State::Running; }));
}

QString ActivityLog::stateName(State state)
{
    switch (state) {
    case State::Running:
        return tr("In progress");
    case State::Waiting:
        return tr("Waiting");
    case State::Done:
        return tr("Done");
    case State::Failed:
        return tr("Failed");
    }
    return {};
}

QString ActivityLog::actionName(Action action)
{
    switch (action) {
    case Action::None:
        return {};
    case Action::Retry:
        return tr("Retry");
    case Action::SignIn:
        return tr("Sign In");
    case Action::ReviewRule:
        return tr("Review Rule");
    case Action::OpenSettings:
        return tr("Open Settings");
    case Action::OpenAccount:
        return tr("Open Account");
    }
    return {};
}

int ActivityLog::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant ActivityLog::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return entry.title;
    case Qt::ToolTipRole: {
        QStringList lines{entry.title};
        if (!entry.detail.isEmpty())
            lines << entry.detail;
        if (!entry.error.isEmpty())
            lines << entry.error;
        return lines.join(QLatin1Char('\n'));
    }
    case KindRole:
        return static_cast<int>(entry.kind);
    case StateRole:
        return static_cast<int>(entry.state);
    case AccountRole:
        return entry.accountId;
    case DetailRole:
        return entry.detail;
    case ErrorRole:
        return entry.error;
    case ActionRole:
        return static_cast<int>(entry.action);
    case TargetRole:
        return entry.target;
    case TimeRole:
        return entry.time;
    case ProgressRole:
        return entry.progress;
    case KeyRole:
        return entry.key;
    }
    return {};
}

QHash<int, QByteArray> ActivityLog::roleNames() const
{
    QHash<int, QByteArray> names = QAbstractListModel::roleNames();
    names.insert(KindRole, "kind");
    names.insert(StateRole, "state");
    names.insert(AccountRole, "account");
    names.insert(DetailRole, "detail");
    names.insert(ErrorRole, "error");
    names.insert(ActionRole, "action");
    names.insert(TargetRole, "target");
    names.insert(TimeRole, "time");
    names.insert(ProgressRole, "progress");
    names.insert(KeyRole, "key");
    return names;
}

}
