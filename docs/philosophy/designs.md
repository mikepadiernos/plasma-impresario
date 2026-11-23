# Plasma Mail interface design

**[Documentation](../README.md)** · **[Scope](scope.md)** ·
**[UI parity](../parity/ui.md)** ·
**[Roadmap](../ROADMAP.md)** · **[Inspirations](inspirations.md)**

This guide separates the current Trojitá interface from the proposed Plasma
Mail experience. The [UI parity guide](../parity/ui.md) records the current
layout; target designs here are not implementation claims.

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

The compact pane layout stacks the list above the reader beside the folder
tree. Wide layout places all three side by side. One-panel-at-a-time navigation
is also available. Splitter sizes and the selected layout are saved.

## Proposed Plasma Mail shell

```text
┌──────────────────────────────────────────────────────────────┐
│ Header: search, compose, account and connection status       │
├───────────────────┬──────────────────────────────────────────┤
│ Navigation        │ Message list                             │
│ accounts/folders  ├──────────────────────────────────────────┤
│ saved views       │ Reader: message, metadata, attachments    │
├───────────────────┴──────────────────────────────────────────┤
│ Status: sync progress, errors and background work           │
└──────────────────────────────────────────────────────────────┘
```

| Region | Design responsibility |
| --- | --- |
| Header | Global search, compose, common actions and account/connection state |
| Navigation | Account groups, folder hierarchy and saved views |
| Message list | Sender, subject, date, unread state, threading and selection |
| Reader | Message metadata and body, attachments and context-aware actions |
| Status | Actionable sync, connection and background-task feedback |

Account groups, saved views, workspace context and rich status surfaces remain
planned. Evolve the working mailbox/list/reader flow instead of replacing it
before the redesigned shell has equivalent behavior.

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
2. Prototype the new shell without changing IMAP, cache or composition
   behavior.
3. Validate layouts, accessibility and navigation with representative data.
4. Migrate settings and saved window state only after the new behavior is
   tested.
