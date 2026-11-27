/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/Workspaces.h"

#include <QCoreApplication>
#include <algorithm>

namespace UiUtils {

namespace Workspaces {

const Workspace *forActivity(const QList<Workspace> &workspaces, const QString &activityId)
{
    if (activityId.isEmpty())
        return nullptr;
    for (const Workspace &workspace : workspaces) {
        if (workspace.activityId == activityId)
            return &workspace;
    }
    return nullptr;
}

QString accountForActivity(const QList<Workspace> &workspaces, const QString &activityId,
                           const QString &currentAccount, const QStringList &accountIds)
{
    const Workspace *workspace = forActivity(workspaces, activityId);
    if (!workspace || workspace->accounts.contains(currentAccount))
        return QString();
    for (const QString &account : workspace->accounts) {
        if (accountIds.contains(account))
            return account;
    }
    return QString();
}

QString notesFolder(const QList<Workspace> &workspaces, const QString &activityId, const QString &fallback)
{
    const Workspace *workspace = forActivity(workspaces, activityId);
    return workspace && !workspace->notesFolder.isEmpty() ? workspace->notesFolder : fallback;
}

QList<Workspace> withoutAccount(const QList<Workspace> &workspaces, const QString &accountId)
{
    QList<Workspace> result = workspaces;
    for (Workspace &workspace : result)
        workspace.accounts.removeAll(accountId);
    return result;
}

QString newId(const QList<Workspace> &workspaces)
{
    for (int n = 1;; ++n) {
        const QString id = QStringLiteral("workspace-%1").arg(n);
        if (std::none_of(workspaces.cbegin(), workspaces.cend(), [&id](const Workspace &w) { return w.id == id; }))
            return id;
    }
}

QStringList problems(const Workspace &workspace, const QStringList &accountIds, const QStringList &activityIds,
                     bool activitiesAvailable)
{
    auto tr = [](const char *text) { return QCoreApplication::translate("UiUtils::Workspaces", text); };
    QStringList result;
    if (workspace.accounts.isEmpty())
        result << tr("No account is attached, so choosing this workspace shows no particular account.");
    for (const QString &account : workspace.accounts) {
        if (!accountIds.contains(account)) {
            result << tr("An attached account no longer exists.");
            break;
        }
    }
    if (!workspace.activityId.isEmpty()) {
        if (!activitiesAvailable)
            result << tr("Plasma Activities is not running, so the attached activity is not followed.");
        else if (!activityIds.contains(workspace.activityId))
            result << tr("The attached activity no longer exists.");
    }
    return result;
}

}

}
