# Reading pane toolbar

**[Documentation](../../README.md)** · **[Roadmap: M13](../../ROADMAP.md#m13-refinements)** ·
**[Refinements](../README.md)** · **[Toolbars](README.md)** ·
**[Menus](../menus/README.md)** · **[Interface design](../../philosophy/designs.md)**

This proposed toolbar appears in the message reading area and supports
message-level actions while reading. It maps each entry to the reader action
bar and other reading actions that exist today.

Legend: ![Exists][exists] in the current code on `main`, not checked in a real
Plasma session · ![Planned][planned] not implemented yet

## Purpose

This is the main reading experience action bar. It supports the user's core
message tasks without cluttering the message content itself.

## Toolbar tree

```text
└─ Reading pane toolbar
   ├─ Left side
   │  ├─ Reply
   │  ├─ Reply all
   │  ├─ Forward
   │  ├─ Mark as read
   │  ├─ Flag
   │  └─ Archive
   ├─ Center
   │  ├─ Open source
   │  ├─ Show details
   │  ├─ Previous message
   │  ├─ Next message
   │  └─ Return to list
   └─ Right side
      ├─ Move to folder
      ├─ Delete
      ├─ Print
      ├─ Save attachment(s)
      └─ Share / external action menu
```

## Actions

The Availability column names where an equivalent action exists today.

| Action                       | Shortcut                                                     | Availability                                                                         | Notes                                                                                                                                                          |
| ---------------------------- | ------------------------------------------------------------ | ------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Reply                        | <kbd>Ctrl</kbd>+<kbd>R</kbd>                                 | ![Exists][exists] Reader action bar, **Message** menu, navigation toolbar            | **Reply by Guess** from the menu; the bar's **Reply** follows the navigation toolbar default                                                                   |
| Reply all                    | <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd> | ![Exists][exists] Reader action bar reply menu, **Message** menu, navigation toolbar | **Reply to All**                                                                                                                                               |
| Forward                      | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F</kbd>                | ![Exists][exists] Reader action bar, **Message** menu, navigation toolbar            | Forwards as an attachment                                                                                                                                      |
| Mark as read                 | <kbd>M</kbd>                                                 | ![Exists][exists] Reader action bar, navigation toolbar, message list context menu   | **Read** toggle in the bar                                                                                                                                     |
| Flag                         | <kbd>S</kbd>                                                 | ![Exists][exists] Reader action bar, navigation toolbar, message list context menu   | **Flag** toggle in the bar; **Mark as Flagged** elsewhere                                                                                                      |
| Archive                      | <kbd>A</kbd>                                                 | ![Exists][exists] Reader action bar, navigation toolbar, message list context menu   |                                                                                                                                                                |
| Open source                  | —                                                            | ![Exists][exists] Message list context menu                                          | **View Message Source…**                                                                                                                                       |
| Show details                 | <kbd>Ctrl</kbd>+<kbd>U</kbd>                                 | ![Exists][exists] Message list context menu, Message Details panel                   | **View Message Headers…**                                                                                                                                      |
| Previous message             | <kbd>B</kbd>                                                 | ![Exists][exists] **Mailbox** menu                                                   | WebEngine builds only. **Previous Message**; **Previous Unread Message** is <kbd>P</kbd>, **Previous Message in Conversation** is <kbd>Alt</kbd>+<kbd>Up</kbd> |
| Next message                 | <kbd>F</kbd>                                                 | ![Exists][exists] **Mailbox** menu                                                   | WebEngine builds only. **Next Message**; **Next Unread Message** is <kbd>N</kbd>, **Next Message in Conversation** is <kbd>Alt</kbd>+<kbd>Down</kbd>           |
| Return to list               | <kbd>Esc</kbd>, <kbd>Alt</kbd>+<kbd>Left</kbd>               | ![Exists][exists] Reader, navigation toolbar in One At a Time layout                 | <kbd>Esc</kbd> returns focus to the list; **Navigate Back** in One At a Time layout                                                                            |
| Move to folder               | <kbd>V</kbd>                                                 | ![Exists][exists] **Message** menu, navigation toolbar                               | **Move to Folder…**; drag and drop from the message list also works                                                                                            |
| Delete                       | <kbd>Delete</kbd>                                            | ![Exists][exists] Reader action bar, navigation toolbar, message list context menu   | **Move to Trash** in the bar when a trash folder is set, else **Delete** or **Undelete**                                                                       |
| Print                        | —                                                            | ![Planned][planned]                                                                  |                                                                                                                                                                |
| Save attachment(s)           | —                                                            | ![Exists][exists] Attachments panel, reader context menu                             | One attachment at a time                                                                                                                                       |
| Share / external action menu | —                                                            | ![Planned][planned]                                                                  |                                                                                                                                                                |

## Current toolbar

The reader action bar (`src/Gui/ReaderActionBar.cpp`) sits above the message in
WebEngine builds only. Items appear in this order:

- **Reply**, a split button with the four reply modes, and **Forward**
- **Archive**, **Move to Trash** (or **Delete** / **Undelete** when deleting
  does not move to a trash folder), and the **Read** and **Flag** toggles
- an expanding space
- an "N attachments" button, shown only when the message has attachments, which
  opens the Attachments panel
- **Zoom Out**, **Reset Zoom**, **Zoom In** and the **Prefer Plain Text** toggle

The mark, archive and delete buttons need network access and a message with a
known UID. **Archive** is disabled when the message is already in the archive
folder.

## Design rules

- The toolbar should remain focused on the currently displayed message and
  nearby reading flow.
- It should not duplicate every folder or bulk-action command from the message
  list.
- The most common actions should remain close to the reading pane and visually
  clear.

[exists]: https://img.shields.io/badge/Exists-1a7f37
[planned]: https://img.shields.io/badge/Planned-6e7781
