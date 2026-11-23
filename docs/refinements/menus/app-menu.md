# Application menu tree

This page defines the top-level app menu structure for Plasma Mail.

```text
└─ Application menu
   ├─ File
   │  ├─ New
   │  │  ├─ New message
   │  │  ├─ New draft
   │  │  ├─ New folder
   │  │  └─ New account
   │  ├─ Open
   │  │  ├─ Open message in new window
   │  │  ├─ Open selected attachment
   │  │  └─ Open folder in external file manager
   │  ├─ Save
   │  │  ├─ Save draft
   │  │  └─ Save attachment as
   │  ├─ Close
   │  │  ├─ Close current window
   │  │  └─ Close current message tab
   │  └─ Quit
   ├─ Edit
   │  ├─ Undo
   │  ├─ Redo
   │  ├─ Cut
   │  ├─ Copy
   │  ├─ Paste
   │  ├─ Select all
   │  ├─ Find in message
   │  ├─ Find in folder
   │  ├─ Preferences
   │  ├─ Account settings
   │  └─ Application settings
   ├─ View
   │  ├─ Show account sidebar
   │  ├─ Show folder tree
   │  ├─ Show message list
   │  ├─ Show message preview
   │  ├─ Toggle compact mode
   │  ├─ Toggle comfortable mode
   │  ├─ Toggle focus mode
   │  ├─ Sort order
   │  │  ├─ Date
   │  │  ├─ Subject
   │  │  ├─ Sender
   │  │  └─ Unread first
   │  ├─ Threading
   │  │  ├─ Group by conversation
   │  │  ├─ Show flat list
   │  │  ├─ Expand all threads
   │  │  └─ Collapse all threads
   │  ├─ Refresh current folder
   │  └─ Reload account
   ├─ Go
   │  ├─ Next unread message
   │  ├─ Previous unread message
   │  ├─ Next message
   │  ├─ Previous message
   │  ├─ Next folder
   │  ├─ Previous folder
   │  ├─ Jump to search
   │  ├─ Jump to drafts
   │  └─ Jump to sent
   ├─ Message
   │  ├─ Reply
   │  ├─ Reply all
   │  ├─ Forward
   │  ├─ Redirect
   │  ├─ Edit as new
   │  ├─ Mark as read
   │  ├─ Mark as unread
   │  ├─ Flag
   │  ├─ Unflag
   │  ├─ Move to folder
   │  ├─ Copy to folder
   │  ├─ Archive
   │  ├─ Delete
   │  ├─ Print
   │  ├─ View source
   │  └─ Open original link
   ├─ Folder
   │  ├─ Refresh folder
   │  ├─ New folder
   │  ├─ Rename folder
   │  ├─ Delete folder
   │  ├─ Subscribe
   │  ├─ Unsubscribe
   │  ├─ Move folder
   │  ├─ Empty trash
   │  └─ Folder properties
   ├─ Account
   │  ├─ Add account
   │  ├─ Remove account
   │  ├─ Edit account
   │  ├─ Account settings
   │  ├─ Synchronize now
   │  ├─ Pause sync
   │  ├─ Resume sync
   │  ├─ Reconnect
   │  └─ Export settings
   ├─ Tools
   │  ├─ Search messages
   │  ├─ Saved searches
   │  ├─ Rules
   │  ├─ Filters
   │  ├─ Attachment safety check
   │  ├─ Spam analysis
   │  ├─ Address book
   │  ├─ Calendar integration
   │  └─ Notes capture
   ├─ Settings
   │  ├─ Configure notifications
   │  ├─ Configure keyboard shortcuts
   │  ├─ Configure appearance
   │  ├─ Configure privacy and remote content
   │  ├─ Configure integrations
   │  └─ Configure workspaces
   └─ Help
      ├─ User guide
      ├─ Keyboard shortcuts
      ├─ Debug information
      ├─ Report bug
      ├─ Check for updates
      └─ About Plasma Mail
```

## Menu design rules

- Global commands must remain in the app bar; account-scoped or message-scoped commands should stay in context menus.
- The most common actions for mail reading must be visible in both the menu and the message toolbar.
- Safety-critical actions such as delete, move, and send must be explicit and recoverable.
- The menu tree should stay stable as new mail features are added; new actions should fit into the correct grouping instead of creating a catch-all list.
