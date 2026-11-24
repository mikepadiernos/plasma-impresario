# Mail UI reference

This page records the current UI baseline and the parity target for Plasma
Impresario. Trojitá is the primary reference for layout and interaction; Kontact
is a secondary reference for KDE integration and polish.

> [!NOTE]
>
> The current executable still presents the Trojitá Qt Widgets interface. The
> expectations below describe product goals, not features of a completed Plasma
> Impresario redesign.

## Current baseline

The main window has a toolbar/menu and three work areas: a mailbox tree, a
message list and a message reader. Compact mode stacks the list above the reader
beside the mailbox tree; wide mode places all three side by side.
One-panel-at-a-time navigation and persisted splitter/layout state are also
implemented. An embedded attachments panel below the reader lists the current
message's attachments; View › Embedded Panels turns it off or hides it while
the message has none. A toolbar selector switches between accounts and marks accounts
that need attention. Optional diagnostic docks expose task, MIME, model and
protocol information.

The baseline does not provide the proposed unified multi-account navigation
(accounts grouped in the folder tree), workspace surfaces or modern Plasma
notification experience. See
[caveats and security](../CAVEATS.md) for security and dependency limitations.

## Reference hierarchy

### Primary: Trojitá

Trojitá is the baseline for the core mail UI because it is a Qt/KDE IMAP client
focused on correctness, clarity, and lightweight desktop use.

Parity expectations:

- Account and folder navigation should be compact, predictable, and visually
  subordinate to the message area.
- The message list and reading pane should feel like a focused work surface
  rather than a heavy dashboard.
- Layout should support a clear split between folder/account hierarchy, message
  list, and content reading.
- The interface should favor readability, low visual noise, and efficient
  keyboard-first use.
- Toolbar actions should be limited to the most important mailbox operations and
  kept easy to scan.

### Secondary: Kontact suite

Kontact provides the KDE-native reference for desktop cohesion, integration
patterns, and system-level polish.

Parity expectations:

- The app should feel at home in KDE and Plasma, with consistent spacing,
  iconography, and interaction patterns.
- Mail should integrate cleanly with the broader desktop workflow and
  conventions used by the Kontact suite.
- Status, notifications, and account state should read as part of a coherent KDE
  application ecosystem rather than as standalone app behavior.
- The visual language should remain modern and desktop-native without copying
  the exact KMail layout.

## Interaction goals

- Define a clear reading workflow: account/folder selection, message list,
  reading pane, and context actions.
- Keep the primary mail work surface readable and uncluttered even with dense
  folders.
- Favor compact density and efficient navigation over decorative chrome.
- Support keyboard navigation and accessible focus states as first-class
  behaviors.
- Preserve a strong distinction between message operations and account-level
  management.

## Layout expectations

- Folder and account navigation should remain visually distinct and easy to
  scan.
- Message list rows should communicate status, unread state, and thread context
  without overloading the UI.
- The reading pane should preserve a calm, document-like reading experience with
  strong typography and contrast.
- Search and filter controls should be readily discoverable without dominating
  screen space.
- Compact actions such as reply, forward, archive, and mark read should remain
  close to the message context.

## Accessibility and polish

- Respect KDE accessibility and contrast conventions.
- Ensure focus order is obvious and predictable for keyboard users.
- Use clear unread/read status and action affordances.
- Keep status feedback visible without becoming intrusive.
- Maintain consistent light/dark theme behavior across the app shell and message
  content.

## Parity checklist

- Message list and reading pane behavior
- Folder and account layout
- Search and filter UI
- Notifications and status feedback
- Theme and contrast support
- Keyboard navigation and focus clarity
- Desktop integration consistency with KDE/Plasma
