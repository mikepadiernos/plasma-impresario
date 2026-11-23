# Toolbar layout specification

These proposed toolbar layouts keep primary actions visible and consistent
without crowding each surface.

## Toolbar tree map

| Specification                                   | Scope                                         |
| ----------------------------------------------- | --------------------------------------------- |
| [Main window toolbar](main-window-toolbar.md)   | Mailbox navigation and global actions         |
| [Message list toolbar](message-list-toolbar.md) | Message triage and selection actions          |
| [Reading pane toolbar](reading-pane-toolbar.md) | Actions while reading a message               |
| [Compose toolbar](compose-toolbar.md)           | Message composition                           |
| [Attachment toolbar](attachment-toolbar.md)     | Attachment actions while reading or composing |

## Layout principles

- Toolbars should mirror the user’s current context rather than repeat
  everything from every surface.
- Primary actions should be concise and visible at a glance.
- Dangerous actions should not crowd the main toolbar; they belong in dialogs or
  contextual menus.
- The app should support compact, comfortable, and focus layout modes without
  duplicating the same toolbar in multiple forms.

> [!TIP]
>
> Use toolbars for the current context; keep less common or destructive actions
> in appropriate menus or dialogs.
