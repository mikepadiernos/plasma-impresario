# Message header context menu

This menu appears in the message header row or metadata region for the currently selected message.

```text
└─ Message header context menu
   ├─ Open in new window
   ├─ Reply
   ├─ Reply all
   ├─ Forward
   ├─ View source
   ├─ Show details
   ├─ Expand / collapse headers
   ├─ Copy sender
   ├─ Copy recipients
   ├─ Copy subject
   ├─ Copy message ID
   ├─ Add sender to contacts
   ├─ Add recipient to contacts
   ├─ Move to folder
   ├─ Archive
   ├─ Mark as read
   └─ Flag
```

## Constraints
- Header actions should be a compact subset of the message list menu.
- Context should remain specific to the currently selected message and its metadata.
- Security-sensitive actions such as opening external links or viewing raw source should be explicit and clearly labeled.

## Purpose

This is the compact metadata and quick-action menu for the message header, designed for fast, low-fuss mail processing without overwhelming the reading pane.
