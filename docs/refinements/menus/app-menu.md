# Application menu tree

**[Documentation](../../README.md)** · **[Roadmap: M13](../../ROADMAP.md#m13-refinements)** ·
**[Refinements](../README.md)** · **[Menus](README.md)** ·
**[Toolbars](../toolbars/README.md)** · **[Interface design](../../philosophy/designs.md)**

This page defines the proposed top-level menu bar structure for Plasma
Impresario and maps each entry to the action that exists today, if any.

Legend: ![Exists][exists] in the current code on `main`, not checked in a real
Plasma session · ![Planned][planned] not implemented yet

## Contents

- [Purpose](#purpose)
- [Menu tree](#menu-tree)
- [Actions](#actions)
  - [File](#file)
  - [Edit](#edit)
  - [View](#view)
  - [Go](#go)
  - [Message](#message)
  - [Folder](#folder)
  - [Account](#account)
  - [Tools](#tools)
  - [Settings](#settings)
  - [Help](#help)
- [Current menu](#current-menu)
- [Design rules](#design-rules)

## Purpose

The menu bar is the complete, discoverable home for global commands. Every
toolbar button and common context-menu action should also be reachable here,
grouped by task, so keyboard and menu users never depend on a single surface.

## Menu tree

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
      └─ About Plasma Impresario
```

## Actions

The Availability column names where an equivalent action exists today. Menu
paths such as **Application › Exit** refer to the
[current menu bar](#current-menu).

### File

| Action                               | Shortcut                          | Availability                                                     | Notes                                                                         |
| ------------------------------------ | --------------------------------- | ---------------------------------------------------------------- | ----------------------------------------------------------------------------- |
| New message                          | <kbd>Ctrl</kbd>+<kbd>N</kbd>      | ![Exists][exists] **Message › New Message…**, navigation toolbar |                                                                               |
| New draft                            | —                                 | ![Planned][planned]                                              | **Message › Edit Draft…** opens a saved draft                                 |
| New folder                           | —                                 | ![Exists][exists] Folder tree context menu                       | **Create Child Mailbox…** on a folder, **Create New Mailbox…** on empty space |
| New account                          | —                                 | ![Exists][exists] **Application › Add Account…**                 |                                                                               |
| Open message in new window           | <kbd>Ctrl</kbd>+<kbd>Return</kbd> | ![Exists][exists] Shortcut only, in the message list             | **Open message in New Window…**                                               |
| Open selected attachment             | —                                 | ![Exists][exists] Attachments panel                              | **Open Attachment**                                                           |
| Open folder in external file manager | —                                 | ![Planned][planned]                                              |                                                                               |
| Save draft                           | —                                 | ![Exists][exists] Composer                                       | Closing an unsent message asks **Save Draft?**                                |
| Save attachment as                   | —                                 | ![Exists][exists] Attachments panel, reader context menu         | **Save Attachment As…**, **Save this message part…**                          |
| Close current window                 | —                                 | ![Exists][exists] Standalone message window                      | **Close Standalone Message Window** has no default shortcut                   |
| Close current message tab            | —                                 | ![Planned][planned]                                              | Tabs are an optional proposal                                                 |
| Quit                                 | <kbd>Ctrl</kbd>+<kbd>Q</kbd>      | ![Exists][exists] **Application › Exit**                         |                                                                               |

### Edit

| Action               | Shortcut                                      | Availability                                                                  | Notes                                                          |
| -------------------- | --------------------------------------------- | ----------------------------------------------------------------------------- | -------------------------------------------------------------- |
| Undo                 | <kbd>Ctrl</kbd>+<kbd>Z</kbd>                  | ![Exists][exists] Composer editor context menu                                | Standard Qt text editing                                       |
| Redo                 | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Z</kbd> | ![Exists][exists] Composer editor context menu                                | Standard Qt text editing                                       |
| Cut                  | <kbd>Ctrl</kbd>+<kbd>X</kbd>                  | ![Exists][exists] Composer editor context menu                                | Standard Qt text editing                                       |
| Copy                 | <kbd>Ctrl</kbd>+<kbd>C</kbd>                  | ![Exists][exists] Reader and composer context menus                           |                                                                |
| Paste                | <kbd>Ctrl</kbd>+<kbd>V</kbd>                  | ![Exists][exists] Composer editor context menu                                | **Paste as Quoted Text** is also available                     |
| Select all           | <kbd>Ctrl</kbd>+<kbd>A</kbd>                  | ![Exists][exists] Reader and composer context menus, **Message › Select All** | The Message menu entry selects all messages in the list        |
| Find in message      | <kbd>Ctrl</kbd>+<kbd>F</kbd>                  | ![Exists][exists] Reader context menu                                         | **Search…** opens the find bar                                 |
| Find in folder       | <kbd>/</kbd>                                  | ![Exists][exists] Shortcut only                                               | **Move Focus to Quick Search**                                 |
| Preferences          | —                                             | ![Exists][exists] **Application › Settings…**                                 |                                                                |
| Account settings     | —                                             | ![Exists][exists] **Application › Account Settings…**, account header menu    | Identities, IMAP, Offline and SMTP pages of the active account |
| Application settings | —                                             | ![Exists][exists] **Application › Settings…**                                 | General and Favorite tags pages                                |

### View

| Action                  | Shortcut                     | Availability                                                                 | Notes                                                                                               |
| ----------------------- | ---------------------------- | ---------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| Show account sidebar    | —                            | ![Planned][planned]                                                          | With two or more accounts the folder sidebar shows a header per account; there is no toggle         |
| Show folder tree        | —                            | ![Planned][planned]                                                          | **View › Folder Sidebar Position** moves it left or right                                           |
| Show message list       | —                            | ![Planned][planned]                                                          |                                                                                                     |
| Show message preview    | —                            | ![Planned][planned]                                                          | **View › Layout › One At a Time** shows one pane at a time                                          |
| Toggle compact mode     | —                            | ![Planned][planned]                                                          | **View › Layout**, **View › Row Density** and **View › Dense Processing Layout** cover part of this |
| Toggle comfortable mode | —                            | ![Planned][planned]                                                          | **View › Row Density › Comfortable** sets row spacing only                                          |
| Toggle focus mode       | —                            | ![Planned][planned]                                                          |                                                                                                     |
| Sort by date            | —                            | ![Exists][exists] **Mailbox › Sorting**                                      | **Date from Message Headers**; **Arrival** is also available                                        |
| Sort by subject         | —                            | ![Exists][exists] **Mailbox › Sorting**                                      | **Subject**                                                                                         |
| Sort by sender          | —                            | ![Exists][exists] **Mailbox › Sorting**                                      | **From Address**                                                                                    |
| Sort unread first       | —                            | ![Planned][planned]                                                          | **Hide Read Messages** filters instead of sorting; **View › Group By › Read State** groups by it    |
| Group by conversation   | —                            | ![Exists][exists] **Mailbox › Sorting**, **View › Group By**                 | **Show Messages in Threads** or **Group By › Conversation**                                         |
| Show flat list          | —                            | ![Exists][exists] **Mailbox › Sorting**                                      | Clear **Show Messages in Threads**                                                                  |
| Expand all threads      | <kbd>Ctrl</kbd>+<kbd>.</kbd> | ![Exists][exists] **View › Group By**                                        | **Expand All Conversations**; **Expand All Groups** also exists                                     |
| Collapse all threads    | <kbd>Ctrl</kbd>+<kbd>,</kbd> | ![Exists][exists] **View › Group By**                                        | **Collapse All Conversations**; **Collapse All Groups** also exists                                 |
| Refresh current folder  | <kbd>F5</kbd>                | ![Exists][exists] **Mailbox › Check for New Mail**, folder tree context menu | **Check for New Messages** in the folder tree and **Application › Debugging**                       |
| Reload account          | —                            | ![Exists][exists] Folder tree context menu, **Application › Debugging**      | **Reload Everything**                                                                               |

### Go

| Action                  | Shortcut                                    | Availability                                             | Notes                                                                                       |
| ----------------------- | ------------------------------------------- | -------------------------------------------------------- | ------------------------------------------------------------------------------------------- |
| Next unread message     | <kbd>N</kbd>                                | ![Exists][exists] **Mailbox** menu                       | Present only in builds with the WebEngine reader                                            |
| Previous unread message | <kbd>P</kbd>                                | ![Exists][exists] **Mailbox** menu                       | Present only in builds with the WebEngine reader                                            |
| Next message            | <kbd>F</kbd>                                | ![Exists][exists] **Mailbox** menu                       | WebEngine builds only; **Next Message in Conversation** is <kbd>Alt</kbd>+<kbd>Down</kbd>   |
| Previous message        | <kbd>B</kbd>                                | ![Exists][exists] **Mailbox** menu                       | WebEngine builds only; **Previous Message in Conversation** is <kbd>Alt</kbd>+<kbd>Up</kbd> |
| Next folder             | <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>N</kbd> | ![Exists][exists] **Mailbox › Go to Next Unread Folder** | Skips folders without unread mail                                                           |
| Previous folder         | —                                           | ![Planned][planned]                                      |                                                                                             |
| Jump to search          | <kbd>/</kbd>                                | ![Exists][exists] Shortcut only                          | The key sequence <kbd>:</kbd> <kbd>=</kbd> focuses it for a raw IMAP query                  |
| Jump to drafts          | —                                           | ![Exists][exists] **Mailbox › Go to Folder › Drafts**    | **Go to Folder…** (<kbd>Ctrl</kbd>+<kbd>J</kbd>) picks any folder                           |
| Jump to sent            | —                                           | ![Exists][exists] **Mailbox › Go to Folder › Sent**      |                                                                                             |

### Message

| Action             | Shortcut                                                     | Availability                                                                               | Notes                                                                                                                             |
| ------------------ | ------------------------------------------------------------ | ------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------- |
| Reply              | <kbd>Ctrl</kbd>+<kbd>R</kbd>                                 | ![Exists][exists] **Message › Reply by Guess**, navigation toolbar                         | The toolbar Reply button has a reply-mode drop-down; **Private Reply** is <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>A</kbd>           |
| Reply all          | <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd> | ![Exists][exists] **Message › Reply to All**, navigation toolbar                           | **Reply to All but Me** uses <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd>                                                        |
| Forward            | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F</kbd>                | ![Exists][exists] **Message › Forward**, navigation toolbar                                | Forwards as an attachment                                                                                                         |
| Redirect           | —                                                            | ![Exists][exists] **Message › Resend…**                                                    |                                                                                                                                   |
| Edit as new        | —                                                            | ![Planned][planned]                                                                        |                                                                                                                                   |
| Mark as read       | <kbd>M</kbd>                                                 | ![Exists][exists] **Message** menu, navigation toolbar, message list context menu          | **Mark as Read** is a toggle                                                                                                      |
| Mark as unread     | <kbd>Shift</kbd>+<kbd>M</kbd>                                | ![Exists][exists] **Message › Mark as Unread**, message list context menu                  | Clearing the **Mark as Read** toggle also works                                                                                   |
| Flag               | <kbd>S</kbd>                                                 | ![Exists][exists] **Message** menu, navigation toolbar, message list context menu          | **Mark as Flagged** is a toggle                                                                                                   |
| Unflag             | <kbd>S</kbd>                                                 | ![Exists][exists] **Message** menu, navigation toolbar, message list context menu          | Clear the **Mark as Flagged** toggle                                                                                              |
| Move to folder     | <kbd>V</kbd>                                                 | ![Exists][exists] **Message › Move to Folder…**, **Message › Move To**, navigation toolbar | **Move To** lists recent folders; drag and drop onto the folder tree also works                                                   |
| Copy to folder     | <kbd>Shift</kbd>+<kbd>V</kbd>                                | ![Exists][exists] **Message › Copy to Folder…**, **Message › Copy To**                     | Drag and drop with <kbd>Ctrl</kbd> also copies                                                                                    |
| Archive            | <kbd>A</kbd>                                                 | ![Exists][exists] **Message** menu, navigation toolbar, message list context menu          | Moves to the account's archive folder                                                                                             |
| Delete             | <kbd>Delete</kbd>                                            | ![Exists][exists] **Message** menu, navigation toolbar, message list context menu          | **Mark as Deleted**; **Delete Permanently…** is <kbd>Shift</kbd>+<kbd>Delete</kbd>; **Mailbox › Expunge** removes marked messages |
| Print              | —                                                            | ![Planned][planned]                                                                        |                                                                                                                                   |
| View source        | —                                                            | ![Exists][exists] Message list context menu                                                | **View Message Source…**                                                                                                          |
| Open original link | —                                                            | ![Planned][planned]                                                                        |                                                                                                                                   |

### Folder

| Action            | Shortcut | Availability                                                                    | Notes                                                                                                                           |
| ----------------- | -------- | ------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------- |
| Refresh folder    | —        | ![Exists][exists] Folder tree context menu                                      | **Check for New Messages**                                                                                                      |
| New folder        | —        | ![Exists][exists] Folder tree context menu                                      | **Create Child Mailbox…**                                                                                                       |
| Rename folder     | —        | ![Exists][exists] **Mailbox › Rename Folder…**, folder tree context menu        | Enabled when the folder can be renamed                                                                                          |
| Delete folder     | —        | ![Exists][exists] Folder tree context menu                                      | **Remove Mailbox** asks for confirmation                                                                                        |
| Subscribe         | —        | ![Exists][exists] Folder tree context menu                                      | **Subscribed** checkbox                                                                                                         |
| Unsubscribe       | —        | ![Exists][exists] Folder tree context menu                                      | Clear the **Subscribed** checkbox                                                                                               |
| Move folder       | —        | ![Planned][planned]                                                             |                                                                                                                                 |
| Empty trash       | —        | ![Exists][exists] **Mailbox › Empty Trash…**, folder tree context menu on Trash | **Empty Junk…** does the same for Junk; **Mailbox › Expunge** (<kbd>Ctrl</kbd>+<kbd>E</kbd>) removes messages marked as deleted |
| Folder properties | —        | ![Planned][planned]                                                             |                                                                                                                                 |

### Account

| Action           | Shortcut | Availability                                                               | Notes                                                                                                                         |
| ---------------- | -------- | -------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| Add account      | —        | ![Exists][exists] **Application › Add Account…**                           |                                                                                                                               |
| Remove account   | —        | ![Exists][exists] **Application › Remove Account…**, account header menu   | Asks for confirmation; needs at least two accounts; server mail is kept                                                       |
| Edit account     | —        | ![Exists][exists] **Application › Rename Account…**, **Account Settings…** | Rename changes the display name; Account Settings edits the active account                                                    |
| Account settings | —        | ![Exists][exists] **Application › Account Settings…**, account header menu | Identities, IMAP, Offline and SMTP pages; titled with the account name                                                        |
| Synchronize now  | —        | ![Exists][exists] Folder tree context menu, **Application › Debugging**    | **Reload Everything**                                                                                                         |
| Pause sync       | —        | ![Exists][exists] **Application › Network Access › Offline**               | Also in the toolbar network indicator                                                                                         |
| Resume sync      | —        | ![Exists][exists] **Application › Network Access**                         | **Free Access** or **Expensive Connection**                                                                                   |
| Reconnect        | —        | ![Planned][planned]                                                        | No dedicated action; **Network Access** controls the connection; OAuth accounts offer **Sign In…** in the account header menu |
| Export settings  | —        | ![Planned][planned]                                                        |                                                                                                                               |

### Tools

| Action                  | Shortcut     | Availability                                      | Notes                                                                                       |
| ----------------------- | ------------ | ------------------------------------------------- | ------------------------------------------------------------------------------------------- |
| Search messages         | <kbd>/</kbd> | ![Exists][exists] Quick search field              | Searches subject, body, senders or recipients                                               |
| Saved searches          | —            | ![Exists][exists] Quick search options menu       | Per account: **Save Current Search…**, run or delete                                        |
| Rules                   | —            | ![Planned][planned]                               |                                                                                             |
| Filters                 | —            | ![Planned][planned]                               | **View › Filter** offers quick Unread, Flagged and With Attachments filters, not mail rules |
| Attachment safety check | —            | ![Planned][planned]                               |                                                                                             |
| Spam analysis           | —            | ![Planned][planned]                               | **Mark as Junk** and **Mark as Not Junk** classify messages by hand                         |
| Address book            | —            | ![Exists][exists] **Application › Address Book…** |                                                                                             |
| Calendar integration    | —            | ![Planned][planned]                               |                                                                                             |
| Notes capture           | —            | ![Planned][planned]                               |                                                                                             |

### Settings

| Action                               | Shortcut | Availability                                             | Notes                                                                  |
| ------------------------------------ | -------- | -------------------------------------------------------- | ---------------------------------------------------------------------- |
| Configure notifications              | —        | ![Planned][planned]                                      |                                                                        |
| Configure keyboard shortcuts         | —        | ![Exists][exists] **Application › Configure Shortcuts…** |                                                                        |
| Configure appearance                 | —        | ![Planned][planned]                                      | Layout, sidebar position, row density and message rows are in **View** |
| Configure privacy and remote content | —        | ![Planned][planned]                                      | The reader offers a per-message **Load Remote Content** bar            |
| Configure integrations               | —        | ![Planned][planned]                                      |                                                                        |
| Configure workspaces                 | —        | ![Planned][planned]                                      |                                                                        |

### Help

| Action                  | Shortcut     | Availability                                                 | Notes                                                  |
| ----------------------- | ------------ | ------------------------------------------------------------ | ------------------------------------------------------ |
| User guide              | —            | ![Planned][planned]                                          |                                                        |
| Keyboard shortcuts      | <kbd>?</kbd> | ![Exists][exists] **Help › Keyboard Shortcuts**              | Shows an overview; **Configure Shortcuts…** edits them |
| Debug information       | —            | ![Exists][exists] **Application › Debugging**                | Protocol log, task and MIME trees, server information  |
| Report bug              | —            | ![Planned][planned]                                          |                                                        |
| Check for updates       | —            | ![Planned][planned]                                          |                                                        |
| About Plasma Impresario | —            | ![Exists][exists] **Application › About Plasma Impresario…** |                                                        |

## Current menu

The menu bar is built in `MainWindow::createMenus()` in `src/Gui/Window.cpp`. It
is hidden by default; **View › Show Main Menu Bar**
(<kbd>Ctrl</kbd>+<kbd>M</kbd>) shows it. While it is hidden, the menu button at
the end of the navigation toolbar opens the same five menus.

| Menu        | Entries                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| ----------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Application | Address Book…, Network Access (Offline, Expensive Connection, Free Access), Debugging, Add Account…, Account Settings…, Rename Account…, Remove Account…, Settings…, Configure Shortcuts…, About Plasma Impresario…, Exit                                                                                                                                                                                                                       |
| View        | Show Main Menu Bar, Show Toolbar, Show Header, Layout (Compact, Wide, One At a Time), Folder Sidebar Position (Left, Right), Row Density (Compact, Comfortable), Message Rows (Two Lines, Columns, Show Sender Initials), Embedded Panels, Dense Processing Layout, Unified Inbox, Unified Inbox Accounts, Filter, Group By, Use This View for All Folders, Reset View for This Folder, Show Only Subscribed Folders                            |
| Mailbox     | Go to Folder (Inbox, Drafts, Sent, Archive, Junk, Trash), Go to Folder…, Go to Next Unread Folder, Check for New Mail, Sorting, Previous and Next Unread Message, Previous and Next Message, Previous and Next Message in Conversation, Expunge, Rename Folder…, Empty Trash…, Empty Junk…                                                                                                                                                      |
| Message     | New Message…, Edit Draft…, Reply by Guess, Private Reply, Reply to All, Reply to All but Me, Reply to Mailing List, Forward, Resend…, Mark as Deleted, Delete Permanently…, Move Deleted Messages to Trash, Mark as Read, Mark as Unread, Mark as Read and Go to Next Unread, Mark as Flagged, Archive, Move To, Copy To, Move to Folder…, Copy to Folder…, Select All, Select Thread, Invert Selection, Deselect, Select Similar, Show Related |
| Help        | Keyboard Shortcuts                                                                                                                                                                                                                                                                                                                                                                                                                              |

Previous and Next Message, Previous and Next Unread Message, and **Mark as
Read and Go to Next Unread** appear only in builds with the WebEngine reader.

<details><summary>Menu details and shortcut-only actions</summary>

**Application › Debugging** holds Show Full Tree Window, Show ImapTask tree,
Show MIME tree, Show protocol log, Log into …, IMAP Server Information…,
Reload Everything and Check for New Messages.

**View › Group By** offers None, Conversation, Date, Sender, Subject, Tag, Read
State and Mailing List, then Expand All Groups, Collapse All Groups, Expand All
Conversations (<kbd>Ctrl</kbd>+<kbd>.</kbd>) and Collapse All Conversations
(<kbd>Ctrl</kbd>+<kbd>,</kbd>). **View › Filter** offers Unread, Flagged, With
Attachments and Clear Filters. **View › Unified Inbox** uses
<kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>I</kbd>.

**Mailbox › Sorting** offers No sorting, Sorted by Threading, Arrival, Cc
(Carbon Copy), Date from Message Headers, From Address, Size, Subject, To
Address and Relevance (enabled only with a search or Show Related), then
Ascending, Descending, Show Messages in Threads, Group By, Hide Read Messages,
Use This View for All Folders and Reset View for This Folder.

**Message › Select Similar** offers Select All from This Sender, Select All
with This Subject, Select Unread and Select Flagged. **Message › Show Related**
offers Messages from This Sender, to This Recipient, with This Subject and from
This Mailing List.

Some actions have shortcuts but no menu entry:

- **Move Focus to Quick Search** (<kbd>/</kbd>)
- **Move Focus to Next Pane** (<kbd>F6</kbd>) and **Move Focus to Previous
  Pane** (<kbd>Shift</kbd>+<kbd>F6</kbd>)
- **Move Focus to Mailbox List**, **Message List** and **Message View**, with
  no default key
- **Switch to Next Account** (<kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>PgDown</kbd>)
  and **Switch to Previous Account**
  (<kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>PgUp</kbd>)
- **View Message Headers…** (<kbd>Ctrl</kbd>+<kbd>U</kbd>)
- **Open message in New Window…** (<kbd>Ctrl</kbd>+<kbd>Return</kbd>)
- **Tag with 1st tag** through **Tag with 9th tag** (<kbd>1</kbd>–<kbd>9</kbd>)
- **Navigate Back** (the platform Back key, <kbd>Alt</kbd>+<kbd>Left</kbd> on
  Plasma) in the One At a Time layout; <kbd>Esc</kbd> from the reader or a
  panel returns to the message list

</details>

## Design rules

- Global commands must remain in the app bar; account-scoped or message-scoped
  commands should stay in context menus.
- The most common actions for mail reading must be visible in both the menu and
  the message toolbar.
- Safety-critical actions such as delete, move, and send must be explicit and
  recoverable.
- The menu tree should stay stable as new mail features are added; new actions
  should fit into the correct grouping instead of creating a catch-all list.

[exists]: https://img.shields.io/badge/Exists-1a7f37
[planned]: https://img.shields.io/badge/Planned-6e7781
