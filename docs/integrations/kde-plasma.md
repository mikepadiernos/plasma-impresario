# KDE Plasma integrations

**[Integrations](README.md)** · **[Plasma Opus](plasma-opus.md)** ·
**[Roadmap: M06](../ROADMAP.md#m06-desktop-integration-and-provider-support)** ·
**[UI parity](../parity/ui.md)** · **[Caveats](../CAVEATS.md)**

Plasma Impresario targets KDE Plasma only. This guide lists the intended desktop
integration, suggested approaches and decisions to validate; the status table
records what the client does today.

Status: ✅ done or verified · 🧪 implemented, needs verification · 📋 planned ·
⚠️ limitation

> [!IMPORTANT]
>
> Verify service availability, privacy and recovery in a real Plasma session. No
> integration below has been verified in one yet.

## Status

Roadmap work is in
[M06](../ROADMAP.md#m06-desktop-integration-and-provider-support): **M06.01**
KDE and Plasma services, **M06.02** Plasma widgets and desktop feedback, and
**M06.03** file manager handoff. Baloo search is part of [M05](../ROADMAP.md#m05-search-and-indexing).
Notification controls are also part of
[M07](../ROADMAP.md#m07-output-notifications-and-rules-automation), and app
themes are part of [M09](../ROADMAP.md#m09-themes-and-branding-foundation).

| Area                                                                     | Status | Current behavior                                                                                                                                                              | Roadmap        |
| ------------------------------------------------------------------------ | ------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------- |
| 🪟 [Windows and session lifecycle](#windows-and-session-lifecycle)       | 🧪     | Shell layout, panel and density preferences are stored with KConfig; the selected folder and message with `KSharedConfig::openStateConfig()`. No Plasma session-restore hooks | M06.01         |
| 🔔 [Notifications and status](#notifications-and-status)                 | ⚠️     | The footer shows connection status; an optional tray icon shows unread counts and network errors. KNotification is not used                                                   | M06.01, M07    |
| 🧩 [Plasma widgets](#plasma-widgets)                                     | 📋     | None                                                                                                                                                                          | M06.02         |
| 📎 [Files and attachments](#files-and-attachments)                       | ⚠️     | `QFileDialog` for open and save; attachments open through desktop file associations; the composer accepts dropped local files only. KIO is not used                           | M06.01, M06.03 |
| 🔍 [Search and indexing](#search-and-indexing)                           | 📋     | Search runs on the IMAP server for the selected mailbox; Baloo, KRunner and a local index are not used                                                                        | M05, M06.01    |
| 🔑 [Settings and credentials](#settings-and-credentials)                 | ⚠️     | Account settings use `QSettings`; passwords use the QtKeychain or cleartext plugin ([details](../CAVEATS.md#security-and-data-protection))                                    | M06.01         |
| 🎨 [Appearance and accessibility](#appearance-and-accessibility)         | 🧪     | Theme icons and a row density selector; accessibility, high-contrast and scaling checks are not recorded                                                                      | M06.01, M09    |
| 🚀 [Desktop actions and availability](#desktop-actions-and-availability) | 🧪     | The desktop entry registers the `mailto:` scheme handler and a Composer action                                                                                                | M06.01         |

## Windows and session lifecycle

- **What:** Start and restore the app predictably, preserve window and pane
  state, and handle shutdown without losing drafts or unfinished work.
- **How:** Follow Plasma window and session conventions. Store persistent
  preferences with KConfig and session-specific state with
  `KSharedConfig::openStateConfig()`. Restore windows and navigation only after
  their account context is available.
- **Recommendations:** Keep window state separate from account credentials. Make
  unfinished sends and background tasks visible before exit; do not imply that
  an interrupted operation succeeded.
- **Verify:** Restart and restore multiple windows, unavailable accounts and
  interrupted drafts in a Plasma session.

## Notifications and status

- **What:** Report new mail, sync failures and account state without
  interrupting reading. Provide per-account and per-folder controls.
- **How:** Use KDE notification facilities and desktop preferences. Group
  repeated events, expose useful actions when available, and keep persistent
  errors visible in the application.
- **Recommendations:** Offer privacy-conscious previews, honor notification
  settings and do-not-disturb behavior, and avoid placing credentials or message
  bodies in diagnostic output. Do not rely on a notification as the only record
  of a failure. Background account errors currently appear only in the account
  selector; see [multiple accounts](../CAVEATS.md#multiple-accounts).
- **Verify:** Exercise notification suppression, grouped new mail, unavailable
  notification services and retry after a sync failure.

## Plasma widgets

Planned in **M06.02** (Plasma widgets and desktop feedback).

- **What:** Offer optional Plasma widgets for the desktop or panel, such as an
  unread-count summary, account sync status, a compose shortcut and a scrolling
  unread-mail marquee. The marquee can draw inspiration from
  [The Bat!'s MailTicker](https://www.ritlabs.com/en/support/tips-and-tricks/7593/),
  which displays sender and subject information for unread messages.
- **How:** Design Plasma widget interfaces around a verified, minimal
  application service rather than direct access to mail storage or credentials.
  Allow users to choose accounts and folders, visible fields, display timing and
  whether the ticker scrolls. A message action opens the corresponding message
  in the mail client.
- **Recommendations:** Keep widgets optional and respect Plasma themes, panel
  sizes, keyboard access and reduced-motion preferences. Do not expose sender or
  subject details by default without a privacy choice. Show unavailable or stale
  status clearly when the client or its service is not running; never report
  failed synchronization as successful.
- **Verify:** Test account and folder filtering, message navigation, privacy
  settings, non-scrolling operation, panel and desktop layouts, service shutdown
  and recovery using isolated mail data.

## Files and attachments

KDE file dialogs and KIO are part of **M06.01**; compose requests, dropped
remote files and showing saved attachments in the file manager are part of
**M06.03** (see
[Plasma Opus integrations](plasma-opus.md#plasma-opus-features-to-use)).

- **What:** Save, open, preview and share attachments using familiar Plasma file
  workflows.
- **How:** Prefer KDE file dialogs, KIO and desktop file associations where
  appropriate. Support drag-and-drop and Open With while preserving download
  progress, cancellation and error reporting.
- **Recommendations:** Treat message content and filenames as untrusted. Confirm
  risky actions, avoid exposing incomplete files as completed downloads, and
  clean up temporary files at the appropriate lifecycle point. Use user-selected
  destinations rather than silently overwriting files.
- **Verify:** Test local and KIO destinations, unavailable or read-only
  locations, interrupted downloads, duplicate filenames and missing file
  associations with isolated attachments.

## Search and indexing

Mail search and the optional local index are planned in
[M05](../ROADMAP.md#m05-search-and-indexing); the KRunner provider is part of
**M06.01** (see [Plasma Opus search and
indexing](plasma-opus.md#search-and-indexing)).

- **What:** Find saved attachments and exported messages with Plasma's file
  search, and optionally find mail from KRunner (Plasma Search).
- **How:** Open KIO `baloosearch:` URLs in the default file manager for files on
  disk. Keep the mail index in the app's SQLite cache, and serve a KRunner
  provider from it only when the user enables it.
- **Recommendations:** Never send message content to Baloo. Show only subject,
  sender and date in KRunner and return nothing when the index is off. Explain
  when Baloo file indexing is disabled or excludes the save location, and offer
  a live search instead.
- **Verify:** Test with Baloo indexing on, off and excluding the save location,
  and with the KRunner provider enabled and disabled, using isolated files and
  synthetic messages.

## Settings and credentials

- **What:** Integrate account preferences and credentials with KDE settings and
  credential storage while keeping accounts isolated.
- **How:** Use KConfig for preferences and a KWallet-compatible credential
  provider where available. Migrate existing settings and secrets explicitly; do
  not treat an optional plugin as a guaranteed secure store.
- **Recommendations:** Prefer secure credential storage, explain when a wallet
  is locked or unavailable, and require explicit consent for cleartext storage.
  Never silently retry authentication over plaintext after TLS fails.
- **Verify:** Test wallet unavailable, locked and unlocked states; account
  removal; migration; and restart without exposing credentials in logs.

> [!WARNING]
>
> Profiles that already store cleartext passwords keep the cleartext plugin,
> which writes secrets to `QSettings`. See
> [security and data protection](../CAVEATS.md#security-and-data-protection).

## Appearance and accessibility

- **What:** Follow the active Plasma color scheme, accent, icon theme and light
  or dark appearance, including high-contrast and scaled displays.
- **How:** Use theme-provided colors and icons, prefer symbolic icons at small
  sizes, and react to appearance changes. Keep menu and status semantics
  available to keyboard and assistive technologies.
- **Recommendations:** Do not rely on color alone for focus, selection or
  status. Keep layout density and optional symbolic tray icons configurable;
  preserve meaningful tree disclosure indicators.
- **Verify:** Check keyboard focus, screen-reader names, localized labels,
  scaling and light, dark and high-contrast themes.

## Desktop actions and availability

- **What:** Provide predictable application-menu, file-association and `mailto:`
  entry points alongside ordinary in-app actions.
- **How:** Install desktop metadata for the chosen prefix and use desktop
  services for external links and file associations. Check whether optional
  services are available before offering dependent actions.
- **Recommendations:** Surface failed launches and invalid destinations; do not
  silently discard requests. Keep core mail usable when a desktop service is
  unavailable.
- **Verify:** Test menu discovery, custom-prefix discovery, `mailto:` launch and
  missing-handler errors on the supported Plasma session.
