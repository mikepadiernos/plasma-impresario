# Toolbar layout specification

**[Refinements](../README.md)** · **[Menus](../menus/README.md)** ·
**[Interface design](../../philosophy/designs.md)** ·
**[M14 in roadmap](../../ROADMAP.md#m14-refinements)**

These proposed toolbar layouts keep primary actions visible and consistent
without crowding each surface. Each specification maps its entries to actions
that exist today.

**Legend:** ![Exists][exists] in the current code on `main` ·
![Planned][planned] not implemented yet

## Toolbar tree map

| Specification                                   | Scope                                         | Exists today                                          |
| ----------------------------------------------- | --------------------------------------------- | ----------------------------------------------------- |
| [Navigation toolbar](main-window-toolbar.md)    | Movable icon toolbar and global actions       | ![Exists][exists] **Navigation** toolbar, docked left |
| [Message list toolbar](message-list-toolbar.md) | Message triage and selection actions          | ![Planned][planned] Quick search field only           |
| [Reading pane toolbar](reading-pane-toolbar.md) | Actions while reading a message               | ![Planned][planned] No toolbar                        |
| [Compose toolbar](compose-toolbar.md)           | Message composition                           | ![Planned][planned] Button row only                   |
| [Attachment toolbar](attachment-toolbar.md)     | Attachment actions while reading or composing | ![Exists][exists] Attachments panel header buttons    |

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
