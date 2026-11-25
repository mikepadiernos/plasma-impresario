# Pending tasks

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/4)** ·
**[Bugs](BUGS.md)** · **[Caveats](CAVEATS.md)**

This log records unfinished work and resumable handoffs. Confirm current issue
and project status before acting.

## Backlog

- [ ] Validate the [initial compatibility target](philosophy/compatibility.md) with a
      complete reader and desktop session before declaring release support
- [ ] Define the minimum viable account and mailbox acceptance scenarios
- [ ] Verify the main window, message list and preview layout in an end-to-end flow
- [ ] Add implementation-backed acceptance criteria to planned notification,
      search and mailbox-organization features

## In progress

- Review the documentation structure and keep milestone status synchronized
  with project-board decisions.
- M01 installer: package-manager selection now follows distribution identity
  and fails for unsupported or missing managers instead of selecting an
  unrelated one. The installer supports dependency-only checks, build-only
  builds and explicit renderer selection. Its mock suite and real temporary-prefix
  renderer-free/WebEngine installations passed on CachyOS. Package lists and
  real installations still need verification across supported distribution families.
- M01.02 renderer: merged and complete. The WebEngine reader is integrated with
  MIME parts, alternatives, attachments, headers, source, asynchronous quoting
  and search. Issues #33 and #65-#71 are Merged.
- M01 verification: on CachyOS x86_64 with Qt 6.12.0, CMake 4.5.1 and GCC
  16.2.1, a renderer-free build and user-prefix install succeeded; the
  installed executable's `--help`, handbook, man page, desktop entry and
  translations were present. A renderer-enabled Debug build using Qt 6.12.0 and
  Qt WebEngine 6.140.0 passed all 35 CTest cases. Installation to a temporary
  prefix, executable startup, and installer checks also passed. Account setup,
  live message reading, replies, attachments, and Plasma-session behavior
  remain unverified. The clean renderer-free Debug build was also rebuilt and
  passed all available CTest cases with `WITH_WEBENGINE=OFF`.

## Pending verification

### M02 shell and account foundation

- **Current state:** M02 #4, shell #35, skeleton #74, account setup #78,
  account foundation #36, shell navigation #80 and protocol-aware account #215
  are in Development. Connection status #79, folder counts and saved
  searches #76, multiple accounts #222, session persistence #81, density #75
  and dependency gate #77 are Completed; OAuth #218 is Defined. The
  `feature/m02-shell-accounts` branch adds a row-density selector; the
  continuation branch adds left/right folder sidebar placement in the compact
  and wide layouts. A further continuation adds persistent connection status
  in the footer. Multiple accounts, per-account status and keyboard account
  switching are described below; F6 and Shift+F6 cycle focus between the
  account switcher, folder tree, message list, reader and attachments panel.
  Shell #35 is implemented on `feature/m02-shell-next` and awaits review;
  the My Day / Agenda panel and OAuth are not implemented.
- **Account acceptance:** #78 has been exercised against a disposable Dovecot
  2.4.4 container using temporary configuration and synthetic credentials.
  The application saved IMAP settings, recovered from a rejected password,
  selected INBOX and reconnected after going offline. New profiles default to
  QtKeychain, and account removal deletes the account's settings and stored
  passwords; both still need verification against disposable IMAP.
  The opt-in `test_LiveImap` executable uses `PLASMA_TEST_IMAPS_PORT` for a
  loopback-only Dovecot IMAPS listener configured with the synthetic password
  `synthetic-test-only`; never point it at a real account or service.
- **Session selection:** #81 saves a message's account, mailbox, UIDVALIDITY
  and UID, restores it after asynchronous mailbox loading, and cancels pending
  restoration on navigation. Synthetic live IMAP checks exercised restoration
  and rapid offline/reconnect with and without WebEngine. To include message
  restoration in `test_LiveImap`, seed INBOX with a synthetic message and set
  `PLASMA_TEST_IMAP_RESTORE_UID` and `PLASMA_TEST_IMAP_RESTORE_VALIDITY`.
  Separate-process tests verify saved selection identifiers, pane proportions
  in wide, compact and one-panel layouts, sidebar position and density across
  close, quit and hidden-window quit. End-to-end message restoration across
  application restarts passed against disposable IMAP with and without
  WebEngine. Multi-account isolation remains to be verified with #222.
- **Folder counts and saved searches:** #76 has existing unread indicators and
  total, unread and recent counts in folder tooltips. Unknown counts are omitted
  rather than reported as zero. The search options menu saves, runs and deletes
  named searches for the active account profile and selected folder. Folder
  counts and saved-search filtering were verified with two synthetic messages
  against disposable IMAP, with and without WebEngine. The opt-in live test
  uses `PLASMA_TEST_IMAP_SEARCH_SUBJECT` with the matching synthetic UID and
  UIDVALIDITY; its INBOX must contain exactly two unread synthetic messages.
  Per-account folders, counts and saved searches were verified with three
  accounts in `test_LiveImapAccounts` (below).
- **Multiple accounts:** #222 and #79 are implemented on
  `feature/m02-shell-next`. Each account has its own settings file and
  `ImapAccess`; a toolbar selector, menu actions and Ctrl+Alt+PgUp/PgDown
  switch accounts without reconnecting. Accounts other than the active one
  connect at startup, and their connection, network and authentication status
  appears in the selector. Cleartext passwords are kept per account, new
  profiles default to the QtKeychain backend, and removing an account deletes
  its settings and stored passwords. Saved searches and folder expansion
  follow the active account; only the migrated legacy account inherits the
  previously shared expansion state. Offscreen tests cover migration,
  switching, background status, removal, keyboard navigation, per-account
  saved searches and expansion, and pane focus cycling. The opt-in
  `test_LiveImapAccounts`, run by `tools/dev-container/live-imap-accounts.sh`
  inside the dev container, starts a loopback-only Dovecot 2.4.5 with
  synthetic users. It verified that two background accounts connect at
  startup, that a rejected stored password is flagged in the selector without
  a prompt, and that switching keeps each account's folders, counts and saved
  searches without reconnecting. That check found and fixed two bugs:
  background accounts never logged in because the model connects lazily, and
  certificate prompts were answered for the active account instead of the
  requesting one. Not yet verified: a real Plasma session. Known limits:
  accounts are grouped by a selector, not in the folder tree; background
  accounts surface errors only in the selector; each account asks about the
  same untrusted certificate separately; the offline message cache of a
  removed account is not deleted. See [caveat #262](CAVEATS.md#multiple-accounts).
- **Implemented, #35:** the shell has a left column, main column, right
  column and footer below the header toolbar. The mailbox sidebar can sit in
  either side column, and the message list and reader can be stacked or side
  by side. View › Embedded Panels has a submenu per panel to show it, hide it
  when it has nothing to show, and place it below the reader, in either side
  column or in the footer. Side columns and the footer appear only while they
  hold the sidebar or a visible panel, and remember their sizes. Attachments
  stays on below the reader by default; the new message details (metadata),
  contact and related messages panels are off by default in the right column,
  so the default look is unchanged. Offscreen tests cover placement, sidebar
  swaps, hiding empty regions, one-panel layout and size persistence. The
  opt-in `test_LiveImapPanels`, run by
  `tools/dev-container/live-imap-panels.sh`, verified the details, contact,
  related messages and attachments panels against a synthetic Dovecot
  mailbox with a conversation, attachments and a signed message, and saves
  layout screenshots. It found that Contacts listed the sender twice when a
  message has no Reply-To header, which is fixed. The screenshots also showed
  panels squeezed to a sliver when sharing a column, attachments below the
  reader showing one row, and a tall footer; panels now start at a size for a
  few rows, and each keeps a size the user drags it to. `tools/dev-container/vnc.sh`
  opens the same mailbox in a VNC view for manual checks. Not verified: a
  real Plasma session. Saved pane sizes reset once after upgrading; see
  [caveat #264](CAVEATS.md#related-messages-and-message-details-panels).
  Remaining for #35 before Completed: verification in a real Plasma session.
  Moved from #35 to other milestones (all Backlog): server-side related
  messages #302 and verified security details #303 (M04); the My Day / Agenda
  panel #304 and address-book contacts #305 (M05.04); the designed header #306
  and sliding, floating or rearrangeable panels #307 (M12.02).
- **Blocked, #218:** OAuth needs XOAUTH2 or OAUTHBEARER authentication in the
  IMAP layer, which has none, and a token source. KAccounts and signond are
  installed, but token retrieval and renewal cannot be verified without a
  real Google or Microsoft account. Steps to resume: add SASL support with
  parser tests, add a token interface backed by KAccounts, then verify with an
  account you control.
- **Open, #78, #36 and #215:** `AccountCapabilities` exists but nothing in the
  interface consults it yet; POP3 and JMAP belong to M05.05. Account editing uses
  the settings dialog; per-account identities are not separate from the shared
  sender identities.
- **Resume:** #79 and #222 are Completed. #80 still needs account grouping in
  the folder tree before Review. Resolve the decisions above, then
  implement the remaining issues. Advance each issue through Completed before
  checking its roadmap item; merge the milestone only when all items and
  acceptance checks are complete. Reconcile the pending roadmap changes in
  #250 and #252 before merging.

### M03 mail operations

- **Branch:** `feature/m03-mail-operations` is based on the unmerged
  `feature/m02-shell-next`, because M03 builds on the shell and multiple
  accounts. Merging M03 into `main` would also merge the M02 work, so merge M02
  first (it waits on the real-Plasma check for #35 and on OAuth #218).
- **Current state:** M03 #5 is in Development. Folder navigation #37,
  destructive operations #89, quick actions #83, sorting, filtering and
  selection #38 and multi-selection and batch feedback #88 are Completed;
  power-user workflows #84 are in Review. The other feature issues follow in
  this order: #85, #90, #87, #82, #223 and #86.
- **Pending verification:** Delete to Trash, Undo, Delete Permanently, Empty
  Trash/Junk and Rename Folder are covered by model and unit tests but have not
  been exercised in a window against a live server. Extend
  `tools/dev-container/live-imap-accounts.sh` or check them by hand with an
  isolated account. Limitations are tracked in
  [#312](https://github.com/mikepadiernos/plasma-impresario/issues/312).

### M01 release gate and integration

- **Deferred by milestone scope:** Live account setup belongs to M02 issue #78.
  Live message reading, replies and attachments belong to M04 issue #93.
  Plasma desktop-session startup and recovery belong to M05.01 issue #101.
  M01.04 is complete from automated build, installer, launch and privacy
  evidence without an isolated account or live Plasma session.
- **Pending:** Verify renderer-enabled and renderer-free installs at system
  prefixes and validate package-manager branches on distribution families other
  than CachyOS before claiming them as supported.

### Milestone feature-issue synchronization

- **M01-M12:** Each roadmap item has its own linked project card and issue.
  Items belong to their subsection issue where applicable, or otherwise to
  their milestone issue; subsection issues belong to the milestone issue.
  Issue milestone assignments and parent links were verified. Existing project
  statuses were preserved; newly created M01 issues are Defined and other
  newly created issues are Backlog.
- Keep roadmap links, issue parents, milestone assignments, and project status
  synchronized when roadmap scope changes.
