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

const Workspace *active(const QList<Workspace> &workspaces, const QString &activityId, const QString &accountId)
{
    if (const Workspace *workspace = forActivity(workspaces, activityId))
        return workspace;
    if (accountId.isEmpty())
        return nullptr;
    for (const Workspace &workspace : workspaces) {
        if (workspace.accounts.contains(accountId))
            return &workspace;
    }
    return nullptr;
}

QString configName(Workspace::Appearance appearance)
{
    switch (appearance) {
    case Workspace::Appearance::All:
        return QStringLiteral("all");
    case Workspace::Appearance::Selected:
        return QStringLiteral("selected");
    case Workspace::Appearance::None:
        break;
    }
    return QStringLiteral("none");
}

Workspace::Appearance appearanceFromConfigName(const QString &name)
{
    if (name == QLatin1String("all"))
        return Workspace::Appearance::All;
    if (name == QLatin1String("selected"))
        return Workspace::Appearance::Selected;
    return Workspace::Appearance::None;
}

ResolvedAppearance appearance(const Workspace *workspace, const QList<ThemeProfiles::Profile> &profiles,
                              const Theme::Values &global, bool disabled)
{
    ResolvedAppearance result;
    result.layers.global = global;
    if (!workspace || disabled || workspace->appearance == Workspace::Appearance::None)
        return result;
    const bool selectedOnly = workspace->appearance == Workspace::Appearance::Selected;
    const auto keep = [&](const QString &entry) {
        return !selectedOnly || workspace->selectedAppearance.contains(entry);
    };
    if (!workspace->themeProfile.isEmpty()) {
        if (const ThemeProfiles::Profile *profile = ThemeProfiles::find(profiles, workspace->themeProfile)) {
            if (selectedOnly) {
                // Only the selected options of the profile replace the global ones; a partial layer is applied as
                // overrides, so that the others are not reset to their defaults
                for (auto it = profile->values.constBegin(); it != profile->values.constEnd(); ++it) {
                    if (keep(it.key()) && Theme::overrideEntries().contains(it.key()))
                        result.layers.overrides.insert(it.key(), it.value());
                }
            } else {
                result.layers.profile = profile->values;
            }
        } else {
            result.problems.append(QCoreApplication::translate(
                "UiUtils::Workspaces",
                "The theme profile of workspace %1 is missing; it applies again once a profile with its id is back.")
                                       .arg(workspace->name));
        }
    }
    for (auto it = workspace->appearanceOverrides.constBegin(); it != workspace->appearanceOverrides.constEnd(); ++it) {
        if (!keep(it.key()))
            continue;
        if (!Theme::overrideEntries().contains(it.key()) || !Theme::validate(it.key(), it.value())) {
            result.problems.append(QCoreApplication::translate("UiUtils::Workspaces",
                                                               "The option %1 of workspace %2 cannot be applied.")
                                       .arg(it.key(), workspace->name));
            continue;
        }
        result.layers.overrides.insert(it.key(), it.value());
    }
    return result;
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
