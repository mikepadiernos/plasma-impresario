# Message list context menu

**[Refinements](../README.md)** · **[Menus](README.md)** ·
**[Interface design](../../philosophy/designs.md)** ·
**[M12 in roadmap](../../ROADMAP.md#m12-refinements)**

This proposed menu appears when the user right-clicks one or more rows in the
message list.

**Legend:** ✅ exists in the current code on `feature/m02-shell-next` · 📋
planned only

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

| Action              | Shortcut                                                     | Availability                            | Notes                                        |
| ------------------- | ------------------------------------------------------------ | --------------------------------------- | -------------------------------------------- |
| Open                | —                                                            | ✅ Message list                         | Select the row to show it in the reader      |
| Open in new window  | <kbd>Ctrl</kbd>+<kbd>Return</kbd>                            | ✅ Shortcut only                        | **Open message in New Window…**              |
| Reply               | <kbd>Ctrl</kbd>+<kbd>R</kbd>                                 | ✅ **Message** menu, navigation toolbar | **Reply by Guess**                           |
| Reply all           | <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd> | ✅ **Message** menu, navigation toolbar | **Reply to All**                             |
| Forward             | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F</kbd>                | ✅ **Message** menu, navigation toolbar | Forwards as an attachment                    |
| Mark as read        | <kbd>M</kbd>                                                 | ✅ Context menu, navigation toolbar     | **Mark as Read** is a toggle                 |
| Mark as unread      | <kbd>M</kbd>                                                 | ✅ Context menu, navigation toolbar     | Clear the **Mark as Read** toggle            |
| Flag                | <kbd>S</kbd>                                                 | ✅ Context menu, navigation toolbar     | **Mark as Flagged** is a toggle              |
| Unflag              | <kbd>S</kbd>                                                 | ✅ Context menu, navigation toolbar     | Clear the **Mark as Flagged** toggle         |
| Move to folder      | —                                                            | ✅ Drag and drop onto the folder tree   | Hold <kbd>Shift</kbd> to move without asking |
| Copy to folder      | —                                                            | ✅ Drag and drop onto the folder tree   | Hold <kbd>Ctrl</kbd> to copy without asking  |
| Archive             | <kbd>A</kbd>                                                 | ✅ Context menu, navigation toolbar     | Moves to the account's archive folder        |
| Delete              | <kbd>Delete</kbd>                                            | ✅ Context menu, navigation toolbar     | **Mark as Deleted**; **Expunge** removes it  |
| Print               | —                                                            | 📋 Planned                              |                                              |
| Add to notes        | —                                                            | 📋 Planned                              |                                              |
| Add contact         | —                                                            | 📋 Planned                              |                                              |
| View source         | —                                                            | ✅ Context menu                         | **View Message Source…**                     |
| Copy sender address | —                                                            | 📋 Planned                              | The reader's header addresses can be copied  |
| Copy subject        | —                                                            | 📋 Planned                              |                                              |
| Copy message ID     | —                                                            | 📋 Planned                              |                                              |

### Multi-select

The list uses extended selection, and the mark, archive and save actions apply
to every selected message.

| Action                   | Shortcut          | Availability                          | Notes                                           |
| ------------------------ | ----------------- | ------------------------------------- | ----------------------------------------------- |
| Mark selected as read    | <kbd>M</kbd>      | ✅ Context menu, navigation toolbar   |                                                 |
| Mark selected as unread  | <kbd>M</kbd>      | ✅ Context menu, navigation toolbar   | Clear the **Mark as Read** toggle               |
| Flag selected            | <kbd>S</kbd>      | ✅ Context menu, navigation toolbar   |                                                 |
| Archive selected         | <kbd>A</kbd>      | ✅ Context menu, navigation toolbar   |                                                 |
| Move selected to folder  | —                 | ✅ Drag and drop onto the folder tree |                                                 |
| Delete selected          | <kbd>Delete</kbd> | ✅ Context menu, navigation toolbar   | No count or confirmation today                  |
| Copy selected messages   | —                 | ✅ Drag and drop onto the folder tree |                                                 |
| Export selected messages | —                 | ✅ Context menu                       | **Save Message…** asks for one file per message |

## Current menu

`MainWindow::showContextMenuMsgListTree()` in `src/Gui/Window.cpp` shows the
menu only on a message row:

- 📬 Mark as Read, Mark as Deleted, Mark as Flagged, Mark as Junk
  (<kbd>J</kbd>), Mark as Not Junk (<kbd>Shift</kbd>+<kbd>J</kbd>), Archive
- 📁 Mark Mailbox as Read
- 💾 Save Message…, View Message Source…, View Message Headers…
  (<kbd>Ctrl</kbd>+<kbd>U</kbd>)
- 🏷️ Tag with 1st tag through Tag with 9th tag (<kbd>1</kbd>–<kbd>9</kbd>), one
  entry per defined favorite tag

## Safety and quality rules

- Multi-select menus should operate only on the selected messages and never on
  all messages in the current folder.
- Actions must reflect the current visible selection and not stale state after
  an async refresh.
- Destructive bulk actions should include a count and confirm before applying.
