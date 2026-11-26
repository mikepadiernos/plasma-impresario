# Message list toolbar

**[Documentation](../../README.md)** · **[Roadmap: M14](../../ROADMAP.md#m14-refinements)** ·
**[Refinements](../README.md)** · **[Toolbars](README.md)** ·
**[Menus](../menus/README.md)** · **[Interface design](../../philosophy/designs.md)**

This proposed toolbar appears when the main message list is active and supports
triage and selection actions. It maps each entry to the message list controls
that exist today.

Legend: ![Exists][exists] in the current code on `main`, not checked in a real
Plasma session · ![Planned][planned] not implemented yet

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

The Availability column names where an equivalent action exists today.

| Action                         | Shortcut                      | Availability                                                         | Notes                                                                         |
| ------------------------------ | ----------------------------- | -------------------------------------------------------------------- | ----------------------------------------------------------------------------- |
| Select all / clear selection   | <kbd>Ctrl</kbd>+<kbd>A</kbd>  | ![Exists][exists] Message list                                       | **Select All** and **Deselect** while the list has focus                      |
| Mark selected as read          | <kbd>M</kbd>                  | ![Exists][exists] Navigation toolbar, message list context menu      | **Mark as Read** toggle                                                       |
| Mark selected as unread        | <kbd>Shift</kbd>+<kbd>M</kbd> | ![Exists][exists] **Message** menu, message list context menu        | **Mark as Unread**; clearing the **Mark as Read** toggle also works           |
| Flag selected                  | <kbd>S</kbd>                  | ![Exists][exists] Navigation toolbar, message list context menu      | **Mark as Flagged** toggle                                                    |
| Archive selected               | <kbd>A</kbd>                  | ![Exists][exists] Navigation toolbar, message list context menu      |                                                                               |
| Delete selected                | <kbd>Delete</kbd>             | ![Exists][exists] Navigation toolbar, message list context menu      | **Mark as Deleted**                                                           |
| Sort by date                   | —                             | ![Exists][exists] Tool row **Sort** button, **Mailbox › Sorting**    | **Date from Message Headers** or **Arrival**                                  |
| Sort by sender                 | —                             | ![Exists][exists] Tool row **Sort** button, **Mailbox › Sorting**    | **From Address**                                                              |
| Sort by subject                | —                             | ![Exists][exists] Tool row **Sort** button, **Mailbox › Sorting**    | **Subject**                                                                   |
| Sort by unread                 | —                             | ![Planned][planned]                                                  | The **Unread** filter and **Hide Read Messages** filter instead of sorting    |
| Threading toggle               | —                             | ![Exists][exists] Tool row **Sort** button, **Mailbox › Sorting**    | **Show Messages in Threads**                                                  |
| Filter / saved search selector | <kbd>/</kbd>                  | ![Exists][exists] Tool row filter buttons, quick search options menu | **Unread**, **Flagged**, **With Attachments**; **Saved Searches** per account |
| Search results count indicator | —                             | ![Exists][exists] Status row                                         | "showing X of Y" while filters are on                                         |
| Refresh folder                 | —                             | ![Exists][exists] Folder tree context menu                           | **Check for New Messages**                                                    |
| Folder properties              | —                             | ![Planned][planned]                                                  |                                                                               |
| Move selected to folder        | <kbd>V</kbd>                  | ![Exists][exists] **Message** menu, navigation toolbar               | **Move to Folder…**; drag and drop onto the folder tree also works            |
| Bulk actions menu              | —                             | ![Planned][planned]                                                  | The message list context menu acts on the whole selection                     |

## Current toolbar

The message list (`src/Gui/MessageListWidget.cpp`) has a tool row above the
list and a status row below it. The tool row holds:

- the **Quick Search** field. <kbd>/</kbd> focuses it, and the key sequence
  <kbd>:</kbd> <kbd>=</kbd> focuses it with the `:=` prefix for a raw IMAP
  query.
- filter buttons **Unread**, **Flagged** and **With Attachments**, plus
  **Clear Filters**. The filter state is saved per account.
- a **Sort** menu button with the same entries as **Mailbox › Sorting**.

The quick search options menu offers:

- **Fuzzy Search**
- **Save Current Search…** and a **Saved Searches** submenu for the active
  account, where each search offers **Run Search** and **Delete Search…**
- search fields **Subject**, **Body**, **Senders** and **Recipients**
- **Allow raw IMAP search**, which reveals the **Complex IMAP query** templates

The status row shows the scope as "Account › Folder", then the counts joined by
" · ": "N messages, N unread", "showing X of Y" while filters are on, and "N
selected" when more than one message is selected.

## Design rules

- Keep the toolbar aligned with the current folder and visible selection.
- Never expose actions that are not valid for the current selected messages.
- Preserve keyboard shortcut parity with the same actions in the menu system.

[exists]: https://img.shields.io/badge/Exists-1a7f37
[planned]: https://img.shields.io/badge/Planned-6e7781
