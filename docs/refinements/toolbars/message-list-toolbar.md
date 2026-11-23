# Message list toolbar

This toolbar appears when the main message list is active and supports triage and selection actions.

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

## Behavioral rules
- Keep the toolbar aligned with the current folder and visible selection.
- Never expose actions that are not valid for the current selected messages.
- Preserve keyboard shortcut parity with the same actions in the menu system.

## Purpose

This is the bulk-processing toolbar for the inbox and folder view. It should support fast triage while staying compact and predictable.
