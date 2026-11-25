# Menu and context-menu tree specification

**[Refinements](../README.md)** · **[Toolbars](../toolbars/README.md)** ·
**[Interface design](../../philosophy/designs.md)** ·
**[M13 in roadmap](../../ROADMAP.md#m13-refinements)**

These proposed application and context menus organize actions by the object and
task, following KDE Plasma conventions. Each specification maps its entries to
actions that exist today.

**Legend:** ✅ exists in the current code on `main` · 📋 planned only

## Menu tree map

| Specification                                                 | Scope                                           | Exists today                                    |
| ------------------------------------------------------------- | ----------------------------------------------- | ----------------------------------------------- |
| [Application menu](app-menu.md)                               | Top-level application menu bar                  | ✅ Application, View, Mailbox and Message menus |
| [Account context menu](account-tree-context-menu.md)          | Account navigation actions                      | 📋 No account tree; toolbar account selector    |
| [Folder context menu](folder-tree-context-menu.md)            | Mailbox and folder actions                      | ✅ Folder tree context menu                     |
| [Message list context menu](message-list-context-menu.md)     | Message and selection actions                   | ✅ Message list context menu                    |
| [Message header context menu](message-header-context-menu.md) | Per-message header actions                      | ✅ Address context menu only                    |
| [Reading pane context menu](reading-pane-context-menu.md)     | Reading area and content actions                | ✅ Reader context menu                          |
| [Attachment context menu](attachment-context-menu.md)         | Attachment and safe file actions                | ✅ Attachments panel and composer list menus    |
| [Compose context menu](compose-context-menu.md)               | Composer text, recipient and attachment actions | ✅ Editor and attachment list menus             |

> [!NOTE]
>
> Current menus are defined in `src/Gui/Window.cpp` (the `MainWindow` class) and
> the reader, attachment and composer widgets under `src/Gui/`. The status in
> these specifications reflects `main`.

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
