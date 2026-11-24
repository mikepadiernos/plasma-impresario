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

- **Current state:** M02 #4, shell #35, skeleton #74, connection status #79,
  account setup #78, account foundation #36, folder counts and saved searches
  #76 and protocol-aware account #215 are in Development. Session persistence
  #81 is in Review; density #75 and dependency gate #77 are Completed; other M02
  issues are Defined. The
  `feature/m02-shell-accounts` branch adds a row-density selector; the
  continuation branch adds left/right folder sidebar placement in the compact
  and wide layouts. A further continuation adds persistent connection status
  in the footer. The five-region shell and per-account status and error
  recovery still need implementation.
- **Account acceptance:** #78 has been exercised against a disposable Dovecot
  2.4.4 container using temporary configuration and synthetic credentials.
  The application saved IMAP settings, recovered from a rejected password,
  selected INBOX and reconnected after going offline. Secure credential
  storage and account removal still require implementation and verification.
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
  Integration with simultaneous accounts remains to be verified with #222.
- **Resume:** Implement and test the remaining M02 shell, account,
  protocol-capability and OAuth issues. Advance each issue through Review and
  Completed before checking its roadmap item; merge the milestone only when
  all items and acceptance checks are complete. Reconcile the pending roadmap
  changes in #250 and #252 before merging.

### M01 release gate and integration

- **Deferred by milestone scope:** Live account setup belongs to M02 issue #78.
  Live message reading, replies and attachments belong to M04 issue #93.
  Plasma desktop-session startup and recovery belong to M05 issue #101.
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
