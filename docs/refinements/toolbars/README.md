# Toolbar specifications

**[Documentation](../../README.md)** · **[Roadmap: M13](../../ROADMAP.md#m13-refinements)** ·
**[Refinements](../README.md)** · **[Menus](../menus/README.md)** ·
**[Interface design](../../philosophy/designs.md)**

These proposed toolbar layouts keep primary actions visible and consistent
without crowding each surface. Each specification maps its entries to the
actions that exist today.

Legend: ![Exists][exists] in the current code on `main`, not checked in a real
Plasma session · ![Planned][planned] not implemented yet

## Specifications

Every specification uses the same sections: **Purpose**, **Toolbar tree**,
**Actions**, **Current toolbar** and **Design rules**.

| Specification                                   | Scope                                         | Exists today                                                               |
| ----------------------------------------------- | --------------------------------------------- | -------------------------------------------------------------------------- |
| [Navigation toolbar](main-window-toolbar.md)    | Movable icon toolbar and global actions       | ![Exists][exists] **Navigation** toolbar, docked left                      |
| [Message list toolbar](message-list-toolbar.md) | Message triage and selection actions          | ![Exists][exists] Tool row with quick search, filters and sort; status row |
| [Reading pane toolbar](reading-pane-toolbar.md) | Actions while reading a message               | ![Exists][exists] Reader action bar, WebEngine builds only                 |
| [Compose toolbar](compose-toolbar.md)           | Message composition                           | ![Exists][exists] Composer button row, no toolbar                          |
| [Attachment toolbar](attachment-toolbar.md)     | Attachment actions while reading or composing | ![Exists][exists] Attachments panel header buttons                         |

> [!NOTE]
>
> The navigation toolbar and the message list tool row are built in
> `src/Gui/Window.cpp` and `src/Gui/MessageListWidget.cpp`; the reader action
> bar, composer and Attachments panel live in their own widgets under
> `src/Gui/`. The status in these specifications reflects `main` on 2026-11-25.

## Layout principles

- Toolbars should mirror the user's current context rather than repeat
  everything from every surface.
- Primary actions should be concise and visible at a glance.
- Make destructive actions recognizable and confirm them when appropriate.
- The app should support compact, comfortable, and focus layout modes without
  duplicating the same toolbar in multiple forms.
- Follow the
  [shared placement and customization model](../../philosophy/designs.md#toolbars-and-customization):
  toolbars and their items can be rearranged in customization mode while
  pane-specific actions retain their context.

> [!TIP]
>
> Use toolbars for the current context; keep less common or destructive actions
> in appropriate menus or dialogs.

[exists]: https://img.shields.io/badge/Exists-1a7f37
[planned]: https://img.shields.io/badge/Planned-6e7781
