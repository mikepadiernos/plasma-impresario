# Menu specifications

**[Documentation](../../README.md)** · **[Roadmap: M14](../../ROADMAP.md#m14-refinements)** ·
**[Refinements](../README.md)** · **[Toolbars](../toolbars/README.md)** ·
**[Interface design](../../philosophy/designs.md)**

These proposed application and context menus organize actions by object and
task, following KDE Plasma conventions. Each specification maps its entries to
the actions that exist today.

Legend: ![Exists][exists] in the current code on `main`, not checked in a real
Plasma session · ![Planned][planned] not implemented yet

## Specifications

Every specification uses the same sections: **Purpose**, **Menu tree**,
**Actions**, **Current menu** and **Design rules**.

| Specification                                                 | Scope                                           | Exists today                                                         |
| ------------------------------------------------------------- | ----------------------------------------------- | -------------------------------------------------------------------- |
| [Application menu](app-menu.md)                               | Top-level application menu bar                  | ![Exists][exists] Application, View, Mailbox, Message and Help menus |
| [System tray context menu](system-tray-context-menu.md)       | Tray access to global actions                   | ![Exists][exists] Quit only                                          |
| [Account context menu](account-tree-context-menu.md)          | Account navigation actions                      | ![Exists][exists] Account header menu in the folder sidebar          |
| [Folder context menu](folder-tree-context-menu.md)            | Mailbox and folder actions                      | ![Exists][exists] Folder tree context menu                           |
| [Message list context menu](message-list-context-menu.md)     | Message and selection actions                   | ![Exists][exists] Message list context menu                          |
| [Message header context menu](message-header-context-menu.md) | Per-message header actions                      | ![Exists][exists] Address context menu only                          |
| [Reading pane context menu](reading-pane-context-menu.md)     | Reading area and content actions                | ![Exists][exists] Reader context menu                                |
| [Attachment context menu](attachment-context-menu.md)         | Attachment and safe file actions                | ![Exists][exists] Attachments panel and composer list menus          |
| [Compose context menu](compose-context-menu.md)               | Composer text, recipient and attachment actions | ![Exists][exists] Editor and attachment list menus                   |

> [!NOTE]
>
> Current menus are defined in `src/Gui/Window.cpp` (the `MainWindow` class) and
> in the sidebar, reader, attachment and composer widgets under `src/Gui/`. The
> status in these specifications reflects `main` on 2026-11-25.

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

[exists]: https://img.shields.io/badge/Exists-1a7f37
[planned]: https://img.shields.io/badge/Planned-6e7781
