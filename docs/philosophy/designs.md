# Plasma Impresario interface design

**[Documentation](../README.md)** · **[Scope](scope.md)** ·
**[UI parity](../parity/ui.md)** · **[Roadmap](../ROADMAP.md)** ·
**[Inspirations](inspirations.md)**

This guide separates the current Trojitá interface from the proposed Plasma
Impresario experience. The [UI parity guide](../parity/ui.md) records the
current layout; target designs here are not implementation claims.

## Current interface

The Qt Widgets application has a toolbar/menu above a shell with a left
column, a main column, a right column and a footer. With the default settings
it looks like the classic three-pane layout:

```text
Toolbar / menu
┌────────────────────┬──────────────────────────────────────────┬ ─ ─ ─ ─ ─ ┐
│ Left column        │ Main column                              │ Right
│ Mailbox tree       │ Message list                             │ column
│                    ├──────────────────────────────────────────┤ (hidden
│                    │ Message reader                           │ while
│                    ├──────────────────────────────────────────┤ empty)
│                    │ Attachments panel                        │
├────────────────────┴──────────────────────────────────────────┴ ─ ─ ─ ─ ─ ┤
│ Footer (hidden while empty)                                                │
└────────────────────────────────────────────────────────────────────────────┘
Status bar
```

The mailbox tree goes in the left or right column. The main column holds the
message list and the reader column: stacked in the compact layout, side by
side in the wide layout, or replaced by one-panel-at-a-time navigation, which
shows the mailbox tree, list and reader in turn while the side columns keep
any embedded panels placed there. A side column or the footer is shown only
while it holds the mailbox tree or a shown embedded panel; when it returns, it
gets its previous size back. Region widths, the main column's split, the
sidebar side and the selected layout are saved for each layout. Each embedded
panel starts with room for its title and a few rows (three attachments, four
contacts, five detail or related lines); panels side by side in the footer
share its width. A size the user sets by dragging a divider is kept for that
panel, including after it is hidden or moved back, and saved with the layout.
Panels sharing a column with the mailbox tree, or the reader column with the
reader, take at most two thirds of it. Sizes saved by versions before the
shell are not migrated, so the panes start at their default sizes once after
upgrading. The selected
folder and message are restored when they become available, unless the user
navigates elsewhere first. Message restoration checks the mailbox's UIDVALIDITY
before selecting a saved UID.

The mailbox tree recognizes **special folders**: Inbox, Drafts, Sent, Archive,
Junk and Trash (plus All Mail and Flagged on servers that announce them). The
server's RFC 6154 SPECIAL-USE attributes decide first, including the legacy
XLIST names; the client asks for them when the server supports LIST-EXTENDED
and SPECIAL-USE. The account's configured archive folder also counts as
Archive. Without attributes, top-level folders and direct children of INBOX
with a common English name (for example _Sent Items_, _Deleted Items_ or
_Junk E-mail_) are used, at most one per kind. Special folders get Plasma
theme icons and are listed first among their siblings in that order; other
folders keep the server's order. **Mailbox › Go to Folder** opens the active
account's Inbox, Drafts, Sent, Archive, Junk or Trash; an entry is disabled
while the account has no such folder. **Mailbox › Go to Folder…** (Ctrl+J)
opens a searchable list of the folders already loaded in the tree: typing
filters it, the arrow keys move through matches and Enter opens the folder.
Both select the folder in the tree and move focus to the message list, also
in one-panel-at-a-time navigation.

**Deleting messages** depends on the account's Trash folder. While the active
account has one and the open folder is not that Trash, Delete (the **Move to
Trash** action) moves the selected messages there with IMAP MOVE. In Trash,
without a Trash folder, or with **Message › Move Deleted Messages to Trash**
turned off, Delete toggles the `\Deleted` mark as before: the messages stay
recoverable until the folder is expunged. The option is stored per account as
`imap.deleteMovesToTrash` and is **on by default**. **Delete Permanently…**
(Shift+Delete, in the Message menu and the message list's context menu) marks
the selected messages `\Deleted` and removes only them with UID EXPUNGE. A
server without UIDPLUS cannot do that, so the messages stay marked and the
notification offers **Expunge** for the whole folder. **Expunge** (Ctrl+E)
removes every marked message in the open folder and says how many of the
loaded messages are marked.

The folder tree's context menu and the Mailbox menu offer **Rename Folder…**,
which asks for the new last name component and sends IMAP RENAME, keeping the
folder's subscription. Renaming is unavailable offline, for INBOX and for
containers that cannot be selected. A name containing the hierarchy separator
is refused. When the open folder or one of its parents is renamed, the window
reopens it under the new name. **Empty Trash…** and **Empty Junk…** are
available in the Mailbox menu while the account has those folders, and in the
context menu of the Trash or Junk folder itself. They mark every message in the
folder `\Deleted` and expunge it; an empty folder is only reported as empty.

**Quick actions** for the selected messages are in the Message menu, the
message list's context menu and the main toolbar, and are disabled offline or
without a selection:

- **Archive** (A) moves the messages to the account's archive folder: the
  folder set in the account settings, otherwise the Archive special folder the
  tree recognizes (such as the server's `\Archive` folder), otherwise
  `Archive`.
- **Move to Folder…** (V) and **Copy to Folder…** (Shift+V) open the folder
  picker without the open folder and without folders that cannot hold
  messages. The **Move To** and **Copy To** submenus list the account's five
  most recent destinations, newest first, followed by **Other Folder…**.
  Destinations are remembered per account as `imap.recentDestinationFolders`
  when an operation starts. Folders the loaded tree shows as deleted or not
  selectable are dropped when a menu opens; folders below a branch that has not
  been loaded are kept. On the toolbar, Move to Folder… has the recent
  destinations behind its arrow.
- **Mark as Read** (M) and **Mark as Flagged** (S) are toggles that show the
  state of the selection. **Mark as Unread** (Shift+M) always marks the
  selection unread.

Toolbar-sized icons use the theme's `-symbolic` variants when available.
Per-message hover actions in the list are planned separately.

Delete Permanently, Expunge and the Empty actions ask first, naming the number
of messages when it is known. The question has a **Do not ask again** check
box, which takes effect only when the operation is accepted. The answers are
stored in `plasma-impresariorc`, group `Confirmations`, as `ConfirmExpunge`,
`ConfirmDeletePermanently` and `ConfirmEmptyFolder` (`false` skips the
question). Moving to Trash, archiving and moving or copying to other folders
are not confirmed because they can be undone or are harmless.

Results appear in a **notification bar** between the quick search field and
the message list instead of a dialog. For example, it reports _Moved 3
messages to Trash_, _Archived 2 messages_, _Permanently deleted 1 message_,
renames, emptied folders and failures, including the server's message.
Information hides itself after eight seconds; errors stay until closed. After
a move to Trash, Archive or another folder, **Undo** moves the messages back to
the source folder when the server reported their new UIDs (RFC 4315 COPYUID).
Otherwise the bar offers **Open Trash**, **Open Archive** or **Open Folder**.
Copies report _Copied 2 messages to …_ with **Open Folder** and cannot be
undone. Marking more than one message reports _Marked 5 messages as read_,
_Marked 5 messages as unread_, _Flagged 5 messages_ or _Removed the flag from 5
messages_ when the command is sent; a single message is not reported. The bar
is cleared when the account changes.

The **message list toolbar** shares a row with the quick search field. It
holds checkable **quick filters**, **Unread**, **Flagged** and **With
Attachments**, as icon buttons with tooltips and accessible names, followed
by **Clear Filters** (shown only while a filter is on) and a **Sort** button
that opens the Mailbox › Sorting menu and shows the sort column and direction
in its icon and tooltip. The same filters are in **View › Filter** and in the
context menu of the empty area below the messages. Active filters must all
match, and they combine with the quick search. **Hide Read Messages** is the
Unread filter under its old name. Filters are stored per account as
`gui/msgList.filterUnread`, `filterFlagged` and `filterAttachments`, so they
apply to every folder of that account. An account without its own filters
starts with the former global Hide Read Messages preference. A thread is
shown when any of its messages matches, and then all of its messages are
shown. A message whose flags are not loaded yet is hidden by Unread and
Flagged until they arrive. A message whose structure is not loaded yet is
shown by With Attachments, so that rows do not disappear while scrolling. The
current message stays listed while it is current, even after it stops
matching (for example, when reading marks it read under Unread). It is hidden
once another message becomes current.

Below the list, a **status line** shows the folder's message and unread
counts as reported by the server (_120 messages, 7 unread_). It adds _showing
7 of 120_ while filters are on and _3 selected_ when more than one message is
selected. **Select All** (Ctrl+A), **Select Thread**, **Invert Selection** and
**Deselect** are in the Message menu. Their shortcuts apply only while the
message list has the focus, so Ctrl+A still selects text in the reader or the
search field. Select Thread selects the current message's whole thread and is
available only while threading is on. Invert Selection treats a collapsed
thread as one row. Only Select All has a default shortcut; the others can be
assigned one in the shortcut settings. The message context menu offers Select
Thread and Select All; the empty area offers Select All, the filters and **Mark
Mailbox as Read**. All message actions, including read, flag, tags, archive,
delete and move or copy, apply to the whole selection.

An embedded **attachments panel** sits below the reader in every layout, in a
resizable reader column whose split is saved with the layout. It lists the
current message's attachments with their type icons, names and sizes, and
provides Open and Save As actions, a context menu and dragging into the
composer. Attachments that arrive late, such as after decryption, are added as
they appear. Without a listable attachment, the panel shows _No message
selected_, _Loading message…_ or _This message has no attachments_. The panel is
**shown by default**. **View › Embedded Panels** turns it off, or turns on
**Hide Attachments Panel When Empty**, which shows it only while the current
message has attachments. That option is **off by default**, so the panel keeps a
stable position while reading. Both settings are saved in the `Appearance`
group. The panel's rules differ from the message-list paperclip:

- It lists only what a reader would call a file: inline images that HTML
  references through `multipart/related`, signatures and the encrypted envelope
  are omitted.
- An attached message (`message/rfc822`) is listed once, not expanded into its
  own attachments.

Each embedded panel has a submenu in **View › Embedded Panels** with **Show
Panel**, **Hide When Empty** and a position: **Below Reader**, **Left Column**,
**Right Column** or **Footer**. Preferences are saved in the `Appearance` group
as `<Panel>Visible`, `<Panel>AutoHide` and `<Panel>Placement`, where `<Panel>`
is `AttachmentPanel`, `MessageDetailsPanel`, `ContactPanel` or
`RelatedMessagesPanel`. Panels in one region keep a fixed order (attachments,
details, contacts, related messages) after the mailbox tree or reader.
F6 and Shift+F6 also visit the shown panels. Besides the attachments panel,
three panels are available and **off by default**, so the default look does not
change:

- **Message Details** (right column): subject, sender, recipients, date, size,
  folder, read/flagged/replied/forwarded state and whether the message is signed
  or encrypted at the MIME level. Rows without a value are hidden.
- **Contacts** (right column): the From, Reply-To, To and Cc addresses, each
  listed once with all its roles, with Write Email, Copy Address and Open in
  Address Book actions. Reply-To is listed only when it names other addresses
  than From. The address-book action is available only when an
  address-book plugin is loaded.
- **Related Messages** (right column): messages of the current folder in the
  same conversation (same subject without reply prefixes, or linked through
  Message-ID and In-Reply-To) and other messages from the same sender, newest
  first, at most 25 per group. Activating one selects it in the message list.
  Only messages whose envelope the list has already loaded are considered, so
  the panel never makes the client download envelopes; see
  [CAVEATS](../CAVEATS.md#related-messages-and-message-details-panels).

The panels follow the message in the reader and update when its envelope,
flags or structure arrive later. While a message is loading they say so;
with **Hide When Empty** they stay hidden until there is something to show.

The footer reports IMAP connection activity, offline state, network errors and
connection or authentication failures; richer per-account and background-task
status remains planned.

## Proposed Plasma Impresario shell

The target follows the
[Plasma Opus five-region layout](https://github.com/mikepadiernos/plasma-opus-file-manager/blob/main/docs/philosophy/designs.md#interface-structure):
header, left column, main column, right column and footer. It is a design
target, not a claim that all Plasma Opus or Impresario placements are
implemented.

```text
┌─────────────────────────────────────────────────────────────────────┐
│ Header: search, compose, account and connection status              │
├──────────────────┬─────────────────────────────┬────────────────────┤
│ Left column      │ Main column                 │ Right column       │
│ Sidebar or       │ Message list                │ Sidebar or         │
│ embedded panels  ├─────────────────────────────┤ embedded panels    │
│                  │ Reading pane                │                    │
├──────────────────┴─────────────────────────────┴────────────────────┤
│ Footer: status and optional embedded panels                         │
└─────────────────────────────────────────────────────────────────────┘
```

This illustrates one arrangement; users should be able to configure the
placement and visibility of supporting panels.

### Panes and placement

- **Sidebar panels:** Place account groups, folder navigation, saved views and
  similar navigation panels in either the left or right column.
- **Main content panes:** Keep message lists and reading panes in the main
  column, arranged side by side or stacked. Preserve account, folder and message
  context when changing the arrangement.
- **Embedded panels:** Place message metadata, attachments, contact information
  and similar contextual tools in the left, right or main column, or the footer,
  as appropriate to the user's layout. When an embedded panel occupies
  main-column space, preserve the message-list and reader state.
- **Sliding and floating panels:** Treat these as additional, configurable
  placements outside the five fixed regions, following the Plasma Opus target
  design. Planned for M12.02 with rearranging panels in the customization mode
  ([#307](https://github.com/mikepadiernos/plasma-impresario/issues/307)).
- **Header and footer:** Keep global actions and connection context accessible
  in the header, and provide sync progress, errors and background-work status in
  the footer. The footer shows connection status today; the designed header
  is planned for M12.02
  ([#306](https://github.com/mikepadiernos/plasma-impresario/issues/306)).

### Embedded panels

Embedded panels are context-aware: each follows the account, folder or message
in focus. Each can be turned off from **View › Embedded Panels** and can hide
itself while it has nothing to show for the current context. Hiding must not
move focus or selection, and a hidden panel's size is restored when it returns.

- **Attachments (implemented):** below the reader by default, or in a side
  column or the footer; see [Current interface](#current-interface).
- **Message details, contacts and related messages (implemented):** see
  [Current interface](#current-interface). Verified signer details
  ([M04, #303](https://github.com/mikepadiernos/plasma-impresario/issues/303)),
  address-book details
  ([M05.04, #305](https://github.com/mikepadiernos/plasma-impresario/issues/305))
  and related messages across folders
  ([M04, #302](https://github.com/mikepadiernos/plasma-impresario/issues/302))
  remain planned.
- **My Day / Agenda (planned for M05.04, not implemented,
  [#304](https://github.com/mikepadiernos/plasma-impresario/issues/304)):** a
  daily overview beside the
  mail workflow. Content:
  - today's events and upcoming meetings, with time, title, location and a join
    or open action;
  - tasks due today or overdue;
  - flagged messages and messages marked for follow-up, opened in the reader
    without leaving the current folder.

  Calendar and task data come from KDE PIM and Akonadi when they are available.
  Without them, the panel shows only mail-derived items and says that calendars
  are unavailable rather than showing an empty schedule. By default it goes in a
  side column, with a condensed footer form showing the next event and the count
  of due items. With **hide when empty**, it disappears when there are no
  events, tasks or follow-ups for the day. Items are reachable from the keyboard
  in time order, each with an accessible name that combines its time, title and
  source. The day boundary follows the system time zone and updates at
  midnight. The panel does not create or edit calendar data; editing hands off
  to the KDE PIM application.

Save panel placement, visibility, sizes and focus with the layout. Provide
keyboard access and clear context for actions, and preserve drafts and selection
when panels are moved or hidden. Account groups, saved views, workspace context
and rich status surfaces remain planned. Evolve the working mailbox/list/reader
flow without losing existing behavior.

### Optional tabs

Offer a Thunderbird-inspired tabbed mail workspace as an explicit setting, **off
by default**. With tabs disabled, use the normal single-workspace shell without
a tab bar.

When enabled, allow mail views and individual messages to open in separate tabs
within the main workspace. Keep each tab's account, folder, selection and
reading context clear when switching, and provide labeled tabs, closing and
keyboard navigation. Tabs complement rather than replace stacked or side-by-side
content panes.

When first enabled, place the tab bar in the header by default. Let users move
it to any shell region: header, left column, main column, right column or
footer. Support horizontal and vertical orientations in each region, with the
tab bar movable independently of the content panes. Save its placement and
orientation with the layout, and keep labels, focus and keyboard navigation
usable in either orientation.

Define session restoration and behavior for unavailable accounts or removed
messages. Disabling tabs or closing a tab must not silently discard drafts or
pending work; allow the user to resolve or cancel the change. Future
productivity views can use tabs when those integrations are available, without
assuming they already exist.

### Toolbars and customization

Follow the
[Plasma Opus toolbar model](https://github.com/mikepadiernos/plasma-opus-file-manager/blob/main/docs/philosophy/designs.md#toolbars-and-customization)
as a target: toolbars can occupy any of the five regions, including the top or
bottom of a column, and main content panes can have their own toolbars. Allow
items and widgets to be mixed across built-in and custom toolbars while
retaining the intended account, folder, message and pane context.

Use one customization mode to rearrange panels, toolbars and toolbar items;
outside that mode, keep the layout and toolbar contents fixed while normal
actions remain available. Preserve customizations, shortcuts and saved layouts.

Impresario's distinctive navigation icon toolbar can be placed in any shell
region. In either side column it can dock to the top, bottom, left or right
edge, including vertically. It also has a distinct full-app-height placement at
the left or right window edge, spanning vertically from the top of the header to
the bottom of the status bar; reserve space so it does not cover the shell.
Other toolbars remain horizontal. The pictured left-edge arrangement separates
mail and utility or menu actions with expanding space. This toolbar is separate
from movable sidebar panels. See the
[navigation toolbar](../refinements/toolbars/main-window-toolbar.md).

### Themes and appearance

Appearance options adapt the
[Plasma Opus themes milestone](https://github.com/mikepadiernos/plasma-opus/blob/main/docs/ROADMAP.md#m17-themes)
to mail; see [M08](../ROADMAP.md#m08-themes-and-branding-foundation). They are
planned, not implemented.

- **Plasma first:** Every option follows the active Plasma color scheme,
  accent, fonts, icon theme and animation speed until the user changes it.
  App-local choices never change the desktop's global appearance, widget style
  or window decorations.
- **Layering:** Apply Plasma defaults, then the theme profile, then workspace
  overrides, then the optional custom stylesheet. Options a workspace does not
  select keep their global values.
- **Mail surfaces:** Themes cover the shell, folder tree, message list, message
  headers, composer, panels, tabs and status bar. HTML message content keeps its
  own styling and the reader's privacy policy; app stylesheets and palettes do
  not restyle it or load remote resources for it.
- **Mail-specific options:** Account and folder colors appear in the folder
  tree, unified-inbox source indicators, message cards and tabs. Tree lines
  apply to both folders and threads. The message list gains a card layout
  beside compact and comfortable rows.
- **Effects:** Blur uses KWin through KWindowEffects and is reported as
  unavailable without compositing. Text, icons, selection and message content
  stay fully opaque.
- **Accessibility:** Never convey unread, account or thread state through color
  alone. Validate contrast for every custom color, offer to suspend effects with
  high-contrast schemes, and provide a one-step reset to Plasma defaults.
- **Sharing:** Theme profiles are versioned and validated, never execute code,
  and can be exchanged with Plasma Opus; see
  [Plasma Opus integrations](../integrations/plasma-opus.md#themes-and-appearance).

## Proposed interface widgets

These are in-app interface components, not Plasma desktop widgets. They adapt
ideas from the [inspirations](inspirations.md); they are design candidates, not
claims that every component is implemented.

- **Search:** Search the current folder or selected accounts, with visible
  scope, filters and saved searches. Inspired by Vivaldi Mail and Aqua Mail.
- **Account and folder navigation:** Show account groups, folder hierarchy and
  unread counts with clear account context. Inspired by Trojitá and Aqua Mail.
  Special-folder icons and ordering, quick navigation to special folders and a
  searchable folder picker are implemented (see
  [Current interface](#current-interface)); account groups remain planned.
- **Unified inbox:** Combine selected accounts while identifying each message's
  source and keeping actions account-aware. Inspired by Vivaldi Mail.
- **Message list and quick actions:** Present sender, subject, date and unread
  state with accessible archive, move, flag and delete actions. Inspired by Aqua
  Mail and The Bat!. Menu, context-menu, toolbar and keyboard quick actions,
  including recent move and copy destinations, are implemented (see
  [Current interface](#current-interface)); per-message hover actions remain
  planned.
- **Filters and sorting:** Expose active filters, sort order and threading
  without obscuring the current folder or selection. Inspired by Vivaldi Mail
  and The Bat!. Unread, Flagged and With Attachments quick filters, a Sort
  button, selection commands and folder counts are implemented (see
  [Current interface](#current-interface)); filters by tag, date or sender
  and per-folder filter state remain planned.
- **Conversation reader:** Display message content, metadata, quoted text and
  reply actions in a focused reading pane. Inspired by Trojitá and Aqua Mail.
- **Compose and recipients:** Provide identity selection, recipient entry,
  drafts and sending feedback. Inspired by desktop mail workflows in Trojitá and
  Kontact.
- **Attachments:** Show file names, sizes, loading progress and safe open or
  save actions. Inspired by Aqua Mail and The Bat!. A first embedded panel
  below the reader is implemented; see
  [Current interface](#current-interface).
- **My Day / Agenda:** Summarize today's events, due tasks and flagged or
  follow-up mail in an embedded panel. Planned; see
  [Embedded panels](#embedded-panels). Inspired by ePim and Kontact.
- **Connection and task status:** Show account state, sync progress, errors and
  actionable recovery without interrupting reading. Inspired by KDE and Kontact
  desktop conventions.
- **Rules and bulk actions:** Let users inspect rule conditions and apply
  operations to selected messages with progress and failure feedback. Inspired
  by The Bat!.
- **Workspace and productivity context:** Offer optional account/workspace,
  contact and scheduling entry points when integrations are available. Inspired
  by ePim and Kontact.

See the
[KDE Plasma integration guide](../integrations/kde-plasma.md#plasma-widgets) for
separate desktop and panel widget proposals.

## Interaction and accessibility

- Keep account, folder, selection and connection context visible.
- Make primary actions keyboard accessible with visible focus and predictable
  focus restoration. F6 and Shift+F6 cycle focus between the account switcher,
  folder tree, message list, reader and attachments panel, skipping hidden
  panes.
- Remember folder expansion per account so switching accounts restores each
  account's tree.
- Do not use color as the only way to convey unread, connection or security
  state.
- Validate narrow windows, high-DPI scaling, long translations, RTL layouts and
  screen-reader names.
- Keep destructive and external-content actions explicit and recoverable.

## Layout recommendations

The existing **compact**, **wide** and **one-panel-at-a-time** modes describe
pane arrangement. The separate **Row Density** menu selects compact or
comfortable spacing for folder and message rows and saves the preference.
Focus-reading mode remains planned.

Prototype each density with realistic mail data and long folder names. Preserve
saved splitter sizes where practical, and test keyboard navigation and message
selection while switching layouts.

## Implementation sequence

1. Record the current main-window behavior and its acceptance tests.
2. Prototype the new shell without changing IMAP, cache or composition behavior.
3. Validate layouts, accessibility and navigation with representative data.
4. Migrate settings and saved window state only after the new behavior is
   tested.
