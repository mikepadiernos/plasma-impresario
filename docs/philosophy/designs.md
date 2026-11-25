# Plasma Impresario interface design

**[Documentation](../README.md)** · **[Scope](scope.md)** ·
**[Inspirations](inspirations.md)** · **[Compatibility](compatibility.md)** ·
**[UI parity](../parity/ui.md)** · **[Roadmap](../ROADMAP.md)** ·
**[Caveats](../CAVEATS.md)**

This guide is the single source for Plasma Impresario's interface and layout.
[Current interface](#current-interface) describes what the code implements;
[Proposed Plasma Impresario shell](#proposed-plasma-impresario-shell) and the
sections after it describe planned designs, not implemented behavior.

Legend: ✅ done · 🧪 implemented, needs verification · 🚧 in progress · 📋
planned · ⚠️ limitation

## Contents

- [Current interface](#current-interface)
  - [Shell status](#shell-status)
  - [Layouts and regions](#layouts-and-regions)
  - [Current embedded panels](#current-embedded-panels)
  - [Status bar](#status-bar)
- [Proposed Plasma Impresario shell](#proposed-plasma-impresario-shell)
  - [Panes and placement](#panes-and-placement)
  - [Embedded panels](#embedded-panels)
  - [Optional tabs](#optional-tabs)
  - [Toolbars and customization](#toolbars-and-customization)
  - [Themes and appearance](#themes-and-appearance)
  - [Message list presentation](#message-list-presentation)
  - [Aqua Mail-inspired desktop customization](#aqua-mail-inspired-desktop-customization)
  - [eM Client-inspired feature candidates](#em-client-inspired-feature-candidates)
- [Proposed interface widgets](#proposed-interface-widgets)
- [Interaction and accessibility](#interaction-and-accessibility)
- [Layout recommendations](#layout-recommendations)
- [Implementation sequence](#implementation-sequence)

## Current interface

> [!IMPORTANT]
>
> The shell described in this section was built in M02. Offscreen tests and
> opt-in live IMAP tests against a synthetic Dovecot server in
> `tools/dev-container/` cover it; it has not been verified in a real Plasma
> session, which M15
> ([#319](https://github.com/mikepadiernos/plasma-impresario/issues/319))
> tracks.

The Qt Widgets application has a shell with a left column, a main column, a
right column and a footer, followed by the status bar. The Navigation toolbar
docks at the left window edge by default and can be moved to another edge; the
menu bar is hidden by default and opens from the toolbar's main menu button. The
diagram below shows the regions only, not the toolbar's position. With the
default settings it looks like the classic three-pane layout:

```text
Toolbar / menu
┌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌┐
╎ Header (View › Show Header, off by default): account, folder and status    ╎
└╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌┘
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

### Shell status

✅ marks issues that are Completed on the
[project board](https://github.com/users/mikepadiernos/projects/4); 🧪 marks
work that is implemented but whose issue is still in Review or earlier.

| Feature                                                                  | Status | Milestone | Issue                                                                                                                                               |
| ------------------------------------------------------------------------ | ------ | --------- | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| 🧭 Shell regions: toolbar, left, main and right columns, footer          | ✅     | M02       | [#35](https://github.com/mikepadiernos/plasma-impresario/issues/35)                                                                                 |
| 🧭 Mailbox sidebar in either side column                                 | ✅     | M02       | [#35](https://github.com/mikepadiernos/plasma-impresario/issues/35)                                                                                 |
| 🧭 Compact, wide and one-at-a-time layouts with sizes saved per layout   | ✅     | M02       | [#35](https://github.com/mikepadiernos/plasma-impresario/issues/35)                                                                                 |
| 📎 Attachments panel, shown by default                                   | ✅     | M02       | [#35](https://github.com/mikepadiernos/plasma-impresario/issues/35)                                                                                 |
| 🧩 Message Details, Contacts and Related Messages panels, off by default | ✅     | M02       | [#35](https://github.com/mikepadiernos/plasma-impresario/issues/35), caveat [#264](https://github.com/mikepadiernos/plasma-impresario/issues/264)   |
| ⌨️ <kbd>F6</kbd> / <kbd>Shift</kbd>+<kbd>F6</kbd> focus cycling          | ✅     | M02       | [#35](https://github.com/mikepadiernos/plasma-impresario/issues/35)                                                                                 |
| 📬 Multiple accounts with a toolbar selector                             | ✅     | M02       | [#222](https://github.com/mikepadiernos/plasma-impresario/issues/222), caveat [#262](https://github.com/mikepadiernos/plasma-impresario/issues/262) |
| 📶 Connection, authentication and offline status                         | ✅     | M02       | [#79](https://github.com/mikepadiernos/plasma-impresario/issues/79)                                                                                 |
| 🔍 Folder counts and per-account saved searches                          | ✅     | M02       | [#76](https://github.com/mikepadiernos/plasma-impresario/issues/76)                                                                                 |
| 🎨 Compact and comfortable row density                                   | ✅     | M02       | [#75](https://github.com/mikepadiernos/plasma-impresario/issues/75)                                                                                 |
| 💾 Restore layout, sizes, density and selected message across sessions   | ✅     | M02       | [#81](https://github.com/mikepadiernos/plasma-impresario/issues/81)                                                                                 |
| 🧭 Account groups in the folder sidebar                                  | ✅     | M02       | [#80](https://github.com/mikepadiernos/plasma-impresario/issues/80)                                                                                 |
| 🧭 Optional header with the account, folder and status                   | ✅     | M02       | [#74](https://github.com/mikepadiernos/plasma-impresario/issues/74)                                                                                 |
| 🧩 Actions hidden when the account protocol cannot perform them          | ✅     | M02       | [#215](https://github.com/mikepadiernos/plasma-impresario/issues/215)                                                                               |
| 🔍 Related messages from the server and other folders                    | 📋     | M05.01    | [#302](https://github.com/mikepadiernos/plasma-impresario/issues/302)                                                                               |
| 🔍 Search across folders and accounts, saved searches as virtual folders | 📋     | M05       | [Roadmap](../ROADMAP.md#m05-search-and-indexing)                                                                                                    |
| 🔍 Search Results panel                                                  | 📋     | M05.05    | [#348](https://github.com/mikepadiernos/plasma-impresario/issues/348)                                                                               |
| 🔒 Verified signature and decryption results in Message Details          | 📋     | M04       | [#303](https://github.com/mikepadiernos/plasma-impresario/issues/303)                                                                               |
| 📅 My Day / Agenda panel                                                 | 📋     | M06.04    | [#304](https://github.com/mikepadiernos/plasma-impresario/issues/304)                                                                               |
| 👥 Address-book names, photos and organizations in Contacts              | 📋     | M06.04    | [#305](https://github.com/mikepadiernos/plasma-impresario/issues/305)                                                                               |
| 🗂️ Optional mail workspace tabs                                          | 📋     | M08       | [#246](https://github.com/mikepadiernos/plasma-impresario/issues/246)                                                                               |
| 🎨 Themes and appearance                                                 | 📋     | M09       | [Roadmap](../ROADMAP.md#m09-themes-and-branding-foundation)                                                                                         |
| 🧭 Designed header with search, compose, account switcher and status     | 📋     | M14.02    | [#306](https://github.com/mikepadiernos/plasma-impresario/issues/306)                                                                               |
| 🧩 Sliding and floating panels; rearranging panels in customization mode | 📋     | M14.02    | [#307](https://github.com/mikepadiernos/plasma-impresario/issues/307)                                                                               |
| 🧰 Toolbar customization and the navigation icon toolbar                 | 📋     | M14.02    | [#151](https://github.com/mikepadiernos/plasma-impresario/issues/151)                                                                               |

### Layouts and regions

The folder sidebar goes in the left or right column, chosen in **View › Folder
Sidebar Position**. **View › Layout** arranges the main column:

| Layout        | Main column                                                                                             |
| ------------- | ------------------------------------------------------------------------------------------------------- |
| Compact       | Message list stacked above the reader column                                                            |
| Wide          | Message list beside the reader column                                                                   |
| One At a Time | Shows the mailbox tree, list and reader in turn; the side columns keep any embedded panels placed there |

Region sizes follow these rules:

- A side column or the footer is shown only while it holds the mailbox tree or a
  shown embedded panel. When it returns, it gets its previous size back.
- Region widths, the main column's split, the sidebar side and the selected
  layout are saved for each layout.
- Each embedded panel starts with room for its title and a few rows (three
  attachments, four contacts, five detail or related lines). Panels side by side
  in the footer share its width.
- A size the user sets by dragging a divider is kept for that panel, including
  after it is hidden or moved back, and saved with the layout.
- Panels sharing a column with the mailbox tree, or the reader column with the
  reader, take at most two thirds of it.
- ⚠️ Sizes saved by versions before the shell are not migrated, so the panes
  start at their default sizes once after upgrading; see
  [CAVEATS](../CAVEATS.md#related-messages-and-message-details-panels).

### Account sidebar and header

With two or more accounts, the folder sidebar shows a header for each account
and the folder tree under the active one; with one account it shows the tree
alone. Each header shows the account's name and flags an account that needs
attention. Clicking another account's header switches to that account, like the
toolbar's account selector; clicking the active account's header collapses or
expands its folders. The headers take keyboard focus and announce whether the
folders are shown.

**View › Show Header** adds a header region above the columns with the active
account, the current folder and the account's status when it needs attention.
It is off by default, saved as `ShowHeader` in the `Appearance` group of
`plasma-impresariorc`, and is the first region in the <kbd>F6</kbd> focus cycle
while shown. The designed header with search, compose and the account switcher
remains planned for M14.02
([#306](https://github.com/mikepadiernos/plasma-impresario/issues/306)).

Actions the active account's protocol cannot perform are hidden rather than
disabled: with a POP3 account, folder management, archiving, junk and tags are
hidden, and they return when switching to an IMAP or JMAP account. Actions that
are hidden for another reason stay hidden.

### Row density and restored selection

**View › Row Density** selects **Compact** or **Comfortable** spacing for folder
and message rows. The selected folder and message are restored when they become
available, unless the user navigates elsewhere first. Message restoration checks
the mailbox's UIDVALIDITY before selecting a saved UID.

### Current embedded panels

Four embedded panels are available. Only Attachments is shown by default, so the
default look matches the classic layout.

| Panel               | Default             | Content                                                                                                                                                                                                                  | Actions                                                                                  |
| ------------------- | ------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------- |
| 📎 Attachments      | Shown, below reader | The current message's attachments with type icons, names and sizes. Attachments that arrive late, such as after decryption, are added as they appear.                                                                    | Open, Save As, context menu, drag into the composer                                      |
| 🧾 Message Details  | Off, right column   | Subject, From, To, Cc, date, size, folder, read/flagged/replied/forwarded state and whether the message is signed or encrypted at the MIME level. Rows without a value are hidden.                                       | None                                                                                     |
| 👥 Contacts         | Off, right column   | The From, Reply-To, To and Cc addresses, each listed once with all its roles. Reply-To is listed only when it names other addresses than From.                                                                           | Write Email, Copy Address, Open in Address Book (only when an address-book plugin loads) |
| 🔗 Related Messages | Off, right column   | Messages of the current folder in the same conversation (same subject without reply prefixes, or linked through Message-ID and In-Reply-To) and other messages from the same sender, newest first, at most 25 per group. | Activating a message selects it in the message list                                      |

Each panel has a submenu in **View › Embedded Panels**. Preferences are saved in
the `Appearance` group of `plasma-impresariorc`, where `<Panel>` is
`AttachmentPanel`, `MessageDetailsPanel`, `ContactPanel` or
`RelatedMessagesPanel`:

| Menu item                                                       | Key                | Default                                                   |
| --------------------------------------------------------------- | ------------------ | --------------------------------------------------------- |
| **Show Panel**                                                  | `<Panel>Visible`   | On for Attachments, off for the others                    |
| **Hide When Empty**                                             | `<Panel>AutoHide`  | Off                                                       |
| **Position:** Below Reader, Left Column, Right Column or Footer | `<Panel>Placement` | Below Reader for Attachments, Right Column for the others |

Panels in one region keep a fixed order (attachments, details, contacts, related
messages) after the mailbox tree or reader. They follow the message in the
reader and update when its envelope, flags or structure arrive later. While a
message is loading they say so; with **Hide When Empty** they stay hidden until
there is something to show. Without **Hide When Empty**, the Attachments panel
keeps a stable position and shows _No message selected_, _Loading message…_ or
_This message has no attachments_.

The Attachments panel lists files differently from the message-list paperclip:

- It lists only what a reader would call a file: inline images that HTML
  references through `multipart/related`, signatures and the encrypted envelope
  are omitted.
- An attached message (`message/rfc822`) is listed once, not expanded into its
  own attachments.

> [!NOTE]
>
> Related Messages considers only messages whose envelope the list has already
> loaded, so the panel never makes the client download envelopes. Message
> Details reports MIME-level security only, not verified signatures. See
> [CAVEATS](../CAVEATS.md#related-messages-and-message-details-panels).

### Status bar

The status bar below the footer reports IMAP connection activity, offline state,
network errors and connection or authentication failures. The toolbar's account
selector flags accounts that need attention, including background accounts.
Richer per-account and background-task status remains planned.

## Proposed Plasma Impresario shell

The target follows the
[Plasma Opus five-region layout](https://github.com/mikepadiernos/plasma-opus/blob/main/docs/philosophy/designs.md#interface-structure):
header, left column, main column, right column and footer. The regions
themselves exist today (see [Current interface](#current-interface)); the
designed header and the placements below are not yet implemented unless marked.

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

This illustrates one arrangement; users configure the placement and visibility
of supporting panels.

### Panes and placement

| Area                    | Design                                                                                                                                                                                 | Status                                                                                                                                                                                                               |
| ----------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 🧭 Sidebar panels       | Account groups, folder navigation, saved views and similar navigation panels in either side column.                                                                                    | ✅ folder sidebar ([#35](https://github.com/mikepadiernos/plasma-impresario/issues/35)) and account groups ([#80](https://github.com/mikepadiernos/plasma-impresario/issues/80))                                     |
| 📬 Main content panes   | Message list and reading pane in the main column, side by side or stacked, preserving account, folder and message context when the arrangement changes.                                | ✅ [#35](https://github.com/mikepadiernos/plasma-impresario/issues/35)                                                                                                                                               |
| 🧩 Embedded panels      | Message metadata, attachments, contacts and similar tools in a side column, below the reader or in the footer. Panels in main-column space preserve the message-list and reader state. | ✅ [#35](https://github.com/mikepadiernos/plasma-impresario/issues/35)                                                                                                                                               |
| 🧩 Sliding and floating | Additional configurable placements outside the five fixed regions, following the Plasma Opus design, with panels rearranged in the customization mode.                                 | 📋 M14.02 [#307](https://github.com/mikepadiernos/plasma-impresario/issues/307)                                                                                                                                      |
| 🧭 Header               | Global actions, search, compose, account switcher and connection context.                                                                                                              | ✅ account, folder and status ([#74](https://github.com/mikepadiernos/plasma-impresario/issues/74)); 📋 search, compose and switcher in M14.02 [#306](https://github.com/mikepadiernos/plasma-impresario/issues/306) |
| 📶 Footer and status    | Sync progress, errors and background-work status.                                                                                                                                      | ✅ connection status ([#79](https://github.com/mikepadiernos/plasma-impresario/issues/79)); 📋 sync and task status                                                                                                  |

### Embedded panels

Embedded panels are context-aware: each follows the account, folder or message
in focus. Each can be turned off from **View › Embedded Panels** and can hide
itself while it has nothing to show. Hiding must not move focus or selection,
and a hidden panel's size is restored when it returns.

- 🧪 **Attachments, Message Details, Contacts and Related Messages:**
  implemented; see [Current embedded panels](#current-embedded-panels).
- 📋 **Panel extensions:** verified signer details
  ([M04, #303](https://github.com/mikepadiernos/plasma-impresario/issues/303)),
  related messages from the server and across folders
  ([M05.01, #302](https://github.com/mikepadiernos/plasma-impresario/issues/302)),
  a Search Results panel ([M05.05, #348](https://github.com/mikepadiernos/plasma-impresario/issues/348))
  and address-book details
  ([M06.04, #305](https://github.com/mikepadiernos/plasma-impresario/issues/305)).
- 📋 **My Day / Agenda**
  ([M06.04, #304](https://github.com/mikepadiernos/plasma-impresario/issues/304)):
  a daily overview beside the mail workflow, described below.

The My Day / Agenda panel shows:

- today's events and upcoming meetings, with time, title, location and a join or
  open action;
- tasks due today or overdue;
- flagged messages and messages marked for follow-up, opened in the reader
  without leaving the current folder.

Calendar and task data come from KDE PIM and Akonadi when they are available.
Without them, the panel shows only mail-derived items and says that calendars
are unavailable rather than showing an empty schedule. By default it goes in a
side column, with a condensed footer form showing the next event and the count
of due items. With **Hide When Empty**, it disappears when there are no events,
tasks or follow-ups for the day. Items are reachable from the keyboard in time
order, each with an accessible name that combines its time, title and source.
The day boundary follows the system time zone and updates at midnight. The panel
does not create or edit calendar data; editing hands off to the KDE PIM
application.

Save panel placement, visibility, sizes and focus with the layout. Provide
keyboard access and clear context for actions, and preserve drafts and selection
when panels are moved or hidden. Saved views, workspace context and rich status
surfaces remain planned. Evolve the working mailbox/list/reader flow without
losing existing behavior.

### Optional tabs

> [!NOTE]
>
> Planned for M08
> ([#246](https://github.com/mikepadiernos/plasma-impresario/issues/246)); not
> implemented.

Offer a Thunderbird-inspired tabbed mail workspace as an explicit setting, **off
by default**. With tabs disabled, use the normal single-workspace shell without
a tab bar.

- **Content:** Mail views and individual messages open in separate tabs within
  the main workspace. Each tab's account, folder, selection and reading context
  stays clear when switching, with labeled tabs, closing and keyboard
  navigation. Tabs complement rather than replace stacked or side-by-side
  content panes.
- **Placement:** When first enabled, the tab bar goes in the header. Users can
  move it to any shell region (header, left column, main column, right column or
  footer), horizontally or vertically, independently of the content panes. Its
  placement and orientation are saved with the layout, and labels, focus and
  keyboard navigation stay usable in either orientation.
- **Safety:** Define session restoration and behavior for unavailable accounts
  or removed messages. Disabling tabs or closing a tab must not silently discard
  drafts or pending work; the user resolves or cancels the change. Future
  productivity views can use tabs when those integrations are available.

### Toolbars and customization

> [!NOTE]
>
> Planned for M14.02
> ([#151](https://github.com/mikepadiernos/plasma-impresario/issues/151)). Today
> the existing Navigation toolbar docks at the left window edge and the menu bar
> is hidden by default.

Follow the
[Plasma Opus toolbar model](https://github.com/mikepadiernos/plasma-opus/blob/main/docs/philosophy/designs.md#toolbars-and-customization)
as a target: toolbars can occupy any of the five regions, including the top or
bottom of a column, and main content panes can have their own toolbars. Items
and widgets can be mixed across built-in and custom toolbars while keeping the
intended account, folder, message and pane context.

One customization mode rearranges panels, toolbars and toolbar items. Outside
that mode, the layout and toolbar contents stay fixed while normal actions
remain available. Customizations, shortcuts and saved layouts are preserved.

Impresario's distinctive navigation icon toolbar can be placed in any shell
region:

- In either side column it can dock to the top, bottom, left or right edge,
  including vertically.
- It also has a full-app-height placement at the left or right window edge,
  spanning from the top of the header to the bottom of the status bar, with
  space reserved so it does not cover the shell.
- Other toolbars remain horizontal. The pictured left-edge arrangement separates
  mail and utility or menu actions with expanding space.

This toolbar is separate from movable sidebar panels. See the
[navigation toolbar](../refinements/toolbars/main-window-toolbar.md).

### Themes and appearance

> [!NOTE]
>
> Planned for [M09](../ROADMAP.md#m09-themes-and-branding-foundation); not
> implemented. Row density is the only appearance option available today.

Appearance options adapt the
[Plasma Opus themes milestone](https://github.com/mikepadiernos/plasma-opus/blob/main/docs/ROADMAP.md#m17-themes)
to mail.

| Topic                    | Design                                                                                                                                                                                                                                                                 |
| ------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 🖥️ Plasma first          | Every option follows the active Plasma color scheme, accent, fonts, icon theme and animation speed until the user changes it. App-local choices never change the desktop's global appearance, widget style or window decorations.                                      |
| 🎨 Layering              | Plasma defaults, then the theme profile, then workspace overrides, then the optional custom stylesheet. Options a workspace does not select keep their global values.                                                                                                  |
| 📬 Mail surfaces         | Themes cover the shell, folder tree, message list, message headers, composer, panels, tabs and status bar. HTML message content keeps its own styling and the reader's privacy policy; app stylesheets and palettes do not restyle it or load remote resources for it. |
| 🗂️ Mail-specific options | Account and folder colors appear in the folder tree, unified-inbox source indicators, message cards and tabs. Tree lines apply to both folders and threads. The message list gains a card layout beside compact and comfortable rows.                                  |
| ✨ Effects               | Blur uses KWin through KWindowEffects and is reported as unavailable without compositing. Text, icons, selection and message content stay fully opaque.                                                                                                                |
| ♿ Accessibility         | Unread, account and thread state are never conveyed through color alone. Every custom color is contrast-checked; effects can be suspended with high-contrast schemes; one step resets to Plasma defaults.                                                              |
| 🔗 Sharing               | Theme profiles are versioned and validated, never execute code, and can be exchanged with Plasma Opus; see [Plasma Opus integrations](../integrations/plasma-opus.md#themes-and-appearance).                                                                           |

### Message list presentation

> [!NOTE]
>
> Planned for M03
> ([#87](https://github.com/mikepadiernos/plasma-impresario/issues/87)). Today
> the message list uses single-line rows with
> [compact or comfortable density](#row-density-and-restored-selection).

An Aqua Mail-inspired, multi-line message list is the target presentation,
adapted to a keyboard- and mouse-driven Plasma desktop. Each row emphasizes the
sender and subject, with the date or time, unread state, flag and attachment
indicators easy to scan. An optional short message preview appears when
available, and combined views identify the source account and folder.

```text
Sender name                                  10:42  flag  attachment
Subject
Optional message preview...
```

- Compact and comfortable spacing, optional contact images and preview text,
  and date grouping when supported.
- Selection stays distinct from unread state, with clear focus, keyboard
  navigation and accessible names for indicators and actions. Quick actions are
  also available without a pointer.
- Rows follow the active Plasma theme and handle narrow panes, long text and
  text scaling without hiding essential state.
- Rows never fetch remote contact images or message bodies merely to populate
  the list without the user's privacy choice.

See Aqua Mail's
[customization reference](https://www.aqua-mail.com/android/features/customizations).
These are proposed Impresario behaviors, not claims about the current client.

### Aqua Mail-inspired desktop customization

> [!NOTE]
>
> Planned for M08
> ([#114](https://github.com/mikepadiernos/plasma-impresario/issues/114)). Today
> row density, per-account identities and plain-text identity signatures are
> available; the other options below are not implemented.

Aqua Mail's customization patterns are adapted to the Plasma desktop rather
than copied from its mobile interface:

- **Reading and lists:** Per-surface text sizing, list density, optional preview
  text and contact images, and optional date grouping. Folder color indicators
  never serve as the only identifier. Options follow the active Plasma theme,
  display scaling and accessibility settings.
- **Actions and navigation:** Message actions are arranged in the shared
  [customization mode](#toolbars-and-customization), with configurable keyboard
  shortcuts. Touch-capable devices can enable configurable gestures that do not
  replace mouse or keyboard actions or interfere with normal scrolling.
- **Accounts and composition:** Building on per-account identities and
  plain-text signatures, rich-text signatures are offered where supported, and
  the composer shows which account and identity will be used before sending.
- **Notifications and sync:** Per-account and per-folder notification
  preferences, quiet periods and privacy-conscious previews respect Plasma Do
  Not Disturb. Account sync schedules and loading preferences are offered where
  the service supports them, with clear offline and failure states.
- **Desktop widgets:** The separate
  [Plasma widgets](../integrations/kde-plasma.md#plasma-widgets) stay optional,
  with configurable account and folder scope, presentation and privacy.

Defaults are sensible and inherited account or folder settings are explained.
Preferences are saved through KConfig, only options backed by available features
are shown, and choices persist across restarts. Message content is never
preloaded, remote contact images are never fetched and content is never sent to
external services without the appropriate privacy choice.

References: Aqua Mail's
[customization page](https://www.aqua-mail.com/android/features/customizations),
[font-size controls](https://aquamail.freshdesk.com/support/solutions/articles/77000519559-how-do-i-adjust-font-size-),
[account signatures](https://aquamail.freshdesk.com/support/solutions/articles/77000519349-account-signature),
[core functions](https://aquamail.freshdesk.com/support/solutions/articles/77000519534-aqua-mail-s-core-functions)
and
[action arrangement](https://www.aqua-mail.com/blog/unlocking-efficiency-rearrange-action-icons-in-aqua-mail-message-view).
These are design targets, not claims of feature parity or implementation.

### eM Client-inspired feature candidates

Selected ideas from [eM Client](https://www.emclient.com/?lang=en) are adapted to
the Plasma shell. These are candidates, not implemented features or milestone
completion criteria:

- **Context panels:** Communication and attachment history for the selected
  contact, keeping account and folder scope visible. Calendar and agenda
  information can appear when the corresponding integration is available.
- **Triage and follow-up:** Pinning, snoozing and reply reminders. Persistence,
  offline behavior, account scope and provider support must be defined before
  promising cross-device synchronization.
- **Command discovery:** A keyboard-accessible command palette alongside
  existing menus and shortcuts, showing only actions available in the active
  context.
- **Composition and attachments:** Reusable text and templates, plus an
  attachment search view that identifies each file's source message and account
  while preserving identity, draft and attachment state.

#### Recommendation: improve spam filtering

> [!NOTE]
>
> Tracked in M14.07
> ([#185](https://github.com/mikepadiernos/plasma-impresario/issues/185)); not
> implemented.

Use the mail provider's spam filtering as the primary layer where available, and
offer explicit, reversible **Mark as junk** and **Not junk** actions. Provide
optional per-account sender or domain block rules and explain whether they run
locally or on the provider; do not imply that a local rule protects other
devices. Submit provider training feedback only when supported and clearly
report failures or unavailable services.

Let users review junk messages and undo mistaken classifications or rules. Treat
sender addresses as spoofable: allow rules must not bypass malware, phishing or
other security checks. Avoid automatic permanent deletion and external message
submission; require informed consent for any external classification. Keep
malware scanning separate from spam classification, and show unknown or
unavailable results rather than suggesting a message is safe. Test account
isolation, offline recovery, false positives and provider capability
differences.

eM Client's [spam controls](https://www.emclient.com/blogpost.aspx?id=748)
illustrate the distinction between local and provider-side rules. The
recommendation above is a proposed Impresario design, not a claim that those
protections are implemented.

## Proposed interface widgets

These are in-app interface components, not Plasma desktop widgets; see the
[KDE Plasma integration guide](../integrations/kde-plasma.md#plasma-widgets) for
desktop and panel widget proposals. They adapt ideas from the
[inspirations](inspirations.md) and are design candidates unless marked
otherwise.

| Widget                            | Design                                                                                                | Inspired by             | Status                                                                                                                                                                                                 |
| --------------------------------- | ----------------------------------------------------------------------------------------------------- | ----------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 🔍 Search                         | Search the current folder or selected accounts, with visible scope, filters and saved searches.       | Vivaldi Mail, Aqua Mail | ✅ per-account saved searches ([#76](https://github.com/mikepadiernos/plasma-impresario/issues/76)); 📋 cross-account scope, index and virtual folders in [M05](../ROADMAP.md#m05-search-and-indexing) |
| 🧭 Account and folder navigation  | Account groups, folder hierarchy and unread counts with clear account context.                        | Trojitá, Aqua Mail      | ✅ account selector and counts; 🚧 account groups ([#80](https://github.com/mikepadiernos/plasma-impresario/issues/80))                                                                                |
| 📥 Unified inbox                  | Combine selected accounts while identifying each message's source and keeping actions account-aware.  | Vivaldi Mail            | 📋 M03                                                                                                                                                                                                 |
| 📬 Message list and quick actions | Sender, subject, date and unread state with accessible archive, move, flag and delete actions.        | Aqua Mail, The Bat!     | 📋 M03                                                                                                                                                                                                 |
| 🔃 Filters and sorting            | Active filters, sort order and threading, without obscuring the current folder or selection.          | Vivaldi Mail, The Bat!  | 📋 M03                                                                                                                                                                                                 |
| 📖 Conversation reader            | Message content, metadata, quoted text and reply actions in a focused reading pane.                   | Trojitá, Aqua Mail      | 📋 M04                                                                                                                                                                                                 |
| ✉️ Compose and recipients         | Identity selection, recipient entry, drafts and sending feedback.                                     | Trojitá, Kontact        | 📋 M04                                                                                                                                                                                                 |
| 📎 Attachments                    | File names, sizes, loading progress and safe open or save actions.                                    | Aqua Mail, The Bat!     | 🧪 [Attachments panel](#current-embedded-panels)                                                                                                                                                       |
| 📅 My Day / Agenda                | Today's events, due tasks and flagged or follow-up mail in an embedded panel.                         | ePim, Kontact           | 📋 M06.04 ([#304](https://github.com/mikepadiernos/plasma-impresario/issues/304))                                                                                                                      |
| 📶 Connection and task status     | Account state, sync progress, errors and actionable recovery without interrupting reading.            | KDE, Kontact            | ✅ connection status; 📋 sync and task progress                                                                                                                                                        |
| ⚙️ Rules and bulk actions         | Inspect rule conditions and apply operations to selected messages with progress and failure feedback. | The Bat!                | 📋 M07                                                                                                                                                                                                 |
| 🗂️ Workspace and productivity     | Optional account/workspace, contact and scheduling entry points when integrations are available.      | ePim, Kontact           | 📋 M08                                                                                                                                                                                                 |

## Interaction and accessibility

- Keep account, folder, selection and connection context visible.
- ✅ <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>PgDown</kbd> and
  <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>PgUp</kbd> switch to the next or previous
  account without reconnecting.
- 🧪 <kbd>F6</kbd> and <kbd>Shift</kbd>+<kbd>F6</kbd> cycle focus between the
  account switcher, folder tree, message list, reader and each shown embedded
  panel, skipping hidden panes.
- ✅ Folder expansion is remembered per account, so switching accounts restores
  each account's tree.
- Make primary actions keyboard accessible with visible focus and predictable
  focus restoration.
- Do not use color as the only way to convey unread, connection or security
  state.
- Validate narrow windows, high-DPI scaling, long translations, RTL layouts and
  screen-reader names.
- Keep destructive and external-content actions explicit and recoverable.

## Layout recommendations

The **Compact**, **Wide** and **One At a Time** layouts arrange panes; the
separate **Row Density** menu sets compact or comfortable row spacing. A
focus-reading mode remains planned (M08 settings, validated in M14.02).

Prototype each density with realistic mail data and long folder names. Preserve
saved splitter sizes where practical, and test keyboard navigation and message
selection while switching layouts.

## Implementation sequence

| Step                                                                             | Status                                                                                                                      |
| -------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------- |
| 1. Record the current main-window behavior and its acceptance tests              | ✅ Offscreen QtTest suites such as `test_EmbeddedPanels`, `test_MainWindowDensity` and `test_SessionRestart`                |
| 2. Build the new shell without changing IMAP, cache or composition behavior      | 🧪 [#35](https://github.com/mikepadiernos/plasma-impresario/issues/35) Completed                                            |
| 3. Validate layouts, accessibility and navigation with representative data       | 🚧 Synthetic Dovecot checks (`test_LiveImapPanels`) pass; a real Plasma session (M15, #319) and accessibility review remain |
| 4. Migrate settings and saved window state only after the new behavior is tested | ⚠️ Pre-shell pane sizes are not migrated; see [#264](https://github.com/mikepadiernos/plasma-impresario/issues/264)         |
