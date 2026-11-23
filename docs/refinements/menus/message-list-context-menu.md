# Message list context menu

This menu appears when the user right-clicks one or more rows in the message list.

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

## Safety and quality rules
- Multi-select menus should operate only on the selected messages and never on all messages in the current folder.
- Actions must reflect the current visible selection and not stale state after an async refresh.
- Destructive bulk actions should include a count and confirm before applying.

## Purpose

This menu supports the core triage workflow. It is the main high-density action surface for mailbox management and batch message operations.
