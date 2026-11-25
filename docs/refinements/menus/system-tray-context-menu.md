# System tray context menu

**[Refinements](../README.md)** · **[Menus](README.md)** ·
**[Interface design](../../philosophy/designs.md)** ·
**[M14 in roadmap](../../ROADMAP.md#m14-refinements)**

This proposed menu appears when the user opens the context menu of the Plasma
system tray icon.

**Legend:** ✅ exists in the current code on `main` · 📋 planned only

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

| Action                  | Shortcut | Availability | Notes                                             |
| ----------------------- | -------- | ------------ | ------------------------------------------------- |
| Show / hide main window | —        | 📋 Planned   | Clicking the tray icon already toggles the window |
| New message             | —        | 📋 Planned   |                                                   |
| Check mail              | —        | 📋 Planned   |                                                   |
| Settings                | —        | 📋 Planned   |                                                   |
| Quit                    | —        | ✅ Tray menu | **Quit**                                          |

## Current menu

The tray icon is created in `MainWindow::createSysTray()` in
`src/Gui/Window.cpp` when the tray setting is enabled. Its context menu contains
only **Quit**. Clicking the icon shows or hides the main window, and the icon
also shows network and background-account warnings.

## Behavior

- Show or hide the main window without closing the application. Keep tray
  activation and the menu action consistent.
- Open the composer for a new message. Check mail for configured, available
  accounts; report failures rather than implying synchronization succeeded.
- Open application settings, including tray preferences when available.
- Quit the application rather than merely hiding its window. Protect unsaved
  drafts and pending work by allowing the user to resolve or cancel quitting.
- Keep actions accessible and localized. Disable or explain unavailable actions
  without exposing private message details in the menu.
