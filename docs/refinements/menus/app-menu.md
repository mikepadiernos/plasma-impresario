# Application menu tree

**[Refinements](../README.md)** · **[Menus](README.md)** ·
**[Interface design](../../philosophy/designs.md)** ·
**[M14 in roadmap](../../ROADMAP.md#m14-refinements)**

This page defines the proposed top-level menu bar structure for Plasma
Impresario and maps each entry to the action that exists today, if any.

**Legend:** ![Exists][exists] in the current code on `main` ·
![Planned][planned] not implemented yet

## Contents

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
- [Current menu bar](#current-menu-bar)
- [Menu design rules](#menu-design-rules)

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
[current menu bar](#current-menu-bar).

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

| Action               | Shortcut                                      | Availability                                          | Notes                                                          |
| -------------------- | --------------------------------------------- | ----------------------------------------------------- | -------------------------------------------------------------- |
| Undo                 | <kbd>Ctrl</kbd>+<kbd>Z</kbd>                  | ![Exists][exists] Composer editor context menu        | Standard Qt text editing                                       |
| Redo                 | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Z</kbd> | ![Exists][exists] Composer editor context menu        | Standard Qt text editing                                       |
| Cut                  | <kbd>Ctrl</kbd>+<kbd>X</kbd>                  | ![Exists][exists] Composer editor context menu        | Standard Qt text editing                                       |
| Copy                 | <kbd>Ctrl</kbd>+<kbd>C</kbd>                  | ![Exists][exists] Reader and composer context menus   |                                                                |
| Paste                | <kbd>Ctrl</kbd>+<kbd>V</kbd>                  | ![Exists][exists] Composer editor context menu        | **Paste as Quoted Text** is also available                     |
| Select all           | <kbd>Ctrl</kbd>+<kbd>A</kbd>                  | ![Exists][exists] Reader and composer context menus   |                                                                |
| Find in message      | <kbd>Ctrl</kbd>+<kbd>F</kbd>                  | ![Exists][exists] Reader context menu                 | **Search…** opens the find bar                                 |
| Find in folder       | <kbd>/</kbd>                                  | ![Exists][exists] Shortcut only                       | **Move Focus to Quick Search**                                 |
| Preferences          | —                                             | ![Exists][exists] **Application › Settings…**         |                                                                |
| Account settings     | —                                             | ![Exists][exists] **Application › Account Settings…** | Identities, IMAP, Offline and SMTP pages of the active account |
| Application settings | —                                             | ![Exists][exists] **Application › Settings…**         | General and Favorite tags pages                                |

### View

| Action                  | Shortcut | Availability                                                            | Notes                                                                     |
| ----------------------- | -------- | ----------------------------------------------------------------------- | ------------------------------------------------------------------------- |
| Show account sidebar    | —        | ![Planned][planned]                                                     | Accounts switch through the toolbar selector; there is no account sidebar |
| Show folder tree        | —        | ![Planned][planned]                                                     | **View › Folder Sidebar Position** moves it left or right                 |
| Show message list       | —        | ![Planned][planned]                                                     |                                                                           |
| Show message preview    | —        | ![Planned][planned]                                                     | **View › Layout › One At a Time** shows one pane at a time                |
| Toggle compact mode     | —        | ![Planned][planned]                                                     | **View › Layout** and **View › Row Density** cover part of this           |
| Toggle comfortable mode | —        | ![Planned][planned]                                                     | **View › Row Density › Comfortable** sets row spacing only                |
| Toggle focus mode       | —        | ![Planned][planned]                                                     |                                                                           |
| Sort by date            | —        | ![Exists][exists] **Mailbox › Sorting**                                 | **Sorted by Date from Message Headers**; **Arrival** is also available    |
| Sort by subject         | —        | ![Exists][exists] **Mailbox › Sorting**                                 | **Sorted by Subject**                                                     |
| Sort by sender          | —        | ![Exists][exists] **Mailbox › Sorting**                                 | **Sorted by From Address**                                                |
| Sort unread first       | —        | ![Planned][planned]                                                     | **Hide Read Messages** filters instead of sorting                         |
| Group by conversation   | —        | ![Exists][exists] **Mailbox › Sorting**                                 | **Show Messages in Threads**                                              |
| Show flat list          | —        | ![Exists][exists] **Mailbox › Sorting**                                 | Clear **Show Messages in Threads**                                        |
| Expand all threads      | —        | ![Planned][planned]                                                     |                                                                           |
| Collapse all threads    | —        | ![Planned][planned]                                                     |                                                                           |
| Refresh current folder  | —        | ![Exists][exists] Folder tree context menu, **Application › Debugging** | **Check for New Messages**                                                |
| Reload account          | —        | ![Exists][exists] Folder tree context menu, **Application › Debugging** | **Reload Everything**                                                     |

### Go

| Action                  | Shortcut     | Availability                       | Notes                                                                      |
| ----------------------- | ------------ | ---------------------------------- | -------------------------------------------------------------------------- |
| Next unread message     | <kbd>N</kbd> | ![Exists][exists] **Mailbox** menu | Present only in builds with the WebEngine reader                           |
| Previous unread message | <kbd>P</kbd> | ![Exists][exists] **Mailbox** menu | Present only in builds with the WebEngine reader                           |
| Next message            | —            | ![Planned][planned]                |                                                                            |
| Previous message        | —            | ![Planned][planned]                |                                                                            |
| Next folder             | —            | ![Planned][planned]                |                                                                            |
| Previous folder         | —            | ![Planned][planned]                |                                                                            |
| Jump to search          | <kbd>/</kbd> | ![Exists][exists] Shortcut only    | The key sequence <kbd>:</kbd> <kbd>=</kbd> focuses it for a raw IMAP query |
| Jump to drafts          | —            | ![Planned][planned]                |                                                                            |
| Jump to sent            | —            | ![Planned][planned]                |                                                                            |

### Message

| Action             | Shortcut                                                     | Availability                                                       | Notes                                                                                                                    |
| ------------------ | ------------------------------------------------------------ | ------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------ |
| Reply              | <kbd>Ctrl</kbd>+<kbd>R</kbd>                                 | ![Exists][exists] **Message › Reply by Guess**, navigation toolbar | The toolbar Reply button picks a default per message; **Private Reply** is <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>A</kbd> |
| Reply all          | <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd> | ![Exists][exists] **Message › Reply to All**, navigation toolbar   | **Reply to All but Me** uses <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd>                                               |
| Forward            | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F</kbd>                | ![Exists][exists] **Message › Forward**, navigation toolbar        | Forwards as an attachment                                                                                                |
| Redirect           | —                                                            | ![Exists][exists] **Message › Resend…**                            |                                                                                                                          |
| Edit as new        | —                                                            | ![Planned][planned]                                                |                                                                                                                          |
| Mark as read       | <kbd>M</kbd>                                                 | ![Exists][exists] Navigation toolbar, message list context menu    | **Mark as Read** is a toggle                                                                                             |
| Mark as unread     | <kbd>M</kbd>                                                 | ![Exists][exists] Navigation toolbar, message list context menu    | Clear the **Mark as Read** toggle                                                                                        |
| Flag               | <kbd>S</kbd>                                                 | ![Exists][exists] Navigation toolbar, message list context menu    | **Mark as Flagged** is a toggle                                                                                          |
| Unflag             | <kbd>S</kbd>                                                 | ![Exists][exists] Navigation toolbar, message list context menu    | Clear the **Mark as Flagged** toggle                                                                                     |
| Move to folder     | —                                                            | ![Exists][exists] Drag and drop onto the folder tree               | Hold <kbd>Shift</kbd> to move without asking                                                                             |
| Copy to folder     | —                                                            | ![Exists][exists] Drag and drop onto the folder tree               | Hold <kbd>Ctrl</kbd> to copy without asking                                                                              |
| Archive            | <kbd>A</kbd>                                                 | ![Exists][exists] Navigation toolbar, message list context menu    | Moves to the account's archive folder                                                                                    |
| Delete             | <kbd>Delete</kbd>                                            | ![Exists][exists] Navigation toolbar, message list context menu    | **Mark as Deleted**; **Mailbox › Expunge** removes marked messages                                                       |
| Print              | —                                                            | ![Planned][planned]                                                |                                                                                                                          |
| View source        | —                                                            | ![Exists][exists] Message list context menu                        | **View Message Source…**                                                                                                 |
| Open original link | —                                                            | ![Planned][planned]                                                |                                                                                                                          |

### Folder

| Action            | Shortcut | Availability                               | Notes                                                                                   |
| ----------------- | -------- | ------------------------------------------ | --------------------------------------------------------------------------------------- |
| Refresh folder    | —        | ![Exists][exists] Folder tree context menu | **Check for New Messages**                                                              |
| New folder        | —        | ![Exists][exists] Folder tree context menu | **Create Child Mailbox…**                                                               |
| Rename folder     | —        | ![Planned][planned]                        |                                                                                         |
| Delete folder     | —        | ![Exists][exists] Folder tree context menu | **Remove Mailbox** asks for confirmation                                                |
| Subscribe         | —        | ![Exists][exists] Folder tree context menu | **Subscribed** checkbox                                                                 |
| Unsubscribe       | —        | ![Exists][exists] Folder tree context menu | Clear the **Subscribed** checkbox                                                       |
| Move folder       | —        | ![Planned][planned]                        |                                                                                         |
| Empty trash       | —        | ![Planned][planned]                        | **Mailbox › Expunge** (<kbd>Ctrl</kbd>+<kbd>E</kbd>) removes messages marked as deleted |
| Folder properties | —        | ![Planned][planned]                        |                                                                                         |

### Account

| Action           | Shortcut | Availability                                                               | Notes                                                                      |
| ---------------- | -------- | -------------------------------------------------------------------------- | -------------------------------------------------------------------------- |
| Add account      | —        | ![Exists][exists] **Application › Add Account…**                           |                                                                            |
| Remove account   | —        | ![Exists][exists] **Application › Remove Account…**                        | Asks for confirmation; needs at least two accounts; server mail is kept    |
| Edit account     | —        | ![Exists][exists] **Application › Rename Account…**, **Account Settings…** | Rename changes the display name; Account Settings edits the active account |
| Account settings | —        | ![Exists][exists] **Application › Account Settings…**                      | Identities, IMAP, Offline and SMTP pages; titled with the account name     |
| Synchronize now  | —        | ![Exists][exists] Folder tree context menu, **Application › Debugging**    | **Reload Everything**                                                      |
| Pause sync       | —        | ![Exists][exists] **Application › Network Access › Offline**               | Also in the toolbar network indicator                                      |
| Resume sync      | —        | ![Exists][exists] **Application › Network Access**                         | **Free Access** or **Expensive Connection**                                |
| Reconnect        | —        | ![Planned][planned]                                                        | No dedicated action; **Network Access** controls the connection            |
| Export settings  | —        | ![Planned][planned]                                                        |                                                                            |

### Tools

| Action                  | Shortcut     | Availability                                      | Notes                                                               |
| ----------------------- | ------------ | ------------------------------------------------- | ------------------------------------------------------------------- |
| Search messages         | <kbd>/</kbd> | ![Exists][exists] Quick search field              | Searches subject, body, senders or recipients                       |
| Saved searches          | —            | ![Exists][exists] Quick search options menu       | Per account: **Save Current Search…**, run or delete                |
| Rules                   | —            | ![Planned][planned]                               |                                                                     |
| Filters                 | —            | ![Planned][planned]                               |                                                                     |
| Attachment safety check | —            | ![Planned][planned]                               |                                                                     |
| Spam analysis           | —            | ![Planned][planned]                               | **Mark as Junk** and **Mark as Not Junk** classify messages by hand |
| Address book            | —            | ![Exists][exists] **Application › Address Book…** |                                                                     |
| Calendar integration    | —            | ![Planned][planned]                               |                                                                     |
| Notes capture           | —            | ![Planned][planned]                               |                                                                     |

### Settings

| Action                               | Shortcut | Availability                                             | Notes                                                           |
| ------------------------------------ | -------- | -------------------------------------------------------- | --------------------------------------------------------------- |
| Configure notifications              | —        | ![Planned][planned]                                      |                                                                 |
| Configure keyboard shortcuts         | —        | ![Exists][exists] **Application › Configure Shortcuts…** |                                                                 |
| Configure appearance                 | —        | ![Planned][planned]                                      | Layout, sidebar position and row density are in **View**        |
| Configure privacy and remote content | —        | ![Planned][planned]                                      | The reader offers a per-message **Load** bar for remote content |
| Configure integrations               | —        | ![Planned][planned]                                      |                                                                 |
| Configure workspaces                 | —        | ![Planned][planned]                                      |                                                                 |

### Help

| Action                  | Shortcut | Availability                                                 | Notes                                                 |
| ----------------------- | -------- | ------------------------------------------------------------ | ----------------------------------------------------- |
| User guide              | —        | ![Planned][planned]                                          |                                                       |
| Keyboard shortcuts      | —        | ![Planned][planned]                                          | **Configure Shortcuts…** lists and edits shortcuts    |
| Debug information       | —        | ![Exists][exists] **Application › Debugging**                | Protocol log, task and MIME trees, server information |
| Report bug              | —        | ![Planned][planned]                                          |                                                       |
| Check for updates       | —        | ![Planned][planned]                                          |                                                       |
| About Plasma Impresario | —        | ![Exists][exists] **Application › About Plasma Impresario…** |                                                       |

## Current menu bar

The menu bar is built in `MainWindow::createMenus()` in `src/Gui/Window.cpp`. It
is hidden by default; **View › Show Main Menu Bar**
(<kbd>Ctrl</kbd>+<kbd>M</kbd>) shows it. While it is hidden, the menu button at
the end of the navigation toolbar opens the same menus.

| Menu        | Entries                                                                                                                                                                                           |
| ----------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Application | Address Book…, Network Access, Debugging, Add Account…, Account Settings…, Rename Account…, Remove Account…, Settings…, Configure Shortcuts…, About Plasma Impresario…, Exit                      |
| View        | Show Main Menu Bar, Show Toolbar, Layout (Compact, Wide, One At a Time), Folder Sidebar Position (Left, Right), Row Density (Compact, Comfortable), Embedded Panels, Show Only Subscribed Folders |
| Mailbox     | Sorting (sort keys, ascending or descending, Show Messages in Threads, Hide Read Messages), Previous Unread Message, Next Unread Message, Expunge                                                 |
| Message     | New Message…, Edit Draft…, Reply by Guess, Private Reply, Reply to All, Reply to All but Me, Reply to Mailing List, Forward, Resend…                                                              |

Some actions have shortcuts but no menu entry: **Move Focus to Quick Search**
(<kbd>/</kbd>), **Move Focus to Next Pane** (<kbd>F6</kbd>) and **Move Focus to
Previous Pane** (<kbd>Shift</kbd>+<kbd>F6</kbd>), **Switch to Next Account**
(<kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>PgDown</kbd>) and **Switch to Previous
Account** (<kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>PgUp</kbd>), **View Message
Headers…** (<kbd>Ctrl</kbd>+<kbd>U</kbd>) and **Tag with 1st tag** through **Tag
with 9th tag** (<kbd>1</kbd>–<kbd>9</kbd>).

## Menu design rules

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
