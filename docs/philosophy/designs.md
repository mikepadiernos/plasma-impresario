# Plasma Impresario interface design

**[Documentation](../README.md)** · **[Scope](scope.md)** ·
**[UI parity](../parity/ui.md)** · **[Roadmap](../ROADMAP.md)** ·
**[Inspirations](inspirations.md)**

This guide separates the current Trojitá interface from the proposed Plasma
Impresario experience. The [UI parity guide](../parity/ui.md) records the
current layout; target designs here are not implementation claims.

## Current interface

The Qt Widgets application has a toolbar/menu and three work areas:

```text
Toolbar / menu
┌────────────────────┬──────────────────────────────────────────┐
│ Mailbox tree       │ Message list                             │
│                    ├──────────────────────────────────────────┤
│                    │ Message reader                           │
└────────────────────┴──────────────────────────────────────────┘
```

The compact pane layout stacks the list above the reader beside the folder tree.
Wide layout places all three side by side. One-panel-at-a-time navigation is
also available. Splitter sizes and the selected layout are saved.

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
  design.
- **Header and footer:** Keep global actions and connection context accessible
  in the header, and provide sync progress, errors and background-work status in
  the footer.

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

## Proposed interface widgets

These are in-app interface components, not Plasma desktop widgets. They adapt
ideas from the [inspirations](inspirations.md); they are design candidates, not
claims that every component is implemented.

- **Search:** Search the current folder or selected accounts, with visible
  scope, filters and saved searches. Inspired by Vivaldi Mail and Aqua Mail.
- **Account and folder navigation:** Show account groups, folder hierarchy and
  unread counts with clear account context. Inspired by Trojitá and Aqua Mail.
- **Unified inbox:** Combine selected accounts while identifying each message's
  source and keeping actions account-aware. Inspired by Vivaldi Mail.
- **Message list and quick actions:** Present sender, subject, date and unread
  state with accessible archive, move, flag and delete actions. Inspired by Aqua
  Mail and The Bat!.
- **Filters and sorting:** Expose active filters, sort order and threading
  without obscuring the current folder or selection. Inspired by Vivaldi Mail
  and The Bat!.
- **Conversation reader:** Display message content, metadata, quoted text and
  reply actions in a focused reading pane. Inspired by Trojitá and Aqua Mail.
- **Compose and recipients:** Provide identity selection, recipient entry,
  drafts and sending feedback. Inspired by desktop mail workflows in Trojitá and
  Kontact.
- **Attachments:** Show file names, sizes, loading progress and safe open or
  save actions. Inspired by Aqua Mail and The Bat!.
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
  focus restoration.
- Do not use color as the only way to convey unread, connection or security
  state.
- Validate narrow windows, high-DPI scaling, long translations, RTL layouts and
  screen-reader names.
- Keep destructive and external-content actions explicit and recoverable.

## Layout recommendations

The existing **compact**, **wide** and **one-panel-at-a-time** modes describe
pane arrangement. Proposed compact/comfortable density and focus-reading modes
are separate product settings and are not implemented density controls.

Prototype each density with realistic mail data and long folder names. Preserve
saved splitter sizes where practical, and test keyboard navigation and message
selection while switching layouts.

## Implementation sequence

1. Record the current main-window behavior and its acceptance tests.
2. Prototype the new shell without changing IMAP, cache or composition behavior.
3. Validate layouts, accessibility and navigation with representative data.
4. Migrate settings and saved window state only after the new behavior is
   tested.
