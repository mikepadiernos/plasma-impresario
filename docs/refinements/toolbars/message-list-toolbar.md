# Message list toolbar

**[Refinements](../README.md)** · **[Toolbars](README.md)** ·
**[Interface design](../../philosophy/designs.md)** ·
**[M13 in roadmap](../../ROADMAP.md#m13-refinements)**

This proposed toolbar appears when the main message list is active and supports
triage and selection actions.

**Legend:** ✅ exists in the current code on `main` · 📋 planned only

## Purpose

This is the bulk-processing toolbar for the inbox and folder view. It should
support fast triage while staying compact and predictable.

## Toolbar tree

```text
└─ Message list toolbar
   ├─ Left side
   │  ├─ Select all / clear selection
   │  ├─ Mark selected as read
   │  ├─ Mark selected as unread
   │  ├─ Flag selected
   │  ├─ Archive selected
   │  └─ Delete selected
   ├─ Center
   │  ├─ Sort by
   │  │  ├─ Date
   │  │  ├─ Sender
   │  │  ├─ Subject
   │  │  └─ Unread
   │  ├─ Threading toggle
   │  ├─ Filter / saved search selector
   │  └─ Search results count indicator
   └─ Right side
      ├─ Refresh folder
      ├─ Folder properties
      ├─ Move selected to folder
      └─ Bulk actions menu
```

## Actions

There is no message list toolbar today. The Availability column names where an
equivalent action exists.

| Action                         | Shortcut                     | Availability                                     | Notes                                                     |
| ------------------------------ | ---------------------------- | ------------------------------------------------ | --------------------------------------------------------- |
| Select all / clear selection   | <kbd>Ctrl</kbd>+<kbd>A</kbd> | ✅ Message list                                  | Standard list selection                                   |
| Mark selected as read          | <kbd>M</kbd>                 | ✅ Navigation toolbar, message list context menu | **Mark as Read** toggle                                   |
| Mark selected as unread        | <kbd>M</kbd>                 | ✅ Navigation toolbar, message list context menu | Clear the **Mark as Read** toggle                         |
| Flag selected                  | <kbd>S</kbd>                 | ✅ Navigation toolbar, message list context menu | **Mark as Flagged** toggle                                |
| Archive selected               | <kbd>A</kbd>                 | ✅ Navigation toolbar, message list context menu |                                                           |
| Delete selected                | <kbd>Delete</kbd>            | ✅ Navigation toolbar, message list context menu | **Mark as Deleted**                                       |
| Sort by date                   | —                            | ✅ **Mailbox › Sorting**                         | **Sorted by Date from Message Headers** or **Arrival**    |
| Sort by sender                 | —                            | ✅ **Mailbox › Sorting**                         | **Sorted by From Address**                                |
| Sort by subject                | —                            | ✅ **Mailbox › Sorting**                         | **Sorted by Subject**                                     |
| Sort by unread                 | —                            | 📋 Planned                                       | **Hide Read Messages** filters instead of sorting         |
| Threading toggle               | —                            | ✅ **Mailbox › Sorting**                         | **Show Messages in Threads**                              |
| Filter / saved search selector | <kbd>/</kbd>                 | ✅ Quick search field and its options menu       | **Saved Searches** per account                            |
| Search results count indicator | —                            | 📋 Planned                                       |                                                           |
| Refresh folder                 | —                            | ✅ Folder tree context menu                      | **Check for New Messages**                                |
| Folder properties              | —                            | 📋 Planned                                       |                                                           |
| Move selected to folder        | —                            | ✅ Drag and drop onto the folder tree            |                                                           |
| Bulk actions menu              | —                            | 📋 Planned                                       | The message list context menu acts on the whole selection |

## Current quick search

The message list has a quick search field (`src/Gui/MessageListWidget.cpp`)
instead of a toolbar. <kbd>/</kbd> focuses it, and the key sequence <kbd>:</kbd>
<kbd>=</kbd> focuses it for a raw IMAP query. Its options menu offers:

- 🔍 **Fuzzy Search**
- 🔍 **Save Current Search…** and a **Saved Searches** submenu for the active
  account
- 🔍 search fields **Subject**, **Body**, **Senders** and **Recipients**
- 🔍 **Complex IMAP query** templates and **Allow raw IMAP search**

## Behavioral rules

- Keep the toolbar aligned with the current folder and visible selection.
- Never expose actions that are not valid for the current selected messages.
- Preserve keyboard shortcut parity with the same actions in the menu system.
