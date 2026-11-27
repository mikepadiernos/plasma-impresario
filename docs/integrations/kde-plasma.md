# KDE Plasma integrations

**[Documentation](../README.md)** · **[Roadmap](../ROADMAP.md)** ·
**[Caveats](../CAVEATS.md)** · **[Integrations](README.md)** ·
**[Plasma Opus](plasma-opus.md)** · **[UI parity](../parity/ui.md)**

Plasma Impresario targets KDE Plasma only. This guide lists the intended desktop
integration, suggested approaches and decisions to validate; the status table
records what the client does today, as of 2026-11-27.

Legend: ![Tested][tested] · ![Needs verification][needs-verification] ·
![Planned][planned] · ![Limitation][limitation]

> [!IMPORTANT]
>
> Verify service availability, privacy and recovery in a real Plasma session. No
> integration below has been verified in one yet; those checks belong to
> [M15](../ROADMAP.md#m15-real-plasma-session-verification)
> ([#319](https://github.com/mikepadiernos/plasma-impresario/issues/319)).

## Contents

- [Status](#status)
- [Windows and session lifecycle](#windows-and-session-lifecycle)
- [Workspaces and window activation](#workspaces-and-window-activation)
- [Notifications and status](#notifications-and-status)
- [Plasma widgets](#plasma-widgets)
- [Files and attachments](#files-and-attachments)
- [Search and indexing](#search-and-indexing)
- [Settings and credentials](#settings-and-credentials)
- [Appearance and accessibility](#appearance-and-accessibility)
- [Desktop actions and availability](#desktop-actions-and-availability)
- [Service boundaries](#service-boundaries)
- [KDE services in use](#kde-services-in-use)

## Status

Roadmap work is in
[M06](../ROADMAP.md#m06-desktop-integration-and-provider-support), which is in
Development: **M06.01** KDE and Plasma services
([#42](https://github.com/mikepadiernos/plasma-impresario/issues/42)),
**M06.02** Plasma widgets and desktop feedback, and **M06.03** file manager
handoff. Baloo search is part of [M05](../ROADMAP.md#m05-search-and-indexing).
Notification controls are also part of
[M07](../ROADMAP.md#m07-output-notifications-and-rules-automation), and app
themes are part of [M09](../ROADMAP.md#m09-themes-and-branding-foundation),
whose branding work is in Development.

| Area                                                                  | Status                                    | Current behavior                                                                                                                                                                                                                                                                    | Roadmap        |
| --------------------------------------------------------------------- | ----------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------- |
| [Windows and session lifecycle](#windows-and-session-lifecycle)       | ![Needs verification][needs-verification] | Shell layout, panel and density preferences are stored with KConfig; the selected folder and message with `KSharedConfig::openStateConfig()`. Opt-in background running and start after login. Unsent messages are saved as drafts when the session ends; no Plasma session restore | M06.01         |
| [Workspaces and window activation](#workspaces-and-window-activation) | ![Needs verification][needs-verification] | Requests from launchers, other applications, Plasma Search and notifications bring the window they are about to the front with Plasma's activation token. Windows are not pinned to desktops, Activities or screens                                                                 | M06.01         |
| [Notifications and status](#notifications-and-status)                 | ![Limitation][limitation]                 | The footer shows connection status; an optional tray icon shows unread counts and network errors. KNotification is used only for background-account errors; there are no new-mail notifications                                                                                     | M06.01, M07    |
| [Plasma widgets](#plasma-widgets)                                     | ![Planned][planned]                       | None                                                                                                                                                                                                                                                                                | M06.02         |
| [Files and attachments](#files-and-attachments)                       | ![Limitation][limitation]                 | `QFileDialog` for open and save; attachments open through desktop file associations; the composer attaches dropped local files and fetches remote ones through KIO. KIO opens Plasma's file search for saved files                                                                  | M06.01, M06.03 |
| [Search and indexing](#search-and-indexing)                           | ![Limitation][limitation]                 | Search runs on the IMAP server or in an opt-in index in the app's cache; saved files are found with Baloo or a live search in the file manager. Plasma Search (KRunner) can show indexed mail by subject or sender, off by default                                                  | M05, M06.01    |
| [Settings and credentials](#settings-and-credentials)                 | ![Limitation][limitation]                 | Account settings use `QSettings`; app preferences use KConfig; passwords use the QtKeychain or cleartext plugin ([details](../CAVEATS.md#security-and-data-protection))                                                                                                             | M06.01         |
| [Appearance and accessibility](#appearance-and-accessibility)         | ![Needs verification][needs-verification] | Theme icons and a row density selector; accessibility, high-contrast and scaling checks are not recorded                                                                                                                                                                            | M06.01, M09    |
| [Desktop actions and availability](#desktop-actions-and-availability) | ![Needs verification][needs-verification] | The desktop entry registers the `mailto:` and `mid:` scheme handlers and a Composer action                                                                                                                                                                                          | M06.01         |
| [KDE services in use](#kde-services-in-use)                           | ![Needs verification][needs-verification] | Session end, notifications, file handoff, KIO, KConfig and credential plugins, as listed below; file dialogs are Qt's, shown by Plasma's platform theme                                                                                                                             | M06.01         |
| [Service boundaries](#service-boundaries)                             | ![Tested][tested]                         | The mail core links no KDE desktop service; helpers that use KConfig or Akonadi are built into `DesktopServices`, and `test_ArchitectureBoundaries` checks the boundary                                                                                                             | M06.01         |

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
- **Today:** **Run in the background** and **Start after login**, both off by
  default in **Settings › General**, keep mail checked while the window is
  closed and start the application hidden after login, through an XDG
  autostart entry or, in a sandbox, the XDG Background portal. Without them,
  closing the window quits, also while the tray icon is shown
  ([#371](https://github.com/mikepadiernos/plasma-impresario/issues/371); [caveats](../CAVEATS.md#desktop-integration)).
  When the session ends, through the X11 session manager's request to save or
  a `SIGTERM`, open composers save their unsent messages as drafts without
  asking, and the next start reopens them
  ([#100](https://github.com/mikepadiernos/plasma-impresario/issues/100);
  [caveat #468](../CAVEATS.md#saving-unsent-messages-at-session-end)). Windows
  are not restored through Plasma's session restore.
- **Verify:** Restart and restore multiple windows, unavailable accounts and
  interrupted drafts in a Plasma session; log out with an edited composer on
  Wayland and X11; log in with the autostart entry in place.

## Workspaces and window activation

Defined in **M06.01**
([#98](https://github.com/mikepadiernos/plasma-impresario/issues/98)).

- **What:** Behave predictably when the user works across virtual desktops,
  Activities and screens, and when mail is opened from somewhere other than the
  window: an application launcher, a `mailto:` or `mid:` link, a file manager,
  Plasma Search or a notification.
- **How:** Leave window placement to KWin and the user's window rules. Bring a
  window to the front only for a request the user made, with the XDG activation
  token (Wayland) or startup ID (X11) of that request, as Plasma's
  focus-stealing prevention expects.
- **Defined behavior:**
  - One instance runs per profile. A second launch hands its request, and its
    activation token, to the running instance of the same profile over D-Bus
    and exits.
  - The window a request is about comes to the front: the main window for a
    plain launch, a `mid:` link, a Plasma Search match or a notification's
    **Open Account**; the composer for a `mailto:` link, a file manager's request to send
    files, or **New Message**. Other windows stay where they are.
  - A token is spent on the first window shown for its request, and is
    dropped after 20 seconds if no window used it. Without a token, Plasma may
    mark the window as wanting attention instead of raising it.
  - The application never pins its windows to a virtual desktop, an Activity
    or a screen, and never moves them. Activating a window on another desktop
    or Activity follows Plasma's settings.
  - New composer windows open centered on the main window, offset so that they
    do not fully cover another composer.
- **Recommendations:** Do not raise windows for background events such as new
  mail or sync errors; use notifications. Keep per-Activity behavior, such as
  showing different accounts in different Activities, out of scope until users
  ask for it.
- **Today:** As defined above. The launcher's token is read before Qt clears
  it, and Plasma Search hands its token over through `SetActivationToken`
  before `Run`. The address book window does not take a token
  ([caveat #467](../CAVEATS.md#window-activation-for-requests)).
- **Verify:** On Wayland and X11, open mail from a launcher, a `mailto:` link,
  Dolphin, Plasma Search and a notification while the window is hidden,
  minimized, behind another window and on another desktop or Activity.

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
opt-in KRunner provider is part of **M06.01**
([#354](https://github.com/mikepadiernos/plasma-impresario/issues/354); see
[Plasma Opus search and indexing](plasma-opus.md#search-and-indexing)). Today, **Find Saved Files…**
opens a `baloosearch:` or `filenamesearch:` URL in the default file manager and
explains, from File Search's settings, whether it is set to index the chosen
folder or a folder inside it. With **Show indexed mail in Plasma Search** on,
the running app answers KRunner from the offline index with the subject,
sender and date of matching messages
([caveat](../CAVEATS.md#plasma-search-mail)).

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

## Service boundaries

Defined in **M06.01**
([#99](https://github.com/mikepadiernos/plasma-impresario/issues/99)).

- **What:** Keep desktop and provider integrations in their own layer, so that
  the mail core (IMAP, SMTP, the composer, encryption, OAuth and the plugin
  interfaces) works and is tested without a Plasma session, and an integration
  can change or be missing without touching mail handling.
- **How:** The build separates the layers into the libraries in the table
  below. Provider support follows the same rule: provider data and discovery
  that need only the network belong to the core, while credentials kept by a
  desktop service, such as a wallet, go through a password plugin.
- **Defined behavior:**
  - The core libraries neither include the headers nor link the libraries of
    KConfig, KIO, KNotifications, KWindowSystem, KWallet, Akonadi, KDE PIM or
    D-Bus, and do not link `DesktopServices`, `IPC` or `DesktopGui`.
  - `test_ArchitectureBoundaries` checks the includes of every core source and
    the libraries each core target links, and names each violation. Fixture
    tests confirm that it accepts the allowed libraries, exempt files and
    plugins, and rejects violations.
- **Recommendations:** Put new code that reads KConfig or talks to a desktop
  service into `DesktopServices`, `IPC` or `DesktopGui`, behind an interface
  the core can use without the service; add a plugin when the service is
  optional or has alternatives. Keep `i18n` out of the core.
- **Today:** As defined above. The `DesktopServices` helpers keep the `UiUtils`
  namespace and their files in `src/UiUtils`; the check exempts exactly those
  files.
- **Verify:** Run `ctest -R ArchitectureBoundaries` after changing a core
  library's sources or links.

| Layer            | Libraries                                                                                                                                            | May use                                                                              |
| ---------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------ |
| Mail core        | `Common`, `AppVersion`, `Imap`, `Streams`, `Composer`, `MSA`, `qwwsmtpclient`, `Cryptography`, `OAuth`, `Plugins` (interfaces), `UiUtils`            | Qt, the MIME and GnuPG libraries, KItemModels, and KFileMetaData for attachment text |
| Desktop services | `DesktopServices`: background running, saved searches, Plasma Search, file search, delete and confirmation settings, and the Akonadi calendar source | KConfig and Akonadi, in addition to the core                                         |
| Integration      | `IPC` (D-Bus, KRunner), `DesktopGui` (windows, KIO, KNotifications, KWindowSystem), the message renderer                                             | Any KDE Framework the feature needs                                                  |
| Plugins          | Address books (Akonadi, abook), passwords (QtKeychain, cleartext), spell checking (Sonnet)                                                           | The service they wrap, loaded through the plugin interfaces                          |

## KDE services in use

Recorded in **M06.01**
([#100](https://github.com/mikepadiernos/plasma-impresario/issues/100)). The
table lists the KDE and freedesktop services the application uses for files,
credentials, the session and notifications, and what it does without them.

| Need              | Service used                                                                                                               | Without it                                                                                                            |
| ----------------- | -------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------- |
| Session end       | Qt's X11 session manager request and `SIGTERM`, watched with `KSignalHandler`, save unsent messages as drafts              | Drafts saved every 30 seconds are still recovered                                                                     |
| Background start  | An XDG autostart entry, or the XDG Background portal in a sandbox                                                          | Starting after login is unavailable                                                                                   |
| Notifications     | KNotifications for account errors, with actions                                                                            | A tray message while the tray icon is shown                                                                           |
| File dialogs      | `QFileDialog`, which Plasma's platform theme shows as the KDE dialog                                                       | Qt's own dialog; the XDG portal file chooser is [#101](https://github.com/mikepadiernos/plasma-impresario/issues/101) |
| Files and links   | KIO for remote attachments and saved-file search; desktop file associations open attachments                               | Local files only                                                                                                      |
| Settings          | KConfig for app preferences and `KSharedConfig::openStateConfig()` for session state; account settings stay in `QSettings` | Not optional                                                                                                          |
| Credentials       | The QtKeychain plugin, which stores passwords in KWallet or the Secret Service                                             | The cleartext plugin, only with consent ([caveat](../CAVEATS.md#security-and-data-protection))                        |
| Window activation | KWindowSystem with the request's activation token                                                                          | The window may only be marked as wanting attention                                                                    |

[limitation]: https://img.shields.io/badge/Limitation-bc4c00
[needs-verification]: https://img.shields.io/badge/Needs_verification-0969da
[planned]: https://img.shields.io/badge/Planned-6e7781
[tested]: https://img.shields.io/badge/Tested-1a7f37
