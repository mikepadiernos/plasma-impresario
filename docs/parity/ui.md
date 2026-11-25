# Mail UI reference

**[Documentation](../README.md)** ·
**[Interface design](../philosophy/designs.md)** ·
**[Backend parity](backend.md)** · **[Roadmap](../ROADMAP.md)** ·
**[Caveats](../CAVEATS.md)**

This page compares the current mail interface with the parity targets for Plasma
Impresario. Trojitá is the primary reference for layout and interaction; Kontact
is a secondary reference for KDE integration and polish.

Status: ✅ done or verified · 🧪 implemented, needs verification · 📋 planned ·
⚠️ limitation

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
> The shell, sidebar placement and embedded panels are on
> `feature/m02-shell-next` and are not yet merged into `main`. Offscreen tests
> and opt-in live IMAP tests against a synthetic Dovecot
> (`tools/dev-container/`) cover them; a real Plasma session has not been
> verified.

The main window is `MainWindow` in `src/Gui/Window.cpp`; the panels are in
`src/Gui/` (`AttachmentPanel`, `MessageDetailsPanel`, `ContactPanel`,
`RelatedMessagesPanel`).

| Area                  | Status | Current behavior                                                                                                                                         | Tracking                                                                                                                                                                                                       |
| --------------------- | ------ | -------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 🧭 Shell regions      | 🧪     | Header, left, main and right columns, and a footer; side columns and the footer appear only while they hold content                                      | M02 [#35](https://github.com/mikepadiernos/plasma-impresario/issues/35) (Review)                                                                                                                               |
| 🗂️ Sidebar and layout | 🧪     | Mailbox tree in either side column; message list and reader stacked (compact), side by side (wide) or one panel at a time; region sizes saved per layout | [#35](https://github.com/mikepadiernos/plasma-impresario/issues/35)                                                                                                                                            |
| 🧩 Embedded panels    | 🧪     | Attachments (on by default), Message Details, Contacts and Related Messages (off by default), each with Show, Hide When Empty and a placement            | [#35](https://github.com/mikepadiernos/plasma-impresario/issues/35) · ⚠️ [panel caveat](../CAVEATS.md#related-messages-and-message-details-panels)                                                             |
| ⌨️ Focus cycling      | 🧪     | <kbd>F6</kbd> and <kbd>Shift</kbd>+<kbd>F6</kbd> move between the account selector, folder tree, message list, reader and shown panels                   | M02 [#80](https://github.com/mikepadiernos/plasma-impresario/issues/80) (Development)                                                                                                                          |
| 📬 Multiple accounts  | ✅     | One window holds several IMAP accounts; a toolbar selector switches between them and flags accounts that need attention                                  | [#222](https://github.com/mikepadiernos/plasma-impresario/issues/222) · ⚠️ [multiple accounts caveat](../CAVEATS.md#multiple-accounts) ([#262](https://github.com/mikepadiernos/plasma-impresario/issues/262)) |
| 🔍 Saved searches     | ✅     | Folder and unread counts, and saved-search entries; saved searches and folder expansion are kept per account                                             | [#76](https://github.com/mikepadiernos/plasma-impresario/issues/76)                                                                                                                                            |
| 📏 Row density        | ✅     | View menu density selector for the folder tree and message list                                                                                          | [#75](https://github.com/mikepadiernos/plasma-impresario/issues/75)                                                                                                                                            |
| 💾 Session restore    | ✅     | Layout, density and the selected folder and message are restored; UIDVALIDITY is checked before selecting a saved message                                | [#81](https://github.com/mikepadiernos/plasma-impresario/issues/81)                                                                                                                                            |
| 📶 Connection status  | ✅     | The footer reports connection activity, offline state, network errors and authentication failures                                                        | [#79](https://github.com/mikepadiernos/plasma-impresario/issues/79)                                                                                                                                            |
| 🔔 Tray icon          | ✅     | Optional tray icon with the current account's unread count and offline state; network errors appear as tray messages while the window is inactive        | Inherited from Trojitá                                                                                                                                                                                         |
| 🛠️ Diagnostics        | ✅     | Debug docks: Everything, IMAP Tasks, MIME Tree and Protocol log                                                                                          | Inherited from Trojitá                                                                                                                                                                                         |

The interface does not yet provide the following:

| Area                                           | Status | Tracking                                                                      |
| ---------------------------------------------- | ------ | ----------------------------------------------------------------------------- |
| 📬 Accounts grouped in the folder tree         | 📋     | M02 [#80](https://github.com/mikepadiernos/plasma-impresario/issues/80)       |
| 📥 Unified inbox across accounts               | 📋     | [M03](../ROADMAP.md#m03-mail-operations-sorting-and-folder-management)        |
| 🔐 Verified signature results in Details panel | 📋     | M04 [#303](https://github.com/mikepadiernos/plasma-impresario/issues/303)     |
| 🔍 Server-side related-message search          | 📋     | M04 [#302](https://github.com/mikepadiernos/plasma-impresario/issues/302)     |
| 📅 My Day / Agenda panel                       | 📋     | M05.04 in [M05](../ROADMAP.md#m05-desktop-integration-and-provider-support)   |
| 🔔 Plasma notifications                        | 📋     | M05.01 and [M06](../ROADMAP.md#m06-output-notifications-and-rules-automation) |
| 🗂️ Optional tabs                               | 📋     | [M07](../ROADMAP.md#m07-settings-and-workspace-configuration)                 |
| 🎨 App-local themes                            | 📋     | [M08](../ROADMAP.md#m08-themes-and-branding-foundation)                       |

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

| Area                                     | Status | Current state                                                                                                                                    |
| ---------------------------------------- | ------ | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| 📬 Message list and reader behavior      | 🧪     | Inherited list and reader placed in the new shell; reading-pane work is planned in [M04](../ROADMAP.md#m04-reading-search-and-conversation-flow) |
| 🗂️ Folder and account layout             | 🧪     | Sidebar placement and the account selector exist; accounts are not grouped in the folder tree                                                    |
| 🔍 Search and filter UI                  | 📋     | Inherited search and saved searches exist; scoped search and saved filters are planned in M04                                                    |
| 🔔 Notifications and status feedback     | 📋     | Footer connection status exists; Plasma notifications are planned in M05.01 and M06                                                              |
| 🎨 Theme and contrast support            | 📋     | Follows the Qt and Plasma palette and icon theme; contrast and high-DPI checks are not recorded; app themes are planned in M08                   |
| ⌨️ Keyboard navigation and focus clarity | 🧪     | <kbd>F6</kbd> region cycling exists; desktop-heavy keyboard navigation is planned in M04                                                         |
| 🖥️ KDE and Plasma integration            | 📋     | See the [KDE Plasma integration guide](../integrations/kde-plasma.md)                                                                            |
