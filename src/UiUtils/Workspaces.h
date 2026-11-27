/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "UiUtils/Theme.h"
#include "UiUtils/ThemeProfiles.h"

namespace UiUtils {

/** @short Workspaces: mail accounts, a notes folder, an appearance and a Plasma Activity which belong together

A workspace such as "Work" groups the accounts used for it. Attached to a Plasma Activity, it is chosen when that
activity becomes current, so that the window shows one of its accounts; its notes folder receives the notes made
from messages while it is chosen.
*/
namespace Workspaces {

struct Workspace {
    QString id;
    QString name;
    QStringList accounts;
    /** The Plasma Activity, or empty for none */
    QString activityId;
    /** Where notes made from messages go, or empty for the notes folder of the settings */
    QString notesFolder;

    /** @short Which of the appearance attached to the workspace applies while it is active */
    enum class Appearance {
        None,
        All,
        Selected,
    };
    Appearance appearance = Appearance::None;
    /** The id of the theme profile, or empty for none */
    QString themeProfile;
    /** Options replacing the global ones and the profile's; see Theme::overrideEntries() */
    Theme::Values appearanceOverrides;
    /** The options which apply when only selected ones do */
    QStringList selectedAppearance;

    bool operator==(const Workspace &other) const = default;
};

/** @short The workspace attached to @arg activityId, or nullptr */
const Workspace *forActivity(const QList<Workspace> &workspaces, const QString &activityId);

/** @short The account to show when @arg activityId becomes current, or empty to stay on @arg currentAccount

The current account stays when it belongs to the activity's workspace; otherwise the first of the workspace's
accounts which still exists is shown.
*/
QString accountForActivity(const QList<Workspace> &workspaces, const QString &activityId,
                           const QString &currentAccount, const QStringList &accountIds);

/** @short Where notes go while @arg activityId is current: the workspace's folder, or else @arg fallback */
QString notesFolder(const QList<Workspace> &workspaces, const QString &activityId, const QString &fallback);

/** @short @arg workspaces without @arg accountId, as when the account is removed */
QList<Workspace> withoutAccount(const QList<Workspace> &workspaces, const QString &accountId);

/** @short The active workspace: the one of @arg activityId, else the first holding @arg accountId, or nullptr */
const Workspace *active(const QList<Workspace> &workspaces, const QString &activityId, const QString &accountId);

QString configName(Workspace::Appearance appearance);
Workspace::Appearance appearanceFromConfigName(const QString &name);

struct ResolvedAppearance {
    /** The layers of the appearance in effect, with the global settings at the bottom */
    Theme::Layers layers;
    /** A missing profile or values which cannot be applied; references stay saved so that they apply again */
    QStringList problems;
};

/** @short The appearance of @arg workspace layered over @arg global

Nothing is layered when @arg workspace is null, applies no appearance, or @arg disabled, as when the application was
started with --no-workspace-appearance. With Selected, only the selected options of the profile and the overrides
apply; the others keep their global values.
*/
ResolvedAppearance appearance(const Workspace *workspace, const QList<ThemeProfiles::Profile> &profiles,
                              const Theme::Values &global, bool disabled = false);

/** @short An id not used by @arg workspaces */
QString newId(const QList<Workspace> &workspaces);

/** @short Problems which keep @arg workspace from working as set up, in sentences for the settings dialog */
QStringList problems(const Workspace &workspace, const QStringList &accountIds, const QStringList &activityIds,
                     bool activitiesAvailable);

}

}
