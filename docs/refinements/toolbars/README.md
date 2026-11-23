# Toolbar layout specification

These proposed toolbar layouts keep primary actions visible and consistent
without crowding each surface.

## Toolbar tree map

| Specification                                   | Scope                                         |
| ----------------------------------------------- | --------------------------------------------- |
| [Navigation toolbar](main-window-toolbar.md)    | Movable icon toolbar and global actions       |
| [Message list toolbar](message-list-toolbar.md) | Message triage and selection actions          |
| [Reading pane toolbar](reading-pane-toolbar.md) | Actions while reading a message               |
| [Compose toolbar](compose-toolbar.md)           | Message composition                           |
| [Attachment toolbar](attachment-toolbar.md)     | Attachment actions while reading or composing |

## Layout principles

- Toolbars should mirror the user’s current context rather than repeat
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
