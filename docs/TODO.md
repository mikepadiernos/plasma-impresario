# Pending tasks

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/4)** ·
**[Bugs](BUGS.md)** · **[Caveats](CAVEATS.md)** · **[Edge cases](EDGE.md)**

This log records unfinished work, blockers and resumable handoffs. Issue
statuses below match the project board as of 2026-11-24.

Legend: ✅ done or verified · 🧪 implemented, needs verification · 🚧 in
progress · 📋 planned · ⛔ blocked

> [!IMPORTANT]
>
> Confirm the current issue and project status before acting. An issue is done
> only when its board status is **Completed** or **Merged**. **Review** is not
> done, and **Completed** does not mean merged into `main`.

## Contents

- [Backlog](#backlog)
- [In progress](#in-progress)
- [Pending verification](#pending-verification)
  - [M02 shell and account foundation](#m02-shell-and-account-foundation)
  - [M13 real Plasma session verification](#m13-real-plasma-session-verification)
  - [M01 release gate and integration](#m01-release-gate-and-integration)
  - [Milestone feature-issue synchronization](#milestone-feature-issue-synchronization)

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

- 🚧 **M02 shell, accounts and mailbox foundation**
  ([#4](https://github.com/mikepadiernos/plasma-impresario/issues/4)): work
  continues on `feature/m02-shell-next`, which is not merged into `main`. See
  the [M02 handoff](#m02-shell-and-account-foundation).
- 🚧 **M03 mail operations, sorting and folder management**
  ([#5](https://github.com/mikepadiernos/plasma-impresario/issues/5)): in
  Development on `feature/m03-mail-operations`, which is based on this branch
  and holds the detailed M03 handoff. Folder navigation
  ([#37](https://github.com/mikepadiernos/plasma-impresario/issues/37)),
  sorting and triage
  ([#38](https://github.com/mikepadiernos/plasma-impresario/issues/38)), quick
  actions ([#83](https://github.com/mikepadiernos/plasma-impresario/issues/83))
  and destructive-operation recovery
  ([#89](https://github.com/mikepadiernos/plasma-impresario/issues/89)) are
  Completed; batch selection and feedback
  ([#88](https://github.com/mikepadiernos/plasma-impresario/issues/88)) is in
  Review and power-user workflows
  ([#84](https://github.com/mikepadiernos/plasma-impresario/issues/84)) in
  Development. The other six features are Defined.
- 🚧 **M08 themes and branding foundation**
  ([#10](https://github.com/mikepadiernos/plasma-impresario/issues/10)): in
  Development because M08.01 Branding and visual language
  ([#269](https://github.com/mikepadiernos/plasma-impresario/issues/269))
  started with the product rename
  ([#221](https://github.com/mikepadiernos/plasma-impresario/issues/221)), which
  is Merged. The other M08.01 features and M08.02–M08.10 are in the Backlog.
- 🚧 **Documentation:** keep the roadmap and these documents synchronized with
  project-board decisions. See
  [issue synchronization](#milestone-feature-issue-synchronization).

## Pending verification

### M02 shell and account foundation

All M02 work is on `feature/m02-shell-next`, which already contains the earlier
`feature/m02-shell-accounts` and `feature/m02-shell-continuation` branches. It
is not merged into `main`. Its features are in **M02.01 Headless
implementation and verification**
([#318](https://github.com/mikepadiernos/plasma-impresario/issues/318)): all
13 features, built and verified with offscreen QtTest and the synthetic Dovecot
server in `tools/dev-container/`.

The checks that need a real Plasma session, KWallet or a real provider account
were the M02.02 subsection. They moved to their own milestone,
[M13](#m13-real-plasma-session-verification), on 2026-11-25 so they do not
block M02.

#### Issue status

| Issue                                                                 | Feature                                                     | Board       | State                                                                                        |
| --------------------------------------------------------------------- | ----------------------------------------------------------- | ----------- | -------------------------------------------------------------------------------------------- |
| [#4](https://github.com/mikepadiernos/plasma-impresario/issues/4)     | M02 milestone                                               | Development | 🚧 Waits on the open features below                                                          |
| [#318](https://github.com/mikepadiernos/plasma-impresario/issues/318) | M02.01 Headless implementation and verification             | Development | 🚧 Parent of the 13 features below                                                           |
| [#35](https://github.com/mikepadiernos/plasma-impresario/issues/35)   | Mail shell                                                  | Completed   | ✅ Verified headless; Plasma-session check moved to #315                                     |
| [#74](https://github.com/mikepadiernos/plasma-impresario/issues/74)   | App skeleton in the design-spec layout                      | Development | ⏸️ Held while M03 is active; needs a layout decision ([held work](#m02-work-held-for-m03))   |
| [#36](https://github.com/mikepadiernos/plasma-impresario/issues/36)   | Account setup and mailbox state                             | Development | 🚧 Decisions recorded in [account model](#account-model-decisions)                           |
| [#78](https://github.com/mikepadiernos/plasma-impresario/issues/78)   | Account setup, editing, removal, identities and credentials | Review      | 👀 Setup, editing, removal, identities and credential default verified; KWallet in #316      |
| [#80](https://github.com/mikepadiernos/plasma-impresario/issues/80)   | Account grouping, folder navigation and keyboard focus      | Development | ⏸️ Held while M03 is active; needs a grouping decision ([held work](#m02-work-held-for-m03)) |
| [#215](https://github.com/mikepadiernos/plasma-impresario/issues/215) | Protocol-aware account and capability model                 | Development | 🚧 Protocol stored per account; interface use held ([held work](#m02-work-held-for-m03))     |
| [#218](https://github.com/mikepadiernos/plasma-impresario/issues/218) | OAuth account authentication                                | Defined     | 📋 Not started; live provider check moved to #317 (see below)                                |
| [#75](https://github.com/mikepadiernos/plasma-impresario/issues/75)   | Density modes                                               | Completed   | ✅ Row-density selector                                                                      |
| [#76](https://github.com/mikepadiernos/plasma-impresario/issues/76)   | Folder counts and saved searches                            | Completed   | ✅ Verified against disposable IMAP                                                          |
| [#77](https://github.com/mikepadiernos/plasma-impresario/issues/77)   | Milestone gate for later mailbox work                       | Completed   | ✅                                                                                           |
| [#79](https://github.com/mikepadiernos/plasma-impresario/issues/79)   | Connection, authentication, offline and loading states      | Completed   | ✅ Footer and per-account status                                                             |
| [#81](https://github.com/mikepadiernos/plasma-impresario/issues/81)   | Pane layout, selection and density across sessions          | Completed   | ✅ Verified against disposable IMAP                                                          |
| [#222](https://github.com/mikepadiernos/plasma-impresario/issues/222) | Multiple simultaneous accounts                              | Completed   | ✅ Verified against disposable IMAP; caveat [#262](CAVEATS.md#multiple-accounts)             |

Completed issues move to Merged when `feature/m02-shell-next` is merged into
`main`. The Plasma-session checks split from #35, #78 and #218 are tracked in
[M13](#m13-real-plasma-session-verification).

#### Live verification environment

> [!WARNING]
>
> The live tests are opt-in and use only loopback-only Dovecot servers with
> synthetic users and the synthetic password `synthetic-test-only`. Never point
> them at a real account or service.

| Check                   | How to run                                                                                     | Server                                                                                | Verifies                                                |
| ----------------------- | ---------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------- | ------------------------------------------------------- |
| `test_LiveImap`         | Set `PLASMA_TEST_IMAPS_PORT` to a loopback-only Dovecot IMAPS listener                         | Dovecot 2.4 in the dev container; 2.4.5 at the recorded account-settings run          | #78, #81, #76                                           |
| `test_LiveImapAccounts` | `tools/dev-container/run.sh sh tools/dev-container/live-imap-accounts.sh` in the dev container | Loopback-only Dovecot 2.4 with synthetic users; 2.4.5 at the recorded run             | #222, #79, per-account #76, #78 removal and credentials |
| `test_LiveImapPanels`   | `tools/dev-container/run.sh sh tools/dev-container/live-imap-panels.sh`                        | Same Dovecot 2.4 helper, seeded with a conversation, attachments and a signed message | #35 embedded panels, screenshots                        |
| VNC view                | `tools/dev-container/vnc.sh`                                                                   | The same seeded mailbox, shown in a VNC session                                       | #35 by hand                                             |

The dev container pins Fedora 44, and `synthetic-dovecot.sh` refuses any Dovecot
outside the 2.4 series, which its configuration requires. It prints the exact
version on each run; record it with new results. The dev container has no Plasma
session, so D-Bus services, KWallet, notifications and the Plasma look still
need a real session.

| Variable                                                                                 | Used by               | Purpose                                                                                                                                                        |
| ---------------------------------------------------------------------------------------- | --------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `PLASMA_TEST_IMAPS_PORT`                                                                 | All live tests        | Loopback IMAPS port. The dev-container scripts set it.                                                                                                         |
| `PLASMA_TEST_IMAP_RESTORE_UID`, `PLASMA_TEST_IMAP_RESTORE_VALIDITY`                      | `test_LiveImap`       | Include message restoration. Seed INBOX with a synthetic message with this UID and UIDVALIDITY.                                                                |
| `PLASMA_TEST_IMAP_RESTART_PHASE` (`write`, then `read`), `PLASMA_TEST_IMAP_RESTART_HOME` | `test_LiveImap`       | Check restoration across two processes. Requires the restore variables; the home must be a temporary directory named `plasma-impresario-imap-restart-*`.       |
| `PLASMA_TEST_IMAP_SEARCH_SUBJECT`                                                        | `test_LiveImap`       | Check folder counts and saved-search filtering. Requires the matching message's UID and UIDVALIDITY; INBOX must contain exactly two unread synthetic messages. |
| `PLASMA_TEST_SCREENSHOT_DIR`                                                             | `test_LiveImapPanels` | Save layout screenshots. `live-imap-panels.sh` defaults to `<build-dir>/shell-screenshots`.                                                                    |

#### Verification record

- ✅ **Account setup, #78:** against the disposable Dovecot 2.4.4 container, the
  app saved IMAP settings, recovered from a rejected password, selected INBOX
  and reconnected after going offline.
- ✅ **Account settings, #78:** **Application › Account Settings…** opens a
  dialog for the active account with the Identities, IMAP, Offline and SMTP
  pages, titled with the account name. **Settings…** now holds only the
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
  - The dialog edits only the active account; see
    [caveat #262](CAVEATS.md#multiple-accounts).
- ✅ **Credential defaults and removal, #78:** verified with
  `test_LiveImapAccounts` against Dovecot 2.4.5. Removing a connected
  background account and then the active account deleted each one's settings
  file and stored password, and the remaining account took over with its own
  stored password. A new profile selected QtKeychain, signed in through the password
  prompt when no secret store was available, and wrote no password to any
  settings file. Storage in KWallet still needs a real session (#316).
- 🧪 **Sender identities, #78:** each account has its own identities by
  default, edited on the Identities tab of the Account Settings dialog. An account can
  instead use the identities shared by all accounts; it keeps its own list for
  later. An account can also send with **No identity**: the composer then
  starts with an empty From address, which is typed by hand, and adds no
  signature or organization; the account's own list is kept, read-only, for
  when it switches back. The **Send from** list on the Identities tab offers
  the three choices. On first start, existing accounts receive a copy of the
  previously shared identities, which remain the shared list. New accounts
  start with no identities, and the composer then accepts a typed From
  address. A composer keeps the identities of the account it was opened from.
  `test_AccountIdentities` and `test_MainWindowAccounts` cover the defaults,
  the shared and no-identity options, migration and switching;
  `test_AccountSettingsDialog` chooses **No identity** on the Identities tab
  and checks that only the edited account changes.
- ✅ **Session selection, #81:** the app saves a message's account, mailbox,
  UIDVALIDITY and UID, restores it after asynchronous mailbox loading, and
  cancels pending restoration on navigation. Live checks passed restoration,
  rapid offline/reconnect and restoration across application restarts, with and
  without WebEngine. Separate-process tests verify saved selection identifiers,
  pane proportions in wide, compact and one-panel layouts, sidebar position and
  density across close, quit and hidden-window quit.
- 🧪 **Saved selection per account, #81 and #222:** isolation of saved
  selections between accounts is not recorded as verified.
- ✅ **Folder counts and saved searches, #76:** folders show unread indicators,
  and tooltips show total, unread and recent counts; unknown counts are omitted
  rather than reported as zero. The search options menu saves, runs and deletes
  named searches for the active account and selected folder. Verified with two
  synthetic messages, with and without WebEngine, and per account with three
  accounts in `test_LiveImapAccounts`.
- ✅ **Multiple accounts, #222 and #79:** each account has its own settings file
  and `ImapAccess`. A toolbar selector, menu actions and
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
  - Known limits are recorded in [caveat #262](CAVEATS.md#multiple-accounts).
- ✅ **Mail shell, #35 (Completed):** the shell has a left column, main
  column, right column and footer, with connection status in the footer. The
  Navigation toolbar docks at the left window edge by default, and the menu bar
  is hidden by default. The mailbox sidebar can sit in either side column; the
  message list and reader can be side by side, stacked or shown one panel at a
  time.
  - View › Embedded Panels has a submenu per panel: **Show Panel**, **Hide When
    Empty** and a position below the reader, in either side column or in the
    footer. Side columns and the footer appear only while they hold the sidebar
    or a visible panel, and remember their sizes.
  - Attachments is on below the reader by default; Message Details, Contacts and
    Related Messages are off by default in the right column, so the default look
    is unchanged.
  - <kbd>F6</kbd> and <kbd>Shift</kbd>+<kbd>F6</kbd> cycle focus through the
    account selector, folder tree, message list, reader and each visible
    embedded panel.
  - Offscreen tests cover placement, sidebar swaps, hiding empty regions,
    one-panel layout and size persistence.
  - `test_LiveImapPanels` verified the details, contacts, related messages and
    attachments panels and saves layout screenshots. It found that Contacts
    listed the sender twice when a message has no Reply-To header (fixed). The
    screenshots showed panels squeezed to a sliver when sharing a column,
    attachments below the reader showing one row, and a tall footer; panels now
    start at a size for a few rows and keep a size the user drags them to.
  - Saved pane sizes reset once after upgrading; see
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
    (M05.04); the designed header
    [#306](https://github.com/mikepadiernos/plasma-impresario/issues/306) and
    sliding, floating or rearrangeable panels
    [#307](https://github.com/mikepadiernos/plasma-impresario/issues/307)
    (M12.02).

#### Blockers and open decisions

- ⛔ **OAuth verification, #218:** the work is not started, so the issue stays
  in Defined. The IMAP layer has no XOAUTH2 or OAUTHBEARER authentication and
  there is no token source; both can be implemented and covered by parser tests
  KAccounts and signond are installed, but token retrieval and renewal
  cannot be verified without a real Google or Microsoft account, which needs
  your input. That check is tracked in M13 as
  [#317](https://github.com/mikepadiernos/plasma-impresario/issues/317).
- 🚧 **Account model, #36, #78 and #215:** decided and implemented as
  recorded in [account model decisions](#account-model-decisions). The
  interface does not consult the capabilities yet; that work is held (below).
- ⏸️ **Held while M03 is active:** #80, #74 and the interface part of #215;
  see [M02 work held for M03](#m02-work-held-for-m03).
- ➡️ **Plasma session:** the shell (#315, from #35), KWallet storage (#316,
  from #78) and OAuth sign-in (#317, from #218) checks moved to
  [M13](#m13-real-plasma-session-verification) and no longer block M02.

#### Account model decisions

These decisions close the open account-model questions in #36, #78 and #215.

| Area                | Decision                                                                                                                                                                                                                                   | State      |
| ------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ---------- |
| Account registry    | `Common::AccountRegistry` keeps the account IDs (`account-N`, never reused), display names and the active account in the application settings. The migrated single profile becomes the first account.                                      | ✅ Decided |
| Account settings    | Each account has its own settings file for IMAP, offline cache, SMTP and identities. Application settings keep only application-wide options. Each settings file is editable only in its own dialog.                                       | ✅ Decided |
| Connection state    | Each account has its own `ImapAccess`, model, offline cache and status. Background accounts connect at startup; switching does not reconnect.                                                                                              | ✅ Decided |
| Protocol            | The account's own settings store its protocol under `account.protocol` (`imap`, `pop3` or `jmap`). Accounts without it are IMAP; an unknown value is reported and treated as IMAP. `AccountCapabilities::forAccount` reads it.             | ✅ Decided |
| Identities          | Each account has its own identities by default. It can instead use the shared identities, or no identity, in which case the composer starts with an empty From address and no signature. The account's own identities are kept either way. | ✅ Decided |
| Credentials         | Passwords are stored per account ID and per service (`imap`, `smtp`). New profiles use QtKeychain; profiles with legacy cleartext passwords keep the cleartext store.                                                                      | ✅ Decided |
| Removal             | Removal deletes the account's settings file, registry entry and stored passwords. The offline cache is kept for now ([caveat #262](CAVEATS.md#multiple-accounts)).                                                                         | ✅ Decided |
| Folder structure    | Each account has its own folder tree, shown for the active account only. Showing all accounts in one tree is #80 and needs your decision.                                                                                                  | ❓ Ask     |
| Unavailable actions | Actions an account's protocol does not support should be hidden, not disabled. Wiring this into the interface waits until M03's actions settle; until POP3 or JMAP exist (M05.05), every account is IMAP.                                  | ❓ Confirm |

#### M02 work held for M03

M03 is in development in `.worktrees/m03` on `feature/m03-mail-operations` and
rewrites much of `Window.cpp`, the folder tree models and the message list. To
avoid conflicting changes, these M02 items are held:

- ⏸️ **Account grouping in the folder tree, #80:** changes `MailboxTree`,
  `PrettyMailboxModel` and `Window.cpp`, all of which M03 changes. Needs your
  decision: one tree with a collapsible node per account, or the current
  selector plus a per-account tree.
- ⏸️ **App skeleton in the design-spec layout, #74:** changes the main window
  layout that M03 also changes. Needs your decision on which design-spec layout
  in [designs](philosophy/designs.md) to follow and what it adds beyond the
  shell from #35.
- ⏸️ **Capability-aware actions, #215:** hides actions that the account's
  protocol does not support. Most of those actions are added or changed by M03,
  so this waits until M03 is merged.
- ⏸️ **Merge M02 into M03:** the merge is prepared and tested on
  `merge/m02-into-m03` (pushed). It is M03 at `50f6f2d0` with M02 at `c0719cfa`
  merged in. The conflicts in `CMakeLists.txt`, `src/Common/SettingsNames.h`
  and `tests/Gui/test_MainWindowAccounts.cpp` keep both sides. M03's caveat,
  edge cases and design notes moved into M02's sectioned tables in
  `docs/CAVEATS.md`, `docs/EDGE.md` and `docs/philosophy/designs.md` (new
  _Mail operations_ sections). All 88 tests pass, and `test_LiveImapAccounts`
  passes against Dovecot 2.4.5. The M03 worktree had new uncommitted work
  (`ViewStateStore`), so `feature/m03-mail-operations` was not changed. When
  that work is committed, run `git merge merge/m02-into-m03` in the M03
  worktree; any new conflicts are limited to files M03 changed since
  `50f6f2d0`. Merge in that direction only, then delete the branch. In a
  container build of a worktree, also mount the main repository's `.git` at
  its host path, or the version header step fails.

#### How to resume

1. Confirm the board status of every issue in the table above.
2. M02.01, headless: finish #74, #80 and the interface part of #215 using the
   decisions above. M03 cannot reach `main` before M02, because #4 blocks later
   milestones, so decide where this work happens: on
   `feature/m03-mail-operations` after merging M02 into it, or on
   `feature/m02-shell-next` once M03 is quiet.
3. For #218, add SASL support with parser tests and a token interface backed by
   KAccounts.
4. Advance each issue through Completed before checking its roadmap item.
5. Reconcile the pending roadmap changes in the open pull requests
   [#250](https://github.com/mikepadiernos/plasma-impresario/pull/250)
   (milestone reorder,
   [#249](https://github.com/mikepadiernos/plasma-impresario/issues/249)) and
   [#252](https://github.com/mikepadiernos/plasma-impresario/pull/252) (feature
   priority,
   [#251](https://github.com/mikepadiernos/plasma-impresario/issues/251)). Both
   issues are in Review.
6. Merge `feature/m02-shell-next` into `main` only when all M02 roadmap items
   and acceptance checks are complete. The M13 Plasma-session checks are not
   required.

### M13 real Plasma session verification

These checks need a real KDE Plasma session, KWallet or a real provider
account, which the headless dev container cannot provide. They were the M02.02
subsection until 2026-11-25 and became their own milestone so that M02 can be
completed and merged. #319, the former M02.02 subsection, is now the M13
parent issue. M13 is blocked by M02 but does not block later milestones.

| Issue                                                                 | Feature                                                | Board   | State                                                  |
| --------------------------------------------------------------------- | ------------------------------------------------------ | ------- | ------------------------------------------------------ |
| [#319](https://github.com/mikepadiernos/plasma-impresario/issues/319) | M13 milestone                                          | Defined | ⛔ Needs a real Plasma session                         |
| [#315](https://github.com/mikepadiernos/plasma-impresario/issues/315) | Mail shell and embedded panels in Plasma (from #35)    | Defined | ⛔ Needs a real Plasma session                         |
| [#316](https://github.com/mikepadiernos/plasma-impresario/issues/316) | KWallet credential storage in Plasma (from #78)        | Defined | ⛔ Needs a real Plasma session and KWallet             |
| [#317](https://github.com/mikepadiernos/plasma-impresario/issues/317) | OAuth sign-in with a real provider account (from #218) | Defined | ⛔ Needs #218, a Plasma session and a provider account |

#### How to resume

1. Install the M02 build in a Plasma 6 session on the
   [compatibility baseline](philosophy/compatibility.md), using a synthetic or
   disposable account.
2. Complete #315 and #316 by following the acceptance criteria in each issue,
   and record the Plasma, Qt and KDE Frameworks versions.
3. Complete #317 after #218 is implemented, with a provider account you
   control.
4. Advance each issue through the board workflow, then close #319.

### M01 release gate and integration

M01 ([#1](https://github.com/mikepadiernos/plasma-impresario/issues/1)) and all
its feature issues, including the renderer port
([#33](https://github.com/mikepadiernos/plasma-impresario/issues/33), #65–#71)
and the installer
([#57](https://github.com/mikepadiernos/plasma-impresario/issues/57)), are
Merged into `main`. M01.04 was completed from automated build, installer, launch
and privacy evidence, without an isolated account or a live Plasma session.

| Handoff                                                                                               | Moved to                                                                     | Board       | State                                                                                |
| ----------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------- | ----------- | ------------------------------------------------------------------------------------ |
| Live account setup                                                                                    | M02 [#78](https://github.com/mikepadiernos/plasma-impresario/issues/78)      | Development | 🚧 Accepted against disposable Dovecot; see [M02](#m02-shell-and-account-foundation) |
| Live message reading, replies and attachments                                                         | M04 [#93](https://github.com/mikepadiernos/plasma-impresario/issues/93)      | Backlog     | 📋                                                                                   |
| Plasma desktop-session startup and recovery                                                           | M05.01 [#101](https://github.com/mikepadiernos/plasma-impresario/issues/101) | Backlog     | 📋                                                                                   |
| Renderer-enabled and renderer-free installs at protected system prefixes                              | Not assigned                                                                 | —           | 📋 Pending                                                                           |
| Package lists, package-manager branches and real installs on distribution families other than CachyOS | Not assigned                                                                 | —           | 📋 Pending; do not claim support until verified                                      |

<details>
<summary>Recorded M01 evidence on CachyOS</summary>

- ✅ **Renderer-free:** on CachyOS x86_64 with Qt 6.12.0, CMake 4.5.1 and GCC
  16.2.1, a renderer-free build and user-prefix install succeeded; the installed
  executable's `--help`, handbook, man page, desktop entry and translations were
  present. A clean renderer-free Debug rebuild passed all available CTest cases
  with `WITH_WEBENGINE=OFF`.
- ✅ **Renderer-enabled:** a Debug build with Qt 6.12.0 and the `qt6-webengine`
  6.140.0 package passed all 35 CTest cases. Installation to a temporary prefix,
  executable startup and installer checks also passed.
- ✅ **Installer:** package-manager selection follows the distribution identity
  and fails for unsupported or missing managers instead of selecting an
  unrelated one. `install.sh` supports dependency-only checks (`--check-deps`),
  build-only runs (`--build-only`) and explicit renderer selection
  (`--with-webengine`, `--without-webengine`). Its mock suite and real
  temporary-prefix renderer-free and WebEngine installations passed on CachyOS.

The [compatibility baseline](philosophy/compatibility.md) records the full
component audit and reproduction steps.

</details>

### Milestone feature-issue synchronization

- ✅ **M01–M12:** each roadmap item has its own linked project card and issue.
  Items belong to their subsection issue where applicable, or otherwise to their
  milestone issue; subsection issues belong to the milestone issue. Issue
  milestone assignments and parent links were verified. Existing project
  statuses were preserved when the cards were created.
- 🚧 **Milestone numbers:** several board issue titles still use earlier
  milestone numbers. For example, roadmap M05 is
  [#7](https://github.com/mikepadiernos/plasma-impresario/issues/7) "M07:
  Desktop integration and provider support", M09 is
  [#11](https://github.com/mikepadiernos/plasma-impresario/issues/11) "M05:
  Reliability, sync, and notification quality", and #304 and #305 are titled
  "M07:" but belong to roadmap M05.04. The milestone reorder
  [#249](https://github.com/mikepadiernos/plasma-impresario/issues/249) is in
  Review in pull request
  [#250](https://github.com/mikepadiernos/plasma-impresario/pull/250). Treat the
  roadmap numbering as current until the titles are updated.
- ✅ **Board audit, 2026-11-24:** every roadmap checkbox matches its issue's
  board status, every closed issue is Completed or Merged, and sub-issue
  milestones match their parents. Missing Start and Target dates were filled
  in from issue, pull request and commit dates for the M01 features, bugs
  #228 and #241, documentation issues #239, #249, #251, #255 and #258, and #36.
  [#49](https://github.com/mikepadiernos/plasma-impresario/issues/49) now lists
  only its own M08.01 feature and links #123 and #124.
- ✅ **Parent checklists, 2026-11-24:** linked feature checkboxes in issue
  bodies match their issues' board statuses, and
  [#4](https://github.com/mikepadiernos/plasma-impresario/issues/4) now lists
  #74, #78, #79 and #80. The Merged M01 issues
  [#33](https://github.com/mikepadiernos/plasma-impresario/issues/33),
  [#34](https://github.com/mikepadiernos/plasma-impresario/issues/34) and
  [#57](https://github.com/mikepadiernos/plasma-impresario/issues/57) check the
  tasks backed by merged work and recorded evidence. Each one leaves its
  live-reading, system-prefix or other-distribution tasks unchecked, with a
  note pointing to the [M01 handoff](#m01-release-gate-and-integration).
- 📋 **Pending wording, #87:** M03 issue
  [#87](https://github.com/mikepadiernos/plasma-impresario/issues/87) is
  titled "Provide an Aqua Mail-inspired message list", while its roadmap item
  still reads "Provide readable compact message rows…". The open pull request
  [#257](https://github.com/mikepadiernos/plasma-impresario/pull/257) rewords
  the roadmap item to match; keep both as they are until it is merged or
  closed.
- Keep roadmap links, issue parents, milestone assignments and project status
  synchronized when roadmap scope changes.
