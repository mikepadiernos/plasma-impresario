# Message list context menu

**[Documentation](../../README.md)** · **[Roadmap: M13](../../ROADMAP.md#m13-refinements)** ·
**[Refinements](../README.md)** · **[Menus](README.md)** ·
**[Toolbars](../toolbars/README.md)** · **[Interface design](../../philosophy/designs.md)**

This proposed menu appears when the user right-clicks one or more rows in the
message list. It maps each entry to the message actions that exist today.

Legend: ![Exists][exists] in the current code on `main`, not checked in a real
Plasma session · ![Planned][planned] not implemented yet

## Purpose

This menu supports the core triage workflow. It is the main high-density action
surface for mailbox management and batch message operations.

## Menu tree

```text
└─ Message list context menu
   ├─ Single message
   │  ├─ Open
   │  ├─ Open in new window
   │  ├─ Reply
   │  ├─ Reply all
   │  ├─ Forward
   │  ├─ Mark as read
   │  ├─ Mark as unread
   │  ├─ Flag
   │  ├─ Unflag
   │  ├─ Move to folder
   │  ├─ Copy to folder
   │  ├─ Archive
   │  ├─ Delete
   │  ├─ Print
   │  ├─ Add to notes
   │  ├─ Add contact
   │  ├─ View source
   │  ├─ Copy sender address
   │  ├─ Copy subject
   │  └─ Copy message ID
   └─ Multi-select
      ├─ Mark selected as read
      ├─ Mark selected as unread
      ├─ Flag selected
      ├─ Archive selected
      ├─ Move selected to folder
      ├─ Delete selected
      ├─ Copy selected messages
      └─ Export selected messages
```

## Actions

### Single message

| Action              | Shortcut                                                     | Availability                                                                                  | Notes                                                                                                      |
| ------------------- | ------------------------------------------------------------ | --------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------- |
| Open                | —                                                            | ![Exists][exists] Message list                                                                | Select the row to show it in the reader                                                                    |
| Open in new window  | <kbd>Ctrl</kbd>+<kbd>Return</kbd>                            | ![Exists][exists] Shortcut only                                                               | **Open message in New Window…**                                                                            |
| Reply               | <kbd>Ctrl</kbd>+<kbd>R</kbd>                                 | ![Exists][exists] **Message** menu, navigation toolbar                                        | **Reply by Guess**                                                                                         |
| Reply all           | <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd> | ![Exists][exists] **Message** menu, navigation toolbar                                        | **Reply to All**                                                                                           |
| Forward             | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F</kbd>                | ![Exists][exists] **Message** menu, navigation toolbar                                        | Forwards as an attachment                                                                                  |
| Mark as read        | <kbd>M</kbd>                                                 | ![Exists][exists] Context menu, navigation toolbar                                            | **Mark as Read** is a toggle                                                                               |
| Mark as unread      | <kbd>Shift</kbd>+<kbd>M</kbd>                                | ![Exists][exists] Context menu, **Message** menu                                              | **Mark as Unread**; clearing the **Mark as Read** toggle also works                                        |
| Flag                | <kbd>S</kbd>                                                 | ![Exists][exists] Context menu, navigation toolbar                                            | **Mark as Flagged** is a toggle                                                                            |
| Unflag              | <kbd>S</kbd>                                                 | ![Exists][exists] Context menu, navigation toolbar                                            | Clear the **Mark as Flagged** toggle                                                                       |
| Move to folder      | <kbd>V</kbd>                                                 | ![Exists][exists] Context menu **Move To**, **Message › Move to Folder…**, navigation toolbar | **Move To** lists recent folders; drag and drop with <kbd>Shift</kbd> moves without asking                 |
| Copy to folder      | <kbd>Shift</kbd>+<kbd>V</kbd>                                | ![Exists][exists] Context menu **Copy To**, **Message › Copy to Folder…**                     | Drag and drop with <kbd>Ctrl</kbd> copies without asking                                                   |
| Archive             | <kbd>A</kbd>                                                 | ![Exists][exists] Context menu, navigation toolbar                                            | Moves to the account's archive folder                                                                      |
| Delete              | <kbd>Delete</kbd>                                            | ![Exists][exists] Context menu, navigation toolbar                                            | **Mark as Deleted**; **Expunge** removes it; **Delete Permanently…** is <kbd>Shift</kbd>+<kbd>Delete</kbd> |
| Print               | —                                                            | ![Planned][planned]                                                                           |                                                                                                            |
| Add to notes        | —                                                            | ![Planned][planned]                                                                           |                                                                                                            |
| Add contact         | —                                                            | ![Planned][planned]                                                                           |                                                                                                            |
| View source         | —                                                            | ![Exists][exists] Context menu                                                                | **View Message Source…**                                                                                   |
| Copy sender address | —                                                            | ![Planned][planned]                                                                           | The reader's header addresses can be copied                                                                |
| Copy subject        | —                                                            | ![Planned][planned]                                                                           |                                                                                                            |
| Copy message ID     | —                                                            | ![Planned][planned]                                                                           |                                                                                                            |

### Multi-select

The list uses extended selection, and the mark, move, archive, delete and save
actions apply to every selected message.

| Action                   | Shortcut                      | Availability                                                                             | Notes                                                                                             |
| ------------------------ | ----------------------------- | ---------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------- |
| Mark selected as read    | <kbd>M</kbd>                  | ![Exists][exists] Context menu, navigation toolbar                                       |                                                                                                   |
| Mark selected as unread  | <kbd>Shift</kbd>+<kbd>M</kbd> | ![Exists][exists] Context menu, **Message** menu                                         | **Mark as Unread**                                                                                |
| Flag selected            | <kbd>S</kbd>                  | ![Exists][exists] Context menu, navigation toolbar                                       |                                                                                                   |
| Archive selected         | <kbd>A</kbd>                  | ![Exists][exists] Context menu, navigation toolbar                                       |                                                                                                   |
| Move selected to folder  | <kbd>V</kbd>                  | ![Exists][exists] Context menu **Move To**, **Message › Move to Folder…**, drag and drop |                                                                                                   |
| Delete selected          | <kbd>Delete</kbd>             | ![Exists][exists] Context menu, navigation toolbar                                       | **Mark as Deleted** asks no confirmation; **Delete Permanently…** confirms with the message count |
| Copy selected messages   | <kbd>Shift</kbd>+<kbd>V</kbd> | ![Exists][exists] Context menu **Copy To**, **Message › Copy to Folder…**, drag and drop |                                                                                                   |
| Export selected messages | —                             | ![Exists][exists] Context menu                                                           | **Save Message…** asks for one file per message                                                   |

## Current menu

`MainWindow::showContextMenuMsgListTree()` in `src/Gui/Window.cpp` builds the
menu from the clicked position:

| Where                            | Entries                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| -------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| On a message row                 | Mark as Read, Mark as Unread, Mark as Deleted, Delete Permanently…, Mark as Flagged, Mark as Junk (<kbd>J</kbd>), Mark as Not Junk (<kbd>Shift</kbd>+<kbd>J</kbd>), Archive, Move To, Copy To, Mark Mailbox as Read, Save Message…, View Message Source…, View Message Headers… (<kbd>Ctrl</kbd>+<kbd>U</kbd>), Tag with 1st tag through Tag with 9th tag (<kbd>1</kbd>–<kbd>9</kbd>, one entry per defined favorite tag), then Select Thread (when available), Select All, Select Similar and Show Related |
| On empty space or a group header | Select All, the Unread, Flagged and With Attachments filters, Clear Filters, Mark Mailbox as Read, then Expand and Collapse All Groups when grouped, or Expand and Collapse All Conversations when threaded                                                                                                                                                                                                                                                                                                 |

## Design rules

- Multi-select menus should operate only on the selected messages and never on
  all messages in the current folder.
- Actions must reflect the current visible selection and not stale state after
  an async refresh.
- Destructive bulk actions should include a count and confirm before applying.

[exists]: https://img.shields.io/badge/Exists-1a7f37
[planned]: https://img.shields.io/badge/Planned-6e7781
