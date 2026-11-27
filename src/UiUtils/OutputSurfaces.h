/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QFlags>
#include <QString>

namespace UiUtils {

/** @short Where the application reports what happens in the background

Every report goes to the activity log, where it can be looked up later. Beyond that, a report reaches the user where
the user is: in the window's status line and inline bars while the account concerned is shown, and on the desktop, as
a notification, when it is about an account which is not shown or the window is not in use. Only new mail and problems
which need the user reach the desktop; progress and routine results never do, and nothing raises the window or takes
the keyboard focus. The task manager entry shows the unread count.

Announcements which wait while the window is active are decided by NewMail::delivery(); this table only says where a
report may go.
*/
namespace OutputSurfaces {

enum class Surface {
    None = 0,
    /** The status line at the bottom of the window */
    StatusBar = 1 << 0,
    /** A bar above the message list or reader, which stays until it is dismissed or the problem is solved */
    InlineBar = 1 << 1,
    /** The activity log */
    Activity = 1 << 2,
    /** A desktop notification */
    DesktopNotification = 1 << 3,
    /** The count on the task manager entry */
    TaskManager = 1 << 4,
};
Q_DECLARE_FLAGS(Surfaces, Surface)

enum class Event {
    /** An account connects or loads its folders and messages */
    SyncProgress,
    /** An account could not connect or sync */
    SyncFailed,
    /** An account needs the user, such as for signing in again */
    AccountAttention,
    NewMail,
    /** A rule moved, marked or tagged messages */
    RuleApplied,
    /** The server refused what a rule asked for */
    RuleFailed,
    /** A rule which moves messages matched new mail but waits for a preview */
    RuleNeedsReview,
    /** An attachment scan found something */
    ScanFinding,
    /** Work started by the user which goes on in the background, such as sending or indexing */
    BackgroundJob,
    BackgroundJobFailed,
};

struct Context {
    /** Whether the window is active */
    bool windowActive = false;
    /** Whether the account concerned is the one shown in the window */
    bool accountShown = false;
    /** Whether the user asked for no desktop notifications for this account or folder */
    bool quiet = false;
};

/** @short Where @arg event is reported in @arg context */
Surfaces route(Event event, const Context &context);

/** The name of @arg surface for documentation and settings, such as "Desktop notification" */
QString surfaceName(Surface surface);
/** The name of @arg event, such as "Rule failed" */
QString eventName(Event event);

}

}

Q_DECLARE_OPERATORS_FOR_FLAGS(UiUtils::OutputSurfaces::Surfaces)
