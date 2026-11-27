# KDE Plasma integrations

**[Documentation](../README.md)** · **[Roadmap](../ROADMAP.md)** ·
**[Caveats](../CAVEATS.md)** · **[Integrations](README.md)** ·
**[Plasma Opus](plasma-opus.md)** · **[UI parity](../parity/ui.md)** ·
**[External services](external-services.md)**

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
- [Desktop feedback](#desktop-feedback)
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
| [Desktop feedback](#desktop-feedback)                                 | ![Needs verification][needs-verification] | Rules for every surface outside the window; the task manager entry shows the unread count of all inboxes, on by default                                                                                                                                                             | M06.02         |
| [Plasma widgets](#plasma-widgets)                                     | ![Planned][planned]                       | None                                                                                                                                                                                                                                                                                | M06.02         |
| [Files and attachments](#files-and-attachments)                       | ![Limitation][limitation]                 | The XDG portal file chooser for open, falling back to the KDE dialog; `QFileDialog` for save; attachments open through desktop file associations; the composer attaches dropped local files and fetches remote ones through KIO. KIO opens Plasma's file search for saved files     | M06.01, M06.03 |
| [Search and indexing](#search-and-indexing)                           | ![Limitation][limitation]                 | Search runs on the IMAP server or in an opt-in index in the app's cache; saved files are found with Baloo or a live search in the file manager. Plasma Search (KRunner) can show indexed mail by subject or sender, off by default                                                  | M05, M06.01    |
| [Settings and credentials](#settings-and-credentials)                 | ![Limitation][limitation]                 | Account settings use `QSettings`; app preferences use KConfig; passwords use the QtKeychain or cleartext plugin ([details](../CAVEATS.md#security-and-data-protection))                                                                                                             | M06.01         |
| [Appearance and accessibility](#appearance-and-accessibility)         | ![Needs verification][needs-verification] | Theme icons with symbolic variants at small sizes, color-scheme state colors, contrast checks, keyboard access and accessible names; no screen-reader, high-contrast or translation checks yet                                                                                      | M06.02, M09    |
| [Desktop actions and availability](#desktop-actions-and-availability) | ![Needs verification][needs-verification] | The desktop entry registers the `mailto:` and `mid:` scheme handlers and a Composer action                                                                                                                                                                                          | M06.01         |
| [KDE services in use](#kde-services-in-use)                           | ![Needs verification][needs-verification] | Session end, notifications, file handoff, KIO, KConfig and credential plugins, as listed below; files are opened through the XDG portal file chooser                                                                                                                                | M06.01         |
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
  implemented. The rules for every surface outside the window are in
  [Desktop feedback](#desktop-feedback).
- **Verify:** Exercise notification suppression, grouped new mail, unavailable
  notification services and retry after a sync failure.

## Desktop feedback

Defined in **M06.02**
([#102](https://github.com/mikepadiernos/plasma-impresario/issues/102)). These
rules apply to every surface outside the main window, so that the desktop
reflects mail without interrupting reading.

| Surface                | Shows                                                                                | Interrupts                                  | Status                                                                                                   |
| ---------------------- | ------------------------------------------------------------------------------------ | ------------------------------------------- | -------------------------------------------------------------------------------------------------------- |
| Task manager entry     | The unread messages in every account's INBOX; only the number                        | Never                                       | ![Tested][tested] On by default ([caveat](../CAVEATS.md#unread-count-on-the-task-manager))               |
| Tray icon              | The unread count of the current account's watched folders, and whether it is offline | Never                                       | ![Tested][tested] Optional                                                                               |
| Account notifications  | One notification for each background account failure, with **Open Account**          | A Plasma popup, which never takes the focus | ![Tested][tested] [#262](https://github.com/mikepadiernos/plasma-impresario/issues/262)                  |
| Plasma Search          | The subject, sender and date of indexed mail                                         | Only when searched                          | ![Tested][tested] Off by default ([#354](https://github.com/mikepadiernos/plasma-impresario/issues/354)) |
| New-mail notifications | Planned                                                                              | Planned to be grouped per account           | ![Planned][planned] [M07](../ROADMAP.md#m07-output-notifications-and-rules-automation)                   |
| Plasma widgets         | Planned                                                                              | Never                                       | ![Planned][planned] [#245](https://github.com/mikepadiernos/plasma-impresario/issues/245)                |

1. **Never take the focus.** Feedback does not raise, activate or focus a
   window and does not open a dialog. Only the user's action, such as a
   notification action, a click on the tray icon or the task manager entry, or
   a Plasma Search match, brings a window forward, with Plasma's activation
   token ([Workspaces and window activation](#workspaces-and-window-activation)).
2. **Show it in the application first.** Every state shown on the desktop is
   also in the window: the account selector, the sidebar header or the footer.
   A notification is never the only record of a failure.
3. **Numbers before content.** Passive surfaces show counts and states, not
   senders, subjects or message text. Subjects and senders leave the
   application only through surfaces the user turns on, such as Plasma Search
   and the planned widgets; message text never does.
4. **Report changes, once.** Counts are sent only when they change, after
   coalescing bursts, and a failure raises one notification rather than one
   per retry. New mail will update one notification per account (M07).
5. **Follow Plasma's settings.** Notification popups, Do Not Disturb and
   history are Plasma's, through KNotifications; the task manager can hide
   badges with **Show progress and status information in task buttons**.
   Each surface the application adds can be turned off in its settings.
6. **Degrade quietly.** Without a tray, a task manager or a notification
   service, nothing is shown and no error appears; the state stays in the
   window.
7. **Leave the reader alone.** Feedback about the folder being read in the
   active window stays in the window; new-mail notifications (M07) are to skip
   it.

- **Verify:** In a Plasma 6 session, check that no surface takes the focus
  while typing in the composer or reading, that the task manager count matches
  the inboxes, and that Do Not Disturb holds the account notifications
  ([M15](../ROADMAP.md#m15-real-plasma-session-verification)).

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

The portal file chooser, KDE file dialogs and KIO are part of **M06.01**
([#101](https://github.com/mikepadiernos/plasma-impresario/issues/101));
compose requests
([#263](https://github.com/mikepadiernos/plasma-impresario/issues/263)),
dropped remote files
([#265](https://github.com/mikepadiernos/plasma-impresario/issues/265)) and
showing saved attachments in the file manager
([#266](https://github.com/mikepadiernos/plasma-impresario/issues/266)) are
part of **M06.03** (see
[Plasma Opus integrations](plasma-opus.md#plasma-opus-features-to-use)). Today,
files to attach, drafts to edit and folders for saved-file search are chosen
through the XDG desktop portal's file chooser, with the app window as its
parent, so whichever program serves the portal shows its dialog. When no portal
answers, `QFileDialog` shows the KDE dialog
([caveat #470](../CAVEATS.md#portal-file-chooser)). Save dialogs use
`QFileDialog`, and files are opened with `QDesktopServices`. Saved
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
- **Status:** Done in **M06.02**
  ([#103](https://github.com/mikepadiernos/plasma-impresario/issues/103)), with
  the [caveat](../CAVEATS.md#accessibility-and-theming):

  | Area | Behavior | Status |
  | --- | --- | --- |
  | Icons | The active icon theme always wins; the few bundled icons only fill gaps. `UiUtils::SymbolicIcon::fromTheme` picks a theme's `-symbolic` variant for action icons of 24 pixels or less | ![Tested][tested] |
  | State colors | `Gui::StateColors` takes positive, neutral and negative field and text colors from `KColorScheme`, for invalid addresses, find results, sorting errors, warnings and the offline tray badge | ![Tested][tested] |
  | Contrast | `UiUtils::Color` measures contrast and keeps avatar initials, selected tag text and the protocol log at 4.5:1 or more against their background | ![Tested][tested] |
  | Keyboard and focus | The address expander is a focusable button with Return and Enter and a focus rectangle; address links take the keyboard | ![Tested][tested] |
  | Accessible names | The main window's visible focusable controls are named, including the message list, quick search, folder tree, network access and main menu; an invalid address is described | ![Tested][tested] |
  | Localization | New strings use `i18nc`, and `Messages.sh` extracts every source file; no translations exist yet | ![Limitation][limitation] |

- **Verify:** Check keyboard focus, screen-reader names, localized labels,
  scaling and light, dark and high-contrast themes in a Plasma 6 session
  ([M15](../ROADMAP.md#m15-real-plasma-session-verification)).

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
- **Outbound calls:** What each service may receive is defined in
  [External services](external-services.md); `test_ExternalExposure` keeps
  its register in step with the sources.

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

| Need              | Service used                                                                                                                    | Without it                                                                                     |
| ----------------- | ------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------- |
| Session end       | Qt's X11 session manager request and `SIGTERM`, watched with `KSignalHandler`, save unsent messages as drafts                   | Drafts saved every 30 seconds are still recovered                                              |
| Background start  | An XDG autostart entry, or the XDG Background portal in a sandbox                                                               | Starting after login is unavailable                                                            |
| Notifications     | KNotifications for account errors, with actions                                                                                 | A tray message while the tray icon is shown                                                    |
| File dialogs      | The XDG portal file chooser for opening files; `QFileDialog`, which Plasma's platform theme shows as the KDE dialog, for saving | The KDE dialog for opening files too                                                           |
| Files and links   | KIO for remote attachments and saved-file search; desktop file associations open attachments                                    | Local files only                                                                               |
| Settings          | KConfig for app preferences and `KSharedConfig::openStateConfig()` for session state; account settings stay in `QSettings`      | Not optional                                                                                   |
| Credentials       | The QtKeychain plugin, which stores passwords in KWallet or the Secret Service                                                  | The cleartext plugin, only with consent ([caveat](../CAVEATS.md#security-and-data-protection)) |
| Window activation | KWindowSystem with the request's activation token                                                                               | The window may only be marked as wanting attention                                             |
| Task manager      | The `com.canonical.Unity.LauncherEntry` signal for the unread count                                                             | No count on the entry; the window and the tray still show it                                   |

[limitation]: https://img.shields.io/badge/Limitation-bc4c00
[needs-verification]: https://img.shields.io/badge/Needs_verification-0969da
[planned]: https://img.shields.io/badge/Planned-6e7781
[tested]: https://img.shields.io/badge/Tested-1a7f37
