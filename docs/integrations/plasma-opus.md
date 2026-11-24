# Plasma Opus integrations

**[Integrations](README.md)** · **[KDE Plasma](kde-plasma.md)** ·
**[Workspace settings: M07](../ROADMAP.md#m07-settings-and-workspace-configuration)**
· **[Refinements: M12](../ROADMAP.md#m12-refinements)**

These are proposed cross-app workflows, not implemented integration contracts.
Plasma Impresario should remain useful without a companion application.

> [!NOTE]
>
> Select and verify companion-app interfaces, authentication and service
> discovery before implementation. The approaches below do not assume that a
> particular Plasma Opus API or service already exists.

## Calendar and scheduling

- **What:** Create or update an event from a message, review meeting context and
  consult availability when the calendar service supports it.
- **How:** Offer an explicit action that passes selected message context to a
  calendar integration. Let the user review the target calendar, title, time,
  time zone, attendees and any copied message content before saving.
- **Recommendations:** Avoid automatically creating events or exposing message
  contents. Show unavailable calendars, permission errors, time-zone ambiguity
  and conflicts without changing the original message.
- **Verify:** Exercise cancellation, unavailable services, denied access,
  conflicting events and event creation with isolated data.

## Contacts and address books

- **What:** Find or open a sender or recipient, create a contact and reuse
  addresses when composing.
- **How:** Use an available address-book provider or a verified companion
  interface. Pass only the selected address and fields needed for the action;
  let users choose the destination address book and review edits.
- **Recommendations:** Distinguish duplicate contacts and account-specific
  sources. Do not silently upload address data or assume every provider is
  writable.
- **Verify:** Test duplicate matches, read-only sources, missing contacts,
  denied access and a provider becoming unavailable.

## Notes and journal

- **What:** Capture selected message text or a follow-up as a note or journal
  entry without losing the mail context.
- **How:** Provide an explicit capture action with a preview, destination and
  editable content. Where supported, include a stable reference back to the
  source message rather than copying the whole message by default.
- **Recommendations:** Make copied content and destination visible before
  saving. If a link cannot be resolved later, explain the failure rather than
  exposing unrelated mail or treating the link as a durable backup.
- **Verify:** Check cancellation, permission failures, deleted or moved source
  messages and recovery of unsaved note content.

## Workspaces and Plasma Activities

- **What:** Allow email accounts to be attached to workspaces and thereby
  associated with the corresponding Plasma Activities.
- **How:** Provide settings to manage the account-to-workspace association and
  use verified workspace and Activity context to restore the relevant view. Keep
  account identity and authorization separate from the workspace mapping.
- **Recommendations:** Make the active context and any filtering visible.
  Changing an Activity must not silently delete, move or modify mail; do not
  assume a workspace is an account or that every account belongs to one. Define
  behavior for unassigned accounts and unavailable context services.
- **Verify:** Switch and restore contexts, edit or remove associations, and test
  unavailable services without leaking account data between contexts.

## Files and document handoff

- **What:** Open, share or forward attachments and related documents through
  suitable desktop or companion applications.
- **How:** Use the desktop file workflow for local files and a verified
  companion interface for other handoffs. Show the selected files, destination,
  progress and errors before completing an action.
- **File open dialog:** Request attachment and file selection through the XDG
  portal file chooser. When Plasma Opus is installed and serves the portal (a
  planned Plasma Opus feature), its file open dialog is used; otherwise the
  standard KDE dialog appears. Keep the chosen files, cancellation and errors
  identical in both cases and do not depend on Plasma Opus being present.
- **Recommendations:** Treat attachments as untrusted and require explicit
  sharing. Explain permissions and link visibility; do not claim a file was
  shared if upload or handoff failed.
- **Verify:** Test cancellation, unavailable applications, denied permissions,
  incomplete downloads and failed uploads with isolated files. Open files with
  and without Plasma Opus serving the portal file chooser.

## Actions, reminders and status

- **What:** Keep shared actions such as Open, Share, Save, Schedule and Add
  contact consistent; coordinate reminders and unread status without noise.
- **How:** Map each action to the appropriate available service and keep its
  account and message context explicit. Apply user notification preferences and
  group repetitive status updates.
- **Recommendations:** Keep sync and delivery failures visible in the mail
  client even when companion status surfaces are unavailable. Avoid duplicating
  reminders or sending message previews to another app without user control.
- **Verify:** Exercise missing services, disabled notifications, duplicate
  events, account switching and action failures.
