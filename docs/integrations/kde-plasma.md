# KDE Plasma integrations

**[Documentation](../README.md)** · **[Roadmap](../ROADMAP.md)** ·
**[Caveats](../CAVEATS.md)** · **[Integrations](README.md)** ·
**[Plasma Opus](plasma-opus.md)** · **[UI parity](../parity/ui.md)**

Plasma Impresario targets KDE Plasma only. This guide lists the intended desktop
integration, suggested approaches and decisions to validate; the status table
records what the client does today, as of 2026-11-25.

Legend: ![Needs verification][needs-verification] · ![Planned][planned] ·
![Limitation][limitation]

> [!IMPORTANT]
>
> Verify service availability, privacy and recovery in a real Plasma session. No
> integration below has been verified in one yet; those checks belong to
> [M15](../ROADMAP.md#m15-real-plasma-session-verification)
> ([#319](https://github.com/mikepadiernos/plasma-impresario/issues/319)).

## Contents

- [Status](#status)
- [Windows and session lifecycle](#windows-and-session-lifecycle)
- [Notifications and status](#notifications-and-status)
- [Plasma widgets](#plasma-widgets)
- [Files and attachments](#files-and-attachments)
- [Search and indexing](#search-and-indexing)
- [Settings and credentials](#settings-and-credentials)
- [Appearance and accessibility](#appearance-and-accessibility)
- [Desktop actions and availability](#desktop-actions-and-availability)

## Status

Roadmap work is in
[M06](../ROADMAP.md#m06-desktop-integration-and-provider-support), which is in
the Backlog: **M06.01** KDE and Plasma services
([#42](https://github.com/mikepadiernos/plasma-impresario/issues/42)),
**M06.02** Plasma widgets and desktop feedback, and **M06.03** file manager
handoff. Baloo search is part of [M05](../ROADMAP.md#m05-search-and-indexing).
Notification controls are also part of
[M07](../ROADMAP.md#m07-output-notifications-and-rules-automation), and app
themes are part of [M09](../ROADMAP.md#m09-themes-and-branding-foundation),
whose branding work is in Development.

| Area                                                                  | Status                                    | Current behavior                                                                                                                                                                                | Roadmap        |
| --------------------------------------------------------------------- | ----------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------- |
| [Windows and session lifecycle](#windows-and-session-lifecycle)       | ![Needs verification][needs-verification] | Shell layout, panel and density preferences are stored with KConfig; the selected folder and message with `KSharedConfig::openStateConfig()`. No Plasma session-restore hooks                   | M06.01         |
| [Notifications and status](#notifications-and-status)                 | ![Limitation][limitation]                 | The footer shows connection status; an optional tray icon shows unread counts and network errors. KNotification is used only for background-account errors; there are no new-mail notifications | M06.01, M07    |
| [Plasma widgets](#plasma-widgets)                                     | ![Planned][planned]                       | None                                                                                                                                                                                            | M06.02         |
| [Files and attachments](#files-and-attachments)                       | ![Limitation][limitation]                 | `QFileDialog` for open and save; attachments open through desktop file associations; the composer accepts dropped local files only. KIO opens Plasma's file search for saved files              | M06.01, M06.03 |
| [Search and indexing](#search-and-indexing)                           | ![Limitation][limitation]                 | Search runs on the IMAP server or in an opt-in index in the app's cache; saved files are found with Baloo or a live search in the file manager. KRunner is not used                             | M05, M06.01    |
| [Settings and credentials](#settings-and-credentials)                 | ![Limitation][limitation]                 | Account settings use `QSettings`; app preferences use KConfig; passwords use the QtKeychain or cleartext plugin ([details](../CAVEATS.md#security-and-data-protection))                         | M06.01         |
| [Appearance and accessibility](#appearance-and-accessibility)         | ![Needs verification][needs-verification] | Theme icons and a row density selector; accessibility, high-contrast and scaling checks are not recorded                                                                                        | M06.01, M09    |
| [Desktop actions and availability](#desktop-actions-and-availability) | ![Needs verification][needs-verification] | The desktop entry registers the `mailto:` and `mid:` scheme handlers and a Composer action                                                                                                      | M06.01         |

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
  of a failure.
- **Today:** Errors from background accounts appear in the account selector, on
  the account's header in the folder sidebar and on a footer button. Each
  failure also raises one `accountNeedsAttention` desktop notification with an
  **Open Account** action (and **Sign In** for OAuth sign-in failures) when the
  build has KNotifications; otherwise it becomes a tray message while the tray
  icon is visible. See [multiple accounts](../CAVEATS.md#multiple-accounts).
  New-mail notifications and per-account or per-folder controls are not
  implemented.
- **Verify:** Exercise notification suppression, grouped new mail, unavailable
  notification services and retry after a sync failure.

## Plasma widgets

Planned in **M06.02** (Plasma widgets and desktop feedback,
[#245](https://github.com/mikepadiernos/plasma-impresario/issues/245)).

- **What:** Offer optional Plasma widgets for the desktop or panel, such as an
  unread-count summary, account sync status, a compose shortcut and a scrolling
  unread-mail marquee. The marquee can draw inspiration from [The Bat!'s
  MailTicker](https://www.ritlabs.com/en/support/tips-and-tricks/7593/), which
  displays sender and subject information for unread messages.
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

KDE file dialogs and KIO are part of **M06.01**
([#101](https://github.com/mikepadiernos/plasma-impresario/issues/101));
compose requests
([#263](https://github.com/mikepadiernos/plasma-impresario/issues/263)),
dropped remote files
([#265](https://github.com/mikepadiernos/plasma-impresario/issues/265)) and
showing saved attachments in the file manager
([#266](https://github.com/mikepadiernos/plasma-impresario/issues/266)) are
part of **M06.03** (see
[Plasma Opus integrations](plasma-opus.md#plasma-opus-features-to-use)). Today,
files are chosen with `QFileDialog` and opened with `QDesktopServices`. Saved
attachments and messages are found through KIO, and the save dialogs remember
the last folder
([#350](https://github.com/mikepadiernos/plasma-impresario/issues/350)).

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

Mail search and the optional local index are part of
[M05](../ROADMAP.md#m05-search-and-indexing), with Baloo file search in M05.06
([#350](https://github.com/mikepadiernos/plasma-impresario/issues/350)); the
KRunner provider is part of **M06.01** (see [Plasma Opus search and
indexing](plasma-opus.md#search-and-indexing)). Today, **Find Saved Files…**
opens a `baloosearch:` or `filenamesearch:` URL in the default file manager and
explains, from File Search's settings, whether it is set to index the chosen
folder or a folder inside it.

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

- **What:** Provide predictable application-menu, file-association, `mailto:`
  and `mid:` entry points alongside ordinary in-app actions. A `mid:` URL
  opens a message by Message-ID in the running window
  ([#351](https://github.com/mikepadiernos/plasma-impresario/issues/351)).
- **How:** Install desktop metadata for the chosen prefix and use desktop
  services for external links and file associations. Check whether optional
  services are available before offering dependent actions.
- **Recommendations:** Surface failed launches and invalid destinations; do not
  silently discard requests. Keep core mail usable when a desktop service is
  unavailable.
- **Verify:** Test menu discovery, custom-prefix discovery, `mailto:` and `mid:`
  launch and missing-handler errors on the supported Plasma session.

[limitation]: https://img.shields.io/badge/Limitation-bc4c00
[needs-verification]: https://img.shields.io/badge/Needs_verification-0969da
[planned]: https://img.shields.io/badge/Planned-6e7781
