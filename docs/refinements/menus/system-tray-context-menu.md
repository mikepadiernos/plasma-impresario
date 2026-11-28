# System tray context menu

**[Documentation](../../README.md)** · **[Roadmap: M13](../../ROADMAP.md#m13-refinements)** ·
**[Refinements](../README.md)** · **[Menus](README.md)** ·
**[Toolbars](../toolbars/README.md)** · **[Interface design](../../philosophy/designs.md)**

This proposed menu appears when the user opens the context menu of the Plasma
system tray icon. It maps each entry to the tray actions that exist today.

Legend: ![Exists][exists] in the current code on `main`, not checked in a real
Plasma session · ![Planned][planned] not implemented yet

## Purpose

The tray menu gives quick access to a few global actions while the main window
is hidden. It should stay short and never show private message details.

## Menu tree

```text
└─ System tray context menu
   ├─ Show / hide main window
   ├─ New message
   ├─ Check mail
   ├─ Settings
   └─ Quit
```

## Actions

| Action                  | Shortcut | Availability                | Notes                                             |
| ----------------------- | -------- | --------------------------- | ------------------------------------------------- |
| Show / hide main window | —        | ![Planned][planned]         | Clicking the tray icon already toggles the window |
| New message             | —        | ![Planned][planned]         |                                                   |
| Check mail              | —        | ![Planned][planned]         |                                                   |
| Settings                | —        | ![Planned][planned]         |                                                   |
| Quit                    | —        | ![Exists][exists] Tray menu | **Quit**                                          |

## Current menu

The tray icon is created in `MainWindow::createSysTray()` in
`src/Gui/Window.cpp` when the tray setting is enabled. Its context menu contains
only **Quit**. Clicking the icon shows or hides the main window.

The icon itself reports two kinds of state, updated in
`MainWindow::handleTrayIconChange()`:

- **Unread count:** drawn on the icon for all or subscribed watched folders, or
  for the Inbox only, depending on the watched-folders setting. The tooltip
  reads "Plasma Impresario - N unread message(s)".
- **Offline state:** the icon is drawn disabled and the count turns red. With
  no unread count, the tooltip ends in "- offline".

> [!NOTE]
>
> The tray icon does not show background-account warnings. Those arrive as the
> `accountNeedsAttention` desktop notification, which offers **Open Account**
> and, for OAuth sign-in failures, **Sign In**. Only builds without
> KNotifications fall back to a tray balloon message.

## Design rules

- Show or hide the main window without closing the application. Keep tray
  activation and the menu action consistent.
- Open the composer for a new message. Check mail for configured, available
  accounts; report failures rather than implying synchronization succeeded.
- Open application settings, including tray preferences when available.
- Quit the application rather than merely hiding its window. Protect unsaved
  drafts and pending work by allowing the user to resolve or cancel quitting.
- Keep actions accessible and localized. Disable or explain unavailable actions
  without exposing private message details in the menu.

[exists]: https://img.shields.io/badge/Exists-1a7f37
[planned]: https://img.shields.io/badge/Planned-6e7781
