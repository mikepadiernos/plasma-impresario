# Caveats and limitations

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Pending tasks](TODO.md)** · **[Bugs](BUGS.md)** ·
**[Edge cases](EDGE.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/4)**

This page is the source of truth for known limitations of implemented
features, including security and privacy behavior, as verified in the current
source; caveat statuses match the project board as of 2026-11-27. The
[backend](parity/backend.md) and [UI](parity/ui.md) parity references describe
the implemented baseline alongside recommended parity goals.

Legend: ![Limitation][limitation] current limitation ·
![Security][security] security or privacy behavior ·
![Planned][planned] planned, not implemented ·
![Resolved][resolved] resolved, kept for context

> [!NOTE]
>
> The app can be built with different optional features. Confirm the active
> branch and build configuration before applying a workaround.

## Contents

- [Current limitations](#current-limitations)
  - [Multiple accounts](#multiple-accounts)
  - [Related messages and message details panels](#related-messages-and-message-details-panels)
  - [Message security](#message-security)
  - [Reading privacy](#reading-privacy)
  - [Loading on demand](#loading-on-demand)
  - [Reader actions](#reader-actions)
  - [Conversations](#conversations)
  - [OAuth sign-in](#oauth-sign-in)
  - [Mail operations](#mail-operations)
  - [Composing and sending](#composing-and-sending)
  - [Search](#search)
  - [Desktop integration](#desktop-integration)
  - [Local data and opened attachments](#local-data-and-opened-attachments)
  - [Outgoing headers and Bcc](#outgoing-headers-and-bcc)
  - [Saved files in the file manager](#saved-files-in-the-file-manager)
  - [Files handed over by file managers](#files-handed-over-by-file-managers)
  - [Files fetched from other computers](#files-fetched-from-other-computers)
  - [Plasma Search mail](#plasma-search-mail)
  - [Contacts from the address book](#contacts-from-the-address-book)
  - [My Day calendars and follow-up mail](#my-day-calendars-and-follow-up-mail)
  - [Server settings discovery](#server-settings-discovery)
  - [Window activation for requests](#window-activation-for-requests)
  - [Saving unsent messages at session end](#saving-unsent-messages-at-session-end)
  - [Portal file chooser](#portal-file-chooser)
  - [Unread count on the task manager](#unread-count-on-the-task-manager)
  - [Read state and unread counts](#read-state-and-unread-counts)
  - [Performance and resource use](#performance-and-resource-use)
  - [Sanitizers and data races](#sanitizers-and-data-races)
  - [Accessibility and theming](#accessibility-and-theming)
  - [Provider setup and Gmail](#provider-setup-and-gmail)
  - [Junk and attachment safety](#junk-and-attachment-safety)
  - [External services and outbound calls](#external-services-and-outbound-calls)
  - [System sleep and account notifications](#system-sleep-and-account-notifications)
  - [Plasma widgets](#plasma-widgets)
  - [POP3 accounts](#pop3-accounts)
  - [JMAP accounts](#jmap-accounts)
  - [Mail rules](#mail-rules)
  - [New-mail notifications and activity](#new-mail-notifications-and-activity)
  - [Settings and workspaces](#settings-and-workspaces)
  - [Mail tabs](#mail-tabs)
  - [Vim mode and shared shortcuts](#vim-mode-and-shared-shortcuts)
  - [Messages to calendar and notes](#messages-to-calendar-and-notes)
  - [Themes and appearance](#themes-and-appearance)
  - [Project naming and compatibility](#project-naming-and-compatibility)
- [Before reporting results](#before-reporting-results)
- [Security and data protection](#security-and-data-protection)
  - [Recommendations](#recommendations)
  - [Safe operating guidance](#safe-operating-guidance)

## Current limitations

The overview lists each area once; the sections below hold the details. Each
section starts with its `[caveat]` issue, the issue's board status and the
issue comment that holds the current recommendation.

| Kind                      | Area               | Current constraint                                                                                                                                                                                                                                                                                                                                                                                                                                    | Caveat                                                                                                                                       |
| ------------------------- | ------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| ![Resolved][resolved]     | Accounts           | One window holds several IMAP accounts, grouped by account in the folder sidebar; the limitations recorded for them are resolved ([details](#multiple-accounts))                                                                                                                                                                                                                                                                                      | [#262](https://github.com/mikepadiernos/plasma-impresario/issues/262)                                                                        |
| ![Limitation][limitation] | Embedded panels    | Related messages search only loaded messages unless the bounded server search is turned on ([details](#related-messages-and-message-details-panels)); message details do not verify inline PGP ([details](#message-security))                                                                                                                                                                                                                         | [#264](https://github.com/mikepadiernos/plasma-impresario/issues/264), [#442](https://github.com/mikepadiernos/plasma-impresario/issues/442) |
| ![Limitation][limitation] | Reading privacy    | Link warnings are heuristic, a link reached with the keyboard shows no tooltip, and remote content consent applies to the open message only ([details](#reading-privacy))                                                                                                                                                                                                                                                                             | [#443](https://github.com/mikepadiernos/plasma-impresario/issues/443)                                                                        |
| ![Limitation][limitation] | Loading on demand  | Parts that failed after a server error load again only on Try Again; the unavailable-message page is not verified live ([details](#loading-on-demand))                                                                                                                                                                                                                                                                                                | [#444](https://github.com/mikepadiernos/plasma-impresario/issues/444)                                                                        |
| ![Limitation][limitation] | Reader actions     | The action bar is in the main window only, zoom is not saved, and Archive and Trash from the bar are not verified live ([details](#reader-actions))                                                                                                                                                                                                                                                                                                   | [#445](https://github.com/mikepadiernos/plasma-impresario/issues/445)                                                                        |
| ![Limitation][limitation] | Conversations      | A search in a threaded list splits threads, conversations cover the loaded messages of the current folder only, and the Unified Inbox has no conversation steps ([details](#conversations))                                                                                                                                                                                                                                                           | [#449](https://github.com/mikepadiernos/plasma-impresario/issues/449)                                                                        |
| ![Limitation][limitation] | OAuth sign-in      | Microsoft uses the `common` tenant only and cannot revoke grants from the app, IMAP and SMTP refresh tokens independently, and switching provider replaces a grant without revoking it ([details](#oauth-sign-in))                                                                                                                                                                                                                                    | [#362](https://github.com/mikepadiernos/plasma-impresario/issues/362)                                                                        |
| ![Limitation][limitation] | Mail operations    | Special folders need server attributes or English names, Go to Folder and related searches use loaded data only, and Undo after a move needs server COPYUID support ([details](#mail-operations))                                                                                                                                                                                                                                                     | [#312](https://github.com/mikepadiernos/plasma-impresario/issues/312)                                                                        |
| ![Limitation][limitation] | Composing          | Each refused send files another copy in Sent, a forward sent before its message downloads is refused, forwards are attachment-only and drafts are local without attachments ([details](#composing-and-sending))                                                                                                                                                                                                                                       | [#450](https://github.com/mikepadiernos/plasma-impresario/issues/450)                                                                        |
| ![Limitation][limitation] | Search             | Date ranges follow the day in each message's Date header, in the sender's time zone, and a search of several folders selects each folder of the account in turn and loads every result's headers; `has:attachment` approximates by message type, folder suggestions cover only the shown account's listed folders, virtual folders count only while shown, and offline search covers only mail kept and indexed on this computer ([details](#search)) | [#454](https://github.com/mikepadiernos/plasma-impresario/issues/454)                                                                        |
| ![Limitation][limitation] | Desktop            | Closing the window quits unless running in the background is on, even with the tray icon shown; both options apply to every profile, and starting after login is not verified in a Plasma session or through the sandbox portal ([details](#desktop-integration))                                                                                                                                                                                     | [#457](https://github.com/mikepadiernos/plasma-impresario/issues/457)                                                                        |
| ![Limitation][limitation] | Saved files        | **Show in Folder** is offered in the main window only, and is not checked with Dolphin or Plasma Opus in a real Plasma session ([details](#saved-files-in-the-file-manager))                                                                                                                                                                                                                                                                          | [#460](https://github.com/mikepadiernos/plasma-impresario/issues/460)                                                                        |
| ![Limitation][limitation] | Handed-over files  | A `mailto:` URL may not attach hidden or system files or name this computer through the network, drafts drop the files, and nothing is checked with the portal, Dolphin or Plasma Opus in a real Plasma session ([details](#files-handed-over-by-file-managers))                                                                                                                                                                                      | [#461](https://github.com/mikepadiernos/plasma-impresario/issues/461)                                                                        |
| ![Limitation][limitation] | Fetched files      | Only HTTP is tested, SFTP, SMB and WebDAV are not, drafts drop fetched files, and nothing is checked in a real Plasma session ([details](#files-fetched-from-other-computers))                                                                                                                                                                                                                                                                        | [#462](https://github.com/mikepadiernos/plasma-impresario/issues/462)                                                                        |
| ![Limitation][limitation] | Plasma Search      | Mail appears only while the app runs, for the default profile, by subject or sender; nothing is checked with KRunner or in a Plasma session ([details](#plasma-search-mail))                                                                                                                                                                                                                                                                          | [#463](https://github.com/mikepadiernos/plasma-impresario/issues/463)                                                                        |
| ![Limitation][limitation] | Contacts panel     | abook has no organizations and Akonadi is not run against a server; photos come only from the address book ([details](#contacts-from-the-address-book))                                                                                                                                                                                                                                                                                               | [#464](https://github.com/mikepadiernos/plasma-impresario/issues/464)                                                                        |
| ![Limitation][limitation] | My Day             | Every Akonadi calendar is shown and the Akonadi path is not run against a server; follow-up mail comes from search indexes and the shown folder, up to 50 messages ([details](#my-day-calendars-and-follow-up-mail))                                                                                                                                                                                                                                  | [#465](https://github.com/mikepadiernos/plasma-impresario/issues/465)                                                                        |
| ![Limitation][limitation] | Add Account        | Found servers are not tried against real providers, guessed servers are only probed for a connection, and most resolvers do not validate DNSSEC, so servers outside the mail domain usually need confirmation ([details](#server-settings-discovery))                                                                                                                                                                                                 | [#466](https://github.com/mikepadiernos/plasma-impresario/issues/466)                                                                        |
| ![Limitation][limitation] | Window activation  | Requests pass Plasma's activation token to the window they open, but nothing is checked in a Plasma session; a composer opened by a first launch and the address book window may only be marked as wanting attention ([details](#window-activation-for-requests))                                                                                                                                                                                     | [#467](https://github.com/mikepadiernos/plasma-impresario/issues/467)                                                                        |
| ![Limitation][limitation] | Session end        | Open composers save unsent messages as drafts when the session ends, but nothing is checked in a Plasma session; attachments are not saved, and on Wayland **Save Draft?** may hold up logging out ([details](#saving-unsent-messages-at-session-end))                                                                                                                                                                                                | [#468](https://github.com/mikepadiernos/plasma-impresario/issues/468)                                                                        |
| ![Limitation][limitation] | Read state         | Read and flag changes, and the unread counts, change only once the server confirms them; a change cut off by a lost connection is not replayed ([details](#read-state-and-unread-counts))                                                                                                                                                                                                                                                             | [#505](https://github.com/mikepadiernos/plasma-impresario/issues/505)                                                                        |
| ![Limitation][limitation] | Performance        | Removing many messages at once slows down with folder size, and a single `VANISHED` report blocks the window while it is applied; quick filters on very large folders and open large attachments are costly ([details](#performance-and-resource-use))                                                                                                                                                                                                | [#506](https://github.com/mikepadiernos/plasma-impresario/issues/506)                                                                        |
| ![Limitation][limitation] | Sanitizers         | The suite and live tests run clean under ASan, LSan and UBSan, but ThreadSanitizer has not been run, so data races in work on other threads are not checked ([details](#sanitizers-and-data-races))                                                                                                                                                                                                                                                   | [#507](https://github.com/mikepadiernos/plasma-impresario/issues/507)                                                                        |
| ![Limitation][limitation] | File chooser       | Files are chosen through the XDG portal file chooser, falling back to the KDE dialog, but nothing is checked in a Plasma session or with Plasma Opus; save dialogs still use the KDE dialog, and remote locations in the portal depend on kio-fuse ([details](#portal-file-chooser))                                                                                                                                                                  | [#470](https://github.com/mikepadiernos/plasma-impresario/issues/470)                                                                        |
| ![Limitation][limitation] | Task manager       | The task manager entry shows the unread messages in every account's INBOX, but nothing is checked in a Plasma session; the tray icon counts other folders, and accounts not yet connected add nothing ([details](#unread-count-on-the-task-manager))                                                                                                                                                                                                  | [#471](https://github.com/mikepadiernos/plasma-impresario/issues/471)                                                                        |
| ![Limitation][limitation] | Accessibility      | Icons, state colors and contrast follow Plasma's theme and the main controls have accessible names, but nothing is checked with a screen reader, a high-contrast scheme or in a Plasma session, and there are no translations yet ([details](#accessibility-and-theming))                                                                                                                                                                             | [#472](https://github.com/mikepadiernos/plasma-impresario/issues/472)                                                                        |
| ![Limitation][limitation] | Providers          | The provider guide is untried with real accounts; Gmail labels are read-only chips and still appear as folders ([details](#provider-setup-and-gmail))                                                                                                                                                                                                                                                                                                 | [#473](https://github.com/mikepadiernos/plasma-impresario/issues/473)                                                                        |
| ![Limitation][limitation] | Security           | Junk comes only from IMAP keywords, the optional ClamAV scan needs a local `clamd` socket and runs only on **Open**, and the reader banner's **Not Junk** does not move mail ([details](#junk-and-attachment-safety))                                                                                                                                                                                                                                 | [#474](https://github.com/mikepadiernos/plasma-impresario/issues/474)                                                                        |
| ![Limitation][limitation] | Privacy            | The outbound-call check works per file, GnuPG may fetch keys by its own settings, and version details are sent by default ([details](#external-services-and-outbound-calls))                                                                                                                                                                                                                                                                          | [#475](https://github.com/mikepadiernos/plasma-impresario/issues/475)                                                                        |
| ![Limitation][limitation] | Sleep              | Connections close before sleep and reopen on wake only through logind, without delaying sleep; account notifications close when the app quits, and KDE Online Accounts is not used ([details](#system-sleep-and-account-notifications))                                                                                                                                                                                                               | [#476](https://github.com/mikepadiernos/plasma-impresario/issues/476)                                                                        |
| ![Limitation][limitation] | Plasma widgets     | The Mail Status and Mail Ticker widgets are untried in a Plasma session; senders and subjects come only from indexed accounts, counts cover INBOX only, and there are no data while the app is not running ([details](#plasma-widgets))                                                                                                                                                                                                               | [#477](https://github.com/mikepadiernos/plasma-impresario/issues/477)                                                                        |
| ![Limitation][limitation] | POP3               | Mail removed from the server exists only on this computer and goes with the account; one inbox, no offline search index and no sent copies ([details](#pop3-accounts))                                                                                                                                                                                                                                                                                | [#478](https://github.com/mikepadiernos/plasma-impresario/issues/478)                                                                        |
| ![Limitation][limitation] | JMAP               | No push, so new mail waits for the next synchronization; messages download one at a time; folders made before the first synchronization stay local; no offline search index ([details](#jmap-accounts))                                                                                                                                                                                                                                               | [#488](https://github.com/mikepadiernos/plasma-impresario/issues/488)                                                                        |
| ![Limitation][limitation] | Mail rules         | Rules see new mail only in folders the application has open, cover IMAP accounts only, cannot check message text, dates or sizes, and preview only the loaded rows ([details](#mail-rules))                                                                                                                                                                                                                                                           | [#491](https://github.com/mikepadiernos/plasma-impresario/issues/491)                                                                        |
| ![Limitation][limitation] | New mail           | Folders other than a background account's INBOX are announced by count, mail moved by a rule is counted, and POP3 and JMAP mail is not announced ([details](#new-mail-notifications-and-activity))                                                                                                                                                                                                                                                    | [#492](https://github.com/mikepadiernos/plasma-impresario/issues/492)                                                                        |
| ![Limitation][limitation] | Settings           | Activities are read over D-Bus only, workspaces do not scope contacts, index changes wait for a reconnect and start folders apply on account switches ([details](#settings-and-workspaces))                                                                                                                                                                                                                                                           | [#496](https://github.com/mikepadiernos/plasma-impresario/issues/496)                                                                        |
| ![Limitation][limitation] | Mail tabs          | Restored message tabs show their folder until opened, a vertical tab bar in the header or footer makes the strip tall, and reconfiguring Next Tab drops Ctrl+Tab ([details](#mail-tabs))                                                                                                                                                                                                                                                              | [#497](https://github.com/mikepadiernos/plasma-impresario/issues/497)                                                                        |
| ![Limitation][limitation] | Vim mode           | Vim keys cannot tell editable content in the HTML reader, Vim bindings are not KActions and the pending keys show in the status bar ([details](#vim-mode-and-shared-shortcuts))                                                                                                                                                                                                                                                                       | [#498](https://github.com/mikepadiernos/plasma-impresario/issues/498)                                                                        |
| ![Limitation][limitation] | Calendar and notes | The notes folder is chosen by hand, encrypted messages are saved without text, events stay in the cache for a day and only the message in the reader is used ([details](#messages-to-calendar-and-notes))                                                                                                                                                                                                                                             | [#499](https://github.com/mikepadiernos/plasma-impresario/issues/499)                                                                        |
| ![Limitation][limitation] | Themes             | Turning translucency on or off applies when the window next opens, a widget style change drops the appearance proxy until restart, cards need the two-line layout and the symbolic tray icon has no count ([details](#themes-and-appearance))                                                                                                                                                                                                         | [#502](https://github.com/mikepadiernos/plasma-impresario/issues/502)                                                                        |
| ![Limitation][limitation] | Naming             | Settings location, launcher, plugin IDs, MIME types, cache keys, translations and `TROJITA_PROFILE` keep their Trojitá names for compatibility ([details](#project-naming-and-compatibility))                                                                                                                                                                                                                                                         | [#503](https://github.com/mikepadiernos/plasma-impresario/issues/503)                                                                        |
| ![Limitation][limitation] | Product identity   | The product is renamed to Plasma Impresario ([#221](https://github.com/mikepadiernos/plasma-impresario/issues/221)), but the application remains based on Trojitá and the product redesign is not complete                                                                                                                                                                                                                                            | —                                                                                                                                            |
| ![Limitation][limitation] | Message rendering  | Qt WebEngine is required for the default reader; a renderer-free build cannot display message contents                                                                                                                                                                                                                                                                                                                                                | —                                                                                                                                            |
| ![Security][security]     | Credentials        | QtKeychain is optional; new profiles default to it, but profiles with saved cleartext passwords keep the cleartext plugin, which stores secrets in `QSettings` ([details](#security-and-data-protection))                                                                                                                                                                                                                                             | —                                                                                                                                            |
| ![Security][security]     | Transport          | Cleartext IMAP remains selectable; account configuration determines whether TLS is used ([details](#security-and-data-protection))                                                                                                                                                                                                                                                                                                                    | —                                                                                                                                            |
| ![Security][security]     | Local data         | The cache and opened attachments are kept user-only, and opened copies are removed after a day, but the application does not encrypt the cache, and files created between starts are tightened at the next start ([details](#local-data-and-opened-attachments))                                                                                                                                                                                      | [#458](https://github.com/mikepadiernos/plasma-impresario/issues/458)                                                                        |
| ![Security][security]     | Outgoing headers   | Header input is cleaned and Bcc is left out of delivered mail, but Sent copies delivered through BURL or the IMAP SENDMAIL Extension also omit Bcc, and cleaned input is not pointed out ([details](#outgoing-headers-and-bcc))                                                                                                                                                                                                                       | [#459](https://github.com/mikepadiernos/plasma-impresario/issues/459)                                                                        |
| ![Planned][planned]       | Integrations       | Plasma new-mail notifications, session integration, Gmail/Outlook APIs, Nextcloud, rules and cross-app workflows remain planned                                                                                                                                                                                                                                                                                                                       | —                                                                                                                                            |
| ![Limitation][limitation] | Compatibility      | The [baseline](philosophy/compatibility.md) records an initial CachyOS target and partial build evidence; no full reader or desktop-session support matrix has been verified                                                                                                                                                                                                                                                                          | —                                                                                                                                            |
| ![Limitation][limitation] | Quality evidence   | Workloads are measured only against fake servers in a Debug build ([performance](performance.md)); broad accessibility, screen-reader, high-DPI and RTL verification are not recorded; nothing has been checked in a real Plasma session yet                                                                                                                                                                                                          | —                                                                                                                                            |

### Multiple accounts

| Caveat issue                                                          | Board             | Recommendation                                                                                         | Verified with                                                              |
| --------------------------------------------------------------------- | ----------------- | ------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------- |
| [#262](https://github.com/mikepadiernos/plasma-impresario/issues/262) | ![Merged][merged] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/262#issuecomment-7142887185) | Disposable Dovecot, the opt-in `test_LiveImapAccounts` and offscreen tests |

All five limitations recorded in #262 are resolved, and the issue is closed.

**Current behavior:**

- ![Resolved][resolved] Errors from background accounts appear in the account
  selector, on the account's header in the folder sidebar, and on a footer
  button that switches to the first flagged account. Each failure also raises
  one desktop notification, with an **Open Account** action, until the account
  connects again. Builds without KNotifications show it as a tray message
  instead, but only while the tray icon is visible.
- ![Resolved][resolved] Removing an account deletes its settings, stored
  passwords and offline cache. For the account migrated from a single-account
  profile, whose cache shares the cache root, only its database and
  message-part directories are deleted; drafts and other accounts' caches are
  kept.
- ![Resolved][resolved] Accepting an untrusted TLS certificate records the host
  name and the SHA-256 fingerprint of the certificate's public key for the
  whole profile. Other accounts on that host, and the host's other service
  (IMAP or SMTP), accept the same key without asking. A different key, or the
  same key for another host name, still prompts.
- ![Resolved][resolved] A composer sends and files the message with the
  settings of the account it was opened from, even after switching accounts.
  If that account is removed, sending is refused with an error and the composer
  stays open.
- ![Resolved][resolved] Each account's header in the folder sidebar has a menu
  with **Account Settings…**, **Rename…** and **Remove…**. **Account
  Settings…** switches to that account first, so the dialog and the window show
  the same account; renaming and removing don't switch.

The notification popup and its action have not yet been checked in a real
Plasma session; that check is
[#315](https://github.com/mikepadiernos/plasma-impresario/issues/315) in
[M15](ROADMAP.md#m15-real-plasma-session-verification).

### Related messages and message details panels

| Caveat issue                                                          | Board             | Recommendation                                                                                         | Applies to                                                         |
| --------------------------------------------------------------------- | ----------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------ |
| [#264](https://github.com/mikepadiernos/plasma-impresario/issues/264) | ![Review][review] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/264#issuecomment-7143646817) | Panels off by default; enable them from **View › Embedded Panels** |

**Current behavior:**

- ![Limitation][limitation] By default the related messages panel only
  searches messages already loaded in the current folder's message list. It
  lists at most 25 messages per group (conversation and same sender) and scans
  at most the 20,000 most recent rows.
- ![Resolved][resolved] **Search the Server**, in the panel's search menu, also
  asks the server for the conversation (`Message-ID`, `In-Reply-To` and
  `References`) and the sender's other mail, in the message's folder, every
  folder of the account or chosen folders. Results merge with the loaded ones,
  rows from other folders start with the folder name, and opening one shows it
  in the reader without changing the message list. The choice and the chosen
  folders are remembered
  ([#302](https://github.com/mikepadiernos/plasma-impresario/issues/302)).
- ![Limitation][limitation] The server search is bounded: **All Folders**
  searches at most 20 folders, and only the newest 50 matches in each folder
  are loaded. The status line says when the limits left something out.
- ![Limitation][limitation] The server matches header keys as substrings, so
  it may return mail from a similar address or naming a similar Message-ID. The
  panel drops these, but their headers are still downloaded.
- ![Limitation][limitation] The server search uses the account's own
  connection and selects each folder in turn; the shown folder is selected
  again when it ends. It waits while a **Search In** search of the same account
  is running and starts once that search finishes or stops. Changing the
  message cancels it.
- ![Resolved][resolved] The message details panel's Security field shows the
  reader's verification and decryption results
  ([#303](https://github.com/mikepadiernos/plasma-impresario/issues/303)).
  Inline PGP and nested signatures remain limited; see
  [Message security](#message-security).
- ![Resolved][resolved] Pane sizes saved by builds before the shell carry over
  the first time each layout opens: the folder tree's width, on the side it was
  saved on, and the split between the message list and the reader. Sizes the
  shell has saved since then take precedence.

**Workarounds:**

- Turn on **Search the Server** with **Chosen Folders** to reach folders that
  **All Folders** leaves out, or use the search bar's **Search In** menu for an
  unbounded search.

**Proposed improvements:**

- Search the other accounts too, for mail that was forwarded between them.
- Use the server's `THREAD` answer, where offered, to find replies that only
  share a subject.

### Message security

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                      |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------- |
| [#442](https://github.com/mikepadiernos/plasma-impresario/issues/442) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/442#issuecomment-7174234978) | Messages with inline PGP or several protected parts, with Message Details shown |

**Current behavior:**

- ![Resolved][resolved] The reader verifies and decrypts PGP/MIME and S/MIME
  through GpgME, and the message details panel shows its result: in progress,
  valid and trusted, valid but untrusted, unknown key, invalid, unverifiable,
  decrypted or failed. The panel never starts verification or decryption
  itself.
- ![Limitation][limitation] The reader does not process inline PGP
  (`-----BEGIN PGP SIGNED MESSAGE-----` or `-----BEGIN PGP MESSAGE-----` in the
  body) and shows the armor as text. The details panel recognizes the armor at
  the start of a line in loaded, non-attachment `text/plain` parts and labels
  it _not verified_ or _not decrypted_. It never downloads a part to look for
  armor.
- ![Limitation][limitation] The panel reports the outermost signature and
  encryption only. Signatures inside an attached message or in a later signed
  part appear in the reader only.

**Workarounds:**

- Save an inline PGP message's text and verify or decrypt it with `gpg` or
  Kleopatra.
- Open an attached message to see its own signature in the reader.

**Proposed improvements:**

- Verify and decrypt inline PGP in the reader through GpgME, so that the
  details panel can report the result. Not yet scheduled.
- List each protected part in the Security tooltip when a message has more
  than one.

### Reading privacy

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                           |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ---------------------------------------------------- |
| [#443](https://github.com/mikepadiernos/plasma-impresario/issues/443) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/443#issuecomment-7175141140) | Messages with remote content or links, in the reader |

**Current behavior:**

- ![Resolved][resolved] Remote content stays blocked until **Load Remote
  Content**; the bar names the servers it would contact. Links that look
  deceptive ask before opening
  ([#97](https://github.com/mikepadiernos/plasma-impresario/issues/97)).
- ![Resolved][resolved] Hovering a link shows its full target, fully encoded
  with the host in its punycode form, and the link's context menu starts with
  the same description and an **Open Link** entry. User names or passwords
  before the host, internationalized host names with both of their forms,
  numeric addresses, link text that shows another site and mailto: links whose
  text is another address are pointed out there
  ([#374](https://github.com/mikepadiernos/plasma-impresario/issues/374)).
- ![Limitation][limitation] Link warnings are heuristics. Sites are compared by
  name, without the public suffix list: a subdomain counts as its parent's
  site.
- ![Limitation][limitation] Link text which is not an address, such as "Sign
  in", is never compared with the target.
- ![Limitation][limitation] A link reached with the keyboard shows no tooltip;
  its context menu shows the target. A target longer than 1000 characters is
  shortened from the middle; **Copy Link Address** copies all of it.
- ![Limitation][limitation] A mailto: link whose text is another address is
  pointed out but opens the composer without asking.
- ![Limitation][limitation] Newsletters that route links through a tracking
  server while showing the destination as the link text warn on every link.
- ![Limitation][limitation] Consent is all or nothing for the open message and
  is not remembered; there is no per-sender allow list.
- ![Limitation][limitation] The bar lists the servers the page has tried so
  far; content requested later adds its server then.

**Workarounds:**

- Hover over a link, or open its context menu, to see its whole target before
  clicking.
- For a warning caused by a tracking server, check that the named site is the
  sender's mail service before choosing **Open Link**.
- Load remote content only for senders you trust.

**Proposed improvements:**

- Compare sites with the public suffix list.
- Remember remote-content consent per sender, with a way to review and revoke
  it (M14.07).
- Show the target of the focused link in the status bar, so that keyboard
  users see it without opening a menu.
- Ask before writing to a mailto: address that differs from the link text.

### Loading on demand

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                      |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------- |
| [#444](https://github.com/mikepadiernos/plasma-impresario/issues/444) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/444#issuecomment-7175640115) | Messages and parts the reader cannot load ([#94](https://github.com/mikepadiernos/plasma-impresario/issues/94)) |

**Current behavior:**

- ![Resolved][resolved] Parts that cannot be loaded are reported in an error
  bar above the message instead of a blank or a Chromium error page. Offline,
  they load on their own when the network returns (#94).
- ![Limitation][limitation] After a server error, a part loads again only on
  **Try Again** or when the message is reopened.
- ![Limitation][limitation] The page for a message whose structure cannot be
  fetched is covered by code review only; the message list fetches the
  structure first, so no live scenario reaches it yet.
- ![Limitation][limitation] Saving an attachment that cannot be downloaded, for
  example offline, still reports the failure in a dialog.

**Workarounds:**

- Offline, go online to load the missing parts.
- After a server error, use **Try Again** in the bar, or reopen the message.
- To read a folder offline, enable offline caching for the account and open its
  messages while online.

**Proposed improvements:**

- Retry a failed part once on its own after a short delay before asking.
- Report failed attachment downloads in the attachments panel.
- Cover the unavailable page and online refusals with a synthetic server that
  refuses FETCH.

### Reader actions

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                  |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------- |
| [#445](https://github.com/mikepadiernos/plasma-impresario/issues/445) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/445#issuecomment-7176256153) | The reader action bar ([#39](https://github.com/mikepadiernos/plasma-impresario/issues/39)) |

**Current behavior:**

- ![Resolved][resolved] The bar above the reader acts on the open message, not
  the list selection, and Undo returns a moved message to its own folder
  (#39).
- ![Limitation][limitation] Messages opened in their own window have no action
  bar.
- ![Limitation][limitation] Zoom applies to the open message and resets when
  another message opens.
- ![Limitation][limitation] **Prefer Plain Text** is the global reader setting,
  not a per-message choice.
- ![Limitation][limitation] Offline, Archive, Trash, Read and Flag are
  disabled.
- ![Limitation][limitation] Archive and Move to Trash from the bar are covered
  by offscreen tests only; the live test covers the flag, read, attachment,
  zoom and plain-text buttons.

**Workarounds:**

- In a standalone message window, use the main window's message actions.
- After opening another message, zoom again with the bar or the Zoom In and
  Zoom Out shortcuts.

**Proposed improvements:**

- Add the bar to standalone message windows.
- Remember the zoom level.
- Cover Archive and Trash from the bar, with Undo for a message opened from
  Related Messages, in the live test.

### Conversations

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                                                                                                                                                      |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [#449](https://github.com/mikepadiernos/plasma-impresario/issues/449) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/449#issuecomment-7178393442) | Message list filters, quick search and the conversation steps ([#40](https://github.com/mikepadiernos/plasma-impresario/issues/40), [#96](https://github.com/mikepadiernos/plasma-impresario/issues/96), [#369](https://github.com/mikepadiernos/plasma-impresario/issues/369)) |

**Current behavior:**

- ![Resolved][resolved] In a threaded list, the Unread, Flagged and Attachments
  filters keep a matching message's whole thread. The open message stays in
  the list when a filter or the grouping changes (#96).
- ![Resolved][resolved] **Next in Conversation** and **Previous in
  Conversation** follow the thread in a threaded list. In a flat list they step
  through the conversation by date and show messages hidden by a filter (#40,
  #96).
- ![Limitation][limitation] A quick search, or **Show Related**, in a threaded
  list threads only the matching messages, because the server is asked to
  thread the search results. Replies that don't match are not shown with their
  thread.
- ![Resolved][resolved] Replies without In-Reply-To or References join their
  conversation by subject, ignoring localized prefixes and list tags, within 30
  days. Two messages without reply prefixes never join by subject, and a
  message that references two conversations merges them, in threads as in flat
  lists (#369).
- ![Limitation][limitation] A conversation is built from the messages loaded in
  the current folder. Your replies in Sent and messages in other folders are
  not included.
- ![Limitation][limitation] A threaded list merges conversations only among
  messages whose envelopes are loaded, so a merge may appear only after
  scrolling. It does not merge again during a search or after the server
  reports threads incrementally, until the folder is threaded again.
- ![Limitation][limitation] A reply that has a reply prefix but no links can
  still join an unrelated message of the same subject sent within 30 days.
- ![Limitation][limitation] The conversation steps do nothing in the Unified
  Inbox.

**Workarounds:**

- To see a whole thread that a search split, clear the search, open the
  message, and use the conversation steps.
- Open the Sent folder to read your own replies, or turn on **Search the
  Server** in the related messages panel to list them beside the message
  ([#302](https://github.com/mikepadiernos/plasma-impresario/issues/302)).
- To see a merge that has not appeared yet, scroll through the folder, or
  switch Group By to None and back to Conversation.

**Proposed improvements:**

- Thread all messages on the server and filter by the search's results on the
  client, so that threads stay whole during a search.
- Step through conversations in the Unified Inbox within each account.

### OAuth sign-in

| Caveat issue                                                          | Board             | Recommendation                                                                                         | Applies to                                                                                                                   |
| --------------------------------------------------------------------- | ----------------- | ------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------- |
| [#362](https://github.com/mikepadiernos/plasma-impresario/issues/362) | ![Review][review] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/362#issuecomment-7165448590) | Accounts set to OAuth in **Account Settings › IMAP** ([#218](https://github.com/mikepadiernos/plasma-impresario/issues/218)) |

**Current behavior:**

- ![Resolved][resolved] When an OAuth account needs to sign in, the status bar
  shows a **Sign In…** button, the account's sidebar menu has **Sign In…**,
  and the error notification offers **Sign In**. Each opens the browser for
  that account and reconnects it after a successful sign-in, without opening
  Account Settings.
- ![Resolved][resolved] The optional client secret is stored with the grant in
  the password plugin, not in the settings file. A secret saved by an earlier
  build moves there the next time the account connects. The field in Account
  Settings starts empty, and saving is blocked while a typed secret has not
  been used to sign in.
- ![Resolved][resolved] Signing out and removing an account revoke the grant at
  Google before deleting it locally. Removal keeps the grant when another
  account uses the same provider, client and user name.
- ![Resolved][resolved] A grant made in Account Settings is revoked when the
  dialog is cancelled, or saved with a different provider or client.
- ![Security][security] OAuth needs a secure credential store and is refused
  with the cleartext password backend.
- ![Security][security] No client ID is built in unless one is set at build
  time with `IMPRESARIO_OAUTH_GOOGLE_CLIENT_ID` or
  `IMPRESARIO_OAUTH_MICROSOFT_CLIENT_ID`.
- ![Limitation][limitation] Microsoft has no revocation endpoint for these
  clients. Signing out and removing the account delete the local grant and
  point to Microsoft's access page.
- ![Limitation][limitation] Signing out in Account Settings revokes the grant
  at Google even when another account shares the same user and client, which
  then needs to sign in again.
- ![Limitation][limitation] Each account stores one grant. Signing in with
  another provider or client replaces the previous grant without revoking it,
  and cancelling the dialog afterwards leaves the account without a grant
  until it signs in again.
- ![Limitation][limitation] Microsoft accounts use the `common` tenant only.
- ![Limitation][limitation] SMTP requests a new access token for every
  message, and IMAP and SMTP refresh independently. A provider that rotates
  refresh tokens can make one of them fail until the next sign-in.
- ![Limitation][limitation] The provider fields are on the IMAP page only;
  SMTP reuses them.
- ![Limitation][limitation] A server that advertises `LOGINDISABLED` still
  needs STARTTLS before OAuth, as with passwords.
- ![Limitation][limitation] Revoking a grant from a cancelled dialog is not
  covered by automated tests, because a sign-in needs the real provider
  endpoints.

**Workarounds:**

- Use QtKeychain, the default for new profiles, before choosing OAuth.
- After signing out of or removing a Microsoft account, remove the app's access
  at [account.live.com/consent/Manage](https://account.live.com/consent/Manage).
- Sign out before switching an account to another provider or client, so the
  old grant is revoked.
- For Microsoft accounts, register your own app in Microsoft Entra ID as
  described under
  [App registration](integrations/microsoft.md#app-registration) and enter its
  client ID. For single-tenant accounts, let the registration accept the
  `common` endpoint, or keep password sign-in where the server allows it.
- Prefer a public client without a secret.

**Proposed improvements:**

- Build a Plasma Impresario Microsoft client ID into release packages (M13.01,
  [#396](https://github.com/mikepadiernos/plasma-impresario/issues/396)).
- Add a tenant choice for Microsoft accounts (M13.01,
  [#397](https://github.com/mikepadiernos/plasma-impresario/issues/397)).
- Share one token session between IMAP and SMTP per account, and reuse the
  access token until it expires (M13.01,
  [#398](https://github.com/mikepadiernos/plasma-impresario/issues/398), for
  Microsoft accounts).
- Keep the previous grant until a dialog that signed in with another provider
  or client is saved, then revoke it (M14.03,
  [#428](https://github.com/mikepadiernos/plasma-impresario/issues/428)).
- Skip revocation when signing out in Account Settings if another account
  shares the grant (M14.03,
  [#429](https://github.com/mikepadiernos/plasma-impresario/issues/429)).
- Verify sign-in and revocation with a real provider account (M15,
  [#317](https://github.com/mikepadiernos/plasma-impresario/issues/317)).

### Mail operations

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                      |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------- |
| [#312](https://github.com/mikepadiernos/plasma-impresario/issues/312) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/312#issuecomment-7148901982) | Folder actions, message list filters and sorting, Unified Inbox |

**Current behavior:**

- ![Limitation][limitation] Special folders (Sent, Drafts, Trash, Junk,
  Archive) are recognized from server SPECIAL-USE attributes or English folder
  names, not localized names. Only the first name match of each kind is used.
  The configured archive folder wins over the server's `\Archive`, and a
  changed name applies after reconnecting.
- ![Limitation][limitation] Go to Folder lists only folders already loaded in
  the folder tree.
- ![Limitation][limitation] Undo after moving to Trash or Archive appears only
  when the server returns `COPYUID` (UIDPLUS); otherwise the notification
  offers to open the folder. Copies cannot be undone. Undo and Empty
  Trash/Junk may briefly select another folder.
- ![Limitation][limitation] Counts in confirmations may be incomplete before
  the folder is synchronized; the Expunge count covers loaded messages only.
- ![Limitation][limitation] Renaming re-lists only the renamed folder and does
  not update the archive or Sent folder settings.
- ![Limitation][limitation] Move To and Copy To share one list of recent
  folders. A folder is added when the operation starts, so a failed operation
  still adds it, and deleted folders stay in the list. The menus show full
  IMAP names, such as `INBOX.Lists.KDE`, rather than a path.
- ![Limitation][limitation] Message list filters are kept per account, not per
  folder; the old global hide-read setting only seeds them. A thread is shown
  whole when any of its messages matches. When messages stop matching, only
  the current message stays visible, not the rest of a multiple selection.
- ![Limitation][limitation] The Flagged and Unread filters hide messages until
  their flags load, so large folders fill in gradually. In "Showing X of Y", X
  also reflects the quick search, and Y is the server's total.
- ![Limitation][limitation] Bulk actions report their result when the command
  is sent, not when the server confirms it.
- ![Limitation][limitation] After messages are removed, the next selection is
  chosen heuristically when the server expunges only some of them, and rows
  hidden by filters or collapsed threads are not considered. The reader keeps
  the old message when the list becomes empty. Failures to mark a message as
  read from the reader are not reported.
- ![Limitation][limitation] The <kbd>?</kbd> shortcut depends on the keyboard
  layout, and the shortcut overview may list stale shortcuts for windows not
  yet opened.
- ![Limitation][limitation] Select Similar and Show Related use loaded
  envelopes only. Mailing-list matches use the list's `List-Id`, or its
  `List-Post` address when it has none.
- ![Limitation][limitation] While the dense layout is on, its compact density
  and hidden panels are written to the normal preferences, so they persist
  after a restart.
- ![Limitation][limitation] The default sort and threading for new folders
  come from the message list header's state; `guiMsgListShowThreading` is no
  longer written. Group By is not tested against the fake server. A folder's
  saved position is restored only after its message UIDs have loaded.
- ![Limitation][limitation] Two-line message rows show no preview text,
  because message bodies are not fetched for the list, and no contact photos,
  only the sender's initials. In the Unified Inbox, Two Lines names each
  message's account; Columns show it only in the row's tooltip. All rows share
  one height, so date group headings are as tall as a two-line row. The column
  header, with its column chooser and sorting by clicking, is hidden in Two
  Lines.
- ![Limitation][limitation] Check for New Mail asks each online account for
  the counts it already knew and resynchronizes the open folder; it does not
  list new folders or download messages of closed folders. Accounts are
  otherwise refreshed every five minutes, or immediately in the open folder
  when the server supports IDLE.
- ![Limitation][limitation] Account headers show only the INBOX unread count,
  not a total across folders. Go to Next Unread Folder covers the active
  account and folders already listed in the tree; folders whose count has not
  arrived are skipped, so the first use after connecting may skip some.
- ![Limitation][limitation] The Unified Inbox
  ([#223](https://github.com/mikepadiernos/plasma-impresario/issues/223)) lists
  the INBOX of each included account only. It has no threads, Group By, drag
  and drop, quick search or Show Related, and sorts on the client rather than
  on the server. Moving or copying messages of several accounts at once is
  refused. Notifications about different accounts replace each other. An
  account added while the Unified Inbox is shown appears after turning it off
  and on again. Offline accounts still list their cached messages, and the
  status line names them.
- ![Limitation][limitation] These paths are unit-tested but not yet exercised
  in a window against a live server.

**Workarounds:**

- Enable SPECIAL-USE on the server, or set the archive folder in the account
  settings and reconnect.
- Expand the folder tree before using Go to Folder.
- Without Undo, open the destination from the notification and move the
  messages back.
- Update the account settings after renaming the archive or Sent folder.
- Open the folder to confirm the result of a bulk action.
- Turn off the dense layout before changing the density or panels you want to
  keep.
- In Two Lines, sort from **View › Sorting**, or switch **View › Message Rows**
  to Columns to choose and resize columns.
- Use Reload All Folders to pick up new folders, and expand a branch so that
  Go to Next Unread Folder can visit it.
- In the Unified Inbox, move or copy the messages of one account at a time,
  and open an account's INBOX to thread, group or search its messages.

**Proposed improvements:**

- Recognize localized names and allow assigning special folders per account.
- Query the server from Go to Folder and from related searches.
- Locate moved messages by `Message-ID` when COPYUID is missing.
- Run Empty and Undo on a dedicated connection.
- Follow renamed archive and Sent folders.
- Record recent folders after the operation succeeds, prune deleted ones, and
  show folder paths.
- Report bulk results after the server confirms them.
- Keep the dense layout's preferences separate from the normal ones.
- Offer an opt-in preview line from already downloaded text, contact photos
  from the address book only, and shorter date group headings.
- Let Check for New Mail re-list folders, add per-account and per-folder check
  intervals, and offer an account-wide or all-accounts unread total and
  next-unread search.
- Extend the Unified Inbox to other special folders, threads and quick search
  across accounts, and moves between accounts.
- Add live window-level tests.

### Composing and sending

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                                                                                     |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [#450](https://github.com/mikepadiernos/plasma-impresario/issues/450) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/450#issuecomment-7179584259) | The composer, replies and forwards, drafts and failed-send recovery ([#41](https://github.com/mikepadiernos/plasma-impresario/issues/41), [#93](https://github.com/mikepadiernos/plasma-impresario/issues/93)) |

**Current behavior:**

- ![Resolved][resolved] New messages, replies, including **Reply to All but
  Me**, and **Forward** send through the account the composer was opened from,
  mark the original as answered or forwarded, and file a copy in Sent (#41,
  #93, [#224](https://github.com/mikepadiernos/plasma-impresario/issues/224)).
- ![Resolved][resolved] A refused send keeps the composer open with the
  server's error, and the message stays in an automatic draft until it is
  sent; **Send** tries again. An automatic draft left by a crash opens again at
  the next start.
- ![Limitation][limitation] The copy for Sent is stored before the outgoing
  server is asked, so every refused attempt leaves another copy in Sent.
- ![Limitation][limitation] Sending a forward before the forwarded message has
  downloaded is refused with "some parts are not available".
- ![Limitation][limitation] **Forward** attaches the original message; there is
  no inline forward.
- ![Limitation][limitation] **Save Draft?** writes a local `.draft` file with
  the recipients, subject and text, but not the attachments.
- ![Limitation][limitation] The live test,
  `tools/dev-container/live-imap-compose.sh`, sends through a local sendmail
  script; SMTP delivery is covered by offscreen tests only.

**Workarounds:**

- After a refused send succeeds, delete the extra copies from Sent.
- If a forward is refused because parts are not available, wait a moment and
  press **Send** again.
- To forward inline, copy the text into a new message.
- Attach files again after opening a draft.

**Proposed improvements:**

- File the Sent copy after the outgoing server accepts the message, or remove
  the copy of a refused attempt.
- Wait for attachments to download when **Send** is pressed, with progress,
  instead of refusing.
- Add an inline forward mode.
- Store drafts in the account's Drafts folder on the server, attachments
  included.
- Exercise SMTP delivery in the live test with a local SMTP server.

### Search

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [#454](https://github.com/mikepadiernos/plasma-impresario/issues/454) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/454#issuecomment-7181065871) | The quick search's query syntax and builder, **Date** and **Search In** menus, saved searches with a date range or places, searches the server cannot run, **Clear All**, search history and suggestions, saved searches as virtual folders, mailing-list views, the offline search index, search snippets and relevance, search facets, the Search Results panel, keyboard access to search, finding saved files, and opening messages by Message-ID |

**Current behavior:**

- ![Resolved][resolved] The **Date** menu in the search options limits the
  quick search to today, the past week, month or year, or custom days with an
  optional start and end. The range narrows text, `from:`-style and raw IMAP
  queries, works on its own and is stored with saved searches. Presets stay
  relative and move on when the day changes
  ([#95](https://github.com/mikepadiernos/plasma-impresario/issues/95)).
- ![Resolved][resolved] The **Search In** menu searches the current folder,
  chosen folders of any account, the whole account or all accounts. Results
  appear in one list as each folder is searched, labelled with the account and
  folder, and places that could not be searched are named in the status line
  and the notification bar ([#331](https://github.com/mikepadiernos/plasma-impresario/issues/331)).
- ![Resolved][resolved] A search of several places runs in the background.
  The status line's tooltip shows each account's progress and count, a slow or
  failing account does not hold up the others, and **Stop** ends the search
  while keeping the results found so far; answers that arrive later are not
  shown. **Retry**, in the status row and the notification bar, searches again
  only the accounts and folders that failed, and reconnects an account whose
  connection was lost ([#332](https://github.com/mikepadiernos/plasma-impresario/issues/332)).
- ![Resolved][resolved] The quick search keeps its field toggles, raw IMAP
  queries after `:=` and fuzzy matching, and uses ESEARCH where the server
  advertises it, plain SEARCH otherwise. A search the server cannot run shows
  **Search failed** and the reason in the status line instead of an empty
  list: a key that needs a capability the server does not advertise is named
  before anything is sent, and a refused search repeats the server's own
  explanation ([#333](https://github.com/mikepadiernos/plasma-impresario/issues/333)).
- ![Resolved][resolved] The quick search reads a query syntax: fields such
  as `from:`, `subject:`, `list:`, `tag:`, `is:unread` and `has:attachment`,
  `date:` and `size:` with `<`, `<=`, `>` and `>=`, quoted phrases, `-` or
  `NOT`, `OR` and parentheses. The whole query is checked before anything is
  sent; an invalid one is explained below the search field, with the faulty
  text selected, and the list keeps the previous results
  ([#334](https://github.com/mikepadiernos/plasma-impresario/issues/334)).
- ![Resolved][resolved] **Search Builder...** in the search options and the
  **Mailbox** menu (<kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>B</kbd>) builds a
  query from rows of field, comparison and value, matching all or any of them.
  It opens with the current search and shows the query it will run; a query
  it cannot show is explained instead
  ([#335](https://github.com/mikepadiernos/plasma-impresario/issues/335)).
- ![Resolved][resolved] A search applies within the quick filters
  (**Unread**, **Flagged**, **With Attachments**). A row above the list shows
  the query, date range, places and filters in effect, and its **Clear All**
  button, **View › Filter › Clear Search and Filters** and
  <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Backspace</kbd> clear them in one
  step. The folder's list comes back with the message that was current before
  the first search or filter, or with the message opened among the results
  ([#336](https://github.com/mikepadiernos/plasma-impresario/issues/336)).
- ![Resolved][resolved] The quick search remembers the last 20 searches run
  with <kbd>Enter</kbd>, the search builder or a suggestion, per profile and
  across restarts. While typing, a list below the search field offers matching
  recent searches, `tag:` terms for favorite tags, `from:` terms for address
  book contacts and the shown account's folders; <kbd>Down</kbd> opens it on
  request. **Remember Searches**, **Suggest While Typing** and **Clear Search
  History** are in the search options. <kbd>Enter</kbd> runs the typed text
  unless a suggestion is chosen, and <kbd>Esc</kbd> closes the list without
  clearing the search
  ([#337](https://github.com/mikepadiernos/plasma-impresario/issues/337)).
- ![Resolved][resolved] A **Saved Searches** section below the folders lists
  the saved searches of every account as virtual folders, by name, with the
  unread and total messages each found when it last ran. Choosing one runs it,
  first switching to its account when it searches the shown folder or the
  whole account. Its counts follow the list while it is shown, such as when a
  message is read, and are kept across restarts. Choosing a folder, typing
  another search or switching accounts leaves the virtual folder. Searches
  saved earlier appear unchanged; nothing is moved, copied or changed to fill a
  virtual folder
  ([#338](https://github.com/mikepadiernos/plasma-impresario/issues/338)).
- ![Resolved][resolved] Saved searches are managed from the context menu of
  the **Saved Searches** section, and from **Saved Searches** in the search
  options. **Edit Search** runs a search and shows a bar above the list; the
  list previews the changed query, fields, dates and places until **Save** or
  **Cancel**. **Rename...** (<kbd>F2</kbd>) refuses empty names and names the
  account already uses, and **Delete...** (<kbd>Del</kbd>) asks first.
  Dragging a search, or **Move Up** and **Move Down**
  (<kbd>Alt</kbd>+<kbd>Up</kbd> and <kbd>Alt</kbd>+<kbd>Down</kbd>), sets the
  order across accounts, and **Move to Account** gives a search to another
  account. Each tooltip shows the query, the places and the dates. Searches of
  a removed account, and chosen folders that the server no longer lists, are
  kept and marked with a warning; **Move to Account**, **Choose Folders...**
  and **Leave Out Missing Folders** update them
  ([#339](https://github.com/mikepadiernos/plasma-impresario/issues/339)).
- ![Resolved][resolved] Messages are read with their `List-Id`,
  `List-Subscribe`, `List-Unsubscribe`, `List-Help`, `List-Archive` and
  `List-Owner` headers. The reader names a message's list and offers its
  actions as links; each asks first, says whether it writes a message in
  Plasma Impresario's composer or opens a web page, and defaults to
  **Cancel**. **Mailing Lists** in the search options lists the lists of the
  loaded messages with their counts and shows one's messages, and **Pin Shown
  List as Saved Search** saves that view as a search of every folder of every
  account, named after the list; pinning it again runs the saved one. `list:`,
  Show Related and grouping by mailing list use the `List-Id`, or the posting
  address of a list without one
  ([#340](https://github.com/mikepadiernos/plasma-impresario/issues/340)).
- ![Resolved][resolved] **Index downloaded mail for searching offline**, in
  an account's **Offline** settings, is off by default and set per account.
  When on, the account keeps a full-text index of the headers, flags and
  downloaded text of the messages in its offline cache, inside the cache's
  SQLite database. Nothing is downloaded for the index. **Download Message
  Text for Offline Search...** in a folder's context menu says how many
  messages lack their text and asks before fetching it. While the account is
  offline, the quick search, the query syntax and saved searches are answered
  from the index; the status line ends with **offline, in mail indexed on this
  computer**, and its tooltip counts the messages whose text was not
  downloaded, so that only their headers were searched. Turning the option off
  deletes the index
  ([#341](https://github.com/mikepadiernos/plasma-impresario/issues/341)).
- ![Resolved][resolved] The index uses SQLite's FTS5 extension with its
  `trigram` tokenizer, which finds any part of a word, as IMAP `SEARCH` does,
  and folds the case of non-ASCII letters. It needs SQLite 3.34 or later
  (Debian 12 ships 3.40, Fedora 44 3.51). The account checks for it when it
  connects; without it the index is reported as unavailable and searches use
  the server as before
  ([#341](https://github.com/mikepadiernos/plasma-impresario/issues/341)).
- ![Resolved][resolved] **Application › Offline Search Index…** shows the
  shown account's index: each folder of its offline cache with its state
  (**Indexed**, **Partly indexed**, **Changed since indexed**, **Not indexed
  yet** or **Not on this computer**), its offline, indexed and with-text
  message counts and when it was last indexed, how many folders are confirmed
  as indexed, the progress of a running update, and a warning when some
  folders are not fully indexed. **Pause** stops updates until **Resume** and
  is remembered for the account; **Rebuild…** asks first, then empties the
  index and builds it again. An offline search adds **results may be
  incomplete** to the status line when a searched folder is not fully
  indexed, indexing is paused or an update is under way, and its tooltip says
  which
  ([#342](https://github.com/mikepadiernos/plasma-impresario/issues/342)).
- ![Resolved][resolved] With the index on, a search of several places or of
  the Unified Inbox is answered from the index first while the account is
  online too; the status line ends with **in mail indexed on this computer**.
  Folders without indexed matches are not opened on the server. **Search
  Server**, next to the status line, then searches the same places on the
  server and adds the messages it finds there, without listing a message
  twice. When a folder was renumbered on the server (a new UIDVALIDITY), its
  indexed matches are dropped, the results are marked as possibly incomplete,
  and the tooltip names the folder
  ([#343](https://github.com/mikepadiernos/plasma-impresario/issues/343)).
- ![Resolved][resolved] Unchecking a folder in **Application › Offline Search
  Index…** keeps it out of the index: its indexed text is removed at once, it
  is shown as **Excluded**, and the choice is kept for the account. Offline
  searches skip it and say so in the tooltip; online, **Search Server**
  searches it on the server
  ([#344](https://github.com/mikepadiernos/plasma-impresario/issues/344)).
- ![Resolved][resolved] Decrypted text is indexed only after **Also index the
  decrypted text of encrypted messages I open**, in the account's **Offline**
  settings, is turned on; it is off by default. The text and HTML parts of an
  opened message are added once it is decrypted, never its attachments, and
  turning the option off removes them from the index. The index dialog counts
  the messages that hold decrypted text
  ([#344](https://github.com/mikepadiernos/plasma-impresario/issues/344)).
- ![Resolved][resolved] **Delete Index…** in the index dialog asks first,
  then deletes the account's index from the offline cache and turns offline
  search off, keeping the mail. Turning the option off, choosing **Don't use
  cache**, or removing the account also deletes the index; removing an
  account says so and warns when its files cannot be deleted
  ([#344](https://github.com/mikepadiernos/plasma-impresario/issues/344)).
- ![Resolved][resolved] **Also index the text of downloaded attachments up
  to** a size, in the account's **Offline** settings, is off by default and
  set per account, with a limit of 10 MB per file. When on, the index reads
  the text of attached files once they are downloaded, such as by opening or
  saving them, through the KFileMetaData extractors that Plasma's file search
  uses; text and HTML files are read directly. Nothing is downloaded for it.
  Pictures, sound, video, keys and encrypted data are skipped. `body:` and
  `text:` find the text, and the index dialog counts the files read, waiting
  to be downloaded, over the limit or unreadable, and warns about formats no
  installed extractor reads. Turning the option off or changing the limit
  indexes the affected messages again
  ([#345](https://github.com/mikepadiernos/plasma-impresario/issues/345)).
- ![Resolved][resolved] While a search is shown, each row of the message list
  gains a line with a snippet of the message's text around the first match,
  with the matched words in bold and the highlight color. Rows already name
  the sender and date, and in results of several places or of the Unified
  Inbox, the account and folder. **Mailbox › Sorting › Relevance** now orders
  those results too: best match first, then newest. Date stays the default;
  choosing **Relevance** or another sort during a search is remembered for
  later searches
  ([#346](https://github.com/mikepadiernos/plasma-impresario/issues/346)).
- ![Resolved][resolved] **Narrow**, above the results of a search, and **View
  › Filter › Narrow Results** count the shown results by account, folder,
  sender, date (**Today**, **Yesterday**, **Earlier This Week**, **Last
  Week**, **Earlier This Month**, **Earlier This Year**, **Older**), with or
  without attachments, and tag, and list the 25 most frequent values of each
  with a note for the rest. Choosing values narrows the list within the quick
  filters: values of one kind are alternatives, kinds combine, and each kind
  is counted among the messages that the other kinds keep. Each chosen value
  shows as a chip, such as **Sender: Ann**, that removes it; **Clear Facets**
  and **Clear All** remove them all
  ([#347](https://github.com/mikepadiernos/plasma-impresario/issues/347)).
- ![Resolved][resolved] **View › Embedded Panels › Search Results**, off by
  default, lists the results of the quick search in a panel, and the message
  list keeps showing its folder; every search is then run as a search of the
  chosen places, the current folder included. Opening a result shows it in
  the reader and selects it in the list only when the list shows it. **Show
  in Folder**, in the panel's header and context menu and in the message
  list's context menu while it shows results of several places, switches to
  the result's account and folder and selects it there; the panel keeps its
  results. The panel sorts by relevance (equal matches newest first) or date,
  remembered across restarts, and offers **Stop**, **Retry** and **Search
  Server**. Turning the panel off moves the results to the message list, and
  turning it on moves them back
  ([#348](https://github.com/mikepadiernos/plasma-impresario/issues/348)).
- ![Resolved][resolved] **Mailbox › Find...** (<kbd>Ctrl</kbd>+<kbd>F</kbd>)
  and <kbd>/</kbd> move to the quick search with its query selected, so that
  typing replaces it, and <kbd>Esc</kbd> returns to the message list. While the
  reader or its action bar has focus and a message is shown,
  <kbd>Ctrl</kbd>+<kbd>F</kbd> opens the reader's find bar instead.
  <kbd>F6</kbd> and <kbd>Shift</kbd>+<kbd>F6</kbd> stop at the quick search
  just before the message list, and at the Search Results panel when it is
  shown
  ([#349](https://github.com/mikepadiernos/plasma-impresario/issues/349)).
- ![Resolved][resolved] **Mailbox › Find Saved Files…**, and **Find Saved
  Copies…** in the attachment panel, find saved attachments and messages with
  Plasma's File Search in the default file manager, through a `baloosearch:`
  URL, or with a live search of the names and text of a folder's files through
  `filenamesearch:`. The dialog reads File Search's settings and says when it
  is not installed or turned off, does not include or skips the folder, skips
  hidden folders, or indexes only file names. The save dialogs for
  attachments, parts and messages share a remembered folder, which the search
  starts from
  ([#350](https://github.com/mikepadiernos/plasma-impresario/issues/350)).
- ![Resolved][resolved] A `mid:` URL, a `mid:` argument or `--message-id`
  opens the message with that Message-ID, in the running window if there is
  one. Listed messages and the offline search index answer at once; otherwise
  the app asks before searching every folder of every online account, shows
  progress with **Stop**, and lists the folders it could not search. Invalid
  IDs are explained without searching
  ([#351](https://github.com/mikepadiernos/plasma-impresario/issues/351)).
- ![Resolved][resolved] The query syntax follows the advanced search of Plasma
  Opus where it fits mail, and reads its `content:`, `contents:` and `tags:` as
  `body:` and `tag:`. **Find Saved Files…** words its notices as Plasma Opus
  does, saying what File Search is set to index rather than confirming that it
  has, and searches the index for an indexed folder inside a folder that is not
  indexed or is excluded. Search needs nothing from Plasma Opus
  ([#352](https://github.com/mikepadiernos/plasma-impresario/issues/352)).
- ![Limitation][limitation] The server compares the calendar day in each
  message's Date header, in the sender's time zone, because IMAP's `SENTSINCE`
  and `SENTBEFORE` disregard the time and zone. The list shows local time, so a
  message sent near midnight in another zone can fall outside a range that its
  listed date is inside, or the other way round.
- ![Limitation][limitation] The Date header is set by the sender and can be
  wrong. Servers differ in how they treat a message without one.
- ![Limitation][limitation] Each account searches its folders one after
  another on its own connection, by selecting each folder. While a search runs,
  the account's selected folder changes; the shown folder is selected again
  when the account's search finishes or is cancelled. An account that is not
  shown stays on the last folder it searched.
- ![Limitation][limitation] The headers of every result are loaded before the
  next folder is searched, so a broad search of many large folders downloads
  many headers and can take a while.
- ![Limitation][limitation] Results show the full mailbox name, such as
  `Archive/2024`, rather than a translated special-folder name. **Selected
  Folders** lists only folders the sidebar has loaded; **Current Account** and
  **All Accounts** search every folder on the server.
- ![Limitation][limitation] **Retry** of an account whose password was rejected
  reports the failure again; it does not ask for a new password. A stopped
  search cannot be resumed, only started again.
- ![Limitation][limitation] The check before sending knows only common
  extension keys, such as `FUZZY`, `OLDER`, `YOUNGER`, `MODSEQ`, `SAVEDBEFORE`
  and `X-GM-RAW`; other keys go to the server, and its answer is shown.
  Dovecot accepts `FUZZY` without advertising `SEARCH=FUZZY`, so the fuzzy
  option stays off there and a raw `FUZZY` query is explained as unsupported.
  A server that advertises `SEARCH=FUZZY` is covered by offline tests only.
- ![Limitation][limitation] IMAP cannot ask for attachments, so
  `has:attachment` finds messages whose top-level type is `multipart/mixed`.
  It misses an attachment inside a signed or encrypted message or a
  `multipart/related` one, and finds a `multipart/mixed` message without a
  real attachment.
- ![Limitation][limitation] Words without quotes must all match, each
  anywhere in the chosen fields; `shared inbox` no longer means the phrase.
  The find bar of an opened message shows only the first subject or body term.
- ![Limitation][limitation] Text before a colon is read as a field name, so
  an unknown one such as in `https://example.org` or `ticket:42` is refused
  instead of searched; `Re:` or `Note:` followed by a space is still text.
  Field names, `is:` values and the upper-case operators `AND`, `OR` and `NOT`
  are English in every language.
- ![Limitation][limitation] The builder has one level of rows. A query with
  groups inside groups or a negated group cannot be opened in it, and a raw
  `:=` query is replaced by the built one. The query text in the dialog is read
  only, and the builder is unavailable while the quick search is disabled.
- ![Limitation][limitation] A builder value with spaces is searched as a
  phrase, unlike the same words typed without quotes in the search field.
- ![Limitation][limitation] Online, a search of the shown folder alone is
  still run by the server; only searches of several places and of the Unified
  Inbox use the index first. The index holds only the messages the offline
  cache keeps, so mail that was never downloaded, or is older than the days
  the cache remembers, is found only after **Search Server**, which is not
  pressed automatically. Offline, a search of the shown folder runs as a
  search of the account's indexed folders, with each result labelled by its
  folder.
- ![Limitation][limitation] Online, each folder with indexed matches is still
  selected on the server to show its results, so the account's selected folder
  changes as described above. A folder that the server lists again during the
  search, such as after a reconnect, can drop the indexed results found in it
  so far; run the search again, or press **Search Server**
  ([#343](https://github.com/mikepadiernos/plasma-impresario/issues/343)).
- ![Limitation][limitation] Terms shorter than three characters have no
  trigram, so the index reads every message for them and folds the case of
  ASCII letters only. Each message's text is cut at 512K characters.
- ![Limitation][limitation] The index keeps the text of a message's text
  parts, and of attached files only when that option is on. The text of
  attached messages is not indexed, only their subjects or names.
  `has:attachment` follows the top-level type, as on the server.
- ![Limitation][limitation] Attached files are read only once downloaded, and
  only those within the limit; the server's size is the encoded size, so a
  file is judged by about three quarters of it until it is downloaded. Which
  formats can be read depends on the KFileMetaData extractors installed, and a
  build without KFileMetaData reads text and HTML files only. Extraction runs
  in the application, during short index batches, so a large or slow document
  can briefly hold up the window. Attachments of encrypted messages are not
  read, even with decrypted indexing on, and the read text is stored
  unencrypted with the rest of the index
  ([#345](https://github.com/mikepadiernos/plasma-impresario/issues/345)).
- ![Limitation][limitation] Snippets appear only in rows of several lines,
  not in the column layout, and while a search is shown every row grows to
  three lines, also rows without a snippet. A snippet uses only text parts
  already downloaded, never the text of attachments, and nothing is fetched
  for it: a message whose text was not downloaded has none, and one found by
  its headers alone shows the start of its text. In a conversation, replies
  show a snippet that arrives later only once they are drawn again, such as
  by scrolling
  ([#346](https://github.com/mikepadiernos/plasma-impresario/issues/346)).
- ![Limitation][limitation] Relevance scores matches in the subject, senders
  and recipients of loaded messages; body text is not scored, so a message
  found only by its text ranks last among the matches. With **Descending**,
  the best matches come last
  ([#346](https://github.com/mikepadiernos/plasma-impresario/issues/346)).
- ![Limitation][limitation] Facets apply in the application to the results
  already loaded; they do not search the server again. The sender, date and
  attachments of a message are known only once its headers are loaded, so
  until then it is not counted for them and a facet of these kinds hides it.
  Counts are taken when the menu opens and do not follow the list while it is
  open. Facets reset when the search, its dates or places, or the shown list
  change, and are not kept with saved searches. Dates are grouped by the
  local day of the Date header
  ([#347](https://github.com/mikepadiernos/plasma-impresario/issues/347)).
- ![Limitation][limitation] While a result outside the shown folder is open
  from the Search Results panel, the message list keeps its own selection, so
  the list's actions, such as **Archive** or **Move to Trash**, apply to the
  message selected there and not to the result in the reader. **Show in
  Folder** of a result of another account switches the shown account. The
  quick filters and the facets above the message list narrow the panel while
  it holds the results, and **Mailbox › Sorting** does not sort it. A saved
  search chosen in the sidebar still shows in the message list. A search
  selects each searched folder on the account's connection, so changes to the
  shown folder arrive once the search is over
  ([#348](https://github.com/mikepadiernos/plasma-impresario/issues/348)).
- ![Limitation][limitation] The quick search stays above the message list
  until the designed header (M14.02,
  [#306](https://github.com/mikepadiernos/plasma-impresario/issues/306)) can
  host it. From an embedded panel, such as Message Details,
  <kbd>Ctrl</kbd>+<kbd>F</kbd> moves to the quick search rather than finding
  in the message. In One At a Time, Find from outside the reader goes back to
  the list first. <kbd>F6</kbd> skips the quick search while it is unavailable,
  such as before a folder is chosen
  ([#349](https://github.com/mikepadiernos/plasma-impresario/issues/349)).
- ![Limitation][limitation] The coverage notice reads File Search's settings,
  not its index: files saved in the last few minutes, or before their folder
  was added, may not be indexed yet, and indexing progress is not shown. File
  name exclude filters are not checked, so a file type that File Search skips
  is reported as covered. Without the `filenamesearch` worker, **Live Search**
  opens the folder instead. Only the last save folder is remembered. If hidden
  folders are indexed, File Search can also index the files in the app's cache
  folder
  ([#350](https://github.com/mikepadiernos/plasma-impresario/issues/350)).
- ![Limitation][limitation] Opening by Message-ID ignores the Content-ID part
  of a `mid:` URL and opens the whole message. With several copies, the first
  one found opens and the others are only named. The server search asks every
  folder of every online account one after another, so it can take a while on
  large accounts, and offline accounts are not searched. The question before
  it hides after eight seconds, like other notices. An index entry for a
  message deleted since the last update opens its folder without selecting
  anything or saying why
  ([#351](https://github.com/mikepadiernos/plasma-impresario/issues/351)).
- ![Limitation][limitation] The query parser of Plasma Opus is built into it
  rather than published as a library, so the syntax is shared by convention
  and kept in step by hand. The two differ where mail and files differ: bare
  words search the chosen mail fields instead of file names; `-` before a
  parenthesis negates the group here but is a plain minus there; sizes need
  `<`, `<=`, `>` or `>=` and stop at 4 GB, the IMAP limit; and file fields such
  as `name:` or `type:` are refused. `Content:` or `Tags:` followed by a space
  is now read as an empty field rather than as text. Saved searches run as
  virtual folders when chosen, while Plasma Opus loads them for review first,
  and neither app reads the other's
  ([#352](https://github.com/mikepadiernos/plasma-impresario/issues/352)).
- ![Limitation][limitation] Decrypted text is added only when a message is
  opened and decrypted in the reader with the option on; messages decrypted
  earlier are not added until they are opened again. Inline PGP is indexed as
  the encrypted text it is, because the reader does not decrypt it for the
  index. A message whose decryption fails adds nothing
  ([#344](https://github.com/mikepadiernos/plasma-impresario/issues/344)).
- ![Limitation][limitation] The index is not encrypted. Its text, including
  decrypted text when that option is on, can be read by anyone who can read
  the account's cache files, as the cached mail can. The connection log, when
  turned on, records what the server sends, whatever is excluded from the
  index ([#344](https://github.com/mikepadiernos/plasma-impresario/issues/344)).
- ![Limitation][limitation] Flags are as of the last time the account was
  online. A search key the index does not keep, such as `FUZZY`, a raw
  header other than the list and type headers, or a modification sequence,
  fails the search with an explanation instead of searching the server.
- ![Limitation][limitation] **Download Message Text for Offline Search...**
  fetches only each message's main text part and needs a connection. It
  covers the messages the index already holds, so a folder has to be opened
  online once first. The index catches up about half a minute after new mail
  or flag changes arrive, or a message is read.
- ![Limitation][limitation] Deleting the index removes its text from the
  cache database but does not overwrite the freed space, and the database file
  stays the same size until SQLite reuses it. Should SQLite refuse to remove
  the emptied tables, their definitions stay until the next deletion. Online,
  a search of several places skips an excluded folder until **Search Server**
  is pressed. Turning decrypted indexing off while indexing is paused leaves
  those messages out of the index until **Resume**
  ([#344](https://github.com/mikepadiernos/plasma-impresario/issues/344)).
- ![Limitation][limitation] **Offline Search Index…** shows the shown account
  only, and lists only the folders its offline cache knows. A folder is
  **Partly indexed** while some of its cached messages have no headers on this
  computer; headers load as messages are listed, so a large folder may need to
  be scrolled through online first. Pausing during an update keeps the
  messages indexed so far, but the folder under way is not confirmed until a
  later update finishes it. A search started while an update runs is answered
  from what is indexed at that moment
  ([#342](https://github.com/mikepadiernos/plasma-impresario/issues/342)).
- ![Limitation][limitation] **Clear All** restores the selection only in a
  single folder. In the Unified Inbox it clears the search and filters but
  leaves the selection as it is. After a search of several places, the list
  returns to the message that was current before the search; a result opened
  in another folder is not kept.
- ![Limitation][limitation] Filters still on from an earlier session have no
  position to return to, so **Clear All** shows the whole folder and keeps the
  current message. The **Search In** places stay chosen after **Clear All**.
- ![Limitation][limitation] Folder suggestions cover only folders of the shown
  account that the sidebar has already listed, and choosing one replaces the
  **Search In** places with that folder.
- ![Limitation][limitation] Contact suggestions come only from the address
  book plugin and complete a plain word as `from:`; type `to:`, `cc:` or `bcc:`
  first for another field. Tag suggestions cover only the favorite tags.
- ![Limitation][limitation] Running a saved search, and searches started for
  another reason, such as **Show Related**, a field filter or a change of date
  range or places, are not remembered.
- ![Limitation][limitation] A virtual folder's counts are only as fresh as
  its last run: they change while it is shown, but mail that arrives or is
  read elsewhere is not counted until it is chosen again. A search of several
  places keeps its counts only when every folder was searched, not after
  **Stop**.
- ![Limitation][limitation] A search saved for the shown folder runs in the
  folder its account shows when it is chosen, so its counts depend on that
  folder.
- ![Limitation][limitation] A chosen folder is reported missing only after
  its account's folders, down to the folder's parent, were listed in this
  session. Until then the folder is assumed to exist, and a search of it fails
  when it runs. A removed account's folders are reported at once.
- ![Limitation][limitation] Choosing a folder, switching accounts or opening
  another saved search ends an edit without saving it or asking. Moving a
  search to another account forgets its counts, and is refused while that
  account has a search of the same name. One order applies to every account;
  searches saved later follow the ordered ones by name.
- ![Limitation][limitation] The earlier shell-like <kbd>Up</kbd> and
  <kbd>Down</kbd> history of the search field is replaced by the suggestion
  list. **Remember Searches** and **Suggest While Typing** apply to every
  profile; the history itself is kept per profile, and turning **Remember
  Searches** off clears it.
- ![Limitation][limitation] IMAP's `HEADER` key matches part of a header, so
  `list:dev.example.org` also finds a list such as `old-dev.example.org`, and
  a list whose `List-Post` holds the ID of another list.
- ![Limitation][limitation] **Mailing Lists** offers only the lists of
  messages loaded in the shown list. A list without loaded messages there has
  to be typed as `list:`.
- ![Limitation][limitation] The first start after this change empties the
  message header cache once, so each folder downloads its message headers
  again when it is next opened. Every message then fetches its `List-*`
  headers too, a few hundred bytes more for mail from a list.
- ![Limitation][limitation] Unsubscribing opens a message to send or a web
  page; one-click unsubscribing (`List-Unsubscribe-Post`, RFC 8058) is not
  offered. Each action follows the first address of its header that is a
  `mailto:`, `http:` or `https:` link; other links are not shown.

**Workarounds:**

- Widen a custom range by a day at either end when a message near a boundary
  is missing, then sort by date.
- To match by arrival time instead, allow raw IMAP search in the search
  options and use `SINCE` or `BEFORE`, which compare the date the server
  received the message.
- Choose a few folders rather than a whole account to keep a search short.
- Expand an account's folders in the sidebar before **Selected Folders** to pick
  subfolders.
- After a rejected password, correct it in the account settings, then press
  **Retry** or **Enter** in the search field.
- When a search is explained as unsupported, leave out the named key or use
  the field search, which every server supports.
- Put a phrase, an address with a colon or a web address in quotation marks,
  such as `"shared inbox"` or `"https://example.org"`.
- To find attachments the `has:` term misses, combine a sender or subject
  with `size>100K`.
- For a query the builder cannot open, type it in the search field, or use
  one builder row per word to match the words anywhere.
- To keep a result from another folder in view, double-click it to open it
  in its own window before clearing the search.
- To search a folder the suggestions do not offer, expand it in the sidebar
  first, or pick it with **Search In › Selected Folders...**.
- Keep a search you run often as a saved search, which stays until deleted,
  rather than relying on the history of the last 20.
- Choose a virtual folder again to bring its counts up to date. Save a search
  of chosen folders rather than of the shown folder to have it look in the
  same place each time.
- Expand an account's folders in the sidebar to have missing chosen folders
  reported, and press **Save** before choosing a folder while editing a search.
- When list IDs overlap, leave the other list out, as in
  `list:dev.example.org -list:old-dev.example.org`, or edit a pinned search to
  add a sender or subject.
- To view a list not offered in **Mailing Lists**, open a folder that holds
  its messages, or type `list:` followed by its ID.
- Before going offline, choose **Store forever** in the account's **Offline**
  settings, open the folders you need, and use **Download Message Text for
  Offline Search...** on them, so that their text can be searched.
- Search offline for words of three letters or more, and search for an
  attached message by its subject or file name.
- Before going offline, open **Application › Offline Search Index…** and check
  that the folders you need are **Indexed**; open and scroll through a
  **Partly indexed** folder online, and wait for **Changed since indexed** to
  clear.
- Online, press **Search Server** when an indexed search misses older or
  undownloaded mail, or reports a renumbered folder.
- To search the text of an attachment offline, open or save it once while
  online; the index reads it shortly after. For a format the index dialog
  lists as unreadable, install the distribution's KFileMetaData extractors,
  which Plasma's file search uses as well.
- To see snippets, use a layout of several lines, and open a message once
  online, or download the text of its folder for offline search, so that its
  text is on this computer. Choose **Ascending** with **Relevance** to see the
  best matches first.
- To act on a result found by the Search Results panel, press **Show in
  Folder** first, so that the message list selects it, or turn the panel off
  to act on the results in the message list.
- To find text in the open message from the list, press <kbd>F6</kbd> until
  the reader has focus, then <kbd>Ctrl</kbd>+<kbd>F</kbd>, or choose
  **Search...** in the reader's context menu.
- To find a copy saved in a folder that File Search does not index, use
  **Live Search**, or add the folder in **System Settings › Search › File
  Search**; the notice updates when you return to the dialog. For a copy saved
  somewhere other than the last save folder, pick its folder with **Choose…**.
- If a message opened by Message-ID is not found, open its account online and
  answer **Search Servers** again, or turn on the offline search index for the
  account so that later lookups do not need the server.
- To search for text that reads as a field, such as `Content: draft` or
  `name:report`, quote it: `"Content:" draft`. For a query that behaves the
  same in Plasma Opus, keep to the shared terms: words, phrases, operators,
  `date`, `size` with a comparison, `content:` and `tag:`
  ([#352](https://github.com/mikepadiernos/plasma-impresario/issues/352)).
- Leave decrypted indexing off unless the disk is encrypted, and exclude
  folders whose text should not be copied into the index. To free the space
  as well, remove the account, which deletes its cache files, and add it
  again.

**Proposed improvements:**

- Offer a choice between the sent date and the received date.
- Ask the server for one extra day at each end and keep only the messages
  whose local date is inside the range, once their envelopes are loaded or the
  local index (M05.04) has them.
- Search on a separate connection, or with `MULTISEARCH` (RFC 7377) where the
  server offers it, so that the shown folder stays selected.
- Load the headers of results as their rows are shown.
- Offer fuzzy matching on servers that accept `FUZZY` without advertising it,
  once a test search succeeds.
- Show translated special-folder names in results, and list unloaded folders
  in **Selected Folders** on demand.
- Ask for the password again when **Retry** meets a rejected password, and
  offer **Continue** after **Stop** to search the remaining folders.
- Confirm `has:attachment` against each result's body structure, once loaded
  or in the local index.
- Highlight every searched word in the find bar, not only the first.
- Accept translated field names next to the English ones.
- Show nested groups in the builder as indented rows, and let the query text
  be edited there.
- Open the folder of a result picked in a search of several places when
  **Clear All** is pressed, and restore the Unified Inbox selection.
- Offer to reset the **Search In** places with **Clear All**.
- Suggest folders of every account and add a folder to the chosen places
  instead of replacing them.
- Suggest contacts from recent correspondents as well as the address book,
  and tags already used on messages, not only the favorite ones.
- Count every virtual folder in the background from the local index
  (M05.04), so the counts stay current without choosing each one.
- Ask before leaving an unsaved edit, list each account's folders in the
  background to report missing folders early, and offer a new name when a
  moved search's name is taken.
- Keep only results whose `List-Id` equals the searched ID once their headers
  load, and list the lists of every folder from the local index (M05.04).
- Offer one-click unsubscribing where the list supports RFC 8058, after the
  same confirmation.
- Offer a setting that continues every indexed search on the server
  automatically, and answer searches of the shown folder from the index first
  as well ([#343](https://github.com/mikepadiernos/plasma-impresario/issues/343)).
- Search the shown folder alone when offline, and refresh the index right
  after a sync rather than after a pause.
- Show the index of every account in one place, offer to load the missing
  headers of a **Partly indexed** folder, and show the index state in the
  folder tree.
- Fold the case of non-ASCII letters in short terms, index the text of
  attached messages, and fetch every text part on request.
- Offer **Clear Offline Cache** to remove an account's cached mail and index
  together and shrink the file, encrypt the index with a key kept in KWallet,
  and decrypt inline PGP for the index when the option is on
  ([#344](https://github.com/mikepadiernos/plasma-impresario/issues/344)).
- Read attachments in a separate process with a time limit, as Plasma's file
  search does, and offer **Download Attachments for Offline Search...** next
  to the text download, asking first and respecting the limit
  ([#345](https://github.com/mikepadiernos/plasma-impresario/issues/345)).
- Show a short snippet in the column layout's tooltip, keep rows without a
  snippet at their usual height, score body matches from the index, and put
  the best matches first whatever the order
  ([#346](https://github.com/mikepadiernos/plasma-impresario/issues/346)).
- Keep facets with saved searches, offer them in the search builder as query
  terms, and count them over the whole result on the server where it can
  return them
  ([#347](https://github.com/mikepadiernos/plasma-impresario/issues/347)).
- Offer the reader's actions for a result opened from the Search Results
  panel, open a result of another account without switching accounts, give
  the panel its own filters and facets, and search on a separate connection
  so that the shown folder keeps updating
  ([#348](https://github.com/mikepadiernos/plasma-impresario/issues/348)).
- Move the quick search into the designed header with M14.02, and offer
  **Find in Message** in the reader's action bar and from the embedded panels
  ([#349](https://github.com/mikepadiernos/plasma-impresario/issues/349)).
- Ask File Search over D-Bus for its indexing progress and whether a folder
  is indexed yet, check its exclude filters, remember several save folders,
  search a folder in the app when the file manager has no live search, and
  keep the app's cache out of File Search
  ([#350](https://github.com/mikepadiernos/plasma-impresario/issues/350)).
- Open the part named by a `mid:` URL's Content-ID, offer a list of the copies
  found, search the folders most likely to hold the message first, keep the
  question until it is answered, and report a stale index entry
  ([#351](https://github.com/mikepadiernos/plasma-impresario/issues/351)).
- Use the Plasma Opus query parser if it is published as a library, accept
  `size:` and `=` as a size range, share saved searches with Plasma Opus in a
  common format, and confirm coverage from File Search's index rather than its
  settings
  ([#352](https://github.com/mikepadiernos/plasma-impresario/issues/352)).

### Desktop integration

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                                                                                                         |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [#457](https://github.com/mikepadiernos/plasma-impresario/issues/457) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/457#issuecomment-7208324176) | **Run in the background** and **Start after login** in **Settings › General**, closing the main window, the tray icon's menu and the `--background` option ([#371](https://github.com/mikepadiernos/plasma-impresario/issues/371)) |

**Current behavior:**

- ![Resolved][resolved] **Settings › General** has **Run in the background**
  and **Start after login**, both off by default. Starting after login needs
  running in the background, and turning that off turns it off as well. With
  running in the background on, closing the window hides it while mail is
  still checked, and a notice says to open it again from the tray icon, or
  from the application launcher when the tray icon is off. Starting the
  application again shows the running window instead of a second copy. The
  tray icon's menu has **Show Plasma Impresario** and **Quit**
  ([#371](https://github.com/mikepadiernos/plasma-impresario/issues/371)).
- ![Resolved][resolved] Outside a sandbox, **Start after login** writes
  `io.github.mikepadiernos.plasma_impresario.desktop` to the user's autostart
  folder, starting with `--background` and the current profile, and turning it
  off removes only an entry the application wrote. Inside a Flatpak or Snap
  sandbox it asks the XDG Background portal instead. An entry that cannot be
  written or removed, or a portal that cannot be reached, is explained and the
  earlier choice is kept.
- ![Limitation][limitation] Closing the window now quits unless **Run in the
  background** is on, even while the tray icon is shown; before, a visible tray
  icon kept the application running.
- ![Limitation][limitation] The close notice reuses the earlier "don't show
  again" choice, so someone who dismissed the old tray notice is not told that
  the application keeps running.
- ![Limitation][limitation] With the tray icon off, nothing on screen shows
  that a closed window is still running.
- ![Limitation][limitation] Both options apply to every profile. The autostart
  entry starts the profile it was turned on from, and turning it on from
  another profile replaces it.
- ![Limitation][limitation] In a sandbox the choice is saved once the portal
  request is sent; a refusal arrives later, shows a warning and leaves **Start
  after login** checked. The development image has no portal, so this path is
  covered by tests of the request options only.
- ![Limitation][limitation] Starting after login, a hidden start with
  `--background` and the tray icon of a hidden start have not been checked in a
  real Plasma session; that is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- To keep closing to the tray, turn on **Run in the background**.
- If the close notice was dismissed before and running in the background is
  on, a closed window is still running: open it from the tray icon or the
  launcher, or quit from the tray icon's menu.
- To start another profile after login, turn **Start after login** off and on
  again from that profile, or edit the `Exec` line of the autostart entry.
- In a sandbox, if the portal refuses, allow background running for the
  application in **System Settings › Applications**, then turn **Start after
  login** off and on again.

**Proposed improvements:**

- Show the new close notice once, rather than reusing the old choice.
- Store both options per profile and name the profile in the autostart entry's
  file name.
- Show the background state in the tray icon's tooltip and offer **Check Mail
  Now** in its menu.
- Save the sandboxed choice only after the portal accepts it, and read the
  portal's answer at startup.

### Local data and opened attachments

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                                                                                                                                 |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [#458](https://github.com/mikepadiernos/plasma-impresario/issues/458) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/458#issuecomment-7208481426) | The offline cache of each account, automatic drafts and connection logs in the cache, attachments opened from the reader or the Attachments panel, and names suggested when saving ([#375](https://github.com/mikepadiernos/plasma-impresario/issues/375)) |

**Current behavior:**

- ![Resolved][resolved] Cache folders are created readable only by the user
  (0700). At each start the cache is tightened in the background, folders to
  0700 and files to 0600, read-only files staying read-only; linked files and
  folders are left alone. New message parts are written 0600
  ([#375](https://github.com/mikepadiernos/plasma-impresario/issues/375)).
- ![Resolved][resolved] An attachment opened in another application is copied
  to `Opened Attachments/<random>/<name>` in the profile's cache. The copy is
  created 0600, made read-only once downloaded and never executable, and is
  removed if the download fails. Copies whose newest change is more than a day
  old are removed at startup and every hour, also while running in the
  background.
- ![Resolved][resolved] The name comes from the message without folder parts,
  control characters, bidirectional overrides or leading dots, and is
  shortened to 255 bytes with its extension kept. Names suggested when saving
  attachments and messages are cleaned the same way.
- ![Limitation][limitation] The cache, including the offline search index and
  any decrypted text it holds, is not encrypted. Folder permissions do not
  protect it from a compromised account or an unencrypted disk, and removed
  files are not overwritten.
- ![Limitation][limitation] Files that SQLite creates between starts may be
  readable by others until the next start; their user-only folders keep others
  out meanwhile.
- ![Limitation][limitation] A copy still open after a day is removed while the
  other application uses it, together with anything that application wrote
  beside it, such as lock or backup files. Copies are read-only, so an edited
  attachment has to be saved elsewhere.
- ![Limitation][limitation] Drafts saved with **Save Draft** in the
  application's data folder, and attachments saved with **Save Attachment**,
  are not tightened.

**Workarounds:**

- To keep or edit an attachment, use **Save Attachment** rather than opening it.
- Use full-disk or home-folder encryption when keeping mail offline, or choose
  **Don't use cache** in an account's offline settings.
- Keep saved drafts and attachments in a folder that only you can read.

**Proposed improvements:**

- Create the SQLite cache and index files 0600 from the start.
- Tighten the drafts folder in the data folder, or keep drafts on the server
  ([#450](https://github.com/mikepadiernos/plasma-impresario/issues/450)).
- Remove a copy once the other application no longer has it open, and offer a
  setting for how long copies are kept.
- Offer encryption of the offline cache and index with a key from the wallet.

### Outgoing headers and Bcc

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                                                           |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| [#459](https://github.com/mikepadiernos/plasma-impresario/issues/459) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/459#issuecomment-7208697224) | New messages, replies, forwards, redirected messages, mailto: links, the copy filed in Sent and saved drafts ([#373](https://github.com/mikepadiernos/plasma-impresario/issues/373)) |

**Current behavior:**

- ![Resolved][resolved] Every value written to a header is cleaned first. In
  the subject, organization, names and attachment names, each run of line
  breaks, tabs, C0 and C1 controls, NEL and the Unicode line and paragraph
  separators becomes one space. In addresses, the SMTP envelope, In-Reply-To,
  References and MIME types these characters are removed. A mailto: subject is
  cleaned when the link opens.
- ![Resolved][resolved] The copy handed to the outgoing server never lists Bcc
  or Resent-Bcc; the server still delivers to those recipients.
- ![Resolved][resolved] The copy filed in Sent lists Bcc, and drafts keep each
  recipient with its kind, Bcc included.
- ![Resolved][resolved] Message-IDs are a random UUID at the sender's domain,
  IDN-encoded, and never contain the computer's host name. Without a usable
  domain, such as an IP address literal, `localhost` is used.
- ![Limitation][limitation] When **Send with BURL** or the **IMAP SENDMAIL
  Extension** delivers the copy stored in Sent, that copy omits Bcc, so Sent
  does not show who was blind-copied.
- ![Limitation][limitation] Cleaned input is not pointed out; pasted line
  breaks silently become spaces.

**Workarounds:**

- To keep Bcc in the Sent folder, turn off **Send with BURL** and choose SMTP
  or a local sendmail instead of the **IMAP SENDMAIL Extension** in the
  account's outgoing settings.
- Check the subject after pasting multi-line text.
- Use a sender address with a domain name rather than an IP address literal.
- If others can read your Sent folder, send a separate message instead of a
  blind copy.

**Proposed improvements:**

- Show a short note under a field whose pasted text was cleaned.
- With BURL or the IMAP SENDMAIL Extension, record Bcc recipients through a
  private keyword or a local note instead of a header.
- Offer a setting to leave Bcc out of the Sent copy for shared Sent folders.
- Use the domain of the account's outgoing server for Message-IDs when the
  sender address has none.

### Saved files in the file manager

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                                                                 |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| [#460](https://github.com/mikepadiernos/plasma-impresario/issues/460) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/460#issuecomment-7209307644) | **Save Attachment** in the reader and the Attachments panel, saving from the attachment list, and **Save Message** ([#266](https://github.com/mikepadiernos/plasma-impresario/issues/266)) |

**Current behavior:**

- ![Resolved][resolved] After an attachment or message is saved, the main
  window's notice bar says where it went and offers **Show in Folder**. Nothing
  is offered when saving was cancelled or failed
  ([#266](https://github.com/mikepadiernos/plasma-impresario/issues/266)).
- ![Resolved][resolved] **Show in Folder** asks the desktop's file manager
  through `org.freedesktop.FileManager1` to open the folder with the file
  selected, so any file manager that registers the service works. A file
  moved or deleted since it was saved, and a file manager that cannot be
  reached, are reported instead.
- ![Limitation][limitation] Attachments saved from a message opened in its own
  window offer no **Show in Folder**; that window has no notice bar.
- ![Limitation][limitation] Without the service, KIO opens the folder in the
  default application for folders, which may not select the file.
- ![Limitation][limitation] A later notice in the same bar, such as a download
  error, replaces the offer.
- ![Limitation][limitation] It has not been checked with Dolphin or Plasma Opus
  in a real Plasma session; tests replace the file manager. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- For attachments saved from a separate message window, open the folder
  chosen in the save dialog, or save from the main window.
- If the folder opens without the file selected, install a file manager that
  provides `org.freedesktop.FileManager1`, such as Dolphin.
- If another notice replaced the offer, use **Find Saved Copies…** in the
  Attachments panel.

**Proposed improvements:**

- Give separate message windows their own notice bar, so saves and download
  errors there are reported too.
- Keep the offer available after another notice replaces it, for example in a
  list of recent saves.

### Files handed over by file managers

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                                                                                                           |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| [#461](https://github.com/mikepadiernos/plasma-impresario/issues/461) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/461#issuecomment-7209609909) | `mailto:` URLs with `attach=` or `attachment=` items from another application, `plasma-impresario --attach <file>` and the `composeMailWithFiles` D-Bus call ([#263](https://github.com/mikepadiernos/plasma-impresario/issues/263)) |

**Current behavior:**

- ![Resolved][resolved] Files that another application hands over, such as a
  file manager through KDE's e-mail launcher or the XDG Email portal, a second
  start of the application, or `--attach`, are attached to a new unsent
  message. Nothing is sent until you send it
  ([#263](https://github.com/mikepadiernos/plasma-impresario/issues/263)).
- ![Resolved][resolved] A notice at the top of the composer names every
  attached file and stays until dismissed. Files that could not be attached
  are listed with the reason instead of being dropped.
- ![Resolved][resolved] A `mailto:` link clicked inside a message attaches
  nothing; the composer says that it asked for files.
- ![Limitation][limitation] A `mailto:` URL may not attach hidden files, files
  in hidden folders or files in `/proc`, `/sys`, `/dev`, `/etc`, `/boot` and
  `/root`, also through links. The list is fixed. Files named with `--attach`
  only have to be readable regular files.
- ![Resolved][resolved] Files on other computers are fetched through KIO
  ([#265](https://github.com/mikepadiernos/plasma-impresario/issues/265)). A `mailto:` URL may only name them
  through sftp, fish, smb, ftp, ftps, webdav, webdavs, http or https on another
  computer; see [fetched files](#files-fetched-from-other-computers).
- ![Limitation][limitation] Drafts do not keep attachments
  ([#450](https://github.com/mikepadiernos/plasma-impresario/issues/450)), so handed-over files must be attached again after
  reopening a draft or recovering an unsent message.
- ![Limitation][limitation] It has not been checked with the XDG Email portal,
  Dolphin or Plasma Opus in a real Plasma session; tests build the URLs the way
  KDE's e-mail launcher does and call the D-Bus entry point directly. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- To send a hidden or system file, attach it with **Attach** in the composer,
  or name it with `--attach`.
- After reopening a draft or recovering an unsent message, attach the files
  again.

**Proposed improvements:**

- Keep attachments with drafts ([#450](https://github.com/mikepadiernos/plasma-impresario/issues/450)).
- Ask before reading files that an unknown application requests.

### Files fetched from other computers

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                                                                                          |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [#462](https://github.com/mikepadiernos/plasma-impresario/issues/462) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/462#issuecomment-7209981171) | Remote URLs dropped on the composer, chosen with **Attach**, named with `--attach` or D-Bus, or requested by a file manager's `mailto:` URL ([#265](https://github.com/mikepadiernos/plasma-impresario/issues/265)) |

**Current behavior:**

- ![Resolved][resolved] Files on other computers can be dropped on the
  attachment list or the message text, chosen with **Attach**, or handed over.
  Each is fetched through KIO, one at a time, into a private copy in the cache
  ([#265](https://github.com/mikepadiernos/plasma-impresario/issues/265)). The source is only read.
- ![Resolved][resolved] A notice names the file and server being fetched,
  shows progress and has **Cancel**. Afterwards it lists what was fetched with
  sizes, what failed with the reason and what was cancelled. Failed and
  cancelled files are removed, and the message cannot be sent until fetching
  ends.
- ![Resolved][resolved] Addresses are shown without user name, password, query
  or fragment. Copies are removed with their attachment or composer, and
  leftovers from a crash at the next start.
- ![Limitation][limitation] Only HTTP is tested, against a loopback server.
  SFTP, SMB and WebDAV are untested because the development image has no such
  KIO workers.
- ![Limitation][limitation] Drafts do not keep attachments
  ([#450](https://github.com/mikepadiernos/plasma-impresario/issues/450)), so fetched files are not recovered
  with a draft.
- ![Limitation][limitation] Files are fetched one at a time, with no size limit
  or warning. Web links (http, https) dropped into the text stay links.
- ![Limitation][limitation] Not checked in a real Plasma session: drags from
  Dolphin or Plasma Opus, browsing remote places from **Attach**, and KIO's
  password prompts. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- If a server fails or asks for a password that never appears, copy the file
  to a local folder in the file manager first, then attach it.
- To attach a web file, drop its link on the attachment list or choose it with
  **Attach**.
- After reopening a draft or recovering an unsent message, attach the files
  again.

**Proposed improvements:**

- Add the KIO SFTP and WebDAV workers to the development image and cover them
  in the live test.
- Keep attachments with drafts ([#450](https://github.com/mikepadiernos/plasma-impresario/issues/450)).
- Warn before fetching a very large file, and fetch several at once.

### Plasma Search mail

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                    |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------- |
| [#463](https://github.com/mikepadiernos/plasma-impresario/issues/463) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/463#issuecomment-7210363971) | Mail offered to Plasma Search (KRunner) from the offline search index ([#354](https://github.com/mikepadiernos/plasma-impresario/issues/354)) |

**Current behavior:**

- ![Resolved][resolved] **Settings › General › Show indexed mail in Plasma
  Search** is off by default. When it is on, the running app answers
  KRunner's `org.kde.krunner1` calls at `/runner`
  ([#354](https://github.com/mikepadiernos/plasma-impresario/issues/354)).
- ![Resolved][resolved] Only accounts with an offline search index are
  searched. Every word of three or more letters must appear in the subject or
  the sender; message text is never searched. At most ten matches are shown,
  newest first, each with the subject, the sender and the date.
- ![Resolved][resolved] Choosing a match shows the window and opens the
  message. A match names the account, folder, UIDVALIDITY and UID; other IDs
  are ignored.
- ![Limitation][limitation] Results appear only while the app is running;
  KRunner does not start it.
- ![Limitation][limitation] Only the default profile answers, because the
  plugin names its D-Bus service.
- ![Limitation][limitation] KRunner lists the plugin as enabled, but it shows
  nothing until the option in the app is on.
- ![Limitation][limitation] Not checked with KRunner itself or in a Plasma
  session: the plugin's metadata under KF6, the second line with the sender and
  date, and whether the window comes to the front on Wayland. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- Turn on **Run in the background** so that the app is there to answer.
- Keep the accounts whose mail should appear in Plasma Search in the default
  profile.
- If the window stays behind after choosing a match, open it from the tray
  icon or the launcher; the message is already selected.

**Proposed improvements:**

- Let KRunner start the app in the background through a D-Bus service file,
  still answering nothing while the option is off.
- Answer for named profiles too.
- Pass KRunner's activation token so that Wayland raises the window.
- Check the plugin with KRunner in a Plasma 6 session, and add KRunner to the
  development image for an end-to-end test.

### Contacts from the address book

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                          |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| [#464](https://github.com/mikepadiernos/plasma-impresario/issues/464) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/464#issuecomment-7210647420) | Names, organizations and photos from the address book in the Contacts panel ([#305](https://github.com/mikepadiernos/plasma-impresario/issues/305)) |

**Current behavior:**

- ![Resolved][resolved] With an address book that can describe contacts, the
  Contacts panel looks up each address in the background. Known contacts show
  the address book's name, organization and stored photo; unknown addresses
  offer **Add to Address Book** next to **Open in Address Book**, in the
  panel's header button and context menu.
- ![Resolved][resolved] Lookups read only the local address book. They are
  cancelled when the message changes and repeated when the address book
  changes. Photos and states have accessible descriptions.
- ![Resolved][resolved] Without an address book, or with one that cannot
  describe contacts, the panel lists the addresses from the headers only.
- ![Limitation][limitation] abook, the default address book, has no
  organization field. Its photos are read only from a file next to the address
  book or a local path, up to 2 MiB.
- ![Limitation][limitation] Akonadi (**KDE Addressbook (Akonadi)**) supplies
  the name, the organization and photos stored in the contact. It opens
  contacts in the KDE contact viewer and adds them through Akonadi's dialog.
  It is built and compiled but has not been run against an Akonadi server.
- ![Limitation][limitation] No photo is fetched from a URL, Gravatar or a
  server.
- ![Limitation][limitation] Not checked in a Plasma session: photo size, theme
  icons and how Akonadi's dialogs are parented on Wayland. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- To see organizations, choose **KDE Addressbook (Akonadi)** under
  **Settings › General › Addressbook**, and keep the organization in the
  contact.
- For abook contacts, store the photo as a file next to the address book and
  name it in the contact's photo field.
- If Akonadi's dialog opens behind the window, bring it forward from the task
  manager.

**Proposed improvements:**

- Test the Akonadi plugin against an isolated Akonadi instance (`akonaditest`
  with the knut resource).
- Check photo size, icons and dialog parenting in a Plasma 6 session.
- Let abook contacts keep an organization in a custom field.

### My Day calendars and follow-up mail

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                   |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------- |
| [#465](https://github.com/mikepadiernos/plasma-impresario/issues/465) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/465#issuecomment-7211205968) | Events, tasks and follow-up mail in the My Day panel ([#304](https://github.com/mikepadiernos/plasma-impresario/issues/304)) |

**Current behavior:**

- ![Resolved][resolved] **View › Embedded Panels › My Day** lists today's
  events, including multi-day events that started in the last 14 days, and
  tasks due today or overdue, from KDE PIM calendars through Akonadi. The
  Akonadi source runs only while the panel is on, and the build option
  `WITH_AKONADI_CALENDAR` leaves it out.
- ![Resolved][resolved] Follow-up mail is flagged mail and mail with the
  `$TODO` keyword, from the shown folder and each account's offline search
  index, newest first, up to 50 messages. Opening one looks for it in the shown
  folder, then in loaded mail, then by Message-ID in its folder; if it is not
  found, a notice offers **Show in Folder**.
- ![Resolved][resolved] **Join** opens an event's online meeting link through
  KIO. **Open in Calendar** starts KOrganizer, or Merkuro Calendar when
  KOrganizer is not installed. Without calendars, the panel says so and lists
  the messages only. The day changes at midnight in the system time zone.
- ![Limitation][limitation] Every calendar Akonadi offers is shown; the
  calendars chosen in KOrganizer or Merkuro are not followed. A multi-day event
  that started more than 14 days ago is left out.
- ![Limitation][limitation] Accounts without an offline search index add only
  the shown folder's messages. Other clients may not set `$TODO`.
- ![Limitation][limitation] **Open in Calendar** does not open the selected
  event or task. **Join** accepts only `https`, `http`, `sip`, `sips` and
  `tel` links.
- ![Limitation][limitation] Opening a message from another folder stops a
  search across folders that is running.
- ![Limitation][limitation] The Akonadi calendar source is built and compiled
  but has not been run against an Akonadi server. Opening messages from the
  panel and the calendar hand-off have no direct automated test, and nothing is
  checked in a Plasma session. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- Unchecking a calendar in KOrganizer or Merkuro does not hide it from My Day;
  only calendars removed from Akonadi are left out. Turn the panel off if
  unwanted calendars crowd it.
- Turn on the offline search index for each account so that follow-up mail from
  every folder is listed.
- Flag messages, rather than relying on keywords set by other clients.
- Finish a search across folders before opening messages from My Day.
- For meeting links in other schemes, open the event in the calendar
  application.

**Proposed improvements:**

- Test the Akonadi calendar source against an isolated Akonadi instance
  (`akonaditest`) with a calendar resource.
- Follow the calendar selection made in KOrganizer or Merkuro.
- Open the selected event or task in the calendar application, and check the
  application names in a Plasma 6 session.
- Add a window-level test for opening follow-up messages and for the calendar
  hand-off.
- Let a follow-up lookup run without stopping a search across folders.

### Server settings discovery

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------- |
| [#466](https://github.com/mikepadiernos/plasma-impresario/issues/466) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/466#issuecomment-7212352050) | Finding IMAP and SMTP settings in **Add Account** ([#372](https://github.com/mikepadiernos/plasma-impresario/issues/372)) |

**Current behavior:**

- ![Resolved][resolved] **Add Account** asks for a name, an email address and
  an optional account name. **Find Settings** tries the providers built into
  the application, the domain's autoconfig file over HTTPS, Thunderbird's ISP
  database (ISPDB), DNS SRV records (RFC 6186 and RFC 8314), the domain's MX
  hosts and common host names, in that order, and stops at the first source
  that names an IMAP server. Nothing signs in and no password is sent.
- ![Resolved][resolved] The dialog shows the provider, the servers, where they
  were found, and guidance for app passwords, turning on IMAP or OAuth-only
  providers. Servers found through unsigned DNS outside the mail domain and the
  known providers must be confirmed before the account can be added. After
  adding, the settings and an identity are saved and the account settings
  open.
- ![Limitation][limitation] The autoconfig request sends the full email address
  to `autoconfig.<domain>`, as Thunderbird does. The ISPDB request sends only
  the domain.
- ![Limitation][limitation] An ISPDB lookup through MX hosts takes the last two
  labels of the MX host as the provider's domain. This fails for suffixes such
  as `co.uk`, which then fall back to guessed host names.
- ![Limitation][limitation] DNS replies count as signed only when the system
  resolver sets the DNSSEC authenticated-data bit. Most resolvers do not
  validate, so most SRV or MX servers outside the mail domain need
  confirmation.
- ![Limitation][limitation] Guessed servers count as found after a TCP or TLS
  connection succeeds, even with a certificate error. The IMAP or SMTP greeting
  and STARTTLS are not checked. Servers without encryption and single-label
  domains such as `localhost` are not used.
- ![Limitation][limitation] Nine providers are built in, and the Apple help
  link is unchecked. Settings can be found only in **Add Account**, and
  cancelling the account settings that open next keeps the account and the
  settings found.
- ![Limitation][limitation] Automated tests use a fake network, apart from
  probes and fetches against local servers. Nothing is tried against real
  providers, a validating resolver or in a Plasma session. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- Check the servers shown before adding the account. If they are wrong or
  missing, add the account anyway and enter the servers in its settings.
- Confirm servers outside your mail domain only if you recognise them as your
  provider's.
- If a guessed server is wrong, or its certificate is rejected when signing in,
  enter the server named in your provider's help pages.
- To look up settings again for an existing account, add a new account with the
  same address and remove the old one.
- If your domain's autoconfig server should not see your address, enter the
  settings by hand instead of using **Find Settings**.

**Proposed improvements:**

- Add a live test with Dovecot and a locally served autoconfig file, and check
  real providers in a Plasma 6 session.
- Use the public suffix list to find the registered domain of MX hosts.
- Check the IMAP and SMTP greetings, and STARTTLS, when probing guessed
  servers, and report certificate errors.
- Offer **Find Settings** in the settings of existing accounts.
- Check the Apple help link and extend the built-in providers.

### Window activation for requests

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                                                        |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [#467](https://github.com/mikepadiernos/plasma-impresario/issues/467) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/467#issuecomment-7212676193) | Bringing windows forward for requests from launchers, links, file managers, Plasma Search and notifications ([#98](https://github.com/mikepadiernos/plasma-impresario/issues/98)) |

**Current behavior:**

- ![Resolved][resolved] A second launch of the same profile hands its
  activation token to the running instance over D-Bus before its request.
  Plasma Search hands its token over before `Run`, and a notification's
  **Open Account** passes the notification's token.
- ![Resolved][resolved] The window a request is about comes forward: the main
  window, or the composer for a `mailto:` link, a file manager's request to
  send files, or **New Message**. A token is used once and dropped after 20
  seconds. Windows are never pinned to a virtual desktop, Activity or screen,
  and never moved.
- ![Limitation][limitation] On a first launch, Qt applies the launch token to
  the first window it activates. When a `mailto:` link starts the application,
  that is probably the main window, so the composer may only be marked as
  wanting attention.
- ![Limitation][limitation] The address book window, opened with
  `--addressbook`, takes no token. The tray icon relies on Qt to pass on the
  token of its click, which is unchecked.
- ![Limitation][limitation] On X11, the startup notification is not explicitly
  ended, so the launch feedback may stay until it times out.
- ![Limitation][limitation] Plasma Search hands its token only to the default
  profile ([Plasma Search mail](#plasma-search-mail)). A token unused for 20
  seconds, for example while an account is being set up, is dropped.
- ![Limitation][limitation] Automated tests run on Qt's offscreen platform,
  where no token is applied, and do not cover the session-bus hand-over or the
  notification token. Nothing is tried in a Plasma session on Wayland or X11.
  That is [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- If a window is only marked as wanting attention, click its task manager
  entry, or lower KWin's focus stealing prevention level in the window behavior
  settings.
- To open the composer from a `mailto:` link without focus problems, start the
  application first.

**Proposed improvements:**

- Check every request path in a Plasma 6 session on Wayland and X11, from a
  hidden, minimized, covered and other-desktop window.
- On a first launch with a `mailto:` link, keep the launch token for the
  composer instead of the main window.
- Pass the token to the address book window, and end the X11 startup
  notification explicitly.
- Add a session-bus test for the second-launch hand-over and a test for the
  notification token.

### Saving unsent messages at session end

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                 |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------- |
| [#468](https://github.com/mikepadiernos/plasma-impresario/issues/468) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/468#issuecomment-7213042207) | Saving open composers when the Plasma session ends ([#100](https://github.com/mikepadiernos/plasma-impresario/issues/100)) |

**Current behavior:**

- ![Resolved][resolved] Composers save changes every 30 seconds. When the
  session ends, through the X11 session manager's request to save or a
  `SIGTERM`, each open composer also saves the changes made since then, without
  asking, and the application quits.
- ![Resolved][resolved] Messages that were sent or never edited are not saved.
  The next start reopens the saved messages from the cache, as after a crash.
- ![Limitation][limitation] Attachments are not saved with drafts, so they are
  missing from a recovered message.
- ![Limitation][limitation] A message opened from a saved draft is written back
  to that draft, which does not reopen at the next start; it opens through
  **Edit Draft…**.
- ![Limitation][limitation] A message still being sent when the session ends is
  saved too, so it can reopen even if the server accepted it.
- ![Limitation][limitation] On Wayland, if Plasma closes the windows before it
  ends the process, a composer with changes asks **Save Draft?** and can hold up
  logging out.
- ![Limitation][limitation] Only composers come back; Plasma's session restore
  is not used. The test saves directly and exercises neither the session
  manager's request nor `SIGTERM`, and nothing is tried in a Plasma session.
  That is [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- Send or save messages with attachments before logging out, and attach the
  files again after a recovery.
- If **Save Draft?** holds up logging out on Wayland, choose **Save** or
  **Discard** so that the window can close.
- Open a message that was opened from a saved draft through **Edit Draft…**
  after the next start.

**Proposed improvements:**

- Log out of Plasma 6 on Wayland and X11 with an edited composer, and check
  that the message reopens and that no prompt holds up logging out.
- Save without asking when Plasma closes the windows to end the session.
- Keep the attached files' locations in drafts, or warn when a recovered message
  had attachments.
- Do not save a message whose sending is under way, or mark it as possibly
  sent.

### Portal file chooser

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                         |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| [#470](https://github.com/mikepadiernos/plasma-impresario/issues/470) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/470#issuecomment-7213372472) | Choosing files to attach, drafts to edit and folders for saved-file search ([#101](https://github.com/mikepadiernos/plasma-impresario/issues/101)) |

**Current behavior:**

- ![Resolved][resolved] Attaching files, **Edit Draft…** and choosing a folder
  for **Find Saved Files…** ask the XDG desktop portal's file chooser first,
  with the app window as its parent, so whichever program serves the portal,
  such as Plasma or Plasma Opus, shows its dialog. Filters and the starting
  folder are passed on.
- ![Resolved][resolved] When no portal answers, or the portal says it cannot
  show the dialog, the KDE dialog appears, as before. Cancelling either dialog
  changes nothing.
- ![Limitation][limitation] Only the window that asked is disabled while the
  portal's dialog is open; the app's other windows stay usable.
- ![Limitation][limitation] Saving attachments and messages still uses the KDE
  dialog directly.
- ![Limitation][limitation] Plasma's portal returns local paths, so remote
  locations chosen in it depend on kio-fuse. Remote URIs a portal returns are
  fetched through KIO for attachments; **Edit Draft…** and the saved-file
  search folder take local paths only.
- ![Limitation][limitation] On Wayland, handing the window to the portal needs
  KDE Frameworks 6.28 or later. The tests use a fake portal on a private D-Bus
  connection; nothing is tried in a Plasma session or with Plasma Opus. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- If the portal's dialog does not appear over the app window on Wayland, find
  it in the task manager; the app window stays disabled until a file is chosen
  or the dialog is cancelled.
- To attach a file from a remote location the portal cannot show, drop it on
  the composer, or install kio-fuse so that the portal offers it as a local
  path.

**Proposed improvements:**

- Open files in a Plasma 6 session on Wayland and X11, with and without Plasma
  Opus serving the portal, and check the parent window, filters, starting
  folder, cancelling and multiple selection.
- Ask the portal's `SaveFile` for save dialogs as well.
- Disable every app window while the portal's dialog is open, as a modal
  dialog would.

### Unread count on the task manager

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                         |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------ |
| [#471](https://github.com/mikepadiernos/plasma-impresario/issues/471) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/471#issuecomment-7213633218) | The unread count on the task manager entry ([#102](https://github.com/mikepadiernos/plasma-impresario/issues/102)) |

**Current behavior:**

- ![Resolved][resolved] The task manager entry shows the unread messages in the
  INBOX of every account, through the `com.canonical.Unity.LauncherEntry`
  signal. Only the number is sent, and only when it changes; nothing unread
  hides it, and the first count clears what an earlier run left.
- ![Resolved][resolved] It is on by default and can be turned off under
  **Settings → General → Show the unread count on the task manager entry**,
  which hides the count.
- ![Limitation][limitation] The tray icon counts the watched folders of the
  current account, so its number can differ.
- ![Limitation][limitation] Accounts that are offline or not yet connected add
  nothing until their count is known, so the number can grow after start.
- ![Limitation][limitation] Plasma's task manager matches the signal to an
  installed desktop entry, so a build run without installing shows no count.
- ![Limitation][limitation] The tests receive the signal over a private D-Bus
  connection and check a live total with two Dovecot accounts; nothing is tried
  in a Plasma session. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- To hide the count, turn off the setting, or turn off **Show progress and
  status information in task buttons** in the Plasma task manager's settings.
- For the count of a single account, see the account selector or the sidebar
  header.

**Proposed improvements:**

- Check the count in Plasma 6 on Wayland and X11 with several accounts,
  offline and after quitting.
- Count the same folders for the tray icon and the task manager entry, with one
  setting for both.
- Let the count include other watched folders or only selected accounts.

### Read state and unread counts

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                   |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------- |
| [#505](https://github.com/mikepadiernos/plasma-impresario/issues/505) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/505#issuecomment-7232160785) | Unread counts across accounts and concurrent changes ([#130](https://github.com/mikepadiernos/plasma-impresario/issues/130)) |

**Current behavior:**

- ![Resolved][resolved] For each account, the folder counter, the message
  list, the Unified Inbox, the sidebar header and the task manager entry agree.
  Changes on one account's server leave the other accounts' counts alone.
- ![Resolved][resolved] A change from another client that arrives while the
  user's own change is on its way is counted once. Removed and newly arrived
  messages update the count of their account only, and an account that loses
  its connection keeps its last count.
- ![Limitation][limitation] Read and flag changes show only once the server
  confirms them, so on a slow connection the message and the counts change
  late.
- ![Limitation][limitation] A change cut off by a lost connection is not
  replayed after reconnecting; it stays in the activity log with **Retry**.
  Read and flag actions are disabled while offline.
- ![Limitation][limitation] These cases are tested with two fake IMAP servers,
  not with several real servers at once.

**Workarounds:**

- On a slow server, wait for the message to change; the counts catch up once
  the server answers.
- After a dropped connection, use **Retry** in the activity log.

**Proposed improvements:**

- Show pending changes at once with a pending marker, and roll them back if the
  server refuses them.
- Queue changes made offline or cut off by a lost connection and replay them
  after reconnecting, alongside the persistent outbox
  ([#378](https://github.com/mikepadiernos/plasma-impresario/issues/378)).
- Check the counts with several accounts on the disposable Dovecot server,
  changing flags from a second client.

### Performance and resource use

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                  |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------- |
| [#506](https://github.com/mikepadiernos/plasma-impresario/issues/506) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/506#issuecomment-7232463278) | Responsiveness and resource use for large mailboxes ([#131](https://github.com/mikepadiernos/plasma-impresario/issues/131)) |

**Current behavior:**

- ![Resolved][resolved] Syncing a large folder, long conversations, bulk read
  and flag changes, and messages with many attachments grow about linearly
  with their size and stay well inside their budgets; the measurements are in
  [performance and resource use](performance.md).
- ![Limitation][limitation] Removing many messages slows down with the square
  of the folder size, because each removed message renumbers every later one.
  Reported one `EXPUNGE` at a time, as servers without QRESYNC do, the window
  keeps responding, but removing 40,000 of 200,000 messages takes about 14 s.
- ![Limitation][limitation] Reported as one `VANISHED` range, as QRESYNC
  servers do, the same removal is applied in a single pass and the window stops
  responding for its whole duration: about 0.5 s for 10,000 of 50,000 messages
  and 8 s for 40,000 of 200,000. Against a local Dovecot server, another client
  removing 10,000 of 50,000 messages takes about 6 s in the window, which stops
  responding for up to about 0.5 s at a time.
- ![Limitation][limitation] The Unread quick filter re-filters the whole list
  in one pass, which blocks the window for about 1.2 s at 200,000 messages.
- ![Limitation][limitation] Each synced message costs about 0.9 KiB before its
  headers load, and an opened attachment is held in memory at about 2.3 times
  its decoded size while it is open.
- ![Resolved][resolved] Against a local Dovecot server with 50,000 messages,
  opening the folder takes about 0.6 s and the Unread quick filter about 0.1 s,
  down from about 56 s and 16 s before the window stopped updating its actions
  and scroll position for every changed message
  ([live server](performance.md#live-server)).
- ![Limitation][limitation] The measurements use fake IMAP servers and a local
  Dovecot server in a Linux container, in a Debug configuration compiled with
  `-O2`; network latency, a Release build and a Plasma session are not
  measured.

**Workarounds:**

- On folders of 100,000 messages or more, archive older mail into yearly
  folders, and search instead of using quick filters.
- Delete or move large batches of messages in smaller selections, so that the
  server reports smaller removals.
- Save large attachments instead of keeping them open in the reader.

**Proposed improvements:**

- Apply removals in batches: one model removal per run of adjacent messages and
  one renumbering pass per response, for both `VANISHED` ranges and
  consecutive `EXPUNGE` responses.
- Filter large lists incrementally, in chunks between event-loop passes.
- Stream large attachments to a temporary file instead of keeping them in
  memory.
- Repeat the measurements in a Release build, with network latency, and in a
  Plasma session with the GUI painting.

### Sanitizers and data races

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                         |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------ |
| [#507](https://github.com/mikepadiernos/plasma-impresario/issues/507) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/507#issuecomment-7234348639) | Sanitizer runs of the suite and live tests ([#481](https://github.com/mikepadiernos/plasma-impresario/issues/481)) |

**Current behavior:**

- ![Resolved][resolved] The test suite and every live Dovecot and POP3 test
  pass under AddressSanitizer, LeakSanitizer and UndefinedBehaviorSanitizer
  with no report. The findings, all fixed, and the third-party suppressions
  with their reasons are in [sanitizer runs](sanitizers.md).
- ![Limitation][limitation] ThreadSanitizer has not been run. It needs Qt to
  be built with it, and the dev container uses Fedora's Qt and has no
  `libtsan`. Data races in work on other threads, GpgME verification and
  decryption and the startup cache maintenance, are not checked.

**Workarounds:**

- Rely on the ASan and UBSan runs, which catch a cross-thread mistake when it
  ends in a use after free or a leak.
- Keep work handed to threads self-contained, so that it shares no data with
  the GUI thread while it runs.

**Proposed improvements:**

- Build Qt 6 with ThreadSanitizer in a separate container image, add
  `libtsan`, and run `tools/dev-container/sanitizers.sh` with
  `PLASMA_SANITIZERS=thread`, starting with the crypto and cache code.

### Accessibility and theming

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                           |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------- |
| [#472](https://github.com/mikepadiernos/plasma-impresario/issues/472) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/472#issuecomment-7214151837) | Accessibility and theming parity with Plasma ([#103](https://github.com/mikepadiernos/plasma-impresario/issues/103)) |

**Current behavior:**

- ![Resolved][resolved] Icons come from the active icon theme, which always
  wins over the few bundled copies; small action icons prefer the theme's
  `-symbolic` variants.
- ![Resolved][resolved] Field and text states, such as an invalid address, a
  search with or without results, a sorting error, warnings and the offline
  tray badge, use the color scheme's positive, neutral and negative roles.
  Avatar initials, selected tag text and the protocol log keep at least 4.5:1
  contrast.
- ![Resolved][resolved] The address expander is a focusable button that opens
  with Return or Enter and shows a focus rectangle, and address links take the
  keyboard. The message list, quick search, folder tree, network access and
  main menu controls have accessible names, and a test checks that every
  visible focusable control in the main window has one.
- ![Limitation][limitation] Every source file is scanned for translatable
  strings, but no `.po` catalog exists for the `plasma-impresario` domain, so
  the interface is in English only.
- ![Limitation][limitation] Small icons built from sources other than the
  theme are not converted to symbolic ones; there is no `SymbolicIcon::wrap`.
- ![Limitation][limitation] The protocol log inserts each entry's source label
  without HTML escaping; the labels are internal names today.
- ![Resolved][resolved] Offscreen tests check that the wide and compact
  layouts keep every pane readable at 800x600, 1366x768 and 2560x1440, with 10
  and 16 pt text and at 200% scale, and that a high-contrast scheme can suspend
  appearance effects
  ([M09.08](ROADMAP.md#m09-themes-and-branding-foundation)).
- ![Limitation][limitation] The tests use Qt's accessibility interfaces
  offscreen. Nothing is tried with a screen reader such as Orca, with Breeze
  High Contrast, in right-to-left languages or in a Plasma session. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- For more contrast, choose a high-contrast Plasma color scheme; the state
  colors follow it.
- To reach an address or a collapsed address list without a mouse, Tab through
  the message header.

**Proposed improvements:**

- Check the shell with Orca, keyboard only, at 200% scaling, in right-to-left
  languages and with Breeze High Contrast in a Plasma 6 session.
- Add `SymbolicIcon::wrap` for icons built from other sources, and escape the
  protocol log's source label.
- Start translations for the `plasma-impresario` domain once the strings
  settle.

### Provider setup and Gmail

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                       |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| [#473](https://github.com/mikepadiernos/plasma-impresario/issues/473) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/473#issuecomment-7214308273) | The [Mail providers](integrations/providers.md) guide ([#104](https://github.com/mikepadiernos/plasma-impresario/issues/104)) and Gmail accounts |

**Current behavior:**

- ![Resolved][resolved] [Mail providers](integrations/providers.md) lists the
  servers, sign-in and first steps of the nine built-in providers, OAuth and
  app passwords, the IMAP extensions the client uses, sending options and
  troubleshooting.
- ![Resolved][resolved] On Gmail, with **Archive Folder Name** empty,
  **Archive** moves messages to the `\All` folder, **All Mail**, instead of
  creating an `Archive` label
  ([#43](https://github.com/mikepadiernos/plasma-impresario/issues/43)).
- ![Resolved][resolved] Messages sent through `smtp.gmail.com` or
  `smtp.googlemail.com` are not uploaded to **Sent Mail**, because Gmail keeps
  its own copy; **Save Outgoing Mail** is shown off and disabled for them.
- ![Limitation][limitation] An archive folder name stored by an earlier
  version, usually `Archive`, still wins over **All Mail**.
- ![Limitation][limitation] Gmail labels still appear as folders. A message
  with several labels appears in each, and removing it from one removes only
  that label. The open message shows its other labels as chips, but they are
  read-only and are fetched only when the message is opened online.
- ![Limitation][limitation] Only Google and Microsoft offer OAuth; Yahoo,
  iCloud and AOL need app passwords. POP3 and JMAP accounts have their own
  limits: see [POP3 accounts](#pop3-accounts) and
  [JMAP accounts](#jmap-accounts).
- ![Limitation][limitation] Nothing in the guide is tried with real provider
  accounts; the tests use a fake network and local Dovecot servers. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- For Gmail, clear **Archive Folder Name** under **Account Settings › IMAP**
  if it still holds `Archive`.
- To change a message's labels, copy it to or delete it from the label
  folders.
- For Yahoo, iCloud and AOL, create an app password.
- For providers that offer POP3 besides IMAP, use IMAP. For JMAP, see
  [JMAP accounts](#jmap-accounts).

**Proposed improvements:**

- Edit labels from the chips with `STORE X-GM-LABELS`, and optionally hide
  label folders from the folder tree
  ([#43](https://github.com/mikepadiernos/plasma-impresario/issues/43)).
- Check each built-in provider with a real account in a Plasma 6 session.

### Junk and attachment safety

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                                                                                                 |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [#474](https://github.com/mikepadiernos/plasma-impresario/issues/474) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/474#issuecomment-7214741902) | Junk verdicts and attachment opening ([#105](https://github.com/mikepadiernos/plasma-impresario/issues/105)), and the ClamAV and spam-handling hooks ([#44](https://github.com/mikepadiernos/plasma-impresario/issues/44)) |

**Current behavior:**

- ![Resolved][resolved] A message with a `$Junk` or `Junk` keyword shows a
  warning banner above the reader. Its **Not Junk** button removes `$Junk` and
  adds `$NotJunk` on the server. A `$NotJunk`, `NotJunk` or `NonJunk` keyword
  always wins over a junk mark.
- ![Resolved][resolved] Junk rows show the `mail-mark-junk` icon, and screen
  readers hear "Marked as junk".
- ![Resolved][resolved] Opening an attachment of junk mail from the reader or
  the attachments panel asks first. **Cancel** is the default, and nothing is
  downloaded until the user agrees.
- ![Resolved][resolved] Scan results have fixed meanings: an infected
  attachment is blocked, and a failed or unavailable scan asks first and is
  never shown as clean.
- ![Limitation][limitation] The verdict comes only from IMAP keywords. Spam
  headers such as `X-Spam-Status` are not read, so mail that the server files
  into the Junk folder without a keyword is not marked.
- ![Resolved][resolved] With **Scan attachments with ClamAV before opening
  them** turned on under **Settings › General**, **Open** streams the
  downloaded copy to the local `clamd` before another application receives
  it. A threat blocks opening and names the signature. A `clamd` that cannot
  be reached, an error reply such as its stream size limit, or no answer
  within a minute asks first. Scanning is off by default.
- ![Resolved][resolved] With **Move messages marked as junk to the Junk
  folder** turned on, **Mark as Junk** also moves the message to the
  account's Junk folder, and **Mark as Not Junk** on a message in the Junk
  folder moves it to the inbox, so a spam filter on the server can learn from
  the move. Moving is off by default.
- ![Limitation][limitation] Only a `clamd` Unix socket is supported. It is
  found at the usual distribution paths, or set as `ClamdSocket` in the
  `[MailSafety]` group of `plasma-impresariorc`; there is no setting for it in
  the dialog. A `clamd` over TCP and `clamscan` are not used.
- ![Limitation][limitation] The scan was checked against a real `clamd` with a
  test signature only, not with ClamAV's own signatures or in a Plasma
  session. A busy cursor is the only sign of a running scan.
- ![Limitation][limitation] Only **Open** asks first and scans. Saving or
  dragging an attachment does neither, and there is no separate **Scan**
  action.
- ![Limitation][limitation] The reader banner's **Not Junk** button changes
  only keywords. Mail selected from several folders, accounts without a Junk
  folder, and **Not Junk** outside the Junk folder are not moved.
- ![Limitation][limitation] With column rows, the junk mark replaces the
  replied and forwarded icon in the subject column. Closing the reader's junk
  banner hides it until another message is shown.

**Workarounds:**

- Let the server's spam filter set `$Junk`, for example Dovecot with Rspamd
  or SpamAssassin and a Sieve `addflag`, so its verdict reaches the reader.
- To move mail as well, use **Mark as Not Junk** from the toolbar or menu
  instead of the reader banner, or use **Move to**.
- Scan saved attachments with `clamscan` before opening them.
- For a `clamd` that listens only on TCP, add a `LocalSocket` to its
  `clamd.conf`, or set `ClamdSocket` to the socket it uses.

**Proposed improvements:**

- Read the server's spam headers when they are cached, and treat mail in the
  Junk folder as junk.
- Ask and scan before saving or dragging attachments as well, and add a
  **Scan** action to the attachment menu and toolbar.
- Let the reader banner's **Not Junk** move the message as well, with
  **Undo**.
- Show the scan's progress, and offer the socket in the settings dialog.
- Check the scan with ClamAV's own signatures in a Plasma session
  ([M15](ROADMAP.md#m15-real-plasma-session-verification)).

### External services and outbound calls

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                     |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------ |
| [#475](https://github.com/mikepadiernos/plasma-impresario/issues/475) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/475#issuecomment-7214973305) | [External services](integrations/external-services.md) ([#106](https://github.com/mikepadiernos/plasma-impresario/issues/106)) |

**Current behavior:**

- ![Resolved][resolved] Message content goes only to the account's own IMAP
  and SMTP servers, or the configured `sendmail` program. Every other outbound
  call is listed in the [register](integrations/external-services.md#register)
  with what it sends and the user action that comes first.
- ![Resolved][resolved] `test_ExternalExposure` fails when a source file sends
  something out without a register entry, or an entry no longer does.
- ![Limitation][limitation] The check works per file and by known calls. A new
  outbound call in a file that is already listed, such as
  `src/Gui/Window.cpp`, passes without a register change, and calls made by
  other means, such as QML `XMLHttpRequest` or sockets opened by a library,
  are not found.
- ![Limitation][limitation] Libraries follow their own settings. With
  `auto-key-retrieve` in `gpg.conf`, verifying a signature can fetch the
  signer's key from a key server.
- ![Limitation][limitation] The IMAP `ID` command and the `User-Agent` header
  include the version and operating system by default.
- ![Limitation][limitation] A remote image the user allows still tells its
  server the time and the reader's network address.
- ![Limitation][limitation] What notifications, Plasma Search and the portals
  show has not been checked in a real Plasma session.

**Workarounds:**

- Turn off **Reveal Plasma Impresario's version and platform information** in
  the **General** settings to send only the application name.
- Leave `auto-key-retrieve` out of `~/.gnupg/gpg.conf`.
- Keep **Load Remote Content** for senders you trust.
- When reviewing a change to a file that is already in the register, check its
  new outbound calls by hand.

**Proposed improvements:**

- Record outbound calls by function rather than by file, so that a new call in
  a listed file needs a register change too.
- Add a runtime test that runs the application's network uses against a local
  server with a marker in a message, and fails if the marker appears in any
  request.
- Check notifications, Plasma Search and the portals in a real Plasma session
  ([M15](ROADMAP.md#m15-real-plasma-session-verification)).

### System sleep and account notifications

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                                                                                                            |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [#476](https://github.com/mikepadiernos/plasma-impresario/issues/476) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/476#issuecomment-7215881903) | [Windows and session lifecycle](integrations/kde-plasma.md#windows-and-session-lifecycle), [notifications](integrations/kde-plasma.md#notifications-and-status) ([#42](https://github.com/mikepadiernos/plasma-impresario/issues/42)) |

**Current behavior:**

- ![Resolved][resolved] When logind announces sleep with `PrepareForSleep`,
  every account goes offline and closes its connection. On wake, each account
  returns to the network policy the user chose; an account set to **Offline**
  stays offline, and the chosen policy is never overwritten.
- ![Resolved][resolved] A background account with a connection problem has at
  most one desktop notification. A later failure replaces it, and it closes
  when the account connects again, is opened or is removed; **Open Account**
  does nothing for a removed account.
- ![Limitation][limitation] No logind sleep-delay inhibitor is taken, so the
  IMAP `LOGOUT` may not reach the server before the system sleeps; the server
  then drops the session on its own time-out.
- ![Limitation][limitation] Without logind or a system bus, as on systems
  without systemd, sleep is not noticed; connections recover through time-outs
  and the normal reconnect.
- ![Limitation][limitation] Account notifications close when the application
  quits.
- ![Limitation][limitation] KDE Online Accounts (KAccounts) is not used;
  accounts are not shared with the system or other applications.
- ![Limitation][limitation] New-mail notifications are not part of this item;
  they come with
  [M07](ROADMAP.md#m07-output-notifications-and-rules-automation).
- ![Limitation][limitation] Sleep, wake and notifications have not been
  checked in a real Plasma session.

**Workarounds:**

- A stale server session after sleep is dropped by the server's time-out;
  nothing needs to be done in the application.
- On systems without logind, choose **Offline** in the network menu before
  suspending and **Free Access** or **Expensive Connection** afterwards.
- Open the account from its notification, or switch to it, to dismiss the
  notice.

**Proposed improvements:**

- Take a logind `delay` inhibitor while connections log out, and release it
  once they close or after a short limit.
- Follow `org.freedesktop.login1` appearing and disappearing on the bus, so
  that a restarted logind is noticed.
- Offer KDE Online Accounts as an account source.
- Add new-mail notifications
  ([M07](ROADMAP.md#m07-output-notifications-and-rules-automation)).
- Check sleep, wake and notifications in a real Plasma session
  ([M15](ROADMAP.md#m15-real-plasma-session-verification)).

### Plasma widgets

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                          |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------- |
| [#477](https://github.com/mikepadiernos/plasma-impresario/issues/477) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/477#issuecomment-7217492341) | [Plasma widgets](integrations/kde-plasma.md#plasma-widgets) ([#245](https://github.com/mikepadiernos/plasma-impresario/issues/245)) |

**Current behavior:**

- ![Resolved][resolved] The optional **Mail Status** and **Mail Ticker**
  widgets follow the running application over D-Bus and never read mail
  storage or credentials. **Plasma widgets may show** in Settings, General
  decides what they learn: nothing, unread counts and account status (the
  default), or the senders, and also the subjects, of unread mail. Message
  text is never shared.
- ![Resolved][resolved] Both widgets say when the application is not running
  or not answering and never show old data as current; the ticker pages
  instead of scrolling when animations are off or it has the keyboard focus.
- ![Limitation][limitation] Senders and subjects come only from accounts with
  an offline search index and follow its last pass, so a message read
  elsewhere can stay in the ticker until the index catches up.
- ![Limitation][limitation] Unread counts cover each account's INBOX only.
- ![Limitation][limitation] The ticker shows either unread messages or one
  count per account, not both together.
- ![Limitation][limitation] While the application is not running there are no
  data; the status widget keeps the last counts, marked as out of date.
- ![Limitation][limitation] The application is found under its legacy D-Bus
  service name, `net.flaska.trojita`.
- ![Limitation][limitation] The shared views and the D-Bus client are tested
  outside Plasma; the widgets have not been loaded on a panel or the desktop
  of a real Plasma session. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- To see senders or subjects in the ticker, choose a sharing level that allows
  them and turn on the offline search index for the account.
- If a read message stays in the ticker, let the index finish its next pass,
  or open the folder so that it is reindexed.
- Keep the application running, for example with **Run in the background**, so
  that the widgets stay current.

**Proposed improvements:**

- Load and check both widgets in a real Plasma 6 session, on the panel and the
  desktop ([M15](ROADMAP.md#m15-real-plasma-session-verification)).
- Update the index's flags when messages are read in the application, so that
  the ticker follows at once.
- Offer counts for folders other than INBOX.
- Register the application under its own D-Bus service name.
- Let the ticker mix messages from indexed accounts with counts from the
  others.

### POP3 accounts

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                              |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------- |
| [#478](https://github.com/mikepadiernos/plasma-impresario/issues/478) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/478#issuecomment-7219265960) | [POP3 accounts](integrations/providers.md#pop3) ([#216](https://github.com/mikepadiernos/plasma-impresario/issues/216)) |

**Current behavior:**

- ![Resolved][resolved] **Add Account** can create a POP3 account. It
  downloads mail over TLS or STLS into a store on this computer, signing in
  with AUTH PLAIN when offered or USER and PASS, and never sends the password
  when the chosen protection is not available.
- ![Resolved][resolved] **Account Settings › POP3** chooses whether mail stays
  on the server, is removed once downloaded, or is removed after a number of
  days, and how often to check. Each message is stored before the server is
  asked to remove it, and a broken connection removes nothing. A message is
  never downloaded twice: it is known by its UIDL, or by a digest of its
  headers on servers without UIDL.
- ![Resolved][resolved] Read, flagged and deleted states, and searching in the
  window, work on the local copy.
- ![Limitation][limitation] Mail removed from the server exists only on this
  computer. Removing the account deletes it with the account's other local
  data, and nothing backs it up.
- ![Limitation][limitation] The window shows a single inbox: folder
  management, moving, archive, junk and tags are hidden, although the local
  store could hold folders.
- ![Limitation][limitation] There is no offline search index for POP3
  accounts, so Plasma Search and the Plasma widgets' senders and subjects do
  not cover them.
- ![Limitation][limitation] SMTP cannot reuse the POP3 password or sign in
  with OAuth, and no sent copy is kept, as there is no Sent folder.
- ![Limitation][limitation] APOP and SASL mechanisms other than PLAIN are not
  used.
- ![Limitation][limitation] A certificate the system does not trust can be
  accepted only when the check was asked for, by opening the account or
  reloading its folders; background checks only report it.
- ![Limitation][limitation] A password typed at the prompt is kept for the
  session only; a password stored under **Account Settings › POP3** is used
  across restarts.
- ![Limitation][limitation] Tested against Dovecot in the development
  container, not against real providers or in a Plasma session. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- Prefer IMAP when the provider offers it: it keeps folders, flags and sent
  copies on the server, and the offline search index covers it.
- Keep mail on the server, or remove it only after some days, until the local
  mail is backed up.
- Back up `~/.local/share/flaska.net/trojita/local-mail/`, which holds one
  folder of mail per POP3 account.
- Open the account or reload its folders to answer a certificate prompt.
- Store the POP3 password under **Account Settings › POP3** so that
  background checks can sign in after a restart.

**Proposed improvements:**

- Offer local folders, moving and tags on POP3 accounts, since the local store
  supports them.
- Index POP3 mail for offline search, Plasma Search and the widgets.
- Keep sent copies in a local Sent folder, and let SMTP reuse the POP3
  sign-in.
- Export and import the local store, and offer to keep it when the account is
  removed.
- Try real providers in a Plasma 6 session.

### JMAP accounts

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                              |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------- |
| [#488](https://github.com/mikepadiernos/plasma-impresario/issues/488) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/488#issuecomment-7221227815) | [JMAP accounts](integrations/providers.md#jmap) ([#217](https://github.com/mikepadiernos/plasma-impresario/issues/217)) |

**Current behavior:**

- ![Resolved][resolved] **Add Account** can create a JMAP account, and
  **Account Settings › JMAP** takes the server (a host name or a session
  URL), the user name, a password or an access token, how often to check and
  how to send. Only HTTPS is used, or HTTP to this computer for testing.
- ![Resolved][resolved] Folders and messages are kept in a store on this
  computer and synchronized both ways: read, flagged and other keywords,
  moves, copies, new, renamed and removed folders, and removals go up; the
  server's changes come down, through `Email/changes` when the server can
  tell them. Folders, moving, archive, junk and tags stay available.
- ![Resolved][resolved] The account synchronizes when it is opened or its
  folders are reloaded, at the chosen interval, five seconds after a change
  on this computer, and after sending.
- ![Resolved][resolved] Sending goes through the JMAP server, which keeps the
  copy in Sent and removes it again if sending is refused, or through the
  account's SMTP server.
- ![Limitation][limitation] There is no push: mail the server receives
  appears with the next synchronization. Changes made here while a
  synchronization runs go up with the next one.
- ![Limitation][limitation] Messages are downloaded one at a time, so the
  first synchronization of a large mailbox is slow.
- ![Limitation][limitation] Folders that were on this computer before the
  first synchronization stay here only, and moving a message into one removes
  it from the server. Renaming a special folder (Inbox, Sent, Drafts, Trash,
  Junk, Archive) is undone at the next synchronization, and a `/` in a server
  folder name becomes `_` here.
- ![Limitation][limitation] There is no offline search index for JMAP
  accounts, so Plasma Search and the Plasma widgets' senders and subjects do
  not cover them; searching in the window works on the local copy.
- ![Limitation][limitation] Sending through JMAP needs a sending identity for
  the sender's address on the server. Some servers create one per address;
  others need it added in their own settings.
- ![Limitation][limitation] A certificate the system does not trust can be
  accepted only when the synchronization was asked for; a password typed at
  the prompt is kept for the session only.
- ![Limitation][limitation] Tested against Stalwart in the development
  container (`live-jmap.sh`), not against hosted providers such as Fastmail
  or in a Plasma session. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- For mail that must arrive at once, lower **Check for new mail every** under
  **Account Settings › JMAP**, or reload the folders.
- Let the first synchronization of a large mailbox finish before working in
  the account.
- Create folders after the first synchronization so that they exist on the
  server, and rename special folders on the server.
- If the server may not send as your address, add the address as a sending
  identity in the server's settings, or set **Send messages** to **Through
  the SMTP server on the SMTP page**.
- Use IMAP when the provider offers it and Plasma Search or the Plasma
  widgets should cover the account.
- Open the account or reload its folders to answer a certificate prompt, and
  store the password under **Account Settings › JMAP** so that background
  synchronization can sign in after a restart.

**Proposed improvements:**

- Synchronize on the server's push (EventSource or WebSocket).
- Download several messages per request, within the server's limits.
- Run another synchronization when changes were made during one.
- Index JMAP mail for offline search, Plasma Search and the widgets.
- Try hosted providers such as Fastmail in a Plasma 6 session.

### Mail rules

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                            |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------- |
| [#491](https://github.com/mikepadiernos/plasma-impresario/issues/491) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/491#issuecomment-7223090175) | [M07](ROADMAP.md#m07-output-notifications-and-rules-automation) ([#46](https://github.com/mikepadiernos/plasma-impresario/issues/46)) |

**Current behavior:**

- ![Resolved][resolved] **Mail Rules…** in the application menu keeps rules
  in the order they run. Each covers one account or every account and the
  INBOX or chosen folders, matches all or any of its conditions on senders,
  recipients, subject, attachments, folder, tags and junk, and moves,
  archives, deletes, flags, marks read, tags or announces the message.
- ![Resolved][resolved] The first matching rule that moves a message decides
  where it goes and ends the evaluation; **Skip the rules after this one for
  matching messages** ends it too.
- ![Resolved][resolved] A rule that moves messages runs on new mail by
  itself only after its current version was previewed. Until then it waits,
  and the activity log offers **Review Rule**. **Apply to Folder…** asks
  before it runs a rule on the folder shown. Actions the server refuses are
  reported as failed in the activity log.
- ![Resolved][resolved] **Create Rule from Search…** in the mailbox menu and
  **Create Rule...** on a saved search turn a search into conditions and
  preview the matches at once. Searches that rules cannot check are refused
  with the reason.
- ![Limitation][limitation] Rules run on mail the application sees arrive.
  An account that is not shown keeps its INBOX open for this; the shown
  account's INBOX and every other folder are checked only while open in the
  window, or when the folder is opened again.
- ![Limitation][limitation] Only IMAP accounts are covered. POP3 and JMAP
  accounts keep their mail in a local store that the rules do not watch.
- ![Limitation][limitation] Message text, dates and sizes cannot be checked
  on arrival.
- ![Limitation][limitation] A preview and **Apply to Folder…** look at the
  folder shown in the window, and only at the messages loaded in its list.
- ![Limitation][limitation] A rule made from a saved search takes its text
  only, not its choice of fields. The preview made when it is added counts as
  its review, so a moving rule made that way runs on its own once saved.
- ![Limitation][limitation] Rules are tested without a server; moves and
  refusals on a real server are not exercised, and nothing is checked in a
  Plasma session. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- Keep the folders a rule covers open in the window now and then, so that
  rules see their new mail.
- Load the whole list (scroll to the end) before a preview, then use
  **Apply to Folder…** for mail that arrived before the rule existed.
- Use a search or a saved search for conditions on message text, dates or
  sizes.
- Check the conditions of a rule made from a saved search before saving.
- Try a rule that only flags, tags or marks read first, and add moving
  actions once its matches look right.

**Proposed improvements:**

- Check folders other than the open one in the background.
- Run rules on POP3 and JMAP accounts through their local stores.
- Preview against the server with a search instead of the loaded rows.
- Carry a saved search's field choices into the rule.
- Validate rules against the disposable Dovecot and in a Plasma session.

### New-mail notifications and activity

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                                                                                   |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| [#492](https://github.com/mikepadiernos/plasma-impresario/issues/492) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/492#issuecomment-7223091124) | [M07](ROADMAP.md#m07-output-notifications-and-rules-automation) ([#45](https://github.com/mikepadiernos/plasma-impresario/issues/45), [#377](https://github.com/mikepadiernos/plasma-impresario/issues/377)) |

**Current behavior:**

- ![Resolved][resolved] Each IMAP account has one notification for its new
  mail, updated as mail arrives, with **Open**, **Mark as Read** and, for a
  single message, **Delete**. Mail already read, old mail moved into the
  folder and the conversation being read are skipped.
- ![Resolved][resolved] **Configure Notifications…** in the application menu
  chooses per account **Each message, in one updated notification**, **A
  summary** (every 15 minutes at most by default) or **Nothing**, and the
  folders announced besides the INBOX. **Wait while the window is in use**
  and **Show senders and subjects** apply to every account. **Notify About
  New Mail** on a folder turns it on or off there.
- ![Resolved][resolved] The **Activity** window, opened from the application
  menu or the status bar button, lists connections, offline text downloads
  with progress, attachment scans, rules and announcements, with **Retry**,
  **Sign In**, **Review Rule**, **Open Settings** or **Open Account** where
  they help.
- ![Limitation][limitation] Folders other than a background account's INBOX
  are announced from their unread counts, looked at every two minutes, so
  their notification gives a count rather than senders and subjects.
- ![Limitation][limitation] Mail a rule moves to another folder is announced
  by count, as its place in that folder is not known yet.
- ![Limitation][limitation] Mail that arrived while the application was
  closed is announced only if it is unread and was received after the
  folder's last check, less ten minutes for delivery delays.
- ![Limitation][limitation] POP3 and JMAP mail is not announced; their
  connection state is still in the activity log.
- ![Limitation][limitation] The decisions are tested without a desktop; the
  notification and its actions are untried in a Plasma session. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- Open a folder, or add a rule with **Notify** for the mail that matters, to
  have senders and subjects announced from folders other than the INBOX.
- Check the **Activity** window after starting the application to see what
  arrived while it was closed and what rules did.
- Keep POP3 and JMAP folders open in the window to see their new mail.

**Proposed improvements:**

- Watch announced folders in the background so they can list senders and
  subjects.
- Look up where a rule moved a message so it can be listed.
- Announce POP3 and JMAP mail from their local stores.
- Try the notification and its actions in a Plasma 6 session.

### Settings and workspaces

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                                                                                                                                                                                                                          |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [#496](https://github.com/mikepadiernos/plasma-impresario/issues/496) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/496#issuecomment-7225440304) | [M08](ROADMAP.md#m08-settings-and-workspace-configuration) ([#47](https://github.com/mikepadiernos/plasma-impresario/issues/47), [#48](https://github.com/mikepadiernos/plasma-impresario/issues/48), [#115](https://github.com/mikepadiernos/plasma-impresario/issues/115), [#356](https://github.com/mikepadiernos/plasma-impresario/issues/356)) |

**Current behavior:**

- ![Resolved][resolved] **Settings...** in the application menu shows General,
  Accounts, Appearance, Reading, Composition, Privacy, Notifications, Search,
  Integrations, Workspaces and Keyboard in a page list, and reopens the group
  shown last.
- ![Resolved][resolved] Reading, message and search settings can be set for an
  account or a folder under **Accounts**; each override shows where the
  inherited value comes from, such as **Inherited from account Work**, and can
  be removed.
- ![Resolved][resolved] A workspace groups accounts, a notes folder and a
  Plasma Activity. With **Show a workspace's account when its activity becomes
  current** on, switching to an attached activity shows one of its accounts;
  new notes go to its folder while it is current.
- ![Resolved][resolved] **Search** chooses where searches look, whether the
  offline index or the server answers while online, how many searches are
  kept, and which accounts index messages and attachments.
- ![Limitation][limitation] Plasma Activities are read from the Activity
  Manager over D-Bus. Without it the activity list is empty, workspaces do not
  follow activities, and the Workspaces group says why.
- ![Limitation][limitation] A workspace covers accounts, a notes folder and an
  activity only. Contacts and address books are shared by every workspace.
- ![Limitation][limitation] Turning the offline index on or off for an account
  takes effect when the account connects again.
- ![Limitation][limitation] An account's **Folder shown first** opens when the
  window switches to the account, not when the application starts.
- ![Limitation][limitation] The settings are tested offscreen; the Activity
  Manager and a Plasma session are not exercised. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- Start the Activity Manager (it runs in every Plasma session) before relying
  on workspaces that follow activities.
- Use the account's address book settings to keep contacts apart; a workspace
  does not filter them.
- After changing which accounts are indexed, take the account offline and
  online again, or restart the application.
- Switch to another account and back to land in the start folder.

**Proposed improvements:**

- Use the KActivities library where it is available and keep D-Bus as the
  fallback.
- Let a workspace choose address books, so contact suggestions follow it.
- Start or stop indexing at once when the setting changes.
- Apply the start folder of the shown account at startup when no folder is
  restored.
- Try activities and workspaces in a Plasma 6 session
  ([M15](ROADMAP.md#m15-real-plasma-session-verification)).

### Mail tabs

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                         |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------- |
| [#497](https://github.com/mikepadiernos/plasma-impresario/issues/497) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/497#issuecomment-7225442893) | [M08](ROADMAP.md#m08-settings-and-workspace-configuration) ([#246](https://github.com/mikepadiernos/plasma-impresario/issues/246)) |

**Current behavior:**

- ![Resolved][resolved] Tabs are off by default. **Open folders and messages
  in tabs** under **Appearance** turns them on and chooses their region and
  orientation; tab shapes open towards the content.
- ![Resolved][resolved] **New Tab**, **Close Tab**, **Next Tab** and
  **Previous Tab** are in the View menu and **Open in New Tab** in the message
  list context menu; Ctrl+Tab and Ctrl+Shift+Tab switch tabs as well.
- ![Resolved][resolved] Tabs are restored at the next start in the region they
  were in. Tabs of removed accounts are dropped and renamed folders are
  followed.
- ![Resolved][resolved] Tabs hold no drafts; closing a tab or turning tabs off
  loses nothing.
- ![Limitation][limitation] Subjects are not written to the state file, so a
  restored message tab shows its folder name until its message opens again.
- ![Limitation][limitation] A vertical tab bar in the header or footer is
  aligned to the left and makes that strip as tall as the tabs.
- ![Limitation][limitation] Ctrl+Tab and Ctrl+Shift+Tab are added to the
  configured shortcuts of **Next Tab** and **Previous Tab** when the window
  starts; after those shortcuts are changed in the shortcut settings, the
  extra keys are gone until the next start.
- ![Limitation][limitation] Tabs are tested offscreen, not in a Plasma
  session. That is [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- Hover over a restored tab for its account and folder, or click it to load
  its message and subject.
- Use a horizontal tab bar in the header or footer, or put a vertical one in a
  side column.
- Restart the application after changing the tab shortcuts to get Ctrl+Tab
  back.

**Proposed improvements:**

- Offer to keep subjects of restored tabs, off by default for privacy.
- Turn a header or footer tab bar horizontal by itself, or explain the choice
  in Appearance.
- Keep Ctrl+Tab as a fixed alternate, independent of the configured shortcut.
- Try tab placement in a Plasma 6 session
  ([M15](ROADMAP.md#m15-real-plasma-session-verification)).

### Vim mode and shared shortcuts

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                                                                                |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [#498](https://github.com/mikepadiernos/plasma-impresario/issues/498) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/498#issuecomment-7225446051) | [M08](ROADMAP.md#m08-settings-and-workspace-configuration) ([#437](https://github.com/mikepadiernos/plasma-impresario/issues/437), [#438](https://github.com/mikepadiernos/plasma-impresario/issues/438)) |

**Current behavior:**

- ![Resolved][resolved] Vim mode is off by default. **Keyboard** turns it on,
  sets the hint delay and sequence timeout, edits each command's keys and
  reports conflicts and window shortcuts it shadows.
- ![Resolved][resolved] Keys typed into text fields stay text. Vim commands
  come before the window's single-key shortcuts while Vim mode is on.
- ![Resolved][resolved] A hint popup lists what may follow a pending key; the
  shortcut overview lists the Vim commands while Vim mode is on.
- ![Resolved][resolved] **Export…**, **Import…** with a preview, **Undo
  Import** and **Follow changes to a shared keymap file** share Vim mode and
  the shared commands with Plasma Opus; mail-only commands stay.
- ![Limitation][limitation] In the HTML message reader, Vim mode cannot tell
  whether an editable element inside the page has focus, so keys there are
  taken as commands.
- ![Limitation][limitation] Vim bindings are kept in the **Keyboard** group of
  the settings, not as window actions; the shortcut settings do not list them.
- ![Limitation][limitation] The pending keys and the **Vim** indicator are
  shown in the status bar only.
- ![Limitation][limitation] Vim mode is tested offscreen with simulated keys,
  not in a Plasma session. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- Turn Vim mode off, or click outside the message, before typing into a form
  inside an HTML message.
- Edit Vim keys under **Keyboard** in the settings; check its conflict list
  after changing window shortcuts.
- Keep the status bar shown while using Vim mode, or rely on the hint popup.

**Proposed improvements:**

- Ask the web view whether an editable element has focus before taking a key.
- Expose Vim commands as actions so the shortcut settings can show them.
- Offer the pending keys in the hint popup when the status bar is hidden.
- Try Vim mode and keymap following in a Plasma 6 session
  ([M15](ROADMAP.md#m15-real-plasma-session-verification)).

### Messages to calendar and notes

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                                                                                |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [#499](https://github.com/mikepadiernos/plasma-impresario/issues/499) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/499#issuecomment-7225448737) | [M08](ROADMAP.md#m08-settings-and-workspace-configuration) ([#121](https://github.com/mikepadiernos/plasma-impresario/issues/121), [#122](https://github.com/mikepadiernos/plasma-impresario/issues/122)) |

**Current behavior:**

- ![Resolved][resolved] Both actions are off by default. **Integrations**
  turns them on and explains why one cannot be offered: no application opens
  calendar events, or no notes folder is chosen.
- ![Resolved][resolved] They are in the Message menu, and in the list context
  menu for the message open in the reader.
- ![Resolved][resolved] An event starts on the next hour and holds the
  subject, sender, date and text; the calendar application opens it for
  editing.
- ![Resolved][resolved] A note is a new Markdown file in the notes folder, or
  in the current workspace's folder when **Save notes to the folder of the
  current workspace** is on, with **Open Note** afterwards.
- ![Limitation][limitation] The notes folder is chosen in the settings; it is
  not read from Marknote's own configuration.
- ![Limitation][limitation] Encrypted messages are saved without their text,
  as subject, sender and date only.
- ![Limitation][limitation] The event file is written to the application's
  cache and removed after a day.
- ![Limitation][limitation] The actions use the message open in the reader,
  not a selection of several messages.
- ![Limitation][limitation] The hand-off is tested offscreen; the calendar
  application and Marknote are not exercised in a Plasma session. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- Choose the folder Marknote uses under **Integrations**.
- Copy the text of an encrypted message by hand into the note or event when it
  is needed.
- Save the event in the calendar application when it opens; the cached file is
  only the hand-off.
- Open each message in turn to save several.

**Proposed improvements:**

- Read Marknote's notes folder when it is installed.
- Ask before including decrypted text, instead of leaving it out.
- Hand events over with Akonadi when it is available instead of a file.
- Save each selected message as a note or event.
- Try both hand-offs in a Plasma 6 session
  ([M15](ROADMAP.md#m15-real-plasma-session-verification)).

### Themes and appearance

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                 |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------- |
| [#502](https://github.com/mikepadiernos/plasma-impresario/issues/502) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/502#issuecomment-7227463706) | [M09](ROADMAP.md#m09-themes-and-branding-foundation) ([#10](https://github.com/mikepadiernos/plasma-impresario/issues/10)) |

**Current behavior:**

- ![Resolved][resolved] Every appearance option follows Plasma by default and
  is on the **Theme** settings page. Changes preview live and can be reverted
  before saving.
- ![Resolved][resolved] The appearance is layered from Plasma's defaults
  through the global settings, the active workspace's theme profile and its
  overrides to the custom stylesheet.
- ![Resolved][resolved] **Reset to Plasma Defaults**, `--reset-appearance` and
  `--no-workspace-appearance` restore Plasma's look, and a high-contrast color
  scheme can suspend blur, translucency, tints, custom radii and the
  stylesheet.
- ![Limitation][limitation] Translucency must be requested before a window is
  created, so turning window opacity or blur on or off applies when the window
  next opens. Opacity values and blur strength change live.
- ![Limitation][limitation] The options are drawn by a proxy around the widget
  style chosen at startup; changing the widget style in System Settings while
  the application runs drops the proxy until the next start.
- ![Limitation][limitation] Buttons, combo boxes and scroll bars keep the
  widget style's own corner radius.
- ![Limitation][limitation] Message cards appear only in the two-line layout
  and have no hover quick actions.
- ![Limitation][limitation] When a workspace overrides the message list style,
  a **Cards** or **Rows** choice from the menu is replaced by the workspace's
  choice when the workspace applies again. Workspace overrides are taken from
  the saved global values, not from an unapplied preview.
- ![Limitation][limitation] The reader column's tint covers the embedded
  panels placed in it, and unified-inbox columns in the single-line layout
  show no account color dot.
- ![Limitation][limitation] With the style's own tab shape, **None** cannot
  hide the style's selected-tab marking, and **Line** is drawn by the style.
- ![Limitation][limitation] A symbolic tray icon shows no unread count; the
  count is in its tooltip.
- ![Limitation][limitation] Appearance is tested offscreen; KWin blur, Breeze
  and Breeze High Contrast are not tried in a Plasma session. That is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).

**Workarounds:**

- After turning window opacity or blur on or off, close and reopen the window,
  or restart the application.
- After changing the widget style in System Settings, restart the application.
- To keep a message list style regardless of the workspace, remove the style
  from the workspace's appearance overrides.
- Apply a preview before saving a workspace so that its overrides capture it.
- To see the unread count in the tray, turn the symbolic tray icon off.

**Proposed improvements:**

- Recreate the native window when translucency changes, keeping its geometry.
- Rewrap a new widget style in the appearance proxy on `QEvent::StyleChange`.
- Add hover quick actions to message cards, and a card layout for the
  single-line list.
- Add an account color dot to unified single-line columns.
- Draw an unread badge on the symbolic tray icon in the panel's text color.
- Try blur, Breeze and Breeze High Contrast in a Plasma 6 session
  ([M15](ROADMAP.md#m15-real-plasma-session-verification)).

### Project naming and compatibility

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                      |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------- |
| [#503](https://github.com/mikepadiernos/plasma-impresario/issues/503) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/503#issuecomment-7227464544) | [M09.01](ROADMAP.md#m09-themes-and-branding-foundation) ([#242](https://github.com/mikepadiernos/plasma-impresario/issues/242)) |

**Current behavior:**

- ![Resolved][resolved] Project-owned macros, CMake helpers and modules,
  generated headers and C++ symbols use Plasma Impresario names.
  `CONTRIBUTING.md` lists the Trojitá names kept on purpose and why.
- ![Resolved][resolved] `PLASMA_IMPRESARIO_PROFILE` chooses a profile;
  `TROJITA_PROFILE` is read when it is not set.
- ![Limitation][limitation] The settings location, the `bin/trojita`
  launcher, the desktop file, plugin targets and the `trojita_plugins`
  directory keep their names.
- ![Limitation][limitation] `x-trojita` MIME types and plugin interface IDs,
  `x-trojita-*` cache keys and URL schemes, and the `trojita_qt` translation
  catalog keep their names.
- ![Limitation][limitation] NSIS installer file names, license headers and
  historical references in parity and philosophy documents are unchanged.

**Workarounds:**

- Use `PLASMA_IMPRESARIO_PROFILE` in new scripts.
- Do not rename the kept identifiers in packaging or plugins; installations
  and caches look for them.

**Proposed improvements:**

- Migrate the settings location with a one-time copy and keep reading the old
  one for a release.
- Install a `plasma-impresario` launcher and desktop file, keeping `trojita`
  as an alias.
- Accept new MIME types and plugin IDs alongside the `x-trojita` ones before
  deprecating them.

## Before reporting results

- [ ] Confirm the desktop environment and display server.
- [ ] Record the selected account transport and password plugin.
- [ ] Identify the provider, server capabilities and whether WebEngine or
      OpenPGP support is enabled.
- [ ] Isolate test credentials and mail data.
- [ ] Record Qt, KF6, compiler, distribution and build options.
- [ ] Retain a reproducible mailbox size and sample message set.

## Security and data protection

> [!CAUTION]
>
> This is a source-based status summary, not a security audit. Behavior varies
> with account settings and optional build features.

| Area              | Existing behavior                                                                                                                                                                                                                                                                                                                                  | Limitation                                                                                                                                                                                                                                                                                                                  |
| ----------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| IMAP transport    | Supports cleartext, STARTTLS, dedicated TLS and process-backed connections. Qt certificate errors prompt the user; a previously accepted matching public key can be remembered, and an accepted key is trusted for its host by every account of the profile.                                                                                       | Cleartext is selectable; the active connection method determines whether traffic is encrypted. Certificate decisions and policy need focused regression tests.                                                                                                                                                              |
| SMTP transport    | Supports cleartext, STARTTLS and dedicated TLS. Certificate errors prompt the user in the composer; an accepted public key is remembered per account and forgotten when the SMTP host changes. A key accepted for the same host by any account, over IMAP or SMTP, is accepted without asking.                                                     | Cleartext is selectable. Certificates the system trusts are accepted without remembering their key, so only the system's policy applies to them. There is no way yet to review or forget accepted keys.                                                                                                                     |
| Credentials       | The password-plugin interface supports retrieval, storage and deletion. QtKeychain can provide encrypted storage when built and selected, and is the default for new profiles. Each account keeps its own stored passwords, which are deleted when the account is removed.                                                                         | QtKeychain is optional. Profiles that already have saved cleartext passwords keep the cleartext plugin, which stores IMAP/SMTP secrets in `QSettings`.                                                                                                                                                                      |
| Message rendering | The reader uses a private profile per message, disables scripts, plugins and persistent storage, blocks local-file and remote requests by default, and delegates permitted external links. Remote images require explicit per-message consent. Switching messages destroys the profile, discarding loaded content and cancelling pending requests. | Renderer behavior has not been smoke-tested in a live Plasma session or across the documented distribution matrix.                                                                                                                                                                                                          |
| Cached data       | Persistent SQL cache can hold message metadata and fetched parts. Cache folders are 0700 and files 0600, tightened at each start; copies of opened attachments are user-only and removed after a day ([details](#local-data-and-opened-attachments)).                                                                                              | Application-level cache encryption is not present, and the offline search index, with decrypted text when chosen, is stored unencrypted in the cache. Directory permissions do not protect mail from a compromised account or unencrypted disk. Removing an account deletes its cache files without overwriting them first. |
| Outgoing mail     | Header values are stripped of line breaks and control characters, Bcc and Resent-Bcc are removed from the delivered copy, and Message-IDs use the sender's domain instead of the host name ([details](#outgoing-headers-and-bcc)).                                                                                                                 | Sent copies delivered through BURL or the IMAP SENDMAIL Extension omit Bcc; cleaned input is not pointed out.                                                                                                                                                                                                               |
| OpenPGP           | Optional GpgME++/Mimetic MIME handling and decryption tests are present.                                                                                                                                                                                                                                                                           | Availability depends on build options; complete signature-status UX and verification policy are not established.                                                                                                                                                                                                            |
| Diagnostics       | Persistent protocol logging is opt-in at startup; an in-app protocol log is available.                                                                                                                                                                                                                                                             | Treat protocol logs as sensitive and review them before sharing.                                                                                                                                                                                                                                                            |

### Recommendations

| Priority | Focus                                                                                                                                  |
| -------- | -------------------------------------------------------------------------------------------------------------------------------------- |
| **P0**   | [Secure account defaults](#p0--secure-account-defaults); [protect local data and diagnostics](#p0--protect-local-data-and-diagnostics) |
| **P1**   | [Reduce message-content risk](#p1--reduce-message-content-risk); [clarify cryptographic results](#p1--clarify-cryptographic-results)   |
| **P2**   | [Define release security checks](#p2--define-release-security-checks)                                                                  |

#### P0 — secure account defaults

- Default new accounts to verified TLS. Keep cleartext as an explicit, clearly
  warned compatibility option.
- Fail closed when required TLS negotiation fails; never retry authentication
  over plaintext after a TLS failure.
- Prefer QtKeychain or another supported OS credential store. Make `QSettings`
  cleartext storage an explicit opt-in and provide a safe migration path for
  existing credentials.
- Test certificate rejection and key changes, TLS negotiation failures, STARTTLS
  downgrade handling and SMTP transport policy.

#### P0 — protect local data and diagnostics

- Document cache location, retention and deletion; keep restrictive file and
  directory permissions and recommend full-disk encryption for cached mail.
- Review IMAP and SMTP logs for credentials, message content and personal data.
  Redact secrets before buffering or writing logs and test the redaction.
- Make persistent logging discoverable and temporary, with a clear way to stop
  logging and remove the resulting file.

#### P1 — reduce message-content risk

- Verify the Qt WebEngine reader in live Plasma sessions and across the
  supported distribution matrix.
- Keep scripts, plugins and remote content disabled by default; show external
  destinations before enabling requests where practical and reset consent when
  changing messages.
- Test malformed MIME, nested parts, oversized content, link schemes and failed
  attachment downloads using isolated fixtures.

#### P1 — clarify cryptographic results

- Distinguish encrypted, decrypted, signed and verified states, including
  invalid, unknown-key, expired-key and unsupported cases.
- Ensure failed or unavailable crypto never appears as successful verification;
  expand tests for modified signatures, expired and unknown keys, nested MIME
  and error recovery.

#### P2 — define release security checks

- Document protected data and trust boundaries across mail servers, message
  content, plugins, the local account and external tools.
- Add dependency review, static analysis, clean configuration builds, privacy
  review and regression tests to release checks.
- Record tested configurations and residual risks without claiming that tests
  prove the client is secure.

### Safe operating guidance

> [!TIP]
>
> These settings reduce risk with the current build; they do not replace the
> recommendations above.

- Use implicit TLS or required STARTTLS with valid certificates; reject
  certificate warnings unless independently verified with the provider.
- Select a secure credential plugin and avoid saving passwords through the
  cleartext plugin.
- Keep remote message content blocked unless needed for a trusted message.
- Avoid persistent debug logging during ordinary use; sanitize logs before
  attaching them to reports.
- Use operating-system disk encryption when retaining cached mail.

[backlog]: https://img.shields.io/badge/Backlog-6e7781
[limitation]: https://img.shields.io/badge/Limitation-bc4c00
[merged]: https://img.shields.io/badge/Merged-8250df
[planned]: https://img.shields.io/badge/Planned-6e7781
[resolved]: https://img.shields.io/badge/Resolved-1a7f37
[review]: https://img.shields.io/badge/Review-2f81f7
[security]: https://img.shields.io/badge/Security-bf3989
