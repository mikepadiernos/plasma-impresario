# Plasma Mail interface design

This page captures the intended layout structure and illustration for Plasma Mail. It is intentionally lightweight and placeholder-based, but it keeps the same design-document structure expected for a KDE-style application workflow.

## Product intent

Plasma Mail should feel desktop-native, readable, and efficient for daily email use. The interface should balance account navigation, message triage, and contextual reading without overwhelming the user.

## Primary layout

The default layout is a four-region desktop composition:

- Header: global search, compose action, filter controls, and status indicators
- Left column: accounts, folders, and mail navigation
- Main column: message list, sorting, thread grouping, and selection state
- Right column: message preview, metadata, attachments, and actions
- Footer: sync status, connection state, and low-priority notifications

## Panel map

```text
┌──────────────────────────────────────────────────────────────────────────────────────┐
│ HEADER / TOOLBAR                                                                     │
│ [App menu] [Search mailbox] [New Message] [Filter] [Sync] [Status/Account]           │
├───────────────────────┬───────────────────────────────┬──────────────────────────────┤
│ LEFT COLUMN           │ MAIN COLUMN                   │ RIGHT COLUMN                 │
│                       │                               │                              │
│ Accounts              │ Message list                  │ Message preview              │
│ - Inbox               │ - Sender / Subject / Time     │ - Sender / Recipients        │
│ - Sent                │ - Unread badges               │ - Subject                    │
│ - Drafts              │ - Threaded / flat mode        │ - Body content               │
│ - Archive             │ - Selection state             │ - Attachments                │
│ - Trash               │ - Sort controls               │ - Quick actions              │
│ Folder filters        │                               │                              │
│ Search saved items    │                               │                              │
├───────────────────────┼───────────────────────────────┼──────────────────────────────┤
│ FOOTER / STATUS BAR                                                                  │
│ Sync status | connection state | unread count | background task / alert messages     │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

## Pane definitions

### Header pane

- Global search field and filter controls
- Compose, reply, forward, and archive actions
- Account and status chip selectors
- Window-level actions such as sync, refresh, and settings

### Left navigation pane

- Account list and identities
- Folder hierarchy, labels, and smart folders
- Unread counts, star states, and quick filters
- Optional compact or collapsed mode for narrower layouts

### Main content pane

- Message list with sender, subject, date, and thread state
- Sorting and grouping controls
- Selection, highlight, and read/unread status indicators
- Keyboard-friendly navigation and bulk actions

### Right preview pane

- Active message metadata and full content view
- Attachment previews and contextual actions
- Message headers, tags, and threading context
- Quick actions for reply, move, flag, and mark as read

### Footer pane

- Sync and connection state
- Active account or selected folder summary
- Background job and notification status
- Low-priority alerts and progress details

## Layout sections

### 1. Header and toolbar

- Global search field
- Composer shortcut
- Mail actions for reply, forward, archive, delete, mark unread
- Status indicators for sync, offline state, and unread counts

### 2. Left navigation panel

- Accounts and identities
- Mail folders and saved searches
- Folder counts and starred/unread state
- Optional compact mode for narrow layouts

### 3. Message list panel

- Thread or flat list depending on user preference
- Subject, sender, time, and tags
- Selection highlighting and unread states
- Keyboard navigation support

### 4. Preview panel

- Message body, attachments, and metadata
- Reading controls: zoom, mark as read, open external viewer when needed
- Conversation threading and quick actions

### 5. Footer/status bar

- Sync status
- Current account or folder context
- Background job notifications
- Alerts for authentication, errors, and local actions

## Candidate modes

### Compact mode

- Reduced row height
- Hidden preview metadata for density
- Tighter spacing and fewer visible controls

### Comfortable mode

- Larger spacing and preview clarity
- More metadata visible in the message list
- Better readability for long message threads

### Focus mode

- Emphasize the reading panel
- Minimize navigation clutter
- Preserve global actions without stacking extra panes

## Interaction principles

- Keep primary actions visible and keyboard-accessible
- Maintain readable contrast and focus states
- Avoid excessive nesting in the account/folder tree
- Support both KDE Plasma and desktop-native conventions

## Design checklist

- Define primary actions and toolbar layout
- Capture account and folder navigation patterns
- Record keyboard and accessibility requirements
- Document notification and status states
- Confirm spacing and density behaviors across screens
- Decide how threading, search, and folder views behave together
