# Account tree context menu

This menu appears when the user right-clicks an account in the account/sidebar navigation tree.

```text
└─ Account context menu
   ├─ Open account
   ├─ Expand / collapse account tree
   ├─ Synchronize now
   ├─ Pause or resume sync
   ├─ Edit account
   ├─ Remove account
   ├─ Account settings
   ├─ Create folder
   ├─ Rename account
   ├─ Set as default account
   ├─ Show unread counts
   └─ Show account in sidebar
```

## Safety and quality rules
- Account deletion must require confirmation and show the number of folders, messages, and local data affected.
- Sync actions should remain visible even when the account is offline or broken.
- Account settings should open in a dedicated dialog with no ambiguous side effects.

## Purpose

This menu is about account lifecycle and maintenance. It should not contain message actions that belong in the message list or reading pane.
