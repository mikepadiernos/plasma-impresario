# Main window toolbar

This toolbar sits in the primary desktop shell and supports global mailbox actions.

```text
└─ Main window toolbar
   ├─ Left side
   │  ├─ Account selector / account switcher
   │  ├─ Refresh all accounts
   │  ├─ New message
   │  ├─ New folder
   │  └─ Search
   ├─ Center
   │  ├─ Folder navigation / back / forward
   │  ├─ View mode toggle
   │  └─ Compact / comfortable / focus mode
   └─ Right side
      ├─ Sync status indicator
      ├─ Notifications indicator
      ├─ Account settings
      ├─ Application settings
      └─ Help / keyboard shortcuts
```

## Behavioral rules
- Keep the left side focused on navigation and creation.
- Keep the center compact and stable across the mail workflow.
- Keep the right side for status and maintenance, not for core message actions.
- The toolbar should remain usable even when the account tree and message list are hidden.

## Purpose

This is the main desktop-level action surface for the app shell. It should feel like a compact, trusted mail toolbar rather than a dense command dock.
