# Plasma Opus integrations

**[Integrations](README.md)** · **[KDE Plasma](kde-plasma.md)** ·
**[File handoff: M05](../ROADMAP.md#m05-desktop-integration-and-provider-support)**
·
**[Workspace settings: M07](../ROADMAP.md#m07-settings-and-workspace-configuration)**
· **[Refinements: M12](../ROADMAP.md#m12-refinements)**

These are proposed workflows between Plasma Impresario and the
[Plasma Opus](https://github.com/mikepadiernos/plasma-opus) file manager, not
implemented integration contracts. Plasma Impresario stays useful without a
companion application.

Status: ✅ done · 📋 planned or proposed

> [!NOTE]
>
> Select and verify companion-app interfaces, authentication and service
> discovery before implementation. The approaches below do not assume that a
> particular Plasma Opus API or service already exists.

## Contents

- [Plasma Opus features to use](#plasma-opus-features-to-use)
- [Calendar and scheduling](#calendar-and-scheduling)
- [Contacts and address books](#contacts-and-address-books)
- [Notes and journal](#notes-and-journal)
- [Workspaces and Plasma Activities](#workspaces-and-plasma-activities)
- [Files and document handoff](#files-and-document-handoff)
- [Themes and appearance](#themes-and-appearance)
- [Actions, reminders and status](#actions-reminders-and-status)

## Plasma Opus features to use

The
[Plasma Opus roadmap](https://github.com/mikepadiernos/plasma-opus/blob/main/docs/ROADMAP.md)
and its
[mail integration proposals](https://github.com/mikepadiernos/plasma-opus/blob/main/docs/integrations/plasma-mail.md)
include features that Plasma Impresario can use through standard desktop
interfaces. Each use must also work with other file managers and without Plasma
Opus. Plasma Opus status was checked against its roadmap on 2026-11-24.
Milestone numbers in the first column are Plasma Opus milestones; those in the
Tracking column are Plasma Impresario milestones. All Plasma Impresario issues
listed are in the Backlog.

| Plasma Opus feature                                                                                                            | Plasma Opus status                         | Use in Plasma Impresario                                                      | Tracking                                                                                                                                    |
| ------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------ | ----------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------- |
| 📂 XDG portal file chooser (M27.01)                                                                                            | 📋 Planned                                 | Its dialog opens files and attachments; the KDE dialog is the fallback        | [M05.01](../ROADMAP.md#m05-desktop-integration-and-provider-support), [#101](https://github.com/mikepadiernos/plasma-impresario/issues/101) |
| ✉️ Compose with selected files                                                                                                 | 📋 Proposed                                | Start a draft with the files selected in the file manager                     | [M05.03](../ROADMAP.md#m05-desktop-integration-and-provider-support), [#263](https://github.com/mikepadiernos/plasma-impresario/issues/263) |
| 📎 Drag and drop into a draft                                                                                                  | 📋 Proposed                                | Attach dropped local and remote files; only local files work today            | [M05.03](../ROADMAP.md#m05-desktop-integration-and-provider-support), [#265](https://github.com/mikepadiernos/plasma-impresario/issues/265) |
| 🗂️ `org.freedesktop.FileManager1` service (M27.01, [plasma-opus#279](https://github.com/mikepadiernos/plasma-opus/issues/279)) | 📋 Planned                                 | Show saved attachments in the file manager                                    | [M05.03](../ROADMAP.md#m05-desktop-integration-and-provider-support), [#266](https://github.com/mikepadiernos/plasma-impresario/issues/266) |
| ☁️ KDE Online Accounts (M27.06) and Nextcloud shares (M29)                                                                     | 📋 Planned                                 | Reuse the same Nextcloud identity and insert reviewed share links into drafts | [M11](../ROADMAP.md#m11-nextcloud-and-digital-asset-management), [#267](https://github.com/mikepadiernos/plasma-impresario/issues/267)      |
| 🧭 Workspaces (M15) and Plasma Activities (M27.01)                                                                             | ✅ Workspaces done · 📋 Activities planned | Follow the same Activities so mail and file workspaces switch together        | [M07](../ROADMAP.md#m07-settings-and-workspace-configuration) workspace settings                                                            |
| 🎨 App-local themes and theme profiles (M17, M18)                                                                              | 📋 Planned                                 | Import its theme or follow it, and export ours for it to import               | [M08](../ROADMAP.md#m08-themes-and-branding-foundation), [#268](https://github.com/mikepadiernos/plasma-impresario/issues/268)              |
| 🖥️ Default file manager registration (M27.01)                                                                                  | ✅ Done                                    | Folders opened through KIO use Plasma Opus when it is the default             | ✅ No change needed                                                                                                                         |

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
  Coordinate with Plasma Opus workspaces through Plasma Activities rather than
  by reading its workspace configuration.
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

## Themes and appearance

- **What:** Carry theme settings over between Plasma Impresario and Plasma Opus
  in both directions, so both apps can share one look without changing the
  desktop's global appearance.
- **How:** Agree on a versioned theme profile format with Plasma Opus, whose M17
  and M18 milestones plan app-local presets, palette roles, theme profiles and
  an optional Qt Style Sheet. Plasma Impresario imports a Plasma Opus profile
  once or keeps following it when it changes, and exports its own profile for
  Plasma Opus to import. Exchange profiles through export and import files or a
  documented shared location; neither app writes the other's configuration.
  Store the choice in KConfig.
- **Recommendations:** Follow the active Plasma color scheme by default and keep
  applying a shared theme opt-in, previewable and reversible. Validate profiles,
  ignore unknown palette roles, keep text, selection and focus readable, never
  execute code from theme files and block external resources in stylesheets.
  Fall back to the Plasma color scheme when a profile is missing, invalid or
  unsupported.
- **Verify:** Test import, export, following changes, invalid and missing
  profiles, and use without Plasma Opus installed, with isolated configuration.

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
