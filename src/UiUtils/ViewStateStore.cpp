/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "ViewStateStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace UiUtils {

namespace {

const QString folderKey = QStringLiteral("folder");
const QString sortKey = QStringLiteral("sort");
const QString orderKey = QStringLiteral("order");
const QString threadsKey = QStringLiteral("threads");
const QString groupByKey = QStringLiteral("groupBy");
const QString uidValidityKey = QStringLiteral("uidValidity");
const QString uidKey = QStringLiteral("uid");
const QString anchorKey = QStringLiteral("anchor");
const QString selectedKey = QStringLiteral("selected");
const QString descending = QStringLiteral("descending");
const QString ascending = QStringLiteral("ascending");

QJsonObject viewToObject(const MessageListView &view)
{
    QJsonObject object;
    object.insert(sortKey, view.sortColumn);
    object.insert(orderKey, view.sortOrder == Qt::DescendingOrder ? descending : ascending);
    object.insert(threadsKey, view.threading);
    if (!view.groupBy.isEmpty())
        object.insert(groupByKey, view.groupBy);
    return object;
}

std::optional<MessageListView> viewFromObject(const QJsonObject &object)
{
    if (!object.contains(sortKey) || !object.value(sortKey).isDouble())
        return std::nullopt;
    MessageListView view;
    view.sortColumn = qMax(-1, object.value(sortKey).toInt(-1));
    view.sortOrder = object.value(orderKey).toString() == descending ? Qt::DescendingOrder : Qt::AscendingOrder;
    view.threading = object.value(threadsKey).toBool();
    view.groupBy = object.value(groupByKey).toString();
    return view;
}

QJsonArray parseArray(const QString &json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    return document.isArray() ? document.array() : QJsonArray();
}

QString compact(const QJsonDocument &document)
{
    return QString::fromUtf8(document.toJson(QJsonDocument::Compact));
}

}

const QString ViewStateStore::folderViewsKey = QStringLiteral("gui/msgList.folderViews");
const QString ViewStateStore::defaultViewKey = QStringLiteral("gui/msgList.defaultView");
const QString ViewStateStore::legacyGroupByKey = QStringLiteral("gui/msgList.groupBy");

ViewStateStore::ViewStateStore(int limit)
    : m_folders(limit)
{
}

void ViewStateStore::load(const QSettings &settings)
{
    m_accountDefault = fromJson(settings.value(defaultViewKey).toString());
    m_legacyGroupBy = m_accountDefault ? QString() : settings.value(legacyGroupByKey).toString();
    m_folders.clear();
    const QJsonArray folders = parseArray(settings.value(folderViewsKey).toString());
    for (const QJsonValue &value : folders) {
        const QJsonObject object = value.toObject();
        if (const auto view = viewFromObject(object))
            m_folders.appendOlder(object.value(folderKey).toString(), *view);
    }
}

void ViewStateStore::save(QSettings &settings) const
{
    if (m_accountDefault)
        settings.setValue(defaultViewKey, toJson(*m_accountDefault));
    else
        settings.remove(defaultViewKey);
    // Kept for the account default, so that older versions show the same groups
    const QString groupBy = m_accountDefault ? m_accountDefault->groupBy : m_legacyGroupBy;
    if (groupBy.isEmpty())
        settings.remove(legacyGroupByKey);
    else
        settings.setValue(legacyGroupByKey, groupBy);

    QJsonArray folders;
    for (const auto &entry : m_folders.entries()) {
        QJsonObject object = viewToObject(entry.second);
        object.insert(folderKey, entry.first);
        folders.append(object);
    }
    if (folders.isEmpty())
        settings.remove(folderViewsKey);
    else
        settings.setValue(folderViewsKey, compact(QJsonDocument(folders)));
}

void ViewStateStore::setGlobalDefault(const MessageListView &view)
{
    m_global = view;
}

MessageListView ViewStateStore::globalDefault() const
{
    return m_global;
}

std::optional<MessageListView> ViewStateStore::accountDefault() const
{
    return m_accountDefault;
}

MessageListView ViewStateStore::effectiveAccountDefault() const
{
    if (m_accountDefault)
        return *m_accountDefault;
    MessageListView view = m_global;
    if (!m_legacyGroupBy.isEmpty()) {
        // The groups show a flat list
        view.groupBy = m_legacyGroupBy;
        view.threading = false;
    }
    return view;
}

void ViewStateStore::setAccountDefault(const MessageListView &view)
{
    m_accountDefault = view;
    m_legacyGroupBy.clear();
}

std::optional<MessageListView> ViewStateStore::folderView(const QString &mailbox) const
{
    if (mailbox.isEmpty())
        return std::nullopt;
    return m_folders.value(mailbox);
}

MessageListView ViewStateStore::effectiveView(const QString &mailbox) const
{
    if (const auto view = folderView(mailbox))
        return *view;
    return effectiveAccountDefault();
}

ViewStateStore::Scope ViewStateStore::scope(const QString &mailbox) const
{
    if (folderView(mailbox))
        return Scope::Folder;
    if (m_accountDefault || !m_legacyGroupBy.isEmpty())
        return Scope::Account;
    return Scope::Global;
}

void ViewStateStore::setFolderView(const QString &mailbox, const MessageListView &view)
{
    m_folders.insert(mailbox, view);
}

bool ViewStateStore::resetFolder(const QString &mailbox)
{
    return m_folders.remove(mailbox);
}

void ViewStateStore::applyToAllFolders(const MessageListView &view)
{
    setAccountDefault(view);
    m_folders.clear();
}

bool ViewStateStore::renameFolder(const QString &oldName, const QString &newName, const QString &separator)
{
    return m_folders.rename(oldName, newName, separator);
}

bool ViewStateStore::prune(const RecentFolders::StateLookup &state)
{
    return m_folders.prune(state);
}

QStringList ViewStateStore::folders() const
{
    return m_folders.folders();
}

int ViewStateStore::limit() const
{
    return m_folders.limit();
}

QString ViewStateStore::toJson(const MessageListView &view)
{
    return compact(QJsonDocument(viewToObject(view)));
}

std::optional<MessageListView> ViewStateStore::fromJson(const QString &json)
{
    if (json.isEmpty())
        return std::nullopt;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isObject())
        return std::nullopt;
    return viewFromObject(document.object());
}

FolderPositionStore::FolderPositionStore(int limit)
    : m_folders(limit)
{
}

std::optional<FolderPosition> FolderPositionStore::position(const QString &mailbox) const
{
    if (mailbox.isEmpty())
        return std::nullopt;
    return m_folders.value(mailbox);
}

void FolderPositionStore::setPosition(const QString &mailbox, const FolderPosition &position)
{
    if (!position.isValid()) {
        m_folders.remove(mailbox);
        return;
    }
    FolderPosition bounded = position;
    // A large selection is rather a passing state than a place to come back to
    if (bounded.selectedUids.size() > FolderPosition::MaxSelectedUids)
        bounded.selectedUids.clear();
    m_folders.insert(mailbox, bounded);
}

bool FolderPositionStore::remove(const QString &mailbox)
{
    return m_folders.remove(mailbox);
}

bool FolderPositionStore::renameFolder(const QString &oldName, const QString &newName, const QString &separator)
{
    return m_folders.rename(oldName, newName, separator);
}

bool FolderPositionStore::prune(const RecentFolders::StateLookup &state)
{
    return m_folders.prune(state);
}

QStringList FolderPositionStore::folders() const
{
    return m_folders.folders();
}

QString FolderPositionStore::toJson() const
{
    QJsonArray folders;
    for (const auto &entry : m_folders.entries()) {
        QJsonObject object;
        object.insert(folderKey, entry.first);
        object.insert(uidValidityKey, static_cast<qint64>(entry.second.uidValidity));
        if (entry.second.uid)
            object.insert(uidKey, static_cast<qint64>(entry.second.uid));
        if (entry.second.anchorUid)
            object.insert(anchorKey, static_cast<qint64>(entry.second.anchorUid));
        if (!entry.second.selectedUids.isEmpty()) {
            QJsonArray selected;
            for (const uint uid : entry.second.selectedUids)
                selected.append(static_cast<qint64>(uid));
            object.insert(selectedKey, selected);
        }
        folders.append(object);
    }
    return compact(QJsonDocument(folders));
}

void FolderPositionStore::loadJson(const QString &json)
{
    m_folders.clear();
    const QJsonArray folders = parseArray(json);
    for (const QJsonValue &value : folders) {
        const QJsonObject object = value.toObject();
        FolderPosition position;
        position.uidValidity = static_cast<uint>(object.value(uidValidityKey).toInteger());
        position.uid = static_cast<uint>(object.value(uidKey).toInteger());
        position.anchorUid = static_cast<uint>(object.value(anchorKey).toInteger());
        const QJsonArray selected = object.value(selectedKey).toArray();
        for (const QJsonValue &uid : selected) {
            if (const auto number = uid.toInteger(); number > 0 && position.selectedUids.size() < FolderPosition::MaxSelectedUids)
                position.selectedUids << static_cast<uint>(number);
        }
        if (position.isValid())
            m_folders.appendOlder(object.value(folderKey).toString(), position);
    }
}

}
