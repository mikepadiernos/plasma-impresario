# Pending tasks

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/4)** ·
**[Bugs](BUGS.md)** · **[Caveats](CAVEATS.md)** · **[Edge cases](EDGE.md)**

This log records unfinished work, blockers and resumable handoffs. Issue
statuses below match the project board as of 2026-11-25.

Legend: ![Done][done] or verified · ![Needs verification][needs-verification] ·
![In progress][in-progress] · ![Planned][planned] · ![Blocked][blocked]

> [!IMPORTANT]
>
> Confirm the current issue and project status before acting. An issue is done
> only when its board status is $\color{#2da44e}\textbf{Completed}$ or
> $\color{#8957e5}\textbf{Merged}$. $\color{#2f81f7}\textbf{Review}$ is not
> done, and $\color{#2da44e}\textbf{Completed}$ does not mean merged into
> `main`.

## Contents

- [Backlog](#backlog)
- [In progress](#in-progress)
- [Pending verification](#pending-verification)
  - [M02 shell and account foundation](#m02-shell-and-account-foundation)
  - [M15 real Plasma session
    verification](#m15-real-plasma-session-verification)
  - [M03 mail operations](#m03-mail-operations)
  - [M01 release gate and integration](#m01-release-gate-and-integration)
  - [Milestone feature-issue
    synchronization](#milestone-feature-issue-synchronization)
  - [M13 Microsoft milestone insertion](#m13-microsoft-milestone-insertion)

## Backlog

- [ ] Validate the [initial compatibility target](philosophy/compatibility.md)
      with a complete reader and desktop session before declaring release
      support
- [ ] Define the minimum viable account and mailbox acceptance scenarios
- [ ] Verify the main window, message list and preview layout in an end-to-end
      flow
- [ ] Add implementation-backed acceptance criteria to planned notification,
      search and mailbox-organization features

## In progress

- ![Done][done] **M02 shell, accounts and mailbox foundation**
  ([#4](https://github.com/mikepadiernos/plasma-impresario/issues/4)): merged
  into `main` and into `feature/m03-mail-operations`.
- ![Done][done] **M03 mail operations, sorting and folder management**
  ([#5](https://github.com/mikepadiernos/plasma-impresario/issues/5)): merged
  into `main`. See [M03 mail operations](#m03-mail-operations).
- ![In progress][in-progress] **M09 themes and branding foundation**
  ([#10](https://github.com/mikepadiernos/plasma-impresario/issues/10)): in
  Development because M09.01 Branding and visual language
  ([#269](https://github.com/mikepadiernos/plasma-impresario/issues/269))
  started with the product rename
  ([#221](https://github.com/mikepadiernos/plasma-impresario/issues/221)), which
  is Merged. The other M09.01 features and M09.02–M09.10 are in the Backlog.
- ![In progress][in-progress] **Documentation:** keep the roadmap and these
  documents synchronized with project-board decisions. See [issue
  synchronization](#milestone-feature-issue-synchronization).

## Pending verification

### M02 shell and account foundation

M02 ([#4](https://github.com/mikepadiernos/plasma-impresario/issues/4)) is
feature-complete: all 14 roadmap items are checked and
`feature/m02-shell-next` is merged into `main`. Its features were in **M02.01
Headless implementation and verification**
([#318](https://github.com/mikepadiernos/plasma-impresario/issues/318)), built
and verified with offscreen QtTest and the synthetic Dovecot server in
`tools/dev-container/`. The checks that need a real Plasma session, KWallet or
a real provider account moved to
[M15](#m15-real-plasma-session-verification) on 2026-11-25. What remains here
are the [open follow-ups](#open-follow-ups).

#### Issue status

| Issue                                                                 | Feature                                                     | Board  | State                                                                                       |
| --------------------------------------------------------------------- | ----------------------------------------------------------- | ------ | ------------------------------------------------------------------------------------------- |
| [#4](https://github.com/mikepadiernos/plasma-impresario/issues/4)     | M02 milestone                                               | Merged | ![Done][done] Feature-complete                                                              |
| [#318](https://github.com/mikepadiernos/plasma-impresario/issues/318) | M02.01 Headless implementation and verification             | Merged | ![Done][done] All 13 features                                                               |
| [#35](https://github.com/mikepadiernos/plasma-impresario/issues/35)   | Mail shell                                                  | Merged | ![Done][done] Verified headless; Plasma-session check is #315                               |
| [#74](https://github.com/mikepadiernos/plasma-impresario/issues/74)   | App skeleton in the design-spec layout                      | Merged | ![Done][done] Optional header region                                                        |
| [#36](https://github.com/mikepadiernos/plasma-impresario/issues/36)   | Account setup and mailbox state                             | Merged | ![Done][done] [Account model decisions](#account-model-decisions)                           |
| [#78](https://github.com/mikepadiernos/plasma-impresario/issues/78)   | Account setup, editing, removal, identities and credentials | Merged | ![Done][done] Verified against disposable Dovecot; KWallet is #316                          |
| [#80](https://github.com/mikepadiernos/plasma-impresario/issues/80)   | Account grouping, folder navigation and keyboard focus      | Merged | ![Done][done] Account sidebar                                                               |
| [#215](https://github.com/mikepadiernos/plasma-impresario/issues/215) | Protocol-aware account and capability model                 | Merged | ![Done][done] Unsupported actions hidden                                                    |
| [#218](https://github.com/mikepadiernos/plasma-impresario/issues/218) | OAuth account authentication                                | Merged | ![Done][done] Headless; live sign-in is #317, caveat [#362](CAVEATS.md#oauth-sign-in)       |
| [#75](https://github.com/mikepadiernos/plasma-impresario/issues/75)   | Density modes                                               | Merged | ![Done][done] Row-density selector                                                          |
| [#76](https://github.com/mikepadiernos/plasma-impresario/issues/76)   | Folder counts and saved searches                            | Merged | ![Done][done] Verified against disposable IMAP                                              |
| [#77](https://github.com/mikepadiernos/plasma-impresario/issues/77)   | Milestone gate for later mailbox work                       | Merged | ![Done][done]                                                                               |
| [#79](https://github.com/mikepadiernos/plasma-impresario/issues/79)   | Connection, authentication, offline and loading states      | Merged | ![Done][done] Footer and per-account status                                                 |
| [#81](https://github.com/mikepadiernos/plasma-impresario/issues/81)   | Pane layout, selection and density across sessions          | Merged | ![Done][done] Verified against disposable IMAP                                              |
| [#222](https://github.com/mikepadiernos/plasma-impresario/issues/222) | Multiple simultaneous accounts                              | Merged | ![Done][done] Verified against disposable IMAP; caveat [#262](CAVEATS.md#multiple-accounts) |

#### Live verification environment

> [!WARNING]
>
> The live tests are opt-in and use only loopback-only Dovecot servers with
> synthetic users and the synthetic password `synthetic-test-only`. Never point
> them at a real account or service.

| Check                   | How to run                                                                                     | Server                                                                                    | Verifies                                                |
| ----------------------- | ---------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------- | ------------------------------------------------------- |
| `test_LiveImap`         | Set `PLASMA_TEST_IMAPS_PORT` to a loopback-only Dovecot IMAPS listener                         | Dovecot 2.4 in the dev container; 2.4.5 at the recorded account-settings run              | #78, #81, #76                                           |
| `test_LiveImapAccounts` | `tools/dev-container/run.sh sh tools/dev-container/live-imap-accounts.sh` in the dev container | Loopback-only Dovecot 2.4 with synthetic users; 2.4.5 at the recorded run                 | #222, #79, per-account #76, #78 removal and credentials |
| `test_LiveImapPanels`   | `tools/dev-container/run.sh sh tools/dev-container/live-imap-panels.sh`                        | Same Dovecot 2.4 helper, seeded with a conversation, attachments and a signed message     | #35 embedded panels, screenshots                        |
| `test_LiveImapCompose`  | `tools/dev-container/run.sh sh tools/dev-container/live-imap-compose.sh`                       | The same seeded mailbox with a Sent folder; mail goes out through a local sendmail script | #93 reading to responses, #41 composing and recovery    |
| VNC view                | `tools/dev-container/vnc.sh`                                                                   | The same seeded mailbox, shown in a VNC session                                           | #35 by hand                                             |

The dev container pins Fedora 44, and `synthetic-dovecot.sh` refuses any Dovecot
outside the 2.4 series, which its configuration requires. It prints the exact
version on each run; record it with new results. The dev container has no Plasma
session, so D-Bus services, KWallet, notifications and the Plasma look still
need a real session.

| Variable                                                                                 | Used by                | Purpose                                                                                                                                                        |
| ---------------------------------------------------------------------------------------- | ---------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `PLASMA_TEST_IMAPS_PORT`                                                                 | All live tests         | Loopback IMAPS port. The dev-container scripts set it.                                                                                                         |
| `PLASMA_TEST_IMAP_RESTORE_UID`, `PLASMA_TEST_IMAP_RESTORE_VALIDITY`                      | `test_LiveImap`        | Include message restoration. Seed INBOX with a synthetic message with this UID and UIDVALIDITY.                                                                |
| `PLASMA_TEST_IMAP_RESTART_PHASE` (`write`, then `read`), `PLASMA_TEST_IMAP_RESTART_HOME` | `test_LiveImap`        | Check restoration across two processes. Requires the restore variables; the home must be a temporary directory named `plasma-impresario-imap-restart-*`.       |
| `PLASMA_TEST_IMAP_SEARCH_SUBJECT`                                                        | `test_LiveImap`        | Check folder counts and saved-search filtering. Requires the matching message's UID and UIDVALIDITY; INBOX must contain exactly two unread synthetic messages. |
| `PLASMA_TEST_MAILDIR`                                                                    | `test_LiveImapCompose` | The synthetic user's Maildir, to count the copies filed in Sent. `live-imap-compose.sh` sets it.                                                               |
| `PLASMA_TEST_SCREENSHOT_DIR`                                                             | `test_LiveImapPanels`  | Save layout screenshots. `live-imap-panels.sh` defaults to `<build-dir>/shell-screenshots`.                                                                    |

#### Verification record

- ![Done][done] **Account setup, #78:** against the disposable Dovecot 2.4.4
  container, the app saved IMAP settings, recovered from a rejected password,
  selected INBOX and reconnected after going offline.
- ![Done][done] **Account settings, #78:** **Application › Account Settings…**
  opens a dialog for the active account with the Identities, IMAP, Offline and
  SMTP pages, titled with the account name. **Settings…** now holds only the
  application-wide General and Favorite tags pages; saving it no longer
  reconnects the account. Adding an account opens its account settings, and so
  does starting with an unconfigured account. `test_AccountSettingsDialog`
  covers both dialogs, cancelling, and that saving edits only the active
  account. The live setup in `test_LiveImap` passed through the new dialog
  against Dovecot 2.4.5.
  - Fixed on this branch: every account opened its offline cache under the same
    SQLite connection name, so each account replaced the previous one's
    connection and an "IMAP Cache Error" appeared at startup with two accounts.
    Each account now uses its own connection name. Not on `main`, which has no
    multiple accounts.
  - The dialog edits the active account. An account header's menu in the
    folder sidebar opens it for another account by switching first
    ([#262](CAVEATS.md#multiple-accounts)).
- ![Done][done] **Credential defaults and removal, #78:** verified with
  `test_LiveImapAccounts` against Dovecot 2.4.5. Removing a connected background
  account and then the active account deleted each one's settings file and
  stored password, and the remaining account took over with its own stored
  password. A new profile selected QtKeychain, signed in through the password
  prompt when no secret store was available, and wrote no password to any
  settings file. Storage in KWallet still needs a real session (#316).
- ![Done][done] **Sender identities, #78:** each account has its own identities
  by default, edited on the Identities tab of the Account Settings dialog. An
  account can instead use the identities shared by all accounts; it keeps its
  own list for later. An account can also send with **No identity**: the
  composer then starts with an empty From address, which is typed by hand, and
  adds no signature or organization; the account's own list is kept, read-only,
  for when it switches back. The **Send from** list on the Identities tab offers
  the three choices. On first start, existing accounts receive a copy of the
  previously shared identities, which remain the shared list. New accounts start
  with no identities, and the composer then accepts a typed From address. A
  composer keeps the identities of the account it was opened from.
  `test_AccountIdentities` and `test_MainWindowAccounts` cover the defaults, the
  shared and no-identity options, migration and switching;
  `test_AccountSettingsDialog` chooses **No identity** on the Identities tab and
  checks that only the edited account changes.
- ![Done][done] **Session selection, #81:** the app saves a message's account,
  mailbox, UIDVALIDITY and UID, restores it after asynchronous mailbox loading,
  and cancels pending restoration on navigation. Live checks passed restoration,
  rapid offline/reconnect and restoration across application restarts, with and
  without WebEngine. Separate-process tests verify saved selection identifiers,
  pane proportions in wide, compact and one-panel layouts, sidebar position and
  density across close, quit and hidden-window quit.
- ![Done][done] **Saved selection per account, #81 and #222:** verified with
  `test_LiveImapAccounts` against Dovecot 2.4.5. Selecting a message in one
  account and a message and then another folder in a second account saved each
  selection under its own account and left the other unchanged. Switching back
  and forth restored each account's own mailbox and message.
- ![Done][done] **Folder counts and saved searches, #76:** folders show unread
  indicators, and tooltips show total, unread and recent counts; unknown counts
  are omitted rather than reported as zero. The search options menu saves, runs
  and deletes named searches for the active account and selected folder.
  Verified with two synthetic messages, with and without WebEngine, and per
  account with three accounts in `test_LiveImapAccounts`.
- ![Done][done] **Multiple accounts, #222 and #79:** each account has its own
  settings file and `ImapAccess`. A toolbar selector, menu actions and
  <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>PgUp</kbd> /
  <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>PgDown</kbd> switch accounts without
  reconnecting. Other accounts connect at startup, and their connection, network
  and authentication status appears in the selector. Cleartext passwords are
  kept per account. Saved searches and folder expansion follow the active
  account; only the migrated legacy account inherits the previously shared
  expansion state.
  - Offscreen tests cover migration, switching, background status, removal,
    keyboard navigation, per-account saved searches and expansion, and pane
    focus cycling.
  - `test_LiveImapAccounts` verified that two background accounts connect at
    startup, that a rejected stored password is flagged in the selector without
    a prompt, and that switching keeps each account's folders, counts and saved
    searches without reconnecting.
  - That check found and fixed two bugs: background accounts never logged in
    because the model connects lazily, and certificate prompts were answered for
    the active account instead of the requesting one.
  - The limitations found then are resolved in
    [#262](CAVEATS.md#multiple-accounts).
- ![Done][done] **Mail shell, #35 (Completed):** the shell has a left column,
  main column, right column and footer, with connection status in the footer.
  The Navigation toolbar docks at the left window edge by default, and the menu
  bar is hidden by default. The mailbox sidebar can sit in either side column;
  the message list and reader can be side by side, stacked or shown one panel at
  a time.
  - View › Embedded Panels has a submenu per panel: **Show Panel**, **Hide When
    Empty** and a position below the reader, in either side column or in the
    footer. Side columns and the footer appear only while they hold the sidebar
    or a visible panel, and remember their sizes.
  - Attachments is on below the reader by default; Message Details, Contacts and
    Related Messages are off by default in the right column, so the default look
    is unchanged.
  - <kbd>F6</kbd> and <kbd>Shift</kbd>+<kbd>F6</kbd> cycle focus through the
    header (when shown), account selector, folder sidebar, message list, reader
    and each visible embedded panel.
  - Offscreen tests cover placement, sidebar swaps, hiding empty regions,
    one-panel layout and size persistence.
  - `test_LiveImapPanels` verified the details, contacts, related messages and
    attachments panels and saves layout screenshots. It found that Contacts
    listed the sender twice when a message has no Reply-To header (fixed). The
    screenshots showed panels squeezed to a sliver when sharing a column,
    attachments below the reader showing one row, and a tall footer; panels now
    start at a size for a few rows and keep a size the user drags them to.
  - Pane sizes saved before the shell carry over to it; the remaining panel
    limitations are
    [caveat #264](CAVEATS.md#related-messages-and-message-details-panels).
  - Moved from #35 to later milestones, all Backlog: server-side related
    messages
    [#302](https://github.com/mikepadiernos/plasma-impresario/issues/302) and
    verified security details
    [#303](https://github.com/mikepadiernos/plasma-impresario/issues/303) (M04);
    the My Day / Agenda panel
    [#304](https://github.com/mikepadiernos/plasma-impresario/issues/304) and
    address-book contacts
    [#305](https://github.com/mikepadiernos/plasma-impresario/issues/305)
    (M06.04); the designed header
    [#306](https://github.com/mikepadiernos/plasma-impresario/issues/306) and
    sliding, floating or rearrangeable panels
    [#307](https://github.com/mikepadiernos/plasma-impresario/issues/307)
    (M14.02).
- ![Done][done] **Capability-aware actions, #215:** `Gui::CapabilityActionGate`
  hides the actions the active account's protocol cannot perform and shows them
  again on switching back: folder management as `ServerFolders`, junk and tags
  as `ServerSideFlags`, archive as `ServerSideMove`. It re-shows only actions it
  hid itself. Every account is IMAP until M06.05 (POP3 and JMAP) or M13 (Graph
  and EWS), so nothing is hidden today. `test_MainWindowShell` switches between
  an IMAP and a POP3 account.
- ![Done][done] **Account grouping, #80:** with two or more accounts,
  `Gui::AccountSidebar` shows a header per account above the active account's
  folder tree. Clicking another header switches to it; clicking the active one
  collapses or expands its folders. Up and Down move between headers, Return
  activates, and F6 treats the sidebar as one region. With one account no header
  is shown. The sidebar replaces the bare tree in both side columns and the
  one-panel layout.
- ![Done][done] **Header region, #74:** **View › Show Header** adds an optional
  header above the columns with the account, folder and account status. It is
  off by default and saved as `Appearance/ShowHeader` in `plasma-impresariorc`.
  The designed header stays in M14.02 as
  [#306](https://github.com/mikepadiernos/plasma-impresario/issues/306).
- ![Done][done] **OAuth, #218:** authorization code flow with PKCE through the
  browser and a loopback redirect, token refresh, and SASL
  `XOAUTH2`/`OAUTHBEARER` for IMAP and SMTP, with Google and Microsoft
  providers. Grants are stored per account through the password plugin and
  deleted with the account. `test_OAuth` covers sign-in, renewal, failures,
  isolation and the SASL exchanges. Live sign-in is #317 in M15; limitations are
  [caveat #362](CAVEATS.md#oauth-sign-in).

#### Open follow-ups

- ![Done][done] **Multiple accounts:** caveat
  [#262](CAVEATS.md#multiple-accounts) is resolved, closed and Merged
  ([#420](https://github.com/mikepadiernos/plasma-impresario/pull/420)).
- ![Planned][planned] **OAuth:** the M02 items of [caveat
  #362](CAVEATS.md#oauth-sign-in) are merged in
  [#421](https://github.com/mikepadiernos/plasma-impresario/pull/421). Still
  open: the Microsoft tenant and shared token session (#397, #398, M13.01), live
  sign-in (#317, M15), and the grant follow-ups #428 and #429 in M14.03.
- ![Planned][planned] **Panels:** the M02 item of
  [caveat #264](CAVEATS.md#related-messages-and-message-details-panels), pane
  sizes after upgrading, is merged in
  [#422](https://github.com/mikepadiernos/plasma-impresario/pull/422). Still
  open: #302 and #303 in M04.
- ![Planned][planned] **Resume:** #362 and #264 stay open in Review because
  their remaining items belong to later milestones. Move each to Completed and
  close it when those items are merged, then remove its bullet here.

#### Account model decisions

These decisions close the account-model questions in #36, #78, #80 and #215.

| Area                | Decision                                                                                                                                                                                                                                   | State                 |
| ------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | --------------------- |
| Account registry    | `Common::AccountRegistry` keeps the account IDs (`account-N`, never reused), display names and the active account in the application settings. The migrated single profile becomes the first account.                                      | ![Done][done] Decided |
| Account settings    | Each account has its own settings file for IMAP, offline cache, SMTP and identities. Application settings keep only application-wide options. Each settings file is editable only in its own dialog.                                       | ![Done][done] Decided |
| Connection state    | Each account has its own `ImapAccess`, model, offline cache and status. Background accounts connect at startup; switching does not reconnect.                                                                                              | ![Done][done] Decided |
| Protocol            | The account's own settings store its protocol under `account.protocol` (`imap`, `pop3` or `jmap`). Accounts without it are IMAP; an unknown value is reported and treated as IMAP. `AccountCapabilities::forAccount` reads it.             | ![Done][done] Decided |
| Identities          | Each account has its own identities by default. It can instead use the shared identities, or no identity, in which case the composer starts with an empty From address and no signature. The account's own identities are kept either way. | ![Done][done] Decided |
| Credentials         | Passwords are stored per account ID and per service (`imap`, `smtp`). New profiles use QtKeychain; profiles with legacy cleartext passwords keep the cleartext store.                                                                      | ![Done][done] Decided |
| Removal             | Removal deletes the account's settings file, registry entry, stored passwords and offline cache.                                                                                                                                           | ![Done][done] Decided |
| Folder structure    | Each account has its own folder tree. With several accounts, the sidebar is an accordion: a header per account and the active account's tree under its header (#80).                                                                       | ![Done][done] Decided |
| Unavailable actions | Actions an account's protocol does not support are hidden, not disabled, through `CapabilityActionGate` (#215). New server-dependent actions must be registered with it.                                                                   | ![Done][done] Decided |
| Mailbox lifecycle   | Accounts are created at startup and kept across switches; folders load on demand, and the selected mailbox and message are restored per account. Removal also deletes OAuth grants.                                                        | ![Done][done] Decided |

### M15 real Plasma session verification

These checks need a real KDE Plasma session, KWallet or a real provider
account, which the headless dev container cannot provide. They were the M02.02
subsection until 2026-11-25 and became their own milestone so that M02 can be
completed and merged. #319, the former M02.02 subsection, is now the M15
parent issue. M15 is blocked by M02 but does not block later milestones.

| Issue                                                                 | Feature                                                | Board   | State                                                             |
| --------------------------------------------------------------------- | ------------------------------------------------------ | ------- | ----------------------------------------------------------------- |
| [#319](https://github.com/mikepadiernos/plasma-impresario/issues/319) | M15 milestone                                          | Defined | ![Blocked][blocked] Needs a real Plasma session                   |
| [#315](https://github.com/mikepadiernos/plasma-impresario/issues/315) | Mail shell and embedded panels in Plasma (from #35)    | Defined | ![Blocked][blocked] Needs a real Plasma session                   |
| [#316](https://github.com/mikepadiernos/plasma-impresario/issues/316) | KWallet credential storage in Plasma (from #78)        | Defined | ![Blocked][blocked] Needs a real Plasma session and KWallet       |
| [#317](https://github.com/mikepadiernos/plasma-impresario/issues/317) | OAuth sign-in with a real provider account (from #218) | Defined | ![Blocked][blocked] Needs a Plasma session and a provider account |

#### How to resume

1. Install the M02 build in a Plasma 6 session on the
   [compatibility baseline](philosophy/compatibility.md), using a synthetic or
   disposable account.
2. Complete #315 and #316 by following the acceptance criteria in each issue,
   and record the Plasma, Qt and KDE Frameworks versions.
3. Complete #317 with a provider account you control; #218 is implemented.
4. Advance each issue through the board workflow, then close #319.

### M03 mail operations

- **Merged:** M03 #5 is feature-complete: all 12 roadmap items are checked
  and `feature/m03-mail-operations` is merged into `main`. The parent and its
  features #37, #38, #89, #83, #88, #84, #85, #90, #87, #82, #223 and #86 are
  Merged. M03's server-dependent actions are registered with M02's capability
  gate. In a container build of a git worktree, also mount the main
  repository's `.git` at its host path, or the version header step fails.
- **Handoff to M04:** replies, forwards and resends from the Unified Inbox
  already open with the message's account. M04's "Preserve the originating
  account and sending identity" item still covers drafts, the reader and the
  sending identity across views.
- **Pending verification:** Delete to Trash, Undo, Delete Permanently, Empty
  Trash/Junk and Rename Folder are covered by model and unit tests but have not
  been exercised in a window against a live server. Extend
  `tools/dev-container/live-imap-accounts.sh` or check them by hand with an
  isolated account. Limitations are tracked in
  [#312](https://github.com/mikepadiernos/plasma-impresario/issues/312).

### M01 release gate and integration

M01 ([#1](https://github.com/mikepadiernos/plasma-impresario/issues/1)) and all
its feature issues, including the renderer port
([#33](https://github.com/mikepadiernos/plasma-impresario/issues/33), #65–#71)
and the installer
([#57](https://github.com/mikepadiernos/plasma-impresario/issues/57)), are
Merged into `main`. M01.04 was completed from automated build, installer, launch
and privacy evidence, without an isolated account or a live Plasma session.

| Handoff                                                                                               | Moved to                                                                     | Board     | State                                                                                                                                  |
| ----------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------- | --------- | -------------------------------------------------------------------------------------------------------------------------------------- |
| Live account setup                                                                                    | M02 [#78](https://github.com/mikepadiernos/plasma-impresario/issues/78)      | Merged    | ![Done][done] Accepted against disposable Dovecot; see [M02](#m02-shell-and-account-foundation)                                        |
| Live message reading, replies and attachments                                                         | M04 [#93](https://github.com/mikepadiernos/plasma-impresario/issues/93)      | Completed | ![Done][done] Verified against disposable Dovecot with `test_LiveImapCompose`; see [live verification](#live-verification-environment) |
| Plasma desktop-session startup and recovery                                                           | M06.01 [#101](https://github.com/mikepadiernos/plasma-impresario/issues/101) | Backlog   | ![Planned][planned]                                                                                                                    |
| Renderer-enabled and renderer-free installs at protected system prefixes                              | Not assigned                                                                 | —         | ![Planned][planned] Pending                                                                                                            |
| Package lists, package-manager branches and real installs on distribution families other than CachyOS | Not assigned                                                                 | —         | ![Planned][planned] Pending; do not claim support until verified                                                                       |

<details>
<summary>Recorded M01 evidence on CachyOS</summary>

- ![Done][done] **Renderer-free:** on CachyOS x86_64 with Qt 6.12.0, CMake 4.5.1
  and GCC 16.2.1, a renderer-free build and user-prefix install succeeded; the
  installed executable's `--help`, handbook, man page, desktop entry and
  translations were present. A clean renderer-free Debug rebuild passed all
  available CTest cases with `WITH_WEBENGINE=OFF`.
- ![Done][done] **Renderer-enabled:** a Debug build with Qt 6.12.0 and the
  `qt6-webengine` 6.140.0 package passed all 35 CTest cases. Installation to a
  temporary prefix, executable startup and installer checks also passed.
- ![Done][done] **Installer:** package-manager selection follows the
  distribution identity and fails for unsupported or missing managers instead of
  selecting an unrelated one. `install.sh` supports dependency-only checks
  (`--check-deps`), build-only runs (`--build-only`) and explicit renderer
  selection (`--with-webengine`, `--without-webengine`). Its mock suite and real
  temporary-prefix renderer-free and WebEngine installations passed on CachyOS.

The [compatibility baseline](philosophy/compatibility.md) records the full
component audit and reproduction steps.

</details>

### Milestone feature-issue synchronization

- ![Done][done] **M01–M15:** each roadmap item has its own linked project card
  and issue. Items belong to their subsection issue where applicable, or
  otherwise to their milestone issue; subsection issues belong to the milestone
  issue. Issue milestone assignments and parent links were verified. Existing
  project statuses were preserved when the cards were created.
- ![Done][done] **Renumbered, 2026-11-25:** 167 issue titles and 115
  issue-body `Milestone:` lines now use roadmap milestone numbers, keeping
  their subsection suffixes. Six GitHub milestones were renamed to match the
  roadmap (M06, M07, M10, M11, M12 and M14), and eight closed, empty milestones
  were renamed "Retired: …" instead of being deleted. This replaces the closed
  reorder [#249](https://github.com/mikepadiernos/plasma-impresario/issues/249)
  and its pull request
  [#250](https://github.com/mikepadiernos/plasma-impresario/pull/250).
- ![Done][done] **Board audit, 2026-11-24:** every roadmap checkbox matches its
  issue's board status, every closed issue is Completed or Merged, and sub-issue
  milestones match their parents. Missing Start and Target dates were filled in
  from issue, pull request and commit dates for the M01 features, bugs
  #228 and #241, documentation issues #239, #249, #251, #255 and #258, and #36.
  [#49](https://github.com/mikepadiernos/plasma-impresario/issues/49) now lists
  only its own M09.01 feature and links #123 and #124.
- ![Done][done] **Parent checklists, 2026-11-24:** linked feature checkboxes in
  issue bodies match their issues' board statuses, and
  [#4](https://github.com/mikepadiernos/plasma-impresario/issues/4) now lists
  #74, #78, #79 and #80. The Merged M01 issues
  [#33](https://github.com/mikepadiernos/plasma-impresario/issues/33),
  [#34](https://github.com/mikepadiernos/plasma-impresario/issues/34) and
  [#57](https://github.com/mikepadiernos/plasma-impresario/issues/57) check the
  tasks backed by merged work and recorded evidence. Each one leaves its
  live-reading, system-prefix or other-distribution tasks unchecked, with a
  note pointing to the [M01 handoff](#m01-release-gate-and-integration).
- ![Done][done] **Wording, #87:** the M03 roadmap item now matches its issue title,
  "Provide an Aqua Mail-inspired message list", and links the
  [message list design](philosophy/designs.md#message-list-presentation).
- Keep roadmap links, issue parents, milestone assignments and project status
  synchronized when roadmap scope changes.

### M13 Microsoft milestone insertion

- ![Done][done] **Inserted, 2026-11-25**
  ([#385](https://github.com/mikepadiernos/plasma-impresario/issues/385)):
  Microsoft accounts and Exchange became M13
  ([#388](https://github.com/mikepadiernos/plasma-impresario/issues/388)) before
  Refinements, which became M14; Real Plasma session verification became M15.
  The GitHub milestone "M13: Microsoft accounts and Exchange", subsections
  #389–#395 and features #396–#417 were created in the Backlog, and #388 is
  blocked by M02
  ([#4](https://github.com/mikepadiernos/plasma-impresario/issues/4)).
- ![Done][done] **Moved:**
  [#219](https://github.com/mikepadiernos/plasma-impresario/issues/219) and
  [#220](https://github.com/mikepadiernos/plasma-impresario/issues/220) moved
  from M06.05
  ([#43](https://github.com/mikepadiernos/plasma-impresario/issues/43)) to
  M13.02 (#390) and M13.03 (#391) with their milestone and were retitled "M13:".
  #43 and #7 no longer list them.
- ![Done][done] **Renumbered:** GitHub milestone "M13: Real Plasma session
  verification" is now "M15: …", and #319 and #315–#317 were retitled "M15:".
  Roadmap-number references in the bodies of #4, #324, #329, #349, #362, #382
  and #383 were shifted, and the
  [#362](https://github.com/mikepadiernos/plasma-impresario/issues/362)
  recommendation comment links M13.01 (#397, #398).
- ![Done][done] **E-Mail checklists:** recommendations #369–#375 and #377–#383
  were added to their parent issues' feature lists.
- ![Done][done] **Merged:** #385 merged in
  [#418](https://github.com/mikepadiernos/plasma-impresario/pull/418) and is
  closed and Merged on the board.
- ![Blocked][blocked] **Microsoft app registration,
  [#396](https://github.com/mikepadiernos/plasma-impresario/issues/396):**
  needs the maintainer's Microsoft account, so it cannot be done here. The
  plan is in [App registration](integrations/microsoft.md#app-registration).
  To resume:
  1. Use a work or school Microsoft Entra tenant with a DNS-verified project
     domain; an app registered with a personal Microsoft account cannot be
     publisher-verified later.
  2. Register "Plasma Impresario" for accounts in any organizational
     directory and personal Microsoft accounts, as a mobile and desktop
     client with the `http://localhost` redirect and public client flows
     allowed, and add the delegated permissions listed in the guide.
  3. Record the client ID for release builds through
     `IMPRESARIO_OAUTH_MICROSOFT_CLIENT_ID`; it is not a secret.
  4. Ask the KDE PIM developers, such as Daniel Vrátil, in the KDE PIM Matrix
     room whether KMail's and the EWS resource's client IDs are
     publisher-verified and which tenant holds them.
  5. Ask KDE e.V. whether it can host or publisher-verify the app, and record
     both answers in #396.
- ![Planned][planned] **Resume:** remove this section when #396 is done.

[blocked]: https://img.shields.io/badge/Blocked-cf222e
[done]: https://img.shields.io/badge/Done-1a7f37
[in-progress]: https://img.shields.io/badge/In_progress-9a6700
[needs-verification]: https://img.shields.io/badge/Needs_verification-0969da
[planned]: https://img.shields.io/badge/Planned-6e7781
