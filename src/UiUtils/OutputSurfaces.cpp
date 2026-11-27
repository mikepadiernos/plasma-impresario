/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/OutputSurfaces.h"

#include <QCoreApplication>

namespace UiUtils {

namespace OutputSurfaces {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("UiUtils::OutputSurfaces", text);
}

/** The window when the user can see the account there, the desktop otherwise */
Surfaces whereTheUserIs(const Context &context)
{
    if (context.windowActive && context.accountShown)
        return Surface::InlineBar | Surface::StatusBar;
    if (context.quiet)
        return context.accountShown ? Surfaces(Surface::InlineBar) : Surfaces();
    return context.accountShown ? (Surface::InlineBar | Surface::DesktopNotification) : Surfaces(Surface::DesktopNotification);
}

}

Surfaces route(Event event, const Context &context)
{
    Surfaces surfaces = Surface::Activity;
    switch (event) {
    case Event::SyncProgress:
    case Event::BackgroundJob:
        if (context.accountShown || event == Event::BackgroundJob)
            surfaces |= Surface::StatusBar;
        break;
    case Event::SyncFailed:
    case Event::AccountAttention:
    case Event::RuleFailed:
    case Event::BackgroundJobFailed:
        surfaces |= whereTheUserIs(context);
        break;
    case Event::NewMail:
        surfaces |= Surface::TaskManager;
        if (!context.quiet)
            surfaces |= Surface::DesktopNotification;
        break;
    case Event::RuleApplied:
        if (context.windowActive && context.accountShown)
            surfaces |= Surface::StatusBar;
        break;
    case Event::RuleNeedsReview:
    case Event::ScanFinding:
        if (context.accountShown)
            surfaces |= Surface::InlineBar;
        break;
    }
    return surfaces;
}

QString surfaceName(Surface surface)
{
    switch (surface) {
    case Surface::None:
        return {};
    case Surface::StatusBar:
        return tr("Status line");
    case Surface::InlineBar:
        return tr("Inline bar");
    case Surface::Activity:
        return tr("Activity log");
    case Surface::DesktopNotification:
        return tr("Desktop notification");
    case Surface::TaskManager:
        return tr("Task manager count");
    }
    return {};
}

QString eventName(Event event)
{
    switch (event) {
    case Event::SyncProgress:
        return tr("Sync progress");
    case Event::SyncFailed:
        return tr("Sync failed");
    case Event::AccountAttention:
        return tr("Account needs attention");
    case Event::NewMail:
        return tr("New mail");
    case Event::RuleApplied:
        return tr("Rule applied");
    case Event::RuleFailed:
        return tr("Rule failed");
    case Event::RuleNeedsReview:
        return tr("Rule waits for a preview");
    case Event::ScanFinding:
        return tr("Attachment scan finding");
    case Event::BackgroundJob:
        return tr("Background work");
    case Event::BackgroundJobFailed:
        return tr("Background work failed");
    }
    return {};
}

}

}
