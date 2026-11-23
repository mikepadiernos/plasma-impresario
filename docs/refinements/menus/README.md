# Menu and context-menu tree specification

These proposed application and context menus organize actions by the object and
task, following KDE Plasma conventions.

## Menu tree map

| Specification                                                 | Scope                                           |
| ------------------------------------------------------------- | ----------------------------------------------- |
| [Application menu](app-menu.md)                               | Top-level application menu bar                  |
| [Account context menu](account-tree-context-menu.md)          | Account navigation actions                      |
| [Folder context menu](folder-tree-context-menu.md)            | Mailbox and folder actions                      |
| [Message list context menu](message-list-context-menu.md)     | Message and selection actions                   |
| [Message header context menu](message-header-context-menu.md) | Per-message header actions                      |
| [Reading pane context menu](reading-pane-context-menu.md)     | Reading area and content actions                |
| [Attachment context menu](attachment-context-menu.md)         | Attachment and safe file actions                |
| [Compose context menu](compose-context-menu.md)               | Composer text, recipient and attachment actions |

## Product principles

- Keep menu trees scoped to the object under the cursor.
- Group commands by user intent, not by implementation detail.
- Keep destructive actions visible, confirmed, and recoverable.
- Keep keyboard-first flows consistent with the same actions available in menus.
- Distinguish global, folder-scoped, and message-scoped actions clearly.

> [!TIP]
>
> Keep primary actions in the menu bar, related actions in toolbars and
> contextual equivalents in the relevant object menu.
