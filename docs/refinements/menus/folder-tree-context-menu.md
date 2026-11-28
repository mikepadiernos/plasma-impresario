# Folder tree context menu

**[Documentation](../../README.md)** · **[Roadmap: M13](../../ROADMAP.md#m13-refinements)** ·
**[Refinements](../README.md)** · **[Menus](README.md)** ·
**[Toolbars](../toolbars/README.md)** · **[Interface design](../../philosophy/designs.md)**

This proposed menu appears when the user right-clicks a mailbox or folder in the
folder tree. It maps each entry to the folder actions that exist today.

Legend: ![Exists][exists] in the current code on `main`, not checked in a real
Plasma session · ![Planned][planned] not implemented yet

## Purpose

This menu is for mailbox administration and status. It should remain focused on
the folder itself and not expose read or compose actions unless they are
relevant to the selected folder header.

## Menu tree

```text
└─ Folder context menu
   ├─ Open folder
   ├─ Refresh folder
   ├─ New subfolder
   ├─ Rename folder
   ├─ Delete folder
   ├─ Empty folder
   ├─ Empty trash
   ├─ Move folder
   ├─ Copy folder
   ├─ Mark all as read
   ├─ Subscribe
   ├─ Unsubscribe
   ├─ Folder properties
   ├─ Set as favorite
   ├─ Set as default folder
   └─ Show folder statistics
```

## Actions

| Action                 | Shortcut                     | Availability                                                                    | Notes                                                                                                |
| ---------------------- | ---------------------------- | ------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------- |
| Open folder            | <kbd>Ctrl</kbd>+<kbd>J</kbd> | ![Exists][exists] Folder tree, **Mailbox › Go to Folder**                       | Select the folder, or use **Go to Folder…** to pick one by name                                      |
| Refresh folder         | —                            | ![Exists][exists] Folder tree context menu                                      | **Check for New Messages**                                                                           |
| New subfolder          | —                            | ![Exists][exists] Folder tree context menu                                      | **Create Child Mailbox…**                                                                            |
| Rename folder          | —                            | ![Exists][exists] Folder tree context menu, **Mailbox › Rename Folder…**        | Enabled when the folder can be renamed                                                               |
| Delete folder          | —                            | ![Exists][exists] Folder tree context menu                                      | **Remove Mailbox** asks for confirmation                                                             |
| Empty folder           | —                            | ![Planned][planned]                                                             | Only Trash and Junk can be emptied today                                                             |
| Empty trash            | —                            | ![Exists][exists] Folder tree context menu on Trash, **Mailbox › Empty Trash…** | **Empty Junk…** appears on the Junk folder; **Mailbox › Expunge** removes messages marked as deleted |
| Move folder            | —                            | ![Planned][planned]                                                             |                                                                                                      |
| Copy folder            | —                            | ![Planned][planned]                                                             |                                                                                                      |
| Mark all as read       | —                            | ![Exists][exists] Folder tree context menu                                      | **Mark Mailbox as Read**                                                                             |
| Subscribe              | —                            | ![Exists][exists] Folder tree context menu                                      | **Subscribed** checkbox                                                                              |
| Unsubscribe            | —                            | ![Exists][exists] Folder tree context menu                                      | Clear the **Subscribed** checkbox                                                                    |
| Folder properties      | —                            | ![Planned][planned]                                                             |                                                                                                      |
| Set as favorite        | —                            | ![Planned][planned]                                                             |                                                                                                      |
| Set as default folder  | —                            | ![Planned][planned]                                                             |                                                                                                      |
| Show folder statistics | —                            | ![Planned][planned]                                                             |                                                                                                      |

## Current menu

`MainWindow::showContextMenuMboxTree()` in `src/Gui/Window.cpp` builds the menu
from the clicked position:

| Where          | Entries                                                                                                                                                                                           |
| -------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| On a folder    | Create Child Mailbox…, Rename Folder…, Remove Mailbox, Empty Trash… (on Trash) or Empty Junk… (on Junk), Mark Mailbox as Read, Check for New Messages, Update List of Child Mailboxes, Subscribed |
| On empty space | Create New Mailbox…                                                                                                                                                                               |
| Always         | Reload Everything, Show Only Subscribed Folders                                                                                                                                                   |

Messages dragged from the message list onto a folder are moved or copied. A menu
asks which unless a default is set; hold <kbd>Shift</kbd> to move or
<kbd>Ctrl</kbd> to copy.

## Design rules

- Empty folder and delete folder operations should be explicit and reversible
  through confirmation or undo windows.
- Folder operations should preserve account scope and not leak into another
  account or remote mailbox.
- Favorite or special-folder status must not alter the actual folder data model
  in unexpected ways.

[exists]: https://img.shields.io/badge/Exists-1a7f37
[planned]: https://img.shields.io/badge/Planned-6e7781
