# Reading pane toolbar

**[Refinements](../README.md)** · **[Toolbars](README.md)** ·
**[Interface design](../../philosophy/designs.md)** ·
**[M14 in roadmap](../../ROADMAP.md#m14-refinements)**

This proposed toolbar appears in the message reading area and supports
message-level actions while reading.

**Legend:** ![Exists][exists] in the current code on `main` ·
![Planned][planned] not implemented yet

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

There is no reading pane toolbar today. The Availability column names where an
equivalent action exists.

| Action                       | Shortcut                                                     | Availability                                                       | Notes                                                                    |
| ---------------------------- | ------------------------------------------------------------ | ------------------------------------------------------------------ | ------------------------------------------------------------------------ |
| Reply                        | <kbd>Ctrl</kbd>+<kbd>R</kbd>                                 | ![Exists][exists] **Message** menu, navigation toolbar             | **Reply by Guess**                                                       |
| Reply all                    | <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd> | ![Exists][exists] **Message** menu, navigation toolbar             | **Reply to All**                                                         |
| Forward                      | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F</kbd>                | ![Exists][exists] **Message** menu, navigation toolbar             | Forwards as an attachment                                                |
| Mark as read                 | <kbd>M</kbd>                                                 | ![Exists][exists] Navigation toolbar, message list context menu    |                                                                          |
| Flag                         | <kbd>S</kbd>                                                 | ![Exists][exists] Navigation toolbar, message list context menu    | **Mark as Flagged**                                                      |
| Archive                      | <kbd>A</kbd>                                                 | ![Exists][exists] Navigation toolbar, message list context menu    |                                                                          |
| Open source                  | —                                                            | ![Exists][exists] Message list context menu                        | **View Message Source…**                                                 |
| Show details                 | <kbd>Ctrl</kbd>+<kbd>U</kbd>                                 | ![Exists][exists] Message list context menu, Message Details panel | **View Message Headers…**                                                |
| Previous message             | —                                                            | ![Planned][planned]                                                | **Mailbox › Previous Unread Message** (<kbd>P</kbd>) skips read messages |
| Next message                 | —                                                            | ![Planned][planned]                                                | **Mailbox › Next Unread Message** (<kbd>N</kbd>) skips read messages     |
| Return to list               | <kbd>Alt</kbd>+<kbd>Left</kbd>                               | ![Exists][exists] Navigation toolbar, One At a Time layout only    | **Navigate Back**                                                        |
| Move to folder               | —                                                            | ![Exists][exists] Drag and drop onto the folder tree               | From the message list                                                    |
| Delete                       | <kbd>Delete</kbd>                                            | ![Exists][exists] Navigation toolbar, message list context menu    | **Mark as Deleted**                                                      |
| Print                        | —                                                            | ![Planned][planned]                                                |                                                                          |
| Save attachment(s)           | —                                                            | ![Exists][exists] Attachments panel, reader context menu           | One attachment at a time                                                 |
| Share / external action menu | —                                                            | ![Planned][planned]                                                |                                                                          |

## Behavioral rules

- The toolbar should remain focused on the currently displayed message and
  nearby reading flow.
- It should not duplicate every folder or bulk-action command from the message
  list.
- The most common actions should remain close to the reading pane and visually
  clear.

[exists]: https://img.shields.io/badge/Exists-1a7f37
[planned]: https://img.shields.io/badge/Planned-6e7781
