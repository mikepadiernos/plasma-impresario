# Mail UI reference

**[Documentation](../README.md)** · **[Roadmap](../ROADMAP.md)** ·
**[Caveats](../CAVEATS.md)** ·
**[Interface design](../philosophy/designs.md)** ·
**[Backend parity](backend.md)**

This page records the UI reference patterns that Plasma Impresario aims for and
compares them with the current mail interface, as of 2026-11-25. Trojitá is the
primary reference for layout and interaction; Kontact is a secondary reference
for KDE integration and polish.

> [!NOTE]
>
> Parity guides are reference patterns, not a guaranteed description of the
> current implementation. Only the [current baseline](#current-baseline) tables
> describe what the code does today; the parity expectations further down are
> targets.

Legend: ![Done][done] · ![Needs verification][needs-verification] ·
![Planned][planned] · ![Limitation][limitation]

## Contents

- [Current baseline](#current-baseline)
- [Reference hierarchy](#reference-hierarchy)
- [Interaction goals](#interaction-goals)
- [Layout expectations](#layout-expectations)
- [Accessibility and polish](#accessibility-and-polish)
- [Parity checklist](#parity-checklist)

## Current baseline

The Qt Widgets interface inherited from Trojitá now sits in a shell with left,
main and right columns and a footer. The Navigation toolbar docks at the left
window edge by default, and the menu bar is hidden by default. With default
settings it keeps the classic three-pane look. The
[current interface](../philosophy/designs.md#current-interface) section of the
design guide describes the layouts, embedded panels, saved sizes and settings
keys; this page does not repeat them.

> [!NOTE]
>
> The shell, sidebar placement and embedded panels were built in M02; M03 and
> M04 added mail operations, the unified inbox and the reading flow. All four
> milestones are merged into `main`. Offscreen tests and opt-in live IMAP tests
> against a disposable synthetic Dovecot (`tools/dev-container/`) cover them; a
> real Plasma session has not been verified. M15
> ([#319](https://github.com/mikepadiernos/plasma-impresario/issues/319)) tracks
> that check.

The main window is `MainWindow` in `src/Gui/Window.cpp`; the folder sidebar is
`AccountSidebar` and the panels are in `src/Gui/` (`AttachmentPanel`,
`MessageDetailsPanel`, `ContactPanel`, `RelatedMessagesPanel`).

| Area               | Status                                    | Current behavior                                                                                                                                                                                 | Tracking                                                                                                                                                                                                                                                 |
| ------------------ | ----------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Shell regions      | ![Needs verification][needs-verification] | Header, left, main and right columns, and a footer; side columns and the footer appear only while they hold content                                                                              | M02 [#35](https://github.com/mikepadiernos/plasma-impresario/issues/35) (Merged; Plasma check [#315](https://github.com/mikepadiernos/plasma-impresario/issues/315) Defined)                                                                             |
| Sidebar and layout | ![Needs verification][needs-verification] | Mailbox tree in either side column; message list and reader stacked (compact), side by side (wide) or one panel at a time; region sizes saved per layout                                         | [#35](https://github.com/mikepadiernos/plasma-impresario/issues/35)                                                                                                                                                                                      |
| Embedded panels    | ![Needs verification][needs-verification] | Attachments (on by default), Message Details, Contacts and Related Messages (off by default), each with Show, Hide When Empty and a placement                                                    | [#35](https://github.com/mikepadiernos/plasma-impresario/issues/35) · ![Limitation][limitation] [panel caveat](../CAVEATS.md#related-messages-and-message-details-panels)                                                                                |
| Focus cycling      | ![Needs verification][needs-verification] | <kbd>F6</kbd> and <kbd>Shift</kbd>+<kbd>F6</kbd> move through the header, account selector, folder tree, quick search, message list, reader and shown panels in screen order, also right to left | M02 [#80](https://github.com/mikepadiernos/plasma-impresario/issues/80) (Merged), M05 [#349](https://github.com/mikepadiernos/plasma-impresario/issues/349) (Merged); Plasma check [#315](https://github.com/mikepadiernos/plasma-impresario/issues/315) |
| Multiple accounts  | ![Done][done]                             | One window holds several IMAP accounts; a toolbar selector switches between them and flags accounts that need attention                                                                          | [#222](https://github.com/mikepadiernos/plasma-impresario/issues/222) · ![Limitation][limitation] [multiple accounts caveat](../CAVEATS.md#multiple-accounts) ([#262](https://github.com/mikepadiernos/plasma-impresario/issues/262))                    |
| Account grouping   | ![Done][done]                             | With two or more accounts, the folder sidebar shows a header per account with its status and unread count; the active account's folders expand below it                                          | M02 [#80](https://github.com/mikepadiernos/plasma-impresario/issues/80), M03 [#82](https://github.com/mikepadiernos/plasma-impresario/issues/82)                                                                                                         |
| Unified inbox      | ![Done][done]                             | Combines selected accounts' inboxes while keeping each message's source account and folder, also when replying                                                                                   | M03 [#223](https://github.com/mikepadiernos/plasma-impresario/issues/223), M04 [#224](https://github.com/mikepadiernos/plasma-impresario/issues/224)                                                                                                     |
| Message security   | ![Done][done]                             | The Message Details panel shows the reader's signature verification and decryption results                                                                                                       | M04 [#303](https://github.com/mikepadiernos/plasma-impresario/issues/303) · ![Limitation][limitation] [message security caveat](../CAVEATS.md#message-security) ([#442](https://github.com/mikepadiernos/plasma-impresario/issues/442))                  |
| Saved searches     | ![Done][done]                             | Folder and unread counts, and saved-search entries; saved searches and folder expansion are kept per account                                                                                     | [#76](https://github.com/mikepadiernos/plasma-impresario/issues/76)                                                                                                                                                                                      |
| Row density        | ![Done][done]                             | View menu density selector for the folder tree and message list                                                                                                                                  | [#75](https://github.com/mikepadiernos/plasma-impresario/issues/75)                                                                                                                                                                                      |
| Session restore    | ![Done][done]                             | Layout, density and the selected folder and message are restored; UIDVALIDITY is checked before selecting a saved message                                                                        | [#81](https://github.com/mikepadiernos/plasma-impresario/issues/81)                                                                                                                                                                                      |
| Connection status  | ![Done][done]                             | The footer reports connection activity, offline state, network errors and authentication failures; background-account errors raise a desktop notification                                        | [#79](https://github.com/mikepadiernos/plasma-impresario/issues/79), [#262](https://github.com/mikepadiernos/plasma-impresario/issues/262)                                                                                                               |
| Tray icon          | ![Done][done]                             | Optional tray icon with the current account's unread count and offline state; network errors appear as tray messages while the window is inactive                                                | Inherited from Trojitá                                                                                                                                                                                                                                   |
| Diagnostics        | ![Done][done]                             | Debug docks: Everything, IMAP Tasks, MIME Tree and Protocol log                                                                                                                                  | Inherited from Trojitá                                                                                                                                                                                                                                   |

The interface does not yet provide the following:

| Area                               | Status              | Tracking                                                                      |
| ---------------------------------- | ------------------- | ----------------------------------------------------------------------------- |
| Server-side related-message search | ![Planned][planned] | M05 [#302](https://github.com/mikepadiernos/plasma-impresario/issues/302)     |
| My Day / Agenda panel              | ![Planned][planned] | M06.04 in [M06](../ROADMAP.md#m06-desktop-integration-and-provider-support)   |
| Plasma new-mail notifications      | ![Planned][planned] | M06.01 and [M07](../ROADMAP.md#m07-output-notifications-and-rules-automation) |
| Optional tabs                      | ![Planned][planned] | [M08](../ROADMAP.md#m08-settings-and-workspace-configuration)                 |
| App-local themes                   | ![Planned][planned] | [M09](../ROADMAP.md#m09-themes-and-branding-foundation)                       |

See [caveats and security](../CAVEATS.md) for other limitations.

## Reference hierarchy

### Primary: Trojitá

Trojitá is the baseline for the core mail UI because it is a Qt/KDE IMAP client
focused on correctness, clarity and lightweight desktop use.

Parity expectations:

- Account and folder navigation is compact, predictable and visually subordinate
  to the message area.
- The message list and reading pane feel like a focused work surface rather than
  a heavy dashboard.
- The layout keeps a clear split between the folder and account hierarchy, the
  message list and the reader.
- The interface favors readability, low visual noise and efficient
  keyboard-first use.
- Toolbar actions are limited to the most important mailbox operations and are
  easy to scan.

### Secondary: Kontact suite

Kontact is the KDE-native reference for desktop cohesion, integration patterns
and system-level polish.

Parity expectations:

- The app feels at home in KDE and Plasma, with consistent spacing, iconography
  and interaction patterns.
- Mail fits the broader desktop workflow and the conventions of the Kontact
  suite.
- Status, notifications and account state read as part of a coherent KDE
  application ecosystem rather than as standalone app behavior.
- The visual language stays modern and desktop-native without copying the exact
  KMail layout.

## Interaction goals

- Define a clear reading workflow: account and folder selection, message list,
  reader and context actions.
- Keep the primary mail work surface readable and uncluttered, even with dense
  folders.
- Favor compact density and efficient navigation over decorative chrome.
- Treat keyboard navigation and accessible focus states as first-class
  behaviors.
- Keep message operations clearly distinct from account-level management.

## Layout expectations

- Folder and account navigation stays visually distinct and easy to scan.
- Message list rows communicate status, unread state and thread context without
  overloading the UI.
- The reader keeps a calm, document-like reading experience with strong
  typography and contrast.
- Search and filter controls are easy to discover without dominating screen
  space.
- Compact actions such as reply, forward, archive and mark read stay close to
  the message context.

## Accessibility and polish

- Respect KDE accessibility and contrast conventions.
- Keep focus order obvious and predictable for keyboard users.
- Use clear unread and read states and action affordances.
- Keep status feedback visible without becoming intrusive.
- Keep light and dark theme behavior consistent across the app shell and message
  content.

## Parity checklist

| Area                                  | Status                                    | Current state                                                                                                                                                                                                                      |
| ------------------------------------- | ----------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Message list and reader behavior      | ![Needs verification][needs-verification] | The list and reader were reworked in [M03](../ROADMAP.md#m03-mail-operations-sorting-and-folder-management) and [M04](../ROADMAP.md#m04-reading-and-conversation-flow), both merged; a real Plasma session check is pending in M15 |
| Folder and account layout             | ![Needs verification][needs-verification] | Sidebar placement, the account selector and per-account headers in the folder sidebar exist                                                                                                                                        |
| Search and filter UI                  | ![Planned][planned]                       | Inherited search and saved searches exist; cross-account search, virtual folders and a local index are planned in [M05](../ROADMAP.md#m05-search-and-indexing)                                                                     |
| Notifications and status feedback     | ![Planned][planned]                       | Footer connection status and background-account notifications exist; new-mail notifications are planned in M06.01 and M07                                                                                                          |
| Theme and contrast support            | ![Planned][planned]                       | Follows the Qt and Plasma palette and icon theme; contrast and high-DPI checks are not recorded; app themes are planned in M09                                                                                                     |
| Keyboard navigation and focus clarity | ![Needs verification][needs-verification] | <kbd>F6</kbd> region cycling and M04 desktop-heavy keyboard navigation ([#92](https://github.com/mikepadiernos/plasma-impresario/issues/92)) exist; not checked in Plasma                                                          |
| KDE and Plasma integration            | ![Planned][planned]                       | See the [KDE Plasma integration guide](../integrations/kde-plasma.md)                                                                                                                                                              |

[done]: https://img.shields.io/badge/Done-1a7f37
[limitation]: https://img.shields.io/badge/Limitation-bc4c00
[needs-verification]: https://img.shields.io/badge/Needs_verification-0969da
[planned]: https://img.shields.io/badge/Planned-6e7781
